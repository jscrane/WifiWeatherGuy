#include <ArduinoJson.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <TFT_ILI9163C.h>
#include <time.h>
#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include "Configuration.h"

const char host[] = "api.wunderground.com";

#define BLACK   0x0000
#define WHITE   0xFFFF

#define CS D6
#define DC D8
#define TFT_LED D2
#define SWITCH D3

TFT_ILI9163C tft = TFT_ILI9163C(CS, DC);

MDNSResponder mdns;
ESP8266WebServer server(80);

Print &out = Serial;
#define DEBUG

uint32_t last_fetch_conditions = 0, last_fetch_forecasts = 0;
uint32_t display_on = 0;
uint8_t fade;

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
  uint8_t bright, dim;

  void configure(JsonObject &o);
} cfg;

void config::configure(JsonObject &o) {
  strncpy(ssid, o["ssid"], sizeof(ssid));
  strncpy(password, o["password"], sizeof(password));
  strncpy(key, o["key"], sizeof(key));
  strncpy(station, o["station"], sizeof(station));
  conditions_interval = 1000 * (int)o["conditions_interval"];
  forecasts_interval = 1000 * (int)o["forecasts_interval"];
  metric = (bool)o["metric"];
  on_time = 1000 * (int)o["display"];
  bright = o["bright"];
  dim = o["dim"];
  strncpy(hostname, o["hostname"], sizeof(hostname));
}

extern int bmp_draw(TFT_ILI9163C &tft, const char *filename, uint8_t x, uint8_t y);

void setup() {

  Serial.begin(115200);

  tft.begin();
  tft.setTextColor(WHITE, BLACK);
  tft.setCursor(0, 0);

  pinMode(SWITCH, INPUT_PULLUP);

  bool result = SPIFFS.begin();
  if (!result) {
    out.print("SPIFFS: ");
    out.println(result);
    return;
  }

  if (!cfg.read_file("/config.json")) {
    out.print(F("config!"));
    return;
  }

  fade = cfg.bright;
  analogWrite(TFT_LED, fade);

  tft.println(F("Weather Guy (c)2017"));
  tft.println();
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

  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.ssid, cfg.password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    out.print(F("."));
  }
#ifdef DEBUG  
  out.println();
  out.print(F("Connected to "));
  out.println(cfg.ssid);
  out.println(WiFi.localIP());
