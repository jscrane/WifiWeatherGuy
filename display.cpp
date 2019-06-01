#include <stdint.h>
#include <string.h>
#include <FS.h>
#include <time.h>
#include <TFT_eSPI.h>
#include "Configuration.h"
#include "display.h"
#include "dbg.h"
#include "state.h"

#define ICON_W		50
#define ICON_H		ICON_W

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.
static uint16_t read16(File &f) {
	uint16_t result;
	size_t n = f.read((uint8_t *)&result, sizeof(result));
	return result;
}

static uint32_t read32(File &f) {
	uint32_t result;
	size_t n = f.read((uint8_t *)&result, sizeof(result));
	return result;
}

// from Adafruit's spitftbitmap ST7735 example
// updated with Bodmer's example in BMP_functions.cpp
static int display_bmp(const char *filename, uint16_t x, uint16_t y) {

	if ((x >= tft.width()) || (y >= tft.height())) return -1;

	uint32_t startTime = millis();

	char fbuf[32];
	strcpy(fbuf, "/");
	strcat(fbuf, filename);
	strcat(fbuf, ".bmp");
	File f = SPIFFS.open(fbuf, "r");
	if (!f) {
		ERR(print(F("file.open!")));
		ERR(print(' '));
		ERR(println(filename));
		return -1;
	}

	// Parse BMP header
	if (read16(f) != 0x4D42) {
		ERR(println(F("Unknown BMP signature")));
		f.close();
		return -1;
	}

	uint32_t size = read32(f);
	DBG(print(F("File size: ")));
	DBG(println(size));

	(void)read32(f); // Read & ignore creator bytes
	uint32_t bmpImageoffset = read32(f); // Start of image data

	DBG(print(F("Image Offset: ")));
	DBG(println(bmpImageoffset, DEC));

	// Read DIB header
	size = read32(f);
	DBG(print(F("Header size: ")));
	DBG(println(size));
	uint16_t w = read32(f);
	uint16_t h = read32(f);
	if (read16(f) != 1) {
		ERR(println(F("# planes -- must be '1'")));
		f.close();
		return -1;
	}
	uint16_t bmpDepth = read16(f); // bits per pixel
	DBG(print(F("Bit Depth: ")));
	DBG(println(bmpDepth));
	if ((bmpDepth != 24) || (read32(f) != 0)) {
		// 0 = uncompressed
		ERR(println(F("BMP format not recognized.")));
		f.close();
		return -1; 
	}
	
	DBG(print(F("Image size: ")));
	DBG(print(w));
	DBG(print('x'));
	DBG(println(h));
	
	uint32_t rowSize = (w * 3 + 3) & ~3;
	tft.setSwapBytes(true);
	y += h-1;
	
	uint16_t padding = (4 - ((w * 3) & 3)) & 3;
	uint8_t lineBuffer[w * 3 + padding];
	uint32_t pos = bmpImageoffset;

	for (uint16_t row = 0; row < h; row++) {

		if (f.position() != pos)
			f.seek(pos);

		f.read(lineBuffer, sizeof(lineBuffer));
		uint8_t *bptr = lineBuffer;
		uint16_t *tptr = (uint16_t *)lineBuffer;
		for (uint16_t col = 0; col < w; col++) {
			uint8_t b = *bptr++;
			uint8_t g = *bptr++;
			uint8_t r = *bptr++;
			*tptr++ = tft.color565(r, g, b);
		}
		tft.pushImage(x, y--, w, 1, (uint16_t*)lineBuffer);
		pos += rowSize;
	}
	f.close();
	DBG(print(F("Loaded in ")));
	DBG(print(millis() - startTime));
	DBG(println(F(" ms")));
	return 0;
}

static int centre_text(const char *s, int x, int size) {
	return x - (strlen(s) * size * 6) / 2;
}

static int right(int n, int x, int size) {
	return x - n * size * 6;
}

static int val_len(int b) {
	if (b >= 1000) return 4;
	if (b >= 100) return 3;
	if (b >= 10) return 2;
	if (b >= 0) return 1;
	if (b > -10) return 2;
	return 3;
}

static void display_time(time_t &epoch, bool metric) {
	char buf[32];
	strftime(buf, sizeof(buf), metric? "%H:%M": "%I:%M%p", localtime(&epoch));
	tft.setCursor(centre_text(buf, tft.width()/2, 1), tft.height() - 20);
	tft.print(buf);
	strftime(buf, sizeof(buf), "%a %d", localtime(&epoch));
	tft.setCursor(centre_text(buf, tft.width()/2, 1), tft.height() - 10);
	tft.print(buf);
}

