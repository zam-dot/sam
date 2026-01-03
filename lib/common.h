// common.h - Common types and utilities
#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stddef.h>

// Simple dynamic string
typedef struct {
    char  *data;
    size_t length;
    size_t capacity;
} String;

String string_create(const char *initial);
void   string_append(String *str, const char *text);
void   string_free(String *str);

// Error handling
void error(const char *format, ...);

#endif
