#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>
#include <stddef.h>

struct HandlerContext {
    const char *method;
    struct ResponseData *responseData;
    struct AuthData *authData;
    struct RequestData *requestData;
    struct QueryData *queryData;
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

struct RequestData {
    char *postData;
    size_t postDataSize;
    int postDataProcessed;
};

struct QueryData {
    char *startTime;
    char *endTime;
    char *timezone;
    char *granularity;
    int fields;
};

int http_server_init(int port, int nThreads);

void http_server_process(void);

void http_server_cleanup(void);

#endif
