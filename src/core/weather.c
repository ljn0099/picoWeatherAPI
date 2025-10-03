#include "../utils/utils.h"
#include "../database/database.h"
#include "weather.h"
#include <jansson.h>
#include <libpq-fe.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

apiError_t users_list(const char *userId, const char *sessionToken, json_t **users) {
    if (!sessionToken || !users)
        return API_INVALID_PARAMS;

    PGconn *conn = get_conn();
    if (!conn)
        return API_DB_ERROR;

    if (!validate_session_token(conn, userId, sessionToken)) {
        close_db_conn(conn);
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
        close_db_conn(conn);
        return API_DB_ERROR;
    }

    if (PQntuples(res) == 0) {
        PQclear(res);
        close_db_conn(conn);
        return API_NOT_FOUND;
    }

    *users = pgresult_to_json(res);
    if (!*users) {
        PQclear(res);
        close_db_conn(conn);
        return API_JSON_ERROR;
    }

    PQclear(res);

    release_conn(conn);

    return API_OK;
}
