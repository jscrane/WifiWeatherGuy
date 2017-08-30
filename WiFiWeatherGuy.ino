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

const size_t bufferSize = JSON_OBJECT_SIZE(0) + 9 * JSON_OBJECT_SIZE(2) + 2 * JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(8) + JSON_OBJECT_SIZE(9) + JSON_OBJECT_SIZE(12) + JSON_OBJECT_SIZE(56) + 2530;
DynamicJsonBuffer jsonBuffer(bufferSize);

uint32_t last_fetch = 0;
uint32_t update_interval;
uint32_t display_on = 0;
uint32_t on_time;
uint8_t bright = 255, dim = 0, fade;

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
  tft.println(WiFi.localIP());

  ArduinoOTA.setHostname("WiFiWeatherGuy");
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

const char *icon;
const char *weather;
int temp, feelslike;
char temp_unit;
const char *humidity;
int wind;
const char *wind_unit;
int atmos_pressure;
const char *pres_unit;
const char *pressure_trend;
const char *wind_dir;
const char *sunrise_hour;
const char *sunrise_minute;
const char *sunset_hour;
const char *sunset_minute;
time_t epoch;
const char *city;
int wind_degrees;

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

void update_display() {

  tft.fillScreen(WHITE);
  tft.setTextColor(BLACK);
  tft.setTextSize(2);
  tft.setCursor(1, 1);
  tft.print(wind);
  tft.setTextSize(1);
  tft.print(wind_unit);
  tft.setCursor(1, 17);
  tft.print(wind_dir);

  tft.setTextSize(2);
  tft.setCursor(1, tft.height() - 16);
  tft.print(temp);
  tft.setTextSize(1);
  tft.print(temp_unit);
  if (temp != feelslike) {
    tft.setCursor(1, tft.height() - 24);
    tft.print(feelslike);
  }

  tft.setTextSize(2);
  int h = atoi(humidity);
  tft.setCursor(right(val_len(h), tft.width(), 2) - 6, tft.height() - 16);
  tft.print(h);
  tft.setTextSize(1);
  tft.setCursor(tft.width() - 6, tft.height() - 16);
  tft.print('%');

  tft.setTextSize(2);
  tft.setCursor(right(val_len(atmos_pressure), tft.width(), 2)-6*strlen(pres_unit), 1);
  tft.print(atmos_pressure);
  tft.setTextSize(1);
  tft.print(pres_unit);
  int atmos_rising = atoi(pressure_trend);
  if (atmos_rising == 1) {
    tft.setCursor(right(6, tft.width(), 1), 17);
    tft.print(F("rising"));
  } else if (atmos_rising == -1) {
    tft.setCursor(right(7, tft.width(), 1), 17);
    tft.print(F("falling"));
  }
  
  tft.setCursor(centre_text(city, tft.width()/2, 1), 25);
  tft.print(city);
  tft.setCursor(centre_text(weather, tft.width()/2, 1), 34);
  tft.print(weather);
  bmp_draw(icon, tft.width()/2 - 25, 42);
  char buf[32];
  strftime(buf, sizeof(buf), metric? "%H:%S": "%I:%S%p", localtime(&epoch));
  tft.setCursor(centre_text(buf, tft.width()/2, 1), 109);
  tft.print(buf);
  strftime(buf, sizeof(buf), "%a %d", localtime(&epoch));
  tft.setCursor(centre_text(buf, tft.width()/2, 1), 118);
  tft.print(buf);

  // http://www.iquilezles.org/www/articles/sincos/sincos.htm
  int rad = 60, cx = 80, cy = 64;
  const float a = 0.999847695, b = 0.017452406;
  // wind dir is azimuthal angle with N at 0
  float s = 1.0, c = 0.0;
  for (uint16_t i = 0; i < wind_degrees; i++) {
    const float ns = a*s + b*c;
    const float nc = a*c - b*s;
    c = nc;
    s = ns;
  }
  // wind dir rotates clockwise so compensate
  int ex = cx-rad*c, ey = cy-rad*s;
  tft.fillCircle(ex, ey, 3, BLACK);
  tft.drawLine(ex, ey, ex + wind*(cx - ex)/50, ey+wind*(cy-ey)/50, BLACK);
}

void loop() {

  ArduinoOTA.handle();

  uint32_t now = millis();

  if (fade == dim) {
    if (!digitalRead(SWITCH)) {
      display_on = now;
      fade = bright;
      analogWrite(TFT_LED, fade);
    }
  } else if (now - display_on > on_time) {
    analogWrite(TFT_LED, --fade);
    delay(25);
  }
  
  if (now - last_fetch > update_interval) {
#ifdef DEBUG
    out.println(F("Updating..."));
#endif
    last_fetch = now;
    WiFiClient client;
    if (client.connect(host, 80)) {
      client.print(String("GET /api/") + key + "/astronomy/conditions/q/" + station
                   + ".json HTTP/1.1\r\n"
                   + "Host: " + host + "\r\n"
                   + "Connection: close\r\n"
                   + "\r\n");
      if (client.connected()) {
        client.find("\r\n\r\n");
        JsonObject &root = jsonBuffer.parseObject(client);
        JsonObject& current_observation = root["current_observation"];
        epoch = (time_t)atoi(current_observation["observation_epoch"]);
        icon = current_observation["icon"];
        weather = current_observation["weather"];
        if (metric) {
          temp = current_observation["temp_c"];
          feelslike = atoi(current_observation["feelslike_c"]);
          temp_unit = 'C';
          wind = current_observation["wind_kph"];
          wind_unit = "kph";
          atmos_pressure = atoi(current_observation["pressure_mb"]);
          pres_unit = "mb";
        } else {
          temp = current_observation["temp_f"];
          feelslike = atoi(current_observation["feelslike_f"]);
          temp_unit = 'F';          
          wind = current_observation["wind_mph"];
          wind_unit = "mph";
          atmos_pressure = atoi(current_observation["pressure_in"]);
          pres_unit = "in";
        }
        humidity = current_observation["relative_humidity"];
        pressure_trend = current_observation["pressure_trend"];
        wind_dir = current_observation["wind_dir"];
        wind_degrees = current_observation["wind_degrees"];
        city = current_observation["observation_location"]["city"];
        sunrise_hour = root["sun_phase"]["sunrise"]["hour"];
        sunrise_minute = root["sun_phase"]["sunrise"]["minute"];
        sunset_hour = root["sun_phase"]["sunset"]["hour"];
        sunset_minute = root["sun_phase"]["sunset"]["minute"];
        update_display();
      }
      client.stop();
    } else
      out.println(F("Connection failed!"));
  }
}
