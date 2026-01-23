/*
 * NatureLang Compiler
 * Copyright (c) 2026
 * 
 * Token Implementation
 * 
 * Implementation of token creation, manipulation, and utility functions.
 */

#define _POSIX_C_SOURCE 200809L
#include "tokens.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ============================================================================
 * KEYWORD TABLE
 * ============================================================================
 * Sorted alphabetically for potential binary search optimization
 */
static const KeywordEntry keyword_table[] = {
    /* A */
    {"a", TOK_A},
    {"an", TOK_AN},
    {"and", TOK_AND},
    {"as", TOK_AS},
    {"ask", TOK_ASK},
    {"at", TOK_AT_LEAST},  /* Will be combined with "least" or "most" */
    
    /* B */
    {"back", TOK_BACK},
    {"becomes", TOK_BECOMES},
    {"by", TOK_BY},
    
    /* C */
    {"call", TOK_CALL},
    {"called", TOK_CALLED},
    {"create", TOK_CREATE},
    
    /* D */
    {"decimal", TOK_TYPE_DECIMAL},
    {"define", TOK_DEFINE},
    {"display", TOK_DISPLAY},
    {"divided", TOK_DIVIDED},
    {"do", TOK_DO},
    
    /* E */
    {"each", TOK_EACH},
    {"else", TOK_ELSE},
    {"end", TOK_END},
    {"enter", TOK_ENTER},
    {"equal", TOK_EQUAL},
    {"equals", TOK_EQUALS},
    
    /* F */
    {"false", TOK_FALSE},
    {"flag", TOK_TYPE_FLAG},
    {"for", TOK_FOR},
    {"from", TOK_FROM},
    {"function", TOK_FUNCTION},
    
    /* G */
    {"give", TOK_GIVE},
    {"greater", TOK_GREATER},
    
    /* I */
    {"if", TOK_IF},
    {"in", TOK_IN},
    {"into", TOK_INTO},
    {"is", TOK_IS},
    {"it", TOK_IT},
    
    /* L */
    {"less", TOK_LESS},
    {"list", TOK_TYPE_LIST},
    
    /* M */
    {"make", TOK_MAKE},
    {"minus", TOK_MINUS},
    {"modulo", TOK_MODULO},
    {"multiplied", TOK_MULTIPLIED},
    
    /* N */
    {"named", TOK_NAMED},
    {"no", TOK_NO},
    {"not", TOK_NOT},
    {"nothing", TOK_TYPE_NOTHING},
    {"number", TOK_TYPE_NUMBER},
    
    /* O */
    {"of", TOK_OF},
    {"or", TOK_OR},
    {"otherwise", TOK_OTHERWISE},
    
    /* P */
    {"plus", TOK_PLUS},
    {"power", TOK_POWER},
    {"print", TOK_PRINT},
    
    /* R */
    {"read", TOK_READ},
    {"remainder", TOK_REMAINDER},
    {"remember", TOK_REMEMBER},
    {"repeat", TOK_REPEAT},
    {"returns", TOK_RETURNS},
    {"root", TOK_ROOT},
    
    /* S */
    {"safe", TOK_SAFE},
    {"save", TOK_SAVE},
    {"secure", TOK_SECURE},
    {"set", TOK_SET},
    {"show", TOK_SHOW},
    {"skip", TOK_SKIP},
    {"square", TOK_SQUARE},
    {"squared", TOK_SQUARED},
    {"stop", TOK_STOP},
    
    /* T */
    {"takes", TOK_TAKES},
    {"text", TOK_TYPE_TEXT},
    {"than", TOK_THAN},
    {"that", TOK_THAT},
    {"then", TOK_THEN},
    {"times", TOK_TIMES},
    {"to", TOK_TO},
    {"true", TOK_TRUE},
    
    /* U */
    {"until", TOK_UNTIL},
    
    /* W */
    {"while", TOK_WHILE},
    {"with", TOK_WITH},
    
    /* Y */
    {"yes", TOK_YES},
    
    /* Z */
    {"zone", TOK_ZONE},
};

static const size_t keyword_table_size = sizeof(keyword_table) / sizeof(keyword_table[0]);

/* ============================================================================
 * TOKEN TYPE STRING REPRESENTATIONS
 * ============================================================================
 */
