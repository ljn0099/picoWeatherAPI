#define _XOPEN_SOURCE 700
#include "../core/weather.h"
#include "utils.h"
#include <ctype.h>
#include <jansson.h>
#include <libpq-fe.h>
#include <sodium.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unicode/ucal.h>
#include <unicode/ustring.h>
#include <unicode/utypes.h>

#define GENERIC_WEATHER_QUERY_SIZE 4096
#define APPEND_QUERY_FIELD(FIELD_FLAG, SQL_EXPR)                                                   \
    do {                                                                                           \
        if (fields & (FIELD_FLAG)) {                                                               \
            if (!append_to_buffer(&p, &remaining, SQL_EXPR)) {                                     \
                free(query);                                                                       \
                return NULL;                                                                       \
            }                                                                                      \
        }                                                                                          \
    } while (0)

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
    const char *result = strptime(timestamp, "%Y-%m-%dT%H:%M:%S", &tm);
    if (result == NULL || *result != '\0')
        return false; // Invalid format

    return true;
}

bool validate_email(const char *email) {
    if (email == NULL)
        return false;

    const char *at = strchr(email, '@');
    if (!at)
        return false; // Must contain '@'
    if (at == email)
        return false; // Cannot start with '@'

    const char *dot = strrchr(at, '.');
    if (!dot)
        return false; // Must contain at least one '.'
    if (dot < at + 2)
        return false; // Must have at least one character between '@' and '.'
    if (*(dot + 1) == '\0')
        return false; // Cannot end with '.'

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
            return false; // Only letters allowed in TLD
    }

    return true;
}

bool append_to_buffer(char **buf_ptr, size_t *remaining, const char *fmt, ...) {
    if (!buf_ptr || !*buf_ptr || !remaining || !fmt)
        return false;

    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(*buf_ptr, *remaining, fmt, args);
    va_end(args);

    if (ret < 0 || (size_t)ret >= *remaining)
        return false;

    *buf_ptr += ret;
    *remaining -= ret;
    return true;
}

