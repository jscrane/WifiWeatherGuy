#include <ArduinoJson.h>
#include <SPI.h>
#include <FS.h>
#include <Adafruit_GFX.h>
#include <TFT_eSPI.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

#include "Configuration.h"
#include "display.h"
#include "dbg.h"

#define CS	D6
#define DC	D8
#define TFT_LED	D2
#define SWITCH	D3

TFT_eSPI tft;
MDNSResponder mdns;
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

uint32_t last_fetch_conditions, last_fetch_forecasts;
uint32_t display_on;
uint8_t fade;
bool connected, debug;

class config: public Configuration {
public:
	char ssid[33];
	char password[33];
	char key[17];
	char station[33];
	char hostname[17];
	bool metric, dimmable;
	uint32_t conditions_interval, forecasts_interval;
	uint32_t on_time, retry_interval;
	uint16_t bright, dim;
	uint8_t rotate;

	void configure(JsonObject &o);
} cfg;

void config::configure(JsonObject &o) {
	strlcpy(ssid, o[F("ssid")] | "", sizeof(ssid));
	strlcpy(password, o[F("password")] | "", sizeof(password));
	strlcpy(key, o[F("key")] | "", sizeof(key));
	strlcpy(station, o[F("station")] | "", sizeof(station));
	strlcpy(hostname, o[F("hostname")] | "", sizeof(hostname));
	conditions_interval = 1000 * (int)o[F("conditions_interval")];
	forecasts_interval = 1000 * (int)o[F("forecasts_interval")];
	metric = o[F("metric")];
	dimmable = o[F("dimmable")];
	on_time = 1000 * (int)o[F("display")];
	bright = o[F("bright")];
	dim = o[F("dim")];
	rotate = o[F("rotate")];
	retry_interval = 1000 * (int)o[F("retry_interval")];
}

static volatile bool swtch;

