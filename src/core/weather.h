#ifndef WEATHER_H
#define WEATHER_H

#include <jansson.h>

typedef enum {
    API_OK = 0,
    API_INVALID_PARAMS,
    API_AUTH_ERROR,
    API_NOT_FOUND,
    API_DB_ERROR,
    API_FORBIDDEN,
    API_MEMORY_ERROR,
    API_JSON_ERROR
} apiError_t;

apiError_t users_list(const char *userId, const char *sessionToken, json_t **users);
#endif