static void display_wind(int wind_degrees, int wind_speed) {
	// http://www.iquilezles.org/www/articles/sincos/sincos.htm
	int w = tft.width(), h = tft.height();
	int rad = w > h? h/3: w/3;
	int cx = w/2, cy = h/2;
	const float a = 0.999847695, b = 0.017452406;
	// wind dir is azimuthal angle with N at 0
	float sin = 1.0, cos = 0.0;
	for (uint16_t i = 0; i < wind_degrees; i++) {
		const float ns = a*sin + b*cos;
		const float nc = a*cos - b*sin;
		cos = nc;
		sin = ns;
	}
	// wind dir rotates clockwise so compensate
	int ex = cx-rad*cos, ey = cy-rad*sin;
	tft.fillCircle(ex, ey, 3, TFT_BLACK);
	tft.drawLine(ex, ey, ex+wind_speed*(cx-ex)/ICON_W, ey+wind_speed*(cy-ey)/ICON_H, TFT_BLACK);
}

static const __FlashStringHelper *wind_dir(int d) {
	if (d < 12) return F("N");
	if (d < 34) return F("NNE");
	if (d < 57) return F("NE");
	if (d < 79) return F("ENE");
	if (d < 102) return F("E");
	if (d < 124) return F("ESE");
	if (d < 147) return F("SE");
	if (d < 169) return F("SSE");
	if (d < 192) return F("S");
	if (d < 214) return F("SSW");
	if (d < 237) return F("SW");
	if (d < 259) return F("WSW");
	if (d < 282) return F("W");
	if (d < 304) return F("WNW");
	if (d < 327) return F("NW");
	if (d < 349) return F("NNW");
	return F("N");
}

static void display_wind_speed(int speed, int deg, bool metric) {
	tft.setTextSize(2);
	tft.setCursor(1, 1);
	tft.print(speed);
	tft.setTextSize(1);
	tft.print(metric? F("kph"): F("mph"));
	tft.setCursor(1, 17);
	tft.print(wind_dir(deg));
}

static void display_temperature(int temp, int temp_min, bool metric) {
	tft.setTextSize(2);
	tft.setCursor(1, tft.height() - 16);
	tft.print(temp);
	tft.setTextSize(1);
	tft.print(metric? 'C': 'F');
	if (temp > temp_min) {
		tft.setCursor(1, tft.height() - 24);
		tft.print(temp_min);
	}
}

static void display_humidity(int humidity) {
	tft.setTextSize(2);
	tft.setCursor(right(val_len(humidity), tft.width(), 2) - 6, tft.height() - 16);
	tft.print(humidity);
	tft.setTextSize(1);
	tft.setCursor(tft.width() - 6, tft.height() - 16);
	tft.print('%');
}

void display_weather(struct Conditions &c) {
	tft.fillScreen(TFT_WHITE);
	tft.setTextColor(TFT_BLACK);

	display_wind_speed(c.wind, c.wind_degrees, cfg.metric? F("kph"): F("mph"));
	display_temperature(c.temp, c.feelslike, cfg.metric);
	display_humidity(c.humidity);

	tft.setTextSize(2);
	tft.setCursor(right(val_len(c.pressure), tft.width(), 2)-12, 1);
	tft.print(c.pressure);
	tft.setTextSize(1);
	tft.print(cfg.metric? F("mb"): F("in"));
	if (c.pressure_trend == 1) {
		tft.setCursor(right(6, tft.width(), 1), 17);
		tft.print(F("rising"));
	} else if (c.pressure_trend == -1) {
		tft.setCursor(right(7, tft.width(), 1), 17);
		tft.print(F("falling"));
	}

	unsigned by = (tft.height() - ICON_H)/2, cy = by - 10, wy = by + ICON_H;
	tft.setCursor(centre_text(c.city, tft.width()/2, 1), cy);
	tft.print(c.city);
	tft.setCursor(centre_text(c.weather, tft.width()/2, 1), wy);
	tft.print(c.weather);
	display_bmp(c.icon, (tft.width() - ICON_W)/2, by);

	display_time(c.epoch, cfg.metric);
	if (c.wind > 0)
		display_wind(c.wind_degrees, c.wind);
}

