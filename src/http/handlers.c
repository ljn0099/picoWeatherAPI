#include <jansson.h>
#include <microhttpd.h>
#include <stdio.h>
#include <string.h>
#include <sodium/utils.h>

#include "../core/weather.h"
#include "../utils/utils.h"
#include "handlers.h"
#include "server.h"

#define DEFAULT_SESSION_AGE 3600

int apiError_to_http(apiError_t err, char **data) {
    switch (err) {
        case API_INVALID_PARAMS:
            *data = strdup("{\"error\":\"Invalid parameters\"}");
            return MHD_HTTP_BAD_REQUEST;
        case API_AUTH_ERROR:
            *data = strdup("{\"error\":\"Authentication error\"}");
            return MHD_HTTP_UNAUTHORIZED;
        case API_FORBIDDEN:
            *data = strdup("{\"error\":\"Forbidden\"}");
            return MHD_HTTP_FORBIDDEN;
        case API_NOT_FOUND:
            *data = strdup("{\"error\":\"Resource not found\"}");
            return MHD_HTTP_NOT_FOUND;
        case API_DB_ERROR:
            *data = strdup("{\"error\":\"Database error\"}");
            return MHD_HTTP_INTERNAL_SERVER_ERROR;
        case API_JSON_ERROR:
            *data = strdup("{\"error\":\"Json parsing error\"}");
            return MHD_HTTP_INTERNAL_SERVER_ERROR;
        case API_MEMORY_ERROR:
            *data = strdup("{\"error\":\"Memory error\"}");
            return MHD_HTTP_INTERNAL_SERVER_ERROR;
        default:
            *data = strdup("{\"error\":\"Internal server error\"}");
            return MHD_HTTP_INTERNAL_SERVER_ERROR;
    }
}

void handle_user(struct HandlerContext *handlerContext, const char *userId) {
    if (strcmp(handlerContext->method, "GET") == 0) {
        handlerContext->responseData->httpStatus = MHD_HTTP_OK;
        handle_users_list(handlerContext, userId);
    }
    else if (strcmp(handlerContext->method, "POST") == 0) {
        handlerContext->responseData->httpStatus = MHD_HTTP_CREATED;
        handle_users_create(handlerContext);
    }
    else if (strcmp(handlerContext->method, "DELETE") == 0) {
        handlerContext->responseData->httpStatus = MHD_HTTP_NO_CONTENT;
        handle_users_delete(handlerContext, userId);
    }
}

void handle_sessions(struct HandlerContext *handlerContext, const char *userId,
                     const char *sessionUUID) {
    if (strcmp(handlerContext->method, "GET") == 0) {
        handlerContext->responseData->httpStatus = MHD_HTTP_OK;
        handle_sessions_list(handlerContext, userId, sessionUUID);
    }
    else if (strcmp(handlerContext->method, "POST") == 0) {
        handlerContext->responseData->httpStatus = MHD_HTTP_CREATED;
        handle_sessions_create(handlerContext, userId);
    }
    else if (strcmp(handlerContext->method, "DELETE") == 0) {
        handlerContext->responseData->httpStatus = MHD_HTTP_NO_CONTENT;
        handle_sessions_delete(handlerContext, userId, sessionUUID);
    }
}

void handle_stations(struct HandlerContext *handlerContext, const char *stationId) {
    if (strcmp(handlerContext->method, "GET") == 0) {
        handlerContext->responseData->httpStatus = MHD_HTTP_OK;
        handle_stations_list(handlerContext, stationId);
    }
    else if (strcmp(handlerContext->method, "POST") == 0) {
        handlerContext->responseData->httpStatus = MHD_HTTP_CREATED;
        handle_stations_create(handlerContext);
    }
}

void handle_api_key(struct HandlerContext *handlerContext, const char *userId,
                    const char *apiKeyUUID) {
    if (strcmp(handlerContext->method, "POST") == 0) {
        handlerContext->responseData->httpStatus = MHD_HTTP_CREATED;
        handle_api_key_create(handlerContext, userId);
    }
}

