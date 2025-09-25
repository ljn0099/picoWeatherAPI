#include "database.h"
#include <libpq-fe.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))
#define FIND_BY_BODY(TYPE, TABLE, COLUMNS, outItems, count, where, extra, params, nParams)         \
    do {                                                                                           \
        if (!(outItems) || !(count))                                                               \
            return DB_NULL_PARAMS;                                                                 \
        return find_records_by(conn, flags, (void **)(outItems), count, TABLE, COLUMNS,            \
                               ARRAY_LEN(COLUMNS), sizeof(TYPE), where, extra, params, nParams);   \
    } while (0)

const colMap_t userColumns[] = {
    COL(USER_FIELD_ID, "user_id", user_t, userId, COL_TYPE_INT),
    COL(USER_FIELD_UUID, "uuid", user_t, userUUID, COL_TYPE_STRING),
    COL(USER_FIELD_USERNAME, "username", user_t, username, COL_TYPE_STRING),
    COL(USER_FIELD_EMAIL, "email", user_t, email, COL_TYPE_STRING),
    COL(USER_FIELD_PASSWORD, "password", user_t, password, COL_TYPE_STRING),
    COL(USER_FIELD_CREATED_AT, "created_at", user_t, createdAt, COL_TYPE_STRING),
    COL(USER_FIELD_MAX_STATIONS, "max_stations", user_t, maxStations, COL_TYPE_INT),
    COL(USER_FIELD_IS_ADMIN, "is_admin", user_t, isAdmin, COL_TYPE_BOOL),
    COL(USER_FIELD_DELETED_AT, "deleted_at", user_t, deletedAt, COL_TYPE_STRING)
};

const colMap_t sessionColumns[] = {
    COL(SESSION_FIELD_ID, "session_id", session_t, sessionId, COL_TYPE_INT),
    COL(SESSION_FIELD_USER_ID, "user_id", session_t, userId, COL_TYPE_INT),
    COL(SESSION_FIELD_TOKEN, "session_token", session_t, sessionToken, COL_TYPE_STRING),
    COL(SESSION_FIELD_CREATED_AT, "created_at", session_t, createdAt, COL_TYPE_STRING),
    COL(SESSION_FIELD_LAST_SEEN_AT, "last_seen_at", session_t, lastSeenAt, COL_TYPE_STRING),
    COL(SESSION_FIELD_EXPIRES_AT, "expires_at", session_t, expiresAt, COL_TYPE_STRING),
    COL(SESSION_FIELD_REAUTH_AT, "reauth_at", session_t, reauthAt, COL_TYPE_STRING),
    COL(SESSION_FIELD_IP_ADDRESS, "ip_address", session_t, ipAddress, COL_TYPE_STRING),
    COL(SESSION_FIELD_USER_AGENT, "user_agent", session_t, userAgent, COL_TYPE_STRING),
    COL(SESSION_FIELD_REVOKED_AT, "revoked_at", session_t, revokedAt, COL_TYPE_STRING)
};

const colMap_t apiKeyColumns[] = {
    COL(API_KEY_FIELD_ID, "key_id", apiKey_t, keyId, COL_TYPE_INT),
    COL(API_KEY_FIELD_USER_ID, "user_id", apiKey_t, userId, COL_TYPE_INT),
    COL(API_KEY_FIELD_NAME, "name", apiKey_t, name, COL_TYPE_STRING),
    COL(API_KEY_FIELD_API_KEY, "api_key", apiKey_t, apiKey, COL_TYPE_STRING),
    COL(API_KEY_FIELD_KEY_TYPE, "api_key_type", apiKey_t, keyType, COL_TYPE_STRING),
    COL(API_KEY_FIELD_CREATED_AT, "created_at", apiKey_t, createdAt, COL_TYPE_STRING),
    COL(API_KEY_FIELD_EXPIRES_AT, "expires_at", apiKey_t, expiresAt, COL_TYPE_STRING),
    COL(API_KEY_FIELD_REVOKED_AT, "revoked_at", apiKey_t, revokedAt, COL_TYPE_STRING)
};