char *build_generic_weather_query(int fields) {
    const char *queryBase = "WITH params AS (\n"
                            "    SELECT\n"
                            "        (SELECT station_id FROM stations.stations WHERE name = $1 OR uuid::text = $1) AS station_id,\n"
                            "        $2::timestamp AS start_ts,\n"
                            "        $3::timestamp AS end_ts,\n"
                            "        $4::text AS granularity\n"
                            "),\n"
                            "time_ranges AS (\n"
                            "    SELECT\n"
                            "        station_id,\n"
                            "        granularity,\n"
                            "        tstzrange(\n"
                            "            ts,\n"
                            "            ts + (\n"
                            "                CASE granularity\n"
                            "                    WHEN 'hour' THEN interval '1 hour'\n"
                            "                    WHEN 'day' THEN interval '1 day'\n"
                            "                    WHEN 'week' THEN interval '1 week'\n"
                            "                    WHEN 'month' THEN interval '1 month'\n"
                            "                    WHEN 'year' THEN interval '1 year'\n"
                            "                END\n"
                            "            )\n"
                            "        ) AS time_range\n"
                            "    FROM params,\n"
                            "    generate_series(\n"
                            "        date_trunc(granularity, start_ts),\n"
                            "        date_trunc(granularity, end_ts),\n"
                            "        CASE granularity\n"
                            "            WHEN 'hour' THEN interval '1 hour'\n"
                            "            WHEN 'day' THEN interval '1 day'\n"
                            "            WHEN 'week' THEN interval '1 week'\n"
                            "            WHEN 'month' THEN interval '1 month'\n"
                            "            WHEN 'year' THEN interval '1 year'\n"
                            "        END\n"
                            "    ) AS ts\n"
                            ")\n"
                            "SELECT "
                            "lower(d.time_range) AS period_start, "
                            "upper(d.time_range) AS period_end, "
                            "d.granularity, ";

    const char *queryEnd = " FROM time_ranges d\n"
                           "LEFT JOIN weather.weather_data wd\n"
                           "   ON wd.station_id = d.station_id\n"
                           "   AND wd.time_range && d.time_range\n"
                           "GROUP BY d.station_id, d.time_range, d.granularity\n"
                           "ORDER BY d.time_range;";

    size_t querySize = GENERIC_WEATHER_QUERY_SIZE;
    size_t remaining = GENERIC_WEATHER_QUERY_SIZE;
    char *query = malloc(querySize);
    if (!query)
        return NULL;

    char *p = query;

    if (!append_to_buffer(&p, &remaining, "%s", queryBase)) {
        free(query);
        return NULL;
    }

    APPEND_QUERY_FIELD(SUMMARY_AVG_TEMPERATURE, " AVG(wd.temperature) AS avg_temperature,");
    APPEND_QUERY_FIELD(SUMMARY_MAX_TEMPERATURE, " MAX(wd.temperature) AS max_temperature,");
    APPEND_QUERY_FIELD(SUMMARY_MIN_TEMPERATURE, " MIN(wd.temperature) AS min_temperature,");
    APPEND_QUERY_FIELD(SUMMARY_STDDEV_TEMPERATURE,
                       " STDDEV(wd.temperature) AS stddev_temperature,");

    APPEND_QUERY_FIELD(SUMMARY_AVG_HUMIDITY, " AVG(wd.humidity) AS avg_humidity,");
    APPEND_QUERY_FIELD(SUMMARY_MAX_HUMIDITY, " MAX(wd.humidity) AS max_humidity,");
    APPEND_QUERY_FIELD(SUMMARY_MIN_HUMIDITY, " MIN(wd.humidity) AS min_humidity,");
    APPEND_QUERY_FIELD(SUMMARY_STDDEV_HUMIDITY, " STDDEV(wd.humidity) AS stddev_humidity,");

    APPEND_QUERY_FIELD(SUMMARY_AVG_PRESSURE, " AVG(wd.pressure) AS avg_pressure,");
    APPEND_QUERY_FIELD(SUMMARY_MAX_PRESSURE, " MAX(wd.pressure) AS max_pressure,");
    APPEND_QUERY_FIELD(SUMMARY_MIN_PRESSURE, " MIN(wd.pressure) AS min_pressure,");

    APPEND_QUERY_FIELD(SUMMARY_SUM_RAINFALL, " SUM(wd.rainfall) AS sum_rainfall,");
    APPEND_QUERY_FIELD(SUMMARY_STDDEV_RAINFALL, " STDDEV(wd.rainfall) AS stddev_rainfall,");

    APPEND_QUERY_FIELD(SUMMARY_AVG_WIND_SPEED, " AVG(wd.wind_speed) AS avg_wind_speed,");
    APPEND_QUERY_FIELD(SUMMARY_AVG_WIND_DIRECTION,
                       " MOD( "
                       " CAST(DEGREES( "
                       "   ATAN2( "
                       "     SUM(CAST(wd.wind_speed AS numeric) * "
                       "SIN(RADIANS(CAST(wd.wind_direction AS numeric)))), "
                       "     SUM(CAST(wd.wind_speed AS numeric) * "
                       "COS(RADIANS(CAST(wd.wind_direction AS numeric)))) "
                       "   ) "
                       " ) AS numeric) + 360, 360 "
                       ") AS avg_wind_direction,");
    APPEND_QUERY_FIELD(SUMMARY_STDDEV_WIND_SPEED, " STDDEV(wd.wind_speed) AS stddev_wind_speed,");
    APPEND_QUERY_FIELD(SUMMARY_WIND_RUN,
                       " SUM(wd.wind_speed * EXTRACT(EPOCH FROM (upper(wd.time_range) - "
                       "lower(wd.time_range)))) AS wind_run,");

    APPEND_QUERY_FIELD(SUMMARY_MAX_GUST_SPEED, " MAX(wd.gust_speed) AS max_gust_speed,");
    APPEND_QUERY_FIELD(SUMMARY_MAX_GUST_DIRECTION,
                       " (SELECT wd2.gust_direction FROM weather.weather_data wd2 WHERE "
                       "wd2.station_id = d.station_id AND wd2.time_range && d.time_range ORDER "
                       "BY wd2.gust_speed DESC LIMIT 1) AS max_gust_direction,");

    APPEND_QUERY_FIELD(SUMMARY_MAX_LUX, " MAX(wd.lux) AS max_lux,");
    APPEND_QUERY_FIELD(SUMMARY_AVG_LUX, " AVG(wd.lux) AS avg_lux,");

    APPEND_QUERY_FIELD(SUMMARY_MAX_UVI, " MAX(wd.uvi) AS max_uvi,");
    APPEND_QUERY_FIELD(SUMMARY_AVG_UVI, " AVG(wd.uvi) AS avg_uvi,");

    APPEND_QUERY_FIELD(SUMMARY_AVG_SOLAR_IRRADIANCE,
                       " AVG(wd.solar_irradiance) AS avg_solar_irradiance,");

    // Delete the last ,
    if (p > query && *(p - 1) == ',') {
        p--;
        *p = '\0';
        remaining++;
    }

    if (!append_to_buffer(&p, &remaining, "%s", queryEnd)) {
        free(query);
        return NULL;
    }

    return query;
}

