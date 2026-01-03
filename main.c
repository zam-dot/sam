// main.c - Updated with proper file handling
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

// Function declarations
void add_semicolons(FILE *in, FILE *out);
void transform_strings(FILE *in, FILE *out);
void add_refcounting(FILE *in, FILE *out);
void add_arena_support(FILE *in, FILE *out);
// Helper to ensure directory exists
int ensure_dir(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0755) == -1) {
            perror("Failed to create directory");
            return 0;
        }
    }
    return 1;
}


// Inline runtime (same as before, includes arena functions)
static const char *inline_runtime =
    "#include <stdio.h>\n"
    "#include <stdlib.h>\n"
    "#include <string.h>\n"
    "\n"
    "// ========== ARENA ALLOCATOR ==========\n"
    "typedef struct Arena Arena;\n"
    "struct Arena {\n"
    "    unsigned char *buffer;\n"
    "    size_t offset;\n"
    "    size_t capacity;\n"
    "    int is_dynamic;\n"
    "};\n"
    "\n"
    "Arena *arena_create(size_t capacity) {\n"
    "    Arena *arena = malloc(sizeof(Arena));\n"
    "    if (!arena) return NULL;\n"
    "    arena->buffer = malloc(capacity);\n"
    "    if (!arena->buffer) { free(arena); return NULL; }\n"
    "    arena->offset = 0;\n"
    "    arena->capacity = capacity;\n"
    "    arena->is_dynamic = 1;\n"
    "    return arena;\n"
    "}\n"
    "\n"
    "void arena_destroy(Arena *arena) {\n"
    "    if (!arena) return;\n"
    "    if (arena->is_dynamic && arena->buffer) free(arena->buffer);\n"
    "    free(arena);\n"
    "}\n"
    "\n"
    "void arena_reset(Arena *arena) {\n"
    "    if (arena) arena->offset = 0;\n"
    "}\n"
    "\n"
    "void *arena_alloc(Arena *arena, size_t size) {\n"
    "    if (!arena || size == 0) return NULL;\n"
    "    size = (size + 7) & ~7; // Align to 8 bytes\n"
    "    if (arena->offset + size > arena->capacity) {\n"
    "        fprintf(stderr, \"Arena out of memory\\n\");\n"
    "        return NULL;\n"
    "    }\n"
    "    void *ptr = arena->buffer + arena->offset;\n"
    "    arena->offset += size;\n"
    "    return ptr;\n"
    "}\n"
    "\n"
    "void *arena_alloc_zero(Arena *arena, size_t size) {\n"
    "    void *ptr = arena_alloc(arena, size);\n"
    "    if (ptr) memset(ptr, 0, size);\n"
    "    return ptr;\n"
    "}\n"
    "\n"
    "#define arena_array(arena, type, count) ((type*)arena_alloc_zero(arena, sizeof(type) * "
    "(count)))\n"
    "\n"
    "// ========== REFCOUNTING RUNTIME ==========\n"
    "typedef struct RCHeader {\n"
    "    size_t refcount;\n"
    "    size_t weak_count;\n"
    "    size_t array_count;\n"
    "} RCHeader;\n"
    "\n"
    "#define RC_HEADER_SIZE sizeof(RCHeader)\n"
    "#define RC_GET_HEADER(ptr) ((RCHeader *)((char *)(ptr) - RC_HEADER_SIZE))\n"
    "\n"
    "void *rc_alloc(size_t size) {\n"
    "    RCHeader *header = (RCHeader *)calloc(1, RC_HEADER_SIZE + size);\n"
    "    if (header) header->refcount = 1;\n"
    "    return header ? (char *)header + RC_HEADER_SIZE : NULL;\n"
    "}\n"
    "\n"
    "void rc_retain(void *ptr) {\n"
    "    if (ptr) RC_GET_HEADER(ptr)->refcount++;\n"
    "}\n"
    "\n"
    "void rc_release(void *ptr) {\n"
    "    if (!ptr) return;\n"
    "    RCHeader *header = RC_GET_HEADER(ptr);\n"
    "    if (--header->refcount == 0 && header->weak_count == 0) {\n"
    "        free(header);\n"
    "    }\n"
    "}\n"
    "\n"
    "// ========== STRING API ==========\n"
    "typedef char *string;\n"
    "\n"
    "string string_create(const char *literal) {\n"
    "    if (!literal) return NULL;\n"
    "    size_t len = strlen(literal);\n"
    "    char *str = rc_alloc(len + 1);\n"
    "    if (str) strcpy(str, literal);\n"
    "    return str;\n"
    "}\n"
    "\n"
    "string string_concat(string a, string b) {\n"
    "    if (!a || !b) return NULL;\n"
    "    size_t len_a = strlen(a);\n"
    "    size_t len_b = strlen(b);\n"
    "    char *result = rc_alloc(len_a + len_b + 1);\n"
    "    if (result) {\n"
    "        strcpy(result, a);\n"
    "        strcpy(result + len_a, b);\n"
    "    }\n"
    "    return result;\n"
    "}\n"
    "\n"
    "string string_substr(string s, size_t start, size_t len) {\n"
    "    if (!s) return NULL;\n"
    "    size_t s_len = strlen(s);\n"
    "    if (start >= s_len) return string_create(\"\");\n"
    "    if (start + len > s_len) len = s_len - start;\n"
    "    char *result = rc_alloc(len + 1);\n"
    "    if (result) {\n"
    "        strncpy(result, s + start, len);\n"
    "        result[len] = '\\0';\n"
    "    }\n"
    "    return result;\n"
    "}\n"
    "\n"
    "size_t string_length(string s) { return s ? strlen(s) : 0; }\n"
    "void string_free(string s) { rc_release(s); }\n"
    "\n"
    "// ========== USER CODE STARTS HERE ==========\n";


