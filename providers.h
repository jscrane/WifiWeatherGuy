#ifndef __PROVIDERS_H__
#define __PROVIDERS_H__

class Provider {
public:
	bool fetch_conditions(struct Conditions &c);
	bool fetch_forecasts(struct Forecast f[], int days);

protected:
	Provider(const __FlashStringHelper *host): _host(host) {}

	virtual void on_connect(Stream &c, bool conds) = 0;
	virtual bool update_conditions(class JsonDocument &doc, struct Conditions &c) = 0;
	virtual bool update_forecasts(class JsonDocument &doc, struct Forecast f[], int days) = 0;

	// utils
	int moon_age(time_t &epoch);
	const char *moon_phase(int age);
	const char *weather_description(int wmo_code);

private:
	bool connect_and_get(WiFiClient &c, bool conds);

	const __FlashStringHelper *_host;
};

class OpenWeatherMap: public Provider {
public:
	OpenWeatherMap();

protected:
	void on_connect(Stream &c, bool conds);
	bool update_conditions(class JsonDocument &doc, struct Conditions &c);
	bool update_forecasts(class JsonDocument &doc, struct Forecast f[], int days);
};

class OpenMeteo: public Provider {
public:
	OpenMeteo();

protected:
	void on_connect(Stream &c, bool conds);
	bool update_conditions(class JsonDocument &doc, struct Conditions &c);
	bool update_forecasts(class JsonDocument &doc, struct Forecast f[], int days);
};

#endif
