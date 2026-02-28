/*
 * NatureLang Compiler
 * Copyright (c) 2026
 *
 * IR Optimization Implementation
 *
 * Classic compiler optimization passes operating on Three-Address Code:
 *
 *  1. Constant Folding        – evaluate constant expressions at compile time
 *  2. Constant Propagation    – replace variables with known constants
 *  3. Algebraic Simplification – x+0→x, x*1→x, x*0→0, etc.
 *  4. Strength Reduction      – x*2→x+x, pow(x,2)→x*x
 *  5. Redundant Load Elim.    – merge duplicate constant loads
 *  6. Dead Code Elimination   – remove instructions whose results are unused
 *
 * All passes modify the TAC IR in-place and return a count of
 * transformations applied.
 */

#define _POSIX_C_SOURCE 200809L
#include "optimizer.h"
#include "ir.h"
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * HELPERS
 * ============================================================================
 */

/* Check if an operand is an integer constant */
static bool is_int_const(TACOperand *op) {
    return op->kind == OPERAND_INT;
}

/* Check if an operand is a float constant */
static bool is_float_const(TACOperand *op) {
    return op->kind == OPERAND_FLOAT;
}

/* Check if an operand is any numeric constant */
static bool is_numeric_const(TACOperand *op) {
    return op->kind == OPERAND_INT || op->kind == OPERAND_FLOAT;
}

/* Check if an operand is a bool constant */
static bool is_bool_const(TACOperand *op) {
    return op->kind == OPERAND_BOOL;
}

/* Get numeric value of an operand as double */
static double get_numeric_value(TACOperand *op) {
    if (op->kind == OPERAND_INT) return (double)op->val.int_val;
    if (op->kind == OPERAND_FLOAT) return op->val.float_val;
    return 0.0;
}

/* Check if an opcode is a binary arithmetic/comparison op */
static bool is_binary_op(TACOpcode op) {
    switch (op) {
        case TAC_ADD: case TAC_SUB: case TAC_MUL: case TAC_DIV:
        case TAC_MOD: case TAC_POW:
        case TAC_EQ: case TAC_NEQ: case TAC_LT: case TAC_GT:
        case TAC_LTE: case TAC_GTE:
        case TAC_AND: case TAC_OR:
            return true;
        default:
            return false;
    }
}

/* Check if an opcode is a unary op */
static bool is_unary_op(TACOpcode op) {
    return op == TAC_NEG || op == TAC_NOT;
}

/* Check if an opcode is a comparison that produces a boolean result */
static bool is_comparison_op(TACOpcode op) {
    switch (op) {
        case TAC_EQ: case TAC_NEQ: case TAC_LT: case TAC_GT:
        case TAC_LTE: case TAC_GTE:
            return true;
        default:
            return false;
    }
}

/* Set an operand to an integer constant */
static void set_int_const(TACOperand *op, long long val) {
    /* Free any existing allocated memory */
    if (op->kind == OPERAND_VAR || op->kind == OPERAND_FUNC) {
        free(op->val.name);
    } else if (op->kind == OPERAND_STRING) {
        free(op->val.str_val);
    }
    op->kind = OPERAND_INT;
    op->data_type = TYPE_NUMBER;
    op->val.int_val = val;
}

/* Set an operand to a float constant */
static void set_float_const(TACOperand *op, double val) {
    if (op->kind == OPERAND_VAR || op->kind == OPERAND_FUNC) {
        free(op->val.name);
    } else if (op->kind == OPERAND_STRING) {
        free(op->val.str_val);
    }
    op->kind = OPERAND_FLOAT;
    op->data_type = TYPE_DECIMAL;
    op->val.float_val = val;
}

/* Set an operand to a bool constant */
static void set_bool_const(TACOperand *op, int val) {
    if (op->kind == OPERAND_VAR || op->kind == OPERAND_FUNC) {
        free(op->val.name);
    } else if (op->kind == OPERAND_STRING) {
        free(op->val.str_val);
    }
    op->kind = OPERAND_BOOL;
    op->data_type = TYPE_FLAG;
    op->val.bool_val = val;
}

/* Check if two operands refer to the same temp */
static bool same_temp(TACOperand *a, TACOperand *b) {
    return a->kind == OPERAND_TEMP && b->kind == OPERAND_TEMP &&
           a->val.temp_id == b->val.temp_id;
}

/* Check if an operand is a temp */
static bool is_temp(TACOperand *op) {
    return op->kind == OPERAND_TEMP;
}

