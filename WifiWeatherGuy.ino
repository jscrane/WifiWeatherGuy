#include <ArduinoJson.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <TFT_ILI9163C.h>
#include <ESP8266WiFi.h>
#include <time.h>
#include <FS.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>

char ssid[32];
char password[32];
char key[16];
char station[32];
bool metric;
const char host[] = "api.wunderground.com";

#define BLACK   0x0000
#define WHITE   0xFFFF

#define CS D6
#define DC D8
#define TFT_LED D2
#define SWITCH D3

TFT_ILI9163C tft = TFT_ILI9163C(CS, DC);

Print &out = Serial;
#define DEBUG

uint32_t last_fetch = 0;
uint32_t update_interval;
uint32_t display_on = 0;
uint32_t on_time;
uint8_t bright = 255, dim = 0, fade;

extern int bmp_draw(TFT_ILI9163C &tft, const char *filename, uint8_t x, uint8_t y);

void setup() {

  Serial.begin(115200);

  tft.begin();
  tft.setTextColor(WHITE, BLACK);
  tft.setCursor(0, 0);

  pinMode(SWITCH, INPUT_PULLUP);
  fade = bright;
  analogWrite(TFT_LED, fade);

  tft.println(F("Weather Guy (c)2017"));

  bool result = SPIFFS.begin();
  out.print("SPIFFS: ");
  out.println(result);
  if (!result)
    return;

  File f = SPIFFS.open("/config.txt", "r");
  if (!f) {
    out.print(F("file.open!"));
    return;
  }
  char buf[512];
  f.readBytes(buf, sizeof(buf));
  char *b = buf, *p = strsep(&b, "=");
  while (p) {
    if (strcmp(p, "ssid") == 0)
      strcpy(ssid, strsep(&b, "\n"));
    else if (strcmp(p, "password") == 0)
      strcpy(password, strsep(&b, "\n"));
    else if (strcmp(p, "key") == 0)
      strcpy(key, strsep(&b, "\n"));
    else if (strcmp(p, "station") == 0)
      strcpy(station, strsep(&b, "\n"));
    else if (strcmp(p, "update") == 0)
      update_interval = 1000*atoi(strsep(&b, "\n"));
    else if (strcmp(p, "metric") == 0)
      metric = (bool)atoi(strsep(&b, "\n"));
    else if (strcmp(p, "display") == 0)
      on_time = 1000*atoi(strsep(&b, "\n"));
    else if (strcmp(p, "bright") == 0)
      bright = atoi(strsep(&b, "\n"));
    else if (strcmp(p, "dim") == 0)
      dim = atoi(strsep(&b, "\n"));
    p = strsep(&b, "=");
  }
  f.close();

  tft.println();
  tft.print(F("ssid: "));
  tft.println(ssid);
  tft.print(F("password: "));
  tft.println(password);
  tft.print(F("key: "));
  tft.println(key);
  tft.print(F("station: "));
  tft.println(station);
  tft.print(F("update: "));
  tft.println(update_interval);
  tft.print(F("display: "));
  tft.println(on_time);
  tft.print(F("metric: "));
  tft.println(metric);
  tft.print(F("bright: "));
  tft.println(bright);
  tft.print(F("dim: "));
  tft.println(dim);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    out.print(F("."));
  }
#ifdef DEBUG  
  out.println();
  out.print(F("Connected to "));
  out.println(ssid);
  out.println(WiFi.localIP());
