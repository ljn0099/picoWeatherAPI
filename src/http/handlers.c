#include "../core/weather.h"
#include "../db/database.h"
#include "handlers.h"
#include <curl/curl.h>
#include <microhttpd.h>
#include <jansson.h>
#include <stdlib.h>
#include <string.h>

PGconn *conn = NULL;

static void json_copy_string(json_t *root, const char *key, char *dest, size_t destSize) {
    const char *tmp = json_string_value(json_object_get(root, key));
    if (tmp) {
        strncpy(dest, tmp, destSize - 1);
        dest[destSize - 1] = '\0';
    } else {
        dest[0] = '\0';
    }
}

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

    if (users_list(&users, &count, sessionCookie, userId, false) == DB_OK) {
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

void handle_create_user(CURL *curl, const char *postData, size_t postSize, struct ResponseData *response) {
    (void)curl;
    if (postData) {
        printf("DEBUG: postData = '%.*s'\n", (int)postSize, postData);
    }

    if (!postData || postSize <= 0) {
        printf("DEBUG: No data received\n");
        response->data = strdup("{\"error\": \"No data received\"}");
        response->size = strlen(response->data);
        return;
    }

    json_error_t error;
    json_t* root = json_loadb(postData, postSize, 0, &error);

    if (!root || !json_is_object(root)) {
        printf("DEBUG: Invalid JSON data. Error: %s\n", error.text);
        response->data = strdup("{\"error\": \"Invalid JSON data\"}");
        response->size = strlen(response->data);
        return;
    }
    
    printf("DEBUG: Successfully parsed JSON\n");
    user_t user;
    json_copy_string(root, "username", user.username, sizeof(user.username));
    json_copy_string(root, "email", user.email, sizeof(user.email));
    json_copy_string(root, "password", user.plainPass, sizeof(user.plainPass));


    // printf("DEBUG: username = %s, email = %s, password = %s\n", 
    //        user.username, 
    //        user.email,
    //        user.password);

    user_t userResponse;
    if (user.username[0] != '\0' && user.email[0] != '\0' && user.plainPass[0] != '\0') {
        if (user_create(&user, &userResponse) == DB_OK) {
            json_t *userJSON = json_object();
            fill_json_from_user(&userResponse, userJSON);
            response->data = json_dumps(userJSON, JSON_INDENT(2));

            json_decref(userJSON);
            printf("DEBUG: User created successfully\n");
        } else {
            printf("DEBUG: Failed to create user\n");
            response->data = strdup("{\"error\": \"Failed to create user\"}");
        }
    } else {
        printf("DEBUG: Invalid request data\n");
        response->data = strdup("{\"error\": \"Invalid request data\"}");
    }

    json_decref(root);
    response->size = strlen(response->data);
}
