#ifndef __PROVIDERS_H__
#define __PROVIDERS_H__

class Provider {
public:
	virtual bool fetch_conditions(struct Conditions &) = 0;
	virtual bool fetch_forecasts(struct Forecast *, int days) = 0;
};

class Wunderground: public Provider {
public:
	bool fetch_conditions(struct Conditions &);
	bool fetch_forecasts(struct Forecast *, int days);
};

class OpenWeatherMap: public Provider {
public:
	bool fetch_conditions(struct Conditions &);
	bool fetch_forecasts(struct Forecast *, int days);
};

#endif
