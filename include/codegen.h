/*
 * NatureLang Compiler
 * Copyright (c) 2026
 * 
 * Code Generator Header
 * 
 * Translates validated AST to C source code.
 */

#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include "symbol_table.h"
#include <stdio.h>

/* Code generation options */
typedef struct {
    int emit_comments;        /* Include source location comments */
    int emit_debug_info;      /* Include debugging macros */
    int use_safe_functions;   /* Use bounds-checked functions */
    int indent_size;          /* Indentation spaces (default: 4) */
} CodegenOptions;

/* Code generation result */
typedef struct {
    int success;
    char *generated_code;     /* The generated C source */
    size_t code_length;
    int error_count;
    char *error_message;
} CodegenResult;

/* Code generator state (internal) */
typedef struct CodegenContext {
    FILE *output;             /* Output stream (memory or file) */
    char *buffer;             /* Output buffer for string generation */
    size_t buffer_size;
    size_t buffer_capacity;
    int indent_level;
    int temp_var_counter;     /* For generating temporary variables */
    int label_counter;        /* For generating labels */
    SymbolTable *symtab;      /* Symbol table from semantic analysis */
    CodegenOptions options;
    int error_count;
    char error_message[1024];
    int in_function;          /* Track if we're in a function body */
    int in_loop;              /* Track if we're in a loop */
    int needs_input_buffer;   /* Track if program uses input */
    int needs_list_support;   /* Track if program uses lists */
} CodegenContext;

/*
 * Initialize code generator with options
 */
CodegenContext *codegen_create(SymbolTable *symtab, CodegenOptions *options);

/*
 * Free code generator resources
 */
void codegen_destroy(CodegenContext *ctx);

/*
 * Generate C code from AST
 * Returns the generated code as a string (caller must free)
 */
CodegenResult codegen_generate(CodegenContext *ctx, ASTNode *ast);

/*
 * Generate C code to a file
 */
int codegen_to_file(CodegenContext *ctx, ASTNode *ast, const char *filename);

/*
 * Get default code generation options
 */
CodegenOptions codegen_default_options(void);

/*
 * Helper: Convert NatureLang type to C type string
 */
const char *naturelang_type_to_c(DataType type);

/*
 * Helper: Generate unique temporary variable name
 */
char *codegen_temp_var(CodegenContext *ctx);

/*
 * Helper: Generate unique label name
 */
char *codegen_label(CodegenContext *ctx, const char *prefix);

#endif /* CODEGEN_H */
