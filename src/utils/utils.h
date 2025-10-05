#ifndef UTILS_H
#define UTILS_H

#include <jansson.h>
#include <libpq-fe.h>
#include <sodium.h>
#include <stdbool.h>

#ifdef DEBUG
#define DEBUG_PRINTF(...)                                                                          \
    do {                                                                                           \
        fprintf(stderr, "DEBUG: ");                                                                \
        fprintf(stderr, __VA_ARGS__);                                                              \
    } while (0)
#else
#define DEBUG_PRINTF(...) ((void)0)
#endif

#define UUID_SIZE 36
#define NAME_SIZE 30
#define NAME_SIZE_MIN 3
#define KEY_ENTROPY 32
#define BASE64_VARIANT sodium_base64_VARIANT_URLSAFE_NO_PADDING

bool validate_name(const char *str);
bool validate_uuid(const char *uuid);

bool validate_password(PGconn *conn, const char *userId, const char *password);

bool validate_session_token(PGconn *conn, const char *userId, const char *sessionToken);

void generateSessionToken(char *tokenB64, size_t tokenB64Len,
                          char *hashB64, size_t hashB64Len);

bool get_user_session_token(PGconn *conn, char **userId, const char *sessionToken);

json_t *pgresult_to_json(PGresult *res, bool canBeObject);
#endif
