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
    /* কতগুলো transformation (fold) হয়েছে তার কাউন্টার। */
    int count = 0;
    /* safety: function pointer না থাকলে কিছুই optimize করা যাবে না। */
    if (!func) return 0;

    /* function-এর সব instruction একে একে স্ক্যান করি। */
    for (TACInstr *instr = func->first; instr; instr = instr->next) {
        /* dead instruction আগে থেকেই mark থাকলে skip করি। */
        if (instr->is_dead) continue;

        /* Binary op + দুই পাশেই numeric constant হলে compile-time evaluate করা যাবে। */
        if (is_binary_op(instr->opcode) &&
            is_numeric_const(&instr->arg1) && is_numeric_const(&instr->arg2)) {

            /* arg1/arg2-কে generic double রূপে নেই, float/integer common handling-এর জন্য। */
            double a = get_numeric_value(&instr->arg1);
            double b = get_numeric_value(&instr->arg2);
            /* দুই operand-ই integer কিনা ধরে রাখি। */
            bool both_int = is_int_const(&instr->arg1) && is_int_const(&instr->arg2);
            /* fast integer path-এর জন্য raw int মান। */
            long long ia = instr->arg1.val.int_val;
            long long ib = instr->arg2.val.int_val;

            /* folded=false হলে এই instruction fold করা হবে না। */
            bool folded = true;
            /* int result রাখার slot। */
            long long int_result = 0;
            /* float result রাখার slot। */
            double float_result = 0.0;
            /* bool comparison result রাখার slot। */
            int bool_result = 0;
            /* result boolean কিনা আলাদা flag। */
            bool result_is_bool = false;

            /* opcode অনুযায়ী compile-time হিসাব করি। */
            switch (instr->opcode) {
                /* ADD: int+int হলে int, নাহলে float। */
                case TAC_ADD: if (both_int) int_result = ia + ib; else float_result = a + b; break;
                /* SUB: int-int হলে int, নাহলে float। */
                case TAC_SUB: if (both_int) int_result = ia - ib; else float_result = a - b; break;
                /* MUL: int*int হলে int, নাহলে float। */
                case TAC_MUL: if (both_int) int_result = ia * ib; else float_result = a * b; break;
                case TAC_DIV:
                    /* division by zero fold করব না (undefined/runtime-sensitive)। */
                    if (b == 0.0) { folded = false; break; }
                    /* integer division হলে truncation semantics বজায় থাকে। */
                    if (both_int) int_result = ia / ib; else float_result = a / b;
                    break;
                case TAC_MOD:
                    /* integer modulo-তে divisor 0 হলে fold নিষিদ্ধ। */
                    if (ib == 0) { folded = false; break; }
                    /* modulo কেবল int path-এ সমর্থিত; float হলে fold করব না। */
                    if (both_int) int_result = ia % ib; else { folded = false; }
                    break;
                case TAC_POW:
                    /* integer power non-negative exponent হলে exact int fold করি। */
                    if (both_int && ib >= 0) {
                        int_result = 1;
                        /* repeated multiply দিয়ে ia^ib। */
                        for (long long i = 0; i < ib; i++) int_result *= ia;
                    } else {
                        /* সাধারণ case: double pow ব্যবহার। */
                        float_result = pow(a, b);
                        /* float path-এ নামলে result আর int নয়। */
                        both_int = false;
                    }
                    break;
                /* comparison op সবসময় bool result দেয়। */
                case TAC_EQ:  bool_result = (a == b); result_is_bool = true; break;
                case TAC_NEQ: bool_result = (a != b); result_is_bool = true; break;
                case TAC_LT:  bool_result = (a < b);  result_is_bool = true; break;
                case TAC_GT:  bool_result = (a > b);  result_is_bool = true; break;
                case TAC_LTE: bool_result = (a <= b); result_is_bool = true; break;
                case TAC_GTE: bool_result = (a >= b); result_is_bool = true; break;
                /* unhandled opcode হলে folding disable করি। */
                default: folded = false; break;
            }

            /* fold সম্ভব হলে original opcode-কে LOAD_* এ রিরাইট করি। */
            if (folded) {
                /* boolean result path: LOAD_BOOL এ নামাই। */
                if (result_is_bool) {
                    /* verbose mode-এ fold trace print। */
                    /*verbose true হলে pass চলাকালে detailed trace দেখা যায়।
verbose false হলে একই optimization হবে, শুধু print হবে না।*/
                    if (verbose)
                        printf("  [fold] %s → %s\n",
                               tac_opcode_to_string(instr->opcode),
                               bool_result ? "true" : "false");
                    /* opcode replace: runtime compute বাদ, constant load। */
                    instr->opcode = TAC_LOAD_BOOL;
                    /* arg1-এ folded bool constant বসাই। */
                    set_bool_const(&instr->arg1, bool_result);
                    /* binary second operand আর দরকার নেই। */
                    instr->arg2.kind = OPERAND_NONE;
                /* pure integer result path। */
                } else if (both_int) {
                    /* verbose trace: int folding details। */
                    if (verbose)
                        printf("  [fold] %s %lld, %lld → %lld\n",
                               tac_opcode_to_string(instr->opcode), ia, ib, int_result);
                    /* integer constant load-এ convert। */
                    instr->opcode = TAC_LOAD_INT;
                    /* arg1-এ folded integer বসাই। */
                    set_int_const(&instr->arg1, int_result);
                    /* arg2 remove করি। */
                    instr->arg2.kind = OPERAND_NONE;
                /* floating result path। */
                } else {
                    /* verbose trace: float folding details। */
                    if (verbose)
                        printf("  [fold] %s %g, %g → %g\n",
                               tac_opcode_to_string(instr->opcode), a, b, float_result);
                    /* float constant load-এ convert। */
                    instr->opcode = TAC_LOAD_FLOAT;
                    /* arg1-এ folded float বসাই। */
                    set_float_const(&instr->arg1, float_result);
                    /* arg2 invalidate করি। */
                    instr->arg2.kind = OPERAND_NONE;
                }
                /* সফল fold কাউন্ট বাড়াই। */
                count++;
            }
        }

        /* Unary opcode-এ numeric constant থাকলে compile-time simplify করি। */
        if (is_unary_op(instr->opcode) && is_numeric_const(&instr->arg1)) {
            /* বর্তমানে unary folding-এর মধ্যে NEG handle করা হয়েছে। */
            if (instr->opcode == TAC_NEG) {
                /* int NEG path। */
                if (is_int_const(&instr->arg1)) {
                    /* compile-time unary minus। */
                    long long val = -instr->arg1.val.int_val;
                    /* verbose trace। */
                    if (verbose) printf("  [fold] NEG %lld → %lld\n", instr->arg1.val.int_val, val);
                    /* LOAD_INT এ rewrite। */
                    instr->opcode = TAC_LOAD_INT;
                    /* arg1-এ folded integer বসাই। */
                    set_int_const(&instr->arg1, val);
                    /* unary হওয়ায় arg2 empty। */
                    instr->arg2.kind = OPERAND_NONE;
                    /* transform count update। */
                    count++;
                /* float NEG path। */
                } else if (is_float_const(&instr->arg1)) {
                    /* compile-time unary minus on float। */
                    double val = -instr->arg1.val.float_val;
                    /* verbose trace। */
                    if (verbose) printf("  [fold] NEG %g → %g\n", instr->arg1.val.float_val, val);
                    /* LOAD_FLOAT এ rewrite। */
                    instr->opcode = TAC_LOAD_FLOAT;
                    /* arg1-এ folded float বসাই। */
                    set_float_const(&instr->arg1, val);
                    /* arg2 clear। */
                    instr->arg2.kind = OPERAND_NONE;
                    /* transform count update। */
                    count++;
                }
            }
        }

        /* NOT true/false compile-time fold। */
        if (instr->opcode == TAC_NOT && is_bool_const(&instr->arg1)) {
            /* logical not evaluate করি। */
            int val = !instr->arg1.val.bool_val;
            /* verbose trace print। */
            if (verbose) printf("  [fold] NOT %s → %s\n",
                               instr->arg1.val.bool_val ? "true" : "false",
                               val ? "true" : "false");
            /* opcode rewrite to bool load। */
            instr->opcode = TAC_LOAD_BOOL;
            /* arg1-এ computed bool বসাই। */
            set_bool_const(&instr->arg1, val);
            /* arg2 unused। */
            instr->arg2.kind = OPERAND_NONE;
            /* fold count increment। */
            count++;
        }

        /* AND/OR-এ দুই operand-ই bool constant হলে fold। */
        if ((instr->opcode == TAC_AND || instr->opcode == TAC_OR) &&
            is_bool_const(&instr->arg1) && is_bool_const(&instr->arg2)) {
            /* folded boolean result রাখার local variable। */
            int val;
            /* opcode অনুযায়ী logical combine। */
            if (instr->opcode == TAC_AND)
                val = instr->arg1.val.bool_val && instr->arg2.val.bool_val;
            else
                val = instr->arg1.val.bool_val || instr->arg2.val.bool_val;
            /* verbose trace print। */
            if (verbose) printf("  [fold] %s → %s\n",
                               tac_opcode_to_string(instr->opcode),
                               val ? "true" : "false");
            /* constant bool load-এ rewrite। */
            instr->opcode = TAC_LOAD_BOOL;
            /* arg1-এ folded bool set। */
            set_bool_const(&instr->arg1, val);
            /* arg2 আর দরকার নেই। */
            instr->arg2.kind = OPERAND_NONE;
            /* fold count update। */
            count++;
        }
    }

    /* মোট fold transformation count caller-এ ফেরত দিই। */
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
/*
এই সেকশনের উদ্দেশ্য:
1) TEMP register (t0, t1...) এর জন্য known constant value ট্র্যাক করা।
2) পরে অন্য instruction-এ ওই temp ব্যবহার হলে constant দিয়ে replace করা।
3) control-flow boundary এ table reset করে conservative/safe থাকা।
*/
#define MAX_CONSTS 4096

