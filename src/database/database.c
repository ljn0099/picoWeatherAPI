#include <libpq-fe.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>

#define MAX_CONN 10

typedef struct {
    PGconn *conn;
    int busy;
} ConnWrapper;

ConnWrapper pool[MAX_CONN];
pthread_mutex_t poolMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t poolCond = PTHREAD_COND_INITIALIZER;

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

PGconn *get_conn() {
    PGconn *ret = NULL;

    pthread_mutex_lock(&poolMutex);

    // Wait for a free connection
    while (1) {
        for (int i = 0; i < MAX_CONN; i++) {
            if (pool[i].busy == 0) {
                pool[i].busy = 1; // Mark connection as busy
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
            pool[i].busy = 0;               // Mark connection as free
            pthread_cond_signal(&poolCond); // Awake a waiting thread
            break;
        }
    }
    pthread_mutex_unlock(&poolMutex);
}
