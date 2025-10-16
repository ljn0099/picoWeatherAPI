#include <arpa/inet.h>
#include <microhttpd.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "../utils/utils.h"
#include "router.h"
#include "server.h"

struct MHD_Connection;

static struct MHD_Daemon *httpDaemon = NULL;

#define MAX_POST_DATA_SIZE 16384 // 16KiB max

static void free_request_data(struct RequestData *requestData) {
    if (requestData) {
        if (requestData->postData) {
            free(requestData->postData);
        }
        free(requestData);
    }
}

void get_client_ip(struct MHD_Connection *connection, char *clientIp, size_t clientIpSize) {
    const union MHD_ConnectionInfo *ci;
    ci = MHD_get_connection_info(connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS);

    strncpy(clientIp, "0.0.0.0", clientIpSize);
    clientIp[clientIpSize - 1] = '\0';

    if (ci && ci->client_addr) {
        if (ci->client_addr->sa_family == AF_INET) {
            // IPv4
            struct sockaddr_in *addr4 = (struct sockaddr_in *)ci->client_addr;
            inet_ntop(AF_INET, &addr4->sin_addr, clientIp, clientIpSize);
        }
        else if (ci->client_addr->sa_family == AF_INET6) {
            struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)ci->client_addr;

            // IPv4-mapped IPv6 check
            if (IN6_IS_ADDR_V4MAPPED(&addr6->sin6_addr)) {
                struct in_addr ipv4;
                memcpy(&ipv4, &addr6->sin6_addr.s6_addr[12], sizeof(ipv4));
                inet_ntop(AF_INET, &ipv4, clientIp, clientIpSize);
            }
            else {
                inet_ntop(AF_INET6, &addr6->sin6_addr, clientIp, clientIpSize);
            }
        }
    }
}

bool method_accepts_body(const char *method) {
    if (strcmp(method, "POST") == 0 || strcmp(method, "PUT") == 0 || strcmp(method, "PATCH") == 0)
        return true;
    else
        return false;
}

static enum MHD_Result process_param(void *cls, enum MHD_ValueKind kind, const char *key,
                                     const char *value) {
    struct QueryData *queryData = cls;
    (void)kind;
    DEBUG_PRINTF("Processing: %s = %s\n", key, value);

    if (strcmp(key, "start_time") == 0) {
        queryData->startTime = strdup(value);
    }
    else if (strcmp(key, "end_time") == 0) {
        queryData->endTime = strdup(value);
    }
    else if (strcmp(key, "timezone") == 0) {
        queryData->timezone = strdup(value);
    }
    else if (strcmp(key, "granularity") == 0) {
        queryData->granularity = strdup(value);
    }
    else if (strcmp(key, "fields") == 0) {
        char *tmp = strdup(value);
        if (!tmp) return MHD_NO;

        char *saveptr;
        char *token = strtok_r(tmp, ",", &saveptr);
        queryData->fields = 0;

        while (token) {
            int field = string_to_field(token);
            if (field >= 0) {
                DEBUG_PRINTF("Field: %s\n", token);
                queryData->fields |= field;
            }
            token = strtok_r(NULL, ",", &saveptr);
        }

        free(tmp);
    }


    return MHD_YES;
}

