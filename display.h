#pragma once

extern TFT_eSPI tft;

void display_weather(struct Conditions &c);
void display_astronomy(struct Conditions &c);
void display_forecast(struct Forecast &f);
void display_about(struct Statistics &s);