/* constant table-এর প্রতিটি entry একটি temp-এর বর্তমান known constant ধরে। */
typedef struct {
    /* কোন temp id-এর জন্য entry (যেমন t7 হলে temp_id=7)। */
    int temp_id;
    /* temp-এর known constant operand value (INT/FLOAT/BOOL)। */
    TACOperand value;
    /* entry বর্তমানে বৈধ কিনা (overwrite হলে invalidate হয়)। */
    bool valid;
} ConstEntry;

/* স্থির আকারের table: compilation pass চলাকালীন temp→constant map। */
static ConstEntry const_table[MAX_CONSTS];
/* table-এ এখন পর্যন্ত কয়টি slot fill হয়েছে। */
static int const_table_count = 0;

static void const_table_clear(void) {
    /* logical clear: count 0 করলেই আগের entry গুলো অকার্যকর ধরা হয়। */
    const_table_count = 0;
}

static void const_table_set(int temp_id, TACOperand val) {
    /* আগে দেখি temp_id-এর entry আগে থেকেই আছে কিনা। */
    for (int i = 0; i < const_table_count; i++) {
        if (const_table[i].temp_id == temp_id) {
            /* থাকলে value update করে entry-কে valid রাখি। */
            const_table[i].value = val;
            const_table[i].valid = true;
            return;
        }
    }
    /* নতুন entry হলে capacity আছে কিনা দেখি। */
    if (const_table_count < MAX_CONSTS) {
        /* table-এর পরের slot-এ temp/value লিখি। */
        const_table[const_table_count].temp_id = temp_id;
        const_table[const_table_count].value = val;
        const_table[const_table_count].valid = true;
        /* entry count বাড়াই। */
        const_table_count++;
    }
}

