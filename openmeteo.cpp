#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <time.h>
#include <Timezone.h>

#include "Configuration.h"
#include "dbg.h"
#include "state.h"
#include "providers.h"

OpenMeteo::OpenMeteo(): Provider(4096, 4096, F("api.open-meteo.com")) {}

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

	int utc_offset_seconds = doc["utc_offset_seconds"]; // 0
	const char* timezone = doc["timezone"]; // "Europe/Dublin"
	const char* timezone_abbreviation = doc["timezone_abbreviation"]; // "GMT"
	
	const JsonObject &current = doc["current"];
	long current_time = current["time"]; // 1730971800
	time_t epoch = tz->toLocal((time_t)current_time);
	if (epoch <= c.epoch)
		return false;

	stats.num_updates++;
	stats.update(epoch - c.epoch);
	c.epoch = epoch;
	c.age_of_moon = moon_age(epoch);
	strncpy_P(c.moon_phase, moon_phase(c.age_of_moon), sizeof(c.moon_phase));

	int current_interval = current["interval"]; // 900
	float current_temperature_2m = current["temperature_2m"]; // 13.6
	c.temp = (int)(0.5 + current_temperature_2m);

	float current_apparent_temperature = current["apparent_temperature"]; // 12.1
	c.feelslike = (int)(0.5 + current_apparent_temperature);

	int current_relative_humidity_2m = current["relative_humidity_2m"]; // 88
	c.humidity = current_relative_humidity_2m;

	int current_is_day = current["is_day"]; // 1
	int current_weather_code = current["weather_code"]; // 3
	strncpy_P(c.weather, weather_description(current_weather_code), sizeof(c.weather));
	snprintf(c.icon, sizeof(c.icon), "%d%c", current_weather_code, current_is_day? 'd': 'n');

	int current_surface_pressure = current["surface_pressure"]; // 1022
	c.pressure = current_surface_pressure;

	int current_wind_speed_10m = current["wind_speed_10m"]; // 14
	c.wind = current_wind_speed_10m;

	int current_wind_direction_10m = current["wind_direction_10m"]; // 134
	c.wind_degrees = current_wind_direction_10m;
	
	const JsonObject &daily = doc["daily"];
	long daily_time_0 = daily["time"][0]; // 1730937600
	long daily_sunrise_0 = daily["sunrise"][0]; // 1730964978
	time_t sunrise = tz->toLocal(daily_sunrise_0);
	struct tm *sr = gmtime(&sunrise);
	c.sunrise_hour = sr->tm_hour;
	c.sunrise_minute = sr->tm_min;

	long daily_sunset_0 = daily["sunset"][0]; // 1730997677
	time_t sunset = tz->toLocal((time_t)daily_sunset_0);
	struct tm *ss = gmtime(&sunset);
	c.sunset_hour = ss->tm_hour;
	c.sunset_minute = ss->tm_min;

	return true;
}

bool OpenMeteo::update_forecasts(JsonDocument &doc, struct Forecast forecasts[], int days) {

	int utc_offset_seconds = doc["utc_offset_seconds"]; // 0
	const char* timezone = doc["timezone"]; // "Europe/Dublin"
	const char* timezone_abbreviation = doc["timezone_abbreviation"]; // "GMT"
	
	JsonObject current = doc["current"];
	long current_time = current["time"]; // 1730982600
	
	JsonObject daily = doc["daily"];
	JsonArray daily_time = daily["time"];
	JsonArray daily_weather_code = daily["weather_code"];
	JsonArray daily_temperature_2m_max = daily["temperature_2m_max"];
	JsonArray daily_temperature_2m_min = daily["temperature_2m_min"];
	JsonArray daily_apparent_temperature_max = daily["apparent_temperature_max"];
	JsonArray daily_apparent_temperature_min = daily["apparent_temperature_min"];
	JsonArray daily_sunrise = daily["sunrise"];
	JsonArray daily_sunset = daily["sunset"];
	JsonArray daily_wind_speed_10m_max = daily["wind_speed_10m_max"];
	JsonArray daily_wind_gusts_10m_max = daily["wind_gusts_10m_max"];
	JsonArray daily_wind_direction_10m_dominant = daily["wind_direction_10m_dominant"];
	for (int i = 0; i < days; i++) {
		struct Forecast &f = forecasts[i];

		long daily_time_i = daily_time[i]; // 1730937600
		f.epoch = tz->toLocal((time_t)daily_time_i);

		int daily_weather_code_i = daily_weather_code[i]; // 3
		strncpy_P(f.conditions, weather_description(daily_weather_code), sizeof(f.conditions));
		// FIXME: icon

		float daily_temperature_2m_max_i = daily_temperature_2m_max[i]; // 14.2
		f.temp_high = (int)(0.5 + daily_temperature_2m_max_i);

		float daily_temperature_2m_min_i = daily_temperature_2m_min[i]; // 13.1
		f.temp_low = (int)(0.5 + daily_temperature_2m_min_i);

		float daily_apparent_temperature_max_i = daily_apparent_temperature_max[i]; // 12.1
		float daily_apparent_temperature_min_i = daily_apparent_temperature_min[i]; // 11.5
		long daily_sunrise_i = daily_sunrise[i]; // 1730964978
		long daily_sunset_i = daily_sunset[i]; // 1730997677

		float daily_wind_speed_10m_max_i = daily_wind_speed_10m_max[i]; // 16.9
		f.ave_wind = (int)(0.5 + daily_wind_speed_10m_max_i);

		float daily_wind_gusts_10m_max_i = daily_wind_gusts_10m_max[i]; // 34.9
		f.max_wind = (int)(0.5 + daily_wind_gusts_10m_max_i);

		int daily_wind_direction_10m_dominant_i = daily_wind_direction_10m_dominant[i]; // 148
		f.wind_degrees = daily_wind_direction_10m_dominant_i;
	}
	return true;
}