static const char *token_type_strings[] = {
    [TOK_EOF] = "EOF",
    [TOK_ERROR] = "ERROR",
    [TOK_UNKNOWN] = "UNKNOWN",
    
    /* Keywords - Declaration */
    [TOK_CREATE] = "CREATE",
    [TOK_A] = "A",
    [TOK_AN] = "AN",
    [TOK_CALLED] = "CALLED",
    [TOK_NAMED] = "NAMED",
    [TOK_AND] = "AND",
    [TOK_SET] = "SET",
    [TOK_IT] = "IT",
    [TOK_TO] = "TO",
    [TOK_AS] = "AS",
    
    /* Keywords - Assignment */
    [TOK_BECOMES] = "BECOMES",
    [TOK_EQUALS] = "EQUALS",
    [TOK_MAKE] = "MAKE",
    [TOK_EQUAL] = "EQUAL",
    
    /* Keywords - Types */
    [TOK_TYPE_NUMBER] = "TYPE_NUMBER",
    [TOK_TYPE_TEXT] = "TYPE_TEXT",
    [TOK_TYPE_DECIMAL] = "TYPE_DECIMAL",
    [TOK_TYPE_FLAG] = "TYPE_FLAG",
    [TOK_TYPE_LIST] = "TYPE_LIST",
    [TOK_TYPE_NOTHING] = "TYPE_NOTHING",
    
    /* Keywords - Control Flow */
    [TOK_IF] = "IF",
    [TOK_THEN] = "THEN",
    [TOK_OTHERWISE] = "OTHERWISE",
    [TOK_ELSE] = "ELSE",
    [TOK_END] = "END",
    [TOK_REPEAT] = "REPEAT",
    [TOK_TIMES] = "TIMES",
    [TOK_WHILE] = "WHILE",
    [TOK_DO] = "DO",
    [TOK_FOR] = "FOR",
    [TOK_EACH] = "EACH",
    [TOK_IN] = "IN",
    [TOK_FROM] = "FROM",
    [TOK_UNTIL] = "UNTIL",
    [TOK_STOP] = "STOP",
    [TOK_SKIP] = "SKIP",
    
    /* Keywords - Functions */
    [TOK_DEFINE] = "DEFINE",
    [TOK_FUNCTION] = "FUNCTION",
    [TOK_THAT] = "THAT",
    [TOK_TAKES] = "TAKES",
    [TOK_RETURNS] = "RETURNS",
    [TOK_GIVE] = "GIVE",
    [TOK_BACK] = "BACK",
    [TOK_CALL] = "CALL",
    [TOK_WITH] = "WITH",
    
    /* Keywords - I/O */
    [TOK_DISPLAY] = "DISPLAY",
    [TOK_SHOW] = "SHOW",
    [TOK_PRINT] = "PRINT",
    [TOK_ASK] = "ASK",
    [TOK_READ] = "READ",
    [TOK_REMEMBER] = "REMEMBER",
    [TOK_SAVE] = "SAVE",
    [TOK_INTO] = "INTO",
    
    /* Keywords - Security */
    [TOK_ENTER] = "ENTER",
    [TOK_SECURE] = "SECURE",
    [TOK_ZONE] = "ZONE",
    [TOK_SAFE] = "SAFE",
    
    /* Keywords - Logical */
    [TOK_IS] = "IS",
    [TOK_NOT] = "NOT",
    [TOK_OR] = "OR",
    [TOK_TRUE] = "TRUE",
    [TOK_FALSE] = "FALSE",
    [TOK_YES] = "YES",
    [TOK_NO] = "NO",
    
    /* Keywords - Comparison */
    [TOK_GREATER] = "GREATER",
    [TOK_LESS] = "LESS",
    [TOK_THAN] = "THAN",
    [TOK_GREATER_THAN] = "GREATER_THAN",
    [TOK_LESS_THAN] = "LESS_THAN",
    [TOK_EQUAL_TO] = "EQUAL_TO",
    [TOK_NOT_EQUAL_TO] = "NOT_EQUAL_TO",
    [TOK_AT_LEAST] = "AT_LEAST",
    [TOK_AT_MOST] = "AT_MOST",
    
    /* Keywords - Arithmetic */
    [TOK_PLUS] = "PLUS",
    [TOK_MINUS] = "MINUS",
    [TOK_MULTIPLIED] = "MULTIPLIED",
    [TOK_DIVIDED] = "DIVIDED",
    [TOK_BY] = "BY",
    [TOK_MODULO] = "MODULO",
    [TOK_REMAINDER] = "REMAINDER",
    [TOK_OF] = "OF",
    [TOK_POWER] = "POWER",
    [TOK_SQUARED] = "SQUARED",
    [TOK_SQUARE] = "SQUARE",
    [TOK_ROOT] = "ROOT",
    
    /* Operators - Symbolic */
    [TOK_OP_PLUS] = "OP_PLUS",
    [TOK_OP_MINUS] = "OP_MINUS",
    [TOK_OP_STAR] = "OP_STAR",
    [TOK_OP_SLASH] = "OP_SLASH",
    [TOK_OP_PERCENT] = "OP_PERCENT",
    [TOK_OP_CARET] = "OP_CARET",
    [TOK_OP_EQ] = "OP_EQ",
    [TOK_OP_EQEQ] = "OP_EQEQ",
    [TOK_OP_NEQ] = "OP_NEQ",
    [TOK_OP_LT] = "OP_LT",
    [TOK_OP_GT] = "OP_GT",
    [TOK_OP_LTE] = "OP_LTE",
    [TOK_OP_GTE] = "OP_GTE",
    [TOK_OP_AND] = "OP_AND",
    [TOK_OP_OR] = "OP_OR",
    [TOK_OP_NOT] = "OP_NOT",
    [TOK_OP_ARROW] = "OP_ARROW",
    [TOK_OP_COLON] = "OP_COLON",
    
    /* Punctuation */
    [TOK_LPAREN] = "LPAREN",
    [TOK_RPAREN] = "RPAREN",
    [TOK_LBRACKET] = "LBRACKET",
    [TOK_RBRACKET] = "RBRACKET",
    [TOK_LBRACE] = "LBRACE",
    [TOK_RBRACE] = "RBRACE",
    [TOK_COMMA] = "COMMA",
    [TOK_DOT] = "DOT",
    [TOK_SEMICOLON] = "SEMICOLON",
    [TOK_NEWLINE] = "NEWLINE",
    
    /* Literals */
    [TOK_INTEGER] = "INTEGER",
    [TOK_FLOAT] = "FLOAT",
    [TOK_STRING] = "STRING",
    [TOK_CHAR] = "CHAR",
    
    /* Identifiers */
    [TOK_IDENTIFIER] = "IDENTIFIER",
    
    /* Comments */
    [TOK_COMMENT] = "COMMENT",
    [TOK_BLOCK_COMMENT] = "BLOCK_COMMENT",
};

