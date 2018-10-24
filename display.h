#ifndef __DISPLAY_H__
#define __DISPLAY_H__

extern TFT_eSPI tft;

class config: public Configuration {
public:
	char ssid[33];
	char password[33];
	char key[17];
	char station[33];
	char hostname[17];
	bool metric, dimmable, nearest;
	uint32_t conditions_interval, forecasts_interval;
	uint32_t on_time, retry_interval;
	uint16_t bright, dim;
	uint8_t rotate;

	void configure(class JsonObject &o);
};

extern config cfg;

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

void display_weather(struct Conditions &c);
void display_astronomy(struct Conditions &c);
void display_forecast(struct Forecast &f);
void display_about(struct Statistics &s);

#endif
