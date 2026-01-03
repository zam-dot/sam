// lib/refcount.c - Fixed with proper tracking
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// =========================== [ STRUCTS ] =========================================
typedef struct {
    char name[256];
    int  scope_depth;
    int  is_temporary;
} RefcountedVar;

typedef struct {
    // Variable tracking
    RefcountedVar *vars;
    int            var_count;
    int            var_capacity;
    int            current_scope_depth;

    // For tracking assignments
    char dest_var[256];
    char src_var[256];

    // For tracking string function returns
    char last_string_func[256]; // Last string function called
    int  in_string_func_args;   // Inside string function arguments

    // State flags
    int just_saw_string_type;
    int in_assignment;
    int expecting_var_name;

    // Output buffering
    char output_buffer[1024];
    int  buffer_pos;

    // NEW: For tracking temps in expressions
    char temp_vars[10][256]; // Stack of temporary variables
    int  temp_count;
} RefcountState;

// ============================== [ UTILITIES ] =======================================

static void init_state(RefcountState *state) {
    memset(state, 0, sizeof(RefcountState));
    state->var_capacity = 10;
    state->vars = malloc(sizeof(RefcountedVar) * state->var_capacity);
}

static void buffer_char(RefcountState *state, char ch, FILE *out) {
    if (state->buffer_pos >= (int)(sizeof(state->output_buffer) - 1)) {
        fwrite(state->output_buffer, 1, state->buffer_pos, out);
        state->buffer_pos = 0;
    }
    state->output_buffer[state->buffer_pos++] = ch;
}

static void flush_buffer(RefcountState *state, FILE *out) {
    if (state->buffer_pos > 0) {
        fwrite(state->output_buffer, 1, state->buffer_pos, out);
        state->buffer_pos = 0;
    }
}

// =========================== [ VARIABLE MANAGEMENT ] ====================================

static void add_var(RefcountState *state, const char *name, int is_temp) {
    // Resize if needed
    if (state->var_count >= state->var_capacity) {
        state->var_capacity *= 2;
        state->vars = realloc(state->vars, sizeof(RefcountedVar) * state->var_capacity);
    }

    // Add variable
    RefcountedVar *var = &state->vars[state->var_count++];
    strncpy(var->name, name, sizeof(var->name) - 1);
    var->scope_depth = state->current_scope_depth;
    var->is_temporary = is_temp;
}

static int is_refcounted_type(const char *type) { return strcmp(type, "string") == 0; }

