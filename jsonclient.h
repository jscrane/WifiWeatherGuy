#pragma once

class JsonClient: public WiFiClient {
public:
	JsonClient(const __FlashStringHelper *host): _host(host) {}

	bool get(const char *path) {

		auto lambda = [path](Stream &s) { s.print(path); };
		return get(&lambda);
	}

	template<typename F>
	bool get(const F *func) {

		if (!connect(_host, 80)) {
			ERR(print(F("Failed to connect: ")));
			ERR(println(_host));
			return false;
		}
		print(F("GET "));

		(*func)(*this);

		println(F(" HTTP/1.1"));
		print(F("Host: "));
		println(_host);
		println(F("Connection: close"));
		println(F("Accept: application/json"));
		println();

		if (!connected()) {
			ERR(print(F("Not connected")));
			return false;
		}

		unsigned long now = millis();
		while (!available())
			if (millis() - now > 5000) {
				ERR(println(F("Timeout waiting for server!")));
				return false;
			}
		while (available()) {
			int c = peek();
			if (c == '{' || c == '[')
				return true;
			read();
		}
		ERR(println(F("Unexpected EOF reading server response!")));
		return false;
	}

private:
	const __FlashStringHelper *_host;
};