#endif
  tft.println();
  tft.println(WiFi.localIP());

  ArduinoOTA.setHostname("WifiWeatherGuy");
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
  
  last_fetch = -update_interval;
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
  if (metric) {
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
  int temp_high;
  int temp_low;
  int max_wind;
  int ave_wind;
  char wind_dir[8];
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
    if (metric) {
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

void display_weather(struct Conditions &c) {

  tft.fillScreen(WHITE);
  tft.setTextColor(BLACK);

  tft.setTextSize(2);
  tft.setCursor(1, 1);
  tft.print(c.wind);
  tft.setTextSize(1);
  tft.print(c.wind_unit);
  tft.setCursor(1, 17);
  tft.print(c.wind_dir);

  tft.setTextSize(2);
  tft.setCursor(1, tft.height() - 16);
  tft.print(c.temp);
  tft.setTextSize(1);
  tft.print(c.temp_unit);
  if (c.temp != c.feelslike) {
    tft.setCursor(1, tft.height() - 24);
    tft.print(c.feelslike);
  }

  tft.setTextSize(2);
  tft.setCursor(right(val_len(c.humidity), tft.width(), 2) - 6, tft.height() - 16);
  tft.print(c.humidity);
  tft.setTextSize(1);
  tft.setCursor(tft.width() - 6, tft.height() - 16);
  tft.print('%');

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
  char buf[32];
  strftime(buf, sizeof(buf), metric? "%H:%S": "%I:%S%p", localtime(&c.epoch));
  tft.setCursor(centre_text(buf, tft.width()/2, 1), 109);
  tft.print(buf);
  strftime(buf, sizeof(buf), "%a %d", localtime(&c.epoch));
  tft.setCursor(centre_text(buf, tft.width()/2, 1), 118);
  tft.print(buf);

  // http://www.iquilezles.org/www/articles/sincos/sincos.htm
  int rad = tft.width()/3, cx = tft.width()/2, cy = 68;
  const float a = 0.999847695, b = 0.017452406;
  // wind dir is azimuthal angle with N at 0
  float sin = 1.0, cos = 0.0;
  for (uint16_t i = 0; i < c.wind_degrees; i++) {
    const float ns = a*sin + b*cos;
    const float nc = a*cos - b*sin;
    cos = nc;
    sin = ns;
  }
  // wind dir rotates clockwise so compensate
  int ex = cx-rad*cos, ey = cy-rad*sin;
  tft.fillCircle(ex, ey, 3, BLACK);
  tft.drawLine(ex, ey, ex+c.wind*(cx-ex)/50, ey+c.wind*(cy-ey)/50, BLACK);
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

  strftime(buf, sizeof(buf), metric? "%H:%S": "%I:%S%p", localtime(&c.epoch));
  tft.setCursor(centre_text(buf, tft.width()/2, 1), 109);
  tft.print(buf);
  strftime(buf, sizeof(buf), "%a %d", localtime(&c.epoch));
  tft.setCursor(centre_text(buf, tft.width()/2, 1), 118);
  tft.print(buf);
}

void display_forecast(struct Forecast &f) {

  char buf[32];

  tft.fillScreen(WHITE);
  tft.setTextColor(BLACK);

  tft.setTextSize(2);
  tft.setCursor(1, 1);
  tft.print(f.ave_wind);
  tft.setTextSize(1);
  tft.print(conditions.wind_unit);
  tft.setCursor(1, 17);
  tft.print(f.wind_dir);

  tft.setTextSize(2);
  tft.setCursor(1, tft.height() - 16);
  tft.print(f.temp_high);
  tft.setTextSize(1);
  tft.print(conditions.temp_unit);
  tft.setCursor(1, tft.height() - 24);
  tft.print(f.temp_low);

  tft.setTextSize(2);
  tft.setCursor(right(val_len(f.ave_humidity), tft.width(), 2) - 6, tft.height() - 16);
  tft.print(f.ave_humidity);
  tft.setTextSize(1);
  tft.setCursor(tft.width() - 6, tft.height() - 16);
  tft.print('%');

  tft.setTextSize(2);
  tft.setCursor(right(3, tft.width(), 2), 1);
  strftime(buf, sizeof(buf), "%a", localtime(&f.epoch));
  tft.print(buf);

  tft.setTextSize(1);
  tft.setCursor(centre_text(f.conditions, tft.width()/2, 1), 34);
  tft.print(f.conditions);
  bmp_draw(tft, f.icon, tft.width()/2 - 25, 42);

  strftime(buf, sizeof(buf), metric? "%H:%S": "%I:%S%p", localtime(&f.epoch));
  tft.setCursor(centre_text(buf, tft.width()/2, 1), 109);
  tft.print(buf);
  strftime(buf, sizeof(buf), "%a %d", localtime(&f.epoch));
  tft.setCursor(centre_text(buf, tft.width()/2, 1), 118);
  tft.print(buf);
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
      display_forecast(forecasts[0]);
      break;
    case 3:
      display_forecast(forecasts[1]);
      break;
    case 4:
      display_forecast(forecasts[2]);
      break;
    case 5:
      display_forecast(forecasts[3]);
      break;
  }
}

bool connect_and_get(WiFiClient &client, const char *path) {
  if (client.connect(host, 80)) {
    client.print(String("GET /api/") + key + "/" + path + "/q/" + station
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

  uint32_t now = millis();
  bool swtch = !digitalRead(SWITCH);

  if (fade == dim) {
    if (swtch) {
      display_on = now;
      fade = bright;
      analogWrite(TFT_LED, fade);
    }
  } else if (now - display_on > on_time) {
    analogWrite(TFT_LED, --fade);
    delay(25);
  } else if (swtch && now - display_on > 500) {
    if (screen > 5)
      screen = 0;
    else
      screen++;
    update_display(screen);
  }
  
  if (now - last_fetch > update_interval) {
#ifdef DEBUG
    out.println(F("Updating conditions..."));
#endif
    last_fetch = now;
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

#ifdef DEBUG
    out.println(F("Updating forecast..."));
#endif
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
