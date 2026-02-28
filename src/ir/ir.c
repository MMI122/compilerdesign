/*
 * NatureLang Compiler
 * Copyright (c) 2026
 *
 * Intermediate Representation (IR) Implementation
 *
 * Generates Three-Address Code (TAC) from a validated AST.
 * This is the classic compiler textbook IR phase that bridges
 * semantic analysis and code optimization / code generation.
 *
 * TAC form:  result = arg1 op arg2
 *
 * Example NatureLang:
 *   create a number called x and set it to 3 plus 4 multiplied by 5
 *
 * Generated TAC:
 *   t0 = 4 * 5
 *   t1 = 3 + t0
 *   DECL x : NUMBER
 *   x = t1
 */

#define _POSIX_C_SOURCE 200809L
#include "ir.h"
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ============================================================================
 * INTERNAL CONTEXT FOR IR GENERATION
 * ============================================================================
 */
typedef struct {
    TACProgram  *program;
    TACFunction *current_func;   /* Function we are emitting into       */
    int          in_function;    /* Nesting depth inside function decls */

    /* Loop context for break / continue */
    int loop_break_label;        /* Label to jump to on 'break'    */
    int loop_continue_label;     /* Label to jump to on 'continue' */
    int in_loop;
} IRGenContext;

/* Forward declarations */
static TACOperand ir_gen_expression(IRGenContext *ctx, ASTNode *node);
static void       ir_gen_statement(IRGenContext *ctx, ASTNode *node);
static void       ir_gen_node(IRGenContext *ctx, ASTNode *node);

/* ============================================================================
 * OPERAND CONSTRUCTORS
 * ============================================================================
 */

TACOperand tac_operand_none(void) {
    TACOperand op;
    memset(&op, 0, sizeof(op));
    op.kind = OPERAND_NONE;
    op.data_type = TYPE_UNKNOWN;
    return op;
}

TACOperand tac_operand_temp(int id, DataType type) {
    TACOperand op;
    memset(&op, 0, sizeof(op));
    op.kind = OPERAND_TEMP;
    op.data_type = type;
    op.val.temp_id = id;
    return op;
}

TACOperand tac_operand_var(const char *name, DataType type) {
    TACOperand op;
    memset(&op, 0, sizeof(op));
    op.kind = OPERAND_VAR;
    op.data_type = type;
    /* Store the pointer directly; deep-copy happens in tac_instr_create */
    op.val.name = (char *)name;
    return op;
}

TACOperand tac_operand_int(long long value) {
    TACOperand op;
    memset(&op, 0, sizeof(op));
    op.kind = OPERAND_INT;
    op.data_type = TYPE_NUMBER;
    op.val.int_val = value;
    return op;
}

TACOperand tac_operand_float(double value) {
    TACOperand op;
    memset(&op, 0, sizeof(op));
    op.kind = OPERAND_FLOAT;
    op.data_type = TYPE_DECIMAL;
    op.val.float_val = value;
    return op;
}

TACOperand tac_operand_string(const char *value) {
    TACOperand op;
    memset(&op, 0, sizeof(op));
    op.kind = OPERAND_STRING;
    op.data_type = TYPE_TEXT;
    /* Store the pointer directly; deep-copy happens in tac_instr_create */
    op.val.str_val = (char *)value;
    return op;
}

TACOperand tac_operand_bool(int value) {
    TACOperand op;
    memset(&op, 0, sizeof(op));
    op.kind = OPERAND_BOOL;
    op.data_type = TYPE_FLAG;
    op.val.bool_val = value;
    return op;
}

TACOperand tac_operand_label(int id) {
    TACOperand op;
    memset(&op, 0, sizeof(op));
    op.kind = OPERAND_LABEL;
    op.data_type = TYPE_UNKNOWN;
    op.val.label_id = id;
    return op;
}

TACOperand tac_operand_func(const char *name) {
    TACOperand op;
    memset(&op, 0, sizeof(op));
    op.kind = OPERAND_FUNC;
    op.data_type = TYPE_FUNCTION;
    /* Store the pointer directly; deep-copy happens in tac_instr_create */
    op.val.name = (char *)name;
    return op;
}

/* ============================================================================
 * TAC INSTRUCTION HELPERS
 * ============================================================================
 */

static void free_operand(TACOperand *op) {
    if (!op) return;
    if (op->kind == OPERAND_VAR || op->kind == OPERAND_FUNC) {
        free(op->val.name);
        op->val.name = NULL;
    } else if (op->kind == OPERAND_STRING) {
        free(op->val.str_val);
        op->val.str_val = NULL;
    }
}

/* Deep copy an operand (strdup any heap pointers) */
static TACOperand copy_operand(TACOperand src) {
    TACOperand dst = src;
    if (src.kind == OPERAND_VAR || src.kind == OPERAND_FUNC) {
        dst.val.name = src.val.name ? strdup(src.val.name) : NULL;
    } else if (src.kind == OPERAND_STRING) {
        dst.val.str_val = src.val.str_val ? strdup(src.val.str_val) : NULL;
    }
    return dst;
}

static TACInstr *tac_instr_create(TACOpcode opcode,
                                   TACOperand result,
                                   TACOperand arg1,
                                   TACOperand arg2) {
    TACInstr *instr = calloc(1, sizeof(TACInstr));
    instr->opcode = opcode;
    instr->result = copy_operand(result);
    instr->arg1   = copy_operand(arg1);
    instr->arg2   = copy_operand(arg2);
    instr->arg3   = tac_operand_none();
    instr->is_dead = false;
    return instr;
}

