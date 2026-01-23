/*
 * NatureLang Compiler
 * Copyright (c) 2026
 * 
 * Token Definitions Header
 * 
 * This file defines all tokens used by the NatureLang lexer and parser.
 * Tokens include keywords, operators, literals, and special symbols.
 */

#ifndef NATURELANG_TOKENS_H
#define NATURELANG_TOKENS_H

#include <stddef.h>

/* ============================================================================
 * TOKEN TYPE ENUMERATION
 * ============================================================================
 * All possible token types in NatureLang
 */
typedef enum {
    /* End of file / Error tokens */
    TOK_EOF = 0,
    TOK_ERROR,
    TOK_UNKNOWN,
    
    /* ========== KEYWORDS - Declaration ========== */
    TOK_CREATE,         /* "create" */
    TOK_A,              /* "a" */
    TOK_AN,             /* "an" */
    TOK_CALLED,         /* "called" */
    TOK_NAMED,          /* "named" */
    TOK_AND,            /* "and" */
    TOK_SET,            /* "set" */
    TOK_IT,             /* "it" */
    TOK_TO,             /* "to" */
    TOK_AS,             /* "as" */
    
    /* ========== KEYWORDS - Assignment ========== */
    TOK_BECOMES,        /* "becomes" */
    TOK_EQUALS,         /* "equals" (alternative) */
    TOK_MAKE,           /* "make" */
    TOK_EQUAL,          /* "equal" */
    
    /* ========== KEYWORDS - Types ========== */
    TOK_TYPE_NUMBER,    /* "number" */
    TOK_TYPE_TEXT,      /* "text" */
    TOK_TYPE_DECIMAL,   /* "decimal" */
    TOK_TYPE_FLAG,      /* "flag" */
    TOK_TYPE_LIST,      /* "list" */
    TOK_TYPE_NOTHING,   /* "nothing" (void) */
    
    /* ========== KEYWORDS - Control Flow ========== */
    TOK_IF,             /* "if" */
    TOK_THEN,           /* "then" */
    TOK_OTHERWISE,      /* "otherwise" */
    TOK_ELSE,           /* "else" */
    TOK_END,            /* "end" */
    TOK_REPEAT,         /* "repeat" */
    TOK_TIMES,          /* "times" */
    TOK_WHILE,          /* "while" */
    TOK_DO,             /* "do" */
    TOK_FOR,            /* "for" */
    TOK_EACH,           /* "each" */
    TOK_IN,             /* "in" */
    TOK_FROM,           /* "from" */
    TOK_UNTIL,          /* "until" */
    TOK_STOP,           /* "stop" (break) */
    TOK_SKIP,           /* "skip" (continue) */
    
    /* ========== KEYWORDS - Functions ========== */
    TOK_DEFINE,         /* "define" */
    TOK_FUNCTION,       /* "function" */
    TOK_THAT,           /* "that" */
    TOK_TAKES,          /* "takes" */
    TOK_RETURNS,        /* "returns" */
    TOK_GIVE,           /* "give" */
    TOK_BACK,           /* "back" */
    TOK_CALL,           /* "call" */
    TOK_WITH,           /* "with" */
    
    /* ========== KEYWORDS - I/O ========== */
    TOK_DISPLAY,        /* "display" */
    TOK_SHOW,           /* "show" */
    TOK_PRINT,          /* "print" */
    TOK_ASK,            /* "ask" */
    TOK_READ,           /* "read" */
    TOK_REMEMBER,       /* "remember" */
    TOK_SAVE,           /* "save" */
    TOK_INTO,           /* "into" */
    
    /* ========== KEYWORDS - Security ========== */
    TOK_ENTER,          /* "enter" */
    TOK_SECURE,         /* "secure" */
    TOK_ZONE,           /* "zone" */
    TOK_SAFE,           /* "safe" */
    
    /* ========== KEYWORDS - Logical ========== */
    TOK_IS,             /* "is" */
    TOK_NOT,            /* "not" */
    TOK_OR,             /* "or" */
    TOK_TRUE,           /* "true" */
    TOK_FALSE,          /* "false" */
    TOK_YES,            /* "yes" */
    TOK_NO,             /* "no" */
    
    /* ========== KEYWORDS - Comparison (multi-word handled specially) ========== */
    TOK_GREATER,        /* "greater" */
    TOK_LESS,           /* "less" */
    TOK_THAN,           /* "than" */
    TOK_GREATER_THAN,   /* "greater than" (combined) */
    TOK_LESS_THAN,      /* "less than" (combined) */
    TOK_EQUAL_TO,       /* "equal to" (combined) */
    TOK_NOT_EQUAL_TO,   /* "not equal to" (combined) */
    TOK_AT_LEAST,       /* "at least" (>=) */
    TOK_AT_MOST,        /* "at most" (<=) */
    
    /* ========== KEYWORDS - Arithmetic (natural words) ========== */
    TOK_PLUS,           /* "plus" */
    TOK_MINUS,          /* "minus" */
    TOK_MULTIPLIED,     /* "multiplied" */
    TOK_DIVIDED,        /* "divided" */
    TOK_BY,             /* "by" */
    TOK_MODULO,         /* "modulo" */
    TOK_REMAINDER,      /* "remainder" */
    TOK_OF,             /* "of" */
    TOK_POWER,          /* "power" */
    TOK_SQUARED,        /* "squared" */
    TOK_SQUARE,         /* "square" */
    TOK_ROOT,           /* "root" */
    
    /* ========== OPERATORS - Symbolic ========== */
    TOK_OP_PLUS,        /* "+" */
    TOK_OP_MINUS,       /* "-" */
    TOK_OP_STAR,        /* "*" */
    TOK_OP_SLASH,       /* "/" */
    TOK_OP_PERCENT,     /* "%" */
    TOK_OP_CARET,       /* "^" (power) */
    TOK_OP_EQ,          /* "=" */
    TOK_OP_EQEQ,        /* "==" */
    TOK_OP_NEQ,         /* "!=" */
    TOK_OP_LT,          /* "<" */
    TOK_OP_GT,          /* ">" */
    TOK_OP_LTE,         /* "<=" */
    TOK_OP_GTE,         /* ">=" */
    TOK_OP_AND,         /* "&&" */
    TOK_OP_OR,          /* "||" */
    TOK_OP_NOT,         /* "!" */
    TOK_OP_ARROW,       /* "->" */
    TOK_OP_COLON,       /* ":" */
    
    /* ========== PUNCTUATION ========== */
    TOK_LPAREN,         /* "(" */
    TOK_RPAREN,         /* ")" */
    TOK_LBRACKET,       /* "[" */
    TOK_RBRACKET,       /* "]" */
    TOK_LBRACE,         /* "{" */
    TOK_RBRACE,         /* "}" */
    TOK_COMMA,          /* "," */
    TOK_DOT,            /* "." */
    TOK_SEMICOLON,      /* ";" */
    TOK_NEWLINE,        /* "\n" (significant in some contexts) */
    
    /* ========== LITERALS ========== */
    TOK_INTEGER,        /* Integer literal: 123, -45 */
    TOK_FLOAT,          /* Float literal: 3.14, -2.5 */
    TOK_STRING,         /* String literal: "hello" */
    TOK_CHAR,           /* Character literal: 'a' */
    
    /* ========== IDENTIFIERS ========== */
    TOK_IDENTIFIER,     /* User-defined names */
    
    /* ========== COMMENTS ========== */
    TOK_COMMENT,        /* -- single line comment */
    TOK_BLOCK_COMMENT,  /* {- block comment -} */
    
    /* Marker for token count */
    TOK_COUNT
} TokenType;

