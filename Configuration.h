#ifndef __CONFIGURATION_H__
#define __CONFIGURATION_H__

class Configuration {
public:
	bool read_file(const char *filename);

protected:	
	virtual void configure(class JsonDocument &doc) = 0;
};

class config: public Configuration {
public:
	char ssid[33];
	char password[33];
	char key[33];
	char station[33];
	char hostname[17];
	bool metric, dimmable, nearest;
	uint32_t conditions_interval, forecasts_interval;
	uint32_t on_time, retry_interval;
	uint16_t bright, dim;
	uint8_t rotate;
	float lat, lon;

	TimeChangeRule summer, winter;

	void configure(class JsonDocument &doc);
};

extern config cfg;
extern Timezone *tz;

#endif
