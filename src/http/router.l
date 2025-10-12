%{
#include <stdio.h>
#include <string.h>
#include "../utils/utils.h"
#include "server.h"
#include "handlers.h"

#define USERS_PREFIX_LEN (sizeof("/users/") - 1)
#define SESSIONS_PREFIX_LEN (sizeof("/sessions/") - 1)
#define STATIONS_PREFIX_LEN (sizeof("/stations/") - 1)
#define DATA_PREFIX_LEN (sizeof("/data") - 1)

%}

%option noinput
%option nounput
%option noyywrap
%option reentrant
%option extra-type="struct HandlerContext*"

%%

"/users" {
    handle_user(yyget_extra(yyscanner), NULL);
}

"/users/"[^/]+ {
    char *userId = yytext + USERS_PREFIX_LEN;

    // Validation
    if (!validate_name(userId) && !validate_uuid(userId)) {
        DEBUG_PRINTF("Invalid userId: %s\n", userId);
        return 0;
    }

    handle_user(yyget_extra(yyscanner), userId);
}

"/users/"[^/]+"/api-keys" {
    char *start = yytext + USERS_PREFIX_LEN;  // after '/users/'
    char *slash = strchr(start, '/');  // find the '/'
    *slash = '\0';

    char *userId = start;
    // Validation
    if (!validate_name(userId) && !validate_uuid(userId)) {
        DEBUG_PRINTF("Invalid userId: %s\n", userId);
        return 0;
    }

    handle_api_key(yyget_extra(yyscanner), userId, NULL);
}

"/users/"[^/]+"/api-keys/"[^/]+ {
    char *start = yytext + USERS_PREFIX_LEN;  // after '/users/'
    char *slash = strchr(start, '/');  // find the '/'
    if (slash)
        *slash = '\0';

    char *userId = start;
    char *uuid = slash + SESSIONS_PREFIX_LEN; // after "/sessions/"

    // Validation
    if (!validate_name(userId) && !validate_uuid(userId)) {
        DEBUG_PRINTF("Invalid name: %s\n", userId);
        return 0;
    }

    if (!validate_uuid(uuid)) {
        DEBUG_PRINTF("Invalid UUID: %s\n", uuid);
        return 0;
    }

    handle_api_key(yyget_extra(yyscanner), userId, uuid);
}

"/users/"[^/]+"/sessions" {
    char *start = yytext + USERS_PREFIX_LEN;  // after '/users/'
    char *slash = strchr(start, '/');  // find the '/'
    *slash = '\0';

    char *userId = start;
    // Validation
    if (!validate_name(userId) && !validate_uuid(userId)) {
        DEBUG_PRINTF("Invalid userId: %s\n", userId);
        return 0;
    }

    handle_sessions(yyget_extra(yyscanner), userId, NULL);
}

"/users/"[^/]+"/sessions/"[^/]+ {
    char *start = yytext + USERS_PREFIX_LEN;  // after '/users/'
    char *slash = strchr(start, '/');  // find the '/'
    if (slash)
        *slash = '\0';

    char *userId = start;
    char *uuid = slash + SESSIONS_PREFIX_LEN; // after "/sessions/"

    // Validation
    if (!validate_name(userId) && !validate_uuid(userId)) {
        DEBUG_PRINTF("Invalid name: %s\n", userId);
        return 0;
    }

    if (!validate_uuid(uuid)) {
        DEBUG_PRINTF("Invalid UUID: %s\n", uuid);
        return 0;
    }

    handle_sessions(yyget_extra(yyscanner), userId, uuid);
}

"/stations" {
    handle_stations(yyget_extra(yyscanner), NULL);
}

"/stations/"[^/]+ {
    char *stationId = yytext + STATIONS_PREFIX_LEN;

    // Validation
    if (!validate_name(stationId) && !validate_uuid(stationId)) {
        DEBUG_PRINTF("Invalid stationId: %s\n", stationId);
        return 0;
    }

    handle_stations(yyget_extra(yyscanner), stationId);
}

"/stations/"[^/]+"/data"([^/]+)? {
    char *start = yytext + STATIONS_PREFIX_LEN;  // after '/stations/'
    char *slash = strchr(start, '/');  // find the '/'
    if (slash)
        *slash = '\0';

    char *stationId = start;

    // Validation
    // if (!validate_name(stationId) && !validate_uuid(stationId)) {
    //     DEBUG_PRINTF("Invalid name: %s\n", stationId);
    //     return 0;
    // }

    handle_weather_data_list(yyget_extra(yyscanner), stationId);
}

.|\n    { /* Ignore other */ }

%%
