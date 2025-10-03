#include "../core/weather.h"
#include "handlers.h"
#include "server.h"
#include <jansson.h>
#include <stdio.h>
#include <string.h>
#include <microhttpd.h>

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
        default:
            *data = strdup("{\"error\":\"Internal server error\"}");
            return MHD_HTTP_INTERNAL_SERVER_ERROR;
    }
}

void handle_user(struct HandlerContext *handlerContext, const char *userId) {
    if (strcmp(handlerContext->method, "GET") == 0)
        handle_users_list(handlerContext, userId);
}

void handle_user_session(struct HandlerContext *handlerContext, const char *userId,
                         const char *sessionUUID) {
    printf("User ID: %s, sessionUUID: %s, method: %s\n", userId, sessionUUID,
           handlerContext->method);
}

void handle_api_key(struct HandlerContext *handlerContext, const char *userId,
                    const char *apiKeyUUID) {
    printf("UserId: %s, apiKeyUUID: %s, method: %s\n", userId, apiKeyUUID, handlerContext->method);
}

void handle_users_list(struct HandlerContext *handlerContext, const char *userId) {
    json_t *json = NULL;
    apiError_t code = users_list(userId, handlerContext->authData->sessionToken, &json);

    handlerContext->responseData->httpStatus = apiError_to_http(code, &handlerContext->responseData->data);
    if (code != API_OK) {
        return;
    }

    handlerContext->responseData->data = json_dumps(json, JSON_INDENT(2));

    json_decref(json);
}
