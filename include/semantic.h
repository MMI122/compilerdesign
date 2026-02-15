/*
 * NatureLang Compiler
 * Copyright (c) 2026
 * 
 * Semantic Analyzer Header
 * 
 * The semantic analyzer performs type checking, scope analysis,
 * and validates that the program follows NatureLang's rules.
 */

#ifndef NATURELANG_SEMANTIC_H
#define NATURELANG_SEMANTIC_H

#include "ast.h"
#include "symbol_table.h"
#include <stdbool.h>

/* ============================================================================
 * SEMANTIC ANALYSIS RESULT
 * ============================================================================
 */
typedef struct {
    bool success;           /* Did analysis complete without errors? */
    int error_count;        /* Number of errors found */
    int warning_count;      /* Number of warnings */
    SymbolTable *symtab;    /* Symbol table (for use by code generator) */
} SemanticResult;

/* ============================================================================
 * MAIN ANALYSIS FUNCTION
 * ============================================================================
 */

/* Perform semantic analysis on an AST
 * Returns a SemanticResult with analysis outcome
 * The caller owns the returned SymbolTable and must free it */
SemanticResult semantic_analyze(ASTNode *program);

/* Free a semantic result (destroys symbol table if present) */
void semantic_result_free(SemanticResult *result);

/* ============================================================================
 * TYPE CHECKING UTILITIES
 * ============================================================================
 */

/* Check if two types are compatible for assignment */
bool types_compatible(DataType target, DataType source);

/* Check if a type can be used in arithmetic operations */
bool type_is_numeric(DataType type);

/* Check if a type can be used in boolean expressions */
bool type_is_boolean(DataType type);

/* Get the result type of a binary operation */
DataType get_binary_op_result_type(Operator op, DataType left, DataType right);

/* Get the result type of a unary operation */
DataType get_unary_op_result_type(Operator op, DataType operand);

/* Get string representation of a type */
const char *datatype_to_string(DataType type);

/* Get string representation of an operator */
const char *operator_to_string(Operator op);

#endif /* NATURELANG_SEMANTIC_H */