void setup() {
	Serial.begin(115200);
	tft.init();
	tft.setTextColor(TFT_WHITE, TFT_BLACK);
	tft.setCursor(0, 0);

	pinMode(SWITCH, INPUT_PULLUP);
	debug = digitalRead(SWITCH) == LOW;

	bool result = SPIFFS.begin();
	if (!result) {
		ERR(print(F("SPIFFS!")));
		return;
	}

	if (!cfg.read_file("/config.json")) {
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
	WiFi.hostname(cfg.hostname);
	if (*cfg.ssid) {
		WiFi.begin(cfg.ssid, cfg.password);
		for (int i = 0; i < 60 && WiFi.status() != WL_CONNECTED; i++) {
			delay(500);
			tft.print('.');
		}
		connected = WiFi.status() == WL_CONNECTED;
	}

	server.on("/config", HTTP_POST, []() {
		if (server.hasArg("plain")) {
			String body = server.arg("plain");
			File f = SPIFFS.open("/config.json", "w");
			f.print(body);
			f.close();
			ESP.restart();
		} else
			server.send(400, "text/plain", "No body!");
	});
	server.serveStatic("/", SPIFFS, "/index.html");
	server.serveStatic("/config", SPIFFS, "/config.json");
	server.serveStatic("/js/transparency.min.js", SPIFFS, "/transparency.min.js");

	httpUpdater.setup(&server);
	server.begin();

	if (mdns.begin(cfg.hostname, WiFi.localIP())) {
		DBG(println(F("mDNS started")));
		mdns.addService("http", "tcp", 80);
	} else
		ERR(println(F("Error starting mDNS")));

	if (!connected) {
		WiFi.softAP(cfg.hostname);
		tft.println(F("Connect to SSID"));
		tft.println(cfg.hostname);
		tft.println(F("to configure WiFi"));
	} else {
		DBG(println());
		DBG(print(F("Connected to ")));
		DBG(println(cfg.ssid));
		DBG(println(WiFi.localIP()));

		tft.println();
		tft.print(F("http://"));
		tft.print(WiFi.localIP());
		tft.println('/');

		last_fetch_conditions = -cfg.conditions_interval;
		last_fetch_forecasts = -cfg.forecasts_interval;
	}
	attachInterrupt(SWITCH, []() { swtch=true; }, RISING);
}

struct Conditions {
	char icon[16];
	char weather[32];
	int temp, feelslike;
	int humidity;
	int wind;
	int atmos_pressure;
	int pressure_trend;
	char wind_dir[10];
	char sunrise_hour[3];
	char sunrise_minute[3];
	char sunset_hour[3];
	char sunset_minute[3];
	char moonrise_hour[3];
	char moonrise_minute[3];
	char moonset_hour[3];
	char moonset_minute[3];
	char moon_phase[16];
	char age_of_moon[3];
	time_t epoch;
	char city[48];
	int wind_degrees;
} conditions;

bool update_conditions(JsonObject &root, struct Conditions &c) {
	JsonObject &current_observation = root[F("current_observation")];
	if (!current_observation.success())
		return false;
	time_t epoch = (time_t)atoi(current_observation[F("observation_epoch")] | "0");
	if (epoch <= c.epoch)
		return false;	    

	c.epoch = epoch;
	strlcpy(c.icon, current_observation[F("icon")] | "", sizeof(c.icon));
	strlcpy(c.weather, current_observation[F("weather")] | "", sizeof(c.weather));
	if (cfg.metric) {
		c.temp = current_observation[F("temp_c")];
		c.feelslike = atoi(current_observation[F("feelslike_c")] | "0");
		c.wind = current_observation[F("wind_kph")];
		c.atmos_pressure = atoi(current_observation[F("pressure_mb")] | "0");
	} else {
		c.temp = current_observation[F("temp_f")];
		c.feelslike = atoi(current_observation[F("feelslike_f")] | "0");
		c.wind = current_observation[F("wind_mph")];
		c.atmos_pressure = atoi(current_observation[F("pressure_in")] | "0");
	}
	c.humidity = atoi(current_observation[F("relative_humidity")] | "0");
	c.pressure_trend = atoi(current_observation[F("pressure_trend")] | "0");
	strlcpy(c.wind_dir, current_observation[F("wind_dir")] | "", sizeof(c.wind_dir));;
	c.wind_degrees = current_observation[F("wind_degrees")];
	strlcpy(c.city, current_observation[F("observation_location")][F("city")] | "", sizeof(c.city));
	JsonObject &sun = root[F("sun_phase")];
	strlcpy(c.sunrise_hour, sun[F("sunrise")][F("hour")] | "", sizeof(c.sunrise_hour));
	strlcpy(c.sunrise_minute, sun[F("sunrise")][F("minute")] | "", sizeof(c.sunrise_minute));
	strlcpy(c.sunset_hour, sun[F("sunset")][F("hour")] | "", sizeof(c.sunset_hour));
	strlcpy(c.sunset_minute, sun[F("sunset")][F("minute")] | "", sizeof(c.sunset_minute));
	JsonObject &moon = root[F("moon_phase")];
	strlcpy(c.age_of_moon, moon[F("ageOfMoon")] | "", sizeof(c.age_of_moon));
	strlcpy(c.moon_phase, moon[F("phaseofMoon")] | "", sizeof(c.moon_phase));
	strlcpy(c.moonrise_hour, moon[F("moonrise")][F("hour")] | "", sizeof(c.moonrise_hour));
	strlcpy(c.moonrise_minute, moon[F("moonrise")][F("minute")] | "", sizeof(c.moonrise_minute));
	strlcpy(c.moonset_hour, moon[F("moonset")][F("hour")] | "", sizeof(c.moonset_hour));
	strlcpy(c.moonset_minute, moon[F("moonset")][F("minute")] | "", sizeof(c.moonset_minute));
	return true;
}

struct Forecast {
	time_t epoch;
	char day[4];
	int temp_high;
	int temp_low;
	int max_wind;
	int ave_wind;
	char wind_dir[8];
	int wind_degrees;
	int ave_humidity;
	char conditions[32];
	char icon[16];
} forecasts[4];

void update_forecasts(JsonObject &root) {
	JsonArray &days = root[F("forecast")][F("simpleforecast")][F("forecastday")];
	if (days.success())
		for (int i = 0; i < days.size() && i < sizeof(forecasts)/sizeof(struct Forecast); i++) {
			JsonObject &day = days[i];
			struct Forecast &f = forecasts[i];
			f.epoch = (time_t)atoi(day[F("date")][F("epoch")] | "0");
			strlcpy(f.day, day[F("date")][F("weekday_short")] | "", sizeof(f.day));
			if (cfg.metric) {
				f.temp_high = atoi(day[F("high")][F("celsius")] | "0");
				f.temp_low = atoi(day[F("low")][F("celsius")] | "0");
				f.max_wind = day[F("maxwind")][F("kph")];
				f.ave_wind = day[F("avewind")][F("kph")];
			} else {
				f.temp_high = atoi(day[F("high")][F("fahrenheit")] | "0");
				f.temp_low = atoi(day[F("low")][F("fahrenheit")] | "0");
				f.max_wind = day[F("maxwind")][F("mph")];
				f.ave_wind = day[F("avewind")][F("mph")];
			}
			f.ave_humidity = day[F("avehumidity")];
			f.wind_degrees = day[F("avewind")][F("degrees")];
			strlcpy(f.wind_dir, day[F("avewind")][F("dir")] | "", sizeof(f.wind_dir));
			strlcpy(f.conditions, day[F("conditions")] | "", sizeof(f.conditions));
			strlcpy(f.icon, day[F("icon")] | "", sizeof(f.icon));
		}
}

void display_weather(struct Conditions &c) {
	tft.fillScreen(TFT_WHITE);
	tft.setTextColor(TFT_BLACK);

	display_wind_speed(c.wind, c.wind_dir, cfg.metric? F("kph"): F("mph"));
	display_temperature(c.temp, c.feelslike, cfg.metric);
	display_humidity(c.humidity);

	tft.setTextSize(2);
	tft.setCursor(right(val_len(c.atmos_pressure), tft.width(), 2)-12, 1);
	tft.print(c.atmos_pressure);
	tft.setTextSize(1);
	tft.print(cfg.metric? F("mb"): F("in"));
	if (c.pressure_trend == 1) {
		tft.setCursor(right(6, tft.width(), 1), 17);
		tft.print(F("rising"));
	} else if (c.pressure_trend == -1) {
		tft.setCursor(right(7, tft.width(), 1), 17);
		tft.print(F("falling"));
	}

	tft.setCursor(centre_text(c.city, tft.width()/2, 1), 25);
	tft.print(c.city);
	tft.setCursor(centre_text(c.weather, tft.width()/2, 1), 34);
	tft.print(c.weather);
	display_bmp(c.icon, tft.width()/2 - 25, 42);

	display_time(c.epoch, cfg.metric);
	display_wind(c.wind_degrees, c.wind);
}

void display_astronomy(struct Conditions &c) {
	tft.fillScreen(TFT_BLACK);
	tft.setTextColor(TFT_WHITE);

	tft.setTextSize(2);
	tft.setCursor(1, 1);
	tft.print(F("sun"));
	tft.setCursor(right(4, tft.width(), 2), 1);
	tft.print(F("moon"));
	tft.setTextSize(1);
	tft.setCursor(strlen(c.sunrise_hour) == 1? 7: 1, 17);
	tft.print(c.sunrise_hour);
	tft.print(':');
	tft.print(c.sunrise_minute);
	tft.setCursor(1, 25);
	tft.print(c.sunset_hour);
	tft.print(':');
	tft.print(c.sunset_minute);

	const char *rise = "rise";
	tft.setCursor(centre_text(rise, tft.width()/2, 1), 17);
	tft.print(rise);
	const char *set = "set";
	tft.setCursor(centre_text(set, tft.width()/2, 1), 25);
	tft.print(set);

	tft.setCursor(right(3 + strlen(c.moonrise_hour), tft.width(), 1), 17);
	tft.print(c.moonrise_hour);
	tft.print(':');
	tft.print(c.moonrise_minute);
	tft.setCursor(right(3 + strlen(c.moonset_hour), tft.width(), 1), 25);
	tft.print(c.moonset_hour);
	tft.print(':');
	tft.print(c.moonset_minute);

	char buf[32];
	strcpy(buf, "moon");
	strcat(buf, c.age_of_moon);
	display_bmp(buf, tft.width()/2 - 25, 42);

	tft.setCursor(centre_text(c.moon_phase, tft.width()/2, 1), 92);
	tft.print(c.moon_phase);

	display_time(c.epoch, cfg.metric);
}

void display_forecast(struct Forecast &f) {
	tft.fillScreen(TFT_WHITE);
	tft.setTextColor(TFT_BLACK);

	display_wind_speed(f.ave_wind, f.wind_dir, cfg.metric);
	display_temperature(f.temp_high, f.temp_low, cfg.metric);
	display_humidity(f.ave_humidity);

	tft.setTextSize(2);
	tft.setCursor(right(3, tft.width(), 2), 1);
	tft.print(f.day);

	tft.setTextSize(1);
	tft.setCursor(centre_text(f.conditions, tft.width()/2, 1), 34);
	tft.print(f.conditions);
	display_bmp(f.icon, tft.width()/2 - 25, 42);

	display_time(f.epoch, cfg.metric);
	display_wind(f.wind_degrees, f.ave_wind);
}

void update_display(int screen) {
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
	}
}

