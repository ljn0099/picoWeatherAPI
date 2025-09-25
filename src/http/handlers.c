#include "../core/weather.h"
#include "../db/database.h"
#include "handlers.h"
#include <curl/curl.h>
#include <microhttpd.h>
#include <jansson.h>
#include <stdlib.h>
#include <string.h>

PGconn *conn = NULL;

void fill_json_from_user(const user_t *user, json_t *json) {
    json_object_set_new(json, "uuid", json_string(user->userUUID));
    json_object_set_new(json, "username", json_string(user->username));
    json_object_set_new(json, "email", json_string(user->email));
    json_object_set_new(json, "created_at", json_string(user->createdAt));
    json_object_set_new(json, "max_stations", json_integer(user->maxStations));
    json_object_set_new(json, "is_admin", json_boolean(user->isAdmin));
    if (user->deletedAt[0] != '\0') {
        json_object_set_new(json, "deleted_at", json_string(user->deletedAt));
    }
}

void handle_list_users(CURL *curl, const char *sessionCookie, const char *userId,
                       struct ResponseData *response) {
    (void)curl;
    user_t *users = NULL;
    int count = 0;

    if (users_list(&users, &count, sessionCookie, userId) == DB_OK) {
        if (!userId) {
            json_t *root = json_array();

            for (int i = 0; i < count; i++) {
                json_t *user = json_object();
                fill_json_from_user(&users[i], user);
                json_array_append_new(root, user);
            }

            response->data = json_dumps(root, JSON_INDENT(2));
            response->size = strlen(response->data);

            json_decref(root);
        }
        else {
            json_t *user = json_object();
            fill_json_from_user(&users[0], user);
            response->data = json_dumps(user, JSON_INDENT(2));
            response->size = strlen(response->data);

            json_decref(user);
        }
        users_free(users);
    }
    else {
        response->data = strdup("{\"error\": \"Failed to list users\"}");
        response->size = strlen(response->data);
    }
}

void handle_delete_user(CURL *curl, const char *sessionCookie, const char *userId, struct ResponseData *response) {
    (void)curl;
    if (user_delete(sessionCookie, userId) == DB_OK) {
        *(response->httpStatus) = MHD_HTTP_NO_CONTENT;
    }
    else {
        response->data = strdup("{\"error\": \"Failed to delete user\"}");
        response->size = strlen(response->data);
    }
}