PGconn *init_db_conn() {
    const char *dbHost = getenv("DB_HOST");
    const char *dbUser = getenv("DB_USER");
    const char *dbPass = getenv("DB_PASS");
    const char *dbName = getenv("DB_NAME");
    const char *dbPort = getenv("DB_PORT");

    if (!dbHost || !dbPort || !dbName || !dbUser || !dbPass) {
        fprintf(stderr, "Error: mising requeried env vars.\n");
        if (!dbHost) fprintf(stderr, "DB_HOST\n");
        if (!dbPort) fprintf(stderr, "DB_PORT\n");
        if (!dbName) fprintf(stderr, "DB_NAME\n");
        if (!dbUser) fprintf(stderr, "DB_USER\n");
        if (!dbPass) fprintf(stderr, "DB_PASS\n");
        exit(EXIT_FAILURE);
    }

    PGconn *conn = PQsetdbLogin(dbHost, dbPort,
                                NULL, // options
                                NULL, // tty
                                dbName, dbUser, dbPass);

    if (PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "Connection error: %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        return NULL;
    }

    return conn;
}

void append_column_names(char *query, size_t queryLen, int flags, const colMap_t *columns,
                                size_t nColumns) {
    for (size_t i = 0; i < nColumns; i++) {
        if (flags & columns[i].flag) {
            strncat(query, columns[i].colName, queryLen - strlen(query) - 1);
            strncat(query, ", ", queryLen - strlen(query) - 1);
        }
    }
    query[strlen(query) - 2] = '\0';
}

void fill_struct_from_db(void *outStruct, PGresult *res, int flags, const colMap_t *columns,
                                size_t nColumns, int row) {
    int colIndex = 0;
    for (size_t i = 0; i < nColumns; i++) {
        if (!(flags & columns[i].flag))
            continue;

        void *field_ptr = (char *)outStruct + columns[i].offset;

        switch (columns[i].type) {
            case COL_TYPE_STRING:
                snprintf((char *)field_ptr, columns[i].size, "%s", PQgetvalue(res, row, colIndex));
                break;
            case COL_TYPE_INT:
                if (PQgetisnull(res, row, colIndex))
                    *(int *)field_ptr = INT_NULL;
                else
                    *(int *)field_ptr = atoi(PQgetvalue(res, row, colIndex));
                break;
            case COL_TYPE_FLOAT:
                if (PQgetisnull(res, row, colIndex))
                    *(float *)field_ptr = FLOAT_NULL;
                else
                    *(float *)field_ptr = atof(PQgetvalue(res, row, colIndex));
                break;
            case COL_TYPE_BOOL:
                if (strcmp(PQgetvalue(res, row, colIndex), "t") == 0)
                    *(bool *)field_ptr = true;
                else
                    *(bool *)field_ptr = false;
                break;
        }
        colIndex++;
    }
}

int append_insert_placeholders(char *query, size_t queryLen, int flags,
                                      const colMap_t *columns, size_t nColumns,
                                      const void *inStruct, const char *paramValues[],
                                      int *paramCount, bool *paramIsStrdup) {
    strncat(query, "(", queryLen - strlen(query) - 1);

    for (size_t i = 0; i < nColumns; i++) {
        if (!(flags & columns[i].flag))
            continue;

        (*paramCount)++;
        char placeholder[INT_SIZE];
        snprintf(placeholder, sizeof(placeholder), "$%d", *paramCount);
        strncat(query, placeholder, queryLen - strlen(query) - 1);
        strncat(query, ", ", queryLen - strlen(query) - 1);

        void *field_ptr = (char *)inStruct + columns[i].offset;

        switch (columns[i].type) {
            case COL_TYPE_INT:
                if (*(int *)field_ptr == INT_NULL)
                    paramValues[*paramCount - 1] = NULL;
                else {
                    char buf[INT_SIZE];
                    snprintf(buf, sizeof(buf), "%d", *(int *)field_ptr);
                    paramValues[*paramCount - 1] = strdup(buf);
                    paramIsStrdup[*paramCount - 1] = true;
                }
                break;

            case COL_TYPE_STRING:
                if (((char *)field_ptr)[0] == '\0')
                    paramValues[*paramCount - 1] = NULL;
                else
                    paramValues[*paramCount - 1] = (char *)field_ptr;
                break;

            case COL_TYPE_FLOAT:
                if (*(float *)field_ptr == FLOAT_NULL)
                    paramValues[*paramCount - 1] = NULL;
                else {
                    char buf[FLOAT_SIZE];
                    snprintf(buf, sizeof(buf), "%f", *(float *)field_ptr);
                    paramValues[*paramCount - 1] = strdup(buf);
                    paramIsStrdup[*paramCount - 1] = true;
                }
                break;

            case COL_TYPE_BOOL:
                paramValues[*paramCount - 1] = (*(bool *)field_ptr) ? "true" : "false";
                break;
        }
    }

    query[strlen(query) - 2] = ')';
    query[strlen(query) - 1] = '\0';

    return 0;
}

