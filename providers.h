#ifndef __PROVIDERS_H__
#define __PROVIDERS_H__

class Provider {
public:
	virtual bool fetch_conditions(struct Conditions &c) = 0;
	virtual bool fetch_forecasts(struct Forecast f[], int days) = 0;

protected:
	bool connect_and_get(WiFiClient &c, const char *host, const __FlashStringHelper *path);
	virtual void on_connect(WiFiClient &c, const __FlashStringHelper *path) = 0;
};

class Wunderground: public Provider {
public:
	bool fetch_conditions(struct Conditions &c);
	bool fetch_forecasts(struct Forecast f[], int days);

protected:
	void on_connect(WiFiClient &c, const __FlashStringHelper *path);
};

class OpenWeatherMap: public Provider {
public:
	bool fetch_conditions(struct Conditions &c);
	bool fetch_forecasts(struct Forecast f[], int days);

protected:
	void on_connect(WiFiClient &c, const __FlashStringHelper *path);
};

#endif
