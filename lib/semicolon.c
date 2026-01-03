// lib/semicolon.c - LINE-BASED version (simple!)
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper: Trim leading whitespace
static char *ltrim(char *s) {
    while (isspace(*s))
        s++;
    return s;
}
// Helper: Trim trailing whitespace
static char *rtrim(char *s) {
    char *end = s + strlen(s) - 1;
    while (end >= s && isspace(*end)) {
        *end = '\0';
        end--;
    }
    return s;
}
// Helper: Check if string starts with prefix
static int starts_with(const char *str, const char *prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}


void add_semicolons(FILE *in, FILE *out) {
    char line[1024];

    while (fgets(line, sizeof(line), in)) {
        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
            len--;
        }

        // Remove trailing whitespace
        rtrim(line);
        len = strlen(line);

        char *trimmed = ltrim(line);

        // Skip empty lines
        if (trimmed[0] == '\0') {
            fputs("\n", out);
            continue;
        }

        // Skip comments
        if (starts_with(trimmed, "//")) {
            fputs(line, out);
            fputc('\n', out);
            continue;
        }

        // Skip preprocessor directives
        if (trimmed[0] == '#') {
            fputs(line, out);
            fputc('\n', out);
            continue;
        }

        // Skip lines that already end with semicolon
        if (len > 0 && line[len - 1] == ';') {
            fputs(line, out);
            fputc('\n', out);
            continue;
        }

        // Skip lines that end with {
        if (len > 0 && line[len - 1] == '{') {
            fputs(line, out);
            fputc('\n', out);
            continue;
        }

        // Skip lines that start with control flow keywords
        if (starts_with(trimmed, "if ") || starts_with(trimmed, "if(") ||
            starts_with(trimmed, "for ") || starts_with(trimmed, "for(") ||
            starts_with(trimmed, "while ") || starts_with(trimmed, "while(") ||
            starts_with(trimmed, "switch ") || starts_with(trimmed, "switch(") ||
            starts_with(trimmed, "case ") || starts_with(trimmed, "default:")) {
            fputs(line, out);
            fputc('\n', out);
            continue;
        }

        // Skip lines that start with struct/union/enum
        if (starts_with(trimmed, "struct ") || starts_with(trimmed, "union ") ||
            starts_with(trimmed, "enum ") || starts_with(trimmed, "typedef ")) {
            fputs(line, out);
            fputc('\n', out);
            continue;
        }

        // Skip lines that are just }
        if (strcmp(trimmed, "}") == 0) {
            fputs(line, out);
            fputc('\n', out);
            continue;
        }

        // Check for return statements
        if (strncmp(trimmed, "return", 6) == 0 && (isspace(trimmed[6]) || trimmed[6] == '\0')) {
            fputs(line, out);
            fputs(";\n", out);
            continue;
        }

        // Check for assignments
        char *equals = strchr(trimmed, '=');
        if (equals && !strstr(trimmed, "==") && !strstr(trimmed, "!=") && !strstr(trimmed, ">=") &&
            !strstr(trimmed, "<=") && !strstr(trimmed, "+=") && !strstr(trimmed, "-=") &&
            !strstr(trimmed, "*=") && !strstr(trimmed, "/=") && !strstr(trimmed, "%=") &&
            !strstr(trimmed, "&=") && !strstr(trimmed, "|=") && !strstr(trimmed, "^=")) {
            // Assignment - add semicolon to same line
            fputs(line, out);
            fputs(";\n", out);
            continue;
        }

        // Check for function calls (has parentheses)
        if (strchr(trimmed, '(') && strchr(trimmed, ')')) {
            // Function call - add semicolon to same line
            fputs(line, out);
            fputs(";\n", out);
            continue;
        }

        // Default: keep as-is
        fputs(line, out);
        fputc('\n', out);
    }
}
