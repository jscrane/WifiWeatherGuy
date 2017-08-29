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
int bmp_draw(const char *filename, uint8_t x, uint8_t y) {
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t buf[60];
  uint8_t  buffidx = sizeof(buf);      // Current position in buffer
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0;

  if((x >= tft.width()) || (y >= tft.height())) return -1;

#ifdef DEBUG
  uint32_t startTime = millis();
#endif

  char fbuf[32];
  strcpy(fbuf, "/");
  strcat(fbuf, filename);
  strcat(fbuf, ".bmp");
  File f = SPIFFS.open(fbuf, "r");
  if (!f) {
    out.print(F("file.open!"));
    out.print(' ');
    out.println(filename);
    return -1;
  }

  // Parse BMP header
  if (read16(f) != 0x4D42) {
    out.println(F("Unknown BMP signature"));
    f.close();
    return -1;
  }
#ifdef DEBUG  
  out.print(F("File size: "));
  out.println(read32(f));
#else
  (void)read32(f);
#endif  
  (void)read32(f); // Read & ignore creator bytes
  bmpImageoffset = read32(f); // Start of image data
#ifdef DEBUG
  out.print(F("Image Offset: ")); 
  out.println(bmpImageoffset, DEC);
#endif
  // Read DIB header
#ifdef DEBUG
  uint32_t header_size = read32(f);
  out.print(F("Header size: ")); 
  out.println(header_size);
#else
  (void)read32(f);
#endif
  bmpWidth  = read32(f);
  bmpHeight = read32(f);
  if (read16(f) != 1) {
    out.println(F("# planes -- must be '1'"));
    f.close();
    return -1;
  }
  bmpDepth = read16(f); // bits per pixel
#ifdef DEBUG
  out.print(F("Bit Depth: ")); 
  out.println(bmpDepth);
#endif
  if((bmpDepth != 24) || (read32(f) != 0)) {
    // 0 = uncompressed
    out.println(F("BMP format not recognized."));
    f.close();
    return -1; 
  }
  
#ifdef DEBUG
  out.print(F("Image size: "));
  out.print(bmpWidth);
  out.print('x');
  out.println(bmpHeight);
#endif
  
  // BMP rows are padded (if needed) to 4-byte boundary
  rowSize = (bmpWidth * 3 + 3) & ~3;
  
  // If bmpHeight is negative, image is in top-down order.
  // This is not canon but has been observed in the wild.
  if (bmpHeight < 0) {
    bmpHeight = -bmpHeight;
    flip      = false;
  }
  
  // Crop area to be loaded
  w = bmpWidth;
  h = bmpHeight;
  if ((x+w-1) >= tft.width())  w = tft.width()  - x;
  if ((y+h-1) >= tft.height()) h = tft.height() - y;
  
  // Set TFT address window to clipped image bounds
  tft.setAddrWindow(x, y, x+w-1, y+h-1);
  
  for (row=0; row<h; row++) { // For each scanline...

    // Seek to start of scan line.  It might seem labor-
    // intensive to be doing this on every line, but this
    // method covers a lot of gritty details like cropping
    // and scanline padding.  Also, the seek only takes
    // place if the file position actually needs to change
    // (avoids a lot of cluster math in SD library).
    if (flip) // Bitmap is stored bottom-to-top order (normal BMP)
      pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
    else     // Bitmap is stored top-to-bottom
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

      tft.pushColor(tft.Color565(r,g,b));
    } // end pixel
  }
  f.close();
#ifdef DEBUG
  out.print(F("Loaded in "));
  out.print(millis() - startTime);
  out.println(F(" ms"));
#endif
  return 0;
}

