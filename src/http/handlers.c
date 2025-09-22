#include "handlers.h"
#include "../db/database.h"
#include "../core/weather.h"
#include <stdlib.h>
#include <string.h>
#include <jansson.h>
#include <curl/curl.h>

PGconn *conn = NULL;

void handle_list_users(CURL* curl, struct ResponseData* response) {
    (void)curl;
    user_t *users = NULL;
    int count = 0;

    if (users_list_all(&users, &count) == 0) {
        json_t* root = json_array();

        for (int i = 0; i < count; i++) {
            json_t* user = json_object();
            json_object_set_new(user, "user_id", json_integer(users[i].userId));
            json_object_set_new(user, "username", json_string(users[i].username));
            json_object_set_new(user, "email", json_string(users[i].email));
            json_object_set_new(user, "created_at", json_string(users[i].createdAt));
            json_object_set_new(user, "max_stations", json_integer(users[i].maxStations));
            json_object_set_new(user, "isAdmin", json_boolean(users[i].isAdmin));
            if (users[i].deletedAt[0] != '\0') {
                json_object_set_new(user, "deleted_at", json_string(users[i].deletedAt));
            }
            json_array_append_new(root, user);
        }

        response->data = json_dumps(root, JSON_INDENT(2));
        response->size = strlen(response->data);

        json_decref(root);
        users_free(users);
    } else {
        response->data = strdup("{\"error\": \"Failed to list users\"}");
        response->size = strlen(response->data);
    }
}
