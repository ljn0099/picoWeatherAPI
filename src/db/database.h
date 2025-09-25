#ifndef DATABASE_H
#define DATABASE_H
#include <libpq-fe.h>
#include <sodium.h>
#include <stdbool.h>
#include <stddef.h>

#define USERNAME_SIZE 255
#define EMAIL_SIZE 255
#define TIMESTAMPTZ_SIZE 41
#define TEXT_SIZE 50
#define INT_SIZE 16
#define FLOAT_SIZE 64
#define INET_SIZE 42
#define USER_AGENT_SIZE 256
#define QUERY_SIZE 2048

#define INT_NULL -1
#define FLOAT_NULL -9999.0f
#define UUID_SIZE 37

#define MAX_FIELDS 32

#define FIELD_SIZE(structType, field) sizeof(((structType *)0)->field)

#define COL(fieldEnum, colName, structType, member, colType)                                       \
    {fieldEnum, colName, colType, offsetof(structType, member),                                    \
     (colType == COL_TYPE_STRING ? FIELD_SIZE(structType, member) : 0)}

typedef struct {
    int userId;
    char userUUID[UUID_SIZE];
    char username[USERNAME_SIZE];
    char email[EMAIL_SIZE];
    char password[crypto_pwhash_STRBYTES * 2];
    char createdAt[TIMESTAMPTZ_SIZE];
    int maxStations;
    bool isAdmin;
    char deletedAt[TIMESTAMPTZ_SIZE];
} user_t;

typedef struct {
    int sessionId;
    int userId;
    char sessionToken[crypto_hash_sha256_BYTES * 2];
    char createdAt[TIMESTAMPTZ_SIZE];
    char lastSeenAt[TIMESTAMPTZ_SIZE];
    char expiresAt[TIMESTAMPTZ_SIZE];
    char reauthAt[TIMESTAMPTZ_SIZE];
    char ipAddress[INET_SIZE];
    char userAgent[USER_AGENT_SIZE];
    char revokedAt[TIMESTAMPTZ_SIZE];
} session_t;

typedef struct {
    int keyId;
    int userId;
    char name[TEXT_SIZE];
    char apiKey[crypto_hash_sha256_BYTES * 2];
    char keyType[TEXT_SIZE];
    char createdAt[TIMESTAMPTZ_SIZE];
    char expiresAt[TIMESTAMPTZ_SIZE];
    char revokedAt[TIMESTAMPTZ_SIZE];
} apiKey_t;

typedef enum { COL_TYPE_STRING, COL_TYPE_INT, COL_TYPE_FLOAT, COL_TYPE_BOOL } colType_t;

typedef struct {
    int flag;
    const char *colName;
    colType_t type;
    size_t offset;
    size_t size; // Solo para strings
} colMap_t;

typedef enum {
    USER_FIELD_ID = 1 << 1,
    USER_FIELD_UUID = 1 << 2,
    USER_FIELD_USERNAME = 1 << 3,
    USER_FIELD_EMAIL = 1 << 4,
    USER_FIELD_PASSWORD = 1 << 5,
    USER_FIELD_CREATED_AT = 1 << 6,
    USER_FIELD_MAX_STATIONS = 1 << 7,
    USER_FIELD_IS_ADMIN = 1 << 8,
    USER_FIELD_DELETED_AT = 1 << 9
} userFieldFlags_t;

typedef enum {
    SESSION_FIELD_ID = 1 << 1,
    SESSION_FIELD_USER_ID = 1 << 2,
    SESSION_FIELD_TOKEN = 1 << 3,
    SESSION_FIELD_CREATED_AT = 1 << 4,
    SESSION_FIELD_LAST_SEEN_AT = 1 << 5,
    SESSION_FIELD_EXPIRES_AT = 1 << 6,
    SESSION_FIELD_REAUTH_AT = 1 << 7,
    SESSION_FIELD_IP_ADDRESS = 1 << 8,
    SESSION_FIELD_USER_AGENT = 1 << 9,
    SESSION_FIELD_REVOKED_AT = 1 << 10
} sessionFieldFlags_t;

typedef enum {
    API_KEY_FIELD_ID = 1 << 1,
    API_KEY_FIELD_USER_ID = 1 << 2,
    API_KEY_FIELD_NAME = 1 << 3,
    API_KEY_FIELD_API_KEY = 1 << 4,
    API_KEY_FIELD_KEY_TYPE = 1 << 5,
    API_KEY_FIELD_CREATED_AT = 1 << 6,
    API_KEY_FIELD_EXPIRES_AT = 1 << 7,
    API_KEY_FIELD_REVOKED_AT = 1 << 8
} apiKeyFieldFlags_t;

typedef enum {
    DB_OK = 0,
    DB_CONN_ERROR,
    DB_NULL_PARAMS,
    DB_QUERY_ERROR,
    DB_MEMORY_ERROR,
    DB_NO_ROWS,
    DB_RESULT_PARSING_ERROR,
    DB_AUTH_ERROR
} dbError_t;

extern const colMap_t userColumns[];
extern const colMap_t sessionColumns[];
extern const colMap_t apiKeyColumns[];

PGconn *init_db_conn();

dbError_t find_user_by(PGconn *conn, int flags, user_t **outUsers, int *userCount,
                       const char *where, const char *extra, const char **params, int nParams);

dbError_t find_session_by(PGconn *conn, int flags, session_t **outSessions, int *sessionCount,
                          const char *where, const char *extra, const char **params, int nParams);

dbError_t find_api_key_by(PGconn *conn, int flags, apiKey_t **outApiKeys, int *apiKeysCount,
                          const char *where, const char *extra, const char **params, int nParams);

dbError_t insert_user(PGconn *conn, int flags, user_t *user);

dbError_t insert_session(PGconn *conn, int flags, session_t *session);

dbError_t insert_api_key(PGconn *conn, int flags, apiKey_t *apiKey);

// Internal
void append_column_names(char *query, size_t queryLen, int flags, const colMap_t *columns,
                                size_t nColumns);

void fill_struct_from_db(void *outStruct, PGresult *res, int flags, const colMap_t *columns,
                                size_t nColumns, int row);

int append_insert_placeholders(char *query, size_t queryLen, int flags,
                                      const colMap_t *columns, size_t nColumns,
                                      const void *inStruct, const char *paramValues[],
                                      int *paramCount, bool *paramIsStrdup);

int append_update_placeholders(char *query, size_t queryLen, int flags,
                                      const colMap_t *columns, size_t nColumns,
                                      const void *inStruct, const char *paramValues[],
                                      int *paramCount);

void *fill_struct_array_from_db(PGresult *res, int flags, const colMap_t *columns,
                                       size_t nColumns, size_t structSize, int *outCount);

dbError_t find_records_by(PGconn *conn, int flags, void **outRecords, int *recordCount,
                          const char *tableName, const colMap_t *columns, int nColumns,
                          size_t recordSize, const char *where, const char *extra,
                          const char **params, int nParams);

dbError_t insert_record(PGconn *conn, int flags, const char *tableName, const colMap_t *columns,
                        int nColumns, const void *record);
#endif