/* Deep copy an operand (strdup heap pointers) */
static TACOperand dup_operand(TACOperand src) {
    TACOperand dst = src;
    if (src.kind == OPERAND_VAR || src.kind == OPERAND_FUNC) {
        dst.val.name = src.val.name ? strdup(src.val.name) : NULL;
    } else if (src.kind == OPERAND_STRING) {
        dst.val.str_val = src.val.str_val ? strdup(src.val.str_val) : NULL;
    }
    return dst;
}

/* Free an operand's heap memory */
static void release_operand(TACOperand *op) {
    if (op->kind == OPERAND_VAR || op->kind == OPERAND_FUNC) {
        free(op->val.name); op->val.name = NULL;
    } else if (op->kind == OPERAND_STRING) {
        free(op->val.str_val); op->val.str_val = NULL;
    }
}

/* Convert instruction to a simple copy: result = arg1 */
static void convert_to_assign(TACInstr *instr, TACOperand src) {
    TACOperand new_src = dup_operand(src);
    release_operand(&instr->arg1);
    release_operand(&instr->arg2);
    instr->opcode = TAC_ASSIGN;
    instr->arg1 = new_src;
    instr->arg2.kind = OPERAND_NONE;
}

/* ============================================================================
 * PASS 1: CONSTANT FOLDING
 *
 * If both operands of a binary operation are constants, evaluate at
 * compile time and replace with the result.
 *
 *   t0 = 3 + 4   →   t0 = LOAD_INT 7
 *   t1 = 2.0 * 3.0 → t1 = LOAD_FLOAT 6.0
 * ============================================================================
 */
int opt_constant_folding(TACFunction *func, bool verbose) {
    int count = 0;
    if (!func) return 0;

    for (TACInstr *instr = func->first; instr; instr = instr->next) {
        if (instr->is_dead) continue;

        /* Binary ops with two numeric constants */
        if (is_binary_op(instr->opcode) &&
            is_numeric_const(&instr->arg1) && is_numeric_const(&instr->arg2)) {

            double a = get_numeric_value(&instr->arg1);
            double b = get_numeric_value(&instr->arg2);
            bool both_int = is_int_const(&instr->arg1) && is_int_const(&instr->arg2);
            long long ia = instr->arg1.val.int_val;
            long long ib = instr->arg2.val.int_val;

            bool folded = true;
            long long int_result = 0;
            double float_result = 0.0;
            int bool_result = 0;
            bool result_is_bool = false;

            switch (instr->opcode) {
                case TAC_ADD: if (both_int) int_result = ia + ib; else float_result = a + b; break;
                case TAC_SUB: if (both_int) int_result = ia - ib; else float_result = a - b; break;
                case TAC_MUL: if (both_int) int_result = ia * ib; else float_result = a * b; break;
                case TAC_DIV:
                    if (b == 0.0) { folded = false; break; }
                    if (both_int) int_result = ia / ib; else float_result = a / b;
                    break;
                case TAC_MOD:
                    if (ib == 0) { folded = false; break; }
                    if (both_int) int_result = ia % ib; else { folded = false; }
                    break;
                case TAC_POW:
                    if (both_int && ib >= 0) {
                        int_result = 1;
                        for (long long i = 0; i < ib; i++) int_result *= ia;
                    } else {
                        float_result = pow(a, b);
                        both_int = false;
                    }
                    break;
                case TAC_EQ:  bool_result = (a == b); result_is_bool = true; break;
                case TAC_NEQ: bool_result = (a != b); result_is_bool = true; break;
                case TAC_LT:  bool_result = (a < b);  result_is_bool = true; break;
                case TAC_GT:  bool_result = (a > b);  result_is_bool = true; break;
                case TAC_LTE: bool_result = (a <= b); result_is_bool = true; break;
                case TAC_GTE: bool_result = (a >= b); result_is_bool = true; break;
                default: folded = false; break;
            }

            if (folded) {
                if (result_is_bool) {
                    if (verbose)
                        printf("  [fold] %s → %s\n",
                               tac_opcode_to_string(instr->opcode),
                               bool_result ? "true" : "false");
                    instr->opcode = TAC_LOAD_BOOL;
                    set_bool_const(&instr->arg1, bool_result);
                    instr->arg2.kind = OPERAND_NONE;
                } else if (both_int) {
                    if (verbose)
                        printf("  [fold] %s %lld, %lld → %lld\n",
                               tac_opcode_to_string(instr->opcode), ia, ib, int_result);
                    instr->opcode = TAC_LOAD_INT;
                    set_int_const(&instr->arg1, int_result);
                    instr->arg2.kind = OPERAND_NONE;
                } else {
                    if (verbose)
                        printf("  [fold] %s %g, %g → %g\n",
                               tac_opcode_to_string(instr->opcode), a, b, float_result);
                    instr->opcode = TAC_LOAD_FLOAT;
                    set_float_const(&instr->arg1, float_result);
                    instr->arg2.kind = OPERAND_NONE;
                }
                count++;
            }
        }

        /* Unary ops with constant operand */
        if (is_unary_op(instr->opcode) && is_numeric_const(&instr->arg1)) {
            if (instr->opcode == TAC_NEG) {
                if (is_int_const(&instr->arg1)) {
                    long long val = -instr->arg1.val.int_val;
                    if (verbose) printf("  [fold] NEG %lld → %lld\n", instr->arg1.val.int_val, val);
                    instr->opcode = TAC_LOAD_INT;
                    set_int_const(&instr->arg1, val);
                    instr->arg2.kind = OPERAND_NONE;
                    count++;
                } else if (is_float_const(&instr->arg1)) {
                    double val = -instr->arg1.val.float_val;
                    if (verbose) printf("  [fold] NEG %g → %g\n", instr->arg1.val.float_val, val);
                    instr->opcode = TAC_LOAD_FLOAT;
                    set_float_const(&instr->arg1, val);
                    instr->arg2.kind = OPERAND_NONE;
                    count++;
                }
            }
        }

        /* NOT with bool constant */
        if (instr->opcode == TAC_NOT && is_bool_const(&instr->arg1)) {
            int val = !instr->arg1.val.bool_val;
            if (verbose) printf("  [fold] NOT %s → %s\n",
                               instr->arg1.val.bool_val ? "true" : "false",
                               val ? "true" : "false");
            instr->opcode = TAC_LOAD_BOOL;
            set_bool_const(&instr->arg1, val);
            instr->arg2.kind = OPERAND_NONE;
            count++;
        }

        /* AND/OR with bool constants */
        if ((instr->opcode == TAC_AND || instr->opcode == TAC_OR) &&
            is_bool_const(&instr->arg1) && is_bool_const(&instr->arg2)) {
            int val;
            if (instr->opcode == TAC_AND)
                val = instr->arg1.val.bool_val && instr->arg2.val.bool_val;
            else
                val = instr->arg1.val.bool_val || instr->arg2.val.bool_val;
            if (verbose) printf("  [fold] %s → %s\n",
                               tac_opcode_to_string(instr->opcode),
                               val ? "true" : "false");
            instr->opcode = TAC_LOAD_BOOL;
            set_bool_const(&instr->arg1, val);
            instr->arg2.kind = OPERAND_NONE;
            count++;
        }
    }

    return count;
}

