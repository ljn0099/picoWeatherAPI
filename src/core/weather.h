#ifndef WEATHER_H
#define WEATHER_H

#include <jansson.h>
#include <libpq-fe.h>
#include <stdbool.h>

bool init_db_vars();

PGconn *init_db_conn();

void close_db_conn();

bool users_list(const char *userId, const char *sessionToken, json_t **users);

#endif
