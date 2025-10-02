#include <jansson.h>
#include <libpq-fe.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static PGconn *conn = NULL;

bool init_db_conn() {
    const char *dbHost = getenv("DB_HOST");
    const char *dbUser = getenv("DB_USER");
    const char *dbPass = getenv("DB_PASS");
    const char *dbName = getenv("DB_NAME");
    const char *dbPort = getenv("DB_PORT");

    if (!dbHost || !dbPort || !dbName || !dbUser || !dbPass) {
        fprintf(stderr, "Error: mising requeried env vars.\n");
        if (!dbHost)
            fprintf(stderr, "DB_HOST\n");
        if (!dbPort)
            fprintf(stderr, "DB_PORT\n");
        if (!dbName)
            fprintf(stderr, "DB_NAME\n");
        if (!dbUser)
            fprintf(stderr, "DB_USER\n");
        if (!dbPass)
            fprintf(stderr, "DB_PASS\n");
        return false;
    }

    conn = PQsetdbLogin(dbHost, dbPort,
                                NULL, // options
                                NULL, // tty
                                dbName, dbUser, dbPass);

    if (PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "Connection error: %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        return false;
    }

    return true;
}

void close_db_conn() {
    PQfinish(conn);
}

json_t* pgresult_to_json(PGresult *res) {
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) return NULL;

    int nRows = PQntuples(res);
    int nFields = PQnfields(res);

    json_t *jsonArray = json_array();
    if (!jsonArray) return NULL;

    for (int i = 0; i < nRows; i++) {
        json_t *jsonObj = json_object();
        if (!jsonObj) {
            json_decref(jsonArray);
            return NULL;
        }

        for (int j = 0; j < nFields; j++) {
            const char *colName = PQfname(res, j);
            char *value = PQgetvalue(res, i, j);

            if (PQgetisnull(res, i, j)) {
                if (json_object_set_new(jsonObj, colName, json_null()) != 0) {
                    json_decref(jsonObj);
                    json_decref(jsonArray);
                    return NULL;
                }
            } else {
                Oid colType = PQftype(res, j);
                json_t *jsonVal = NULL;

                switch(colType) {
                    case 16: { // BOOL
                        bool boolVal = (strcmp(value, "t") == 0);
                        jsonVal = json_boolean(boolVal);
                        break;
                    }
                    case 20: // INT8
                    case 21: // INT2
                    case 23: { // INT4
                        long long intVal = atoll(value);
                        jsonVal = json_integer(intVal);
                        break;
                    }
                    case 700: // FLOAT4
                    case 701: { // FLOAT8
                        double floatVal = atof(value);
                        jsonVal = json_real(floatVal);
                        break;
                    }
                    default:
                        jsonVal = json_string(value);
                        break;
                }

                if (!jsonVal || json_object_set_new(jsonObj, colName, jsonVal) != 0) {
                    if (jsonVal) json_decref(jsonVal);
                    json_decref(jsonObj);
                    json_decref(jsonArray);
                    return NULL;
                }
            }
        }

        if (json_array_append_new(jsonArray, jsonObj) != 0) {
            json_decref(jsonObj);
            json_decref(jsonArray);
            return NULL;
        }
    }

    return jsonArray;
}