/* ============================================================================
 * PASS 2: CONSTANT PROPAGATION
 *
 * If a temp is loaded with a constant, replace all uses of that temp
 * with the constant (within the same basic block, conservatively).
 *
 *   t0 = 5
 *   t1 = t0 + 3     →   t1 = 5 + 3   (then constant folding can do 5+3=8)
 * ============================================================================
 */

/* Simple constant table: maps temp_id → constant operand */
#define MAX_CONSTS 4096

typedef struct {
    int temp_id;
    TACOperand value;
    bool valid;
} ConstEntry;

static ConstEntry const_table[MAX_CONSTS];
static int const_table_count = 0;

static void const_table_clear(void) {
    const_table_count = 0;
}

static void const_table_set(int temp_id, TACOperand val) {
    /* Check if already exists */
    for (int i = 0; i < const_table_count; i++) {
        if (const_table[i].temp_id == temp_id) {
            const_table[i].value = val;
            const_table[i].valid = true;
            return;
        }
    }
    if (const_table_count < MAX_CONSTS) {
        const_table[const_table_count].temp_id = temp_id;
        const_table[const_table_count].value = val;
        const_table[const_table_count].valid = true;
        const_table_count++;
    }
}

static void const_table_invalidate(int temp_id) {
    for (int i = 0; i < const_table_count; i++) {
        if (const_table[i].temp_id == temp_id) {
            const_table[i].valid = false;
            return;
        }
    }
}

static TACOperand *const_table_get(int temp_id) {
    for (int i = 0; i < const_table_count; i++) {
        if (const_table[i].temp_id == temp_id && const_table[i].valid) {
            return &const_table[i].value;
        }
    }
    return NULL;
}