/* ============================================================================
 * KEYWORD LOOKUP FUNCTION
 * ============================================================================
 */
TokenType lookup_keyword(const char *str) {
    if (str == NULL) return TOK_IDENTIFIER;
    
    /* Convert to lowercase for case-insensitive matching */
    char lower[256];
    size_t i;
    for (i = 0; str[i] && i < 255; i++) {
        lower[i] = tolower((unsigned char)str[i]);
    }
    lower[i] = '\0';
    
    /* Linear search (could be optimized with hash table or binary search) */
    for (i = 0; i < keyword_table_size; i++) {
        if (strcmp(lower, keyword_table[i].keyword) == 0) {
            return keyword_table[i].type;
        }
    }
    
    return TOK_IDENTIFIER;
}

const KeywordEntry *get_keyword_table(void) {
    return keyword_table;
}

size_t get_keyword_table_size(void) {
    return keyword_table_size;
}

/* ============================================================================
 * TOKEN CREATION FUNCTIONS
 * ============================================================================
 */
static Token *token_alloc(void) {
    Token *token = (Token *)malloc(sizeof(Token));
    if (token == NULL) {
        fprintf(stderr, "Error: Failed to allocate token\n");
        exit(1);
    }
    memset(token, 0, sizeof(Token));
    return token;
}

static char *safe_strdup(const char *str) {
    if (str == NULL) return NULL;
    char *dup = strdup(str);
    if (dup == NULL) {
        fprintf(stderr, "Error: Failed to duplicate string\n");
        exit(1);
    }
    return dup;
}

Token *token_create(TokenType type, const char *lexeme, SourceLocation loc) {
    Token *token = token_alloc();
    token->type = type;
    token->lexeme = safe_strdup(lexeme);
    token->loc = loc;
    return token;
}

Token *token_create_int(long long value, const char *lexeme, SourceLocation loc) {
    Token *token = token_create(TOK_INTEGER, lexeme, loc);
    token->value.int_value = value;
    return token;
}

Token *token_create_float(double value, const char *lexeme, SourceLocation loc) {
    Token *token = token_create(TOK_FLOAT, lexeme, loc);
    token->value.float_value = value;
    return token;
}

