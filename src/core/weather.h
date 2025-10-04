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
apiError_t users_create(const char *username, const char *email, const char *password,
        json_t **user);
apiError_t users_delete(const char *userId, const char *sessionToken);
apiError_t sessions_create(const char *userId, const char *password, char *sessionToken,
                           size_t sessionTokenLen, int sessionTokenMaxAge);
#endif