int append_update_placeholders(char *query, size_t queryLen, int flags,
                                      const colMap_t *columns, size_t nColumns,
                                      const void *inStruct, const char *paramValues[],
                                      int *paramCount) {
    for (size_t i = 0; i < nColumns; i++) {
        if (!(flags & columns[i].flag))
            continue;

        (*paramCount)++;
        char placeholder[INT_SIZE];
        snprintf(placeholder, sizeof(placeholder), "$%d", *paramCount);

        size_t used = strlen(query);
        snprintf(query + used, queryLen - used, "%s = %s, ", columns[i].colName, placeholder);

        void *field_ptr = (char *)inStruct + columns[i].offset;

        switch (columns[i].type) {
            case COL_TYPE_INT:
                if (*(int *)field_ptr == INT_NULL)
                    paramValues[*paramCount - 1] = NULL;
                else {
                    static char buf[INT_SIZE];
                    snprintf(buf, sizeof(buf), "%d", *(int *)field_ptr);
                    paramValues[*paramCount - 1] = strdup(buf);
                }
                break;

            case COL_TYPE_STRING:
                if (((char *)field_ptr)[0] == '\0')
                    paramValues[*paramCount - 1] = NULL;
                else
                    paramValues[*paramCount - 1] = (char *)field_ptr;
                break;

            case COL_TYPE_FLOAT:
                if (*(float *)field_ptr == FLOAT_NULL)
                    paramValues[*paramCount - 1] = NULL;
                else {
                    static char buf[FLOAT_SIZE];
                    snprintf(buf, sizeof(buf), "%f", *(float *)field_ptr);
                    paramValues[*paramCount - 1] = strdup(buf);
                }
                break;

            case COL_TYPE_BOOL:
                paramValues[*paramCount - 1] = (*(bool *)field_ptr) ? "true" : "false";
                break;
        }
    }

    size_t len = strlen(query);
    query[len - 2] = '\0';

    return 0;
}

void *fill_struct_array_from_db(PGresult *res, int flags, const colMap_t *columns,
                                       size_t nColumns, size_t structSize, int *outCount) {
    int nRows = PQntuples(res);
    *outCount = nRows;

    if (nRows == 0)
        return NULL;

    void *array = calloc(nRows, structSize);
    if (!array)
        return NULL;

    for (int i = 0; i < nRows; i++) {
        char *structPtr = (char *)array + i * structSize;

        fill_struct_from_db(structPtr, res, flags, columns, nColumns, i);
    }

    return array;
}

dbError_t find_records_by(PGconn *conn, int flags, void **outRecords, int *recordCount,
                          const char *tableName, const colMap_t *columns, int nColumns,
                          size_t recordSize, const char *where, const char *extra,
                          const char **params, int nParams) {
    if (!conn || !outRecords || !recordCount || !tableName || !columns)
        return DB_NULL_PARAMS;

    char query[QUERY_SIZE];
    strncpy(query, "SELECT ", sizeof(query));
    query[sizeof(query) - 1] = '\0';

    append_column_names(query, sizeof(query), flags, columns, nColumns);
    strncat(query, " FROM ", sizeof(query) - strlen(query) - 1);
    strncat(query, tableName, sizeof(query) - strlen(query) - 1);

    if (where && where[0] != '\0') {
        strncat(query, " WHERE ", sizeof(query) - strlen(query) - 1);
        strncat(query, where, sizeof(query) - strlen(query) - 1);
    }

    if (extra && extra[0] != '\0') {
        strncat(query, " ", sizeof(query) - strlen(query) - 1);
        strncat(query, extra, sizeof(query) - strlen(query) - 1);
    }
    strncat(query, ";", sizeof(query) - strlen(query) - 1);

    PGresult *res = PQexecParams(conn, query, nParams, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Error in query: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return DB_QUERY_ERROR;
    }

    if (PQntuples(res) == 0) {
        PQclear(res);
        return DB_NO_ROWS;
    }

    *outRecords = fill_struct_array_from_db(res, flags, columns, nColumns, recordSize, recordCount);

    PQclear(res);
    return DB_OK;
}

