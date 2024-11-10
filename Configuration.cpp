#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Timezone.h>
#include "Configuration.h"
#include "dbg.h"

bool Configuration::read_file(const char *filename) {
	File f = LittleFS.open(filename, "r");
	if (!f) {
		ERR(print(F("failed to open: ")));
		ERR(println(filename));
		return false;
	}

	JsonDocument doc;
	auto error = deserializeJson(doc, f);
	f.close();
	if (error) {
		ERR(println(error.c_str()));
		return false;
	}

	JsonObject root = doc.as<JsonObject>();
	configure(doc);
	return true;
}

void config::configure(JsonDocument &o) {

	strlcpy(ssid, o[F("ssid")] | "", sizeof(ssid));
	strlcpy(password, o[F("password")] | "", sizeof(password));
	strlcpy(key, o[F("key")] | "", sizeof(key));
	strlcpy(station, o[F("station")] | "", sizeof(station));
	strlcpy(hostname, o[F("hostname")] | "", sizeof(hostname));
	conditions_interval = 1000 * (int)o[F("conditions_interval")];
	forecasts_interval = 1000 * (int)o[F("forecasts_interval")];
	metric = o[F("metric")];
	dimmable = o[F("dimmable")];
	nearest = o[F("nearest")];
	on_time = 1000 * (int)o[F("display")];
	bright = o[F("bright")];
	dim = o[F("dim")];
	rotate = o[F("rotate")];

	lat = lon = 0.0;

	const JsonObject &s = o[F("summer")];
	summer.week = (int)s[F("week")] | 0;
	summer.dow = (int)s[F("dow")] | 1;
	summer.month = (int)s[F("month")] | 1;
	summer.hour = (int)s[F("hour")] | 0;
	summer.offset = (int)s[F("offset")] | 0;

	const JsonObject &w = o[F("winter")];
	winter.week = (int)w[F("week")] | 0;
	winter.dow = (int)w[F("dow")] | 1;
	winter.month = (int)w[F("month")] | 1;
	winter.hour = (int)w[F("hour")] | 0;
	winter.offset = (int)w[F("offset")] | 0;
}