static void tac_instr_free(TACInstr *instr) {
    if (!instr) return;
    free_operand(&instr->result);
    free_operand(&instr->arg1);
    free_operand(&instr->arg2);
    free_operand(&instr->arg3);
    free(instr);
}

/* Append instruction to function's doubly-linked list */
static void tac_func_append(TACFunction *func, TACInstr *instr) {
    if (!func || !instr) return;
    instr->prev = func->last;
    instr->next = NULL;
    if (func->last) {
        func->last->next = instr;
    } else {
        func->first = instr;
    }
    func->last = instr;
    func->instr_count++;
}

/* ============================================================================
 * PUBLIC EMIT FUNCTIONS
 * ============================================================================
 */

int tac_new_temp(TACProgram *prog) {
    return prog->next_temp++;
}

int tac_new_label(TACProgram *prog) {
    return prog->next_label++;
}

TACInstr *tac_emit(TACFunction *func, TACOpcode op,
                   TACOperand result, TACOperand arg1, TACOperand arg2) {
    TACInstr *instr = tac_instr_create(op, result, arg1, arg2);
    tac_func_append(func, instr);
    return instr;
}

TACInstr *tac_emit3(TACFunction *func, TACOpcode op,
                    TACOperand result, TACOperand arg1,
                    TACOperand arg2, TACOperand arg3) {
    TACInstr *instr = tac_instr_create(op, result, arg1, arg2);
    instr->arg3 = copy_operand(arg3);
    tac_func_append(func, instr);
    return instr;
}

TACInstr *tac_emit_label(TACFunction *func, int label_id) {
    return tac_emit(func, TAC_LABEL,
                    tac_operand_label(label_id),
                    tac_operand_none(),
                    tac_operand_none());
}

TACInstr *tac_emit_goto(TACFunction *func, int label_id) {
    return tac_emit(func, TAC_GOTO,
                    tac_operand_label(label_id),
                    tac_operand_none(),
                    tac_operand_none());
}

TACInstr *tac_emit_if_goto(TACFunction *func, TACOperand cond, int label_id) {
    return tac_emit(func, TAC_IF_GOTO,
                    tac_operand_label(label_id),
                    cond,
                    tac_operand_none());
}

TACInstr *tac_emit_if_false_goto(TACFunction *func, TACOperand cond, int label_id) {
    return tac_emit(func, TAC_IF_FALSE_GOTO,
                    tac_operand_label(label_id),
                    cond,
                    tac_operand_none());
}

/* ============================================================================
 * TAC PROGRAM / FUNCTION MANAGEMENT
 * ============================================================================
 */

TACProgram *tac_program_create(void) {
    TACProgram *prog = calloc(1, sizeof(TACProgram));
    /* Create the implicit "main" function for top-level code */
    prog->main_func = tac_function_create(NULL, TYPE_NOTHING);
    return prog;
}

TACFunction *tac_function_create(const char *name, DataType return_type) {
    TACFunction *func = calloc(1, sizeof(TACFunction));
    func->name = name ? strdup(name) : NULL;
    func->return_type = return_type;
    return func;
}

void tac_program_add_function(TACProgram *prog, TACFunction *func) {
    if (!prog || !func) return;
    func->next = prog->functions;
    prog->functions = func;
    prog->func_count++;
}

/* ============================================================================
 * FREE
 * ============================================================================
 */

static void tac_function_free(TACFunction *func) {
    if (!func) return;
    /* Free instructions */
    TACInstr *instr = func->first;
    while (instr) {
        TACInstr *next = instr->next;
        tac_instr_free(instr);
        instr = next;
    }
    /* Free param names */
    for (int i = 0; i < func->param_count; i++) {
        free(func->param_names[i]);
    }
    free(func->param_names);
    free(func->param_types);
    free(func->name);
    free(func);
}

void ir_free(TACProgram *program) {
    if (!program) return;
    tac_function_free(program->main_func);
    TACFunction *f = program->functions;
    while (f) {
        TACFunction *next = f->next;
        tac_function_free(f);
        f = next;
    }
    free(program);
}

/* ============================================================================
 * IR STATISTICS
 * ============================================================================
 */

int ir_count_instructions(TACFunction *func) {
    if (!func) return 0;
    return func->instr_count;
}

int ir_count_total(TACProgram *program) {
    if (!program) return 0;
    int total = ir_count_instructions(program->main_func);
    TACFunction *f = program->functions;
    while (f) {
        total += ir_count_instructions(f);
        f = f->next;
    }
    return total;
}

/* ============================================================================
 * IR PRINTING
 * ============================================================================
 */