/* Try to replace an operand with its known constant value */
static bool try_propagate(TACOperand *op) {
    if (op->kind != OPERAND_TEMP) return false;
    TACOperand *known = const_table_get(op->val.temp_id);
    if (!known) return false;

    /* Replace with the constant */
    switch (known->kind) {
        case OPERAND_INT:
            set_int_const(op, known->val.int_val);
            return true;
        case OPERAND_FLOAT:
            set_float_const(op, known->val.float_val);
            return true;
        case OPERAND_BOOL:
            set_bool_const(op, known->val.bool_val);
            return true;
        default:
            return false;
    }
}

int opt_constant_propagation(TACFunction *func, bool verbose) {
    int count = 0;
    if (!func) return 0;

    const_table_clear();

    for (TACInstr *instr = func->first; instr; instr = instr->next) {
        if (instr->is_dead) continue;

        /* Invalidate constant on control flow boundaries */
        if (instr->opcode == TAC_LABEL ||
            instr->opcode == TAC_FUNC_BEGIN ||
            instr->opcode == TAC_CALL) {
            const_table_clear();
            continue;
        }

        /* Record constants from LOAD instructions */
        if (is_temp(&instr->result)) {
            switch (instr->opcode) {
                case TAC_LOAD_INT:
                    if (is_int_const(&instr->arg1)) {
                        TACOperand c;
                        c.kind = OPERAND_INT;
                        c.data_type = TYPE_NUMBER;
                        c.val.int_val = instr->arg1.val.int_val;
                        const_table_set(instr->result.val.temp_id, c);
                    }
                    continue;
                case TAC_LOAD_FLOAT:
                    if (is_float_const(&instr->arg1)) {
                        TACOperand c;
                        c.kind = OPERAND_FLOAT;
                        c.data_type = TYPE_DECIMAL;
                        c.val.float_val = instr->arg1.val.float_val;
                        const_table_set(instr->result.val.temp_id, c);
                    }
                    continue;
                case TAC_LOAD_BOOL:
                    if (is_bool_const(&instr->arg1)) {
                        TACOperand c;
                        c.kind = OPERAND_BOOL;
                        c.data_type = TYPE_FLAG;
                        c.val.bool_val = instr->arg1.val.bool_val;
                        const_table_set(instr->result.val.temp_id, c);
                    }
                    continue;
                default:
                    break;
            }
        }

        /* Propagate constants into arg1 and arg2 */
        if (try_propagate(&instr->arg1)) {
            if (verbose) printf("  [prop] replaced arg1 in %s\n",
                               tac_opcode_to_string(instr->opcode));
            count++;
        }
        if (try_propagate(&instr->arg2)) {
            if (verbose) printf("  [prop] replaced arg2 in %s\n",
                               tac_opcode_to_string(instr->opcode));
            count++;
        }

        /* If result is a temp being written to, invalidate its old constant */
        if (is_temp(&instr->result)) {
            const_table_invalidate(instr->result.val.temp_id);
        }
    }

    return count;
}

/* ============================================================================
 * PASS 3: ALGEBRAIC SIMPLIFICATION
 *
 *   x + 0 → x          x - 0 → x
 *   0 + x → x          x * 1 → x
 *   1 * x → x          x * 0 → 0
 *   0 * x → 0          x / 1 → x
 *   x ** 1 → x         x ** 0 → 1
 * ============================================================================
 */
