/*
 * NatureLang Compiler
 * Copyright (c) 2024
 * 
 * Parser Public API
 * 
 * Header file for the NatureLang parser interface.
 */

#ifndef NATURELANG_PARSER_H
#define NATURELANG_PARSER_H

#include <stdio.h>
#include "ast.h"

/* ============================================================================
 * PARSER FUNCTIONS
 * ============================================================================
 */

/*
 * Parse a NatureLang program from a file.
 * 
 * @param input  The input file to parse.
 * @return       The root AST node, or NULL on error.
 */
ASTNode *naturelang_parse(FILE *input);

/*
 * Parse a NatureLang program from a string.
 * 
 * @param source  The source code string.
 * @return        The root AST node, or NULL on error.
 */
ASTNode *naturelang_parse_string(const char *source);

/*
 * Get the result of the last parse operation.
 * 
 * @return  The root AST node from the last parse.
 */
ASTNode *get_parse_result(void);

/* ============================================================================
 * ERROR HANDLING
 * ============================================================================
 */

/*
 * Parser error callback (can be overridden).
 */
void yyerror(const char *s);

#endif /* NATURELANG_PARSER_H */
