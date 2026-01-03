// include/safety.h - Complete refcounting system
#ifndef ZAL_ARENA_H
#define ZAL_ARENA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Your refcounting system
typedef struct {
    size_t refcount;
    size_t weak_count;
    size_t array_count;
} RCHeader;

#define RC_HEADER_SIZE sizeof(RCHeader)
#define RC_GET_HEADER(ptr) ((RCHeader *)((char *)(ptr) - RC_HEADER_SIZE))
#define ZAL_RELEASE(ptr)                                                                           \
    do {                                                                                           \
        rc_release(ptr);                                                                           \
        ptr = NULL;                                                                                \
    } while (0)

// Core refcounting functions
void *rc_alloc(size_t size);
void *rc_alloc_array(size_t elem_size, size_t count);
void  rc_retain(void *ptr);
void  rc_release(void *ptr);
void  rc_weak_retain(void *ptr);
void  rc_weak_release(void *ptr);
void  rc_release_array(void *ptr, void (*destructor)(void *));

// String type (refcounted)
typedef char *string;

string string_create(const char *literal);
string string_concat(string a, string b);
string string_substr(string s, size_t start, size_t len);
size_t string_length(string s);
void   string_free(string s);

// Convenience macros
#define rc_new_array(type, count) (type *)rc_alloc_array(sizeof(type), count)
#define rc_string_new(str) string_create(str)
#define string(str) string_create(str)
#define sfree(s) string_free(s)

#endif