int main(int argc, char **argv) {
    if (argc == 1) {
        printf("Usage: %s [options] <input.sam> [output.c]\n", argv[0]);
        printf("Options:\n");
        printf("  --run, --tcc   Transpile and run with tcc\n");
        printf("  --help, -h     Show this help\n");
        printf("\nExamples:\n");
        printf("  %s program.sam               # Transpile to output/out.c\n", argv[0]);
        printf("  %s program.sam output.c      # Transpile to output.c\n", argv[0]);
        printf("  %s --run program.sam         # Transpile and run with tcc\n", argv[0]);
        return 1;
    }

    // Parse arguments
    int         run_with_tcc = 0;
    const char *input_file = NULL;
    const char *output_file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--run") == 0 || strcmp(argv[i], "--tcc") == 0) {
            run_with_tcc = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [options] <input.sam> [output.c]\n", argv[0]);
            printf("Options:\n");
            printf("  --run, --tcc   Transpile and run with tcc\n");
            printf("  --help, -h     Show this help\n");
            return 0;
        } else if (!input_file) {
            input_file = argv[i];
        } else if (!output_file) {
            output_file = argv[i];
        }
    }

    if (!input_file) {
        fprintf(stderr, "Error: No input file specified\n");
        return 1;
    }

    // Set default output file if not specified
    if (!output_file) {
        if (run_with_tcc) {
            // Use temp file for --run mode
            output_file = "/tmp/sam_temp.c";
        } else {
            output_file = "output/out.c";
        }
    }

    // Ensure output directory exists (for non-temp files)
    if (!run_with_tcc || strcmp(output_file, "/tmp/sam_temp.c") != 0) {
        char output_dir[256];
        strncpy(output_dir, output_file, sizeof(output_dir));
        char *last_slash = strrchr(output_dir, '/');
        if (last_slash) {
            *last_slash = '\0';
            if (!ensure_dir(output_dir)) {
                return 1;
            }
        }
    }

    // Open input file
    FILE *in = fopen(input_file, "r");
    if (!in) {
        fprintf(stderr, "Error: Cannot open input '%s'\n", input_file);
        return 1;
    }

    // Open output file
    FILE *out = fopen(output_file, "w");
    if (!out) {
        fprintf(stderr, "Error: Cannot open output '%s'\n", output_file);
        fclose(in);
        return 1;
    }

    // Create temporary files for each stage
    FILE *temp1 = tmpfile();
    FILE *temp2 = tmpfile();
    FILE *temp3 = tmpfile();
    FILE *temp4 = tmpfile();

    if (!temp1 || !temp2 || !temp3 || !temp4) {
        fprintf(stderr, "Error: Cannot create temp files\n");
        fclose(in);
        fclose(out);
        if (temp1) fclose(temp1);
        if (temp2) fclose(temp2);
        if (temp3) fclose(temp3);
        if (temp4) fclose(temp4);
        return 1;
    }

    int ch;

    // 1. add_semicolons - FIRST to ensure all statements end properly
    rewind(in);
    add_semicolons(in, temp1);

    // 2. transform_strings
    rewind(temp1);
    transform_strings(temp1, temp2);

    // 3. add_arena_support - works on code with semicolons
    rewind(temp2);
    add_arena_support(temp2, temp3);

    // 4. add_refcounting
    rewind(temp3);
    add_refcounting(temp3, temp4);

    // Debug: Show what was produced
    rewind(temp4);
    while ((ch = fgetc(temp4)) != EOF)
        putchar(ch);
    rewind(temp4);

    // Write inline runtime to output
    fprintf(out, "%s", inline_runtime);

    // Copy transpiled user code
    rewind(temp4);
    while ((ch = fgetc(temp4)) != EOF) {
        fputc(ch, out);
    }

    // Cleanup - close ALL files
    fclose(in);
    fclose(out);
    fclose(temp1);
    fclose(temp2);
    fclose(temp3);
    fclose(temp4);

    // If --run mode, execute with tcc
    if (run_with_tcc) {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "tcc -run %s", output_file);
        int result = system(cmd);

        // Clean up temp file if we created one
        if (strcmp(output_file, "/tmp/sam_temp.c") == 0) {
            remove(output_file);
        }
        return result;
    }

    printf("Done! Created %s\n", output_file);
    return 0;
}
