// lib/common.c - Basic string utilities
#include "common.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

String string_create(const char *initial) {
    size_t len = initial ? strlen(initial) : 0;
    String str = {.data = malloc(len + 1), .length = len, .capacity = len + 1};
    if (initial && len > 0) {
        memcpy(str.data, initial, len);
    }
    if (str.data) str.data[len] = '\0';
    return str;
}

void string_append(String *str, const char *text) {
    if (!text) return;
    size_t text_len = strlen(text);
    size_t new_len = str->length + text_len;

    if (new_len + 1 > str->capacity) {
        str->capacity = new_len + 1;
        str->data = realloc(str->data, str->capacity);
    }

    memcpy(str->data + str->length, text, text_len);
    str->length = new_len;
    str->data[str->length] = '\0';
}

void string_free(String *str) {
    if (str->data) free(str->data);
    str->data = NULL;
    str->length = 0;
    str->capacity = 0;
}

void error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}