char *build_static_query(int fields, granularity_t granularity) {
    const char *queryBase = "SELECT\n"
                            "lower(time_range) AS period_start,\n"
                            "upper(time_range) AS period_end,\n";

    const char *queryEnd;

    if (granularity == GRANULARITY_DATA) {
        queryEnd = " FROM weather.weather_data\n"
                   "WHERE station_id = (SELECT station_id FROM stations.stations WHERE name = $1 OR uuid::text = $1)\n"
                   "    AND time_range && tstzrange($2, $3)\n"
                   "ORDER BY lower(time_range);";
    }
    else if (granularity == GRANULARITY_HOUR)
        queryEnd = " FROM weather.weather_hourly_summary\n"
                   "WHERE station_id = (SELECT station_id FROM stations.stations WHERE name = $1 OR uuid::text = $1)\n"
                   "    AND time_range && tstzrange($2, $3)\n"
                   "ORDER BY lower(time_range);";
    else if (granularity == GRANULARITY_DAY)
        queryEnd = " FROM weather.weather_daily_summary\n"
                   "WHERE station_id = (SELECT station_id FROM stations.stations WHERE name = $1 OR uuid::text = $1)\n"
                   "    AND time_range && tstzrange($2, $3)\n"
                   "ORDER BY lower(time_range);";
    else if (granularity == GRANULARITY_MONTH)
        queryEnd = " FROM weather.weather_monthly_summary\n"
                   "WHERE station_id = (SELECT station_id FROM stations.stations WHERE name = $1 OR uuid::text = $1)\n"
                   "    AND time_range && tstzrange($2, $3)\n"
                   "ORDER BY lower(time_range);";
    else if (granularity == GRANULARITY_YEAR)
        queryEnd = " FROM weather.weather_yearly_summary\n"
                   "WHERE station_id = (SELECT station_id FROM stations.stations WHERE name = $1 OR uuid::text = $1)\n"
                   "    AND time_range && tstzrange($2, $3)\n"
                   "ORDER BY lower(time_range);";
    else
        queryEnd = NULL;

    size_t querySize = GENERIC_WEATHER_QUERY_SIZE;
    size_t remaining = GENERIC_WEATHER_QUERY_SIZE;
    char *query = malloc(querySize);
    if (!query)
        return NULL;

    char *p = query;

    if (!append_to_buffer(&p, &remaining, "%s", queryBase)) {
        free(query);
        return NULL;
    }

    if (granularity == GRANULARITY_DATA) {
        APPEND_QUERY_FIELD(DATA_TEMP, " temperature,");
        APPEND_QUERY_FIELD(DATA_HUMIDITY, " humidity,");
        APPEND_QUERY_FIELD(DATA_PRESSURE, " pressure,");
        APPEND_QUERY_FIELD(DATA_LUX, " lux,");
        APPEND_QUERY_FIELD(DATA_UVI, " uvi,");
        APPEND_QUERY_FIELD(DATA_WIND_SPEED, " wind_speed,");
        APPEND_QUERY_FIELD(DATA_WIND_DIRECTION, " wind_direction,");
        APPEND_QUERY_FIELD(DATA_GUST_SPEED, " gust_speed,");
        APPEND_QUERY_FIELD(DATA_GUST_DIRECTION, " gust_direction,");
        APPEND_QUERY_FIELD(DATA_RAINFALL, " rainfall,");
        APPEND_QUERY_FIELD(DATA_SOLAR_IRRADIANCE, " solar_irradiance,");
    }

    if (granularity != GRANULARITY_DATA) {
        APPEND_QUERY_FIELD(SUMMARY_AVG_TEMPERATURE, " avg_temperature,");

        APPEND_QUERY_FIELD(SUMMARY_AVG_HUMIDITY, " avg_humidity,");

        APPEND_QUERY_FIELD(SUMMARY_AVG_PRESSURE, " avg_pressure,");

        APPEND_QUERY_FIELD(SUMMARY_SUM_RAINFALL, " sum_rainfall,");
        APPEND_QUERY_FIELD(SUMMARY_STDDEV_RAINFALL, " stddev_rainfall,");

        APPEND_QUERY_FIELD(SUMMARY_AVG_WIND_SPEED, " avg_wind_speed,");
        APPEND_QUERY_FIELD(SUMMARY_AVG_WIND_DIRECTION, " avg_wind_direction,");
        APPEND_QUERY_FIELD(SUMMARY_STDDEV_WIND_SPEED, " stddev_wind_speed,");

        APPEND_QUERY_FIELD(SUMMARY_MAX_GUST_SPEED, " max_gust_speed,");
        APPEND_QUERY_FIELD(SUMMARY_MAX_GUST_DIRECTION, " max_gust_direction,");

        APPEND_QUERY_FIELD(SUMMARY_AVG_LUX, " avg_lux,");

        APPEND_QUERY_FIELD(SUMMARY_AVG_UVI, " avg_uvi,");

        APPEND_QUERY_FIELD(SUMMARY_AVG_SOLAR_IRRADIANCE, " avg_solar_irradiance,");
    }

    if (granularity == GRANULARITY_DAY)
        APPEND_QUERY_FIELD(SUMMARY_WIND_RUN, " wind_run,");

    if (granularity == GRANULARITY_DAY || granularity == GRANULARITY_MONTH ||
        granularity == GRANULARITY_YEAR) {
        APPEND_QUERY_FIELD(SUMMARY_MAX_TEMPERATURE, " max_temperature,");
        APPEND_QUERY_FIELD(SUMMARY_MIN_TEMPERATURE, " min_temperature,");
        APPEND_QUERY_FIELD(SUMMARY_STDDEV_TEMPERATURE, " stddev_temperature,");

        APPEND_QUERY_FIELD(SUMMARY_MAX_HUMIDITY, " max_humidity,");
        APPEND_QUERY_FIELD(SUMMARY_MIN_HUMIDITY, " min_humidity,");
        APPEND_QUERY_FIELD(SUMMARY_STDDEV_HUMIDITY, " stddev_humidity,");

        APPEND_QUERY_FIELD(SUMMARY_MAX_PRESSURE, " max_pressure,");
        APPEND_QUERY_FIELD(SUMMARY_MIN_PRESSURE, " min_pressure,");

        APPEND_QUERY_FIELD(SUMMARY_MAX_LUX, " max_lux,");
        APPEND_QUERY_FIELD(SUMMARY_MAX_UVI, " max_uvi,");
        APPEND_QUERY_FIELD(SUMMARY_AVG_SOLAR_IRRADIANCE, " avg_solar_irradiance,");
    }

    // Delete the last ,
    if (p > query && *(p - 1) == ',') {
        p--;
        *p = '\0';
        remaining++;
    }

    if (!append_to_buffer(&p, &remaining, "%s", queryEnd)) {
        free(query);
        return NULL;
    }

    return query;
}