static void const_table_invalidate(int temp_id) {
    /* temp overwrite/unknown হলে তার known constant invalidate করি। */
    for (int i = 0; i < const_table_count; i++) {
        if (const_table[i].temp_id == temp_id) {
            const_table[i].valid = false;
            return;
        }
    }
}

static TACOperand *const_table_get(int temp_id) {
    /* temp_id-এর valid entry খুঁজে পেলে তার value pointer ফেরত দিই। */
    for (int i = 0; i < const_table_count; i++) {
        if (const_table[i].temp_id == temp_id && const_table[i].valid) {
            return &const_table[i].value;
        }
    }
    /* না পেলে NULL: অর্থাৎ known constant নেই। */
    return NULL;
}

/* Try to replace an operand with its known constant value */
static bool try_propagate(TACOperand *op) {
    /* temp না হলে propagation প্রযোজ্য নয়। */
    if (op->kind != OPERAND_TEMP) return false;
    /* এই temp-এর known constant table-এ আছে কিনা দেখি। */
    TACOperand *known = const_table_get(op->val.temp_id);
    /* জানা না থাকলে replace করা যাবে না। */
    if (!known) return false;

    /* known constant kind অনুযায়ী operand-কে concrete constant-এ convert করি। */
    switch (known->kind) {
        case OPERAND_INT:
            /* temp → int constant */
            set_int_const(op, known->val.int_val);
            return true;
        case OPERAND_FLOAT:
            /* temp → float constant */
            set_float_const(op, known->val.float_val);
            return true;
        case OPERAND_BOOL:
            /* temp → bool constant */
            set_bool_const(op, known->val.bool_val);
            return true;
        default:
            /* অন্য kind (যেমন string/var) এখানে propagate করা হচ্ছে না। */
            return false;
    }
}

