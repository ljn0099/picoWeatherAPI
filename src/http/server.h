#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>
#include <microhttpd.h>

struct HandlerContext {
    const char *method;
    struct ResponseData *responseData;
    struct AuthData *authData;
};

struct ResponseData {
    char *data;
    int httpStatus;
    char *sessionToken;
    int sessionTokenMaxAge;
};

struct AuthData {
    const char *sessionToken;
    const char *apiKey;
    char clientIp[INET6_ADDRSTRLEN];
    const char *userAgent;
};

int http_server_init(int port);

void http_server_process(void);

void http_server_cleanup(void);

#endif
