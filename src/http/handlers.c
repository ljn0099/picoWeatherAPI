#include "server.h"
#include <stdio.h>

void handle_user_session(struct HandlerContext *handlerContext, const char *userId,
                         const char *sessionUUID) {
    printf("User ID: %s, sessionUUID: %s, method: %s\n", userId, sessionUUID, handlerContext->method);
}

void handle_user(struct HandlerContext *handlerContext, const char *userId) {
    printf("UserId: %s, method: %s\n", userId, handlerContext->method);
}

void handle_api_key(struct HandlerContext *handlerContext, const char *userId,
                    const char *apiKeyUUID) {
    printf("UserId: %s, apiKeyUUID: %s, method: %s\n", userId, apiKeyUUID, handlerContext->method);
}