void display_astronomy(struct Conditions &c) {
	tft.fillScreen(TFT_BLACK);
	tft.setTextColor(TFT_WHITE);

	tft.setTextSize(2);
	tft.setCursor(1, 1);
	tft.print(F("sun"));
	tft.setCursor(right(4, tft.width(), 2), 1);
	tft.print(F("moon"));
	tft.setTextSize(1);
	tft.setCursor(c.sunrise_hour < 10? 7: 1, 17);
	tft.print(c.sunrise_hour);
	tft.print(':');
	if (c.sunrise_minute < 10) tft.print('0');
	tft.print(c.sunrise_minute);
	tft.setCursor(1, 25);
	tft.print(c.sunset_hour);
	tft.print(':');
	if (c.sunset_minute < 10) tft.print('0');
	tft.print(c.sunset_minute);

	const char *rise = "rise";
	tft.setCursor(centre_text(rise, tft.width()/2, 1), 17);
	tft.print(rise);
	const char *set = "set";
	tft.setCursor(centre_text(set, tft.width()/2, 1), 25);
	tft.print(set);

	int rl = strlen(c.moonrise_hour);
	if (rl > 0) {
		tft.setCursor(right(3 + rl, tft.width(), 1), 17);
		tft.print(c.moonrise_hour);
		tft.print(':');
		tft.print(c.moonrise_minute);
		tft.setCursor(right(3 + strlen(c.moonset_hour), tft.width(), 1), 25);
		tft.print(c.moonset_hour);
		tft.print(':');
		tft.print(c.moonset_minute);
	}

	char buf[32];
	snprintf(buf, sizeof(buf), "moon%d", c.age_of_moon);
	unsigned by = (tft.height() - ICON_H)/2, ay = by + ICON_H;
	display_bmp(buf, (tft.width() - ICON_W)/2, by);

	tft.setCursor(centre_text(c.moon_phase, tft.width()/2, 1), ay);
	tft.print(c.moon_phase);

	display_time(c.epoch, cfg.metric);
}

void display_forecast(struct Forecast &f) {
	tft.fillScreen(TFT_WHITE);
	tft.setTextColor(TFT_BLACK);

	display_wind_speed(f.ave_wind, f.wind_degrees, cfg.metric);
	display_temperature(f.temp_high, f.temp_low, cfg.metric);
	display_humidity(f.humidity);

	tft.setTextSize(2);
	tft.setCursor(right(3, tft.width(), 2), 1);
	char day[4];
	strftime(day, sizeof(day), "%a", localtime(&f.epoch));
	tft.print(day);

	tft.setTextSize(1);
	unsigned by = (tft.height() - ICON_H)/2, wy = by + ICON_H;
	display_bmp(f.icon, (tft.width() - ICON_W)/2, by);

	tft.setCursor(centre_text(f.conditions, tft.width()/2, 1), wy);
	tft.print(f.conditions);

	display_time(f.epoch, cfg.metric);
	display_wind(f.wind_degrees, f.ave_wind);
}

static char *hms(uint32_t t) {
	static char buf[32];
	unsigned s = t % 60;
	t /= 60;
	unsigned m = t % 60;
	t /= 60;
	unsigned h = t;
	if (h >= 24)
		snprintf(buf, sizeof(buf), "%dd %d:%02d:%02d", h/24, h % 24, m, s);
	else
		snprintf(buf, sizeof(buf), "%d:%02d:%02d", h, m, s);
	return buf;
}

void display_about(struct Statistics &s) {
	tft.fillScreen(TFT_BLACK);
	tft.setTextColor(TFT_WHITE);
	tft.setCursor(1, 1);

	uint32_t now = millis();
	tft.print(F("Version: "));
	tft.println(F(VERSION));
	tft.print(F("Uptime: "));
	tft.println(hms(now / 1000));
	tft.println();
	tft.print(F("Updates: "));
	tft.println(s.num_updates);
	tft.print(F("Conditions: "));
	tft.println(hms((now - s.last_fetch_conditions) / 1000));
	tft.print(F("Forecasts:  "));
	tft.println(hms((now - s.last_fetch_forecasts) / 1000));
	tft.print(F("Age: "));
	tft.println(s.last_age? hms(s.last_age): "");
	tft.print(F("Min: "));
	tft.println(s.min_age? hms(s.min_age): "");
	tft.print(F("Max: "));
	tft.println(s.max_age? hms(s.max_age): "");
	tft.print(F("Ave: "));
	tft.println(s.num_updates > 1? hms(s.total / (s.num_updates-1)): "");
	tft.println();
	tft.println(F("Failures"));
	tft.print("Connect: ");
	tft.println(s.connect_failures);
	tft.print("Parse:   ");
	tft.println(s.parse_failures);
	tft.print("Memory:  ");
	tft.println(s.mem_failures);
}