const char *tac_opcode_to_string(TACOpcode op) {
    switch (op) {
        case TAC_ADD:           return "ADD";
        case TAC_SUB:           return "SUB";
        case TAC_MUL:           return "MUL";
        case TAC_DIV:           return "DIV";
        case TAC_MOD:           return "MOD";
        case TAC_POW:           return "POW";
        case TAC_NEG:           return "NEG";
        case TAC_EQ:            return "EQ";
        case TAC_NEQ:           return "NEQ";
        case TAC_LT:            return "LT";
        case TAC_GT:            return "GT";
        case TAC_LTE:           return "LTE";
        case TAC_GTE:           return "GTE";
        case TAC_AND:           return "AND";
        case TAC_OR:            return "OR";
        case TAC_NOT:           return "NOT";
        case TAC_ASSIGN:        return "ASSIGN";
        case TAC_LOAD_INT:      return "LOAD_INT";
        case TAC_LOAD_FLOAT:    return "LOAD_FLOAT";
        case TAC_LOAD_STRING:   return "LOAD_STRING";
        case TAC_LOAD_BOOL:     return "LOAD_BOOL";
        case TAC_LABEL:         return "LABEL";
        case TAC_GOTO:          return "GOTO";
        case TAC_IF_GOTO:       return "IF_GOTO";
        case TAC_IF_FALSE_GOTO: return "IF_FALSE_GOTO";
        case TAC_FUNC_BEGIN:    return "FUNC_BEGIN";
        case TAC_FUNC_END:      return "FUNC_END";
        case TAC_PARAM:         return "PARAM";
        case TAC_CALL:          return "CALL";
        case TAC_RETURN:        return "RETURN";
        case TAC_DISPLAY:       return "DISPLAY";
        case TAC_READ:          return "READ";
        case TAC_ASK:           return "ASK";
        case TAC_DECL:          return "DECL";
        case TAC_BETWEEN:       return "BETWEEN";
        case TAC_CONCAT:        return "CONCAT";
        case TAC_BREAK:         return "BREAK";
        case TAC_CONTINUE:      return "CONTINUE";
        case TAC_SCOPE_BEGIN:   return "SCOPE_BEGIN";
        case TAC_SCOPE_END:     return "SCOPE_END";
        case TAC_SECURE_BEGIN:  return "SECURE_BEGIN";
        case TAC_SECURE_END:    return "SECURE_END";
        case TAC_LIST_CREATE:   return "LIST_CREATE";
        case TAC_LIST_APPEND:   return "LIST_APPEND";
        case TAC_LIST_GET:      return "LIST_GET";
        case TAC_LIST_SET:      return "LIST_SET";
        case TAC_NOP:           return "NOP";
        case TAC_OPCODE_COUNT:  return "???";
    }
    return "???";
}

/* Thread-local buffer for operand string formatting */
static __thread char operand_buf[256];

const char *tac_operand_to_string(TACOperand *op) {
    if (!op) return "?";
    switch (op->kind) {
        case OPERAND_NONE:
            return "_";
        case OPERAND_TEMP:
            snprintf(operand_buf, sizeof(operand_buf), "t%d", op->val.temp_id);
            return operand_buf;
        case OPERAND_VAR:
            return op->val.name ? op->val.name : "?var";
        case OPERAND_INT:
            snprintf(operand_buf, sizeof(operand_buf), "%lld", op->val.int_val);
            return operand_buf;
        case OPERAND_FLOAT:
            snprintf(operand_buf, sizeof(operand_buf), "%g", op->val.float_val);
            return operand_buf;
        case OPERAND_STRING:
            snprintf(operand_buf, sizeof(operand_buf), "\"%s\"",
                     op->val.str_val ? op->val.str_val : "");
            return operand_buf;
        case OPERAND_BOOL:
            return op->val.bool_val ? "true" : "false";
        case OPERAND_LABEL:
            snprintf(operand_buf, sizeof(operand_buf), "L%d", op->val.label_id);
            return operand_buf;
        case OPERAND_FUNC:
            return op->val.name ? op->val.name : "?func";
    }
    return "?";
}

void ir_print_instr(TACInstr *instr) {
    if (!instr) return;
    if (instr->is_dead) {
        printf("  ; DEAD: ");
    }

    switch (instr->opcode) {
        case TAC_LABEL:
            printf("L%d:\n", instr->result.val.label_id);
            return;

        case TAC_GOTO:
            printf("  goto L%d\n", instr->result.val.label_id);
            return;

        case TAC_IF_GOTO:
            printf("  if %s goto L%d\n",
                   tac_operand_to_string(&instr->arg1),
                   instr->result.val.label_id);
            return;

        case TAC_IF_FALSE_GOTO:
            printf("  ifFalse %s goto L%d\n",
                   tac_operand_to_string(&instr->arg1),
                   instr->result.val.label_id);
            return;

        case TAC_FUNC_BEGIN:
            printf("  FUNC_BEGIN %s\n", tac_operand_to_string(&instr->result));
            return;

        case TAC_FUNC_END:
            printf("  FUNC_END\n");
            return;

        case TAC_PARAM:
            printf("  param %s\n", tac_operand_to_string(&instr->arg1));
            return;

        case TAC_CALL: {
            /* result = call func, nargs */
            char res_buf[256], func_buf[256], nargs_buf[256];
            snprintf(res_buf, sizeof(res_buf), "%s",
                     tac_operand_to_string(&instr->result));
            snprintf(func_buf, sizeof(func_buf), "%s",
                     tac_operand_to_string(&instr->arg1));
            snprintf(nargs_buf, sizeof(nargs_buf), "%s",
                     tac_operand_to_string(&instr->arg2));
            if (instr->result.kind != OPERAND_NONE) {
                printf("  %s = call %s, %s\n", res_buf, func_buf, nargs_buf);
            } else {
                printf("  call %s, %s\n", func_buf, nargs_buf);
            }
            return;
        }

        case TAC_RETURN:
            if (instr->arg1.kind != OPERAND_NONE) {
                printf("  return %s\n", tac_operand_to_string(&instr->arg1));
            } else {
                printf("  return\n");
            }
            return;

        case TAC_DISPLAY:
            printf("  display %s\n", tac_operand_to_string(&instr->arg1));
            return;

        case TAC_READ:
            printf("  %s = read\n", tac_operand_to_string(&instr->result));
            return;

        case TAC_ASK: {
            const char *res = tac_operand_to_string(&instr->result);
            char prompt_buf[256];
            snprintf(prompt_buf, sizeof(prompt_buf), "%s",
                     tac_operand_to_string(&instr->arg1));
            printf("  %s = ask(%s)\n", res, prompt_buf);
            return;
        }

        case TAC_DECL: {
            const char *name = tac_operand_to_string(&instr->result);
            const char *type_str = ast_data_type_to_string(instr->result.data_type);
            printf("  DECL %s : %s\n", name, type_str);
            return;
        }

        case TAC_BETWEEN: {
            const char *res = tac_operand_to_string(&instr->result);
            char a1[256], a2[256], a3[256];
            snprintf(a1, sizeof(a1), "%s", tac_operand_to_string(&instr->arg1));
            snprintf(a2, sizeof(a2), "%s", tac_operand_to_string(&instr->arg2));
            snprintf(a3, sizeof(a3), "%s", tac_operand_to_string(&instr->arg3));
            printf("  %s = %s between %s and %s\n", res, a1, a2, a3);
            return;
        }

        case TAC_SCOPE_BEGIN:
            printf("  SCOPE_BEGIN\n");
            return;
        case TAC_SCOPE_END:
            printf("  SCOPE_END\n");
            return;
        case TAC_SECURE_BEGIN:
            printf("  SECURE_BEGIN\n");
            return;
        case TAC_SECURE_END:
            printf("  SECURE_END\n");
            return;

        case TAC_BREAK:
            printf("  BREAK\n");
            return;
        case TAC_CONTINUE:
            printf("  CONTINUE\n");
            return;

        case TAC_NOP:
            printf("  nop\n");
            return;

        default:
            break;
    }

    /* Generic: result = arg1 OP arg2  (or result = OP arg1) */
    const char *res_str = tac_operand_to_string(&instr->result);
    /* Copy to local buf so second operand call doesn't clobber */
    char res_buf[256];
    snprintf(res_buf, sizeof(res_buf), "%s", res_str);

    char a1_buf[256];
    snprintf(a1_buf, sizeof(a1_buf), "%s", tac_operand_to_string(&instr->arg1));

    if (instr->arg2.kind != OPERAND_NONE) {
        printf("  %s = %s %s %s\n",
               res_buf, a1_buf,
               tac_opcode_to_string(instr->opcode),
               tac_operand_to_string(&instr->arg2));
    } else if (instr->arg1.kind != OPERAND_NONE) {
        printf("  %s = %s %s\n",
               res_buf,
               tac_opcode_to_string(instr->opcode),
               a1_buf);
    } else {
        printf("  %s = %s\n",
               res_buf,
               tac_opcode_to_string(instr->opcode));
    }
}

