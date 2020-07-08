#include <stdint.h>
#include <string.h>
#include <FS.h>
#include <time.h>
#include <TFT_eSPI.h>
#include <Timezone.h>

#include "Configuration.h"
#include "display.h"
#include "dbg.h"
#include "state.h"

#define ICON_W		50
#define ICON_H		ICON_W

#if TFT_WIDTH < 200
#define SMALL	1
#define LARGE	2
#else
#define SMALL	2
#define LARGE	4
#endif

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

static int centre_text(const char *s) {
	return (tft.width() - tft.textWidth(s)) / 2;
}

static int width(char c) {
	char buf[2];
	buf[0] = c;
	buf[1] = 0;
	return tft.textWidth(buf);
}

static void display_time(time_t &local, bool metric) {
	tft.setTextSize(SMALL);
	char buf[32];
	strftime(buf, sizeof(buf), metric? "%H:%M": "%I:%M%p", localtime(&local));
	tft.setCursor(centre_text(buf), tft.height() - 2*tft.fontHeight());
	tft.print(buf);

	strftime(buf, sizeof(buf), "%a %e", localtime(&local));
	tft.setCursor(centre_text(buf), tft.height() - tft.fontHeight());
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
	tft.setTextSize(LARGE);
	int h = tft.fontHeight();
	tft.setCursor(1, 1);
	tft.print(speed);
	tft.setTextSize(SMALL);
	tft.print(metric? F("kph"): F("mph"));
	tft.setCursor(1, h+1);
	tft.print(wind_dir(deg));
}

static void display_temperature(int temp, int temp_min, bool metric) {
	tft.setTextSize(LARGE);
	int h = tft.fontHeight();
	tft.setCursor(1, tft.height() - h);
	tft.print(temp);
	tft.setTextSize(SMALL);
	tft.print(metric? 'C': 'F');
	if (temp > temp_min) {
		tft.setCursor(1, tft.height() - h - tft.fontHeight());
		tft.print(temp_min);
	}
}

static void display_humidity(int humidity) {
	tft.setTextSize(LARGE);
	int h = tft.fontHeight();
	tft.setTextSize(SMALL);
	int w = width('%');
	tft.setCursor(tft.width() - w, tft.height() - h);
	tft.print('%');
	tft.setTextSize(LARGE);
	char hum[8];
	snprintf(hum, sizeof(hum), "%d", humidity);
	tft.setCursor(tft.width() - tft.textWidth(hum) - w, tft.height() - h);
	tft.print(hum);
}

void display_weather(struct Conditions &c) {
	tft.fillScreen(TFT_WHITE);
	tft.setTextColor(TFT_BLACK);

	display_wind_speed(c.wind, c.wind_degrees, cfg.metric? F("kph"): F("mph"));
	display_temperature(c.temp, c.feelslike, cfg.metric);
	display_humidity(c.humidity);

	tft.setTextSize(SMALL);
	const char *unit = cfg.metric? "mb": "in";
	int uw = tft.textWidth(unit);
	tft.setTextSize(LARGE);
	char pres[8];
	snprintf(pres, sizeof(pres), "%d", c.pressure);
	tft.setCursor(tft.width() - tft.textWidth(pres) - uw, 1);
	tft.print(c.pressure);
	tft.setTextSize(SMALL);
	tft.print(unit);
	const char *trend = 0;
	if (c.pressure_trend == 1) {
		trend = "rising";
	} else if (c.pressure_trend == -1) {
		trend = "falling";
	}
	if (trend) {
		tft.setCursor(tft.width() - tft.textWidth(trend), tft.fontHeight()+1);
		tft.print(trend);
	}

	unsigned by = (tft.height() - ICON_H)/2, cy = by - tft.fontHeight(), wy = by + ICON_H;
	tft.setCursor(centre_text(c.city), cy);
	tft.print(c.city);
	tft.setCursor(centre_text(c.weather), wy);
	tft.print(c.weather);
	display_bmp(c.icon, (tft.width() - ICON_W)/2, by);

	display_time(c.epoch, cfg.metric);
	if (c.wind > 0)
		display_wind(c.wind_degrees, c.wind);
}

void display_astronomy(struct Conditions &c) {
	tft.fillScreen(TFT_BLACK);
	tft.setTextColor(TFT_WHITE);

	tft.setTextSize(LARGE);
	int h = tft.fontHeight();
	tft.setCursor(1, 1);
	tft.print(F("sun"));
	const char *moon = "moon";
	tft.setCursor(tft.width() - tft.textWidth(moon), 1);
	tft.print(moon);
	tft.setTextSize(SMALL);
	tft.setCursor(c.sunrise_hour < 10? 1+width(c.sunrise_hour): 1, 1+h);
	tft.print(c.sunrise_hour);
	tft.print(':');
	if (c.sunrise_minute < 10) tft.print('0');
	tft.print(c.sunrise_minute);
	tft.setCursor(1, 1+h+tft.fontHeight());
	tft.print(c.sunset_hour);
	tft.print(':');
	if (c.sunset_minute < 10) tft.print('0');
	tft.print(c.sunset_minute);

	const char *rise = "rise";
	tft.setCursor(centre_text(rise), 1+h);
	tft.print(rise);
	const char *set = "set";
	tft.setCursor(centre_text(set), 1+h+tft.fontHeight());
	tft.print(set);

	int rl = strlen(c.moonrise_hour);
	if (rl > 0) {
		char buf[16];
		snprintf(buf, sizeof(buf), "%s:%s", c.moonrise_hour, c.moonrise_minute);
		tft.setCursor(tft.width() - tft.textWidth(buf), 1+h);
		tft.print(buf);
		snprintf(buf, sizeof(buf), "%s:%s", c.moonset_hour, c.moonset_minute);
		tft.setCursor(tft.width() - tft.textWidth(buf), 1+h+tft.fontHeight());
		tft.print(buf);
	}

	char buf[32];
	snprintf(buf, sizeof(buf), "moon%d", c.age_of_moon);
	unsigned by = (tft.height() - ICON_H)/2, ay = by + ICON_H;
	display_bmp(buf, (tft.width() - ICON_W)/2, by);

	tft.setCursor(centre_text(c.moon_phase), ay);
	tft.print(c.moon_phase);

	display_time(c.epoch, cfg.metric);
}

void display_forecast(struct Forecast &f) {
	tft.fillScreen(TFT_WHITE);
	tft.setTextColor(TFT_BLACK);

	display_wind_speed(f.ave_wind, f.wind_degrees, cfg.metric);
	display_temperature(f.temp_high, f.temp_low, cfg.metric);
	display_humidity(f.humidity);

	tft.setTextSize(LARGE);
	char day[4];
	strftime(day, sizeof(day), "%a", localtime(&f.epoch));
	tft.setCursor(tft.width() - tft.textWidth(day), 1);
	tft.print(day);

	tft.setTextSize(SMALL);
	unsigned by = (tft.height() - ICON_H)/2, wy = by + ICON_H;
	display_bmp(f.icon, (tft.width() - ICON_W)/2, by);

	tft.setCursor(centre_text(f.conditions), wy);
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
#if defined(WWG_VERSION)
	tft.print(F("Version: "));
	tft.println(F(WWG_VERSION));
#endif
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
