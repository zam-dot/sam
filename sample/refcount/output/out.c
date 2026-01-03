#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ========== REFCOUNTING RUNTIME ==========
typedef struct RCHeader {
    size_t refcount;
    size_t weak_count;
    size_t array_count;
} RCHeader;

#define RC_HEADER_SIZE sizeof(RCHeader)
#define RC_GET_HEADER(ptr) ((RCHeader *)((char *)(ptr) - RC_HEADER_SIZE))

void *rc_alloc(size_t size) {
    RCHeader *header = (RCHeader *)calloc(1, RC_HEADER_SIZE + size);
    if (header) header->refcount = 1;
    return header ? (char *)header + RC_HEADER_SIZE : NULL;
}

void rc_retain(void *ptr) {
    if (ptr) RC_GET_HEADER(ptr)->refcount++;
}

void rc_release(void *ptr) {
    if (!ptr) return;
    RCHeader *header = RC_GET_HEADER(ptr);
    if (--header->refcount == 0 && header->weak_count == 0) {
        free(header);
    }
}

// ========== STRING API ==========
typedef char *string;

string string_create(const char *literal) {
    if (!literal) return NULL;
    size_t len = strlen(literal);
    char  *str = rc_alloc(len + 1);
    if (str) strcpy(str, literal);
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

// ========== USER CODE STARTS HERE ==========
#include <stdio.h>


void test() {
    string s1 = string_create("Hello");
    string s3 = string_create("World");
    string s4 = string_concat(s1, " ");
    string s5 = string_concat(s4, s3);

    printf("s1: %s\n", s1);
    printf("s3: %s\n", s3);
    printf("s5: %s\n", s5);

    rc_release(s1);
    rc_release(s3);
    rc_release(s4);
    rc_release(s5);
}


int main() {
    test();
    return 0;
}