static int is_known_var(RefcountState *state, const char *name) {
    for (int i = 0; i < state->var_count; i++) {
        if (strcmp(state->vars[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int is_string_function(const char *name) {
    return strcmp(name, "string_create") == 0 || strcmp(name, "string_concat") == 0 ||
           strcmp(name, "string_substr") == 0;
}

// =========================== [ MAIN TRANSFORMATION ] ====================================

void add_refcounting(FILE *in, FILE *out) {
    RefcountState state;
    init_state(&state);

    int  ch;
    int  prev_ch = 0;
    int  in_string = 0, in_char = 0, in_line_comment = 0, in_block_comment = 0;
    char identifier[256];
    int  ident_pos = 0;

    // For tracking parentheses in function calls
    int paren_depth = 0;

    // MAIN PARSING LOOP
    while ((ch = fgetc(in)) != EOF) {
        if (!in_line_comment && !in_block_comment && !in_char && ch == '"' && prev_ch != '\\') {
            in_string = !in_string;
        } else if (!in_string && !in_char && !in_block_comment && ch == '/' && prev_ch == '/') {
            in_line_comment = 1;
        } else if (!in_string && !in_char && !in_line_comment && ch == '*' && prev_ch == '/') {
            in_block_comment = 1;
        } else if (!in_string && !in_line_comment && ch == '\'' && prev_ch != '\\') {
            in_char = !in_char;
        } else if (ch == '\n') {
            in_line_comment = 0;
        } else if (!in_string && !in_char && in_block_comment && ch == '/' && prev_ch == '*') {
            in_block_comment = 0;
        }

        // SCOPE TRACKING
        if (!in_string && !in_char && !in_line_comment && !in_block_comment) {
            if (ch == '{') {
                state.current_scope_depth++;
            } else if (ch == '}') {
                // Add rc_release() for all variables in this scope
                flush_buffer(&state, out);
                for (int i = 0; i < state.var_count; i++) {
                    if (state.vars[i].scope_depth == state.current_scope_depth &&
                        !state.vars[i].is_temporary) {
                        fprintf(out, "\n    rc_release(%s);", state.vars[i].name);
                    }
                }

                // Remove variables that went out of scope
                int new_count = 0;
                for (int i = 0; i < state.var_count; i++) {
                    if (state.vars[i].scope_depth != state.current_scope_depth) {
                        state.vars[new_count++] = state.vars[i];
                    }
                }
                state.var_count = new_count;
                state.current_scope_depth--;
            }
        }

        // IDENTIFIER PARSING
        if (!in_string && !in_char && !in_line_comment && !in_block_comment) {
            if (isalpha(ch) || ch == '_' || (ident_pos > 0 && isdigit(ch))) {
                if (ident_pos < 255) {
                    identifier[ident_pos++] = ch;
                }
            } else {
                // Process completed identifier
                if (ident_pos > 0) {
                    identifier[ident_pos] = '\0';

                    // Track parentheses for function calls
                    if (ch == '(') {
                        if (is_string_function(identifier)) {
                            state.in_string_func_args = 1;
                            strcpy(state.last_string_func, identifier);
                        }
                        paren_depth++;
                    }

                    // CASE 1: "string" type keyword
                    if (is_refcounted_type(identifier)) {
                        state.just_saw_string_type = 1;
                        state.expecting_var_name = 1;
                    }
                    // CASE 2: Variable name after "string" type
                    else if (state.expecting_var_name) {
                        strcpy(state.dest_var, identifier);
                        add_var(&state, identifier, 0);
                        state.expecting_var_name = 0;
                        state.just_saw_string_type = 0;
                    }
                    // CASE 3: Variable in assignment RHS
                    else if (state.in_assignment && is_known_var(&state, identifier)) {
                        strcpy(state.src_var, identifier);
                    }
                    // CASE 4: Known variable (could be LHS of future assignment)
                    else if (is_known_var(&state, identifier)) {
                        strcpy(state.dest_var, identifier);
                    }

                    ident_pos = 0;
                }

                // Handle parentheses closing
                if (ch == ')') {
                    paren_depth--;
                    if (paren_depth == 0) {
                        state.in_string_func_args = 0;
                        if (state.in_assignment && strlen(state.last_string_func) > 0) {
                            memset(state.src_var, 0, sizeof(state.src_var));
                        }
                    }
                }

                // Handle assignment operator
                if (ch == '=' && strlen(state.dest_var) > 0) {
                    state.in_assignment = 1;
                }
                // Handle statement end
                else if (ch == ';') {
                    if (state.in_assignment) {
                        // Check if we need rc_retain
                        int need_retain = 0;

                        // Need retain if: dest = src (where src is a refcounted variable)
                        if (strlen(state.src_var) > 0) {
                            if (is_known_var(&state, state.src_var)) {
                                need_retain = 1;
                            }
                        }

                        if (need_retain) {
                            buffer_char(&state, ch, out);
                            flush_buffer(&state, out);
                            fprintf(out, "\n    rc_retain(%s);", state.src_var);
                            ch = 0;
                        }
                    }

                    // Reset state
                    state.in_assignment = 0;
                    state.expecting_var_name = 0;
                    memset(state.dest_var, 0, sizeof(state.dest_var));
                    memset(state.src_var, 0, sizeof(state.src_var));
                }
            }
        }

        // Output the character
        if (ch != 0) {
            buffer_char(&state, ch, out);
        }

        prev_ch = ch;
    }

    // Flush any remaining buffered output
    flush_buffer(&state, out);
    free(state.vars);
}
