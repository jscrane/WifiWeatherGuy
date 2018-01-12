#include <ArduinoJson.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <TFT_ILI9163C.h>
#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

#include "Configuration.h"
#include "display.h"
#include "dbg.h"

#define BLACK   0x0000
#define WHITE   0xFFFF

#define CS D6
#define DC D8
#define TFT_LED D2
#define SWITCH D3

TFT_ILI9163C tft = TFT_ILI9163C(CS, DC);

MDNSResponder mdns;
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

bool debug;

uint32_t last_fetch_conditions = 0, last_fetch_forecasts = 0;
uint32_t display_on = 0;
uint8_t fade;
bool connected;

class config: public Configuration {
public:
  char ssid[33];
  char password[33];
  char key[17];
  char station[33];
  char hostname[17];
  bool metric;
  uint32_t conditions_interval, forecasts_interval;
  uint32_t on_time;
  uint8_t bright, dim, rotate;

  void configure(JsonObject &o);
} cfg;

static void strncpy_null(char *dest, const char *src, int n) {
  if (src)
    strncpy(dest, src, n);
  else
    *dest = 0;
}

void config::configure(JsonObject &o) {
  strncpy_null(ssid, o[F("ssid")], sizeof(ssid));
  strncpy_null(password, o[F("password")], sizeof(password));
  strncpy_null(key, o[F("key")], sizeof(key));
  strncpy_null(station, o[F("station")], sizeof(station));
  strncpy_null(hostname, o[F("hostname")], sizeof(hostname));
  conditions_interval = 1000 * (int)o[F("conditions_interval")];
  forecasts_interval = 1000 * (int)o[F("forecasts_interval")];
  metric = (bool)o[F("metric")];
  on_time = 1000 * (int)o[F("display")];
  bright = o[F("bright")];
  dim = o[F("dim")];
  rotate = o[F("rotate")];
}

void setup() {

  Serial.begin(115200);

  tft.begin();
  tft.setTextColor(WHITE, BLACK);
  tft.setCursor(0, 0);

  pinMode(SWITCH, INPUT_PULLUP);
  debug = digitalRead(SWITCH) == LOW;

  bool result = SPIFFS.begin();
  if (!result) {
    ERR(print(F("SPIFFS: ")));
    ERR(println(result));
    return;
  }

  if (!cfg.read_file("/config.json")) {
    ERR(print(F("config!")));
    return;
  }

  fade = cfg.bright;
  tft.setRotation(cfg.rotate);
  analogWrite(TFT_LED, fade);

  tft.println(F("Weather Guy (c)2017"));
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
  tft.print(F("bright: "));
  tft.println(cfg.bright);
  tft.print(F("dim: "));
  tft.println(cfg.dim);
  if (debug)
    tft.println("DEBUG");

  WiFi.mode(WIFI_STA);
  WiFi.hostname(cfg.hostname);
  if (*cfg.ssid) {
    WiFi.begin(cfg.ssid, cfg.password);
    for (int i = 0; i < 60 && WiFi.status() != WL_CONNECTED; i++) {
      delay(500);
      OUT(print(F(".")));
    }
    connected = WiFi.status() == WL_CONNECTED;
  }

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

    if (mdns.begin(cfg.hostname, WiFi.localIP())) {
      DBG(println(F("MDNS started")));
      mdns.addService("http", "tcp", 80);
    } else
      ERR(println(F("Error starting MDNS")));

    last_fetch_conditions = -cfg.conditions_interval;
    last_fetch_forecasts = -cfg.forecasts_interval;
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

  JsonObject& current_observation = root[F("current_observation")];
  time_t epoch = (time_t)atoi(current_observation[F("observation_epoch")]);
  if (epoch == c.epoch)
    return false;
  c.epoch = epoch;
  strncpy(c.icon, current_observation[F("icon")], sizeof(c.icon));
  strncpy(c.weather, current_observation[F("weather")], sizeof(c.weather));
  if (cfg.metric) {
    c.temp = current_observation[F("temp_c")];
    c.feelslike = atoi(current_observation[F("feelslike_c")]);
    c.wind = current_observation[F("wind_kph")];
    c.atmos_pressure = atoi(current_observation[F("pressure_mb")]);
  } else {
    c.temp = current_observation[F("temp_f")];
    c.feelslike = atoi(current_observation[F("feelslike_f")]);
    c.wind = current_observation[F("wind_mph")];
    c.atmos_pressure = atoi(current_observation[F("pressure_in")]);
  }
  c.humidity = atoi(current_observation[F("relative_humidity")]);
  c.pressure_trend = atoi(current_observation[F("pressure_trend")]);
  strncpy(c.wind_dir, current_observation[F("wind_dir")], sizeof(c.wind_dir));;
  c.wind_degrees = current_observation[F("wind_degrees")];
  strncpy(c.city, current_observation[F("observation_location")][F("city")], sizeof(c.city));
  strncpy(c.sunrise_hour, root[F("sun_phase")][F("sunrise")][F("hour")], sizeof(c.sunrise_hour));
  strncpy(c.sunrise_minute, root[F("sun_phase")][F("sunrise")][F("minute")], sizeof(c.sunrise_minute));
  strncpy(c.sunset_hour, root[F("sun_phase")][F("sunset")][F("hour")], sizeof(c.sunset_hour));
  strncpy(c.sunset_minute, root[F("sun_phase")][F("sunset")][F("minute")], sizeof(c.sunset_minute));
  strncpy(c.age_of_moon, root[F("moon_phase")][F("ageOfMoon")], sizeof(c.age_of_moon));
  strncpy(c.moon_phase, root[F("moon_phase")][F("phaseofMoon")], sizeof(c.moon_phase));
  strncpy(c.moonrise_hour, root[F("moon_phase")][F("moonrise")][F("hour")], sizeof(c.moonrise_hour));
  strncpy(c.moonrise_minute, root[F("moon_phase")][F("moonrise")][F("minute")], sizeof(c.moonrise_minute));
  strncpy(c.moonset_hour, root[F("moon_phase")][F("moonset")][F("hour")], sizeof(c.moonset_hour));
  strncpy(c.moonset_minute, root[F("moon_phase")][F("moonset")][F("minute")], sizeof(c.moonset_minute));
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
  for (int i = 0; i < 4; i++) {
    JsonObject &day = days[i];
    struct Forecast &f = forecasts[i];
    f.epoch = (time_t)atoi(day[F("date")][F("epoch")]);
    strncpy(f.day, day[F("date")][F("weekday_short")], sizeof(f.day));
    if (cfg.metric) {
      f.temp_high = atoi(day[F("high")][F("celsius")]);
      f.temp_low = atoi(day[F("low")][F("celsius")]);
      f.max_wind = day[F("maxwind")][F("kph")];
      f.ave_wind = day[F("avewind")][F("kph")];
    } else {
      f.temp_high = atoi(day[F("high")][F("fahrenheit")]);
      f.temp_low = atoi(day[F("low")][F("fahrenheit")]);
      f.max_wind = day[F("maxwind")][F("mph")];
      f.ave_wind = day[F("avewind")][F("mph")];
    }
    f.ave_humidity = day[F("avehumidity")];
    f.wind_degrees = day[F("avewind")][F("degrees")];
    strncpy(f.wind_dir, day[F("avewind")][F("dir")], sizeof(f.wind_dir));
    strncpy(f.conditions, day[F("conditions")], sizeof(f.conditions));
    strncpy(f.icon, day[F("icon")], sizeof(f.icon));
  }
}