int opt_constant_propagation(TACFunction *func, bool verbose) {
    /* মোট propagation replacement count। */
    int count = 0;
    /* safety guard। */
    if (!func) return 0;

    /* নতুন function optimize শুরুতে table fresh করি। */
    const_table_clear();

    /* instruction list forward scan। */
    for (TACInstr *instr = func->first; instr; instr = instr->next) {
        /* dead instruction ignore। */
        if (instr->is_dead) continue;

        /*
        Control-flow boundary এ previous assumptions unsafe হতে পারে,
        তাই conservativeভাবে table clear করি।
        */
        if (instr->opcode == TAC_LABEL ||
            instr->opcode == TAC_FUNC_BEGIN ||
            instr->opcode == TAC_CALL) {
            const_table_clear();
            continue;
        }

        /* result যদি temp হয়, LOAD_* থেকে নতুন known constant record করি। */
        if (is_temp(&instr->result)) {
            switch (instr->opcode) {
                case TAC_LOAD_INT:
                    /* LOAD_INT arg1 সত্যিই int constant কিনা নিশ্চিত করি। */
                    if (is_int_const(&instr->arg1)) {
                        /* temp table-এ বসানোর জন্য compact constant operand বানাই। */
                        TACOperand c;
                        c.kind = OPERAND_INT;
                        c.data_type = TYPE_NUMBER;
                        c.val.int_val = instr->arg1.val.int_val;
                        /* result temp_id এর জন্য known value save। */
                        const_table_set(instr->result.val.temp_id, c);
                    }
                    /* LOAD record করার পর পরের instruction-এ যাই। */
                    continue;
                case TAC_LOAD_FLOAT:
                    /* LOAD_FLOAT constant record। */
                    if (is_float_const(&instr->arg1)) {
                        TACOperand c;
                        c.kind = OPERAND_FLOAT;
                        c.data_type = TYPE_DECIMAL;
                        c.val.float_val = instr->arg1.val.float_val;
                        const_table_set(instr->result.val.temp_id, c);
                    }
                    continue;
                case TAC_LOAD_BOOL:
                    /* LOAD_BOOL constant record। */
                    if (is_bool_const(&instr->arg1)) {
                        TACOperand c;
                        c.kind = OPERAND_BOOL;
                        c.data_type = TYPE_FLAG;
                        c.val.bool_val = instr->arg1.val.bool_val;
                        const_table_set(instr->result.val.temp_id, c);
                    }
                    continue;
                default:
                    /* অন্য opcode হলে এখানে record phase শেষ। */
                    break;
            }
        }

        /* current instruction-এর arg1-এ propagation ট্রাই করি। */
        if (try_propagate(&instr->arg1)) {
            /* verbose trace: কোন opcode-এ arg1 replace হলো। */
            if (verbose) printf("  [prop] replaced arg1 in %s\n",
                               tac_opcode_to_string(instr->opcode));
            /* replacement count বাড়াই। */
            count++;
        }
        /* arg2-এও propagation ট্রাই করি। */
        if (try_propagate(&instr->arg2)) {
            if (verbose) printf("  [prop] replaced arg2 in %s\n",
                               tac_opcode_to_string(instr->opcode));
            count++;
        }

        /*
        এই instruction যদি কোনো temp result লিখে,
        তাহলে ঐ temp-এর আগের known value আর বৈধ নয় (overwrite)।
        */
        if (is_temp(&instr->result)) {
            const_table_invalidate(instr->result.val.temp_id);
        }
    }

    /* মোট propagation transformation count return। */
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
    /* মোট কতগুলো algebraic rewrite হলো তার হিসাব। */
    int count = 0;
    /* safety guard: function pointer null হলে pass চালানো যাবে না। */
    if (!func) return 0;

    /* function-এর instruction list forward স্ক্যান করি। */
    for (TACInstr *instr = func->first; instr; instr = instr->next) {
        /* dead mark করা instruction এই pass-এ বিবেচনা করব না। */
        if (instr->is_dead) continue;

        /* readability-এর জন্য arg1/arg2 pointer alias নেই। */
        TACOperand *a1 = &instr->arg1;
        TACOperand *a2 = &instr->arg2;

        /* arg1 গাণিতিক শূন্য কিনা (int 0 বা float 0.0)। */
        bool a1_zero = (is_int_const(a1) && a1->val.int_val == 0) ||
                       (is_float_const(a1) && a1->val.float_val == 0.0);
        /* arg2 গাণিতিক শূন্য কিনা। */
        bool a2_zero = (is_int_const(a2) && a2->val.int_val == 0) ||
                       (is_float_const(a2) && a2->val.float_val == 0.0);
        /* arg1 গাণিতিক এক কিনা (int 1 বা float 1.0)। */
        bool a1_one  = (is_int_const(a1) && a1->val.int_val == 1) ||
                       (is_float_const(a1) && a1->val.float_val == 1.0);
        /* arg2 গাণিতিক এক কিনা। */
        bool a2_one  = (is_int_const(a2) && a2->val.int_val == 1) ||
                       (is_float_const(a2) && a2->val.float_val == 1.0);

        /* opcode অনুযায়ী algebraic identity rule প্রয়োগ করি। */
        switch (instr->opcode) {
            case TAC_ADD:
                /* x + 0 → x */
                if (a2_zero) {
                    /* verbose mode-এ applied rule log করি। */
                    if (verbose) printf("  [alg] x + 0 → x\n");
                    /* instruction-কে copy/assign এ নামাই: result = arg1 */
                    convert_to_assign(instr, *a1);
                    /* transform count update। */
                    count++;
                }
                /* 0 + x → x */
                else if (a1_zero) {
                    if (verbose) printf("  [alg] 0 + x → x\n");
                    /* commutative identity: result = arg2 */
                    convert_to_assign(instr, *a2);
                    count++;
                }
                /* ADD case handling শেষ। */
                break;

            case TAC_SUB:
                /* x - 0 → x */
                if (a2_zero) {
                    if (verbose) printf("  [alg] x - 0 → x\n");
                    /* subtraction identity: result = arg1 */
                    convert_to_assign(instr, *a1);
                    count++;
                }
                /* x - x → 0 */
                else if (same_temp(a1, a2)) {
                    if (verbose) printf("  [alg] x - x → 0\n");
                    /* expression-কে constant load-এ convert করি। */
                    instr->opcode = TAC_LOAD_INT;
                    /* arg1-এ 0 constant বসাই। */
                    set_int_const(&instr->arg1, 0);
                    /* binary second operand invalidate করি। */
                    instr->arg2.kind = OPERAND_NONE;
                    count++;
                }
                /* SUB case handling শেষ। */
                break;

            case TAC_MUL:
                /* x * 0 or 0 * x → 0 */
                if (a1_zero || a2_zero) {
                    if (verbose) printf("  [alg] x * 0 → 0\n");
                    /* multiplication annihilator rule। */
                    instr->opcode = TAC_LOAD_INT;
                    set_int_const(&instr->arg1, 0);
                    instr->arg2.kind = OPERAND_NONE;
                    count++;
                }
                /* x * 1 → x */
                else if (a2_one) {
                    if (verbose) printf("  [alg] x * 1 → x\n");
                    /* identity rule: result = arg1 */
                    convert_to_assign(instr, *a1);
                    count++;
                }
                /* 1 * x → x */
                else if (a1_one) {
                    if (verbose) printf("  [alg] 1 * x → x\n");
                    /* identity rule: result = arg2 */
                    convert_to_assign(instr, *a2);
                    count++;
                }
                /* MUL case handling শেষ। */
                break;

            case TAC_DIV:
                /* x / 1 → x */
                if (a2_one) {
                    if (verbose) printf("  [alg] x / 1 → x\n");
                    /* division identity: result = arg1 */
                    convert_to_assign(instr, *a1);
                    count++;
                }
                /* DIV case handling শেষ। */
                break;

            case TAC_POW:
                /* x ** 0 → 1 */
                if (a2_zero) {
                    if (verbose) printf("  [alg] x ** 0 → 1\n");
                    /* power-zero rule: constant 1 */
                    instr->opcode = TAC_LOAD_INT;
                    set_int_const(&instr->arg1, 1);
                    instr->arg2.kind = OPERAND_NONE;
                    count++;
                }
                /* x ** 1 → x */
                else if (a2_one) {
                    if (verbose) printf("  [alg] x ** 1 → x\n");
                    /* power-one identity: result = arg1 */
                    convert_to_assign(instr, *a1);
                    count++;
                }
                /* POW case handling শেষ। */
                break;

            default:
                /* এই pass-এ unhandled opcode-এ কোনো পরিবর্তন করিনা। */
                break;
        }
    }

    /* মোট simplification count caller-এ ফেরত দিই। */
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
    /* 0 বা negative সংখ্যা power-of-two হতে পারে না। */
    if (val <= 0) return -1;
    /* bit trick: power-of-two হলে val & (val-1) == 0 হয়। */
    if ((val & (val - 1)) != 0) return -1;
    /* val = 2^exp থেকে exp বের করার কাউন্টার। */
    int exp = 0;
    /* ডানদিকে shift করে 1-এ নামিয়ে exponent গণনা করি। */
    while (val > 1) { val >>= 1; exp++; }
    /* সফল হলে base-2 exponent ফেরত দিই। */
    return exp;
}

