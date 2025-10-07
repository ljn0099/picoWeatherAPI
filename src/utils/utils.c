#define _XOPEN_SOURCE 700
#include "utils.h"
#include <ctype.h>
#include <jansson.h>
#include <libpq-fe.h>
#include <sodium.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

bool validate_name(const char *str) {
    int len = 0;

    if (!str || str[0] == '\0')
        return false;

    for (int i = 0; str[i] != '\0'; i++) {
        char c = str[i];
        if (!(isalnum(c) || c == '-' || c == '_')) {
            return false; // Invalid character
        }
        len++;
        if (len > NAME_SIZE) {
            return false; // Exceeds the lenght limit
        }
    }

    if (len < NAME_SIZE_MIN) // Name too short
        return false;

    return true;
}

bool validate_uuid(const char *uuid) {
    if (!uuid || uuid[0] == '\0')
        return false;

    // It should have exactly 36 chars
    for (int i = 0; i < 36; i++) {
        char c = uuid[i];
        // Dash positions
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (c != '-')
                return false;
        }
        else {
            // The rest should be hexadecimal
            if (!isxdigit(c))
                return false;
        }
    }

    // The last character should be the end of the string
    return uuid[36] == '\0';
}

bool validate_password(PGconn *conn, const char *userId, const char *password) {
    if (!password || !userId)
        return false;

    const char *paramValues[1] = {userId};

    PGresult *res = PQexecParams(conn,
                                 "SELECT password "
                                 " FROM auth.users "
                                 " WHERE uuid::text = $1"
                                 " OR username = $1",
                                 1, NULL, paramValues, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Error executing the query: %s", PQerrorMessage(conn));
        PQclear(res);
        return false;
    }

    if (PQntuples(res) != 1) {
        PQclear(res);
        return false;
    }

    const char *passHash = PQgetvalue(res, 0, 0);
    if (crypto_pwhash_str_verify(passHash, password, strlen(password)) != 0) {
        PQclear(res);
        return false;
    }

    PQclear(res);
    return true;
}

