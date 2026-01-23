/*
 * NatureLang Compiler
 * Copyright (c) 2026
 * 
 * Lexer Public API Header
 * 
 * This file declares the public interface for the NatureLang lexer.
 */

#ifndef NATURELANG_LEXER_H
#define NATURELANG_LEXER_H

#include "tokens.h"
#include <stdio.h>

/* ============================================================================
 * LEXER CONFIGURATION
 * ============================================================================
 */

/* Maximum length of identifiers */
#define MAX_IDENTIFIER_LENGTH 256

/* Maximum length of string literals */
#define MAX_STRING_LENGTH 4096

/* Maximum length of error messages */
#define MAX_ERROR_LENGTH 512

/* ============================================================================
 * GLOBAL LEXER STATE (used by Flex-generated code)
 * ============================================================================
 */

/* Token value storage (set by lexer, read by parser) */
extern long long yylval_int;
extern double yylval_float;
extern char *yylval_string;
extern char yylval_char;

/* Location tracking */
extern SourceLocation yylloc;

/* Flex-provided variables */
extern FILE *yyin;
extern char *yytext;
extern int yyleng;

/* Error tracking */
extern int lexer_error_count;

/* ============================================================================
 * LEXER INITIALIZATION AND CLEANUP
 * ============================================================================
 */

/**
 * Initialize lexer to read from a file.
 * 
 * @param filename Path to the source file
 * @return 0 on success, -1 on failure
 */
int lexer_init_file(const char *filename);

/**
 * Initialize lexer to read from a string buffer.
 * 
 * @param input The source code string
 * @return 0 on success, -1 on failure
 */
int lexer_init_string(const char *input);

/**
 * Clean up lexer resources.
 * Call this when done with lexing.
 */
void lexer_cleanup(void);

/* ============================================================================
 * TOKEN RETRIEVAL
 * ============================================================================
 */

/**
 * Get the next token from the input.
 * 
 * This is the main function for tokenization. Returns a heap-allocated
 * Token structure that must be freed with token_free().
 * 
 * @return Pointer to the next token, or NULL on error
 */
Token *lexer_next_token(void);

/**
 * Low-level lex function (Flex-generated).
 * Use lexer_next_token() instead for Token objects.
 * 
 * @return Token type as integer, 0 for EOF
 */
int yylex(void);

/**
 * Destroy lexer scanner state (Flex-generated).
 */
int yylex_destroy(void);

/* ============================================================================
 * LEXER STATE QUERIES
 * ============================================================================
 */

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
 * @return Pointer to new token list
 */
TokenList *token_list_create(void);

/**
 * Append a token to the list.
 * The list takes ownership of the token.
 * 
 * @param list The token list
 * @param token The token to append
 */
void token_list_append(TokenList *list, Token *token);

/**
 * Free a token list and all contained tokens.
 * @param list The token list to free
 */
void token_list_free(TokenList *list);

/**
 * Tokenize entire input into a list.
 * Must call lexer_init_file() or lexer_init_string() first.
 * 
 * @return Token list containing all tokens
 */
TokenList *lexer_tokenize_all(void);

/**
 * Print all tokens in a list (for debugging).
 * @param list The token list to print
 */
void token_list_print(const TokenList *list);

/* ============================================================================
 * ERROR RECOVERY
 * ============================================================================
 */

/**
 * Skip tokens until a synchronization point (e.g., end of statement).
 * Used for error recovery during parsing.
 */
void lexer_skip_to_sync(void);

/**
 * Skip to end of current line.
 */
void lexer_skip_to_eol(void);

#endif /* NATURELANG_LEXER_H */