int opt_strength_reduction(TACFunction *func, bool verbose) {
    /* strength reduction থেকে মোট কয়টা rewrite হলো তার হিসাব। */
    int count = 0;
    /* safety guard: function null হলে কাজ করার কিছু নেই। */
    if (!func) return 0;

    /* function-এর TAC instruction গুলো একে একে স্ক্যান করি। */
    for (TACInstr *instr = func->first; instr; instr = instr->next) {
        /* dead instruction optimize করার দরকার নেই। */
        if (instr->is_dead) continue;

        /* x * 2 → x + x */
        if (instr->opcode == TAC_MUL) {
            /* arg2 যদি constant 2 হয়, তখন x*2 কে x+x এ নামাই। */
            if (is_int_const(&instr->arg2) && instr->arg2.val.int_val == 2) {
                /* verbose mode-এ applied optimization log। */
                if (verbose) printf("  [str] x * 2 → x + x\n");
                /* opcode multiply থেকে add এ রিরাইট। */
                instr->opcode = TAC_ADD;
                /* পুরোনো arg2 heap-backed হলে আগে clean করি। */
                release_operand(&instr->arg2);
                /* arg2-এ arg1-এর deep copy বসিয়ে x+x ফর্ম বানাই। */
                instr->arg2 = dup_operand(instr->arg1);
                /* transformation counter update। */
                count++;
            }
            /* arg1 যদি constant 2 হয়, 2*x কেই একইভাবে x+x বানাই। */
            else if (is_int_const(&instr->arg1) && instr->arg1.val.int_val == 2) {
                if (verbose) printf("  [str] 2 * x → x + x\n");
                /* opcode rewrite to ADD। */
                instr->opcode = TAC_ADD;
                /* arg1-এর old payload release করি। */
                release_operand(&instr->arg1);
                /* arg1-এ arg2-এর copy বসিয়ে x+x নিশ্চিত করি। */
                instr->arg1 = dup_operand(instr->arg2);
                count++;
            }
        }

        /* pow(x, 2) → x * x */
        if (instr->opcode == TAC_POW &&
            is_int_const(&instr->arg2) && instr->arg2.val.int_val == 2) {
            if (verbose) printf("  [str] x ** 2 → x * x\n");
            /* power opcode-কে multiplication-এ নামিয়ে দিই। */
            instr->opcode = TAC_MUL;
            /* exponent operand আর লাগবে না, তাই release। */
            release_operand(&instr->arg2);
            /* arg2-এ base-এর copy বসিয়ে x*x তৈরি করি। */
            instr->arg2 = dup_operand(instr->arg1);
            /* rewrite count বাড়াই। */
            count++;
        }

        /* pow(x, 3) → x * x * x  – we leave as-is for simplicity */
        /* note: higher-degree expansion intentionally skip করা হয়েছে। */
    }

    /* caller-কে মোট successful strength-reduction সংখ্যা ফেরত দিই। */
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
    /* এই pass-এ মোট কয়টি redundant load eliminate হলো তার হিসাব। */
    int count = 0;
    /* safety: function pointer না থাকলে optimize করার কিছু নেই। */
    if (!func) return 0;

    /* Track recent loads: opcode + value → temp_id */
    /* basic-block local recent-constant cache এর সর্বোচ্চ আকার। */
    #define MAX_RECENT 256
    /* recent table-এ প্রতিটি entry একটি constant load-এর signature ধরে। */
    struct {
        /* কোন LOAD opcode (INT/FLOAT/BOOL) ছিল। */
        TACOpcode opcode;
        /* int constant value (LOAD_INT এর জন্য)। */
        long long int_val;
        /* float constant value (LOAD_FLOAT এর জন্য)। */
        double float_val;
        /* bool constant value (LOAD_BOOL এর জন্য)। */
        int bool_val;
        /* constant যেই temp-এ প্রথম load হয়েছিল তার id। */
        int temp_id;
        /* ঐ temp-এর data type metadata। */
        DataType data_type;
        /* slot বর্তমানে valid কিনা। */
        bool valid;
    } recent[MAX_RECENT];
    /* recent table-এ বর্তমানে কয়টি entry আছে। */
    int recent_count = 0;

    /* function-এর instruction list forward scan। */
    for (TACInstr *instr = func->first; instr; instr = instr->next) {
        /* dead mark করা instruction skip করি। */
        if (instr->is_dead) continue;

        /* Reset on control flow */
        /* control-flow boundary এ block-local reuse unsafe, তাই cache reset। */
        if (instr->opcode == TAC_LABEL || instr->opcode == TAC_FUNC_BEGIN ||
            instr->opcode == TAC_CALL || instr->opcode == TAC_GOTO ||
            instr->opcode == TAC_IF_GOTO || instr->opcode == TAC_IF_FALSE_GOTO) {
            /* recent entries invalidate না করে logical clear (count=0)। */
            recent_count = 0;
            /* boundary instruction handle করে পরের instruction-এ যাই। */
            continue;
        }

        /* Check LOADs of the same constant */
        /* কেবল temp result + LOAD_* হলে duplicate constant detect করব। */
        if (is_temp(&instr->result) &&
            (instr->opcode == TAC_LOAD_INT ||
             instr->opcode == TAC_LOAD_FLOAT ||
             instr->opcode == TAC_LOAD_BOOL)) {

            /* Search for matching recent load */
            /* সাম্প্রতিক table-এ একই signature আগে এসেছে কিনা খুঁজি। */
            for (int i = 0; i < recent_count; i++) {
                /* invalid slot হলে skip। */
                if (!recent[i].valid) continue;
                /* opcode mismatch হলে এই entry matching হতে পারবে না। */
                if (recent[i].opcode != instr->opcode) continue;

                /* value-level exact match হয়েছে কিনা track করার flag। */
                bool match = false;
                /* LOAD_INT হলে integer value compare করি। */
                if (instr->opcode == TAC_LOAD_INT &&
                    is_int_const(&instr->arg1) &&
                    recent[i].int_val == instr->arg1.val.int_val) {
                    match = true;
                }
                /* LOAD_FLOAT হলে float value compare করি। */
                if (instr->opcode == TAC_LOAD_FLOAT &&
                    is_float_const(&instr->arg1) &&
                    recent[i].float_val == instr->arg1.val.float_val) {
                    match = true;
                }
                /* LOAD_BOOL হলে boolean value compare করি। */
                if (instr->opcode == TAC_LOAD_BOOL &&
                    is_bool_const(&instr->arg1) &&
                    recent[i].bool_val == instr->arg1.val.bool_val) {
                    match = true;
                }

                /* match পেলে নতুন load বাদ দিয়ে আগের temp reuse করি। */
                if (match) {
                    /* verbose trace: কোন temp কোন temp-কে reuse করল। */
                    if (verbose) printf("  [rle] t%d = same as t%d\n",
                                       instr->result.val.temp_id,
                                       recent[i].temp_id);
                    /* Convert to assignment from the earlier temp */
                    /* LOAD_* কে ASSIGN এ রিরাইট। */
                    instr->opcode = TAC_ASSIGN;
                    /* arg1-এ আগের temp operand বসাই। */
                    instr->arg1.kind = OPERAND_TEMP;
                    instr->arg1.data_type = recent[i].data_type;
                    instr->arg1.val.temp_id = recent[i].temp_id;
                    /* arg2 এই ফর্মে দরকার নেই। */
                    instr->arg2.kind = OPERAND_NONE;
                    /* successful elimination count বাড়াই। */
                    count++;
                    /* এই instruction-এর জন্য কাজ শেষ, record phase skip। */
                    goto next_instr;
                }
            }

            /* Record this load */
            /* duplicate না পেলে current load-কে recent table-এ রাখি। */
            if (recent_count < MAX_RECENT) {
                /* opcode/temp/type metadata store। */
                recent[recent_count].opcode = instr->opcode;
                recent[recent_count].temp_id = instr->result.val.temp_id;
                recent[recent_count].data_type = instr->result.data_type;
                recent[recent_count].valid = true;
                /* opcode-specific constant payload store। */
                if (instr->opcode == TAC_LOAD_INT)
                    recent[recent_count].int_val = instr->arg1.val.int_val;
                else if (instr->opcode == TAC_LOAD_FLOAT)
                    recent[recent_count].float_val = instr->arg1.val.float_val;
                else if (instr->opcode == TAC_LOAD_BOOL)
                    recent[recent_count].bool_val = instr->arg1.val.bool_val;
                /* table size increment। */
                recent_count++;
            }
            /* table full হলে silently ignore: correctness অক্ষুণ্ণ থাকে। */
        }
        /* goto target label: match হলে control এখানে নামে। */
        next_instr:;
    }
    /* macro scope cleanup। */
    #undef MAX_RECENT

    /* caller-কে মোট eliminated redundant loads ফেরত দিই। */
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
    /* side-effecting opcode-গুলো eliminate করা নিরাপদ নয়, তাই true ফেরত দিই। */
    switch (op) {
        /* I/O, call, control-flow, scope এবং mutation ধরনের instruction। */
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
            /* তালিকার বাইরে থাকলে side-effect নেই ধরে false। */
            return false;
    }
}

