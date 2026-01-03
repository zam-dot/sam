// lexer.h - Enhanced for semicolon removal
#ifndef LEXER_H
#define LEXER_H

typedef enum {
    TOKEN_EOF,
    TOKEN_ERROR,

    // Punctuation
    TOKEN_SEMICOLON, // ;
    TOKEN_LBRACE,    // {
    TOKEN_RBRACE,    // }
    TOKEN_LPAREN,    // (
    TOKEN_RPAREN,    // )
    TOKEN_LBRACKET,  // [
    TOKEN_RBRACKET,  // ]
    TOKEN_COMMA,     // ,
    TOKEN_DOT,       // .
    TOKEN_COLON,     // :

    // Keywords
    TOKEN_INT,
    TOKEN_RETURN,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_FOR,
    TOKEN_WHILE,
    TOKEN_DO,
    TOKEN_SWITCH,
    TOKEN_CASE,
    TOKEN_DEFAULT,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_TYPEDEF,
    TOKEN_STRUCT,
    TOKEN_ENUM,
    TOKEN_UNION,
    TOKEN_CONST,
    TOKEN_VOID,
    TOKEN_CHAR,
    TOKEN_SHORT,
    TOKEN_LONG,
    TOKEN_FLOAT,
    TOKEN_DOUBLE,
    TOKEN_SIGNED,
    TOKEN_UNSIGNED,

    // Our extensions
    TOKEN_AT_C,        // @c
    TOKEN_ARENA,       // arena
    TOKEN_STRING_TYPE, // string

    // Literals
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING_LIT,
    TOKEN_CHAR_LIT,

    // Operators
    TOKEN_PLUS,         // +
    TOKEN_MINUS,        // -
    TOKEN_STAR,         // *
    TOKEN_SLASH,        // /
    TOKEN_PERCENT,      // %
    TOKEN_EQUALS,       // =
    TOKEN_PLUS_EQUALS,  // +=
    TOKEN_MINUS_EQUALS, // -=
    // ... more as needed

    TOKEN_NEWLINE, // \n
    TOKEN_OTHER    // Everything else
} TokenType;

typedef struct {
    TokenType type;
    char     *text;
    int       length;
    int       line;
    int       column;
} Token;

typedef struct {
    const char *source;
    const char *current;
    const char *start;
    int         line;
    int         column;
} Lexer;

// Public API
Lexer       lexer_create(const char *source);
Token       lexer_next(Lexer *lexer);
void        token_free(Token *token);
const char *token_type_to_string(TokenType type);

#endif
