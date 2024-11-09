#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <StreamUtils.h>

#include "providers.h"
#include "state.h"
#include "dbg.h"

bool Provider::connect_and_get(WiFiClient &client, bool conds) {
	if (client.connect(_host, 80)) {

		client.print(F("GET "));

		on_connect(client, conds);

		client.print(F(" HTTP/1.1\r\nHost: "));
		client.print(_host);
		client.print(F("\r\nConnection: close\r\nAccept: application/json\r\n\r\n"));
		if (client.connected()) {
			unsigned long now = millis();
			while (!client.available())
				if (millis() - now > 5000) {
					ERR(println(F("Timeout waiting for server!")));
					return false;
				}
			while (client.available()) {
				int c = client.peek();
				if (c == '{' || c == '[')
					return true;;
				client.read();
			}
			ERR(println(F("Unexpected EOF reading server response!")));
			return false;
		}
	}
	ERR(println(F("Failed to connect!")));
	stats.connect_failures++;
	return false;
}

bool Provider::fetch_conditions(struct Conditions &conditions) {
	WiFiClient client;
	bool ret = false;

	if (connect_and_get(client, true)) {
		JsonDocument doc;
		ReadLoggingStream logger(client, Serial);
		//DeserializationError error = deserializeJson(doc, client);
		DeserializationError error = deserializeJson(doc, logger);
		if (error) {
			ERR(print(F("Deserialization of Conditions failed: ")));
			ERR(println(error.f_str()));
			stats.parse_failures++;
		} else {
			ret = update_conditions(doc, conditions);
			DBG(print(F("Done ")));
		}
	}
	client.stop();
	return ret;
}

bool Provider::fetch_forecasts(struct Forecast forecasts[], int days) {

	WiFiClient client;
	bool ret = false;

	if (connect_and_get(client, false)) {
		JsonDocument doc;
		ReadLoggingStream logger(client, Serial);
		//DeserializationError error = deserializeJson(doc, client);
		DeserializationError error = deserializeJson(doc, logger);
		if (error) {
			ERR(print(F("Deserialization of Forecasts failed: ")));
			ERR(println(error.f_str()));
			stats.parse_failures++;
		} else {
			update_forecasts(doc, forecasts, days);
			DBG(print(F("Done ")));
			ret = true;
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
		return PSTR("mainly clear");
	case 2:
		return PSTR("partly cloudy");
	case 3:
		return PSTR("overcast");
	case 45:
		return PSTR("fog");
	case 48:
		return PSTR("freezing fog");
	case 51:
		return PSTR("light drizzle");
	case 53:
		return PSTR("drizzle");
	case 55:
		return PSTR("dense drizzle");
	case 56:
		return PSTR("light freezing drizzle");
	case 57:
		return PSTR("freezing drizzle");
	case 61:
		return PSTR("light rain");
	case 63:
		return PSTR("rain");
	case 65:
		return PSTR("heavy rain");
	case 66:
		return PSTR("light freezing rain");
	case 67:
		return PSTR("freezing rain");
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
		return PSTR("rain showers");
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
