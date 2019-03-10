#include <Arduino.h>
#include <ESP8266WiFi.h>

#include "providers.h"
#include "state.h"
#include "dbg.h"

bool Provider::connect_and_get(WiFiClient &client, const char *host,  const __FlashStringHelper *path) {
	if (!client.connect(host, 80)) {
		client.print(F("GET "));

		on_connect(client, path);

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
