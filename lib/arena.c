#define _POSIX_C_SOURCE 200809L
// arena.c - Enhanced arena allocator with array support
#include "arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

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

    if (arena->is_dynamic && arena->buffer) {
        free(arena->buffer);
    }
    free(arena);
}

void arena_reset(Arena *arena) {
    if (arena) arena->offset = 0;
}

void *arena_alloc(Arena *arena, size_t size) {
    if (!arena || size == 0) return NULL;

    // Align to 8 bytes
    size = (size + 7) & ~7;

    if (arena->offset + size > arena->capacity) {
        fprintf(stderr, "Arena out of memory: %zu + %zu > %zu\n", arena->offset, size,
                arena->capacity);
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

char *arena_strdup(Arena *arena, const char *str) {
    if (!str) return NULL;

    size_t len = strlen(str);
    char  *copy = arena_alloc(arena, len + 1);
    if (copy) {
        memcpy(copy, str, len);
        copy[len] = '\0';
    }
    return copy;
}

// Enhanced arena transformation that handles arrays
void fix_arena_transformations(FILE *in, FILE *out) {
    char line[1024];
    int  in_arena_declaration = 0;
    char arena_size[64] = "1024 * 1024"; // Default 1MB

    while (fgets(line, sizeof(line), in)) {
        // Check for arena declaration with size
        if (strstr(line, "arena(")) {
            in_arena_declaration = 1;

            // Extract size specification
            char *start = strchr(line, '(');
            char *end = strchr(line, ')');
            if (start && end) {
                size_t len = end - start - 1;
                if (len < sizeof(arena_size) - 1) {
                    strncpy(arena_size, start + 1, len);
                    arena_size[len] = '\0';

                    // Remove the arena(size) part
                    memmove(start, end + 1, strlen(end + 1) + 1);
                }
            }
        }

        // Transform array declarations
        char *array_keyword = strstr(line, "array ");
        if (array_keyword) {
            // Remove "array " keyword
            memmove(array_keyword, array_keyword + 6, strlen(array_keyword + 6) + 1);

            // Check if this is an array declaration with size
            char *bracket = strstr(line, "[");
            if (bracket) {
                // Transform to arena array creation
                char  new_line[1024];
                char *semicolon = strchr(line, ';');
                if (semicolon) {
                    *semicolon = '\0';

                    // Find variable name
                    char *var_start = line;
                    while (*var_start && (*var_start == ' ' || *var_start == '\t'))
                        var_start++;

                    // Skip type
                    while (*var_start && *var_start != ' ')
                        var_start++;
                    while (*var_start && *var_start == ' ')
                        var_start++;

                    char  var_name[256];
                    char *var_end = var_start;
                    while (*var_end && *var_end != '[' && *var_end != ' ')
                        var_end++;
                    strncpy(var_name, var_start, var_end - var_start);
                    var_name[var_end - var_start] = '\0';

                    // Get type
                    char type[256];
                    strncpy(type, line, var_start - line);
                    type[var_start - line] = '\0';

                    // Get array size
                    char *size_start = strchr(line, '[') + 1;
                    char *size_end = strchr(line, ']');
                    char  size_str[64];
                    strncpy(size_str, size_start, size_end - size_start);
                    size_str[size_end - size_start] = '\0';

                    // Build new line
                    if (in_arena_declaration) {
                        snprintf(new_line, sizeof(new_line), "%s %s = array_create(%s, %s);\n",
                                 type, var_name, type, size_str);
                    } else {
                        snprintf(new_line, sizeof(new_line), "%s %s = array_create_heap(%s, %s);\n",
                                 type, var_name, type, size_str);
                    }

                    strcpy(line, new_line);
                }
            }
        }

        // Check for malloc/calloc calls
        char *malloc_pos = strstr(line, "malloc(");
        char *calloc_pos = strstr(line, "calloc(");
        char *rc_alloc_pos = strstr(line, "rc_alloc(");

        if (in_arena_declaration && (malloc_pos || calloc_pos || rc_alloc_pos)) {
            char *alloc_pos = malloc_pos ? malloc_pos : (calloc_pos ? calloc_pos : rc_alloc_pos);

            if (strstr(line, "__arena")) {
                // Already using arena, skip
            } else {
                // Transform to arena allocation
                char  new_line[1024];
                char *func_name = malloc_pos ? "malloc" : (calloc_pos ? "calloc" : "rc_alloc");

                // Find the function call start
                char *paren = strchr(alloc_pos, '(');
                if (paren) {
                    int offset = alloc_pos - line;
                    strncpy(new_line, line, offset);
                    new_line[offset] = '\0';

                    // Replace with arena version
                    if (func_name[0] == 'r') { // rc_alloc
                        strcat(new_line, "rc_arena_alloc(__arena, ");
                    } else if (func_name[0] == 'c' && func_name[1] == 'a') { // calloc
                        // calloc(a, b) -> arena_alloc_zero(__arena, a * b)
                        strcat(new_line, "arena_alloc_zero(__arena, ");
                        // Need to handle multiplication of arguments
                    } else { // malloc
                        strcat(new_line, "arena_alloc(__arena, ");
                    }

                    // Copy the rest (skip the function name and opening paren)
                    strcat(new_line, paren + 1);

                    // Replace the line
                    strcpy(line, new_line);
                }
            }
        }

        fputs(line, out);

        // Reset arena declaration after semicolon
        if (strchr(line, ';')) {
            in_arena_declaration = 0;
            strcpy(arena_size, "1024 * 1024"); // Reset to default
        }
    }
}

// Enhanced arena injection with size support
void inject_arena_declaration(FILE *in, FILE *out) {
    char line[1024];
    int  arena_needed = 0;
    char arena_size[64] = "1024 * 1024"; // Default 1MB

    // First pass: check if we need arena and get size
    while (fgets(line, sizeof(line), in)) {
        if (strstr(line, "arena(")) {
            arena_needed = 1;

            // Extract size
            char *start = strchr(line, '(');
            char *end = strchr(line, ')');
            if (start && end) {
                size_t len = end - start - 1;
                if (len < sizeof(arena_size) - 1) {
                    strncpy(arena_size, start + 1, len);
                    arena_size[len] = '\0';

                    // Convert human-readable size to C expression
                    if (strstr(arena_size, "KB") || strstr(arena_size, "kb")) {
                        char num[32];
                        strncpy(num, arena_size, strcspn(arena_size, "Kk"));
                        num[strcspn(arena_size, "Kk")] = '\0';
                        snprintf(arena_size, sizeof(arena_size), "%s * 1024", num);
                    } else if (strstr(arena_size, "MB") || strstr(arena_size, "mb")) {
                        char num[32];
                        strncpy(num, arena_size, strcspn(arena_size, "Mm"));
                        num[strcspn(arena_size, "Mm")] = '\0';
                        snprintf(arena_size, sizeof(arena_size), "%s * 1024 * 1024", num);
                    } else if (strstr(arena_size, "GB") || strstr(arena_size, "gb")) {
                        char num[32];
                        strncpy(num, arena_size, strcspn(arena_size, "Gg"));
                        num[strcspn(arena_size, "Gg")] = '\0';
                        snprintf(arena_size, sizeof(arena_size), "%s * 1024 * 1024 * 1024", num);
                    }
                }
            }
        } else if (strstr(line, "arena_string_create") || strstr(line, "arena_string_concat") ||
                   strstr(line, "arena_string_substr") || strstr(line, "rc_arena_alloc") ||
                   strstr(line, "array_create(")) {
            arena_needed = 1;
        }
    }
    rewind(in);

    // Second pass: inject arena declaration with correct size
    int brace_depth = 0;
    int arena_injected = 0;

    while (fgets(line, sizeof(line), in)) {
        // Track braces
        char *ch = line;
        while (*ch) {
            if (*ch == '{') brace_depth++;
            if (*ch == '}') brace_depth--;
            ch++;
        }

        // Remove arena(size) annotations
        char *arena_annot = strstr(line, "arena(");
        if (arena_annot) {
            char *end_paren = strchr(arena_annot, ')');
            if (end_paren) {
                memmove(arena_annot, end_paren + 1, strlen(end_paren + 1) + 1);
            }
        }

        // If we're entering a function and need arena, inject declaration
        if (arena_needed && !arena_injected && brace_depth == 0) {
            char *brace = strchr(line, '{');
            if (brace) {
                // Write everything up to the brace
                *brace = '\0';
                fputs(line, out);
                fputc('{', out);

                // Inject arena declaration with specified size
                fprintf(out, "\n    Arena* __arena = arena_create(%s);\n", arena_size);
                arena_injected = 1;

                continue;
            }
        }

        fputs(line, out);
    }
}

void add_arena_cleanup(FILE *in, FILE *out) {
    char line[1024];
    int  brace_depth = 0;
    int  has_arena = 0;

    while (fgets(line, sizeof(line), in)) {
        // Check for arena declaration
        if (strstr(line, "__arena = arena_create")) {
            has_arena = 1;
        }

        // Track braces
        char *ch = line;
        while (*ch) {
            if (*ch == '{') {
                brace_depth++;
            }
            if (*ch == '}') {
                brace_depth--;
                if (brace_depth == 0 && has_arena) {
                    // At function end, add arena cleanup
                    strcat(line, "\n    if (__arena) arena_destroy(__arena);\n");
                    has_arena = 0;
                }
            }
            ch++;
        }

        fputs(line, out);
    }
}

// Helper to parse size specifications
static size_t parse_size_spec(const char *spec) {
    char *endptr;
    long  size = strtol(spec, &endptr, 10);

    if (endptr == spec) return 1024 * 1024; // Default 1MB

    while (*endptr == ' ')
        endptr++;

    if (strncasecmp(endptr, "KB", 2) == 0) return size * 1024;
    if (strncasecmp(endptr, "MB", 2) == 0) return size * 1024 * 1024;
    if (strncasecmp(endptr, "GB", 2) == 0) return size * 1024 * 1024 * 1024;

    return size; // Assume bytes
}

// ==============================================================================
void add_arena_support(FILE *in, FILE *out) {
    char line[1024];
    int  current_function_arenas = 0;
    char current_arena_vars[10][32];
    int  brace_depth = 0;
    int  in_function = 0;

    FILE *temp_out = tmpfile();
    if (!temp_out) return;

    while (fgets(line, sizeof(line), in)) {
        // Track braces to determine function boundaries
        char *ch = line;
        while (*ch) {
            if (*ch == '{') {
                brace_depth++;
                if (brace_depth == 1) {
                    // Entering a new function/scope
                    in_function = 1;
                    current_function_arenas = 0;
                }
            }
            if (*ch == '}') {
                brace_depth--;
                if (brace_depth == 0 && in_function) {
                    // Exiting a function - add arena cleanup
                    in_function = 0;
                    for (int i = current_function_arenas; i >= 1; i--) {
                        fprintf(temp_out, "    arena_destroy(%s);\n", current_arena_vars[i - 1]);
                    }
                    current_function_arenas = 0;
                }
            }
            ch++;
        }

        char *arena_pos = strstr(line, "arena(");

        if (!arena_pos) {
            // Check for return statements within current function
            char *return_pos = strstr(line, "return");
            if (return_pos && in_function && current_function_arenas > 0) {
                // Insert arena_destroy before return
                for (int i = current_function_arenas; i >= 1; i--) {
                    fprintf(temp_out, "    arena_destroy(%s);\n", current_arena_vars[i - 1]);
                }
                current_function_arenas = 0;
            }
            fputs(line, temp_out);
            continue;
        }

        // We're in a function and found arena()
        if (in_function) {
            current_function_arenas++;
        } else {
            // arena() outside any function - error or global scope
            fputs(line, temp_out);
            continue;
        }

        // Extract size
        char *size_start = arena_pos + 6;
        char *size_end = strchr(size_start, ')');
        if (!size_end) {
            fputs(line, temp_out);
            continue;
        }

        char   size_spec[32];
        size_t size_len = size_end - size_start;
        if (size_len >= sizeof(size_spec)) size_len = sizeof(size_spec) - 1;
        strncpy(size_spec, size_start, size_len);
        size_spec[size_len] = '\0';

        // Parse size
        size_t bytes = parse_size_spec(size_spec);

        // Create arena variable name for THIS function
        char arena_var[32];
        snprintf(arena_var, sizeof(arena_var), "__arena%d", current_function_arenas);
        if (in_function) {
            strcpy(current_arena_vars[current_function_arenas - 1], arena_var);
        }

        // Write everything before arena()
        *arena_pos = '\0';
        fputs(line, temp_out);

        // Create arena
        fprintf(temp_out, "Arena *%s = arena_create(%zu);\n", arena_var, bytes);

        // Process what comes after arena()
        char *after_arena = size_end + 1;

        // Look for array declaration (same as before)
        char *brackets = strstr(after_arena, "[]");
        if (brackets) {
            // Find type
            char *type_start = after_arena;
            while (*type_start == ' ' || *type_start == '\t')
                type_start++;

            char *type_end = type_start;
            while (*type_end && *type_end != ' ')
                type_end++;

            if (type_end > type_start) {
                char   type[32];
                size_t type_len = type_end - type_start;
                if (type_len >= sizeof(type)) type_len = sizeof(type) - 1;
                strncpy(type, type_start, type_len);
                type[type_len] = '\0';

                // Find variable name
                char *name_start = type_end;
                while (*name_start == ' ')
                    name_start++;

                char *name_end = name_start;
                while (*name_end && *name_end != '[')
                    name_end++;

                if (name_end > name_start) {
                    char   name[64];
                    size_t name_len = name_end - name_start;
                    if (name_len >= sizeof(name)) name_len = sizeof(name) - 1;
                    strncpy(name, name_start, name_len);
                    name[name_len] = '\0';

                    // Count elements
                    int   count = 0;
                    char *brace = strstr(name_end, "{");
                    if (brace) {
                        count = 1;
                        char *comma = brace;
                        while (*comma && *comma != '}') {
                            if (*comma == ',') count++;
                            comma++;
                        }
                    } else {
                        count = 1;
                    }

                    // Write transformed array allocation
                    fprintf(temp_out, "    %s *%s = arena_array(%s, %s, %d);\n", type, name,
                            arena_var, type, count);

                    // Handle initializer if present
                    if (brace) {
                        // Create temporary array - WITHOUT the " = " part
                        fprintf(temp_out, "    %s temp_%s[]", type, name);

                        // Find and copy the ENTIRE initializer including the "="
                        // We need to find where the initializer starts in the original line
                        char *equals_in_original = strstr(after_arena, "=");
                        if (equals_in_original) {
                            // Write from = to end of line
                            char *line_end = strchr(equals_in_original, ';');
                            if (!line_end) line_end = strchr(equals_in_original, '\n');
                            if (line_end) {
                                fwrite(equals_in_original, 1, line_end - equals_in_original,
                                       temp_out);
                            } else {
                                fputs(equals_in_original, temp_out);
                            }
                        }

                        // Add semicolon and copy loop
                        fprintf(temp_out,
                                ";\n    for (int i = 0; i < %d; i++) %s[i] = temp_%s[i];\n", count,
                                name, name);
                    }

                    continue; // Skip writing original line
                }
            }
        }

        // If not an array, write original line
        fputs(after_arena, temp_out);
    }

    // Handle case where file ends while still in a function
    if (in_function && current_function_arenas > 0) {
        fprintf(temp_out, "\n");
        for (int i = current_function_arenas; i >= 1; i--) {
            fprintf(temp_out, "    arena_destroy(%s);\n", current_arena_vars[i - 1]);
        }
    }

    // Copy to output
    rewind(temp_out);
    int ch;
    while ((ch = fgetc(temp_out)) != EOF) {
        fputc(ch, out);
    }

    fclose(temp_out);
}
