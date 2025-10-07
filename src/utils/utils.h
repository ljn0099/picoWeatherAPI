#ifndef UTILS_H
#define UTILS_H

#include "../core/weather.h"
#include <jansson.h>
#include <libpq-fe.h>
#include <sodium.h>
#include <stdbool.h>

#ifdef DEBUG
#define DEBUG_PRINTF(...)                                                                          \
    do {                                                                                           \
        fprintf(stderr, "DEBUG: ");                                                                \
        fprintf(stderr, __VA_ARGS__);                                                              \
    } while (0)
#else
#define DEBUG_PRINTF(...) ((void)0)
#endif

#define UUID_SIZE 36
#define NAME_SIZE 30
#define NAME_SIZE_MIN 3
#define KEY_ENTROPY 32
#define BASE64_VARIANT sodium_base64_VARIANT_URLSAFE_NO_PADDING

bool validate_name(const char *str);
bool validate_uuid(const char *uuid);

bool validate_password(PGconn *conn, const char *userId, const char *password);

bool validate_session_token(PGconn *conn, const char *userId, const char *sessionToken);

void generateSessionToken(char *tokenB64, size_t tokenB64Len, char *hashB64, size_t hashB64Len);

bool get_user_session_token(PGconn *conn, char **userId, const char *sessionToken);

bool validate_timestamp(const char *timestamp);

bool validate_email(const char *email);

char *build_generic_weather_query(int fields);

json_t *pgresult_to_json(PGresult *res, bool canBeObject);

granularity_t string_to_granularity(const char *granularityStr);

// Query with 4 params $1 = stationId, $2 startTime, $3 endTime, $4 granularity
char *build_generic_weather_query(int fields);

// Query with 3 params $1 = stationId, $2 startTime, $3 endTime
char *build_static_query(int fields, granularity_t granularity);

bool same_timezone_offset_during_range(const char *startStr, const char *endStr, const char *tz1,
                                       const char *tz2);
#endif