void ir_print_function(TACFunction *func) {
    if (!func) return;
    if (func->name) {
        printf("function %s(", func->name);
        for (int i = 0; i < func->param_count; i++) {
            if (i > 0) printf(", ");
            printf("%s: %s",
                   func->param_names[i],
                   ast_data_type_to_string(func->param_types[i]));
        }
        printf(") -> %s\n", ast_data_type_to_string(func->return_type));
    } else {
        printf("function <main>\n");
    }

    TACInstr *instr = func->first;
    while (instr) {
        ir_print_instr(instr);
        instr = instr->next;
    }
    printf("  [%d instructions]\n\n", func->instr_count);
}

void ir_print(TACProgram *program) {
    if (!program) return;
    printf("=== NatureLang TAC IR ===\n");
    printf("Temps: %d, Labels: %d\n\n", program->next_temp, program->next_label);

    /* Print user-defined functions first */
    TACFunction *f = program->functions;
    while (f) {
        ir_print_function(f);
        f = f->next;
    }

    /* Print main (top-level) */
    ir_print_function(program->main_func);

    printf("Total instructions: %d\n", ir_count_total(program));
    printf("=========================\n");
}

/* ============================================================================
 * AST -> TAC LOWERING  (Expression)
 *
 * Each expression returns a TACOperand representing its result.
 * Temporaries are allocated for intermediate results.
 * ============================================================================
 */

static TACOpcode operator_to_tac(Operator op) {
    switch (op) {
        case OP_ADD: return TAC_ADD;
        case OP_SUB: return TAC_SUB;
        case OP_MUL: return TAC_MUL;
        case OP_DIV: return TAC_DIV;
        case OP_MOD: return TAC_MOD;
        case OP_POW: return TAC_POW;
        case OP_EQ:  return TAC_EQ;
        case OP_NEQ: return TAC_NEQ;
        case OP_LT:  return TAC_LT;
        case OP_GT:  return TAC_GT;
        case OP_LTE: return TAC_LTE;
        case OP_GTE: return TAC_GTE;
        case OP_AND: return TAC_AND;
        case OP_OR:  return TAC_OR;
        case OP_NOT: return TAC_NOT;
        case OP_NEG: return TAC_NEG;
        case OP_BETWEEN: return TAC_BETWEEN;
        default:     return TAC_NOP;
    }
}

/* Determine the result type for a binary operation */
static DataType binop_result_type(Operator op, DataType left, DataType right) {
    /* Comparison / logical always produce FLAG */
    switch (op) {
        case OP_EQ: case OP_NEQ: case OP_LT: case OP_GT:
        case OP_LTE: case OP_GTE: case OP_AND: case OP_OR:
        case OP_BETWEEN:
            return TYPE_FLAG;
        default:
            break;
    }
    /* String concat */
    if (op == OP_ADD && (left == TYPE_TEXT || right == TYPE_TEXT))
        return TYPE_TEXT;
    /* Decimal promotion */
    if (left == TYPE_DECIMAL || right == TYPE_DECIMAL)
        return TYPE_DECIMAL;
    return left;
}

