#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <time.h>
#include <Timezone.h>

#include "Configuration.h"
#include "dbg.h"
#include "state.h"
#include "providers.h"

const unsigned cbytes = JSON_OBJECT_SIZE(0) + 9 * JSON_OBJECT_SIZE(2) + 2 * JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(4) +
			JSON_OBJECT_SIZE(8) + JSON_OBJECT_SIZE(9) + JSON_OBJECT_SIZE(12) + JSON_OBJECT_SIZE(56) + 2530;

const unsigned fbytes = JSON_ARRAY_SIZE(4) + JSON_ARRAY_SIZE(8) + 2*JSON_OBJECT_SIZE(1) + 35*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) +
			8*JSON_OBJECT_SIZE(4) + 8*JSON_OBJECT_SIZE(7) + 4*JSON_OBJECT_SIZE(17) + 4*JSON_OBJECT_SIZE(20) + 6150;

Wunderground::Wunderground(): Provider(cbytes, fbytes, F("api.wunderground.com")) {}

bool Wunderground::update_conditions(JsonDocument &root, struct Conditions &c) {
	const JsonObject &current_observation = root[F("current_observation")];
	if (current_observation.isNull())
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
	c.wind_degrees = current_observation[F("wind_degrees")];
	strlcpy(c.city, current_observation[F("observation_location")][F("city")] | "", sizeof(c.city));

	const JsonObject &sun = root[F("sun_phase")];
	c.sunrise_hour = atoi(sun[F("sunrise")][F("hour")] | "0");
	c.sunrise_minute = atoi(sun[F("sunrise")][F("minute")] | "0");
	c.sunset_hour = atoi(sun[F("sunset")][F("hour")] | "0");
	c.sunset_minute = atoi(sun[F("sunset")][F("minute")] | "0");

	const JsonObject &moon = root[F("moon_phase")];
	c.age_of_moon = atoi(moon[F("ageOfMoon")] | "0");
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

bool Wunderground::update_forecasts(JsonDocument &root, struct Forecast fs[], int n) {
	const JsonArray &days = root[F("forecast")][F("simpleforecast")][F("forecastday")];
	if (days.isNull())
		return false;

	for (int i = 0; i < days.size() && i < n; i++) {
		const JsonObject &day = days[i];
		struct Forecast &f = fs[i];
		f.epoch = (time_t)atoi(day[F("date")][F("epoch")] | "0");
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
		f.humidity = day[F("avehumidity")];
		f.wind_degrees = day[F("avewind")][F("degrees")];
		strlcpy(f.conditions, day[F("conditions")] | "", sizeof(f.conditions));
		strlcpy(f.icon, day[F("icon")] | "", sizeof(f.icon));
	}
	return true;
}

void Wunderground::on_connect(Stream &client, bool conds) {
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

	client.print(conds? F("astronomy/conditions"): F("forecast"));
	client.print(F("/q/"));
	client.print(loc);
	client.print(F(".json"));
}
