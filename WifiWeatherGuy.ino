#include <ArduinoJson.h>
#include <SPI.h>
#include <LittleFS.h>
#include <Adafruit_GFX.h>
#include <TFT_eSPI.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <SimpleTimer.h>
#include <TimeLib.h>
#include <Timezone.h>

#include "Switch.h"
#include "Configuration.h"
#include "state.h"
#include "display.h"
#include "dbg.h"
#include "providers.h"

#if !defined(TFT_LED)
#define TFT_LED	D2
#endif

#if !defined(SWITCH)
#define SWITCH	D3
#endif

TFT_eSPI tft;
MDNSResponder mdns;
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
DNSServer dnsServer;

uint32_t display_on;
uint8_t fade;
bool connected, debug;

config cfg;
Timezone *tz;
struct Conditions conditions;
struct Forecast forecasts[4];
struct Statistics stats;

#if !defined(PROVIDER)
#define PROVIDER OpenWeatherMap
#endif

PROVIDER provider;

void config::configure(JsonDocument &o) {
	strlcpy(ssid, o[F("ssid")] | "", sizeof(ssid));
	strlcpy(password, o[F("password")] | "", sizeof(password));
	strlcpy(key, o[F("key")] | "", sizeof(key));
	strlcpy(station, o[F("station")] | "", sizeof(station));
	strlcpy(hostname, o[F("hostname")] | "", sizeof(hostname));
	conditions_interval = 1000 * (int)o[F("conditions_interval")];
	forecasts_interval = 1000 * (int)o[F("forecasts_interval")];
	metric = o[F("metric")];
	dimmable = o[F("dimmable")];
	nearest = o[F("nearest")];
	on_time = 1000 * (int)o[F("display")];
	bright = o[F("bright")];
	dim = o[F("dim")];
	rotate = o[F("rotate")];

	const JsonObject &s = o[F("summer")];
	summer.week = (int)s[F("week")] | 0;
	summer.dow = (int)s[F("dow")] | 1;
	summer.month = (int)s[F("month")] | 1;
	summer.hour = (int)s[F("hour")] | 0;
	summer.offset = (int)s[F("offset")] | 0;

	const JsonObject &w = o[F("winter")];
	winter.week = (int)w[F("week")] | 0;
	winter.dow = (int)w[F("dow")] | 1;
	winter.month = (int)w[F("month")] | 1;
	winter.hour = (int)w[F("hour")] | 0;
	winter.offset = (int)w[F("offset")] | 0;

	::tz = new Timezone(summer, winter);
}

Switch swtch(500);
void IRAM_ATTR swtch_handler() { swtch.on(); }

const char *config_file = "/config.json";
static int screen = 0;
static SimpleTimer timers;

static void update_display() {
	if (cfg.dimmable || fade > cfg.dim) {
		switch (screen) {
		case 0:
			display_weather(conditions);
			break;
		case 1:
			display_astronomy(conditions);
			break;
		case 2:
		case 3:
		case 4:
		case 5:
			display_forecast(forecasts[screen - 2]);
			break;
		case 6:
			display_about(stats);
			break;
		}
	}
}

// timer callbacks
static void update_conditions() {
	DBG(println(F("Updating conditions...")));
	if (provider.fetch_conditions(conditions)) {
		update_display();
		stats.last_fetch_conditions = millis();
	}
}

static void update_forecasts() {
	DBG(println(F("Updating forecasts...")));
	if (provider.fetch_forecasts(&forecasts[0], sizeof(forecasts)/sizeof(forecasts[0])))
		stats.last_fetch_forecasts = millis();
}

static void next_fade() {
	analogWrite(TFT_LED, --fade);
	if (fade == cfg.dim && screen > 0) {
		screen = 0;
		update_display();
	}
}

static void turn_off() {
	if (cfg.dimmable) {
		timers.setTimer(25, next_fade, cfg.bright - cfg.dim);
	} else {
		tft.fillScreen(TFT_BLACK);
		fade = cfg.dim;
	}
}

