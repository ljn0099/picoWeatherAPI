#include "../utils/utils.h"
#include "weather.h"
#include <jansson.h>
#include <libpq-fe.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

const char *DB_HOST;
const char *DB_USER;
const char *DB_PASS;
const char *DB_NAME;
const char *DB_PORT;

#define MAX_CONN 10

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

// ---- Pool

typedef struct {
    PGconn *conn;
    int busy;
} ConnWrapper;

ConnWrapper pool[MAX_CONN];
pthread_mutex_t poolMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t poolCond = PTHREAD_COND_INITIALIZER;

bool init_pool() {
    for (int i = 0; i < MAX_CONN; i++) {
        pool[i].conn = init_db_conn();
        if (!pool[i].conn)
            return false;
        pool[i].busy = 0;
    }
    return true;
}

void free_pool() {
    for (int i = 0; i < MAX_CONN; i++) {
        PQfinish(pool[i].conn);
    }
}

PGconn* get_conn() {
    PGconn *ret = NULL;

    pthread_mutex_lock(&poolMutex);

    // Wait for a free connection
    while (1) {
        for (int i = 0; i < MAX_CONN; i++) {
            if (pool[i].busy == 0) {
                pool[i].busy = 1;   // Mark connection as busy
                ret = pool[i].conn;
                pthread_mutex_unlock(&poolMutex);
                return ret;
            }
        }
        // There aren't any free connection, wait for release_conn to make a signal
        pthread_cond_wait(&poolCond, &poolMutex);
    }
}

void release_conn(PGconn *conn) {
    pthread_mutex_lock(&poolMutex);
    for (int i = 0; i < MAX_CONN; i++) {
        if (pool[i].conn == conn) {
            pool[i].busy = 0; // Mark connection as free
            pthread_cond_signal(&poolCond); // Awake a waiting thread
            break;
        }
    }
    pthread_mutex_unlock(&poolMutex);
}

// ----------------

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