dbError_t insert_record(PGconn *conn, int flags, const char *tableName, const colMap_t *columns,
                        int nColumns, const void *record) {
    if (!conn || !tableName || !columns || !record)
        return DB_NULL_PARAMS;

    char query[QUERY_SIZE];
    strncpy(query, "INSERT INTO ", sizeof(query) - 1);
    query[sizeof(query) - 1] = '\0';
    strncat(query, tableName, sizeof(query) - strlen(query) - 1);
    strncat(query, " (", sizeof(query) - strlen(query) - 1);

    // Agregar nombres de columnas según flags
    append_column_names(query, sizeof(query), flags, columns, nColumns);
    strncat(query, ") VALUES ", sizeof(query) - strlen(query) - 1);

    // Preparar placeholders y valores
    const char *paramValues[MAX_FIELDS];
    bool paramIsStrdup[MAX_FIELDS] = {0};
    int paramCount = 0;
    append_insert_placeholders(query, sizeof(query), flags, columns, nColumns, record, paramValues,
                               &paramCount, paramIsStrdup);

    // Ejecutar la query
    PGresult *res = PQexecParams(conn, query, paramCount,
                                 NULL,        // tipos (inferencia automática)
                                 paramValues, // valores
                                 NULL, NULL,  // lengths y formats
                                 0);          // texto

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Error insert: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return DB_QUERY_ERROR;
    }

    PQclear(res);

    for (int i = 0; i < paramCount; i++) {
        if (paramIsStrdup[i] && paramValues[i] != NULL)
            free((char *)paramValues[i]);
    }

    return DB_OK;
}

dbError_t find_user_by(PGconn *conn, int flags, user_t **outUsers, int *userCount,
                       const char *where, const char *extra, const char **params, int nParams) {
    FIND_BY_BODY(user_t, "auth.users", userColumns, outUsers, userCount, where, extra, params,
                 nParams);
}

dbError_t find_session_by(PGconn *conn, int flags, session_t **outSessions, int *sessionCount,
                          const char *where, const char *extra, const char **params, int nParams) {
    FIND_BY_BODY(session_t, "auth.user_sessions", sessionColumns, outSessions, sessionCount, where,
                 extra, params, nParams);
}

dbError_t find_api_key_by(PGconn *conn, int flags, apiKey_t **outApiKeys, int *apiKeysCount,
                          const char *where, const char *extra, const char **params, int nParams) {
    FIND_BY_BODY(apiKey_t, "auth.api_keys", apiKeyColumns, outApiKeys, apiKeysCount, where, extra,
                 params, nParams);
}

dbError_t insert_user(PGconn *conn, int flags, user_t *user) {
    return insert_record(conn, flags, "auth.users", userColumns, ARRAY_LEN(userColumns), user);
}

dbError_t insert_session(PGconn *conn, int flags, session_t *session) {
    return insert_record(conn, flags, "auth.user_sessions", sessionColumns,
                         ARRAY_LEN(sessionColumns), session);
}

dbError_t insert_api_key(PGconn *conn, int flags, apiKey_t *apiKey) {
    return insert_record(conn, flags, "auth.api_keys", apiKeyColumns, ARRAY_LEN(apiKeyColumns),
                         apiKey);
}

// int main() {
//     PGconn *conn = init_db_conn();
//     if (!conn) {
//         fprintf(stderr, "No se pudo conectar a la base de datos.\n");
//         return -1;
//     }

//     // Crear un nuevo usuario
//     user_t new_user;
//     memset(&new_user, 0, sizeof(new_user));

//     strncpy(new_user.username, "usuario_nuevo", sizeof(new_user.username));
//     strncpy(new_user.email, "nuevo@ejemplo.com", sizeof(new_user.email));
//     strncpy(new_user.password, "passwordtest", sizeof(new_user.password));
//     new_user.maxStations = 3;
//     new_user.isAdmin = false;

//     int insert_fields = USER_FIELD_USERNAME | USER_FIELD_EMAIL | USER_FIELD_PASSWORD |
//                         USER_FIELD_MAX_STATIONS | USER_FIELD_IS_ADMIN;

//     if (insert_user(conn, insert_fields, &new_user) != DB_OK) {
//         fprintf(stderr, "Error al insertar el nuevo usuario\n");
//         PQfinish(conn);
//         return -1;
//     }

//     printf("Nuevo usuario insertado correctamente.\n");

//     PQfinish(conn);
//     return 0;
// }
