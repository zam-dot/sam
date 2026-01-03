// arena.h
#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdio.h>

typedef struct Arena Arena;

// Create/destroy
Arena *arena_create(size_t capacity);
void   arena_destroy(Arena *arena);
void   arena_reset(Arena *arena);

// Allocation
void *arena_alloc(Arena *arena, size_t size);
void *arena_alloc_zero(Arena *arena, size_t size);

// String allocation
char         *arena_strdup(Arena *arena, const char *str);
void          add_arena_support(FILE *in, FILE *out); // NEW
static size_t parse_size_spec(const char *spec);

#endif // ARENA_H
