#ifndef WEATHER_H
#define WEATHER_H

#include <stdbool.h>

/* 查 Open-Meteo，把结果收成一行中文写进 out，形如「重庆 多云 24°C」。
   weather_fetch 用默认城市；weather_fetch_city 先把城市名换成经纬度再查。
   成功返回 true，失败返回 false 且 out 写「天气不可用」。 */

bool weather_fetch(char *out, int out_len);
bool weather_fetch_city(const char *city, char *out, int out_len);

#endif