/* ============================================================================
 * SOURCE LOCATION
 * ============================================================================
 * Tracks where a token appears in source code for error reporting
 */
typedef struct {
    const char *filename;   /* Source file name */
    int first_line;         /* Starting line (1-based) */
    int first_column;       /* Starting column (1-based) */
    int last_line;          /* Ending line */
    int last_column;        /* Ending column */
} SourceLocation;

/* ============================================================================
 * TOKEN STRUCTURE
 * ============================================================================
 * Complete token with type, value, and location information
 */
typedef struct {
    TokenType type;         /* Type of token */
    
    /* Token value (union for different literal types) */
    union {
        long long int_value;    /* For TOK_INTEGER */
        double float_value;     /* For TOK_FLOAT */
        char *string_value;     /* For TOK_STRING, TOK_IDENTIFIER, TOK_ERROR */
        char char_value;        /* For TOK_CHAR */
    } value;
    
    char *lexeme;           /* Original text from source */
    SourceLocation loc;     /* Location in source */
} Token;

/* ============================================================================
 * TOKEN UTILITY FUNCTIONS
 * ============================================================================
 */

/* Create a new token with the given type and location */
Token *token_create(TokenType type, const char *lexeme, SourceLocation loc);

/* Create a token with integer value */
Token *token_create_int(long long value, const char *lexeme, SourceLocation loc);

/* Create a token with float value */
Token *token_create_float(double value, const char *lexeme, SourceLocation loc);

/* Create a token with string value */
Token *token_create_string(const char *value, const char *lexeme, SourceLocation loc);

/* Create an identifier token */
Token *token_create_identifier(const char *name, SourceLocation loc);

/* Create an error token with message */
Token *token_create_error(const char *message, SourceLocation loc);

/* Free a token and its associated memory */
void token_free(Token *token);

/* Get string representation of token type */
const char *token_type_to_string(TokenType type);

/* Get a human-readable description of token type */
const char *token_type_description(TokenType type);

/* Print token for debugging */
void token_print(const Token *token);

/* Print token in detailed format */
void token_print_debug(const Token *token);

/* Check if token is a keyword */
int token_is_keyword(TokenType type);

/* Check if token is an operator */
int token_is_operator(TokenType type);

/* Check if token is a literal */
int token_is_literal(TokenType type);

/* Check if token is a type keyword */
int token_is_type(TokenType type);

/* ============================================================================
 * KEYWORD LOOKUP TABLE
 * ============================================================================
 * For efficient keyword recognition during lexing
 */
typedef struct {
    const char *keyword;
    TokenType type;
} KeywordEntry;

/* Get keyword table for lexer initialization */
const KeywordEntry *get_keyword_table(void);
size_t get_keyword_table_size(void);

/* Lookup a keyword by string (returns TOK_IDENTIFIER if not found) */
TokenType lookup_keyword(const char *str);

#endif /* NATURELANG_TOKENS_H */
