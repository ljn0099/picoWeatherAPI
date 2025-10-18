#include "../core/weather.h"
#include "../database/database.h"
#include "../http/server.h"
#include "../utils/utils.h"
#include "flags.h"
#include <jansson.h>
#include <libpq-fe.h>
#include <sodium/crypto_generichash.h>
#include <sodium/crypto_pwhash.h>
#include <sodium/utils.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

apiError_t users_list(const char *userId, const struct AuthData *authData, json_t **users) {
    if (!authData || !authData->sessionToken || !users)
        return API_AUTH_ERROR;

    PGconn *conn = get_conn();
    if (!conn)
        return API_DB_ERROR;

    if (!validate_session_token(conn, userId, authData->sessionToken)) {
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

    if (!userId)
        *users = pgresult_to_json(res, false);
    else
        *users = pgresult_to_json(res, true);

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

    if (!validate_name(username))
        return API_INVALID_PARAMS;

    if (!validate_email(email))
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

    *user = pgresult_to_json(res, true);
    if (!*user) {
        PQclear(res);
        release_conn(conn);
        return API_JSON_ERROR;
    }

    PQclear(res);

    release_conn(conn);

    return API_OK;
}

apiError_t users_delete(const char *userId, const struct AuthData *authData) {
    if (!authData || !authData->sessionToken)
        return API_AUTH_ERROR;

    PGconn *conn = get_conn();
    if (!conn)
        return API_DB_ERROR;

    if (!validate_session_token(conn, userId, authData->sessionToken)) {
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

apiError_t users_patch(const char *userId, const char *username, const char *email,
                       const int *maxStations, const bool *isAdmin, const char *oldPass,
                       const char *newPass, const struct AuthData *authData, json_t **user) {
    if (!authData || !authData->sessionToken)
        return API_AUTH_ERROR;

    if (!userId)
        return API_INVALID_PARAMS;

    if (username && !validate_name(username))
        return API_INVALID_PARAMS;

    if (email && !validate_email(email))
        return API_INVALID_PARAMS;

    PGconn *conn = get_conn();
    if (!conn)
        return API_DB_ERROR;

    if (!validate_session_token(conn, userId, authData->sessionToken)) {
        release_conn(conn);
        return API_AUTH_ERROR;
    }

    const char *paramValues[6] = {userId, username, email, NULL, NULL, NULL};

    char hashedPasswordBuf[crypto_pwhash_STRBYTES];
    const char *hashedPassPtr = NULL;
    // Atempt to change password
    if (oldPass || newPass) {
        if (validate_password(conn, userId, oldPass)) {
            // Calculate the password hash
            if (crypto_pwhash_str(hashedPasswordBuf, newPass, strlen(newPass),
                                  crypto_pwhash_OPSLIMIT_MODERATE,
                                  crypto_pwhash_MEMLIMIT_MODERATE) != 0) {
                return API_MEMORY_ERROR;
            }
            hashedPassPtr = hashedPasswordBuf;
        }
        else {
            return API_AUTH_ERROR;
        }
    }
    paramValues[5] = hashedPassPtr;

    char maxStationsBuf[20];
    const char *maxStationsStr = NULL;

    if (validate_admin_session_token(conn, authData->sessionToken)) {
        if (maxStations) {
            snprintf(maxStationsBuf, sizeof(maxStationsBuf), "%d", *maxStations);
            maxStationsStr = maxStationsBuf;
        }

        const char *isAdminStr = NULL;
        if (isAdmin) {
            if (*isAdmin)
                isAdminStr = "true";
            else
                isAdminStr = "false";
        }
        paramValues[3] = maxStationsStr;
        paramValues[4] = isAdminStr;
    }

    PGresult *res;

    res = PQexecParams(
        conn,
        "UPDATE auth.users "
        "SET username = COALESCE($2, username), "
        "    email = COALESCE($3, email), "
        "    max_stations = COALESCE($4, max_stations), "
        "    is_admin = COALESCE($5, is_admin), "
        "    password = COALESCE($6, password) "
        "WHERE uuid::text = $1 OR username = $1 "
        "RETURNING uuid::text, username, email, max_stations, is_admin, created_at, deleted_at;",
        6, NULL, paramValues, NULL, NULL, 0);

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

    *user = pgresult_to_json(res, true);
    if (!*user) {
        PQclear(res);
        release_conn(conn);
        return API_JSON_ERROR;
    }

    PQclear(res);

    // Revoke all active sessions
    res = PQexecParams(
        conn,
        "UPDATE auth.user_sessions "
        "SET revoked_at = NOW() "
        "WHERE user_id = (SELECT user_id FROM auth.users WHERE uuid::text = $1 OR username = $1) "
        "AND revoked_at IS NULL;",
        1, NULL, &userId, NULL, NULL, 0);
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

apiError_t sessions_create(const char *userId, const struct AuthData *authData,
                           const char *password, char *sessionToken, size_t sessionTokenLen,
                           int sessionTokenMaxAge, json_t **session) {
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

    PGresult *res = NULL;

    const char *paramValues[5] = {hashB64, userId, maxAgeStr, authData->clientIp,
                                  authData->userAgent};

    res = PQexecParams(conn,
                       "INSERT INTO auth.user_sessions "
                       "(user_id, session_token, expires_at, ip_address, user_agent) "
                       "SELECT u.user_id, $1, now() + $3 * interval '1 second', $4, $5 "
                       "FROM auth.users u "
                       "WHERE u.uuid::text = $2 OR u.username = $2;",
                       5, NULL, paramValues, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Error executing the query: %s", PQerrorMessage(conn));
        PQclear(res);
        return API_DB_ERROR;
    }

    PQclear(res);

    res = PQexecParams(
        conn,
        "SELECT uuid, created_at, last_seen_at, expires_at, reauth_at, ip_address, user_agent "
        "FROM auth.user_sessions "
        "WHERE session_token = $1",
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

    *session = pgresult_to_json(res, true);
    if (!*session) {
        PQclear(res);
        release_conn(conn);
        return API_JSON_ERROR;
    }

    PQclear(res);

    release_conn(conn);

    return API_OK;
}

apiError_t sessions_list(const char *userId, const char *sessionUUID,
                         const struct AuthData *authData, json_t **sessions) {
    if (!authData || !authData->sessionToken || !userId)
        return API_INVALID_PARAMS;

    PGconn *conn = get_conn();
    if (!conn)
        return API_DB_ERROR;

    if (!validate_session_token(conn, userId, authData->sessionToken))
        return API_AUTH_ERROR;

    const char *paramValues[2] = {userId, sessionUUID};

    PGresult *res = PQexecParams(conn,
                                 "SELECT s.created_at, "
                                 "s.last_seen_at, s.expires_at, s.reauth_at, s.ip_address, "
                                 "s.user_agent, s.uuid "
                                 "FROM auth.user_sessions s "
                                 "JOIN auth.users u ON s.user_id = u.user_id "
                                 "WHERE s.expires_at > NOW() "
                                 "  AND s.revoked_at IS NULL "
                                 "  AND (u.uuid::text = $1::text OR u.username = $1::text) "
                                 "  AND ($2::text IS NULL OR s.uuid::text = $2::text)",
                                 2, NULL, paramValues, NULL, NULL, 0);

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

    if (!sessionUUID)
        *sessions = pgresult_to_json(res, false);
    else
        *sessions = pgresult_to_json(res, true);

    if (!*sessions) {
        PQclear(res);
        release_conn(conn);
        return API_JSON_ERROR;
    }

    PQclear(res);

    release_conn(conn);

    return API_OK;
}

apiError_t sessions_delete(const char *userId, const char *sessionUUID,
                           const struct AuthData *authData) {
    if (!authData || !authData->sessionToken)
        return API_AUTH_ERROR;

    PGconn *conn = get_conn();
    if (!conn)
        return API_DB_ERROR;

    if (!validate_session_token(conn, userId, authData->sessionToken)) {
        release_conn(conn);
        return API_AUTH_ERROR;
    }

    const char *paramValues[1] = {sessionUUID};

    PGresult *res = PQexecParams(conn,
                                 "UPDATE auth.user_sessions "
                                 "SET revoked_at = now() "
                                 "WHERE (uuid::text = $1);",
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

apiError_t stations_create(const char *name, double lon, double lat, double alt,
                           const struct AuthData *authData, json_t **station) {
    if (!authData || !authData->sessionToken || !name)
        return API_AUTH_ERROR;

    if (!validate_name(name))
        return API_INVALID_PARAMS;

    PGconn *conn = get_conn();
    if (!conn)
        return API_DB_ERROR;

    char *userUUID = NULL;

    if (!get_user_session_token(conn, &userUUID, authData->sessionToken)) {
        release_conn(conn);
        return API_AUTH_ERROR;
    }

    char location[256];
    snprintf(location, sizeof(location), "SRID=4326;POINTZ(%f %f %f)", lon, lat, alt);

    const char *paramValues[3] = {name, location, userUUID};

    PGresult *res = PQexecParams(
        conn,
        "WITH new_station AS ("
        "  INSERT INTO stations.stations (user_id, name, location)"
        "  SELECT u.user_id, $1, ST_GeogFromText($2)"
        "  FROM auth.users u"
        "  WHERE u.uuid::text = $3"
        "    AND (u.max_stations = -1 OR (SELECT COUNT(*) "
        "        FROM stations.stations s "
        "        WHERE s.user_id = u.user_id AND s.deleted_at IS NULL) < u.max_stations)"
        "  RETURNING uuid, name,"
        "            ST_X(location::geometry) AS lon,"
        "            ST_Y(location::geometry) AS lat,"
        "            COALESCE(ST_Z(location::geometry), 0) AS alt"
        ")"
        "SELECT uuid, name, lon, lat, alt FROM new_station;",
        3, NULL, paramValues, NULL, NULL, 0);

    free(userUUID);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Error executing the query: %s", PQerrorMessage(conn));
        PQclear(res);
        release_conn(conn);
        return API_DB_ERROR;
    }

    if (PQntuples(res) == 0) {
        PQclear(res);
        release_conn(conn);
        return API_FORBIDDEN;
    }

    *station = pgresult_to_json(res, true);
    if (!*station) {
        PQclear(res);
        release_conn(conn);
        return API_JSON_ERROR;
    }

    PQclear(res);

    release_conn(conn);

    return API_OK;
}

apiError_t stations_list(const char *stationId, json_t **stations) {
    if (!stations)
        return API_INVALID_PARAMS;

    PGconn *conn = get_conn();
    if (!conn)
        return API_DB_ERROR;

    const char *paramValues[1] = {stationId};

    PGresult *res = PQexecParams(conn,
                                 "SELECT "
                                 "uuid, "
                                 "name, "
                                 "ST_X(location::geometry) AS lon, "
                                 "ST_Y(location::geometry) AS lat, "
                                 "COALESCE(ST_Z(location::geometry), 0) AS alt "
                                 "FROM stations.stations "
                                 "WHERE deleted_at IS NULL "
                                 "AND ($1::text IS NULL OR uuid::text = $1 OR name = $1);",
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
        return API_FORBIDDEN;
    }

    if (!stationId)
        *stations = pgresult_to_json(res, false);
    else
        *stations = pgresult_to_json(res, true);

    if (!*stations) {
        PQclear(res);
        release_conn(conn);
        return API_JSON_ERROR;
    }

    PQclear(res);

    release_conn(conn);

    return API_OK;
}

apiError_t api_key_create(const char *name, const char *keyType, const char *stationId,
                          const char *userId, const struct AuthData *authData, json_t **key) {
    if (!authData || !authData->sessionToken)
        return API_AUTH_ERROR;

    if (!validate_name(name) || !name || !keyType || !stationId)
        return API_INVALID_PARAMS;

    PGconn *conn = get_conn();
    if (!conn)
        return API_DB_ERROR;

    if (!validate_session_token(conn, userId, authData->sessionToken))
        return API_AUTH_ERROR;

    char tokenB64[sodium_base64_ENCODED_LEN(KEY_ENTROPY, BASE64_VARIANT)];
    char hashB64[sodium_base64_ENCODED_LEN(crypto_generichash_BYTES, BASE64_VARIANT)];

    generateSessionToken(tokenB64, sizeof(tokenB64), hashB64, sizeof(hashB64));

    const char *paramValues[6] = {userId, stationId, name, hashB64, keyType, tokenB64};

    PGresult *res = PQexecParams(
        conn,
        "INSERT INTO auth.api_keys (user_id, name, api_key, api_key_type, station_id, expires_at) "
        "SELECT "
        "  u.user_id, "
        "  $3, "
        "  $4, "
        "  $5, "
        "  s.station_id, "
        "  NULL "
        "FROM auth.users u "
        "JOIN stations.stations s ON s.user_id = u.user_id "
        "WHERE (u.uuid::text = $1 OR u.username = $1) "
        "  AND (s.uuid::text = $2 OR s.name = $2) "
        "RETURNING "
        "  uuid, "
        "  name, "
        "  api_key_type, "
        "  created_at, "
        "  expires_at, "
        "  $2::text AS station_uuid, "
        "  $6::text AS api_key;",
        6, NULL, paramValues, NULL, NULL, 0);

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

    *key = pgresult_to_json(res, true);
    if (!*key) {
        PQclear(res);
        release_conn(conn);
        return API_JSON_ERROR;
    }

    PQclear(res);

    release_conn(conn);

    return API_OK;
}

apiError_t api_key_list(const char *userId, const char *keyId, const struct AuthData *authData,
                        json_t **keys) {
    if (!authData || !authData->sessionToken)
        return API_AUTH_ERROR;

    if (!userId)
        return API_INVALID_PARAMS;

    PGconn *conn = get_conn();
    if (!conn)
        return API_DB_ERROR;

    if (!validate_session_token(conn, userId, authData->sessionToken))
        return API_AUTH_ERROR;

    const char *paramValues[2] = {userId, keyId};

    PGresult *res = PQexecParams(
        conn,
        "SELECT"
        "       k.uuid, "
        "       k.name, "
        "       k.api_key_type, "
        "       s.name AS station_id, "
        "       k.created_at,"
        "       k.expires_at, "
        "       k.revoked_at "
        "FROM auth.api_keys k "
        "JOIN auth.users u ON k.user_id = u.user_id "
        "LEFT JOIN stations.stations s ON k.station_id = s.station_id "
        "WHERE (k.expires_at IS NULL OR k.expires_at > NOW()) "
        "  AND k.revoked_at IS NULL "
        "  AND (u.uuid::text = $1::text OR u.username::text = $1::text) "
        "  AND ($2::text IS NULL OR k.uuid::text = $2::text OR k.name::text = $2::text)",
        2, NULL, paramValues, NULL, NULL, 0);

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

    if (!keyId)
        *keys = pgresult_to_json(res, false);
    else
        *keys = pgresult_to_json(res, true);

    if (!*keys) {
        PQclear(res);
        release_conn(conn);
        return API_JSON_ERROR;
    }

    PQclear(res);

    release_conn(conn);

    return API_OK;
}

apiError_t api_key_delete(const char *userId, const char *keyId, const struct AuthData *authData) {
    if (!authData | !authData->sessionToken)
        return API_AUTH_ERROR;

    if (!userId || !keyId)
        return API_INVALID_PARAMS;

    PGconn *conn = get_conn();
    if (!conn)
        return API_DB_ERROR;

    if (!validate_session_token(conn, userId, authData->sessionToken)) {
        release_conn(conn);
        return API_AUTH_ERROR;
    }

    const char *paramValues[1] = {keyId};

    PGresult *res = PQexecParams(conn,
                                 "UPDATE auth.api_keys "
                                 "SET revoked_at = now() "
                                 "WHERE (uuid::text = $1 OR name = $1);",
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

apiError_t weather_data_list(int fields, const char *granularityStr, const char *stationId,
                             const char *timezone, const char *startTime, const char *endTime,
                             json_t **weatherData) {
    if (!timezone || !startTime || !endTime || !weatherData || !granularityStr)
        return API_INVALID_PARAMS;

    if (fields < 0)
        return API_INVALID_PARAMS;

    PGconn *conn = get_conn();
    if (!conn)
        return API_DB_ERROR;

    PGresult *res = NULL;

    char quoteQuery[256];
    snprintf(quoteQuery, sizeof(quoteQuery), "SELECT quote_literal('%s');", timezone);

    res = PQexec(conn, quoteQuery);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Error quoting literal: %s\n", PQerrorMessage(conn));
        PQclear(res);
        release_conn(conn);
        return API_DB_ERROR;
    }

    char *escapedTz = PQgetvalue(res, 0, 0);

    char queryTz[512];
    snprintf(queryTz, sizeof(queryTz), "SET TIME ZONE %s;", escapedTz);

    PQclear(res);
    res = PQexec(conn, queryTz);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Error executing the query: %s", PQerrorMessage(conn));
        PQclear(res);
        release_conn(conn);
        return API_DB_ERROR;
    }
    PQclear(res);

    char *query;
    granularity_t granularity = string_to_granularity(granularityStr);

    // Cannot use static data
    bool sameTimezone =
        same_timezone_offset_during_range(startTime, endTime, timezone, DEFAULT_TIMEZONE);

    if (!sameTimezone && granularity != GRANULARITY_DATA) {
        query = build_generic_weather_query(fields);
    }
    else {
        query = build_static_query(fields, granularity);
    }

    if (!query) {
        release_conn(conn);
        return API_MEMORY_ERROR;
    }

    const char *paramValues[4] = {stationId, startTime, endTime, granularityStr};
    if (!sameTimezone && granularity != GRANULARITY_DATA)
        res = PQexecParams(conn, query, 4, NULL, paramValues, NULL, NULL, 0);
    else
        res = PQexecParams(conn, query, 3, NULL, paramValues, NULL, NULL, 0);

    free(query);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Error executing the query: %s", PQerrorMessage(conn));
        PQclear(res);
        release_conn(conn);
        return API_DB_ERROR;
    }

    if (PQntuples(res) == 0) {
        PQclear(res);
        release_conn(conn);
        return API_FORBIDDEN;
    }

    *weatherData = pgresult_to_json(res, false);

    PQclear(res);
    release_conn(conn);

    return API_OK;
}
