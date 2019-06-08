#ifndef __PROVIDERS_H__
#define __PROVIDERS_H__

class Provider {
public:
	virtual bool fetch_conditions(struct Conditions &c) = 0;
	virtual bool fetch_forecasts(struct Forecast f[], int days) = 0;

	int moon_age(time_t &epoch);
	const char *moon_phase(int age);
protected:
	bool connect_and_get(WiFiClient &c, const char *host, bool conds);
	virtual void on_connect(WiFiClient &c, bool conds) = 0;
};

class Wunderground: public Provider {
public:
	bool fetch_conditions(struct Conditions &c);
	bool fetch_forecasts(struct Forecast f[], int days);

protected:
	void on_connect(WiFiClient &c, bool conds);
};

class OpenWeatherMap: public Provider {
public:
	bool fetch_conditions(struct Conditions &c);
	bool fetch_forecasts(struct Forecast f[], int days);

protected:
	void on_connect(WiFiClient &c, bool conds);

private:
	bool update_conditions(JsonObject &root, struct Conditions &c);
};

#endif