int opt_algebraic_simplification(TACFunction *func, bool verbose) {
    int count = 0;
    if (!func) return 0;

    for (TACInstr *instr = func->first; instr; instr = instr->next) {
        if (instr->is_dead) continue;

        TACOperand *a1 = &instr->arg1;
        TACOperand *a2 = &instr->arg2;

        bool a1_zero = (is_int_const(a1) && a1->val.int_val == 0) ||
                       (is_float_const(a1) && a1->val.float_val == 0.0);
        bool a2_zero = (is_int_const(a2) && a2->val.int_val == 0) ||
                       (is_float_const(a2) && a2->val.float_val == 0.0);
        bool a1_one  = (is_int_const(a1) && a1->val.int_val == 1) ||
                       (is_float_const(a1) && a1->val.float_val == 1.0);
        bool a2_one  = (is_int_const(a2) && a2->val.int_val == 1) ||
                       (is_float_const(a2) && a2->val.float_val == 1.0);

        switch (instr->opcode) {
            case TAC_ADD:
                /* x + 0 → x */
                if (a2_zero) {
                    if (verbose) printf("  [alg] x + 0 → x\n");
                    convert_to_assign(instr, *a1);
                    count++;
                }
                /* 0 + x → x */
                else if (a1_zero) {
                    if (verbose) printf("  [alg] 0 + x → x\n");
                    convert_to_assign(instr, *a2);
                    count++;
                }
                break;

            case TAC_SUB:
                /* x - 0 → x */
                if (a2_zero) {
                    if (verbose) printf("  [alg] x - 0 → x\n");
                    convert_to_assign(instr, *a1);
                    count++;
                }
                /* x - x → 0 */
                else if (same_temp(a1, a2)) {
                    if (verbose) printf("  [alg] x - x → 0\n");
                    instr->opcode = TAC_LOAD_INT;
                    set_int_const(&instr->arg1, 0);
                    instr->arg2.kind = OPERAND_NONE;
                    count++;
                }
                break;

            case TAC_MUL:
                /* x * 0 or 0 * x → 0 */
                if (a1_zero || a2_zero) {
                    if (verbose) printf("  [alg] x * 0 → 0\n");
                    instr->opcode = TAC_LOAD_INT;
                    set_int_const(&instr->arg1, 0);
                    instr->arg2.kind = OPERAND_NONE;
                    count++;
                }
                /* x * 1 → x */
                else if (a2_one) {
                    if (verbose) printf("  [alg] x * 1 → x\n");
                    convert_to_assign(instr, *a1);
                    count++;
                }
                /* 1 * x → x */
                else if (a1_one) {
                    if (verbose) printf("  [alg] 1 * x → x\n");
                    convert_to_assign(instr, *a2);
                    count++;
                }
                break;

            case TAC_DIV:
                /* x / 1 → x */
                if (a2_one) {
                    if (verbose) printf("  [alg] x / 1 → x\n");
                    convert_to_assign(instr, *a1);
                    count++;
                }
                break;

            case TAC_POW:
                /* x ** 0 → 1 */
                if (a2_zero) {
                    if (verbose) printf("  [alg] x ** 0 → 1\n");
                    instr->opcode = TAC_LOAD_INT;
                    set_int_const(&instr->arg1, 1);
                    instr->arg2.kind = OPERAND_NONE;
                    count++;
                }
                /* x ** 1 → x */
                else if (a2_one) {
                    if (verbose) printf("  [alg] x ** 1 → x\n");
                    convert_to_assign(instr, *a1);
                    count++;
                }
                break;

            default:
                break;
        }
    }

    return count;
}

/* ============================================================================
 * PASS 4: STRENGTH REDUCTION
 *
 *   x * 2  →  x + x
 *   x * 4  →  x << 2   (pow of 2)
 *   pow(x,2) → x * x
 * ============================================================================
 */

/* Check if a value is a power of 2 and return the exponent, or -1 */
static int log2_if_power_of_2(long long val) {
    if (val <= 0) return -1;
    if ((val & (val - 1)) != 0) return -1;
    int exp = 0;
    while (val > 1) { val >>= 1; exp++; }
    return exp;
}

int opt_strength_reduction(TACFunction *func, bool verbose) {
    int count = 0;
    if (!func) return 0;

    for (TACInstr *instr = func->first; instr; instr = instr->next) {
        if (instr->is_dead) continue;

        /* x * 2 → x + x */
        if (instr->opcode == TAC_MUL) {
            if (is_int_const(&instr->arg2) && instr->arg2.val.int_val == 2) {
                if (verbose) printf("  [str] x * 2 → x + x\n");
                instr->opcode = TAC_ADD;
                release_operand(&instr->arg2);
                instr->arg2 = dup_operand(instr->arg1);
                count++;
            }
            else if (is_int_const(&instr->arg1) && instr->arg1.val.int_val == 2) {
                if (verbose) printf("  [str] 2 * x → x + x\n");
                instr->opcode = TAC_ADD;
                release_operand(&instr->arg1);
                instr->arg1 = dup_operand(instr->arg2);
                count++;
            }
        }

        /* pow(x, 2) → x * x */
        if (instr->opcode == TAC_POW &&
            is_int_const(&instr->arg2) && instr->arg2.val.int_val == 2) {
            if (verbose) printf("  [str] x ** 2 → x * x\n");
            instr->opcode = TAC_MUL;
            release_operand(&instr->arg2);
            instr->arg2 = dup_operand(instr->arg1);
            count++;
        }

        /* pow(x, 3) → x * x * x  – we leave as-is for simplicity */
    }

    return count;
}

/* ============================================================================
 * PASS 5: REDUNDANT LOAD ELIMINATION
 *
 * If the same constant is loaded into multiple temps within a basic block,
 * reuse the first temp.
 *
 *   t0 = LOAD_INT 5
 *   t1 = LOAD_INT 5    →   t1 = t0
 * ============================================================================
 */
