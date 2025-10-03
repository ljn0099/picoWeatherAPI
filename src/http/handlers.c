#include "../core/weather.h"
#include "handlers.h"
#include "server.h"
#include <jansson.h>
#include <stdio.h>
#include <string.h>

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
    if (!users_list(userId, handlerContext->authData->sessionToken, &json))
        return;

    handlerContext->responseData->data = json_dumps(json, JSON_INDENT(2));

    json_decref(json);
}