static TACOperand ir_gen_expression(IRGenContext *ctx, ASTNode *node) {
    if (!node) return tac_operand_none();

    TACProgram  *prog = ctx->program;
    TACFunction *func = ctx->current_func;

    switch (node->type) {
        /* ---- Literals ---- */
        case AST_LITERAL_INT: {
            int t = tac_new_temp(prog);
            TACOperand dst = tac_operand_temp(t, TYPE_NUMBER);
            tac_emit(func, TAC_LOAD_INT, dst,
                     tac_operand_int(node->data.literal_int.value),
                     tac_operand_none());
            return tac_operand_temp(t, TYPE_NUMBER);
        }

        case AST_LITERAL_FLOAT: {
            int t = tac_new_temp(prog);
            TACOperand dst = tac_operand_temp(t, TYPE_DECIMAL);
            tac_emit(func, TAC_LOAD_FLOAT, dst,
                     tac_operand_float(node->data.literal_float.value),
                     tac_operand_none());
            return tac_operand_temp(t, TYPE_DECIMAL);
        }

        case AST_LITERAL_STRING: {
            int t = tac_new_temp(prog);
            TACOperand dst = tac_operand_temp(t, TYPE_TEXT);
            tac_emit(func, TAC_LOAD_STRING, dst,
                     tac_operand_string(node->data.literal_string.value),
                     tac_operand_none());
            return tac_operand_temp(t, TYPE_TEXT);
        }

        case AST_LITERAL_BOOL: {
            int t = tac_new_temp(prog);
            TACOperand dst = tac_operand_temp(t, TYPE_FLAG);
            tac_emit(func, TAC_LOAD_BOOL, dst,
                     tac_operand_bool(node->data.literal_bool.value),
                     tac_operand_none());
            return tac_operand_temp(t, TYPE_FLAG);
        }

        /* ---- Identifier ---- */
        case AST_IDENTIFIER: {
            DataType dt = node->data_type != TYPE_UNKNOWN
                          ? node->data_type : TYPE_NUMBER;
            return tac_operand_var(node->data.identifier.name, dt);
        }

        /* ---- Binary Operation ---- */
        case AST_BINARY_OP: {
            Operator op = node->data.binary_op.op;
            TACOperand left  = ir_gen_expression(ctx, node->data.binary_op.left);
            TACOperand right = ir_gen_expression(ctx, node->data.binary_op.right);

            DataType res_type = binop_result_type(op, left.data_type, right.data_type);

            /* String concatenation */
            if (op == OP_ADD &&
                (left.data_type == TYPE_TEXT || right.data_type == TYPE_TEXT)) {
                int t = tac_new_temp(prog);
                TACOperand dst = tac_operand_temp(t, TYPE_TEXT);
                tac_emit(func, TAC_CONCAT, dst, left, right);
                return tac_operand_temp(t, TYPE_TEXT);
            }

            int t = tac_new_temp(prog);
            TACOperand dst = tac_operand_temp(t, res_type);
            tac_emit(func, operator_to_tac(op), dst, left, right);
            return tac_operand_temp(t, res_type);
        }

        /* ---- Unary Operation ---- */
        case AST_UNARY_OP: {
            Operator op = node->data.unary_op.op;
            TACOperand operand = ir_gen_expression(ctx, node->data.unary_op.operand);

            DataType res_type = (op == OP_NOT) ? TYPE_FLAG : operand.data_type;
            int t = tac_new_temp(prog);
            TACOperand dst = tac_operand_temp(t, res_type);
            tac_emit(func, operator_to_tac(op), dst, operand, tac_operand_none());
            return tac_operand_temp(t, res_type);
        }

        /* ---- Ternary: "is between" ---- */
        case AST_TERNARY_OP: {
            TACOperand val   = ir_gen_expression(ctx, node->data.ternary_op.operand);
            TACOperand lower = ir_gen_expression(ctx, node->data.ternary_op.lower);
            TACOperand upper = ir_gen_expression(ctx, node->data.ternary_op.upper);

            int t = tac_new_temp(prog);
            TACOperand dst = tac_operand_temp(t, TYPE_FLAG);
            tac_emit3(func, TAC_BETWEEN, dst, val, lower, upper);
            return tac_operand_temp(t, TYPE_FLAG);
        }

        /* ---- Function Call ---- */
        case AST_FUNC_CALL: {
            ASTNodeList *args = node->data.func_call.args;
            int nargs = args ? (int)args->count : 0;

            /* Evaluate and push parameters in order */
            for (int i = 0; i < nargs; i++) {
                TACOperand arg = ir_gen_expression(ctx, args->nodes[i]);
                tac_emit(func, TAC_PARAM, tac_operand_none(), arg, tac_operand_none());
            }

            DataType ret_type = node->data_type != TYPE_UNKNOWN
                                ? node->data_type : TYPE_NUMBER;
            int t = tac_new_temp(prog);
            TACOperand dst = tac_operand_temp(t, ret_type);
            tac_emit(func, TAC_CALL, dst,
                     tac_operand_func(node->data.func_call.name),
                     tac_operand_int(nargs));
            return tac_operand_temp(t, ret_type);
        }

        /* ---- List Literal ---- */
        case AST_LIST: {
            ASTNodeList *elems = node->data.list_literal.elements;
            int count = elems ? (int)elems->count : 0;
            int t = tac_new_temp(prog);
            TACOperand dst = tac_operand_temp(t, TYPE_LIST);
            tac_emit(func, TAC_LIST_CREATE, dst,
                     tac_operand_int(count), tac_operand_none());

            for (int i = 0; i < count; i++) {
                TACOperand elem = ir_gen_expression(ctx, elems->nodes[i]);
                tac_emit(func, TAC_LIST_APPEND,
                         tac_operand_temp(t, TYPE_LIST),
                         elem, tac_operand_none());
            }
            return tac_operand_temp(t, TYPE_LIST);
        }

        /* ---- Index Access ---- */
        case AST_INDEX: {
            TACOperand arr = ir_gen_expression(ctx, node->data.index_expr.array);
            TACOperand idx = ir_gen_expression(ctx, node->data.index_expr.index);
            DataType elem_type = node->data_type != TYPE_UNKNOWN
                                 ? node->data_type : TYPE_NUMBER;
            int t = tac_new_temp(prog);
            TACOperand dst = tac_operand_temp(t, elem_type);
            tac_emit(func, TAC_LIST_GET, dst, arr, idx);
            return tac_operand_temp(t, elem_type);
        }

        default:
            /* Unsupported expression – return none */
            fprintf(stderr, "IR warning: unhandled expression node type %d\n",
                    node->type);
            return tac_operand_none();
    }
}

