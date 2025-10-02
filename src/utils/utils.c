#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include "utils.h"

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