void display_weather(struct Conditions &c) {

  tft.fillScreen(WHITE);
  tft.setTextColor(BLACK);

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

  tft.fillScreen(BLACK);
  tft.setTextColor(WHITE);

  tft.setTextSize(2);
  tft.setCursor(1, 1);
  tft.print("sun");
  tft.setCursor(right(4, tft.width(), 2), 1);
  tft.print("moon");
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

  tft.fillScreen(WHITE);
  tft.setTextColor(BLACK);

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
    client.print(String("GET /api/") + cfg.key + F("/") + path + F("/q/") + cfg.station
                 + F(".json HTTP/1.1\r\n")
                 + F("Host: ") + host + F("\r\nConnection: close\r\n\r\n"));
    if (client.connected()) {
      client.find("\r\n\r\n");
      return true;
    }
  }
  return false;
}

void loop() {

  static int screen = 0;

  server.handleClient();
  if (!connected)
    return;

  mdns.update();

  uint32_t now = millis();
  bool swtch = !digitalRead(SWITCH);

  if (fade == cfg.dim) {
    if (swtch) {
      display_on = now;
      fade = cfg.bright;
      analogWrite(TFT_LED, fade);
    } else if (screen > 0) {
      screen = 0;
      update_display(screen);
    }
  } else if (now - display_on > cfg.on_time) {
    analogWrite(TFT_LED, --fade);
    delay(25);
  } else if (swtch && now - display_on > 500) {
    if (screen > 5)
      screen = 0;
    else
      screen++;
    update_display(screen);
  }
  
  static WiFiClient client;
  if (now - last_fetch_conditions > cfg.conditions_interval) {
    DBG(println(F("Updating conditions...")));

    last_fetch_conditions = now;
    if (connect_and_get(client, F("astronomy/conditions"))) {
      const size_t bufferSize = JSON_OBJECT_SIZE(0) + 9 * JSON_OBJECT_SIZE(2) + 2 * JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(4) + 
                                JSON_OBJECT_SIZE(8) + JSON_OBJECT_SIZE(9) + JSON_OBJECT_SIZE(12) + JSON_OBJECT_SIZE(56) + 2530;
      DynamicJsonBuffer buffer(bufferSize);
      if (update_conditions(buffer.parseObject(client), conditions))
        update_display(screen);
      client.stop();
      DBG(println(ESP.getFreeHeap()));
      DBG(println(F("Done")));
    } else
      ERR(println(F("Failed to fetch conditions!")));
  }

  if (now - last_fetch_forecasts > cfg.forecasts_interval) {
    DBG(println(F("Updating forecasts...")));

    last_fetch_forecasts = now;
    if (connect_and_get(client, F("forecast"))) {
      const size_t bufferSize = JSON_ARRAY_SIZE(4) + JSON_ARRAY_SIZE(8) + 2*JSON_OBJECT_SIZE(1) + 35*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + 
                                8*JSON_OBJECT_SIZE(4) + 8*JSON_OBJECT_SIZE(7) + 4*JSON_OBJECT_SIZE(17) + 4*JSON_OBJECT_SIZE(20) + 5360;
      DynamicJsonBuffer forecast(bufferSize);
      update_forecasts(forecast.parseObject(client));
      client.stop();
      DBG(println(ESP.getFreeHeap()));
      DBG(println(F("Done")));
    } else
      ERR(println(F("Failed to fetch forecast!")));
  }
}