Token *token_create_string(const char *value, const char *lexeme, SourceLocation loc) {
    Token *token = token_create(TOK_STRING, lexeme, loc);
    token->value.string_value = safe_strdup(value);
    return token;
}

Token *token_create_identifier(const char *name, SourceLocation loc) {
    Token *token = token_create(TOK_IDENTIFIER, name, loc);
    token->value.string_value = safe_strdup(name);
    return token;
}

Token *token_create_error(const char *message, SourceLocation loc) {
    Token *token = token_create(TOK_ERROR, message, loc);
    token->value.string_value = safe_strdup(message);
    return token;
}

void token_free(Token *token) {
    if (token == NULL) return;
    
    if (token->lexeme != NULL) {
        free(token->lexeme);
    }
    
    /* Free string value if applicable */
    if ((token->type == TOK_STRING || 
         token->type == TOK_IDENTIFIER ||
         token->type == TOK_ERROR) && 
        token->value.string_value != NULL) {
        free(token->value.string_value);
    }
    
    free(token);
}

/* ============================================================================
 * TOKEN UTILITY FUNCTIONS
 * ============================================================================
 */
const char *token_type_to_string(TokenType type) {
    if (type >= 0 && type < TOK_COUNT) {
        return token_type_strings[type] ? token_type_strings[type] : "UNKNOWN";
    }
    return "INVALID";
}

const char *token_type_description(TokenType type) {
    switch (type) {
        case TOK_EOF: return "end of file";
        case TOK_ERROR: return "error";
        case TOK_INTEGER: return "integer literal";
        case TOK_FLOAT: return "floating-point literal";
        case TOK_STRING: return "string literal";
        case TOK_IDENTIFIER: return "identifier";
        case TOK_CREATE: return "keyword 'create'";
        case TOK_IF: return "keyword 'if'";
        case TOK_THEN: return "keyword 'then'";
        case TOK_ELSE: return "keyword 'else'";
        case TOK_OTHERWISE: return "keyword 'otherwise'";
        case TOK_END: return "keyword 'end'";
        case TOK_DISPLAY: return "keyword 'display'";
        case TOK_DEFINE: return "keyword 'define'";
        case TOK_FUNCTION: return "keyword 'function'";
        default: return token_type_to_string(type);
    }
}

void token_print(const Token *token) {
    if (token == NULL) {
        printf("(null token)\n");
        return;
    }
    
    printf("%-15s ", token_type_to_string(token->type));
    
    switch (token->type) {
        case TOK_INTEGER:
            printf("%lld", token->value.int_value);
            break;
        case TOK_FLOAT:
            printf("%f", token->value.float_value);
            break;
        case TOK_STRING:
        case TOK_IDENTIFIER:
        case TOK_ERROR:
            printf("\"%s\"", token->value.string_value ? token->value.string_value : "(null)");
            break;
        default:
            if (token->lexeme) {
                printf("%s", token->lexeme);
            }
            break;
    }
    printf("\n");
}

void token_print_debug(const Token *token) {
    if (token == NULL) {
        printf("(null token)\n");
        return;
    }
    
    printf("Token {\n");
    printf("  type:   %s (%d)\n", token_type_to_string(token->type), token->type);
    printf("  lexeme: \"%s\"\n", token->lexeme ? token->lexeme : "(null)");
    printf("  loc:    %s:%d:%d - %d:%d\n",
           token->loc.filename ? token->loc.filename : "(unknown)",
           token->loc.first_line, token->loc.first_column,
           token->loc.last_line, token->loc.last_column);
    
    switch (token->type) {
        case TOK_INTEGER:
            printf("  value:  %lld (int)\n", token->value.int_value);
            break;
        case TOK_FLOAT:
            printf("  value:  %f (float)\n", token->value.float_value);
            break;
        case TOK_STRING:
        case TOK_IDENTIFIER:
            printf("  value:  \"%s\" (string)\n", 
                   token->value.string_value ? token->value.string_value : "(null)");
            break;
        default:
            break;
    }
    printf("}\n");
}

int token_is_keyword(TokenType type) {
    return (type >= TOK_CREATE && type <= TOK_ROOT);
}

int token_is_operator(TokenType type) {
    return (type >= TOK_OP_PLUS && type <= TOK_OP_COLON);
}

int token_is_literal(TokenType type) {
    return (type >= TOK_INTEGER && type <= TOK_CHAR);
}

int token_is_type(TokenType type) {
    return (type >= TOK_TYPE_NUMBER && type <= TOK_TYPE_NOTHING);
}
