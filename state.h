struct Conditions {
	char icon[20];
	char weather[32];
	int temp, feelslike;
	int humidity;
	int wind;
	int atmos_pressure;
	int pressure_trend;
	char wind_dir[10];
	int sunrise_hour;
	char sunrise_minute[3];
	int sunset_hour;
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
};

extern struct Statistics stats;
