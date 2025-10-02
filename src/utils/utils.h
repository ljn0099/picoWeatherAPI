#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>

#define UUID_SIZE 36
#define NAME_SIZE 30
#define NAME_SIZE_MIN 3

bool validate_name(const char *str);
bool validate_uuid(const char *uuid);

#endif