/* Check if a temp_id is used in any operand of any subsequent instruction */
static bool temp_is_used_after(TACInstr *start, int temp_id) {
    /* Scan the entire function for uses of this temp.
     * We must check backward too because loops create back-edges:
     * a temp defined late in the loop may be used at the loop header. */

    /* Forward scan from start->next */
    /* সাধারণ forward use-analysis: current instruction-এর পর থেকে দেখি। */
    for (TACInstr *instr = start->next; instr; instr = instr->next) {
        /* dead instruction-এ use ধরা হবে না। */
        if (instr->is_dead) continue;
        /* arg1-এ temp_id পাওয়া গেলে temp জীবিত (live)। */
        if (instr->arg1.kind == OPERAND_TEMP && instr->arg1.val.temp_id == temp_id)
            return true;
        /* arg2-এ temp_id ব্যবহার আছে কিনা চেক। */
        if (instr->arg2.kind == OPERAND_TEMP && instr->arg2.val.temp_id == temp_id)
            return true;
        /* arg3-এ temp_id ব্যবহার আছে কিনা চেক। */
        if (instr->arg3.kind == OPERAND_TEMP && instr->arg3.val.temp_id == temp_id)
            return true;
    }
    /* Backward scan from start->prev (for loop back-edges) */
    /* loop back-edge conservative handling: পিছনের অংশেও use খুঁজি। */
    for (TACInstr *instr = start->prev; instr; instr = instr->prev) {
        /* dead instruction ignore। */
        if (instr->is_dead) continue;
        /* backward region arg1 use check। */
        if (instr->arg1.kind == OPERAND_TEMP && instr->arg1.val.temp_id == temp_id)
            return true;
        /* backward region arg2 use check। */
        if (instr->arg2.kind == OPERAND_TEMP && instr->arg2.val.temp_id == temp_id)
            return true;
        /* backward region arg3 use check। */
        if (instr->arg3.kind == OPERAND_TEMP && instr->arg3.val.temp_id == temp_id)
            return true;
    }
    /* কোথাও use না পেলে temp-টি dead হিসেবে ধরা যাবে। */
    return false;
}

