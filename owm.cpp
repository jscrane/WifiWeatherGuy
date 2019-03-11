#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <time.h>

#include "Configuration.h"
#include "state.h"
#include "providers.h"
#include "dbg.h"

const char host[] PROGMEM = "api.openweathermap.org";

void OpenWeatherMap::on_connect(WiFiClient &client, const __FlashStringHelper *path) {
	client.print(F("/data/2.5/"));
	client.print(path);

	if (cfg.nearest) {
		client.print(F("?lat="));
		client.print(cfg.lat);
		client.print(F("&lon="));
		client.print(cfg.lon);
	} else {
		client.print(F("?q="));
		client.print(cfg.station);
	}

	client.print(F("&appid="));
	client.print(cfg.key);
	client.print(F("&units="));
	if (cfg.metric)
		client.print(F("metric"));
	else
		client.print(F("imperial"));
}

static bool update_conditions(JsonObject &root, struct Conditions &c) {
	time_t epoch = (time_t)root[F("dt")];
	if (c.epoch == epoch)
		return false;

	c.epoch = epoch;

	JsonObject &w = root[F("weather")][0];
	strlcpy(c.weather, w[F("description")] | "", sizeof(c.weather));
	strlcpy(c.icon, w[F("icon")] | "", sizeof(c.icon));

	JsonObject &main = root[F("main")];
	c.temp = main[F("temp")];
	c.feelslike = main[F("temp_min")];	// hmmm
	c.pressure = main[F("pressure")];
	c.humidity = main[F("humidity")];

	JsonObject &wind = root[F("wind")];
	c.wind = ceil(float(wind[F("speed")]) * 3.6);
	c.wind_degrees = wind[F("deg")];

	JsonObject &sys = root["sys"];
	time_t sunrise = (time_t)sys["sunrise"];
	struct tm *sr = gmtime(&sunrise);
	c.sunrise_hour = sr->tm_hour;
	c.sunrise_minute = sr->tm_min;

	time_t sunset = (time_t)sys["sunset"];
	struct tm *ss = gmtime(&sunset);
	c.sunset_hour = ss->tm_hour;
	c.sunset_minute = ss->tm_min;

	strlcpy(c.city, root[F("name")] | "", sizeof(c.city));
	strlcat(c.city, ", ", sizeof(c.city));
	strlcat(c.city, sys[F("country")], sizeof(c.city));
	return true;
}

const unsigned cbytes = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + 2*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(12) + 956;

bool OpenWeatherMap::fetch_conditions(struct Conditions &conditions) {
	WiFiClient client;
	bool ret = false;

	if (connect_and_get(client, host, F("weather"))) {
		DynamicJsonBuffer buffer(cbytes);
		JsonObject &root = buffer.parseObject(client);
		if (ret = root.success()) {
			update_conditions(root, conditions);
			DBG(print(F("Done ")));
			DBG(println(buffer.size()));
		} else {
			ERR(println(F("Failed to parse!")));
			stats.parse_failures++;
		}
	}
	client.stop();
	return ret;
}

static void update_forecasts(JsonObject &root, struct Forecast fs[], int n) {
	int cnt = root[F("cnt")];
	JsonArray &list = root[F("list")];

	root.printTo(Serial);
	for (int i = 0; i < n && i < cnt; i++) {
	Serial.println(i);
		JsonObject &day = list[i];
		if (!day.success())
			break;
		struct Forecast &f = fs[i];
		f.epoch = day[F("dt")];
		f.ave_humidity = day[F("humidity")];
		f.wind_degrees = day[F("deg")];
		f.ave_wind = ceil(float(day[F("speed")]) * 3.6);

		JsonObject &weather = day[F("weather")];
		strlcpy(f.conditions, weather[F("main")], sizeof(f.conditions));
		strlcpy(f.icon, weather[F("icon")], sizeof(f.icon));

		JsonObject &temp = day[F("temp")];
		f.temp_high = temp[F("max")];
		f.temp_low = temp[F("min")];
	}
}

const unsigned fbytes = 7*JSON_ARRAY_SIZE(1) + JSON_ARRAY_SIZE(7) + JSON_OBJECT_SIZE(2) + 7*JSON_OBJECT_SIZE(4) + 2*JSON_OBJECT_SIZE(5) + 7*JSON_OBJECT_SIZE(6) + 2*JSON_OBJECT_SIZE(8) + 3*JSON_OBJECT_SIZE(9) + 2*JSON_OBJECT_SIZE(10) + 4349;

bool OpenWeatherMap::fetch_forecasts(struct Forecast forecasts[], int days) {
	WiFiClient client;
	bool ret = false;

	if (connect_and_get(client, host, F("forecast"))) {
		DynamicJsonBuffer buffer(fbytes);
		JsonObject &root = buffer.parseObject(client);
		if (ret = root.success()) {
			update_forecasts(root, forecasts, days);
			DBG(print(F("Done ")));
			DBG(println(buffer.size()));
		} else {
			ERR(println(F("Failed to parse!")));
			stats.parse_failures++;
		}
	}
	client.stop();
	return ret;
}
