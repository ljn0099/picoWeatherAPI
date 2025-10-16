#ifndef WEATHER_H
#define WEATHER_H

#include <jansson.h>
#include <stddef.h>

struct AuthData;

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
    KEY_TYPE_WEATHER_UPLOAD = 0,
    KEY_TYPE_STATIONS_MNG,
    KEY_TYPE_STATIONS_CONTROL,
    KEY_TYPE_INVALID
} apiKeyType_t;

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

apiError_t api_key_create(const char *name, const char *keyType, const char *stationId,
                          const char *userId, const struct AuthData *authData, json_t **key);

apiError_t api_key_list(const char *userId, const char *keyId, const struct AuthData *authData,
                        json_t **keys);

apiError_t weather_data_list(int fields, const char *granularityStr, const char *stationId,
                             const char *timezone, const char *startTime, const char *endTime,
                             json_t **weatherData);
#endif