#endif
  tft.println();
  tft.println(WiFi.localIP());

  if (mdns.begin(cfg.hostname, WiFi.localIP()))
    out.println("MDNS started");
  
  ArduinoOTA.setHostname(cfg.hostname);
  ArduinoOTA.onStart([]() {
#ifdef DEBUG
    Serial.println("Start");
#endif
  });
  ArduinoOTA.onEnd([]() {
#ifdef DEBUG
    Serial.println("\nEnd");
#endif
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
#ifdef DEBUG
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
#endif
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  
  last_fetch_conditions = -cfg.conditions_interval;
  last_fetch_forecasts = -cfg.forecasts_interval;

  server.on("/", []() {
    File f = SPIFFS.open("/index.html", "r");
    server.streamFile(f, "text/html");
    f.close();
  });
  server.on("/config", HTTP_GET, []() {
    File f = SPIFFS.open("/config.json", "r");
    server.streamFile(f, "text/json");
    f.close();
  });
  server.on("/config", HTTP_POST, []() {
    if (server.hasArg("plain")) {
      String body = server.arg("plain");
      Serial.print(body);
      File f = SPIFFS.open("/config.json", "w");
      f.print(body);
      f.close();
    } else
      server.send(400, "text/plain", "No body!");
  });
  server.begin();
}

struct Conditions {

  char icon[16];
  char weather[32];
  int temp, feelslike;
  char temp_unit;
  int humidity;
  int wind;
  const char *wind_unit;
  int atmos_pressure;
  const char *pres_unit;
  int pressure_trend;
  char wind_dir[6];
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

void update_conditions(JsonObject &root, struct Conditions &c) {

  JsonObject& current_observation = root["current_observation"];
  c.epoch = (time_t)atoi(current_observation["observation_epoch"]);
  strncpy(c.icon, current_observation["icon"], sizeof(c.icon));
  strncpy(c.weather, current_observation["weather"], sizeof(c.weather));
  if (cfg.metric) {
    c.temp = current_observation["temp_c"];
    c.feelslike = atoi(current_observation["feelslike_c"]);
    c.temp_unit = 'C';
    c.wind = current_observation["wind_kph"];
    c.wind_unit = "kph";
    c.atmos_pressure = atoi(current_observation["pressure_mb"]);
    c.pres_unit = "mb";
  } else {
    c.temp = current_observation["temp_f"];
    c.feelslike = atoi(current_observation["feelslike_f"]);
    c.temp_unit = 'F';
    c.wind = current_observation["wind_mph"];
    c.wind_unit = "mph";
    c.atmos_pressure = atoi(current_observation["pressure_in"]);
    c.pres_unit = "in";
  }
  c.humidity = atoi(current_observation["relative_humidity"]);
  c.pressure_trend = atoi(current_observation["pressure_trend"]);
  strncpy(c.wind_dir, current_observation["wind_dir"], sizeof(c.wind_dir));;
  c.wind_degrees = current_observation["wind_degrees"];
  strncpy(c.city, current_observation["observation_location"]["city"], sizeof(c.city));
  strncpy(c.sunrise_hour, root["sun_phase"]["sunrise"]["hour"], sizeof(c.sunrise_hour));
  strncpy(c.sunrise_minute, root["sun_phase"]["sunrise"]["minute"], sizeof(c.sunrise_minute));
  strncpy(c.sunset_hour, root["sun_phase"]["sunset"]["hour"], sizeof(c.sunset_hour));
  strncpy(c.sunset_minute, root["sun_phase"]["sunset"]["minute"], sizeof(c.sunset_minute));
  strncpy(c.age_of_moon, root["moon_phase"]["ageOfMoon"], sizeof(c.age_of_moon));
  strncpy(c.moon_phase, root["moon_phase"]["phaseofMoon"], sizeof(c.moon_phase));
  strncpy(c.moonrise_hour, root["moon_phase"]["moonrise"]["hour"], sizeof(c.moonrise_hour));
  strncpy(c.moonrise_minute, root["moon_phase"]["moonrise"]["minute"], sizeof(c.moonrise_minute));
  strncpy(c.moonset_hour, root["moon_phase"]["moonset"]["hour"], sizeof(c.moonset_hour));
  strncpy(c.moonset_minute, root["moon_phase"]["moonset"]["minute"], sizeof(c.moonset_minute));
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

  JsonArray &days = root["forecast"]["simpleforecast"]["forecastday"];
  for (int i = 0; i < 4; i++) {
    JsonObject &day = days[i];
    struct Forecast &f = forecasts[i];
    f.epoch = (time_t)atoi(day["date"]["epoch"]);
    strncpy(f.day, day["date"]["weekday_short"], sizeof(f.day));
    if (cfg.metric) {
      f.temp_high = atoi(day["high"]["celsius"]);
      f.temp_low = atoi(day["low"]["celsius"]);
      f.max_wind = day["maxwind"]["kph"];
      f.ave_wind = day["avewind"]["kph"];
    } else {
      f.temp_high = atoi(day["high"]["fahrenheit"]);
      f.temp_low = atoi(day["low"]["fahrenheit"]);
      f.max_wind = day["maxwind"]["mph"];
      f.ave_wind = day["avewind"]["mph"];
    }
    f.ave_humidity = day["avehumidity"];
    f.wind_degrees = day["avewind"]["degrees"];
    strncpy(f.wind_dir, day["avewind"]["dir"], sizeof(f.wind_dir));
    strncpy(f.conditions, day["conditions"], sizeof(f.conditions));
    strncpy(f.icon, day["icon"], sizeof(f.icon));
  }
}

static int centre_text(const char *s, int x, int size)
{
  return x - (strlen(s) * size * 6) / 2;
}

static int right(int n, int x, int size)
{
  return x - n * size * 6;
}

static int val_len(int b)
{
  if (b >= 1000) return 4;
  if (b >= 100) return 3;
  if (b >= 10) return 2;
  if (b >= 0) return 1;
  if (b > -10) return 2;
  return 3;
}

static void display_time(time_t &epoch) {
  char buf[32];
  strftime(buf, sizeof(buf), cfg.metric? "%H:%S": "%I:%S%p", localtime(&epoch));
  tft.setCursor(centre_text(buf, tft.width()/2, 1), 109);
  tft.print(buf);
  strftime(buf, sizeof(buf), "%a %d", localtime(&epoch));
  tft.setCursor(centre_text(buf, tft.width()/2, 1), 118);
  tft.print(buf);
}

static void display_wind(int wind_degrees, int wind_speed) {
    // http://www.iquilezles.org/www/articles/sincos/sincos.htm
  int rad = tft.width()/3, cx = tft.width()/2, cy = 68;
  const float a = 0.999847695, b = 0.017452406;
  // wind dir is azimuthal angle with N at 0
  float sin = 1.0, cos = 0.0;
  for (uint16_t i = 0; i < wind_degrees; i++) {
    const float ns = a*sin + b*cos;
    const float nc = a*cos - b*sin;
    cos = nc;
    sin = ns;
  }
  // wind dir rotates clockwise so compensate
  int ex = cx-rad*cos, ey = cy-rad*sin;
  tft.fillCircle(ex, ey, 3, BLACK);
  tft.drawLine(ex, ey, ex+wind_speed*(cx-ex)/50, ey+wind_speed*(cy-ey)/50, BLACK);
}

static void display_wind_speed(int wind_speed, const char *wind_dir, const char *wind_unit) {
  tft.setTextSize(2);
  tft.setCursor(1, 1);
  tft.print(wind_speed);
  tft.setTextSize(1);
  tft.print(wind_unit);
  tft.setCursor(1, 17);
  tft.print(wind_dir);
}

static void display_temperature(int temp, int temp_min, char temp_unit) {
  tft.setTextSize(2);
  tft.setCursor(1, tft.height() - 16);
  tft.print(temp);
  tft.setTextSize(1);
  tft.print(temp_unit);
  if (temp != temp_min) {
    tft.setCursor(1, tft.height() - 24);
    tft.print(temp_min);
  }
}

static void display_humidity(int humidity) {
  tft.setTextSize(2);
  tft.setCursor(right(val_len(humidity), tft.width(), 2) - 6, tft.height() - 16);
  tft.print(humidity);
  tft.setTextSize(1);
  tft.setCursor(tft.width() - 6, tft.height() - 16);
  tft.print('%');
}

void display_weather(struct Conditions &c) {

  tft.fillScreen(WHITE);
  tft.setTextColor(BLACK);

  display_wind_speed(c.wind, c.wind_dir, c.wind_unit);

  display_temperature(c.temp, c.feelslike, c.temp_unit);

  display_humidity(c.humidity);

  tft.setTextSize(2);
  tft.setCursor(right(val_len(c.atmos_pressure), tft.width(), 2)-6*strlen(c.pres_unit), 1);
  tft.print(c.atmos_pressure);
  tft.setTextSize(1);
  tft.print(c.pres_unit);
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
  bmp_draw(tft, c.icon, tft.width()/2 - 25, 42);

  display_time(c.epoch);

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
  bmp_draw(tft, buf, tft.width()/2 - 25, 42);

  tft.setCursor(centre_text(c.moon_phase, tft.width()/2, 1), 92);
  tft.print(c.moon_phase);

  display_time(c.epoch);
}

void display_forecast(struct Forecast &f) {

  tft.fillScreen(WHITE);
  tft.setTextColor(BLACK);

  display_wind_speed(f.ave_wind, f.wind_dir, conditions.wind_unit);

  display_temperature(f.temp_high, f.temp_low, conditions.temp_unit);

  display_humidity(f.ave_humidity);

  tft.setTextSize(2);
  tft.setCursor(right(3, tft.width(), 2), 1);
  tft.print(f.day);

  tft.setTextSize(1);
  tft.setCursor(centre_text(f.conditions, tft.width()/2, 1), 34);
  tft.print(f.conditions);
  bmp_draw(tft, f.icon, tft.width()/2 - 25, 42);

  display_time(f.epoch);

  display_wind(f.wind_degrees, f.ave_wind);
}

void update_display(int screen) {
  switch (screen) {
    case 0:
      display_weather(conditions);
      break;
    case 1:
    case 2:
    case 3:
    case 4:
      display_forecast(forecasts[screen - 1]);
      break;
    case 5:
      display_astronomy(conditions);
      break;
  }
}

bool connect_and_get(WiFiClient &client, const char *path) {
  if (client.connect(host, 80)) {
    client.print(String("GET /api/") + cfg.key + "/" + path + "/q/" + cfg.station
                 + ".json HTTP/1.1\r\n"
                 + "Host: " + host + "\r\n"
                 + "Connection: close\r\n"
                 + "\r\n");
    if (client.connected()) {
      client.find("\r\n\r\n");
      return true;
    }
  }
  return false;
}

void loop() {

  static int screen = 0;

  ArduinoOTA.handle();
  server.handleClient();

  uint32_t now = millis();
  bool swtch = !digitalRead(SWITCH);

  if (fade == cfg.dim) {
    if (swtch) {
      display_on = now;
      fade = cfg.bright;
      analogWrite(TFT_LED, fade);
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
  
  if (now - last_fetch_conditions > cfg.conditions_interval) {
#ifdef DEBUG
    out.println(F("Updating conditions..."));
#endif
    last_fetch_conditions = now;
    WiFiClient client;
    if (connect_and_get(client, "astronomy/conditions")) {
      const size_t bufferSize = JSON_OBJECT_SIZE(0) + 9 * JSON_OBJECT_SIZE(2) + 2 * JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(4) + 
                                JSON_OBJECT_SIZE(8) + JSON_OBJECT_SIZE(9) + JSON_OBJECT_SIZE(12) + JSON_OBJECT_SIZE(56) + 2530;
      DynamicJsonBuffer buffer(bufferSize);
      update_conditions(buffer.parseObject(client), conditions);
      update_display(screen);
      client.stop();
    } else
      out.println(F("Failed to fetch conditions!"));
  }

  if (now - last_fetch_forecasts > cfg.forecasts_interval) {
#ifdef DEBUG
    out.println(F("Updating forecast..."));
#endif
    last_fetch_forecasts = now;
    WiFiClient client;
    if (connect_and_get(client, "forecast")) {
      const size_t bufferSize = JSON_ARRAY_SIZE(4) + JSON_ARRAY_SIZE(8) + 2*JSON_OBJECT_SIZE(1) + 35*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + 
                                8*JSON_OBJECT_SIZE(4) + 8*JSON_OBJECT_SIZE(7) + 4*JSON_OBJECT_SIZE(17) + 4*JSON_OBJECT_SIZE(20);
      DynamicJsonBuffer forecast(bufferSize);
      update_forecasts(forecast.parseObject(client));
      client.stop();
    } else
      out.println(F("Failed to fetch forecast!"));
  }
}