int opt_redundant_load_elimination(TACFunction *func, bool verbose) {
    int count = 0;
    if (!func) return 0;

    /* Track recent loads: opcode + value → temp_id */
    #define MAX_RECENT 256
    struct {
        TACOpcode opcode;
        long long int_val;
        double float_val;
        int bool_val;
        int temp_id;
        DataType data_type;
        bool valid;
    } recent[MAX_RECENT];
    int recent_count = 0;

    for (TACInstr *instr = func->first; instr; instr = instr->next) {
        if (instr->is_dead) continue;

        /* Reset on control flow */
        if (instr->opcode == TAC_LABEL || instr->opcode == TAC_FUNC_BEGIN ||
            instr->opcode == TAC_CALL || instr->opcode == TAC_GOTO ||
            instr->opcode == TAC_IF_GOTO || instr->opcode == TAC_IF_FALSE_GOTO) {
            recent_count = 0;
            continue;
        }

        /* Check LOADs of the same constant */
        if (is_temp(&instr->result) &&
            (instr->opcode == TAC_LOAD_INT ||
             instr->opcode == TAC_LOAD_FLOAT ||
             instr->opcode == TAC_LOAD_BOOL)) {

            /* Search for matching recent load */
            for (int i = 0; i < recent_count; i++) {
                if (!recent[i].valid) continue;
                if (recent[i].opcode != instr->opcode) continue;

                bool match = false;
                if (instr->opcode == TAC_LOAD_INT &&
                    is_int_const(&instr->arg1) &&
                    recent[i].int_val == instr->arg1.val.int_val) {
                    match = true;
                }
                if (instr->opcode == TAC_LOAD_FLOAT &&
                    is_float_const(&instr->arg1) &&
                    recent[i].float_val == instr->arg1.val.float_val) {
                    match = true;
                }
                if (instr->opcode == TAC_LOAD_BOOL &&
                    is_bool_const(&instr->arg1) &&
                    recent[i].bool_val == instr->arg1.val.bool_val) {
                    match = true;
                }

                if (match) {
                    if (verbose) printf("  [rle] t%d = same as t%d\n",
                                       instr->result.val.temp_id,
                                       recent[i].temp_id);
                    /* Convert to assignment from the earlier temp */
                    instr->opcode = TAC_ASSIGN;
                    instr->arg1.kind = OPERAND_TEMP;
                    instr->arg1.data_type = recent[i].data_type;
                    instr->arg1.val.temp_id = recent[i].temp_id;
                    instr->arg2.kind = OPERAND_NONE;
                    count++;
                    goto next_instr;
                }
            }

            /* Record this load */
            if (recent_count < MAX_RECENT) {
                recent[recent_count].opcode = instr->opcode;
                recent[recent_count].temp_id = instr->result.val.temp_id;
                recent[recent_count].data_type = instr->result.data_type;
                recent[recent_count].valid = true;
                if (instr->opcode == TAC_LOAD_INT)
                    recent[recent_count].int_val = instr->arg1.val.int_val;
                else if (instr->opcode == TAC_LOAD_FLOAT)
                    recent[recent_count].float_val = instr->arg1.val.float_val;
                else if (instr->opcode == TAC_LOAD_BOOL)
                    recent[recent_count].bool_val = instr->arg1.val.bool_val;
                recent_count++;
            }
        }
        next_instr:;
    }
    #undef MAX_RECENT

    return count;
}

/* ============================================================================
 * PASS 6: DEAD CODE ELIMINATION
 *
 * Mark instructions whose results (temporaries) are never read by any
 * subsequent instruction as dead.
 *
 * We do a backward scan: a temp is "live" if it appears as arg1/arg2/arg3
 * of a later instruction. If a temp result is never live, the instruction
 * producing it is dead.
 *
 * We only eliminate instructions that write to temps (not named variables,
 * not side-effecting instructions like DISPLAY, CALL, etc.)
 * ============================================================================
 */

static bool has_side_effect(TACOpcode op) {
    switch (op) {
        case TAC_DISPLAY: case TAC_READ: case TAC_ASK:
        case TAC_CALL: case TAC_PARAM:
        case TAC_RETURN: case TAC_GOTO:
        case TAC_IF_GOTO: case TAC_IF_FALSE_GOTO:
        case TAC_LABEL:
        case TAC_FUNC_BEGIN: case TAC_FUNC_END:
        case TAC_SCOPE_BEGIN: case TAC_SCOPE_END:
        case TAC_SECURE_BEGIN: case TAC_SECURE_END:
        case TAC_DECL:
        case TAC_BREAK: case TAC_CONTINUE:
        case TAC_LIST_APPEND: case TAC_LIST_SET:
            return true;
        default:
            return false;
    }
}