void handle_users_list(struct HandlerContext *handlerContext, const char *userId) {
    json_t *json = NULL;
    apiError_t code = users_list(userId, handlerContext->authData, &json);

    if (code != API_OK) {
        handlerContext->responseData->httpStatus =
            apiError_to_http(code, &handlerContext->responseData->data);
        return;
    }

    handlerContext->responseData->data = json_dumps(json, JSON_INDENT(2));

    json_decref(json);
}

void handle_users_create(struct HandlerContext *handlerContext) {
    apiError_t errorCode;

    if (!handlerContext->requestData->postData || handlerContext->requestData->postDataSize <= 0) {
        errorCode = API_INVALID_PARAMS;
        handlerContext->responseData->httpStatus =
            apiError_to_http(errorCode, &handlerContext->responseData->data);
        return;
    }

    json_t *root = json_loadb(handlerContext->requestData->postData,
                              handlerContext->requestData->postDataSize, 0, NULL);

    if (!root || !json_is_object(root)) {
        errorCode = API_INVALID_PARAMS;
        handlerContext->responseData->httpStatus =
            apiError_to_http(errorCode, &handlerContext->responseData->data);
        return;
    }

    const char *username = json_string_value(json_object_get(root, "username"));
    const char *email = json_string_value(json_object_get(root, "email"));
    const char *password = json_string_value(json_object_get(root, "password"));

    json_t *json = NULL;

    errorCode = users_create(username, email, password, &json);

    json_decref(root);

    if (errorCode != API_OK) {
        handlerContext->responseData->httpStatus =
            apiError_to_http(errorCode, &handlerContext->responseData->data);
        return;
    }
    handlerContext->responseData->data = json_dumps(json, JSON_INDENT(2));
    json_decref(json);
}

void handle_users_delete(struct HandlerContext *handlerContext, const char *userId) {
    apiError_t code = users_delete(userId, handlerContext->authData);

    if (code != API_OK) {
        handlerContext->responseData->httpStatus =
            apiError_to_http(code, &handlerContext->responseData->data);
        return;
    }
}

void handle_sessions_create(struct HandlerContext *handlerContext, const char *userId) {
    apiError_t errorCode;

    if (!handlerContext->requestData->postData || handlerContext->requestData->postDataSize <= 0) {
        errorCode = API_INVALID_PARAMS;
        handlerContext->responseData->httpStatus =
            apiError_to_http(errorCode, &handlerContext->responseData->data);
        return;
    }

    json_t *root = json_loadb(handlerContext->requestData->postData,
                              handlerContext->requestData->postDataSize, 0, NULL);

    if (!root || !json_is_object(root)) {
        errorCode = API_INVALID_PARAMS;
        handlerContext->responseData->httpStatus =
            apiError_to_http(errorCode, &handlerContext->responseData->data);
        return;
    }

    const char *password = json_string_value(json_object_get(root, "password"));

    char tokenB64[sodium_base64_ENCODED_LEN(KEY_ENTROPY, BASE64_VARIANT)];

    json_t *json = NULL;

    errorCode = sessions_create(userId, handlerContext->authData, password, tokenB64,
                                sizeof(tokenB64), DEFAULT_SESSION_AGE, &json);

    handlerContext->responseData->sessionTokenMaxAge = DEFAULT_SESSION_AGE;

    json_decref(root);

    if (errorCode != API_OK) {
        handlerContext->responseData->httpStatus =
            apiError_to_http(errorCode, &handlerContext->responseData->data);
        return;
    }

    handlerContext->responseData->sessionToken = strdup(tokenB64);
    handlerContext->responseData->data = json_dumps(json, JSON_INDENT(2));
    json_decref(json);
}

void handle_sessions_list(struct HandlerContext *handlerContext, const char *userId,
                          const char *sessionUUID) {
    json_t *json = NULL;
    apiError_t code = sessions_list(userId, sessionUUID, handlerContext->authData, &json);

    if (code != API_OK) {
        handlerContext->responseData->httpStatus =
            apiError_to_http(code, &handlerContext->responseData->data);
        return;
    }

    handlerContext->responseData->data = json_dumps(json, JSON_INDENT(2));

    json_decref(json);
}

