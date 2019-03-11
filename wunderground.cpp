#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <time.h>

#include "Configuration.h"
#include "dbg.h"
#include "state.h"
#include "providers.h"

const char host[] PROGMEM = "api.wunderground.com";

static bool update_conditions(JsonObject &root, struct Conditions &c) {
	JsonObject &current_observation = root[F("current_observation")];
	if (!current_observation.success())
		return false;

	time_t epoch = (time_t)atoi(current_observation[F("observation_epoch")] | "0");
	if (epoch <= c.epoch)
		return false;	    

	stats.num_updates++;
	if (c.epoch)
		stats.update(epoch - c.epoch);

	c.epoch = epoch;
	strlcpy(c.weather, current_observation[F("weather")] | "", sizeof(c.weather));
	if (cfg.metric) {
		c.temp = current_observation[F("temp_c")];
		c.feelslike = atoi(current_observation[F("feelslike_c")] | "0");
		c.wind = current_observation[F("wind_kph")];
		c.pressure = atoi(current_observation[F("pressure_mb")] | "0");
	} else {
		c.temp = current_observation[F("temp_f")];
		c.feelslike = atoi(current_observation[F("feelslike_f")] | "0");
		c.wind = current_observation[F("wind_mph")];
		c.pressure = atoi(current_observation[F("pressure_in")] | "0");
	}
	c.humidity = atoi(current_observation[F("relative_humidity")] | "0");
	c.pressure_trend = atoi(current_observation[F("pressure_trend")] | "0");
	strlcpy(c.wind_dir, current_observation[F("wind_dir")] | "", sizeof(c.wind_dir));
	c.wind_degrees = current_observation[F("wind_degrees")];
	strlcpy(c.city, current_observation[F("observation_location")][F("city")] | "", sizeof(c.city));

	JsonObject &sun = root[F("sun_phase")];
	c.sunrise_hour = atoi(sun[F("sunrise")][F("hour")] | "0");
	c.sunrise_minute = atoi(sun[F("sunrise")][F("minute")] | "0");
	c.sunset_hour = atoi(sun[F("sunset")][F("hour")] | "0");
	c.sunset_minute = atoi(sun[F("sunset")][F("minute")] | "0");

	JsonObject &moon = root[F("moon_phase")];
	strlcpy(c.age_of_moon, moon[F("ageOfMoon")] | "", sizeof(c.age_of_moon));
	strlcpy(c.moon_phase, moon[F("phaseofMoon")] | "", sizeof(c.moon_phase));
	strlcpy(c.moonrise_hour, moon[F("moonrise")][F("hour")] | "", sizeof(c.moonrise_hour));
	strlcpy(c.moonrise_minute, moon[F("moonrise")][F("minute")] | "", sizeof(c.moonrise_minute));
	strlcpy(c.moonset_hour, moon[F("moonset")][F("hour")] | "", sizeof(c.moonset_hour));
	strlcpy(c.moonset_minute, moon[F("moonset")][F("minute")] | "", sizeof(c.moonset_minute));

	struct tm *tm = localtime(&epoch);
	const char *icon = current_observation[F("icon")] | "";
	if (!tm || (tm->tm_hour >= c.sunrise_hour && tm->tm_hour <= c.sunset_hour))
		strlcpy(c.icon, icon, sizeof(c.icon));
	else {
		strcpy_P(c.icon, PSTR("nt_"));
		strlcat(c.icon, icon, sizeof(c.icon)-3);
	}
	return true;
}

static void update_forecasts(JsonObject &root, struct Forecast fs[], int n) {
	JsonArray &days = root[F("forecast")][F("simpleforecast")][F("forecastday")];
	if (days.success())
		for (int i = 0; i < days.size() && i < n; i++) {
			JsonObject &day = days[i];
			struct Forecast &f = fs[i];
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

void Wunderground::on_connect(WiFiClient &client, const __FlashStringHelper *path) {
	char loc[sizeof(cfg.station)];

	client.print(F("/api/"));
	client.print(cfg.key);
	client.print('/');

	if (cfg.nearest) {
		client.print(F("geolookup/"));
		snprintf(loc, sizeof(loc), "%f,%f", cfg.lat, cfg.lon);
	} else
		strncpy(loc, cfg.station, sizeof(loc));
	DBG(println(loc));

	client.print(path);
	client.print(F("/q/"));
	client.print(loc);
	client.print(F(".json"));
}

const unsigned cbytes = JSON_OBJECT_SIZE(0) + 9 * JSON_OBJECT_SIZE(2) + 2 * JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(4) +
			JSON_OBJECT_SIZE(8) + JSON_OBJECT_SIZE(9) + JSON_OBJECT_SIZE(12) + JSON_OBJECT_SIZE(56) + 2530;

const unsigned fbytes = JSON_ARRAY_SIZE(4) + JSON_ARRAY_SIZE(8) + 2*JSON_OBJECT_SIZE(1) + 35*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) +
			8*JSON_OBJECT_SIZE(4) + 8*JSON_OBJECT_SIZE(7) + 4*JSON_OBJECT_SIZE(17) + 4*JSON_OBJECT_SIZE(20) + 6150;

static bool has_memory(unsigned need) {
	unsigned heap = ESP.getFreeHeap();
	if (heap <= need) {
		DBG(println(F("Insufficient memory to update!")));
		stats.mem_failures++;
		return false;
	}
	return true;

}

bool Wunderground::fetch_conditions(struct Conditions &conditions) {
	if (!has_memory(cbytes))
		return false;

	bool ret = false;
	WiFiClient client;
	if (connect_and_get(client, host, F("astronomy/conditions"))) {
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

bool Wunderground::fetch_forecasts(struct Forecast forecasts[], int days) {
	if (!has_memory(fbytes))
		return false;

	bool ret = false;
	WiFiClient client;
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