granularity_t string_to_granularity(const char *granularityStr) {
    if (!granularityStr)
        return GRANULARITY_HOUR;

    if (strcmp(granularityStr, "raw") == 0)
        return GRANULARITY_DATA;
    else if (strcmp(granularityStr, "hour") == 0)
        return GRANULARITY_HOUR;
    else if (strcmp(granularityStr, "day") == 0)
        return GRANULARITY_DAY;
    else if (strcmp(granularityStr, "month") == 0)
        return GRANULARITY_MONTH;
    else if (strcmp(granularityStr, "year") == 0)
        return GRANULARITY_YEAR;
    else
        return GRANULARITY_HOUR;
}

int string_to_field(const char *fieldStr) {
    if (strcmp(fieldStr, "temperature") == 0)
        return DATA_TEMP;
    else if (strcmp(fieldStr, "humidity") == 0)
        return DATA_HUMIDITY;
    else if (strcmp(fieldStr, "pressure") == 0)
        return DATA_PRESSURE;
    else if (strcmp(fieldStr, "lux") == 0)
        return DATA_LUX;
    else if (strcmp(fieldStr, "uvi") == 0)
        return DATA_UVI;
    else if (strcmp(fieldStr, "wind_speed") == 0)
        return DATA_WIND_SPEED;
    else if (strcmp(fieldStr, "wind_direction") == 0)
        return DATA_WIND_DIRECTION;
    else if (strcmp(fieldStr, "gust_speed") == 0)
        return DATA_GUST_SPEED;
    else if (strcmp(fieldStr, "gust_direction") == 0)
        return DATA_GUST_DIRECTION;
    else if (strcmp(fieldStr, "rainfall") == 0)
        return DATA_RAINFALL;
    else if (strcmp(fieldStr, "solar_irradiance") == 0)
        return DATA_SOLAR_IRRADIANCE;
    else if (strcmp(fieldStr, "avg_temperature") == 0)
        return SUMMARY_AVG_TEMPERATURE;
    else if (strcmp(fieldStr, "max_temperature") == 0)
        return SUMMARY_MAX_TEMPERATURE;
    else if (strcmp(fieldStr, "min_temperature") == 0)
        return SUMMARY_MIN_TEMPERATURE;
    else if (strcmp(fieldStr, "stddev_temperature") == 0)
        return SUMMARY_STDDEV_TEMPERATURE;
    else if (strcmp(fieldStr, "avg_humidity") == 0)
        return SUMMARY_AVG_HUMIDITY;
    else if (strcmp(fieldStr, "max_humidity") == 0)
        return SUMMARY_MAX_HUMIDITY;
    else if (strcmp(fieldStr, "min_humidity") == 0)
        return SUMMARY_MIN_HUMIDITY;
    else if (strcmp(fieldStr, "stddev_humidity") == 0)
        return SUMMARY_STDDEV_HUMIDITY;
    else if (strcmp(fieldStr, "avg_pressure") == 0)
        return SUMMARY_AVG_PRESSURE;
    else if (strcmp(fieldStr, "max_pressure") == 0)
        return SUMMARY_MAX_PRESSURE;
    else if (strcmp(fieldStr, "min_pressure") == 0)
        return SUMMARY_MIN_PRESSURE;
    else if (strcmp(fieldStr, "sum_rainfall") == 0)
        return SUMMARY_SUM_RAINFALL;
    else if (strcmp(fieldStr, "stddev_rainfall") == 0)
        return SUMMARY_STDDEV_RAINFALL;
    else if (strcmp(fieldStr, "avg_wind_speed") == 0)
        return SUMMARY_AVG_WIND_SPEED;
    else if (strcmp(fieldStr, "avg_wind_direction") == 0)
        return SUMMARY_AVG_WIND_DIRECTION;
    else if (strcmp(fieldStr, "stddev_wind_speed") == 0)
        return SUMMARY_STDDEV_WIND_SPEED;
    else if (strcmp(fieldStr, "wind_run") == 0)
        return SUMMARY_WIND_RUN;
    else if (strcmp(fieldStr, "max_gust_speed") == 0)
        return SUMMARY_MAX_GUST_SPEED;
    else if (strcmp(fieldStr, "max_gust_direction") == 0)
        return SUMMARY_MAX_GUST_DIRECTION;
    else if (strcmp(fieldStr, "avg_lux") == 0)
        return SUMMARY_AVG_LUX;
    else if (strcmp(fieldStr, "max_lux") == 0)
        return SUMMARY_MAX_LUX;
    else if (strcmp(fieldStr, "avg_uvi") == 0)
        return SUMMARY_AVG_UVI;
    else if (strcmp(fieldStr, "max_uvi") == 0)
        return SUMMARY_MAX_UVI;
    else if (strcmp(fieldStr, "avg_solar_irradiance") == 0)
        return SUMMARY_AVG_SOLAR_IRRADIANCE;
    else
        return -1;
}

