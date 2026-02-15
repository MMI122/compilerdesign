/*
 * NatureLang Compiler
 * Copyright (c) 2026
 * 
 * Symbol Table Implementation
 * 
 * Implements the symbol table for semantic analysis, managing scopes,
 * variable declarations, function declarations, and lookups.
 */

#define _POSIX_C_SOURCE 200809L
#include "symbol_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ============================================================================
 * HELPER FUNCTIONS
 * ============================================================================
 */

static void *safe_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "Fatal: Out of memory in symbol table\n");
        exit(1);
    }
    memset(ptr, 0, size);
    return ptr;
}

static char *safe_strdup(const char *str) {
    if (!str) return NULL;
    char *copy = strdup(str);
    if (!copy) {
        fprintf(stderr, "Fatal: Out of memory in symbol table\n");
        exit(1);
    }
    return copy;
}

/* ============================================================================
 * SYMBOL CREATION AND DESTRUCTION
 * ============================================================================
 */

static Symbol *symbol_create(const char *name, SymbolKind kind, DataType type,
                             int scope_level, SourceLocation loc) {
    Symbol *sym = safe_malloc(sizeof(Symbol));
    sym->name = safe_strdup(name);
    sym->kind = kind;
    sym->type = type;
    sym->scope_level = scope_level;
    sym->is_initialized = false;
    sym->decl_loc = loc;
    sym->func_info.params = NULL;
    sym->func_info.return_type = TYPE_NOTHING;
    sym->func_info.has_return = false;
    sym->next = NULL;
    return sym;
}

static void symbol_destroy(Symbol *sym) {
    if (!sym) return;
    free(sym->name);
    /* Note: func_info.params is owned by AST, don't free here */
    free(sym);
}

/* ============================================================================
 * SCOPE CREATION AND DESTRUCTION
 * ============================================================================
 */

static Scope *scope_create(int level, Scope *parent) {
    Scope *scope = safe_malloc(sizeof(Scope));
    scope->level = level;
    scope->symbols = NULL;
    scope->parent = parent;
    scope->is_function_scope = false;
    scope->is_loop_scope = false;
    scope->is_secure_zone = false;
    scope->expected_return = TYPE_NOTHING;
    return scope;
}

static void scope_destroy(Scope *scope) {
    if (!scope) return;
    
    /* Free all symbols in this scope */
    Symbol *sym = scope->symbols;
    while (sym) {
        Symbol *next = sym->next;
        symbol_destroy(sym);
        sym = next;
    }
    
    free(scope);
}

/* ============================================================================
 * SYMBOL TABLE CREATION AND DESTRUCTION
 * ============================================================================
 */

SymbolTable *symtab_create(void) {
    SymbolTable *table = safe_malloc(sizeof(SymbolTable));
    
    /* Create global scope */
    table->global_scope = scope_create(0, NULL);
    table->current_scope = table->global_scope;
    table->scope_depth = 0;
    table->error_count = 0;
    table->warning_count = 0;
    
    return table;
}

void symtab_destroy(SymbolTable *table) {
    if (!table) return;
    
    /* Destroy all scopes from current to global */
    Scope *scope = table->current_scope;
    while (scope) {
        Scope *parent = scope->parent;
        scope_destroy(scope);
        scope = parent;
    }
    
    free(table);
}

/* ============================================================================
 * SCOPE MANAGEMENT
 * ============================================================================
 */

void symtab_enter_scope(SymbolTable *table) {
    table->scope_depth++;
    Scope *new_scope = scope_create(table->scope_depth, table->current_scope);
    
    /* Inherit loop/secure zone context from parent */
    if (table->current_scope) {
        new_scope->is_loop_scope = table->current_scope->is_loop_scope;
        new_scope->is_secure_zone = table->current_scope->is_secure_zone;
        if (table->current_scope->is_function_scope || 
            table->current_scope->expected_return != TYPE_NOTHING) {
            new_scope->expected_return = table->current_scope->expected_return;
        }
    }
    
    table->current_scope = new_scope;
}

void symtab_enter_function_scope(SymbolTable *table, DataType return_type) {
    symtab_enter_scope(table);
    table->current_scope->is_function_scope = true;
    table->current_scope->expected_return = return_type;
    /* Reset loop context - we're in a new function */
    table->current_scope->is_loop_scope = false;
}

void symtab_enter_loop_scope(SymbolTable *table) {
    symtab_enter_scope(table);
    table->current_scope->is_loop_scope = true;
}

