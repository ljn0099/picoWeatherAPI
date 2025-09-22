#ifndef HANDLERS_H
#define HANDLERS_H

#include <stddef.h>
#include <curl/curl.h>

struct ResponseData {
    char* data;
    size_t size;
};

void handle_list_users(CURL* curl, struct ResponseData* response);

#endif
