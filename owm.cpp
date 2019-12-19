#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <time.h>
#include <TimeLib.h>
#include <Timezone.h>

#include "Configuration.h"
#include "state.h"
#include "providers.h"
#include "dbg.h"
#include "zone.h"

const char host[] = "api.openweathermap.org";
static bool forecast;

static Timezone tz(summer, winter);

void OpenWeatherMap::on_connect(WiFiClient &client, bool conds) {
	client.print(F("/data/2.5/"));
	if (conds)
		client.print(F("weather"));
	else
		client.print(F("forecast"));

	if (cfg.nearest) {
		client.print(F("?lat="));
		client.print(cfg.lat);
		client.print(F("&lon="));
		client.print(cfg.lon);
	} else {
		client.print(F("?q="));
		client.print(cfg.station);
	}

	if (!conds)
		client.print(F("&cnt=16"));

	client.print(F("&appid="));
	client.print(cfg.key);
	client.print(F("&units="));
	if (cfg.metric)
		client.print(F("metric"));
	else
		client.print(F("imperial"));
}

bool OpenWeatherMap::update_conditions(JsonDocument &root, struct Conditions &c) {
	time_t epoch = tz.toLocal((time_t)root[F("dt")]);

	if (epoch <= c.epoch)
		return false;

	stats.num_updates++;
	if (c.epoch)
		stats.update(epoch - c.epoch);
	c.epoch = epoch;
	c.age_of_moon = moon_age(epoch);
	strncpy_P(c.moon_phase, moon_phase(c.age_of_moon), sizeof(c.moon_phase));

	const JsonObject &w = root[F("weather")][0];
	const char *desc = w[F("description")] | "";
	int l = strlen(desc);
	if (l >= sizeof(c.weather) || l == 0)
		desc = w[F("main")] | "";
	strlcpy(c.weather, desc, sizeof(c.weather));
	strlcpy(c.icon, w[F("icon")] | "", sizeof(c.icon));

	const JsonObject &main = root[F("main")];
	c.temp = main[F("temp")];
	c.feelslike = main[F("temp_min")];	// hmmm
	c.pressure = main[F("pressure")];
	c.humidity = main[F("humidity")];

	const JsonObject &wind = root[F("wind")];
	c.wind = ceil(float(wind[F("speed")]) * 3.6);
	c.wind_degrees = wind[F("deg")];

	const JsonObject &sys = root["sys"];
	time_t sunrise = tz.toLocal((time_t)sys["sunrise"]);
	struct tm *sr = gmtime(&sunrise);
	c.sunrise_hour = sr->tm_hour;
	c.sunrise_minute = sr->tm_min;

	time_t sunset = tz.toLocal((time_t)sys["sunset"]);
	struct tm *ss = gmtime(&sunset);
	c.sunset_hour = ss->tm_hour;
	c.sunset_minute = ss->tm_min;

	strlcpy(c.city, root[F("name")] | "", sizeof(c.city));
	strlcat(c.city, ", ", sizeof(c.city));
	strlcat(c.city, sys[F("country")], sizeof(c.city));
	return true;
}

const unsigned cbytes = JSON_ARRAY_SIZE(3) + 2*JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + 3*JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(12) + 976;

bool OpenWeatherMap::fetch_conditions(struct Conditions &conditions) {
	WiFiClient client;
	bool ret = false;

	if (connect_and_get(client, host, true)) {
		DynamicJsonDocument doc(cbytes);
		auto error = deserializeJson(doc, client);
		if (error) {
			ERR(println(F("Failed to parse!")));
			stats.parse_failures++;
		} else {
			update_conditions(doc, conditions);
			DBG(print(F("Done ")));
			DBG(println(doc.memoryUsage()));
			ret = true;
		}
	}
	client.stop();
	return ret;
}

static void update_forecasts(JsonDocument &root, struct Forecast fs[], int n) {
	int cnt = root[F("cnt")];
	const JsonArray &list = root[F("list")];

	for (int i = 0, j = 0; i < n && j < cnt; i++, j += 2) {
		struct Forecast &f = fs[i];

		const JsonObject &fc = list[j];
		f.epoch = fc[F("dt")];

		const JsonObject &main = fc[F("main")];
		f.temp_high = main[F("temp_max")];
		f.temp_low = main[F("temp_min")];
		f.humidity = main[F("humidity")];

		const JsonObject &weather = fc[F("weather")][0];
		strlcpy(f.conditions, weather[F("description")], sizeof(f.conditions));
		strlcpy(f.icon, weather[F("icon")], sizeof(f.icon));

		const JsonObject &wind = fc[F("wind")];
		f.wind_degrees = wind[F("deg")];
		f.ave_wind = ceil(float(wind[F("speed")]) * 3.6);
	}
}

const unsigned fbytes = 16*JSON_ARRAY_SIZE(1) + JSON_ARRAY_SIZE(16) + 2*JSON_OBJECT_SIZE(0) + 46*JSON_OBJECT_SIZE(1) + 17*JSON_OBJECT_SIZE(2) + 17*JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + 32*JSON_OBJECT_SIZE(8) + 12603;

bool OpenWeatherMap::fetch_forecasts(struct Forecast forecasts[], int days) {
	WiFiClient client;
	bool ret = false;

	forecast = true;
	if (connect_and_get(client, host, false)) {
		DynamicJsonDocument doc(fbytes);
		auto error = deserializeJson(doc, client);
		if (error) {
			ERR(println(F("Failed to parse!")));
			stats.parse_failures++;
		} else {
			update_forecasts(doc, forecasts, days);
			DBG(print(F("Done ")));
			DBG(println(doc.memoryUsage()));
			ret = true;
		}
	}
	forecast = false;
	client.stop();
	return ret;
}
