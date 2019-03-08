#ifndef __WUNDERGROUND_H__
#define __WUNDERGROUND_H__

class Wunderground {
public:
	bool fetch_conditions(struct Conditions &);
	bool fetch_forecasts(struct Forecast *, int days);
};

#endif
