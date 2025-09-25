#ifndef HANDLERS_H
#define HANDLERS_H

#include <curl/curl.h>
#include <stddef.h>

struct ResponseData {
    char *data;
    size_t size;
    int *httpStatus;
};

void handle_list_users(CURL *curl, const char *sessionCookie, const char *userId,
                       struct ResponseData *response);

void handle_delete_user(CURL *curl, const char *sessionCookie, const char *userId,
                        struct ResponseData *response);

void handle_create_user(CURL *curl, const char *postData, size_t postSize,
                        struct ResponseData *response);

#endif
