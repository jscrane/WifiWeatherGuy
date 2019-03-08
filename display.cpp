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
static int display_bmp(const char *filename, uint8_t x, uint8_t y) {
	int bmpWidth, bmpHeight;	// W+H in pixels
	uint8_t	bmpDepth;		// Bit depth (currently must be 24)
	uint32_t bmpImageoffset;	// Start of image data in file
	uint32_t rowSize;	// Not always = bmpWidth; may have padding
	uint8_t buf[60];
	uint8_t	buffidx = sizeof(buf);	// Current position in buffer
	boolean	flip = true;		// BMP is stored bottom-to-top
	int w, h, row, col;
	uint8_t	r, g, b;
	uint32_t pos = 0;

	if((x >= tft.width()) || (y >= tft.height())) return -1;

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
	bmpImageoffset = read32(f); // Start of image data

	DBG(print(F("Image Offset: ")));
	DBG(println(bmpImageoffset, DEC));

	// Read DIB header
	size = read32(f);
	DBG(print(F("Header size: ")));
	DBG(println(size));
	bmpWidth	= read32(f);
	bmpHeight = read32(f);
	if (read16(f) != 1) {
		ERR(println(F("# planes -- must be '1'")));
		f.close();
		return -1;
	}
	bmpDepth = read16(f); // bits per pixel
	DBG(print(F("Bit Depth: ")));
	DBG(println(bmpDepth));
	if ((bmpDepth != 24) || (read32(f) != 0)) {
		// 0 = uncompressed
		ERR(println(F("BMP format not recognized.")));
		f.close();
		return -1; 
	}
	
	DBG(print(F("Image size: ")));
	DBG(print(bmpWidth));
	DBG(print('x'));
	DBG(println(bmpHeight));
	
	// BMP rows are padded (if needed) to 4-byte boundary
	rowSize = (bmpWidth * 3 + 3) & ~3;
	
	// If bmpHeight is negative, image is in top-down order.
	// This is not canon but has been observed in the wild.
	if (bmpHeight < 0) {
		bmpHeight = -bmpHeight;
		flip			= false;
	}
	
	// Crop area to be loaded
	w = bmpWidth;
	h = bmpHeight;
	if ((x+w-1) >= tft.width())	w = tft.width()	- x;
	if ((y+h-1) >= tft.height()) h = tft.height() - y;
	
	// Set TFT address window to clipped image bounds
	tft.setAddrWindow(x, y, x+w-1, y+h-1);
	
	for (row=0; row<h; row++) { // For each scanline...

		// Seek to start of scan line.	It might seem labor-
		// intensive to be doing this on every line, but this
		// method covers a lot of gritty details like cropping
		// and scanline padding.	Also, the seek only takes
		// place if the file position actually needs to change
		// (avoids a lot of cluster math in SD library).
		if (flip) // Bitmap is stored bottom-to-top order (normal BMP)
			pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
		else		 // Bitmap is stored top-to-bottom
			pos = bmpImageoffset + row * rowSize;
			
		if (f.position() != pos) { // Need seek?
			f.seek(pos);
			buffidx = sizeof(buf); // Force buffer reload
		}
	
		for (col=0; col<w; col++) { // For each pixel...
			// Time to read more pixel data?
			if (buffidx >= sizeof(buf)) { // Indeed
				f.read(buf, sizeof(buf));
				buffidx = 0; // Set index to beginning
			}
	
			// Convert pixel from BMP to TFT format, push to display
			b = buf[buffidx++];
			g = buf[buffidx++];
			r = buf[buffidx++];

			tft.pushColor(tft.color565(r, g, b));
		} // end pixel
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

static void display_wind_speed(int wind_speed, const char *wind_dir, bool metric) {
	tft.setTextSize(2);
	tft.setCursor(1, 1);
	tft.print(wind_speed);
	tft.setTextSize(1);
	tft.print(metric? F("kph"): F("mph"));
	tft.setCursor(1, 17);
	tft.print(wind_dir);
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

	display_wind_speed(c.wind, c.wind_dir, cfg.metric? F("kph"): F("mph"));
	display_temperature(c.temp, c.feelslike, cfg.metric);
	display_humidity(c.humidity);

	tft.setTextSize(2);
	tft.setCursor(right(val_len(c.atmos_pressure), tft.width(), 2)-12, 1);
	tft.print(c.atmos_pressure);
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
	if (c.wind > 0 && strcmp_P(c.wind_dir, PSTR("Variable")))
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
	tft.print(c.sunrise_minute);
	tft.setCursor(1, 25);
	tft.print(c.sunset_hour);
	tft.print(':');
	tft.print(c.sunset_minute);

	const char *rise = "rise";
	tft.setCursor(centre_text(rise, tft.width()/2, 1), 17);
	tft.print(rise);
	const char *set = "set";
	tft.setCursor(centre_text(set, tft.width()/2, 1), 25);
	tft.print(set);

	tft.setCursor(right(3 + strlen(c.moonrise_hour), tft.width(), 1), 17);
	tft.print(c.moonrise_hour);
	tft.print(':');
	tft.print(c.moonrise_minute);
	tft.setCursor(right(3 + strlen(c.moonset_hour), tft.width(), 1), 25);
	tft.print(c.moonset_hour);
	tft.print(':');
	tft.print(c.moonset_minute);

	char buf[32];
	strcpy(buf, "moon");
	strcat(buf, c.age_of_moon);
	unsigned by = (tft.height() - ICON_H)/2, ay = by + ICON_H;
	display_bmp(buf, (tft.width() - ICON_W)/2, by);

	tft.setCursor(centre_text(c.moon_phase, tft.width()/2, 1), ay);
	tft.print(c.moon_phase);

	display_time(c.epoch, cfg.metric);
}

void display_forecast(struct Forecast &f) {
	tft.fillScreen(TFT_WHITE);
	tft.setTextColor(TFT_BLACK);

	display_wind_speed(f.ave_wind, f.wind_dir, cfg.metric);
	display_temperature(f.temp_high, f.temp_low, cfg.metric);
	display_humidity(f.ave_humidity);

	tft.setTextSize(2);
	tft.setCursor(right(3, tft.width(), 2), 1);
	tft.print(f.day);

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
