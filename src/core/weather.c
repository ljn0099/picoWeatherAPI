#include "../db/database.h"
#include "weather.h"
#include <libpq-fe.h>
#include <regex.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static PGconn *conn;

bool is_uuid(const char *str) {
    regex_t regex;
    int reti;

    reti = regcomp(&regex,
                   "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$",
                   REG_EXTENDED | REG_NOSUB);
    if (reti)
        return false;

    reti = regexec(&regex, str, 0, NULL, 0);
    regfree(&regex);

    return (reti == 0);
}

dbError_t weather_init_db() {
    conn = init_db_conn();
    if (!conn)
        return DB_CONN_ERROR;

    return DB_OK;
}

dbError_t users_list(user_t *users[], int *userCount, const char *sessionCookie,
                     const char *userId) {

    if (!validate_cookie(sessionCookie, userId))
        return DB_AUTH_ERROR;

    int flags = USER_FIELD_UUID | USER_FIELD_EMAIL | USER_FIELD_USERNAME | USER_FIELD_IS_ADMIN |
                USER_FIELD_CREATED_AT | USER_FIELD_DELETED_AT | USER_FIELD_MAX_STATIONS;

    if (!userId) {
        return find_user_by(conn, flags, users, userCount, "deleted_at IS NULL ", NULL, NULL, 0);
    }
    else {
        const char *paramValues[1] = {userId};
        if (is_uuid(userId)) {
            return find_user_by(conn, flags, users, userCount, "deleted_at IS NULL AND uuid = $1",
                                NULL, paramValues, 1);
        }
        else {
            return find_user_by(conn, flags, users, userCount,
                                "deleted_at IS NULL AND username = $1", NULL, paramValues, 1);
        }
    }
}

dbError_t user_delete(const char *sessionCookie, const char *userId) {
    if (!validate_cookie(sessionCookie, userId))
        return DB_AUTH_ERROR;

    const char *paramValues[1] = {userId};

    PGresult *res =
        PQexecParams(conn,
                     "UPDATE auth.users "
                     " SET deleted_at = now() "
                     " WHERE uuid::text = $1 OR username = $1;",
                     1, // number of parameters
                     NULL, paramValues, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Error executing the query: %s", PQerrorMessage(conn));
        PQclear(res);
        return DB_CONN_ERROR;
    }

    PQclear(res);

    return DB_OK;
}

bool validate_cookie(const char *sessionCookie, const char *userId) {
    if (!sessionCookie)
        return false;

    PGresult *res;

    if (!userId) {
        const char *paramValues[1] = {sessionCookie};

        res = PQexecParams(conn,
                           "SELECT 1 "
                           "FROM auth.user_sessions s "
                           "JOIN auth.users u ON s.user_id = u.user_id "
                           "WHERE s.session_token = $1 "
                           "  AND s.expires_at > NOW() "
                           "  AND s.revoked_at IS NULL "
                           "  AND u.is_admin = true "
                           "  AND u.deleted_at IS NULL",
                           1, // number of parameters
                           NULL, paramValues, NULL, NULL, 0);
    }
    else {
        const char *paramValues[2] = {sessionCookie, userId};

        res = PQexecParams(conn,
                           "SELECT 1 "
                           "FROM auth.user_sessions s "
                           "JOIN auth.users u ON s.user_id = u.user_id "
                           "WHERE s.session_token = $1 "
                           "  AND s.expires_at > NOW() "
                           "  AND s.revoked_at IS NULL "
                           "  AND u.deleted_at IS NULL "
                           "  AND (u.is_admin = true OR u.uuid::text = $2 OR u.username = $2)",
                           2, // number of parameters
                           NULL, paramValues, NULL, NULL, 0);
    }

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Error executing the query: %s", PQerrorMessage(conn));
        PQclear(res);
        return false;
    }

    if (PQntuples(res) > 0) {
        PQclear(res);
        return true;
    }
    else {
        PQclear(res);
        return false;
    }
}

void weather_close_db() {
    PQfinish(conn);
}

void users_free(user_t *users) {
    free(users);
}
