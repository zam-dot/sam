#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stddef.h>

// Allocator interface
typedef enum {
    ALLOC_STANDARD, // Use malloc/free
    ALLOC_ARENA,    // Use arena allocator
    ALLOC_REFCOUNT  // Use refcounting (default)
} AllocatorMode;

// Initialize allocator system
void allocator_init(AllocatorMode mode, size_t arena_size);

// Allocation functions
void *allocator_alloc(size_t size);
void *allocator_alloc_array(size_t elem_size, size_t count);
void  allocator_free(void *ptr);

// String allocation
char *allocator_strdup(const char *str);

// Cleanup
void allocator_cleanup(void);
void allocator_reset(void);

#endif // ALLOCATOR_H