bool same_timezone_offset_during_range(const char *startStr, const char *endStr, const char *tz1,
                                       const char *tz2) {
    if (!startStr || !endStr || !tz1 || !tz2)
        return false;

    if (strcmp(tz1, tz2) == 0) {
        return true;
    }

    UErrorCode status = U_ZERO_ERROR;

    // Convert timezone names from char* to UChar* for ICU
    UChar uTz1[128], uTz2[128];
    u_charsToUChars(tz1, uTz1, strlen(tz1) + 1);
    u_charsToUChars(tz2, uTz2, strlen(tz2) + 1);

    // Open ICU calendars for both timezones
    UCalendar *cal1 = ucal_open(uTz1, -1, NULL, UCAL_GREGORIAN, &status);
    UCalendar *cal2 = ucal_open(uTz2, -1, NULL, UCAL_GREGORIAN, &status);
    if (U_FAILURE(status) || !cal1 || !cal2) {
        if (cal1) ucal_close(cal1);
        if (cal2) ucal_close(cal2);
        return false;
    }

    // Parse start and end timestamps into year, month, day, hour, minute, second
    int year, month, day, hour, min, sec;
    sscanf(startStr, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &min, &sec);
    ucal_setDateTime(cal1, year, month - 1, day, hour, min, sec, &status);
    UDate start = ucal_getMillis(cal1, &status);

    sscanf(endStr, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &min, &sec);
    ucal_setDateTime(cal1, year, month - 1, day, hour, min, sec, &status);
    UDate end = ucal_getMillis(cal1, &status);

    if (U_FAILURE(status)) {
        ucal_close(cal1);
        ucal_close(cal2);
        return false;
    }

    // Initialize currentTime to start of range
    UDate currentTime = start;

    while (currentTime <= end) {
        // Set calendars to currentTime
        ucal_setMillis(cal1, currentTime, &status);
        ucal_setMillis(cal2, currentTime, &status);

        // Get total UTC offset including DST
        int32_t offset1 = ucal_get(cal1, UCAL_ZONE_OFFSET, &status) + ucal_get(cal1, UCAL_DST_OFFSET, &status);
        int32_t offset2 = ucal_get(cal2, UCAL_ZONE_OFFSET, &status) + ucal_get(cal2, UCAL_DST_OFFSET, &status);

        // If offsets differ, return false immediately
        if (offset1 != offset2) {
            ucal_close(cal1);
            ucal_close(cal2);
            return false;
        }

        // Advance to next possible DST change (approx. next day)
        currentTime += 24 * 3600 * 1000; // 1 day in milliseconds
    }

    // Close calendars and return true if offsets were identical throughout the range
    ucal_close(cal1);
    ucal_close(cal2);
    return true;
}

apiKeyType_t string_to_key_type(const char *typeStr) {
    if (strcmp(typeStr, "weather_upload") == 0)
        return KEY_TYPE_WEATHER_UPLOAD;
    else if (strcmp(typeStr, "stations_management") == 0)
        return KEY_TYPE_STATIONS_MNG;
    else if (strcmp(typeStr, "stations_control") == 0)
        return KEY_TYPE_STATIONS_CONTROL;
    else
        return KEY_TYPE_INVALID;
}
