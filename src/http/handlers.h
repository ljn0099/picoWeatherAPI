#ifndef HANDLERS_H
#define HANDLERS_H

#include "server.h"

void handle_sessions(struct HandlerContext *handlerContext, const char *userId,
                         const char *sessionUUID);

void handle_user(struct HandlerContext *handlerContext, const char *userId);

void handle_api_key(struct HandlerContext *handlerContext, const char *userId,
                    const char *apiKeyUUID);

void handle_users_list(struct HandlerContext *handlerContext, const char *userId);

void handle_users_create(struct HandlerContext *handlerContext);

void handle_users_delete(struct HandlerContext *handlerContext, const char *userId);


void handle_sessions_create(struct HandlerContext *handlerContext, const char *userId);

void handle_sessions_list(struct HandlerContext *handlerContext, const char *userId, const char *sessionUUID);

#endif
