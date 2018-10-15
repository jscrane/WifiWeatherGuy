BOARD := d1_mini
UPLOAD_SPEED := 921600
TERM_SPEED := 115200
CPPFLAGS = -DVERSION=\"${shell date +%F}\"

FLASH_SIZE := 4M1M
BUILD_FCPU := 80000000L

include arduino-esp.mk
