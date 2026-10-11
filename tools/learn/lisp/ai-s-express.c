#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

/* Token Type Enumeration */
typedef enum {
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_SYMBOL,
    TOKEN_STRING,
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

/* Token Structure */
typedef struct {
    TokenType type;
    const char *value;
    size_t length;
} Token;

/* Tokenizer State Structure */
typedef struct {
    const char *source;
    size_t cursor;
    size_t line;
} Tokenizer;

/* Initialize Tokenizer */
void tokenizer_init(Tokenizer *t, const char *source) {
    t->source = source;
    t->cursor = 0;
    t->line = 1;
}

/* Skip Whitespace and Comments */
static void tokenizer_skip_non_code(Tokenizer *t) {
    while (t->source[t->cursor] != '\0') {
        char c = t->source[t->cursor];
        if (c == ' ' || c == '\t' || c == '\r') {
            t->cursor++;
        } else if (c == '\n') {
            t->line++;
            t->cursor++;
        } else if (c == ';') { /* Lisp Comment Handling */
            while (t->source[t->cursor] != '\0' && t->source[t->cursor] != '\n') {
                t->cursor++;
            }
        } else {
            break;
        }
    }
}

/* Fetch Next Token */
Token tokenizer_next(Tokenizer *t) {
    tokenizer_skip_non_code(t);
    Token tok;
    tok.value = &t->source[t->cursor];
    tok.length = 0;
    char c = t->source[t->cursor];
    if (c == '\0') {
        tok.type = TOKEN_EOF;
        return tok;
    }
    /* Single Character Tokens */
    if (c == '(') {
        t->cursor++;
        tok.type = TOKEN_LPAREN;
        tok.length = 1;
        return tok;
    }
    if (c == ')') {
        t->cursor++;
        tok.type = TOKEN_RPAREN;
        tok.length = 1;
        return tok;
    }
    /* String Literals */
    if (c == '"') {
        t->cursor++;
        tok.value = &t->source[t->cursor];
        while (t->source[t->cursor] != '\0' && t->source[t->cursor] != '"') {
            if (t->source[t->cursor] == '\\' && t->source[t->cursor + 1] != '\0') {
                t->cursor++; /* Skip escaped character */
            }
            t->cursor++;
        }
        if (t->source[t->cursor] == '"') {
            tok.type = TOKEN_STRING;
            tok.length = &t->source[t->cursor] - tok.value;
            t->cursor++; /* Consume closing quote */
            return tok;
        } else {
            tok.type = TOKEN_ERROR; /* Unterminated string */
            return tok;
        }
    }
    /* Symbols and Identifiers */
    tok.type = TOKEN_SYMBOL;
    size_t start = t->cursor;
    while (t->source[t->cursor] != '\0' &&
           !isspace((unsigned char)t->source[t->cursor]) &&
           t->source[t->cursor] != '(' &&
           t->source[t->cursor] != ')' &&
           t->source[t->cursor] != ';') {
        t->cursor++;
    }
    tok.length = t->cursor - start;
    return tok;
}

/* Example AST Driver Loop */
int main(void) {
    const char *script = 
        ";; Sample S-Expression Input\n"
        "(set-variable \"tab-width\" \"4\")\n"
        "(define-key \"C-c C-c\" \"compile-buffer\")\n";
    Tokenizer t;
    tokenizer_init(&t, script);
    Token tok;
    printf("--- Beginning Tokenization ---\n");
    do {
        tok = tokenizer_next(&t);
        switch (tok.type) {
            case TOKEN_LPAREN: 
                printf("TOKEN: LPAREN  | '('\n"); 
                break;
            case TOKEN_RPAREN: 
                printf("TOKEN: RPAREN  | ')'\n"); 
                break;
            case TOKEN_STRING: 
                printf("TOKEN: STRING  | %.*s\n", (int)tok.length, tok.value); 
                break;
            case TOKEN_SYMBOL: 
                printf("TOKEN: SYMBOL  | %.*s\n", (int)tok.length, tok.value); 
                break;
            case TOKEN_ERROR:  
                printf("TOKEN: ERROR   | Lexical error on line %zu\n", t.line); 
                break;
            case TOKEN_EOF:    
                printf("TOKEN: EOF\n"); 
                break;
        }
    } while (tok.type != TOKEN_EOF && tok.type != TOKEN_ERROR);
    return 0;
}
