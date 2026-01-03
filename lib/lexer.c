#define _POSIX_C_SOURCE 200809L

// lib/lexer.c - Fixed to preserve newlines
#include "lexer.h"
#include <stdlib.h>
#include <string.h>

Lexer lexer_create(const char *source) {
    Lexer lexer;
    lexer.source = source;
    lexer.current = source;
    lexer.line = 1;
    lexer.column = 1;
    return lexer;
}

Token lexer_next(Lexer *lexer) {
    Token token;

    // Skip whitespace EXCEPT newlines
    while (*lexer->current == ' ' || *lexer->current == '\t' || *lexer->current == '\r') {
        lexer->column++;
        lexer->current++;
    }

    // Handle newline specially
    if (*lexer->current == '\n') {
        token.type = 2; // NEWLINE token type
        token.text = strdup("\n");
        token.length = 1;
        token.line = lexer->line;
        token.column = lexer->column;

        lexer->line++;
        lexer->column = 1;
        lexer->current++;

        return token;
    }

    if (*lexer->current == '\0') {
        token.type = 0; // EOF
        token.text = strdup("");
        token.length = 0;
        token.line = lexer->line;
        token.column = lexer->column;
        return token;
    }

    // Simple: just return next character as a token
    char ch = *lexer->current;
    lexer->current++;
    lexer->column++;

    token.text = malloc(2);
    token.text[0] = ch;
    token.text[1] = '\0';
    token.length = 1;
    token.line = lexer->line;
    token.column = lexer->column - 1;
    token.type = 1; // CHAR token type

    return token;
}

void token_free(Token *token) { free(token->text); }
