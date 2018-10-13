extern TFT_eSPI tft;

#define CITY_Y		26
#define ICON_Y		34
#define WIND_CY		60
#define WEATHER_Y	85

int display_bmp(const char *filename, uint8_t x, uint8_t y);
void display_humidity(int humidity);
void display_temperature(int temp, int temp_min, bool metric);
void display_wind_speed(int wind_speed, const char *wind_dir, bool metric);
void display_wind(int wind_degrees, int wind_speed);
void display_time(time_t &epoch, bool metric);

int val_len(int b);
int right(int n, int x, int size);
int centre_text(const char *s, int x, int size);