void setup() {
	Serial.begin(115200);
	tft.init();
	tft.setTextColor(TFT_WHITE, TFT_BLACK);
	tft.setCursor(0, 0);

#if defined(FONT)
	tft.setTextFont(FONT);
#endif

	pinMode(SWITCH, INPUT_PULLUP);
	debug = digitalRead(SWITCH) == LOW;

	bool result = LittleFS.begin();
	if (!result) {
		ERR(print(F("LittleFS!")));
		return;
	}

	if (!cfg.read_file(config_file)) {
		ERR(print(F("config!")));
		return;
	}

	fade = cfg.bright;
	analogWrite(TFT_LED, fade);

	tft.fillScreen(TFT_BLACK);
	tft.setRotation(cfg.rotate);
	tft.println(F("Weather Guy (c)2018"));
	tft.print(F("ssid: "));
	tft.println(cfg.ssid);
	tft.print(F("password: "));
	tft.println(cfg.password);
	tft.print(F("key: "));
	tft.println(cfg.key);
	tft.print(F("station: "));
	if (cfg.nearest)
		tft.println(F("nearest"));
	else
		tft.println(cfg.station);
	tft.print(F("hostname: "));
	tft.println(cfg.hostname);
	tft.print(F("condition...: "));
	tft.println(cfg.conditions_interval);
	tft.print(F("forecast...: "));
	tft.println(cfg.forecasts_interval);
	tft.print(F("display: "));
	tft.println(cfg.on_time);
	tft.print(F("metric: "));
	tft.println(cfg.metric);
	if (cfg.dimmable) {
		tft.print(F("bright: "));
		tft.println(cfg.bright);
		tft.print(F("dim: "));
		tft.println(cfg.dim);
	} else
		tft.println(F("not dimmable"));
	if (debug)
		tft.println(F("DEBUG"));

	WiFi.mode(WIFI_STA);
	WiFi.setSleepMode(WIFI_NONE_SLEEP);
	WiFi.hostname(cfg.hostname);
	if (*cfg.ssid) {
		WiFi.setAutoReconnect(true);
		WiFi.begin(cfg.ssid, cfg.password);
		const char busy[] = "|/-\\";
		int16_t y = tft.getCursorY();
		for (int i = 0; i < 60 && WiFi.status() != WL_CONNECTED; i++) {
			delay(500);
			char c = busy[i % 4];
			tft.print(c);
			tft.setCursor(0, y);
		}
		connected = WiFi.status() == WL_CONNECTED;
	}

	server.on("/config", HTTP_POST, []() {
		if (server.hasArg("plain")) {
			String body = server.arg("plain");
			File f = LittleFS.open(config_file, "w");
			f.print(body);
			f.close();
			server.send(200);
			WiFi.setAutoConnect(false);
			ESP.restart();
		} else
			server.send(400, "text/plain", "No body!");
	});
	server.serveStatic("/", LittleFS, "/index.html");
	server.serveStatic("/config", LittleFS, config_file);
	server.serveStatic("/js/transparency.min.js", LittleFS, "/transparency.min.js");
	server.serveStatic("/info.png", LittleFS, "/info.png");

	httpUpdater.setup(&server);
	server.begin();

	if (mdns.begin(cfg.hostname, WiFi.localIP())) {
		DBG(println(F("mDNS started")));
		mdns.addService("http", "tcp", 80);
	} else
		ERR(println(F("Error starting mDNS")));

	if (!connected) {
		WiFi.mode(WIFI_AP);
		WiFi.softAP(cfg.hostname);
		tft.println(F("Connect to SSID"));
		tft.println(cfg.hostname);
		tft.println(F("to configure WiFi"));
		dnsServer.start(53, "*", WiFi.softAPIP());
	} else {
		DBG(println());
		DBG(print(F("Connected to ")));
		DBG(println(cfg.ssid));
		DBG(println(WiFi.localIP()));

		tft.println();
		tft.print(F("http://"));
		tft.print(WiFi.localIP());
		tft.println('/');

		if (cfg.nearest) {
			WiFiClient client;
			const __FlashStringHelper *host = F("ip-api.com");
			bool is_geo = false;
			if (client.connect(host, 80)) {
				client.print(F("GET /json HTTP/1.1\r\nHost: "));
				client.print(host);
				client.print(F("\r\nConnection: close\r\n\r\n"));
				if (client.connected()) {
					unsigned long now = millis();
					while (!client.available())
						if (millis() - now > 5000)
							goto cont;
					client.find("\r\n\r\n");

					const size_t size = JSON_OBJECT_SIZE(14) + 290;
					DynamicJsonDocument geo(size);
					auto error = deserializeJson(geo, client);
					if (!error) {
						// if success, decode...
						cfg.lat = geo["lat"];
						cfg.lon = geo["lon"];
						is_geo = true;
					}
				}
			}
			cfg.nearest = is_geo;
		}
cont:
		stats.last_fetch_conditions = -cfg.conditions_interval;
		stats.last_fetch_forecasts = -cfg.forecasts_interval;
	}
	attachInterrupt(SWITCH, swtch_handler, FALLING);

	timers.setInterval(cfg.conditions_interval, update_conditions);
	timers.setInterval(cfg.forecasts_interval, update_forecasts);
	timers.setTimeout(cfg.on_time, turn_off);

	update_conditions();
	update_forecasts();
}

void loop() {
	mdns.update();

	server.handleClient();
	if (!connected) {
		dnsServer.processNextRequest();
		return;
	}

	if (swtch) {
		if (fade == cfg.dim) {
			fade = cfg.bright;
			if (cfg.dimmable)
				analogWrite(TFT_LED, fade);
			else
				update_display();
			timers.setTimeout(cfg.on_time, turn_off);
		} else {
			if (screen >= 6)
				screen = 0;
			else
				screen++;
			update_display();
		}
	}
	timers.run();
}
