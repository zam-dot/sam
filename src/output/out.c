#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ========== ARENA ALLOCATOR ==========
typedef struct Arena Arena;
struct Arena {
    unsigned char *buffer;
    size_t         offset;
    size_t         capacity;
    int            is_dynamic;
};

Arena *arena_create(size_t capacity) {
    Arena *arena = malloc(sizeof(Arena));
    if (!arena) return NULL;
    arena->buffer = malloc(capacity);
    if (!arena->buffer) {
        free(arena);
        return NULL;
    }
    arena->offset = 0;
    arena->capacity = capacity;
    arena->is_dynamic = 1;
    return arena;
}

void arena_destroy(Arena *arena) {
    if (!arena) return;
    if (arena->is_dynamic && arena->buffer) free(arena->buffer);
    free(arena);
}

void arena_reset(Arena *arena) {
    if (arena) arena->offset = 0;
}

void *arena_alloc(Arena *arena, size_t size) {
    if (!arena || size == 0) return NULL;
    size = (size + 7) & ~7; // Align to 8 bytes
    if (arena->offset + size > arena->capacity) {
        fprintf(stderr, "Arena out of memory\n");
        return NULL;
    }
    void *ptr = arena->buffer + arena->offset;
    arena->offset += size;
    return ptr;
}

void *arena_alloc_zero(Arena *arena, size_t size) {
    void *ptr = arena_alloc(arena, size);
    if (ptr) memset(ptr, 0, size);
    return ptr;
}

#define arena_array(arena, type, count) ((type *)arena_alloc_zero(arena, sizeof(type) * (count)))

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
void arenaTest() {
    Arena *__arena1 = arena_create(1048576);
    int   *my_array = arena_array(__arena1, int, 5);
    int    temp_my_array[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++) {
        my_array[i] = temp_my_array[i];
    }
    for (int i = 0; i < 5; i++) {
        printf("%d ", my_array[i]);
    }
    printf("\n");

    arena_destroy(__arena1); // Added HERE, not in main();
}

int main() {
    Arena *__arena1 = arena_create(2048); // Note: reset to __arena1 in this scope;
    float *floats = arena_array(__arena1, float, 3);
    float  temp_floats[] = {1.0, 2.0, 3.0};
    for (int i = 0; i < 3; i++) {
        floats[i] = temp_floats[i];
    }

    for (int i = 0; i < 3; i++) {
        printf("%.1f ", floats[i]);
    }
    arenaTest();
    printf("\n");

    arena_destroy(__arena1); // Only destroys main()'s arena;
    return 0;
}
