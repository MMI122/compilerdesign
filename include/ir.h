/*
 * NatureLang Compiler
 * Copyright (c) 2026
 * 
 * Intermediate Representation (IR) - Three-Address Code (TAC)
 * 
 * This file defines the Three-Address Code intermediate representation.
 * TAC instructions have the form:
 *   result = operand1 op operand2
 * 
 * This IR sits between the AST and final code generation, enabling
 * machine-independent optimizations like constant folding, dead code
 * elimination, and copy propagation.
 */

#ifndef NATURELANG_IR_H
#define NATURELANG_IR_H

#include "ast.h"
#include <stdbool.h>
#include <stddef.h>

/* ============================================================================
 * TAC INSTRUCTION OPCODES
 * ============================================================================
 * Each opcode represents one kind of three-address instruction.
 */
typedef enum {
    /* Arithmetic */
    TAC_ADD,            /* result = arg1 + arg2 */
    TAC_SUB,            /* result = arg1 - arg2 */
    TAC_MUL,            /* result = arg1 * arg2 */
    TAC_DIV,            /* result = arg1 / arg2 */
    TAC_MOD,            /* result = arg1 % arg2 */
    TAC_POW,            /* result = arg1 ^ arg2 */
    TAC_NEG,            /* result = -arg1 (unary) */

    /* Comparison */
    TAC_EQ,             /* result = arg1 == arg2 */
    TAC_NEQ,            /* result = arg1 != arg2 */
    TAC_LT,             /* result = arg1 <  arg2 */
    TAC_GT,             /* result = arg1 >  arg2 */
    TAC_LTE,            /* result = arg1 <= arg2 */
    TAC_GTE,            /* result = arg1 >= arg2 */

    /* Logical */
    TAC_AND,            /* result = arg1 && arg2 */
    TAC_OR,             /* result = arg1 || arg2 */
    TAC_NOT,            /* result = !arg1 */

    /* Data movement */
    TAC_ASSIGN,         /* result = arg1 (copy) */
    TAC_LOAD_INT,       /* result = int_literal */
    TAC_LOAD_FLOAT,     /* result = float_literal */
    TAC_LOAD_STRING,    /* result = string_literal */
    TAC_LOAD_BOOL,      /* result = bool_literal */

    /* Control flow */
    TAC_LABEL,          /* label: */
    TAC_GOTO,           /* goto label */
    TAC_IF_GOTO,        /* if arg1 goto label */
    TAC_IF_FALSE_GOTO,  /* if !arg1 goto label */

    /* Functions */
    TAC_FUNC_BEGIN,     /* function entry (label + setup) */
    TAC_FUNC_END,       /* function exit */
    TAC_PARAM,          /* push parameter for call */
    TAC_CALL,           /* result = call func, num_args */
    TAC_RETURN,         /* return arg1 */

    /* I/O */
    TAC_DISPLAY,        /* display arg1 */
    TAC_READ,           /* result = read */
    TAC_ASK,            /* result = ask(prompt) */

    /* Variable declarations */
    TAC_DECL,           /* declare variable: name, type */

    /* Special: "is between" ternary */
    TAC_BETWEEN,        /* result = (arg1 >= arg2) && (arg1 <= arg3) */

    /* String operations */
    TAC_CONCAT,         /* result = arg1 + arg2 (string concat) */

    /* Control flow helpers */
    TAC_BREAK,          /* break out of loop */
    TAC_CONTINUE,       /* continue to loop start */

    /* Scope markers (for codegen) */
    TAC_SCOPE_BEGIN,    /* { */
    TAC_SCOPE_END,      /* } */
    TAC_SECURE_BEGIN,   /* secure zone begin */
    TAC_SECURE_END,     /* secure zone end */

    /* List operations */
    TAC_LIST_CREATE,    /* result = create_list(count) */
    TAC_LIST_APPEND,    /* list_append(list, item) */
    TAC_LIST_GET,       /* result = list[index] */
    TAC_LIST_SET,       /* list[index] = value */

    /* No-op (for optimization passes) */
    TAC_NOP,

    TAC_OPCODE_COUNT
} TACOpcode;

/* ============================================================================
 * TAC OPERAND
 * ============================================================================
 * An operand can be a temporary, a named variable, or a literal constant.
 */
typedef enum {
    OPERAND_NONE,       /* No operand (unused slot) */
    OPERAND_TEMP,       /* Temporary variable: t0, t1, t2, ... */
    OPERAND_VAR,        /* Named variable from source */
    OPERAND_INT,        /* Integer literal constant */
    OPERAND_FLOAT,      /* Floating-point literal constant */
    OPERAND_STRING,     /* String literal constant */
    OPERAND_BOOL,       /* Boolean literal constant */
    OPERAND_LABEL,      /* Label reference (for jumps) */
    OPERAND_FUNC        /* Function name (for calls) */
} OperandKind;

