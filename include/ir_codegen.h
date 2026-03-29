/*
 * NatureLang Compiler
 * Copyright (c) 2026
 *
 * IR-based Code Generator Header
 *
 * Translates optimized TAC IR to ANSI C source code.
 * This is the textbook pipeline: AST → IR → Optimize → Code Generation.
 */

#ifndef NATURELANG_IR_CODEGEN_H
#define NATURELANG_IR_CODEGEN_H

#include "ir.h"
#include "ast.h"
#include <stdio.h>

/* ============================================================================
 * IR CODEGEN OPTIONS
 * ============================================================================
 */
typedef struct {
    int emit_comments;        /* Include TAC comment annotations */
    int emit_debug_info;      /* Include line number comments */
    int indent_size;          /* Indentation spaces (default: 4) */
} IRCodegenOptions;

/* ============================================================================
 * IR CODEGEN RESULT
 * ============================================================================
 */
typedef struct {
    int success;
    /*
    generated_code একটা pointer, যেখানে generated C code text রাখা হয়।
এই memory function-এর ভিতরে dynamically allocate করা হয় (malloc/realloc টাইপ)।
তাই যে code এই result নেয়, তার কাজ হলো পরে memory free করা।
caller must free = যে function call করেছে, সেই caller-ই free(...) করবে।
free না করলে কী হবে:

Memory leak হবে।
বারবার compile/generate করলে RAM usage বাড়বে।
    */
    char *generated_code;     /* Generated C source (caller must free) */
    size_t code_length;
    int error_count;
    char error_message[1024];
} IRCodegenResult;

/* ============================================================================
 * PUBLIC API
 * ============================================================================
 */

/* Get default options */
IRCodegenOptions ir_codegen_default_options(void);

/* Generate C code from TAC IR program */
IRCodegenResult ir_codegen_generate(TACProgram *program, IRCodegenOptions *opts);

/* Generate C code from TAC IR to a file */
int ir_codegen_to_file(TACProgram *program, IRCodegenOptions *opts,
                       const char *filename);

/* Free the generated code in a result */
void ir_codegen_result_free(IRCodegenResult *result);

#endif /* NATURELANG_IR_CODEGEN_H */
