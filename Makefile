BOARD := d1_mini
UPLOAD_SPEED := 921600
TERMINAL_SPEED := 115200
CPPFLAGS := -DWWG_VERSION=\"${shell date +%F}\" -DSPI_FREQUENCY=40000000 -DUSER_SETUP_LOADED -DLOAD_GLCD -DTFT_RST=-1

FLASH_SIZE := 4M1M
BUILD_FCPU := 80000000L

d ?= default

ifeq ($d,default)
CPPFLAGS += -DILI9163_DRIVER -DTFT_WIDTH=128 -DTFT_HEIGHT=128 -DTFT_CS=PIN_D6 -DTFT_DC=PIN_D8
endif

ifeq ($d,alt)
CPPFLAGS += -DILI9341_DRIVER -DTFT_CS=PIN_D8 -DTFT_DC=PIN_D1
endif

t ?= owm

ifeq ($t,wunderground)
CPPFLAGS += -DPROVIDER=Wunderground
SPIFFS_DIR := data/wunderground
endif

ifeq ($t,owm)
CPPFLAGS += $(XCPPFLAGS) -DPROVIDER=OpenWeatherMap
SPIFFS_DIR := data/owm
endif

include esp8266.mk
