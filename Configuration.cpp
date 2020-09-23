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

	DynamicJsonDocument doc(2*JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(15) + 700);
	auto error = deserializeJson(doc, f);
	f.close();
	if (error) {
		ERR(println(error.c_str()));
		return false;
	}

	DBG(print(F("config size: ")));
	DBG(println(doc.memoryUsage()));
	JsonObject root = doc.as<JsonObject>();
	configure(doc);
	return true;
}

