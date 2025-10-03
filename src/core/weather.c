#include "../utils/utils.h"
#include <jansson.h>
#include <libpq-fe.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

const char *DB_HOST;
const char *DB_USER;
const char *DB_PASS;
const char *DB_NAME;
const char *DB_PORT;

bool init_db_vars() {
    DB_HOST = getenv("DB_HOST");
    DB_USER = getenv("DB_USER");
    DB_PASS = getenv("DB_PASS");
    DB_NAME = getenv("DB_NAME");
    DB_PORT = getenv("DB_PORT");

    if (!DB_HOST || !DB_PORT || !DB_NAME || !DB_USER || !DB_PASS) {
        fprintf(stderr, "Error: mising requeried env vars.\n");
        if (!DB_HOST)
            fprintf(stderr, "DB_HOST\n");
        if (!DB_PORT)
            fprintf(stderr, "DB_PORT\n");
        if (!DB_NAME)
            fprintf(stderr, "DB_NAME\n");
        if (!DB_USER)
            fprintf(stderr, "DB_USER\n");
        if (!DB_PASS)
            fprintf(stderr, "DB_PASS\n");
        return false;
    }
    return true;
}

PGconn *init_db_conn() {

    PGconn *conn;
    conn = PQsetdbLogin(DB_HOST, DB_PORT,
                         NULL, // options
                         NULL, // tty
                         DB_NAME, DB_USER, DB_PASS);

    if (PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "Connection error: %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        return NULL;
    }

    return conn;
}

void close_db_conn(PGconn *conn) {
    PQfinish(conn);
}

bool users_list(const char *userId, const char *sessionToken, json_t **users) {
    if (!sessionToken || !users)
        return false;

    PGconn *conn = init_db_conn();
    if (!conn)
        return false;

    if (!validate_session_token(conn, userId, sessionToken)) {
        close_db_conn(conn);
        return false;
    }

    const char *paramValues[1] = {userId};

    PGresult *res = PQexecParams(conn,
            "SELECT uuid, username, email, created_at, max_stations, is_admin FROM auth.users "
            "WHERE deleted_at IS NULL "
            "AND ($1::text IS NULL OR uuid::text = $1::text OR username = $1::text);",
            1,
            NULL, paramValues, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Error executing the query: %s", PQerrorMessage(conn));
        PQclear(res);
        close_db_conn(conn);
        return false;
    }

    *users = pgresult_to_json(res);
    if (!*users) {
        PQclear(res);
        close_db_conn(conn);
        return false;
    }

    PQclear(res);

    close_db_conn(conn);

    return true;
}
