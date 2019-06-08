#include <FS.h>
#include <ArduinoJson.h>
#include "Configuration.h"
#include "dbg.h"

bool Configuration::read_file(const char *filename) {
	File f = SPIFFS.open(filename, "r");
	if (!f) {
		ERR(print(F("failed to open: ")));
		ERR(println(filename));
		return false;
	}

	DynamicJsonDocument doc(JSON_OBJECT_SIZE(15) + 300);
	auto error = deserializeJson(doc, f);
	f.close();
	if (error) {
		ERR(println(error.c_str()));
		return false;
	}

	DBG(print(F("config size: ")));
	DBG(println(doc.memoryUsage()));
	JsonObject root = doc.as<JsonObject>();
	configure(root);
	return true;
}

