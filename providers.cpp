#include <Arduino.h>
#include <ESP8266WiFi.h>

#include "providers.h"
#include "state.h"
#include "dbg.h"

bool Provider::connect_and_get(WiFiClient &client, const char *host, bool conds) {
	if (client.connect(host, 80)) {
		client.print(F("GET "));

		on_connect(client, conds);

		client.print(F(" HTTP/1.1\r\nHost: "));
		client.print(host);
		client.print(F("\r\nConnection: close\r\n\r\n"));
		if (client.connected()) {
			unsigned long now = millis();
			while (!client.available())
				if (millis() - now > 5000)
					return false;
			client.find("\r\n\r\n");
			return true;
		}
	}
	ERR(println(F("Failed to connect!")));
	stats.connect_failures++;
	return false;
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
