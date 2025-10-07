#ifndef WEATHER_H
#define WEATHER_H

#include "../core/weather.h"
#include "../http/server.h"
#include <jansson.h>

#define DEFAULT_TIMEZONE "Europe/Madrid"

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

typedef enum {
    DATA_TEMP = (1 << 0),
    DATA_HUMIDITY = (1 << 1),
    DATA_PRESSURE = (1 << 2),
    DATA_LUX = (1 << 3),
    DATA_UVI = (1 << 4),
    DATA_WIND_SPEED = (1 << 5),
    DATA_WIND_DIRECTION = (1 << 6),
    DATA_GUST_SPEED = (1 << 7),
    DATA_GUST_DIRECTION = (1 << 8),
    DATA_RAINFALL = (1 << 9),
    DATA_SOLAR_IRRADIANCE = (1 << 10)
} dataFlags_t;

typedef enum {
    SUMMARY_AVG_TEMPERATURE = (1 << 0),
    SUMMARY_MAX_TEMPERATURE = (1 << 1),
    SUMMARY_MIN_TEMPERATURE = (1 << 2),
    SUMMARY_STDDEV_TEMPERATURE = (1 << 3),
    SUMMARY_AVG_HUMIDITY = (1 << 4),
    SUMMARY_MAX_HUMIDITY = (1 << 5),
    SUMMARY_MIN_HUMIDITY = (1 << 6),
    SUMMARY_STDDEV_HUMIDITY = (1 << 7),
    SUMMARY_AVG_PRESSURE = (1 << 8),
    SUMMARY_MAX_PRESSURE = (1 << 9),
    SUMMARY_MIN_PRESSURE = (1 << 10),
    SUMMARY_SUM_RAINFALL = (1 << 11),
    SUMMARY_STDDEV_RAINFALL = (1 << 12),
    SUMMARY_AVG_WIND_SPEED = (1 << 13),
    SUMMARY_AVG_WIND_DIRECTION = (1 << 14),
    SUMMARY_STDDEV_WIND_SPEED = (1 << 15),
    SUMMARY_WIND_RUN = (1 << 16),
    SUMMARY_MAX_GUST_SPEED = (1 << 17),
    SUMMARY_MAX_GUST_DIRECTION = (1 << 18),
    SUMMARY_AVG_LUX = (1 << 19),
    SUMMARY_MAX_LUX = (1 << 20),
    SUMMARY_AVG_UVI = (1 << 21),
    SUMMARY_MAX_UVI = (1 << 22),
    SUMMARY_AVG_SOLAR_IRRADIANCE = (1 << 23),
} summaryFalgs_t;

typedef enum {
    GRANULARITY_DATA = 0,
    GRANULARITY_HOUR,
    GRANULARITY_DAY,
    GRANULARITY_MONTH,
    GRANULARITY_YEAR
} granularity_t;

apiError_t users_list(const char *userId, const struct AuthData *authData, json_t **users);

apiError_t users_create(const char *username, const char *email, const char *password,
                        json_t **user);

apiError_t users_delete(const char *userId, const struct AuthData *authData);

apiError_t sessions_create(const char *userId, const struct AuthData *authData,
                           const char *password, char *sessionToken, size_t sessionTokenLen,
                           int sessionTokenMaxAge, json_t **session);

apiError_t sessions_list(const char *userId, const char *sessionUUID,
                         const struct AuthData *authData, json_t **sessions);

apiError_t sessions_delete(const char *userId, const char *sessionUUID,
                           const struct AuthData *authData);

apiError_t stations_create(const char *name, double lon, double lat, double alt,
                           const struct AuthData *authData, json_t **station);

apiError_t stations_list(const char *stationId, json_t **stations);
#endif