static enum MHD_Result handle_request(void *cls, struct MHD_Connection *connection, const char *url,
                                      const char *method, const char *version,
                                      const char *uploadData, size_t *uploadDataSize,
                                      void **conCls) {
    (void)cls;
    (void)version;

    DEBUG_PRINTF("Request [%s] %s\n", method, url);
    DEBUG_PRINTF("uploadDataSize = %zu\n", uploadDataSize ? *uploadDataSize : 0);

    struct RequestData *requestData = *conCls;

    // Reserve memory for the body of the request
    if (method_accepts_body(method) && requestData == NULL) {
        DEBUG_PRINTF("First call - initializing connection info\n");
        requestData = calloc(1, sizeof(struct RequestData));
        if (!requestData)
            return MHD_NO;

        requestData->postData = malloc(MAX_POST_DATA_SIZE);
        if (!requestData->postData) {
            free(requestData);
            return MHD_NO;
        }
        requestData->postData[0] = '\0';
        requestData->postDataSize = 0;
        requestData->postDataProcessed = 0;

        *conCls = requestData;
        return MHD_YES;
    }

    // Accumulate incoming body data into a buffer
    if (method_accepts_body(method) && *uploadDataSize != 0) {
        DEBUG_PRINTF("Received %zu bytes of POST/PUT data\n", *uploadDataSize);
        if (uploadData) {
            DEBUG_PRINTF("Data: '%.*s'\n", (int)*uploadDataSize, uploadData);
        }

        if (requestData->postDataSize + *uploadDataSize <= MAX_POST_DATA_SIZE) {
            memcpy(requestData->postData + requestData->postDataSize, uploadData, *uploadDataSize);
            requestData->postDataSize += *uploadDataSize;
            requestData->postData[requestData->postDataSize] = '\0'; // Null-terminate

            DEBUG_PRINTF("Total accumulated data: %zu bytes\n", requestData->postDataSize);
            *uploadDataSize = 0;
            return MHD_YES;
        }
        return MHD_NO;
    }

    // Mark the body as fully recived
    if (method_accepts_body(method) && !requestData->postDataProcessed) {
        DEBUG_PRINTF("Finished receiving data - setting processed flag\n");
        requestData->postDataProcessed = 1;
        return MHD_YES;
    }

    // Print the body
    if (requestData != NULL) {
        DEBUG_PRINTF("Processing request with %zu bytes of data\n", requestData->postDataSize);
        if (requestData->postDataSize > 0) {
            DEBUG_PRINTF("Data: '%.*s'\n", (int)requestData->postDataSize, requestData->postData);
        }
    }

    // Response data initialization
    struct ResponseData responseData;
    responseData.data = NULL;
    responseData.httpStatus = MHD_HTTP_NOT_FOUND;
    responseData.sessionToken = NULL;
    responseData.sessionTokenMaxAge = 3600;

    // Auth data initialization
    struct AuthData authData;
    authData.sessionToken =
        MHD_lookup_connection_value(connection, MHD_COOKIE_KIND, "sessiontoken");
    authData.apiKey = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-API-KEY");

    // Get IP address
    get_client_ip(connection, authData.clientIp, sizeof(authData.clientIp));

    // Get user agent
    authData.userAgent = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "User-Agent");

    DEBUG_PRINTF("Cliente IP: %s, User-Agent: %s\n", authData.clientIp, authData.userAgent);

    struct QueryData queryData = {NULL, NULL, NULL, NULL, -1};

    MHD_get_connection_values(connection, MHD_GET_ARGUMENT_KIND, process_param, &queryData);

    // ---- Endpoint handling -----
    struct HandlerContext handlerContext;
    handlerContext.method = method;
    handlerContext.responseData = &responseData;
    handlerContext.authData = &authData;
    handlerContext.requestData = requestData;
    handlerContext.queryData = &queryData;

    // Init scanner with the extra context
    yyscan_t scanner;
    yylex_init_extra(&handlerContext, &scanner);

    // Create buffer from URL
    YY_BUFFER_STATE flexBuffer = yy_scan_string(url, scanner);

    // Execute flex
    yylex(scanner);

    // Free resources
    yy_delete_buffer(flexBuffer, scanner);
    yylex_destroy(scanner);
    // ---------------------------------

    // Response handling
    struct MHD_Response *response;
    enum MHD_Result ret;

    // Check if responseData.data was written
    if (!responseData.data) {
        responseData.data = strdup("");
    }

    if (queryData.startTime)
        free(queryData.startTime);

    if (queryData.endTime)
        free(queryData.endTime);

    if (queryData.timezone)
        free(queryData.timezone);

    if (queryData.granularity)
        free(queryData.granularity);

    // Create the response and say MHD to free the responseData.data on finish
    response = MHD_create_response_from_buffer(strlen(responseData.data), responseData.data,
                                               MHD_RESPMEM_MUST_FREE);
    MHD_add_response_header(response, "Content-Type", "application/json");
    if (strcmp(method, "GET") == 0) {
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    }

    // Create cookie if exists
    if (responseData.sessionToken) {
        char cookieHeader[256];
        snprintf(cookieHeader, sizeof(cookieHeader),
                 "sessiontoken=%s; Path=/; HttpOnly; Secure; SameSite=Lax; Max-Age=%d",
                 responseData.sessionToken, responseData.sessionTokenMaxAge);
        MHD_add_response_header(response, "Set-Cookie", cookieHeader);

        free(responseData.sessionToken);
    }

    // Send and destroy the response
    ret = MHD_queue_response(connection, responseData.httpStatus, response);
    MHD_destroy_response(response);

    // Free the post data
    free_request_data(requestData);
    *conCls = NULL;

    return ret;
}

int http_server_init(int port, int nThreads) {
    httpDaemon = MHD_start_daemon(MHD_USE_EPOLL_INTERNALLY | MHD_USE_IPv6 | MHD_USE_DUAL_STACK,
                                  port, NULL, NULL, &handle_request, NULL,
                                  MHD_OPTION_THREAD_POOL_SIZE, nThreads, MHD_OPTION_END);

    if (httpDaemon)
        return 0;
    else
        return -1;
}

void http_server_process(void) {
}

void http_server_cleanup(void) {
    if (httpDaemon) {
        MHD_stop_daemon(httpDaemon);
        httpDaemon = NULL;
    }
}