bool connect_and_get(WiFiClient &client, const __FlashStringHelper *path) {
	const __FlashStringHelper *host = F("api.wunderground.com");
	if (client.connect(host, 80)) {
		client.print("GET /api/");
		client.print(cfg.key);
		client.print('/');
		client.print(path);
		client.print(F("/q/"));
		client.print(cfg.station);
		client.print(F(".json HTTP/1.1\r\nHost: "));
		client.print(host);
		client.print(F("\r\nConnection: close\r\n\r\n"));
		if (client.connected()) {
			unsigned long now = millis();
			while (!client.available())
				if (millis() - now > 5000)
					return false;
			client.find("\r\n\r\n");
			return true;
		}
	}
	return false;
}

static unsigned cbytes = JSON_OBJECT_SIZE(0) + 9 * JSON_OBJECT_SIZE(2) + 2 * JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(4) +
			JSON_OBJECT_SIZE(8) + JSON_OBJECT_SIZE(9) + JSON_OBJECT_SIZE(12) + JSON_OBJECT_SIZE(56) + 2530;

static unsigned fbytes = JSON_ARRAY_SIZE(4) + JSON_ARRAY_SIZE(8) + 2*JSON_OBJECT_SIZE(1) + 35*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) +
			8*JSON_OBJECT_SIZE(4) + 8*JSON_OBJECT_SIZE(7) + 4*JSON_OBJECT_SIZE(17) + 4*JSON_OBJECT_SIZE(20) + 6150;

