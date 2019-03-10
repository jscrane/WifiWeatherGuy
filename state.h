#ifndef __STATE_H__
#define __STATE_H__

struct Conditions {
	time_t epoch;
	char city[48];
	char icon[20];
	char weather[32];
	int temp, feelslike;
	int humidity;
	int wind, wind_degrees;
	int atmos_pressure;
	int pressure_trend;
	char wind_dir[10];
	int sunrise_hour, sunrise_minute;
	int sunset_hour, sunset_minute;
	char moonrise_hour[3];
	char moonrise_minute[3];
	char moonset_hour[3];
	char moonset_minute[3];
	char moon_phase[16];
	char age_of_moon[3];
};

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

#endif
