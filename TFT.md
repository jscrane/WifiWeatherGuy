TFT boards
==========

David Prentice's 
[st7735_readreg](https://github.com/prenticedavid/TFT_SPI_kbv) 
sketch is useful to identify chipsets in various cheap 1.44" TFT displays.

| SPI | TFT | Pin |
| --- | --- | --- |
| MOSI | SDA | 11 |
| SCK | SCK | 13 |
| SS  | CS  | 10 |
| DC  | AO  | 9 |

KMR1441_SPI V2
--------------
The backlight LED doesn't seem to be variable with PWM on this board.
Set CPPFLAGS to this in the Makefile for this display:

```
CPPFLAGS = -DVERSION=\"${shell date +%F}\" -DST7735_GREENTAB \
	-DUSER_SETUP_LOADED -DST7735_DRIVER -DTFT_WIDTH=128 -DTFT_HEIGHT=128 \
	-DTFT_CS=PIN_D6 -DTFT_DC=PIN_D8 \
	-DTFT_RST=-1 -DSPI_FREQUENCY=27000000 -DLOAD_GLCD
```

```
Bi-directional Read registers

controller reg(0x01) = 0x00
ST7735S reg(0x04) = 0x7C89F0
ST7735S reg(0x09) = 0x610000
ST7735S reg(0x0A) = 0x08
ST7735S reg(0x0B) = 0x00
ST7735S reg(0x0C) = 0x06
ST7735S reg(0x0D) = 0x00
ST7735S reg(0x0E) = 0x00
ST7735S reg(0x0F) = 0x00
ST7735S reg(0x2E) = 0x102008
ST7735S reg(0xDA) = 0x7C
ST7735S reg(0xDB) = 0x89
ST7735S reg(0xDC) = 0xF0
```