void fetch(unsigned now, const __FlashStringHelper *path, unsigned &bytes, unsigned &last_fetch, unsigned interval, void (*f)(JsonObject &)) {
	if (now - last_fetch > interval) {
		unsigned heap = ESP.getFreeHeap();
		if (heap > bytes) {
			DBG(print(F("Updating ")));
			DBG(print(path));
			DBG(print(' '));
			DBG(print(bytes));
			DBG(print(' '));
			DBG(println(heap));
			WiFiClient client;
			if (connect_and_get(client, path)) {
				DynamicJsonBuffer buffer(bytes);
				JsonObject &root = buffer.parseObject(client);
				if (root.success()) {
					f(root);
					unsigned n = buffer.size();
					DBG(print(F("Done ")));
					DBG(println(n));
					if (n > bytes)
						bytes = n;
					last_fetch = now;
				} else {
					ERR(println(F("Failed to parse!")));
					last_fetch += cfg.retry_interval;
				}
			} else {
				ERR(println(F("Failed to fetch!")));
				last_fetch += cfg.retry_interval;
			}
			client.stop();
		} else {
			DBG(println(F("Insufficient memory to update!")));
			last_fetch += cfg.retry_interval;
		}
	}
}

void loop() {
	mdns.update();

	server.handleClient();
	if (!connected)
		return;

	static int screen = 0;
	static uint32_t last_switch = -1000;
	uint32_t now = millis(), ontime = now - display_on;
	if (fade == cfg.dim) {
		if (swtch) {
			display_on = now;
			fade = cfg.bright;
			if (cfg.dimmable)
				analogWrite(TFT_LED, fade);
			else
				update_display(screen);
		} else if (screen > 0) {
			screen = 0;
			if (cfg.dimmable)
				update_display(screen);
		}
	} else if (ontime > cfg.on_time) {
		if (cfg.dimmable) {
			analogWrite(TFT_LED, --fade);
			delay(25);
		} else {
			tft.fillScreen(TFT_BLACK);
			fade = cfg.dim;
		}
	} else if (swtch && ontime > 500) {
		if (screen >= 5)
			screen = 0;
		else
			screen++;
		update_display(screen);
	}
	swtch = false;

	fetch(now, F("astronomy/conditions"), cbytes, last_fetch_conditions, cfg.conditions_interval, [] (JsonObject &root) {
		if (update_conditions(root, conditions) && (cfg.dimmable || fade == cfg.bright))
			update_display(screen);
	});

	fetch(now, F("forecast"), fbytes, last_fetch_forecasts, cfg.forecasts_interval, [] (JsonObject &root) {
		update_forecasts(root);
	});
}