int opt_dead_code_elimination(TACFunction *func, bool verbose) {
    /* এই pass-এ মোট কতটি instruction dead হিসেবে mark হলো। */
    int count = 0;
    /* safety guard: function null হলে pass চালানো যাবে না। */
    if (!func) return 0;

    /* function-এর সব instruction forward scan করি। */
    for (TACInstr *instr = func->first; instr; instr = instr->next) {
        /* আগে থেকেই dead হলে পুনরায় বিচার করব না। */
        if (instr->is_dead) continue;
        /* side-effecting instruction কখনো dead ধরে বাদ দেওয়া যাবে না। */
        if (has_side_effect(instr->opcode)) continue;

        /* Only eliminate instructions that write to temps */
        /* temp result না হলে DCE scope-এর বাইরে রাখি। */
        if (!is_temp(&instr->result)) continue;

        /* ASSIGN to a named var (result is var) – don't eliminate */
        /* named variable write program state পরিবর্তন করতে পারে, তাই retain। */
        if (instr->opcode == TAC_ASSIGN && instr->result.kind == OPERAND_VAR)
            continue;

        /* Check if this temp is used later */
        /* use-analysis false হলে producer instruction dead। */
        if (!temp_is_used_after(instr, instr->result.val.temp_id)) {
            /* verbose mode-এ dead mark trace print করি। */
            if (verbose) printf("  [dce] dead: t%d from %s\n",
                               instr->result.val.temp_id,
                               tac_opcode_to_string(instr->opcode));
            /* instruction-টিকে তাৎক্ষণিক unlink নয়, আগে dead mark করি। */
            instr->is_dead = true;
            /* dead count update। */
            count++;
        }
    }

    /* caller-কে মোট dead-marked instruction সংখ্যা ফেরত দিই। */
    return count;
}

/* ============================================================================
 * SWEEP: Remove dead instructions from the linked list
 * ============================================================================
 */
int opt_sweep_dead(TACFunction *func) {
    /* sweep phase-এ আসলে unlink+free হওয়া instruction-এর সংখ্যা। */
    int count = 0;
    /* safety: null function হলে sweep করার কিছুই নেই। */
    if (!func) return 0;

    /* linked list traversal pointer first instruction থেকে শুরু। */
    TACInstr *instr = func->first;
    /* list শেষ না হওয়া পর্যন্ত iterate করি। */
    while (instr) {
        /* বর্তমান node delete হলে next হারিয়ে যাবে, তাই আগে save করি। */
        TACInstr *next = instr->next;
        /* DCE pass যেগুলো dead mark করেছে শুধু সেগুলোই sweep করি। */
        if (instr->is_dead) {
            /* Unlink from doubly-linked list */
            /* prev থাকলে prev->next কে current-এর next এ যুক্ত করি। */
            if (instr->prev) instr->prev->next = instr->next;
            /* current head হলে head pointer আগাই। */
            else func->first = instr->next;

            /* next থাকলে next->prev কে current-এর prev এ বসাই। */
            if (instr->next) instr->next->prev = instr->prev;
            /* current tail হলে tail pointer পেছাই। */
            else func->last = instr->prev;

            /* live instruction count বজায় রাখতে এক ধাপ কমাই। */
            func->instr_count--;

            /* Free operands and instruction */
            /* result operand heap string/name থাকলে মুক্ত করি। */
            if (instr->result.kind == OPERAND_VAR || instr->result.kind == OPERAND_FUNC)
                free(instr->result.val.name);
            else if (instr->result.kind == OPERAND_STRING)
                free(instr->result.val.str_val);
            /* arg1 operand heap payload free। */
            if (instr->arg1.kind == OPERAND_VAR || instr->arg1.kind == OPERAND_FUNC)
                free(instr->arg1.val.name);
            else if (instr->arg1.kind == OPERAND_STRING)
                free(instr->arg1.val.str_val);
            /* arg2 operand heap payload free। */
            if (instr->arg2.kind == OPERAND_VAR || instr->arg2.kind == OPERAND_FUNC)
                free(instr->arg2.val.name);
            else if (instr->arg2.kind == OPERAND_STRING)
                free(instr->arg2.val.str_val);
            /* arg3 operand heap payload free। */
            if (instr->arg3.kind == OPERAND_VAR || instr->arg3.kind == OPERAND_FUNC)
                free(instr->arg3.val.name);
            else if (instr->arg3.kind == OPERAND_STRING)
                free(instr->arg3.val.str_val);
            /* সব operand clean হলে instruction node free করি। */
            free(instr);

            /* সফলভাবে একটি dead node sweep হয়েছে। */
            count++;
        }
        /* delete হোক/না হোক saved next-এ আগাই। */
        instr = next;
    }

    /* মোট physically removed dead instruction সংখ্যা ফেরত দিই। */
    return count;
}