void symtab_enter_secure_scope(SymbolTable *table) {
    symtab_enter_scope(table);
    table->current_scope->is_secure_zone = true;
}

void symtab_exit_scope(SymbolTable *table) {
    if (table->current_scope == table->global_scope) {
        fprintf(stderr, "Warning: Attempting to exit global scope\n");
        return;
    }
    
    Scope *old_scope = table->current_scope;
    table->current_scope = old_scope->parent;
    table->scope_depth--;
    
    scope_destroy(old_scope);
}

int symtab_get_depth(SymbolTable *table) {
    return table->scope_depth;
}

bool symtab_in_loop(SymbolTable *table) {
    return table->current_scope && table->current_scope->is_loop_scope;
}

bool symtab_in_function(SymbolTable *table) {
    /* Search up the scope chain for a function scope */
    Scope *scope = table->current_scope;
    while (scope) {
        if (scope->is_function_scope) return true;
        scope = scope->parent;
    }
    return false;
}

bool symtab_in_secure_zone(SymbolTable *table) {
    return table->current_scope && table->current_scope->is_secure_zone;
}

DataType symtab_get_return_type(SymbolTable *table) {
    Scope *scope = table->current_scope;
    while (scope) {
        if (scope->is_function_scope) {
            return scope->expected_return;
        }
        scope = scope->parent;
    }
    return TYPE_NOTHING;
}

/* ============================================================================
 * SYMBOL OPERATIONS
 * ============================================================================
 */

const char *symtab_declare_variable(SymbolTable *table, const char *name,
                                     DataType type, bool is_const,
                                     SourceLocation loc) {
    /* Check for redeclaration in current scope */
    Symbol *existing = symtab_lookup_current_scope(table, name);
    if (existing) {
        static char error_buf[256];
        snprintf(error_buf, sizeof(error_buf),
                 "Redeclaration of '%s' (previously declared at line %d)",
                 name, existing->decl_loc.first_line);
        return error_buf;
    }
    
    /* Create and add the symbol */
    Symbol *sym = symbol_create(name, 
                                is_const ? SYMBOL_CONSTANT : SYMBOL_VARIABLE,
                                type, table->scope_depth, loc);
    
    /* Add to front of current scope's symbol list */
    sym->next = table->current_scope->symbols;
    table->current_scope->symbols = sym;
    
    return NULL; /* Success */
}

const char *symtab_declare_function(SymbolTable *table, const char *name,
                                     ASTNodeList *params, DataType return_type,
                                     SourceLocation loc) {
    /* Functions should be declared at global scope (or at least check for redecl) */
    Symbol *existing = symtab_lookup_current_scope(table, name);
    if (existing) {
        static char error_buf[256];
        snprintf(error_buf, sizeof(error_buf),
                 "Redeclaration of function '%s' (previously declared at line %d)",
                 name, existing->decl_loc.first_line);
        return error_buf;
    }
    
    /* Create function symbol */
    Symbol *sym = symbol_create(name, SYMBOL_FUNCTION, TYPE_FUNCTION,
                                table->scope_depth, loc);
    sym->func_info.params = params;
    sym->func_info.return_type = return_type;
    sym->func_info.has_return = false;
    sym->is_initialized = true;  /* Functions are always "initialized" */
    
    /* Add to current scope */
    sym->next = table->current_scope->symbols;
    table->current_scope->symbols = sym;
    
    return NULL; /* Success */
}

const char *symtab_declare_parameter(SymbolTable *table, const char *name,
                                      DataType type, SourceLocation loc) {
    /* Check for duplicate parameter name */
    Symbol *existing = symtab_lookup_current_scope(table, name);
    if (existing) {
        static char error_buf[256];
        snprintf(error_buf, sizeof(error_buf),
                 "Duplicate parameter name '%s'", name);
        return error_buf;
    }
    
    /* Create parameter symbol */
    Symbol *sym = symbol_create(name, SYMBOL_PARAMETER, type,
                                table->scope_depth, loc);
    sym->is_initialized = true;  /* Parameters are initialized by caller */
    
    /* Add to current scope */
    sym->next = table->current_scope->symbols;
    table->current_scope->symbols = sym;
    
    return NULL; /* Success */
}