/* Check if a temp_id is used in any operand of any subsequent instruction */
static bool temp_is_used_after(TACInstr *start, int temp_id) {
    /* Scan the entire function for uses of this temp.
     * We must check backward too because loops create back-edges:
     * a temp defined late in the loop may be used at the loop header. */

    /* Forward scan from start->next */
    for (TACInstr *instr = start->next; instr; instr = instr->next) {
        if (instr->is_dead) continue;
        if (instr->arg1.kind == OPERAND_TEMP && instr->arg1.val.temp_id == temp_id)
            return true;
        if (instr->arg2.kind == OPERAND_TEMP && instr->arg2.val.temp_id == temp_id)
            return true;
        if (instr->arg3.kind == OPERAND_TEMP && instr->arg3.val.temp_id == temp_id)
            return true;
    }
    /* Backward scan from start->prev (for loop back-edges) */
    for (TACInstr *instr = start->prev; instr; instr = instr->prev) {
        if (instr->is_dead) continue;
        if (instr->arg1.kind == OPERAND_TEMP && instr->arg1.val.temp_id == temp_id)
            return true;
        if (instr->arg2.kind == OPERAND_TEMP && instr->arg2.val.temp_id == temp_id)
            return true;
        if (instr->arg3.kind == OPERAND_TEMP && instr->arg3.val.temp_id == temp_id)
            return true;
    }
    return false;
}

int opt_dead_code_elimination(TACFunction *func, bool verbose) {
    int count = 0;
    if (!func) return 0;

    for (TACInstr *instr = func->first; instr; instr = instr->next) {
        if (instr->is_dead) continue;
        if (has_side_effect(instr->opcode)) continue;

        /* Only eliminate instructions that write to temps */
        if (!is_temp(&instr->result)) continue;

        /* ASSIGN to a named var (result is var) – don't eliminate */
        if (instr->opcode == TAC_ASSIGN && instr->result.kind == OPERAND_VAR)
            continue;

        /* Check if this temp is used later */
        if (!temp_is_used_after(instr, instr->result.val.temp_id)) {
            if (verbose) printf("  [dce] dead: t%d from %s\n",
                               instr->result.val.temp_id,
                               tac_opcode_to_string(instr->opcode));
            instr->is_dead = true;
            count++;
        }
    }

    return count;
}

/* ============================================================================
 * SWEEP: Remove dead instructions from the linked list
 * ============================================================================
 */
int opt_sweep_dead(TACFunction *func) {
    int count = 0;
    if (!func) return 0;

    TACInstr *instr = func->first;
    while (instr) {
        TACInstr *next = instr->next;
        if (instr->is_dead) {
            /* Unlink from doubly-linked list */
            if (instr->prev) instr->prev->next = instr->next;
            else func->first = instr->next;

            if (instr->next) instr->next->prev = instr->prev;
            else func->last = instr->prev;

            func->instr_count--;

            /* Free operands and instruction */
            if (instr->result.kind == OPERAND_VAR || instr->result.kind == OPERAND_FUNC)
                free(instr->result.val.name);
            else if (instr->result.kind == OPERAND_STRING)
                free(instr->result.val.str_val);
            if (instr->arg1.kind == OPERAND_VAR || instr->arg1.kind == OPERAND_FUNC)
                free(instr->arg1.val.name);
            else if (instr->arg1.kind == OPERAND_STRING)
                free(instr->arg1.val.str_val);
            if (instr->arg2.kind == OPERAND_VAR || instr->arg2.kind == OPERAND_FUNC)
                free(instr->arg2.val.name);
            else if (instr->arg2.kind == OPERAND_STRING)
                free(instr->arg2.val.str_val);
            if (instr->arg3.kind == OPERAND_VAR || instr->arg3.kind == OPERAND_FUNC)
                free(instr->arg3.val.name);
            else if (instr->arg3.kind == OPERAND_STRING)
                free(instr->arg3.val.str_val);
            free(instr);

            count++;
        }
        instr = next;
    }

    return count;
}

/* ============================================================================
 * OPTIONS
 * ============================================================================
 */