/* ============================================================================
 * OPTIONS
 * ============================================================================
 */

OptOptions opt_default_options(OptLevel level) {
    /* return করার জন্য options struct local-এ নিই। */
    OptOptions opts;
    /* পুরো struct zero-init: unset field যেন garbage না থাকে। */
    memset(&opts, 0, sizeof(opts));
    /* caller যে optimization level চেয়েছে সেটি ধরে রাখি। */
    opts.level = level;
    /* default ভাবে verbose logging বন্ধ। */
    opts.verbose = false;

    /* selected level অনুযায়ী কোন কোন pass চালু হবে সেট করি। */
    switch (level) {
        case OPT_LEVEL_0:
            /* No optimizations */
            /* level 0: সব optimization disabled, pure baseline behavior। */
            break;
        case OPT_LEVEL_1:
            /* level 1: safe/basic subset (fold + dce) enable। */
            opts.constant_folding = true;
            opts.dead_code_elimination = true;
            break;
        case OPT_LEVEL_2:
            /* level 2: aggressive/general optimization pass set enable। */
            opts.constant_folding = true;
            opts.constant_propagation = true;
            opts.dead_code_elimination = true;
            opts.algebraic_simplification = true;
            opts.strength_reduction = true;
            opts.redundant_load_elimination = true;
            break;
    }

    /* প্রস্তুত default option সেট caller-কে ফেরত দিই। */
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
    /* safety: optimize করার target function না থাকলে early return। */
    if (!func) return;

    /* fixpoint loop অসীম না হতে hard cap রাখা হয়েছে। */
    int max_iterations = 10;  /* Avoid infinite loops */

    /* একই function-এর উপর বারবার pass চালিয়ে স্থিতিশীল অবস্থা ধরি। */
    for (int iter = 0; iter < max_iterations; iter++) {
        /* এই iteration-এ মোট কত পরিবর্তন হলো তার যোগফল। */
        int changes = 0;

        /* 1. Constant Propagation (before folding, so folding has constants) */
        /* enable flag true হলে pass চালাই; false হলে skip। */
        if (opts->constant_propagation) {
            /* pass-returned transformation count local n-এ ধরি। */
            int n = opt_constant_propagation(func, opts->verbose);
            /* iteration total change আপডেট। */
            changes += n;
            /* global stats accumulator আপডেট। */
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

        /* প্রতিটি iteration-কে একটি pass-round হিসেবে গুনি। */
        stats->passes_run++;

        /* কোনো পরিবর্তন না হলে fixpoint পৌঁছেছে, loop break। */
        if (changes == 0) break;  /* Fixpoint reached */
    }

    /* Sweep dead instructions */
    /* dead-marked node-গুলো physical list থেকে remove করি। */
    opt_sweep_dead(func);
}

OptStats ir_optimize(TACProgram *program, OptOptions *options) {
    /* সব optimization statistics এই struct-এ জমা হবে। */
    OptStats stats;
    /* clean zero-init: সব counter deterministic শুরু। */
    memset(&stats, 0, sizeof(stats));

    /* invalid input হলে empty stats return। */
    if (!program || !options) return stats;
    /* O0 level-এ optimization disable, তাই early return। */
    if (options->level == OPT_LEVEL_0) return stats;

    /* optimization শুরুর আগে মোট instruction count snapshot। */
    stats.total_instructions_before = ir_count_total(program);

    /* verbose হলে run header print করি। */
    if (options->verbose) {
        printf("\n=== Optimization Pass (Level %d) ===\n", options->level);
    }

    /* Optimize main function */
    /* main function থাকলে তার label সহ verbose info print। */
    if (options->verbose && program->main_func) {
        printf("\nOptimizing <main>:\n");
    }
    /* main function-এ pass driver চালাই (NULL হলেও safe)। */
    optimize_function(program->main_func, options, &stats);

    /* Optimize user functions */
    /* linked list ধরে user-defined function গুলো iterate করি। */
    TACFunction *f = program->functions;
    while (f) {
        /* verbose mode-এ current function name দেখাই। */
        if (options->verbose) {
            printf("\nOptimizing %s:\n", f->name ? f->name : "<?>");
        }
        /* প্রতিটি user function-এ একই optimization pipeline চালাই। */
        optimize_function(f, options, &stats);
        /* পরের function node-এ অগ্রসর হই। */
        f = f->next;
    }

    /* optimization শেষে মোট instruction count snapshot। */
    stats.total_instructions_after = ir_count_total(program);

    /* verbose mode-এ final aggregated statistics print। */
    if (options->verbose) {
        printf("\n");
        opt_print_stats(&stats);
    }

    /* caller-কে পূর্ণ optimization ফলাফল ফেরত দিই। */
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
