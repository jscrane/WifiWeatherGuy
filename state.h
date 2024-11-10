#pragma once

struct Conditions {
	time_t epoch;
	char city[48];
	char icon[20];
	char weather[16];
	int temp, feelslike;
	int humidity;
	int wind, wind_degrees;
	int pressure;
	int pressure_trend;
	int sunrise_hour, sunrise_minute;
	int sunset_hour, sunset_minute;
	char moonrise_hour[3];
	char moonrise_minute[3];
	char moonset_hour[3];
	char moonset_minute[3];
	char moon_phase[16];
	int age_of_moon;
};

struct Forecast {
	time_t epoch;
	int temp_high;
	int temp_low;
	int max_wind;
	int ave_wind;
	int wind_degrees;
	int humidity;
	char conditions[16];
	char icon[20];
};

struct Statistics {
	time_t last_age, min_age, max_age, total;
	uint32_t last_fetch_conditions, last_fetch_forecasts;
	unsigned num_updates;
	unsigned connect_failures;
	unsigned parse_failures;
	unsigned mem_failures;

	void update(time_t age) {
		last_age = age;
		total += age;
		if (age > max_age)
			max_age = age;
		if (age < min_age || !min_age)
			min_age = age;
	}
};

extern struct Statistics stats;
