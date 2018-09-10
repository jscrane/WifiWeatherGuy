#include <stdint.h>
#include <string.h>
#include <FS.h>
#include <time.h>
#include <TFT_eSPI.h>
#include "display.h"
#include "dbg.h"

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
int display_bmp(const char *filename, uint8_t x, uint8_t y) {
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

int centre_text(const char *s, int x, int size) {
	return x - (strlen(s) * size * 6) / 2;
}

int right(int n, int x, int size) {
	return x - n * size * 6;
}

int val_len(int b) {
	if (b >= 1000) return 4;
	if (b >= 100) return 3;
	if (b >= 10) return 2;
	if (b >= 0) return 1;
	if (b > -10) return 2;
	return 3;
}

void display_time(time_t &epoch, bool metric) {
	char buf[32];
	strftime(buf, sizeof(buf), metric? "%H:%M": "%I:%M%p", localtime(&epoch));
	tft.setCursor(centre_text(buf, tft.width()/2, 1), 109);
	tft.print(buf);
	strftime(buf, sizeof(buf), "%a %d", localtime(&epoch));
	tft.setCursor(centre_text(buf, tft.width()/2, 1), 118);
	tft.print(buf);
}

void display_wind(int wind_degrees, int wind_speed) {
	// http://www.iquilezles.org/www/articles/sincos/sincos.htm
	int rad = tft.width()/3, cx = tft.width()/2, cy = 68;
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
	tft.drawLine(ex, ey, ex+wind_speed*(cx-ex)/50, ey+wind_speed*(cy-ey)/50, TFT_BLACK);
}

void display_wind_speed(int wind_speed, const char *wind_dir, bool metric) {
	tft.setTextSize(2);
	tft.setCursor(1, 1);
	tft.print(wind_speed);
	tft.setTextSize(1);
	tft.print(metric? F("kph"): F("mph"));
	tft.setCursor(1, 17);
	tft.print(wind_dir);
}

void display_temperature(int temp, int temp_min, bool metric) {
	tft.setTextSize(2);
	tft.setCursor(1, tft.height() - 16);
	tft.print(temp);
	tft.setTextSize(1);
	tft.print(metric? 'C': 'F');
	if (temp != temp_min) {
		tft.setCursor(1, tft.height() - 24);
		tft.print(temp_min);
	}
}

void display_humidity(int humidity) {
	tft.setTextSize(2);
	tft.setCursor(right(val_len(humidity), tft.width(), 2) - 6, tft.height() - 16);
	tft.print(humidity);
	tft.setTextSize(1);
	tft.setCursor(tft.width() - 6, tft.height() - 16);
	tft.print('%');
}

