// lib/string_transform.c - Complete with fixes
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int in_string_lit;
    int in_char_lit;
    int in_line_comment;
    int in_block_comment;
    int in_preprocessor;
    int prev_char;
    int column;
    int line;

    // For identifier tracking
    char identifier[256];
    int  ident_pos;

    // NEW: Track string functions to prevent double-wrapping
    int in_string_func;   // Inside string_create/string_concat/string_substr
    int func_paren_depth; // Parentheses depth in string function
    int skip_next_string; // Skip wrapping next string literal

    // NEW: Track printf-like functions
    int  in_printf_func; // Inside printf/sprintf/fprintf
    char last_func[32];  // Last function name seen
} TransformState;

void transform_strings(FILE *in, FILE *out) {
    TransformState state;
    memset(&state, 0, sizeof(TransformState));

    int ch;
    while ((ch = fgetc(in)) != EOF) {
        // Track preprocessor lines
        if (ch == '#' && state.column == 0 && !state.in_string_lit && !state.in_char_lit &&
            !state.in_line_comment && !state.in_block_comment) {
            state.in_preprocessor = 1;
        }
        if (ch == '\n') {
            state.in_preprocessor = 0;
        }

        // Track comments and character literals
        if (!state.in_string_lit && !state.in_char_lit && !state.in_block_comment &&
            !state.in_line_comment) {
            if (ch == '/' && state.prev_char == '/') {
                state.in_line_comment = 1;
            } else if (ch == '*' && state.prev_char == '/') {
                state.in_block_comment = 1;
            } else if (ch == '\'' && state.prev_char != '\\') {
                state.in_char_lit = 1;
            }
        }

        if (ch == '\n') state.in_line_comment = 0;
        if (ch == '/' && state.prev_char == '*') state.in_block_comment = 0;
        if (ch == '\'' && state.prev_char != '\\') state.in_char_lit = !state.in_char_lit;

        // Track identifiers (function names)
        if (!state.in_string_lit && !state.in_char_lit && !state.in_line_comment &&
            !state.in_block_comment &&
            (isalpha(ch) || ch == '_' || (state.ident_pos > 0 && isdigit(ch)))) {
            // Building identifier
            if (state.ident_pos < 255) {
                state.identifier[state.ident_pos++] = ch;
            }
        } else if (state.ident_pos > 0) {
            // Identifier completed
            state.identifier[state.ident_pos] = '\0';

            // Check if it's a string function
            if (strcmp(state.identifier, "string_create") == 0 ||
                strcmp(state.identifier, "string_concat") == 0 ||
                strcmp(state.identifier, "string_substr") == 0) {
                state.in_string_func = 1;
                state.func_paren_depth = 0;
                state.skip_next_string = 1; // Don't wrap next string literal
            }
            // Check if it's a printf-like function
            else if (strcmp(state.identifier, "printf") == 0 ||
                     strcmp(state.identifier, "sprintf") == 0 ||
                     strcmp(state.identifier, "fprintf") == 0 ||
                     strcmp(state.identifier, "snprintf") == 0) {
                state.in_printf_func = 1;
            }

            // Store last function name
            strncpy(state.last_func, state.identifier, sizeof(state.last_func) - 1);
            state.last_func[sizeof(state.last_func) - 1] = '\0';

            // Write the identifier
            fputs(state.identifier, out);
            state.ident_pos = 0;
        }

        // Track parentheses for function calls
        if (!state.in_string_lit && !state.in_char_lit && !state.in_line_comment &&
            !state.in_block_comment) {
            if (ch == '(') {
                if (state.in_string_func) {
                    state.func_paren_depth++;
                }
                // Reset printf flag after opening paren (args start)
                if (state.in_printf_func) {
                    // printf( format, ... ) - format is first arg
                    // We're about to see the format string
                }
            } else if (ch == ')') {
                if (state.in_string_func && state.func_paren_depth > 0) {
                    state.func_paren_depth--;
                    if (state.func_paren_depth == 0) {
                        state.in_string_func = 0;
                        state.skip_next_string = 0;
                    }
                }
                if (state.in_printf_func) {
                    state.in_printf_func = 0;
                }
            } else if (ch == ',' && state.in_printf_func) {
                // After first comma in printf, don't wrap strings (they're not format strings)
                state.in_printf_func = 0;
            }
        }

        // Handle string literals
        if (!state.in_line_comment && !state.in_block_comment && !state.in_char_lit &&
            !state.in_preprocessor && ch == '"' && state.prev_char != '\\') {

            if (!state.in_string_lit) {
                // Opening quote
                if (state.skip_next_string || state.in_string_func) {
                    // Already inside string_create() or similar - don't wrap again
                    fputc('"', out);
                    state.skip_next_string = 0;
                } else if (state.in_printf_func && strcmp(state.last_func, "printf") == 0) {
                    // printf format string - don't wrap (printf expects char*)
                    fputc('"', out);
                } else {
                    // Normal string literal - wrap with string_create
                    fputs("string_create(\"", out);
                }
                state.in_string_lit = 1;
            } else {
                // Closing quote
                if (state.skip_next_string || state.in_string_func) {
                    // Wasn't wrapped
                    fputc('"', out);
                } else if (state.in_printf_func && strcmp(state.last_func, "printf") == 0) {
                    // Wasn't wrapped
                    fputc('"', out);
                } else {
                    // Was wrapped - close the function call
                    fputc('"', out);
                    fputc(')', out);
                }
                state.in_string_lit = 0;
                state.skip_next_string = 0;
            }
        } else {
            // Output current character (if not part of string handling)
            if (!state.in_string_lit || ch != '"') {
                // If we're in the middle of outputting an identifier, it was handled above
                if (!(state.ident_pos > 0 && (isalpha(ch) || ch == '_' || isdigit(ch)))) {
                    fputc(ch, out);
                }
            }
        }

        // Update state
        state.prev_char = ch;
        state.column++;
        if (ch == '\n') {
            state.line++;
            state.column = 0;
            // Reset function tracking at newline (simplification)
            if (state.in_printf_func) state.in_printf_func = 0;
        }
    }

    // Handle any remaining identifier
    if (state.ident_pos > 0) {
        state.identifier[state.ident_pos] = '\0';
        fputs(state.identifier, out);
    }
}
