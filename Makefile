BOARD := d1_mini
UPLOAD_SPEED := 921600
TERMINAL_SPEED := 115200
CPPFLAGS = -DVERSION=\"${shell date +%F}\" \
	-DUSER_SETUP_LOADED -DILI9163_DRIVER -DTFT_WIDTH=128 -DTFT_HEIGHT=128 \
	-DTFT_CS=PIN_D6 -DTFT_DC=PIN_D8 \
	-DTFT_RST=-1 -DSPI_FREQUENCY=40000000 -DLOAD_GLCD

FLASH_SIZE := 4M1M
BUILD_FCPU := 80000000L

wunderground: CPPFLAGS += -DPROVIDER=Wunderground
wunderground: SPIFFS_DIR := data/wunderground
owm: CPPFLAGS += -DPROVIDER=OpenWeatherMap
owm: SPIFFS_DIR := data/owm
owm wunderground: all

.PHONY: wunderground owm

include arduino-esp8266.mk
