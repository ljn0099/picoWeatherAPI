#include "../core/weather.h"
#include "../utils/utils.h"
#include "handlers.h"
#include "server.h"
#include <jansson.h>
#include <microhttpd.h>
#include <stdio.h>
#include <string.h>

#define DEFAULT_SESSION_AGE 3600

int apiError_to_http(apiError_t err, char **data) {
    switch (err) {
        case API_OK:
            return MHD_HTTP_OK;
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
    if (strcmp(handlerContext->method, "GET") == 0)
        handle_users_list(handlerContext, userId);
    else if (strcmp(handlerContext->method, "POST") == 0)
        handle_users_create(handlerContext);
    else if (strcmp(handlerContext->method, "DELETE") == 0)
        handle_users_delete(handlerContext, userId);
}

void handle_sessions(struct HandlerContext *handlerContext, const char *userId,
                     const char *sessionUUID) {
    if (strcmp(handlerContext->method, "POST") == 0)
        handle_sessions_create(handlerContext, userId);
}

void handle_api_key(struct HandlerContext *handlerContext, const char *userId,
                    const char *apiKeyUUID) {
    DEBUG_PRINTF("UserId: %s, apiKeyUUID: %s, method: %s\n", userId, apiKeyUUID,
                 handlerContext->method);
}

void handle_users_list(struct HandlerContext *handlerContext, const char *userId) {
    json_t *json = NULL;
    apiError_t code = users_list(userId, handlerContext->authData->sessionToken, &json);

    handlerContext->responseData->httpStatus =
        apiError_to_http(code, &handlerContext->responseData->data);
    if (code != API_OK) {
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

    handlerContext->responseData->httpStatus =
        apiError_to_http(errorCode, &handlerContext->responseData->data);

    json_decref(root);

    if (errorCode != API_OK) {
        return;
    }
    handlerContext->responseData->data = json_dumps(json, JSON_INDENT(2));
    json_decref(json);
}

void handle_users_delete(struct HandlerContext *handlerContext, const char *userId) {
    apiError_t code = users_delete(userId, handlerContext->authData->sessionToken);

    handlerContext->responseData->httpStatus =
        apiError_to_http(code, &handlerContext->responseData->data);
    if (code != API_OK) {
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

    errorCode = sessions_create(userId, password, tokenB64, sizeof(tokenB64),
                                DEFAULT_SESSION_AGE);

    handlerContext->responseData->sessionTokenMaxAge = DEFAULT_SESSION_AGE;


    handlerContext->responseData->httpStatus =
        apiError_to_http(errorCode, &handlerContext->responseData->data);

    json_decref(root);

    if (errorCode != API_OK) {
        return;
    }

    handlerContext->responseData->sessionToken = strdup(tokenB64);
}
