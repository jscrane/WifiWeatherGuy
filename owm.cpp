#include <Arduino.h>
#include <ArduinoJson.h>

#include "providers.h"

bool OpenWeatherMap::fetch_conditions(struct Conditions &) {
	// FIXME
	return false;
}

bool OpenWeatherMap::fetch_forecasts(struct Forecast *, int days) {
	// FIXME
	return false;
}
