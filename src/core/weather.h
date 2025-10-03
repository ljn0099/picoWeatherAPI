#ifndef WEATHER_H
#define WEATHER_H

#include <jansson.h>
#include <libpq-fe.h>
#include <stdbool.h>

typedef enum {
    API_OK = 0,
    API_INVALID_PARAMS,
    API_AUTH_ERROR,
    API_NOT_FOUND,
    API_DB_ERROR,
    API_FORBIDDEN,
    API_JSON_ERROR
} apiError_t;

bool init_db_vars();

PGconn *init_db_conn();

void close_db_conn();

apiError_t users_list(const char *userId, const char *sessionToken, json_t **users);
#endif
