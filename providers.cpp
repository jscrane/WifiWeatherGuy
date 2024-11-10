#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <Timezone.h>

#include "Configuration.h"
#include "providers.h"
#include "state.h"
#include "dbg.h"
#include "jsonclient.h"

void Provider::begin() {

	if (!cfg.nearest)
		return;

	JsonClient client(F("ip-api.com"));
	if (client.get("/json")) {
		extern struct Conditions conditions;
		JsonDocument geo;

		DeserializationError error = deserializeJson(geo, client);
		if (error) {
			ERR(print(F("Deserializing ip-api.com response: ")));
			ERR(println(error.f_str()));
		} else {
			cfg.lat = geo["lat"];
			cfg.lon = geo["lon"];
			strncpy(conditions.city, geo["city"], sizeof(conditions.city));
			cfg.nearest = true;
		}
	}
}

bool Provider::fetch_conditions(struct Conditions &conditions) {

	JsonClient client(_host);
	bool ret = false;
	auto lambda = [&](Stream &s) { on_connect(s, true); };

	if (client.get(&lambda)) {
		JsonDocument doc;
		DeserializationError error = deserializeJson(doc, client);
		if (error) {
			ERR(print(F("Deserialization of Conditions failed: ")));
			ERR(println(error.f_str()));
			stats.parse_failures++;
		} else {
			ret = update_conditions(doc, conditions);
			if (ret)
				stats.num_updates++;
			DBG(print(F("Done ")));
		}
	}
	client.stop();
	return ret;
}

bool Provider::fetch_forecasts(struct Forecast forecasts[], int days) {

	JsonClient client(_host);
	bool ret = false;
	auto lambda = [&](Stream &s) { on_connect(s, false); };

	if (client.get(&lambda)) {
		JsonDocument doc;
		DeserializationError error = deserializeJson(doc, client);
		if (error) {
			ERR(print(F("Deserialization of Forecasts failed: ")));
			ERR(println(error.f_str()));
			stats.parse_failures++;
		} else {
			ret = update_forecasts(doc, forecasts, days);
			DBG(print(F("Done ")));
		}
	}
	client.stop();
	return ret;
}

// https://en.wikipedia.org/wiki/Lunar_phase#Calculating_phase
int Provider::moon_age(time_t &epoch) {
	const long last_full = 937008000;	// Aug 11, 1999
	const double secs_per_day = 86400;
	const double lunation_period = 29.530588853;

	return round(fmod(((long)epoch - last_full) / secs_per_day, lunation_period));
}

const char *Provider::moon_phase(int age) {
	if (age == 0)
		return PSTR("New Moon");
	if (age < 7)
		return PSTR("Waxing Crescent");
	if (age == 7)
		return PSTR("First Quarter");
	if (age < 15)
		return PSTR("Waxing Gibbous");
	if (age == 15)
		return PSTR("Full Moon");
	if (age < 22)
		return PSTR("Waning Gibbous");
	if (age == 22)
		return PSTR("Last Quarter");
	if (age < 29)
		return PSTR("Waning Crescent");
	return PSTR("New Moon");
}

const char *Provider::weather_description(int wmo_code) {

	switch(wmo_code) {
	case 0:
		return PSTR("clear sky");
	case 1:
		return PSTR("mostly clear");
	case 2:
		return PSTR("partly cloudy");
	case 3:
		return PSTR("overcast");
	case 45:
		return PSTR("fog");
	case 48:
		return PSTR("icy fog");
	case 51:
		return PSTR("light drizzle");
	case 53:
		return PSTR("drizzle");
	case 55:
		return PSTR("heavy drizzle");
	case 56:
		return PSTR("light icy drizzle");
	case 57:
		return PSTR("icy drizzle");
	case 61:
		return PSTR("light rain");
	case 63:
		return PSTR("rain");
	case 65:
		return PSTR("heavy rain");
	case 66:
		return PSTR("light icy rain");
	case 67:
		return PSTR("icy rain");
	case 71:
		return PSTR("light snow");
	case 73:
		return PSTR("snow");
	case 75:
		return PSTR("heavy snow");
	case 77:
		return PSTR("snow grains");
	case 80:
		return PSTR("light showers");
	case 81:
		return PSTR("showers");
	case 82:
		return PSTR("heavy showers");
	case 85:
		return PSTR("light snow showers");
	case 86:
		return PSTR("snow showers");
	case 95:
		return PSTR("thunderstorm");
	case 96:
		return PSTR("thunderstorm with light hail");
	case 99:
		return PSTR("thunderstorm with hail");
	default:
		return PSTR("unknown");
	}
}
