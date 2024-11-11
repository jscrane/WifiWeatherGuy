#pragma once

class JsonClient {
public:
	JsonClient(WiFiClient &client, const __FlashStringHelper *host):
		JsonClient(client, host, 80) {}

	JsonClient(WiFiClient &client, const __FlashStringHelper *host, unsigned port):
		_client(client), _host(host), _port(port) {}

	bool get(const char *path) {

		return get([path](Stream &s) { s.print(path); });
	}

	bool get(std::function<void(Stream &)> add_path) {

		if (!_client.connect(_host, _port)) {
			ERR(print(F("Failed to connect: ")));
			ERR(print(_host));
			ERR(print(':'));
			ERR(print(_port));
			return false;
		}
		_client.print(F("GET "));

		add_path(_client);

		_client.println(F(" HTTP/1.1"));
		_client.print(F("Host: "));
		_client.println(_host);
		_client.println(F("Connection: close"));
		_client.println(F("Accept: application/json"));
		_client.println();

		if (!_client.connected()) {
			ERR(print(F("Not connected")));
			return false;
		}

		unsigned long now = millis();
		while (!_client.available())
			if (millis() - now > 5000) {
				ERR(println(F("Timeout waiting for server!")));
				return false;
			}

		while (_client.available()) {
			int c = _client.peek();
			if (c == '{' || c == '[')
				return true;
			_client.read();
		}

		ERR(println(F("Unexpected EOF reading server response!")));
		return false;
	}

private:
	WiFiClient &_client;
	const __FlashStringHelper *_host;
	const unsigned _port;
};