typedef struct {
    OperandKind kind;
    DataType data_type;     /* Type of the operand */
    union {
        int temp_id;        /* OPERAND_TEMP: temporary number */
        char *name;         /* OPERAND_VAR, OPERAND_FUNC: variable/func name */
        long long int_val;  /* OPERAND_INT */
        double float_val;   /* OPERAND_FLOAT */
        char *str_val;      /* OPERAND_STRING */
        int bool_val;       /* OPERAND_BOOL */
        int label_id;       /* OPERAND_LABEL */
    } val;
} TACOperand;

/* ============================================================================
 * TAC INSTRUCTION
 * ============================================================================
 * A single three-address code instruction.
 */
typedef struct TACInstr {
    TACOpcode opcode;
    TACOperand result;      /* Destination */
    TACOperand arg1;        /* First source operand */
    TACOperand arg2;        /* Second source operand */
    TACOperand arg3;        /* Third operand (for BETWEEN) */

    /* Metadata */
    int line_number;        /* Source line for debugging */
    bool is_dead;           /* Marked dead by optimization */

    /* Linked list */
    struct TACInstr *next;
    struct TACInstr *prev;
} TACInstr;

/* ============================================================================
 * TAC FUNCTION
 * ============================================================================
 * Holds the TAC instruction list for one function (or top-level).
 */
typedef struct TACFunction {
    char *name;             /* Function name (NULL = top-level) */
    DataType return_type;
    int param_count;
    char **param_names;
    DataType *param_types;

    TACInstr *first;        /* Head of instruction list */
    TACInstr *last;         /* Tail of instruction list */
    int instr_count;

    struct TACFunction *next; /* Next function in program */
} TACFunction;

/* ============================================================================
 * TAC PROGRAM
 * ============================================================================
 * The complete IR for the entire program.
 */
typedef struct {
    TACFunction *main_func;     /* Top-level code */
    TACFunction *functions;     /* Linked list of user functions */
    int func_count;

    int next_temp;              /* Counter for temporary names */
    int next_label;             /* Counter for labels */

    /* Statistics */
    int total_instructions;
} TACProgram;

/* ============================================================================
 * IR GENERATION (AST -> TAC)
 * ============================================================================
 */

/* Generate TAC IR from a validated AST */
TACProgram *ir_generate(ASTNode *ast);

/* Free all IR resources */
void ir_free(TACProgram *program);

/* ============================================================================
 * TAC CONSTRUCTION HELPERS
 * ============================================================================
 */

/* Create a new TAC program */
TACProgram *tac_program_create(void);

/* Create a new TAC function */
TACFunction *tac_function_create(const char *name, DataType return_type);

/* Add a function to the program */
void tac_program_add_function(TACProgram *prog, TACFunction *func);

/* Create operands */
TACOperand tac_operand_none(void);
TACOperand tac_operand_temp(int id, DataType type);
TACOperand tac_operand_var(const char *name, DataType type);
TACOperand tac_operand_int(long long value);
TACOperand tac_operand_float(double value);
TACOperand tac_operand_string(const char *value);
TACOperand tac_operand_bool(int value);
TACOperand tac_operand_label(int id);
TACOperand tac_operand_func(const char *name);

/* Allocate a new temporary */
int tac_new_temp(TACProgram *prog);

/* Allocate a new label */
int tac_new_label(TACProgram *prog);

/* Emit an instruction into a function */
TACInstr *tac_emit(TACFunction *func, TACOpcode op,
                   TACOperand result, TACOperand arg1, TACOperand arg2);

/* Emit with 3 source args (for BETWEEN) */
TACInstr *tac_emit3(TACFunction *func, TACOpcode op,
                    TACOperand result, TACOperand arg1,
                    TACOperand arg2, TACOperand arg3);

/* Emit a label */
TACInstr *tac_emit_label(TACFunction *func, int label_id);

/* Emit a jump */
TACInstr *tac_emit_goto(TACFunction *func, int label_id);

/* Emit a conditional jump */
TACInstr *tac_emit_if_goto(TACFunction *func, TACOperand cond, int label_id);
TACInstr *tac_emit_if_false_goto(TACFunction *func, TACOperand cond, int label_id);

/* ============================================================================
 * IR PRINTING (for debugging)
 * ============================================================================
 */

/* Print the entire TAC program to stdout */
void ir_print(TACProgram *program);

/* Print a single function's TAC */
void ir_print_function(TACFunction *func);

/* Print a single instruction */
void ir_print_instr(TACInstr *instr);

/* Get string name of an opcode */
const char *tac_opcode_to_string(TACOpcode op);

/* Get string representation of an operand */
const char *tac_operand_to_string(TACOperand *op);

/* ============================================================================
 * IR STATISTICS
 * ============================================================================
 */

/* Count instructions in a function */
int ir_count_instructions(TACFunction *func);

/* Count total instructions in program */
int ir_count_total(TACProgram *program);

#endif /* NATURELANG_IR_H */
