#ifndef HANDLERS_H
#define HANDLERS_H

#include "server.h"

void handle_user_session(struct HandlerContext *handlerContext, const char *userId,
                         const char *sessionUUID);

void handle_user(struct HandlerContext *handlerContext, const char *userId);

void handle_api_key(struct HandlerContext *handlerContext, const char *userId,
                    const char *apiKeyUUID);

#endif