bool validate_session_token(PGconn *conn, const char *userId, const char *sessionToken) {
    if (!sessionToken)
        return false;

    unsigned char recievedToken[KEY_ENTROPY];
    if (sodium_base642bin(recievedToken, sizeof(recievedToken), sessionToken, strlen(sessionToken),
                          NULL, NULL, NULL, BASE64_VARIANT) != 0) {
        return false;
    }

    unsigned char recievedTokenHash[crypto_generichash_BYTES];
    crypto_generichash(recievedTokenHash, sizeof(recievedTokenHash), recievedToken,
                       sizeof(recievedToken), NULL, 0);

    // Convert the hash into base64 for the query
    char recievedTokenHashB64[sodium_base64_ENCODED_LEN((sizeof(recievedTokenHash)),
                                                        BASE64_VARIANT)];
    sodium_bin2base64(recievedTokenHashB64, sizeof(recievedTokenHashB64), recievedTokenHash,
                      (sizeof(recievedTokenHash)), BASE64_VARIANT);

    PGresult *res;

    const char *paramValues[2] = {recievedTokenHashB64, userId};

    res = PQexecParams(conn,
                       "SELECT 1 "
                       "FROM auth.user_sessions s "
                       "JOIN auth.users u ON s.user_id = u.user_id "
                       "WHERE s.session_token = $1 "
                       "  AND s.expires_at > NOW() "
                       "  AND s.revoked_at IS NULL "
                       "  AND u.deleted_at IS NULL "
                       "  AND ( "
                       "        ($2::text IS NULL AND u.is_admin = true) "
                       "        OR ($2::text IS NOT NULL AND ( "
                       "              u.is_admin = true "
                       "              OR u.uuid::text = $2::text "
                       "              OR u.username = $2::text "
                       "        )) "
                       "      )",
                       2,           // number of parameters
                       NULL,        // param types
                       paramValues, // param values
                       NULL,        // param lengths
                       NULL,        // param formats
                       0);          // result format (0 = text)

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

bool get_user_session_token(PGconn *conn, char **userId, const char *sessionToken) {
    if (!sessionToken || !userId)
        return false;

    unsigned char recievedToken[KEY_ENTROPY];
    if (sodium_base642bin(recievedToken, sizeof(recievedToken), sessionToken, strlen(sessionToken),
                          NULL, NULL, NULL, BASE64_VARIANT) != 0) {
        return false;
    }

    unsigned char recievedTokenHash[crypto_generichash_BYTES];
    crypto_generichash(recievedTokenHash, sizeof(recievedTokenHash), recievedToken,
                       sizeof(recievedToken), NULL, 0);

    // Convert the hash into base64 for the query
    char recievedTokenHashB64[sodium_base64_ENCODED_LEN((sizeof(recievedTokenHash)),
                                                        BASE64_VARIANT)];
    sodium_bin2base64(recievedTokenHashB64, sizeof(recievedTokenHashB64), recievedTokenHash,
                      (sizeof(recievedTokenHash)), BASE64_VARIANT);

    const char *paramValues[1] = {recievedTokenHashB64};

    PGresult *res = PQexecParams(conn,
                                 "SELECT u.uuid AS user_uuid "
                                 "FROM auth.user_sessions s "
                                 "JOIN auth.users u ON s.user_id = u.user_id "
                                 "WHERE s.session_token = $1",
                                 1, NULL, paramValues, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Error executing query: %s\n", PQerrorMessage(conn));
        PQclear(res);
        return false;
    }

    if (PQntuples(res) > 0) {
        *userId = strdup(PQgetvalue(res, 0, 0));
    }
    else {
        PQclear(res);
        return false;
    }

    PQclear(res);
    return true;
}

void generateSessionToken(char *tokenB64, size_t tokenB64Len, char *hashB64, size_t hashB64Len) {
    unsigned char sessionToken[KEY_ENTROPY];

    // Generate random token
    randombytes_buf(sessionToken, sizeof(sessionToken));

    // Convert token to base64
    sodium_bin2base64(tokenB64, tokenB64Len, sessionToken, sizeof(sessionToken), BASE64_VARIANT);

    // Hash the token
    unsigned char sessionTokenHash[crypto_generichash_BYTES];
    crypto_generichash(sessionTokenHash, sizeof(sessionTokenHash), sessionToken,
                       sizeof(sessionToken), NULL, 0);

    // Convert hash to base64
    sodium_bin2base64(hashB64, hashB64Len, sessionTokenHash, sizeof(sessionTokenHash),
                      BASE64_VARIANT);
}

json_t *pgresult_to_json(PGresult *res, bool canBeObject) {
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK)
        return NULL;

    int nRows = PQntuples(res);
    int nFields = PQnfields(res);

    if (nRows == 0)
        return json_array();

    json_t *jsonArray = json_array();
    if (!jsonArray)
        return NULL;

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
            }
            else {
                Oid colType = PQftype(res, j);
                json_t *jsonVal = NULL;

                switch (colType) {
                    case 16: { // BOOL
                        bool boolVal = (strcmp(value, "t") == 0);
                        jsonVal = json_boolean(boolVal);
                        break;
                    }
                    case 20:
                    case 21:
                    case 23: { // INT8, INT2, INT4
                        long long intVal = atoll(value);
                        jsonVal = json_integer(intVal);
                        break;
                    }
                    case 700:
                    case 701: { // FLOAT4, FLOAT8
                        double floatVal = atof(value);
                        jsonVal = json_real(floatVal);
                        break;
                    }
                    default:
                        jsonVal = json_string(value);
                        break;
                }

                if (!jsonVal || json_object_set_new(jsonObj, colName, jsonVal) != 0) {
                    if (jsonVal)
                        json_decref(jsonVal);
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

    if (nRows == 1 && canBeObject) {
        json_t *singleObj = json_array_get(jsonArray, 0);
        json_incref(singleObj);
        json_decref(jsonArray);
        return singleObj;
    }

    return jsonArray;
}

bool validate_timestamp(const char *timestamp) {
    if (!timestamp)
        return false;

    struct tm tm;
    const char *result = strptime(timestamp, "%Y-%m-%d %H:%M:%S", &tm);
    if (result == NULL || *result != '\0')
        return false; // Invalid format

    return true;
}

bool validate_email(const char *email) {
    if (email == NULL) return false;

    const char *at = strchr(email, '@');
    if (!at) return false;                     // Must contain '@'
    if (at == email) return false;             // Cannot start with '@'

    const char *dot = strrchr(at, '.');
    if (!dot) return false;                    // Must contain at least one '.'
    if (dot < at + 2) return false;           // Must have at least one character between '@' and '.'
    if (*(dot + 1) == '\0') return false;     // Cannot end with '.'

    // Check allowed characters before '@'
    for (const char *p = email; p < at; p++) {
        if (!(isalnum(*p) || *p == '.' || *p == '_' || *p == '-' || *p == '+'))
            return false;
    }

    // Check allowed characters between '@' and last '.'
    for (const char *p = at + 1; *p && p < dot; p++) {
        if (!(isalnum(*p) || *p == '.' || *p == '-'))
            return false;
    }

    // Check characters after the last '.' (TLD)
    for (const char *p = dot + 1; *p; p++) {
        if (!isalpha(*p))
            return false;                     // Only letters allowed in TLD
    }

    return true;
}
