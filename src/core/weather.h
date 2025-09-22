#ifndef WEATHER_H
#define WEATHER_H

#include "../db/database.h"

dbError_t weather_init_db();

void weather_close_db();

dbError_t users_list_all(user_t *users[], int *userCount);

void users_free(user_t *users);

#endif
