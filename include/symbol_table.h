/*
 * NatureLang Compiler
 * Copyright (c) 2026
 * 
 * Symbol Table Header
 * 
 * The symbol table manages all identifiers (variables, functions, constants)
 * and their associated information (type, scope, etc.) during semantic analysis.
 * It supports nested scopes for blocks, functions, and control structures.
 */

#ifndef NATURELANG_SYMBOL_TABLE_H
#define NATURELANG_SYMBOL_TABLE_H

#include "ast.h"
#include <stdbool.h>

/* ============================================================================
 * SYMBOL KIND ENUMERATION
 * ============================================================================
 * Distinguishes between different kinds of symbols
 */
typedef enum {
    SYMBOL_VARIABLE,        /* Regular variable */
    SYMBOL_CONSTANT,        /* Constant (immutable variable) */
    SYMBOL_FUNCTION,        /* Function declaration */
    SYMBOL_PARAMETER        /* Function parameter */
} SymbolKind;

/* ============================================================================
 * SYMBOL STRUCTURE
 * ============================================================================
 * Represents a single symbol in the symbol table
 */
typedef struct Symbol {
    char *name;                 /* Symbol name (identifier) */
    SymbolKind kind;            /* What kind of symbol */
    DataType type;              /* Data type of the symbol */
    int scope_level;            /* Nesting level where declared (0 = global) */
    bool is_initialized;        /* Has been assigned a value? */
    SourceLocation decl_loc;    /* Where it was declared (for error messages) */
    
    /* Function-specific information */
    struct {
        ASTNodeList *params;    /* Parameter list (for functions) */
        DataType return_type;   /* Return type (for functions) */
        bool has_return;        /* Does function have a return statement? */
    } func_info;
    
    struct Symbol *next;        /* Next symbol in the same scope (linked list) */
} Symbol;

/* ============================================================================
 * SCOPE STRUCTURE
 * ============================================================================
 * Represents a single scope level (global, function, block, etc.)
 */
typedef struct Scope {
    int level;                  /* Nesting level (0 = global) */
    Symbol *symbols;            /* Head of symbol linked list for this scope */
    struct Scope *parent;       /* Enclosing scope (NULL for global) */
    
    /* Scope context information */
    bool is_function_scope;     /* Is this a function body scope? */
    bool is_loop_scope;         /* Is this inside a loop? (for break/continue) */
    bool is_secure_zone;        /* Is this inside a secure zone? */
    DataType expected_return;   /* Expected return type (if in function) */
} Scope;

/* ============================================================================
 * SYMBOL TABLE STRUCTURE
 * ============================================================================
 * The main symbol table that manages all scopes
 */
typedef struct SymbolTable {
    Scope *current_scope;       /* Currently active scope */
    Scope *global_scope;        /* Global scope (always exists) */
    int scope_depth;            /* Current nesting depth */
    
    /* Error tracking */
    int error_count;            /* Number of semantic errors found */
    int warning_count;          /* Number of warnings */
} SymbolTable;

/* ============================================================================
 * SYMBOL TABLE MANAGEMENT
 * ============================================================================
 */

/* Create a new symbol table with global scope */
SymbolTable *symtab_create(void);

/* Destroy symbol table and free all memory */
void symtab_destroy(SymbolTable *table);

/* ============================================================================
 * SCOPE MANAGEMENT
 * ============================================================================
 */

/* Enter a new scope (e.g., entering a function or block) */
void symtab_enter_scope(SymbolTable *table);

/* Enter a new function scope with expected return type */
void symtab_enter_function_scope(SymbolTable *table, DataType return_type);

/* Enter a loop scope (enables break/continue) */
void symtab_enter_loop_scope(SymbolTable *table);

/* Enter a secure zone scope */
void symtab_enter_secure_scope(SymbolTable *table);

/* Exit the current scope (returns to parent scope) */
void symtab_exit_scope(SymbolTable *table);

/* Get current scope depth */
int symtab_get_depth(SymbolTable *table);

/* Check if currently inside a loop */
bool symtab_in_loop(SymbolTable *table);

/* Check if currently inside a function */
bool symtab_in_function(SymbolTable *table);

/* Check if currently inside a secure zone */
bool symtab_in_secure_zone(SymbolTable *table);

/* Get expected return type of current function (TYPE_NOTHING if not in function) */
DataType symtab_get_return_type(SymbolTable *table);

/* ============================================================================
 * SYMBOL OPERATIONS
 * ============================================================================
 */

/* Declare a new variable in current scope
 * Returns NULL on success, or error message on failure (e.g., redeclaration) */
const char *symtab_declare_variable(SymbolTable *table, const char *name, 
                                     DataType type, bool is_const,
                                     SourceLocation loc);

/* Declare a new function in current scope
 * Returns NULL on success, or error message on failure */
const char *symtab_declare_function(SymbolTable *table, const char *name,
                                     ASTNodeList *params, DataType return_type,
                                     SourceLocation loc);

/* Declare a function parameter (adds to current scope)
 * Returns NULL on success, or error message on failure */
const char *symtab_declare_parameter(SymbolTable *table, const char *name,
                                      DataType type, SourceLocation loc);

/* Look up a symbol by name (searches current scope and all parent scopes)
 * Returns NULL if not found */
Symbol *symtab_lookup(SymbolTable *table, const char *name);

/* Look up a symbol only in the current scope (no parent search)
 * Returns NULL if not found */
Symbol *symtab_lookup_current_scope(SymbolTable *table, const char *name);

/* Look up a function by name
 * Returns NULL if not found or if symbol is not a function */
Symbol *symtab_lookup_function(SymbolTable *table, const char *name);

/* Mark a variable as initialized */
void symtab_mark_initialized(Symbol *sym);

/* ============================================================================
 * ERROR/WARNING HELPERS
 * ============================================================================
 */

/* Report a semantic error */
void symtab_error(SymbolTable *table, SourceLocation loc, const char *format, ...);

/* Report a semantic warning */
void symtab_warning(SymbolTable *table, SourceLocation loc, const char *format, ...);

/* Get error count */
int symtab_error_count(SymbolTable *table);

/* Get warning count */
int symtab_warning_count(SymbolTable *table);

/* ============================================================================
 * DEBUGGING / PRINTING
 * ============================================================================
 */

/* Print the entire symbol table (for debugging) */
void symtab_print(SymbolTable *table);

/* Print a single scope */
void symtab_print_scope(Scope *scope);

/* Get string representation of a symbol kind */
const char *symbol_kind_to_string(SymbolKind kind);

#endif /* NATURELANG_SYMBOL_TABLE_H */
