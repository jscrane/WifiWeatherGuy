BOARD := d1_mini
TERMINAL_SPEED := 115200
CPPFLAGS := -DWWG_VERSION=\"${shell date +%F}\" -DARDUINOJSON_USE_LONG_LONG=1 -DSPI_FREQUENCY=40000000 -DUSER_SETUP_LOADED -DLOAD_GLCD -DTFT_RST=-1

EESZ := 4M2M
XTAL := 80
BAUD := 921600

d ?= default

ifeq ($d,default)
CPPFLAGS += -DILI9163_DRIVER -DTFT_WIDTH=128 -DTFT_HEIGHT=128 -DTFT_CS=PIN_D6 -DTFT_DC=PIN_D8
endif

ifeq ($d,alt)
CPPFLAGS += -DLOAD_FONT2 -DFONT=2 -DILI9341_DRIVER -DTFT_CS=PIN_D8 -DTFT_DC=PIN_D1 -DTFT_LED=D4 -DSWITCH=D3
endif

t ?= openmeteo

ifeq ($t,owm)
CPPFLAGS += -DPROVIDER=OpenWeatherMap
FS_DIR := data/owm
endif

ifeq ($t,openmeteo)
CPPFLAGS += -DPROVIDER=OpenMeteo -DICON_W=64
FS_DIR := data/openmeteo
endif

PREBUILD := data/$t/config.json
LIBRARIES := Adafruit_BusIO Wire

data/%/config.json: config.skel
	cp $^ $@

include esp8266.mk