Symbol *symtab_lookup(SymbolTable *table, const char *name) {
    /* Search from current scope up to global scope */
    Scope *scope = table->current_scope;
    while (scope) {
        Symbol *sym = scope->symbols;
        while (sym) {
            if (strcmp(sym->name, name) == 0) {
                return sym;
            }
            sym = sym->next;
        }
        scope = scope->parent;
    }
    return NULL;  /* Not found */
}

Symbol *symtab_lookup_current_scope(SymbolTable *table, const char *name) {
    Symbol *sym = table->current_scope->symbols;
    while (sym) {
        if (strcmp(sym->name, name) == 0) {
            return sym;
        }
        sym = sym->next;
    }
    return NULL;  /* Not found */
}

Symbol *symtab_lookup_function(SymbolTable *table, const char *name) {
    Symbol *sym = symtab_lookup(table, name);
    if (sym && sym->kind == SYMBOL_FUNCTION) {
        return sym;
    }
    return NULL;
}

void symtab_mark_initialized(Symbol *sym) {
    if (sym) {
        sym->is_initialized = true;
    }
}

/* ============================================================================
 * ERROR/WARNING REPORTING
 * ============================================================================
 */

void symtab_error(SymbolTable *table, SourceLocation loc, const char *format, ...) {
    table->error_count++;
    
    fprintf(stderr, "Semantic error at line %d", loc.first_line);
    if (loc.first_column > 0) {
        fprintf(stderr, ":%d", loc.first_column);
    }
    fprintf(stderr, ": ");
    
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    
    fprintf(stderr, "\n");
}

void symtab_warning(SymbolTable *table, SourceLocation loc, const char *format, ...) {
    table->warning_count++;
    
    fprintf(stderr, "Warning at line %d", loc.first_line);
    if (loc.first_column > 0) {
        fprintf(stderr, ":%d", loc.first_column);
    }
    fprintf(stderr, ": ");
    
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    
    fprintf(stderr, "\n");
}

int symtab_error_count(SymbolTable *table) {
    return table->error_count;
}

int symtab_warning_count(SymbolTable *table) {
    return table->warning_count;
}

/* ============================================================================
 * DEBUGGING / PRINTING
 * ============================================================================
 */

const char *symbol_kind_to_string(SymbolKind kind) {
    switch (kind) {
        case SYMBOL_VARIABLE:  return "variable";
        case SYMBOL_CONSTANT:  return "constant";
        case SYMBOL_FUNCTION:  return "function";
        case SYMBOL_PARAMETER: return "parameter";
        default:               return "unknown";
    }
}

static const char *type_to_string(DataType type) {
    switch (type) {
        case TYPE_NUMBER:   return "number";
        case TYPE_DECIMAL:  return "decimal";
        case TYPE_TEXT:     return "text";
        case TYPE_FLAG:     return "flag";
        case TYPE_LIST:     return "list";
        case TYPE_NOTHING:  return "nothing";
        case TYPE_FUNCTION: return "function";
        default:            return "unknown";
    }
}

void symtab_print_scope(Scope *scope) {
    printf("  Scope level %d", scope->level);
    if (scope->is_function_scope) printf(" [function]");
    if (scope->is_loop_scope) printf(" [loop]");
    if (scope->is_secure_zone) printf(" [secure]");
    printf(":\n");
    
    Symbol *sym = scope->symbols;
    if (!sym) {
        printf("    (empty)\n");
        return;
    }
    
    while (sym) {
        printf("    %s '%s' : %s", 
               symbol_kind_to_string(sym->kind),
               sym->name,
               type_to_string(sym->type));
        
        if (sym->kind == SYMBOL_FUNCTION) {
            printf(" -> %s", type_to_string(sym->func_info.return_type));
            if (sym->func_info.params) {
                printf(" (params: %zu)", sym->func_info.params->count);
            }
        }
        
        if (!sym->is_initialized && sym->kind != SYMBOL_FUNCTION) {
            printf(" [uninitialized]");
        }
        
        printf(" (declared line %d)\n", sym->decl_loc.first_line);
        sym = sym->next;
    }
}

void symtab_print(SymbolTable *table) {
    printf("=== Symbol Table ===\n");
    printf("Current depth: %d\n", table->scope_depth);
    printf("Errors: %d, Warnings: %d\n\n", table->error_count, table->warning_count);
    
    /* Print all scopes from current to global */
    Scope *scope = table->current_scope;
    while (scope) {
        symtab_print_scope(scope);
        scope = scope->parent;
    }
    
    printf("====================\n");
}
