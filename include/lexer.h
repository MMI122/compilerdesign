/*
 * NatureLang Compiler
 * Copyright (c) 2026
 * 
 * Lexer Public API Header
 * 
 * This file declares the public interface for the NatureLang lexer.
 * 
 * BUILD MODES:
 * - Standalone mode (default): Full Token API with token_create*, token_free, etc.
 * - Parser mode (USE_BISON_TOKENS): Minimal interface for Bison integration
 */

#ifndef NATURELANG_LEXER_H
#define NATURELANG_LEXER_H

#include <stdio.h>

/* When building with Bison, we don't need the full Token API */
#ifndef USE_BISON_TOKENS
#include "tokens.h"
#endif

/* ============================================================================
 * COMMON INTERFACE (Both modes)
 * ============================================================================
 */

/* Maximum length constants */
#define MAX_IDENTIFIER_LENGTH 256
#define MAX_STRING_LENGTH 4096
#define MAX_ERROR_LENGTH 512

/* Flex-provided variables */
extern FILE *yyin;
extern char *yytext;
extern int yyleng;

/**
 * Low-level lex function (Flex-generated).
 * @return Token type as integer, 0 for EOF
 */
int yylex(void);

/**
 * Destroy lexer scanner state (Flex-generated).
 */
int yylex_destroy(void);

/* ============================================================================
 * STANDALONE LEXER API (only when NOT building with Bison)
 * This provides the full Token structure API for direct lexer usage.
 * ============================================================================
 */
#ifndef USE_BISON_TOKENS

/* Token value storage (set by lexer, read by parser) */
extern long long yylval_int;
extern double yylval_float;
extern char *yylval_string;
extern char yylval_char;

/* Location tracking */
extern SourceLocation yylloc;

/* Error tracking */
extern int lexer_error_count;

/**
 * Initialize lexer to read from a file.
 * @param filename Path to the source file
 * @return 0 on success, -1 on failure
 */
int lexer_init_file(const char *filename);

/**
 * Initialize lexer to read from a string buffer.
 * @param input The source code string
 * @return 0 on success, -1 on failure
 */
int lexer_init_string(const char *input);

/**
 * Clean up lexer resources.
 */
void lexer_cleanup(void);

/**
 * Get the next token from the input.
 * Returns a heap-allocated Token that must be freed with token_free().
 * @return Pointer to the next token, or NULL on error
 */
Token *lexer_next_token(void);

/**
 * Get current line number in source.
 * @return Current line (1-based)
 */
int lexer_get_line(void);

/**
 * Get current column number in source.
 * @return Current column (1-based)
 */
int lexer_get_column(void);

/**
 * Get current source filename.
 * @return Filename string (do not free)
 */
const char *lexer_get_filename(void);

/**
 * Get total number of lexer errors encountered.
 * @return Error count
 */
int lexer_get_error_count(void);

/* ============================================================================
 * TOKEN LIST (for batch tokenization)
 * ============================================================================
 */

/**
 * Node in a linked list of tokens.
 */
typedef struct TokenNode {
    Token *token;
    struct TokenNode *next;
} TokenNode;

/**
 * Token list structure for collecting all tokens.
 */
typedef struct {
    TokenNode *head;
    TokenNode *tail;
    int count;
} TokenList;

/**
 * Create a new empty token list.
 */
TokenList *token_list_create(void);

/**
 * Append a token to the list (takes ownership).
 */
void token_list_append(TokenList *list, Token *token);

/**
 * Free a token list and all contained tokens.
 */
void token_list_free(TokenList *list);

/**
 * Tokenize entire input into a list.
 */
TokenList *lexer_tokenize_all(void);

/**
 * Print all tokens in a list (for debugging).
 */
void token_list_print(const TokenList *list);

/* ============================================================================
 * ERROR RECOVERY
 * ============================================================================
 */

/**
 * Skip tokens until a synchronization point.
 */
void lexer_skip_to_sync(void);

/**
 * Skip to end of current line.
 */
void lexer_skip_to_eol(void);

#endif /* USE_BISON_TOKENS */

#endif /* NATURELANG_LEXER_H */