/* ============================================================================
 * AST -> TAC LOWERING  (Statement)
 * ============================================================================
 */

static void ir_gen_statement(IRGenContext *ctx, ASTNode *node) {
    if (!node) return;

    TACProgram  *prog = ctx->program;
    TACFunction *func = ctx->current_func;

    switch (node->type) {

        /* ---- Variable Declaration ---- */
        case AST_VAR_DECL: {
            DataType vt = node->data.var_decl.var_type;
            TACOperand var_op = tac_operand_var(node->data.var_decl.name, vt);

            /* Emit declaration marker */
            tac_emit(func, TAC_DECL, var_op, tac_operand_none(), tac_operand_none());

            /* If initializer present, evaluate and assign */
            if (node->data.var_decl.initializer) {
                TACOperand val = ir_gen_expression(ctx, node->data.var_decl.initializer);
                tac_emit(func, TAC_ASSIGN,
                         tac_operand_var(node->data.var_decl.name, vt),
                         val, tac_operand_none());
            }
            break;
        }

        /* ---- Assignment ---- */
        case AST_ASSIGN: {
            TACOperand val = ir_gen_expression(ctx, node->data.assign.value);

            /* Target could be identifier or index */
            ASTNode *target = node->data.assign.target;
            if (target->type == AST_INDEX) {
                /* list[idx] = val  →  LIST_SET list, idx, val */
                TACOperand arr = ir_gen_expression(ctx, target->data.index_expr.array);
                TACOperand idx = ir_gen_expression(ctx, target->data.index_expr.index);
                tac_emit3(func, TAC_LIST_SET, arr, idx, val, tac_operand_none());
            } else if (target->type == AST_IDENTIFIER) {
                DataType dt = target->data_type != TYPE_UNKNOWN
                              ? target->data_type : TYPE_NUMBER;
                tac_emit(func, TAC_ASSIGN,
                         tac_operand_var(target->data.identifier.name, dt),
                         val, tac_operand_none());
            } else {
                /* Fallback */
                TACOperand tgt = ir_gen_expression(ctx, target);
                tac_emit(func, TAC_ASSIGN, tgt, val, tac_operand_none());
            }
            break;
        }

        /* ---- Display ---- */
        case AST_DISPLAY: {
            TACOperand val = ir_gen_expression(ctx, node->data.display_stmt.value);
            tac_emit(func, TAC_DISPLAY, tac_operand_none(), val, tac_operand_none());
            break;
        }

        /* ---- Ask (input with prompt) ---- */
        case AST_ASK: {
            TACOperand prompt = node->data.ask_stmt.prompt
                                ? ir_gen_expression(ctx, node->data.ask_stmt.prompt)
                                : tac_operand_none();
            TACOperand var_op = tac_operand_var(node->data.ask_stmt.target_var, TYPE_TEXT);
            tac_emit(func, TAC_ASK, var_op, prompt, tac_operand_none());
            break;
        }

        /* ---- Read (simple input) ---- */
        case AST_READ: {
            TACOperand var_op = tac_operand_var(node->data.read_stmt.target_var, TYPE_TEXT);
            tac_emit(func, TAC_READ, var_op, tac_operand_none(), tac_operand_none());
            break;
        }

        /* ---- If / Else ---- */
        case AST_IF: {
            TACOperand cond = ir_gen_expression(ctx, node->data.if_stmt.condition);

            if (node->data.if_stmt.else_branch) {
                int else_label = tac_new_label(prog);
                int end_label  = tac_new_label(prog);

                tac_emit_if_false_goto(func, cond, else_label);
                /* then branch */
                ir_gen_node(ctx, node->data.if_stmt.then_branch);
                tac_emit_goto(func, end_label);
                /* else branch */
                tac_emit_label(func, else_label);
                ir_gen_node(ctx, node->data.if_stmt.else_branch);
                tac_emit_label(func, end_label);
            } else {
                int end_label = tac_new_label(prog);
                tac_emit_if_false_goto(func, cond, end_label);
                ir_gen_node(ctx, node->data.if_stmt.then_branch);
                tac_emit_label(func, end_label);
            }
            break;
        }

        /* ---- While Loop ---- */
        case AST_WHILE: {
            int loop_start = tac_new_label(prog);
            int loop_end   = tac_new_label(prog);

            int saved_break    = ctx->loop_break_label;
            int saved_continue = ctx->loop_continue_label;
            ctx->loop_break_label    = loop_end;
            ctx->loop_continue_label = loop_start;
            ctx->in_loop++;

            tac_emit_label(func, loop_start);
            TACOperand cond = ir_gen_expression(ctx, node->data.while_stmt.condition);
            tac_emit_if_false_goto(func, cond, loop_end);
            ir_gen_node(ctx, node->data.while_stmt.body);
            tac_emit_goto(func, loop_start);
            tac_emit_label(func, loop_end);

            ctx->in_loop--;
            ctx->loop_break_label    = saved_break;
            ctx->loop_continue_label = saved_continue;
            break;
        }

        /* ---- Repeat N times ---- */
        case AST_REPEAT: {
            /* iter = 0; limit = count; L_start: if iter >= limit goto L_end; body; iter++; goto L_start; L_end: */
            TACOperand limit = ir_gen_expression(ctx, node->data.repeat_stmt.count);
            int iter_t = tac_new_temp(prog);
            TACOperand iter = tac_operand_temp(iter_t, TYPE_NUMBER);

            /* iter = 0 */
            tac_emit(func, TAC_LOAD_INT, iter,
                     tac_operand_int(0), tac_operand_none());

            int loop_start = tac_new_label(prog);
            int loop_end   = tac_new_label(prog);
            int loop_inc   = tac_new_label(prog);

            int saved_break    = ctx->loop_break_label;
            int saved_continue = ctx->loop_continue_label;
            ctx->loop_break_label    = loop_end;
            ctx->loop_continue_label = loop_inc;
            ctx->in_loop++;

            tac_emit_label(func, loop_start);
            /* cond = iter >= limit */
            int cond_t = tac_new_temp(prog);
            TACOperand cond = tac_operand_temp(cond_t, TYPE_FLAG);
            tac_emit(func, TAC_GTE, cond,
                     tac_operand_temp(iter_t, TYPE_NUMBER), limit);
            tac_emit_if_goto(func, cond, loop_end);

            ir_gen_node(ctx, node->data.repeat_stmt.body);

            /* iter = iter + 1 */
            tac_emit_label(func, loop_inc);
            tac_emit(func, TAC_ADD,
                     tac_operand_temp(iter_t, TYPE_NUMBER),
                     tac_operand_temp(iter_t, TYPE_NUMBER),
                     tac_operand_int(1));
            tac_emit_goto(func, loop_start);
            tac_emit_label(func, loop_end);

            ctx->in_loop--;
            ctx->loop_break_label    = saved_break;
            ctx->loop_continue_label = saved_continue;
            break;
        }

        /* ---- For-Each ---- */
        case AST_FOR_EACH: {
            /* list = iterable; idx = 0; L_start: if idx >= list.len goto end; item = list[idx]; body; idx++; goto start; end: */
            TACOperand list = ir_gen_expression(ctx, node->data.for_each_stmt.iterable);

            int idx_t = tac_new_temp(prog);
            TACOperand idx = tac_operand_temp(idx_t, TYPE_NUMBER);
            tac_emit(func, TAC_LOAD_INT, idx,
                     tac_operand_int(0), tac_operand_none());

            int loop_start = tac_new_label(prog);
            int loop_end   = tac_new_label(prog);
            int loop_inc   = tac_new_label(prog);

            int saved_break    = ctx->loop_break_label;
            int saved_continue = ctx->loop_continue_label;
            ctx->loop_break_label    = loop_end;
            ctx->loop_continue_label = loop_inc;
            ctx->in_loop++;

            tac_emit_label(func, loop_start);

            /* Get list length -- we model this as a special LIST_GET with
               a sentinel. For simplicity, we emit a higher-level construct
               that the codegen back-end knows about. We'll use the list 
               operand's inherent length check done at codegen time. 
               For the IR we use a pseudo-GTE compare against a 
               LIST_CREATE-counted value. Simplified: emit a comment and
               use the index directly. */
            /* cond = idx >= list.length  → we emit as GTE with list operand */
            /* Actually, let's emit it properly with a temp for the length */
            int len_t = tac_new_temp(prog);
            TACOperand len = tac_operand_temp(len_t, TYPE_NUMBER);
            /* We use LIST_GET with index -1 as a "get length" convention,
               but that's not clean. Better: emit a special CALL to get len */
            tac_emit(func, TAC_CALL, len,
                     tac_operand_func("__list_length"),
                     tac_operand_int(0));
            /* Actually pass the list as a param first */
            /* Let me restructure – put param before call */
            /* We need to fix the order. Let's remove that and redo. */
            /* The instruction is already appended – we'll fix by emitting properly. 
               For a clean IR, use PARAM then CALL. But since we already emitted CALL,
               let's just overwrite approach: emit a param before, then the call.
               Actually instructions are linked list, so we can just emit them in order.
               Let me just re-emit properly. The stale CALL will remain but we mark it dead. */

            /* Mark the bad instruction dead and re-emit */
            func->last->is_dead = true;

            tac_emit(func, TAC_PARAM, tac_operand_none(), list, tac_operand_none());
            tac_emit(func, TAC_CALL, tac_operand_temp(len_t, TYPE_NUMBER),
                     tac_operand_func("__list_length"),
                     tac_operand_int(1));

            int cond_t = tac_new_temp(prog);
            TACOperand cond = tac_operand_temp(cond_t, TYPE_FLAG);
            tac_emit(func, TAC_GTE, cond,
                     tac_operand_temp(idx_t, TYPE_NUMBER),
                     tac_operand_temp(len_t, TYPE_NUMBER));
            tac_emit_if_goto(func, cond, loop_end);

            /* item = list[idx] */
            TACOperand item_var = tac_operand_var(node->data.for_each_stmt.iterator_name, TYPE_NUMBER);
            tac_emit(func, TAC_DECL, item_var, tac_operand_none(), tac_operand_none());
            int elem_t = tac_new_temp(prog);
            tac_emit(func, TAC_LIST_GET,
                     tac_operand_temp(elem_t, TYPE_NUMBER),
                     list,
                     tac_operand_temp(idx_t, TYPE_NUMBER));
            tac_emit(func, TAC_ASSIGN,
                     tac_operand_var(node->data.for_each_stmt.iterator_name, TYPE_NUMBER),
                     tac_operand_temp(elem_t, TYPE_NUMBER),
                     tac_operand_none());

            ir_gen_node(ctx, node->data.for_each_stmt.body);

            tac_emit_label(func, loop_inc);
            tac_emit(func, TAC_ADD,
                     tac_operand_temp(idx_t, TYPE_NUMBER),
                     tac_operand_temp(idx_t, TYPE_NUMBER),
                     tac_operand_int(1));
            tac_emit_goto(func, loop_start);
            tac_emit_label(func, loop_end);

            ctx->in_loop--;
            ctx->loop_break_label    = saved_break;
            ctx->loop_continue_label = saved_continue;
            break;
        }

        /* ---- Function Declaration ---- */
        case AST_FUNC_DECL: {
            TACFunction *new_func = tac_function_create(
                node->data.func_decl.name,
                node->data.func_decl.return_type);

            /* Parameters */
            ASTNodeList *params = node->data.func_decl.params;
            if (params && params->count > 0) {
                new_func->param_count = (int)params->count;
                new_func->param_names = calloc(params->count, sizeof(char *));
                new_func->param_types = calloc(params->count, sizeof(DataType));
                for (size_t i = 0; i < params->count; i++) {
                    ASTNode *p = params->nodes[i];
                    new_func->param_names[i] = strdup(p->data.param_decl.name);
                    new_func->param_types[i] = p->data.param_decl.param_type;
                }
            }

            tac_emit(new_func, TAC_FUNC_BEGIN,
                     tac_operand_func(node->data.func_decl.name),
                     tac_operand_none(), tac_operand_none());

            /* Switch context to new function */
            TACFunction *saved_func = ctx->current_func;
            int saved_in_func = ctx->in_function;
            ctx->current_func = new_func;
            ctx->in_function = 1;

            /* Generate body */
            ir_gen_node(ctx, node->data.func_decl.body);

            tac_emit(new_func, TAC_FUNC_END,
                     tac_operand_none(), tac_operand_none(), tac_operand_none());

            /* Restore context */
            ctx->current_func = saved_func;
            ctx->in_function = saved_in_func;

            tac_program_add_function(prog, new_func);
            break;
        }

        /* ---- Return ---- */
        case AST_RETURN: {
            if (node->data.return_stmt.value) {
                TACOperand val = ir_gen_expression(ctx, node->data.return_stmt.value);
                tac_emit(func, TAC_RETURN, tac_operand_none(), val, tac_operand_none());
            } else {
                tac_emit(func, TAC_RETURN, tac_operand_none(),
                         tac_operand_none(), tac_operand_none());
            }
            break;
        }

        /* ---- Break / Continue ---- */
        case AST_BREAK:
            if (ctx->in_loop) {
                tac_emit_goto(func, ctx->loop_break_label);
            }
            break;

        case AST_CONTINUE:
            if (ctx->in_loop) {
                tac_emit_goto(func, ctx->loop_continue_label);
            }
            break;

        /* ---- Secure Zone ---- */
        case AST_SECURE_ZONE:
            tac_emit(func, TAC_SECURE_BEGIN,
                     tac_operand_none(), tac_operand_none(), tac_operand_none());
            tac_emit(func, TAC_SCOPE_BEGIN,
                     tac_operand_none(), tac_operand_none(), tac_operand_none());
            ir_gen_node(ctx, node->data.secure_zone.body);
            tac_emit(func, TAC_SCOPE_END,
                     tac_operand_none(), tac_operand_none(), tac_operand_none());
            tac_emit(func, TAC_SECURE_END,
                     tac_operand_none(), tac_operand_none(), tac_operand_none());
            break;

        /* ---- Expression Statement ---- */
        case AST_EXPR_STMT:
            ir_gen_expression(ctx, node->data.expr_stmt.expr);
            break;

        /* ---- Block ---- */
        case AST_BLOCK: {
            tac_emit(func, TAC_SCOPE_BEGIN,
                     tac_operand_none(), tac_operand_none(), tac_operand_none());
            ASTNodeList *stmts = node->data.block.statements;
            if (stmts) {
                for (size_t i = 0; i < stmts->count; i++) {
                    ir_gen_statement(ctx, stmts->nodes[i]);
                }
            }
            tac_emit(func, TAC_SCOPE_END,
                     tac_operand_none(), tac_operand_none(), tac_operand_none());
            break;
        }

        default:
            fprintf(stderr, "IR warning: unhandled statement node type %d (%s)\n",
                    node->type, ast_node_type_to_string(node->type));
            break;
    }
}

/* ============================================================================
 * ir_gen_node – dispatch to statement or block
 * ============================================================================
 */
static void ir_gen_node(IRGenContext *ctx, ASTNode *node) {
    if (!node) return;

    if (node->type == AST_PROGRAM) {
        ASTNodeList *stmts = node->data.program.statements;
        if (stmts) {
            for (size_t i = 0; i < stmts->count; i++) {
                ir_gen_statement(ctx, stmts->nodes[i]);
            }
        }
    } else {
        ir_gen_statement(ctx, node);
    }
}

/* ============================================================================
 * PUBLIC ENTRY POINT
 * ============================================================================
 */

TACProgram *ir_generate(ASTNode *ast) {
    if (!ast) return NULL;

    IRGenContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.program = tac_program_create();
    ctx.current_func = ctx.program->main_func;

    ir_gen_node(&ctx, ast);

    /* Update statistics */
    ctx.program->total_instructions = ir_count_total(ctx.program);

    return ctx.program;
}
