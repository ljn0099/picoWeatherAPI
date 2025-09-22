#include "../db/database.h"
#include <libpq-fe.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static PGconn *conn;

dbError_t weather_init_db() {
    conn = init_db_conn();
    if (!conn)
        return DB_CONN_ERROR;

    return DB_OK;
}

dbError_t users_list_all(user_t *users[], int *userCount) {
    int flags = USER_FIELD_ID | USER_FIELD_EMAIL | USER_FIELD_USERNAME | USER_FIELD_IS_ADMIN |
                USER_FIELD_CREATED_AT | USER_FIELD_DELETED_AT | USER_FIELD_MAX_STATIONS;

    return find_user_by(conn, flags, users, userCount, "deleted_at IS NULL ", NULL, NULL, 0);
}

void weather_close_db() {
    PQfinish(conn);
}

void users_free(user_t *users) {
    free(users);
}