OptOptions opt_default_options(OptLevel level) {
    OptOptions opts;
    memset(&opts, 0, sizeof(opts));
    opts.level = level;
    opts.verbose = false;

    switch (level) {
        case OPT_LEVEL_0:
            /* No optimizations */
            break;
        case OPT_LEVEL_1:
            opts.constant_folding = true;
            opts.dead_code_elimination = true;
            break;
        case OPT_LEVEL_2:
            opts.constant_folding = true;
            opts.constant_propagation = true;
            opts.dead_code_elimination = true;
            opts.algebraic_simplification = true;
            opts.strength_reduction = true;
            opts.redundant_load_elimination = true;
            break;
    }

    return opts;
}

/* ============================================================================
 * MAIN OPTIMIZATION DRIVER
 *
 * Runs passes in the correct order, iterating until a fixpoint
 * (no more transformations) or a maximum iteration count.
 * ============================================================================
 */

static void optimize_function(TACFunction *func, OptOptions *opts, OptStats *stats) {
    if (!func) return;

    int max_iterations = 10;  /* Avoid infinite loops */

    for (int iter = 0; iter < max_iterations; iter++) {
        int changes = 0;

        /* 1. Constant Propagation (before folding, so folding has constants) */
        if (opts->constant_propagation) {
            int n = opt_constant_propagation(func, opts->verbose);
            changes += n;
            stats->constants_propagated += n;
        }

        /* 2. Constant Folding */
        if (opts->constant_folding) {
            int n = opt_constant_folding(func, opts->verbose);
            changes += n;
            stats->constants_folded += n;
        }

        /* 3. Algebraic Simplification */
        if (opts->algebraic_simplification) {
            int n = opt_algebraic_simplification(func, opts->verbose);
            changes += n;
            stats->algebraic_simplifications += n;
        }

        /* 4. Strength Reduction */
        if (opts->strength_reduction) {
            int n = opt_strength_reduction(func, opts->verbose);
            changes += n;
            stats->strength_reductions += n;
        }

        /* 5. Redundant Load Elimination */
        if (opts->redundant_load_elimination) {
            int n = opt_redundant_load_elimination(func, opts->verbose);
            changes += n;
            stats->redundant_loads_removed += n;
        }

        /* 6. Dead Code Elimination */
        if (opts->dead_code_elimination) {
            int n = opt_dead_code_elimination(func, opts->verbose);
            changes += n;
            stats->dead_instructions_removed += n;
        }

        stats->passes_run++;

        if (changes == 0) break;  /* Fixpoint reached */
    }

    /* Sweep dead instructions */
    opt_sweep_dead(func);
}

OptStats ir_optimize(TACProgram *program, OptOptions *options) {
    OptStats stats;
    memset(&stats, 0, sizeof(stats));

    if (!program || !options) return stats;
    if (options->level == OPT_LEVEL_0) return stats;

    stats.total_instructions_before = ir_count_total(program);

    if (options->verbose) {
        printf("\n=== Optimization Pass (Level %d) ===\n", options->level);
    }

    /* Optimize main function */
    if (options->verbose && program->main_func) {
        printf("\nOptimizing <main>:\n");
    }
    optimize_function(program->main_func, options, &stats);

    /* Optimize user functions */
    TACFunction *f = program->functions;
    while (f) {
        if (options->verbose) {
            printf("\nOptimizing %s:\n", f->name ? f->name : "<?>");
        }
        optimize_function(f, options, &stats);
        f = f->next;
    }

    stats.total_instructions_after = ir_count_total(program);

    if (options->verbose) {
        printf("\n");
        opt_print_stats(&stats);
    }

    return stats;
}

/* ============================================================================
 * STATISTICS PRINTING
 * ============================================================================
 */
void opt_print_stats(OptStats *stats) {
    printf("=== Optimization Statistics ===\n");
    printf("  Passes run:              %d\n", stats->passes_run);
    printf("  Constants folded:        %d\n", stats->constants_folded);
    printf("  Constants propagated:    %d\n", stats->constants_propagated);
    printf("  Algebraic simplif.:      %d\n", stats->algebraic_simplifications);
    printf("  Strength reductions:     %d\n", stats->strength_reductions);
    printf("  Redundant loads removed: %d\n", stats->redundant_loads_removed);
    printf("  Dead code eliminated:    %d\n", stats->dead_instructions_removed);
    printf("  Instructions before:     %d\n", stats->total_instructions_before);
    printf("  Instructions after:      %d\n", stats->total_instructions_after);
    int saved = stats->total_instructions_before - stats->total_instructions_after;
    if (stats->total_instructions_before > 0) {
        printf("  Reduction:               %d (%.1f%%)\n",
               saved,
               100.0 * saved / stats->total_instructions_before);
    }
    printf("===============================\n");
}
