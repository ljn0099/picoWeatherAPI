#include "../database/database.h"
#include "../utils/utils.h"
#include "weather.h"
#include <jansson.h>
#include <libpq-fe.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

apiError_t users_list(const char *userId, const char *sessionToken, json_t **users) {
    if (!sessionToken || !users)
        return API_AUTH_ERROR;

    PGconn *conn = get_conn();
    if (!conn)
        return API_DB_ERROR;

    if (!validate_session_token(conn, userId, sessionToken)) {
        release_conn(conn);
        return API_AUTH_ERROR;
    }

    const char *paramValues[1] = {userId};

    PGresult *res = PQexecParams(
        conn,
        "SELECT uuid, username, email, created_at, max_stations, is_admin FROM auth.users "
        "WHERE deleted_at IS NULL "
        "AND ($1::text IS NULL OR uuid::text = $1::text OR username = $1::text);",
        1, NULL, paramValues, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Error executing the query: %s", PQerrorMessage(conn));
        PQclear(res);
        release_conn(conn);
        return API_DB_ERROR;
    }

    if (PQntuples(res) == 0) {
        PQclear(res);
        release_conn(conn);
        return API_NOT_FOUND;
    }

    *users = pgresult_to_json(res);
    if (!*users) {
        PQclear(res);
        release_conn(conn);
        return API_JSON_ERROR;
    }

    PQclear(res);

    release_conn(conn);

    return API_OK;
}

apiError_t users_create(const char *username, const char *email, const char *password,
                        json_t **user) {
    if (!username || !email || !password)
        return API_INVALID_PARAMS;

    char hashedPassword[crypto_pwhash_STRBYTES];

    if (crypto_pwhash_str(hashedPassword, password, strlen(password),
                          crypto_pwhash_OPSLIMIT_MODERATE, crypto_pwhash_MEMLIMIT_MODERATE) != 0) {
        return API_MEMORY_ERROR;
    }

    PGconn *conn = get_conn();
    if (!conn)
        return API_DB_ERROR;

    PGresult *res = NULL;

    const char *paramValues[3] = {username, email, hashedPassword};

    res = PQexecParams(conn,
                       "INSERT INTO auth.users (username, email, password) "
                       "VALUES ($1, $2, $3);",
                       3, NULL, paramValues, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Error executing the query: %s", PQerrorMessage(conn));
        PQclear(res);
        release_conn(conn);
        return API_DB_ERROR;
    }

    PQclear(res);

    res = PQexecParams(
        conn,
        "SELECT uuid, username, email, created_at, max_stations, is_admin FROM auth.users "
        "WHERE username = $1;",
        1, NULL, paramValues, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Error executing the query: %s", PQerrorMessage(conn));
        PQclear(res);
        release_conn(conn);
        return API_DB_ERROR;
    }

    if (PQntuples(res) == 0) {
        PQclear(res);
        release_conn(conn);
        return API_NOT_FOUND;
    }

    *user = pgresult_to_json(res);
    if (!*user) {
        PQclear(res);
        release_conn(conn);
        return API_JSON_ERROR;
    }

    PQclear(res);

    release_conn(conn);

    return API_OK;
}

apiError_t users_delete(const char *userId, const char *sessionToken) {
    if (!sessionToken)
        return API_AUTH_ERROR;

    PGconn *conn = get_conn();
    if (!conn)
        return API_DB_ERROR;

    if (!validate_session_token(conn, userId, sessionToken)) {
        release_conn(conn);
        return API_AUTH_ERROR;
    }

    const char *paramValues[1] = {userId};

    PGresult *res = PQexecParams(conn,
                                 "UPDATE auth.users "
                                 " SET deleted_at = now() "
                                 " WHERE (uuid::text = $1 OR username = $1) "
                                 " AND deleted_at IS NULL;",
                                 1, // number of parameters
                                 NULL, paramValues, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Error executing the query: %s", PQerrorMessage(conn));
        PQclear(res);
        release_conn(conn);
        return API_DB_ERROR;
    }

    PQclear(res);

    release_conn(conn);

    return API_OK;
}

apiError_t sessions_create(const char *userId, const char *password, char *sessionToken,
                           size_t sessionTokenLen, int sessionTokenMaxAge) {
    if (!userId || !password || !sessionToken)
        return API_AUTH_ERROR;

    PGconn *conn = get_conn();
    if (!conn)
        return API_DB_ERROR;

    if (!validate_password(conn, userId, password))
        return API_AUTH_ERROR;

    char hashB64[sodium_base64_ENCODED_LEN(crypto_generichash_BYTES, BASE64_VARIANT)];

    generateSessionToken(sessionToken, sessionTokenLen, hashB64, sizeof(hashB64));

    char maxAgeStr[20];
    sprintf(maxAgeStr, "%d", sessionTokenMaxAge);

    const char *paramValues[3] = {hashB64, userId, maxAgeStr};

    PGresult *res = PQexecParams(conn,
                                 "INSERT INTO auth.user_sessions "
                                 "(user_id, session_token, expires_at) "
                                 "SELECT u.user_id, $1, now() + $3 * interval '1 second' "
                                 "FROM auth.users u "
                                 "WHERE u.uuid::text = $2 OR u.username = $2;",
                                 3, NULL, paramValues, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Error executing the query: %s", PQerrorMessage(conn));
        PQclear(res);
        return API_DB_ERROR;
    }

    PQclear(res);

    release_conn(conn);

    return API_OK;
}
