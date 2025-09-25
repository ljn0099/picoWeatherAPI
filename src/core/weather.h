#ifndef WEATHER_H
#define WEATHER_H

#include "../db/database.h"

dbError_t weather_init_db();

void weather_close_db();

dbError_t users_list(user_t *users[], int *userCount, const char *sessionCookie, const char *userId, bool nocheck);

dbError_t user_delete(const char *sessionCookie, const char *userId);

dbError_t user_create(user_t *user, user_t *userResponse);

bool validate_cookie(const char *sessionCookie, const char *userId);

void users_free(user_t *users);

#endif