void handle_sessions_delete(struct HandlerContext *handlerContext, const char *userId,
                            const char *sessionUUID) {
    apiError_t code = sessions_delete(userId, sessionUUID, handlerContext->authData);

    if (code != API_OK) {
        handlerContext->responseData->httpStatus =
            apiError_to_http(code, &handlerContext->responseData->data);
        return;
    }
}

void handle_stations_create(struct HandlerContext *handlerContext) {
    apiError_t errorCode;

    if (!handlerContext->requestData->postData || handlerContext->requestData->postDataSize <= 0) {
        errorCode = API_INVALID_PARAMS;
        handlerContext->responseData->httpStatus =
            apiError_to_http(errorCode, &handlerContext->responseData->data);
        return;
    }

    json_t *root = json_loadb(handlerContext->requestData->postData,
                              handlerContext->requestData->postDataSize, 0, NULL);

    if (!root || !json_is_object(root)) {
        errorCode = API_INVALID_PARAMS;
        handlerContext->responseData->httpStatus =
            apiError_to_http(errorCode, &handlerContext->responseData->data);
        return;
    }

    const char *name = json_string_value(json_object_get(root, "name"));
    const double lat = json_real_value(json_object_get(root, "lat"));
    const double lon = json_real_value(json_object_get(root, "lon"));
    const double alt = json_real_value(json_object_get(root, "altitude"));

    json_t *json = NULL;

    errorCode = stations_create(name, lon, lat, alt, handlerContext->authData, &json);

    json_decref(root);

    if (errorCode != API_OK) {
        handlerContext->responseData->httpStatus =
            apiError_to_http(errorCode, &handlerContext->responseData->data);
        return;
    }

    handlerContext->responseData->data = json_dumps(json, JSON_INDENT(2));
    json_decref(json);
}

void handle_stations_list(struct HandlerContext *handlerContext, const char *stationId) {
    json_t *json = NULL;
    apiError_t code = stations_list(stationId, &json);

    if (code != API_OK) {
        handlerContext->responseData->httpStatus =
            apiError_to_http(code, &handlerContext->responseData->data);
        return;
    }

    handlerContext->responseData->data = json_dumps(json, JSON_INDENT(2));

    json_decref(json);
}

void handle_api_key_create(struct HandlerContext *handlerContext, const char *userId) {
    apiError_t errorCode;

    if (!handlerContext->requestData->postData || handlerContext->requestData->postDataSize <= 0) {
        errorCode = API_INVALID_PARAMS;
        handlerContext->responseData->httpStatus =
            apiError_to_http(errorCode, &handlerContext->responseData->data);
        return;
    }

    json_t *root = json_loadb(handlerContext->requestData->postData,
                              handlerContext->requestData->postDataSize, 0, NULL);

    if (!root || !json_is_object(root)) {
        errorCode = API_INVALID_PARAMS;
        handlerContext->responseData->httpStatus =
            apiError_to_http(errorCode, &handlerContext->responseData->data);
        return;
    }

    const char *name = json_string_value(json_object_get(root, "name"));
    const char *keyType = json_string_value(json_object_get(root, "api_key_type"));
    const char *stationId = json_string_value(json_object_get(root, "station_id"));

    json_t *json = NULL;

    errorCode = api_key_create(name, keyType, stationId, userId, handlerContext->authData, &json);

    json_decref(root);

    if (errorCode != API_OK) {
        handlerContext->responseData->httpStatus =
            apiError_to_http(errorCode, &handlerContext->responseData->data);
        return;
    }

    handlerContext->responseData->data = json_dumps(json, JSON_INDENT(2));
    json_decref(json);
}

void handle_weather_data_list(struct HandlerContext *handlerContext, const char *stationId) {
    handlerContext->responseData->httpStatus = MHD_HTTP_OK;
    json_t *json;
    apiError_t code = weather_data_list(
        handlerContext->queryData->fields, handlerContext->queryData->granularity, stationId,
        handlerContext->queryData->timezone, handlerContext->queryData->startTime,
        handlerContext->queryData->endTime, &json);

    if (code != API_OK) {
        handlerContext->responseData->httpStatus =
            apiError_to_http(code, &handlerContext->responseData->data);
        return;
    }

    handlerContext->responseData->data = json_dumps(json, JSON_INDENT(2));

    json_decref(json);
}
