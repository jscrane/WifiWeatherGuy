#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <time.h>
#include <Timezone.h>

#include "Configuration.h"
#include "dbg.h"
#include "state.h"
#include "providers.h"
#include "jsonclient.h"

OpenMeteo::OpenMeteo(): Provider(F("api.open-meteo.com")) {}

void OpenMeteo::begin() {

	Provider::begin();

	if (cfg.nearest)
		return;

	if (!cfg.station) {
		ERR(println("No City or Station configured!"));
		return;
	}

	JsonClient client(F("geocoding-api.open-meteo.com"));
	char path[80];
	snprintf(path, sizeof(path), "/v1/search?count=1&name=%s", cfg.station);
	if (client.get(path)) {

		extern struct Conditions conditions;
		JsonDocument doc;
		DeserializationError error = deserializeJson(doc, client);
		if (error) {
			ERR(print(F("Deserializing geocoding-api.com response: ")));
			ERR(println(error.f_str()));
			return;
		}

		JsonObject results_0 = doc[F("results")][0];
		cfg.lat = results_0[F("latitude")];
		cfg.lon = results_0[F("longitude")];
		strncpy_P(conditions.city, results_0[F("name")], sizeof(conditions.city));
	}
}

void OpenMeteo::on_connect(Stream &client, bool is_fetch_conditions) {

	client.print(F("/v1/forecast"));
	client.print(F("?latitude="));
	client.print(cfg.lat);
	client.print(F("&longitude="));
	client.print(cfg.lon);
	client.print(F("&timeformat=unixtime&timezone=auto"));

	if (cfg.metric)
		client.print(F("&temperature_unit=celsius&wind_speed_unit=kmh&precipitation_unit=mm"));
	else
		client.print(F("&temperature_unit=fahrenheit&wind_speed_unit=mph&precipitation_unit=inch"));

	if (is_fetch_conditions)
		client.print(F("&hourly=temperature_2m&forecast_hours=0&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,weather_code,surface_pressure,wind_speed_10m,wind_direction_10m&forecast_days=1&daily=sunrise,sunset"));

	else
		client.print(F("&hourly=temperature_2m&forecast_hours=0&current=temperature_2m&daily=weather_code,temperature_2m_max,temperature_2m_min,apparent_temperature_max,apparent_temperature_min,sunrise,sunset,wind_speed_10m_max,wind_gusts_10m_max,wind_direction_10m_dominant&forecast_days=7"));
}

bool OpenMeteo::update_conditions(JsonDocument &doc, struct Conditions &c) {

	const JsonObject &current = doc[F("current")];
	long current_time = current[F("time")];
	time_t epoch = (time_t)current_time;
	if (epoch <= c.epoch)
		return false;

	if (c.epoch)
		stats.update(epoch - c.epoch);
	c.epoch = epoch;
	c.age_of_moon = moon_age(epoch);
	strncpy_P(c.moon_phase, moon_phase(c.age_of_moon), sizeof(c.moon_phase));

	int current_interval = current[F("interval")];
	float current_temperature_2m = current[F("temperature_2m")];
	c.temp = (int)(0.5 + current_temperature_2m);

	float current_apparent_temperature = current[F("apparent_temperature")];
	c.feelslike = (int)(0.5 + current_apparent_temperature);

	int current_relative_humidity_2m = current[F("relative_humidity_2m")];
	c.humidity = current_relative_humidity_2m;

	int current_is_day = current[F("is_day")];
	int current_weather_code = current[F("weather_code")];
	strncpy_P(c.weather, weather_description(current_weather_code), sizeof(c.weather));
	snprintf(c.icon, sizeof(c.icon), "%d%c", current_weather_code, current_is_day? 'd': 'n');

	int current_surface_pressure = current[F("surface_pressure")];
	c.pressure = current_surface_pressure;
	c.pressure_trend = 0;

	int current_wind_speed_10m = current[F("wind_speed_10m")];
	c.wind = current_wind_speed_10m;

	int current_wind_direction_10m = current[F("wind_direction_10m")];
	c.wind_degrees = current_wind_direction_10m;
	
	const JsonObject &daily = doc[F("daily")];
	long daily_time_0 = daily[F("time")][0];
	long daily_sunrise_0 = daily[F("sunrise")][0];
	time_t sunrise = (time_t)daily_sunrise_0;
	struct tm *sr = gmtime(&sunrise);
	c.sunrise_hour = sr->tm_hour;
	c.sunrise_minute = sr->tm_min;

	long daily_sunset_0 = daily[F("sunset")][0];
	time_t sunset = (time_t)daily_sunset_0;
	struct tm *ss = gmtime(&sunset);
	c.sunset_hour = ss->tm_hour;
	c.sunset_minute = ss->tm_min;

	return true;
}

bool OpenMeteo::update_forecasts(JsonDocument &doc, struct Forecast forecasts[], int days) {

	JsonObject current = doc[F("current")];
	long current_time = current[F("time")];
	
	JsonObject daily = doc[F("daily")];
	JsonArray daily_time = daily[F("time")];
	JsonArray daily_weather_code = daily[F("weather_code")];
	JsonArray daily_temperature_2m_max = daily[F("temperature_2m_max")];
	JsonArray daily_temperature_2m_min = daily[F("temperature_2m_min")];
	JsonArray daily_apparent_temperature_max = daily[F("apparent_temperature_max")];
	JsonArray daily_apparent_temperature_min = daily[F("apparent_temperature_min")];
	JsonArray daily_sunrise = daily[F("sunrise")];
	JsonArray daily_sunset = daily[F("sunset")];
	JsonArray daily_wind_speed_10m_max = daily[F("wind_speed_10m_max")];
	JsonArray daily_wind_gusts_10m_max = daily[F("wind_gusts_10m_max")];
	JsonArray daily_wind_direction_10m_dominant = daily[F("wind_direction_10m_dominant")];
	for (int i = 0; i < days; i++) {
		struct Forecast &f = forecasts[i];
		f.humidity = -1;	// not available

		long daily_time_i = daily_time[i];
		f.epoch = (time_t)daily_time_i;

		int daily_weather_code_i = daily_weather_code[i];
		strncpy_P(f.conditions, weather_description(daily_weather_code), sizeof(f.conditions));
		snprintf(f.icon, sizeof(f.icon), "%dd", daily_weather_code);

		float daily_temperature_2m_max_i = daily_temperature_2m_max[i];
		f.temp_high = (int)(0.5 + daily_temperature_2m_max_i);

		float daily_temperature_2m_min_i = daily_temperature_2m_min[i];
		f.temp_low = (int)(0.5 + daily_temperature_2m_min_i);

		float daily_apparent_temperature_max_i = daily_apparent_temperature_max[i];
		float daily_apparent_temperature_min_i = daily_apparent_temperature_min[i];
		long daily_sunrise_i = daily_sunrise[i];
		long daily_sunset_i = daily_sunset[i];

		float daily_wind_speed_10m_max_i = daily_wind_speed_10m_max[i];
		f.ave_wind = (int)(0.5 + daily_wind_speed_10m_max_i);

		float daily_wind_gusts_10m_max_i = daily_wind_gusts_10m_max[i];
		f.max_wind = (int)(0.5 + daily_wind_gusts_10m_max_i);

		int daily_wind_direction_10m_dominant_i = daily_wind_direction_10m_dominant[i];
		f.wind_degrees = daily_wind_direction_10m_dominant_i;
	}
	return true;
}
