// lib/safety.c - Implementation matching safety.h
#include "safety.h"
#include <stdio.h>
// Core refcounting implementation
void *rc_alloc(size_t size) {
    RCHeader *header = (RCHeader *)calloc(1, RC_HEADER_SIZE + size);
    if (header) {
        header->refcount = 1;
        header->weak_count = 0;
        header->array_count = 0;
    }
    return header ? (char *)header + RC_HEADER_SIZE : NULL;
}

void *rc_alloc_array(size_t elem_size, size_t count) {
    RCHeader *header = (RCHeader *)calloc(1, RC_HEADER_SIZE + (elem_size * count));
    if (header) {
        header->refcount = 1;
        header->weak_count = 0;
        header->array_count = count;
        memset((char *)header + RC_HEADER_SIZE, 0, elem_size * count);
    }
    return header ? (char *)header + RC_HEADER_SIZE : NULL;
}

void rc_retain(void *ptr) {
    if (ptr) {
        RCHeader *header = RC_GET_HEADER(ptr);
        header->refcount++;
    }
}

void rc_release(void *ptr) {
    if (!ptr) return;
    RCHeader *header = RC_GET_HEADER(ptr);

    if (--header->refcount == 0 && header->weak_count == 0) {
        free(header);
    }
}

void rc_weak_retain(void *ptr) {
    if (ptr) {
        RCHeader *header = RC_GET_HEADER(ptr);
        header->weak_count++;
    }
}

void rc_weak_release(void *ptr) {
    if (!ptr) return;
    RCHeader *header = RC_GET_HEADER(ptr);
    if (--header->weak_count == 0) {
        if (header->refcount == 0) {
            free(header);
        }
    }
}

void rc_release_array(void *ptr, void (*destructor)(void *)) {
    if (!ptr) return;
    RCHeader *header = RC_GET_HEADER(ptr);
    if (--header->refcount == 0) {
        if (destructor) {
            void **array = (void **)ptr;
            for (size_t i = 0; i < header->array_count; i++) {
                destructor(array[i]);
            }
        }
        if (header->weak_count == 0) {
            free(header);
        }
    }
}

// String implementation
string string_create(const char *literal) {
    if (!literal) return NULL;
    size_t len = strlen(literal);
    char  *str = rc_alloc(len + 1);
    if (str) {
        strcpy(str, literal);
    }
    return str;
}

string string_concat(string a, string b) {
    if (!a || !b) return NULL;
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    char  *result = rc_alloc(len_a + len_b + 1);
    if (result) {
        strcpy(result, a);
        strcpy(result + len_a, b);
    }
    return result;
}

string string_substr(string s, size_t start, size_t len) {
    if (!s) return NULL;
    size_t s_len = strlen(s);
    if (start >= s_len) return string_create("");
    if (start + len > s_len) len = s_len - start;

    char *result = rc_alloc(len + 1);
    if (result) {
        strncpy(result, s + start, len);
        result[len] = '\0';
    }
    return result;
}

size_t string_length(string s) { return s ? strlen(s) : 0; }
void   string_free(string s) { rc_release(s); }
