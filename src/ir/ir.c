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
/*
ir_gen_expression
Expression AST থেকে TACOperand বানায় (temp/var/literal operand)।
যেমন binary op, unary op, function call, list access।

ir_gen_statement
Statement AST থেকে TAC instruction emit করে।
যেমন decl, assign, if, while, return, display।

ir_gen_node
Dispatcher/entry helper।
Program হলে top-level statements iterate করে, নইলে statement handler-এ দেয়।
Flow টা সাধারণত এমন:

ir_generate -> ir_gen_node -> ir_gen_statement -> ir_gen_expression
nested case-এ আবার recursion হয়
just basically code remains modular and organized, expression vs statement lowering remains different via this one.
*/
static TACOperand ir_gen_expression(IRGenContext *ctx, ASTNode *node);
static void       ir_gen_statement(IRGenContext *ctx, ASTNode *node);
static void       ir_gen_node(IRGenContext *ctx, ASTNode *node);

/* ============================================================================
 * OPERAND CONSTRUCTORS
 * ============================================================================
 */
/*
TAC instruction-এ সবসময় result, arg1, arg2 field থাকে, কিন্তু সব opcode-এ সব field লাগে না।
যে slot লাগবে না, সেখানে meaningful sentinel দরকার।
সেই sentinel-ই OPERAND_NONE।
লাইন ধরে:

TACOperand op;
লোকাল operand object নেয়।

memset(&op, 0, sizeof(op));
পুরো struct zero-initialize করে।
কেন: union field-এ garbage value না থাকুক।

op.kind = OPERAND_NONE;
এই operand “বাস্তব operand না” হিসেবে mark হয়।

op.data_type = TYPE_UNKNOWN;
কারণ এটা কোনো actual typed value না।

return op;
caller empty slot হিসেবে use করে।

The places it has been used, like some examples:

tac_emit_label, tac_emit_goto এ unused args ভরতে: ir.c:233, ir.c:239
unary op emit এ arg2 ফাঁকা রাখতে
tac_instr_create এ default arg3 none সেট করতে: ir.c:181
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
    /*op.val.temp_id = id;
temp id সেট হচ্ছে, যেমন id=3 হলে পরে t3 হিসেবে print হবে।*/
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
    /*পুরো struct clean/zero initialize করে, যাতে union field-এ garbage না থাকে।*/
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
/*if (src.kind == OPERAND_VAR || src.kind == OPERAND_FUNC)
যদি operand-এ name pointer থাকে (variable/function নাম), তখন শুধু pointer copy safe না।
তাই strdup করে নতুন memory-তে নাম copy করে:
dst.val.name = src.val.name ? strdup(src.val.name) : NULL;
else if (src.kind == OPERAND_STRING)
string literal operand হলেও একই সমস্যা।
তাই string-ও strdup দিয়ে deep copy:
dst.val.str_val = src.val.str_val ? strdup(src.val.str_val) : NULL;
return dst;
এখন dst independent copy; source pointer lifetime-এর ওপর নির্ভর করে না।*/
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
        /*list যদি non-empty হয়, old tail-এর next কে নতুন instruction করা হয়।*/
        func->last->next = instr;
    } else {
        /*list empty হলে এই instruction-ই first node।*/
        func->first = instr;
    }
    func->last = instr;
    func->instr_count++;
}

/* ============================================================================
 * PUBLIC EMIT FUNCTIONS
 * ============================================================================
 */
/*
tac_new_temp
কাজ: নতুন temporary id দেয়।
return prog->next_temp++; মানে আগে current value return করে, তারপর counter বাড়ায়।
তাই প্রথম temp হয় t0, তারপর t1, t2...
এটা expression lowering-এ খুব দরকার, কারণ intermediate result temp-এ থাকে।
*/
int tac_new_temp(TACProgram *prog) {
    return prog->next_temp++;
}
/*
tac_new_label
কাজ: নতুন label id দেয় control-flow এর জন্য।
একই post-increment pattern।
label sequence হয় L0, L1, L2...
if/while/repeat/goto target marking-এ লাগে।*/

int tac_new_label(TACProgram *prog) {
    return prog->next_label++;
}
/*tac_instr_create দিয়ে instruction object বানায়
tac_func_append দিয়ে function instruction list-এ append করে
পরে instruction pointer return করে
it is a core helper thoughh here.
*/

TACInstr *tac_emit(TACFunction *func, TACOpcode op,
                   TACOperand result, TACOperand arg1, TACOperand arg2) {
    TACInstr *instr = tac_instr_create(op, result, arg1, arg2);
    tac_func_append(func, instr);
    return instr;
}
/*
tac_emit এর extended version, যেখানে arg3 লাগে।
tac_instr_create প্রথমে result/arg1/arg2 সেট করে।
তারপর instr->arg3 = copy_operand(arg3) দিয়ে ৪র্থ slot ভরে।
কেন copy_operand: ownership safe রাখা (deep-copy semantics maintain করা)।
এটি mainly ternary/pseudo-ternary TAC এর জন্য, যেমন BETWEEN, LIST_SET ধরনের pattern।
*/

TACInstr *tac_emit3(TACFunction *func, TACOpcode op,
                    TACOperand result, TACOperand arg1,
                    TACOperand arg2, TACOperand arg3) {
    TACInstr *instr = tac_instr_create(op, result, arg1, arg2);
    instr->arg3 = copy_operand(arg3);
    tac_func_append(func, instr);
    return instr;
}
/*
wrapper function, TAC_LABEL emit করে।
result slot-এ label operand যায়।
arg1/arg2 unused, তাই tac_operand_none()।
IR call-site readable হয়:
tac_emit_label(func, end_label)
instead of full tac_emit(...) with boilerplate none operands.
*/

TACInstr *tac_emit_label(TACFunction *func, int label_id) {
    return tac_emit(func, TAC_LABEL,
                    tac_operand_label(label_id),
                    tac_operand_none(),
                    tac_operand_none());
}
/*
tac_emit_goto
TAC_GOTO emit করে।
result-এ target label।
arg1/arg2 none।
unconditional jump।
*/
TACInstr *tac_emit_goto(TACFunction *func, int label_id) {
    return tac_emit(func, TAC_GOTO,
                    tac_operand_label(label_id),
                    tac_operand_none(),
                    tac_operand_none());
}
/*
tac_emit_if_goto
TAC_IF_GOTO emit করে।
result = target label
arg1 = cond
arg2 = none
semantics: if cond goto Lx
*/
TACInstr *tac_emit_if_goto(TACFunction *func, TACOperand cond, int label_id) {
    return tac_emit(func, TAC_IF_GOTO,
                    tac_operand_label(label_id),
                    cond,
                    tac_operand_none());
}
/*
tac_emit_if_false_goto
TAC_IF_FALSE_GOTO emit করে।
if condition false হলে jump করে।
if-else lowering এ খুব common:
cond false হলে else/end label-এ চলে যায়।
*/
TACInstr *tac_emit_if_false_goto(TACFunction *func, TACOperand cond, int label_id) {
    return tac_emit(func, TAC_IF_FALSE_GOTO,
                    tac_operand_label(label_id),
                    cond,
                    tac_operand_none());
}

/* ============================================================================
 * TAC PROGRAM / FUNCTION MANAGEMENT
 * 
 * tac_program_create: পুরো IR container bootstrap করে।
tac_function_create: একেকটা function IR bucket বানায়।
tac_program_add_function: সেই bucket program-এর linked list-এ register করে।
এর ওপর ভর করে পরে ir_generate function AST traversal শেষে complete TAC program build করে।
============================================================================
 */
/*
TACProgram *tac_program_create(void)
ir.c:376 এ calloc(1, sizeof(TACProgram)) দিয়ে TACProgram allocate করা হচ্ছে।
calloc ব্যবহার করার ফলে সব field শুরুতে zero/null হয়।
তারপর main_func তৈরি হচ্ছে tac_function_create(NULL, TYPE_NOTHING) দিয়ে।
এখানে NULL name মানে এটা user-defined function না, implicit top-level main container।
TYPE_NOTHING return type দেওয়া হয়েছে কারণ top-level script body সাধারণ function return contract follow করে না।
শেষে ready TACProgram ফেরত দিচ্ছে।
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
    /*নতুন function-কে existing list head-এর আগে attach করছে।
*/
    func->next = prog->functions;
    /*এখন নতুন function list head।
*/
    prog->functions = func;
    /*program-level function count update।*/
    prog->func_count++;
}

/* ============================================================================
 * FREE
 * ============================================================================
 */

static void tac_function_free(TACFunction *func) {
    if (!func) return;
    /* Free instructions */
    /*instr = func->first দিয়ে linked list head নেয়।*/
    TACInstr *instr = func->first;
    /*loop এ আগে next = instr->next save করে, তারপর tac_instr_free(instr)।
কেন আগে next save দরকার:
current node free করার পর আর instr->next access করা যাবে না (use-after-free risk)।*/
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
/*pura tacProgram destroy kortase.*/
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
    /*userdefined function list er head ney*/
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
/*IR print করার সময় numeric enum দেখালে বোঝা কঠিন হয়, string দেখালে readable হয়। for that reason, we got to do it in this way.*/
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
/*
__thread মানে প্রতিটি thread নিজের আলাদা buffer পাবে।
কেন দরকার:
function টি const char* return করে, তাই temporary formatted string রাখার জায়গা লাগে।
global shared buffer হলে multi-thread এ race হতো।
256 byte fixed size, ছোট formatted operand এর জন্য যথেষ্ট ধরা হয়েছে।
*/
static __thread char operand_buf[256];
/*এ function signature।
কাজ: operand kind দেখে string representation return করা।

*/

const char *tac_operand_to_string(TACOperand *op) {
    /*
    null safety।
invalid pointer এ crash না করে placeholder দেয়।
    */
    if (!op) return "?";
    switch (op->kind) {
        /*empty slot বোঝায় (unused operand)।*/
        case OPERAND_NONE:
            return "_";
        /*এ snprintf দিয়ে t0, t1 টাইপ string বানায়।*/
        case OPERAND_TEMP:
            snprintf(operand_buf, sizeof(operand_buf), "t%d", op->val.temp_id);
            return operand_buf;
        /*variable operand বোঝায়।*/
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
    /*unknown kind হলেও function defined behavior দেয়।
debugging-friendly fail-safe।*/
    return "?";
}

void ir_print_instr(TACInstr *instr) {
    /*এ null check: instruction না থাকলে return।*/
    if (!instr) return;
    /*এ is_dead হলে আগে ; DEAD: prefix দেয়, যাতে dead-code optimization-এর effect দেখা যায়।*/
    if (instr->is_dead) {
        printf("  ; DEAD: ");
    }
/*থেকে opcode অনুযায়ী আলাদা print format দেয়।*/
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
            /*tac_operand_to_string internally একই thread-local buffer reuse করে।
consecutive 3 বার call করলে আগের string overwrite হয়ে যেতে পারে।
তাই এখানে snapshot copy করা হচ্ছে, যাতে print করার সময় তিনটাই ঠিক থাকে।
এইটা correctness-এর জন্য খুবই important।*/
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
/*TAC_RETURN ব্লক
যদি arg1 থাকে (OPERAND_NONE না), তাহলে return value সহ print:
return x
না থাকলে plain return print:
return*/
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
/*TAC_READ case
ir.c:668
এটা result = read format print করে।
মানে runtime input নিয়ে instr->result এ রাখবে।
tac_operand_to_string(&instr->result) দিয়ে target variable/temp-এর নাম বের করে।*/
        case TAC_READ:
            printf("  %s = read\n", tac_operand_to_string(&instr->result));
            return;
/*TAC_ASK case
ir.c:672
semantic format: result = ask(prompt)
res এ result operand-এর string নেয়, আর prompt_buf এ arg1 (prompt expression) stringify করে রাখে।
তারপর final line print: x = ask("Enter name") টাইপ output।
*/
        case TAC_ASK: {
            const char *res = tac_operand_to_string(&instr->result);
            char prompt_buf[256];
            snprintf(prompt_buf, sizeof(prompt_buf), "%s",
                     tac_operand_to_string(&instr->arg1));
            printf("  %s = ask(%s)\n", res, prompt_buf);
            return;
        }
/*case TAC_DECL: { ... }
যখন opcode declaration (DECL) হয়, এই branch run করে।
অর্থাৎ variable declaration IR line print হবে।
*/
        case TAC_DECL: {
            /*declaration target variable নাম বের করছে।
instr->result-এ declaration-এর symbol থাকে (যেমন x)।
উদাহরণ: result যদি var x হয়, name হবে "x"।*/
            const char *name = tac_operand_to_string(&instr->result);
            /*variable-এর data type কে string বানাচ্ছে।
যেমন TYPE_NUMBER -> "number", TYPE_TEXT -> "text" টাইপ mapping।*/
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
    /*Generic fallback printer
The comment marks a fallback path for opcodes that are not handled in special switch cases.
Without this block, many instructions would print nothing unless every opcode had a dedicated case.
It keeps the printer maintainable and future-proof: new opcodes can still be shown in a readable form.*/
    const char *res_str = tac_operand_to_string(&instr->result);
    /* Copy to local buf so second operand call doesn't clobber */
    char res_buf[256];
    /*res_str (result operand-এর string) লোকাল বাফারে কপি করছে।
কারণ tac_operand_to_string() একই internal buffer reuse করতে পারে, তাই সরাসরি pointer রেখে দিলে পরে overwrite হওয়ার ঝুঁকি থাকে।*/
    snprintf(res_buf, sizeof(res_buf), "%s", res_str);

    char a1_buf[256];
    snprintf(a1_buf, sizeof(a1_buf), "%s", tac_operand_to_string(&instr->arg1));
/*if (instr->arg2.kind != OPERAND_NONE)
যদি arg2 থাকে, তাহলে binary format print:
আউটপুট: result = arg1 OP arg2
উদাহরণ: t2 = t0 ADD t1*/
/*else if
arg2 না থাকলেও arg1 থাকলে unary/one-operand format:
আউটপুট: result = OP arg1
উদাহরণ: t3 = NEG t2*/
/*else
arg1/arg2 দুটিই না থাকলে শুধু opcode সহ:
আউটপুট: result = OP
এটি rare/fallback cases-এর জন্য।*/
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

    /*function pointer না থাকলে crash এড়ায়।*/
    if (!func) return;
    if (func->name) {
        /*যদি func->name থাকে, তাহলে user-defined function হিসেবে print করে:
function <name>(param1: type, param2: type) -> return_type
parameter loop এ প্রতিটা parameter নাম + type দেখায়।
if (i > 0) printf(", "); দিয়ে comma formatting ঠিক রাখে (আগে comma না, পরেরগুলোতে comma)।*/
        printf("function %s(", func->name);
        for (int i = 0; i < func->param_count; i++) {
            if (i > 0) printf(", ");
            printf("%s: %s",
                   func->param_names[i],
                   ast_data_type_to_string(func->param_types[i]));
        }
        printf(") -> %s\n", ast_data_type_to_string(func->return_type));
    } else {
        /*যদি func->name না থাকে, তাহলে function <main> print করে।
কারণ top-level code-এর implicit main function সাধারণত নামহীন থাকে।*/
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
    /*User-defined function গুলো আগে print
TACFunction *f = program->functions; দিয়ে function list head নেয়।
while loop এ একে একে ir_print_function(f) কল করে।
তাই সব named function আগে output এ আসে।
তারপর main/top-level print
ir_print_function(program->main_func);
top-level script/body যে implicit main function-এ lower হয়, সেটা শেষে দেখায়।*/
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
    /*String concat rule
if (op == OP_ADD && (left == TYPE_TEXT || right == TYPE_TEXT)) return TYPE_TEXT;
মানে + operator-এ যদি দুই পাশের যেকোনো একপাশ text হয়, result text ধরা হবে।
উদাহরণ:
"age: " + 20 → TYPE_TEXT
"A" + "B" → TYPE_TEXT
কেন দরকার:
ভাষায় + কে concat হিসেবে ব্যবহার করতে দিলে type inference ঠিক রাখতে হবে।
Decimal promotion rule
if (left == TYPE_DECIMAL || right == TYPE_DECIMAL) return TYPE_DECIMAL;
মানে arithmetic context-এ যদি একপাশ decimal হয়, result decimal।
উদাহরণ:
5 + 2.5 → decimal
10 * 0.1 → decimal
এটা standard numeric promotion rule, precision হারানো কমায়।
Fallback
return left;
উপরোক্ত special case না হলে left টাইপ result হিসেবে নেয়।*/
    if (op == OP_ADD && (left == TYPE_TEXT || right == TYPE_TEXT))
        return TYPE_TEXT;
    /* Decimal promotion */
    if (left == TYPE_DECIMAL || right == TYPE_DECIMAL)
        return TYPE_DECIMAL;
    return left;
}

static TACOperand ir_gen_expression(IRGenContext *ctx, ASTNode *node) {
    if (!node) return tac_operand_none();
/*temp/label counter access করার জন্য program handle নেয়।
যেমন নতুন temp লাগলে tac_new_temp(prog) ব্যবহার হবে।*/
    TACProgram  *prog = ctx->program;
    /*কোন function-এর instruction list-এ TAC emit হবে, সেটা নেয়।*/
    TACFunction *func = ctx->current_func;

    switch (node->type) {
        /* ---- Literals ---- */
        case AST_LITERAL_INT: {
            int t = tac_new_temp(prog);
            /*destination operand বানায় (t3, type number)।*/
            TACOperand dst = tac_operand_temp(t, TYPE_NUMBER);
            /*tac_emit(func, TAC_LOAD_INT, dst, tac_operand_int(node->data.literal_int.value), tac_operand_none());
instruction emit করে: integer literal-কে temp-এ load করা।
conceptual TAC: t3 = LOAD_INT 42*/
            tac_emit(func, TAC_LOAD_INT, dst,
                     tac_operand_int(node->data.literal_int.value),
                     tac_operand_none());
                     /*caller-কে বলে দেয় expression result কোথায় আছে: ওই temp-এ।*/
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
        /*op বের করে (+, -, *, == ইত্যাদি)।
left আর right recursively generate করে।
তাই nested expression (যেমন a + b * c) automaticভাবে আগে ভেঙে lower হয়।
binop_result_type(...) দিয়ে result type infer করে।
special case:
+ এবং যেকোনো একপাশ text হলে numeric add না করে TAC_CONCAT emit করে।
normal case:
নতুন temp t নেয়
opcode map করে (operator_to_tac(op))
t = left OP right style TAC emit করে
শেষে ওই temp operand return করে, যাতে parent expression এটা use করতে পারে।*/
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
        /*op নেয়: unary operator কোনটা (OP_NOT, OP_NEG ইত্যাদি)।
operand expression recursively generate করে।
তাই operand যদি complex হয়, আগে সেটা TAC-এ convert হয়ে আসে।
result type ঠিক করে:
OP_NOT হলে result সবসময় TYPE_FLAG (true/false)
নাহলে operand-এর type-ই ধরে (যেমন numeric negate)।
নতুন temp তৈরি করে (t)।
unary instruction emit করে:
dst = OP operand
arg2 লাগে না, তাই tac_operand_none()।
temp operand return করে, যাতে parent expression এটা ব্যবহার করতে পারে।*/
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
        /*তিনটা sub-expression আলাদা করে evaluate করছে:
মূল value
lower bound
upper bound
নতুন temp নিচ্ছে (t)।

result operand বানাচ্ছে TYPE_FLAG দিয়ে, কারণ between check সবসময় true/false দেয়।

tac_emit3(...) ব্যবহার করছে, কারণ এই opcode-তে 3টা input লাগে:

arg1 = val
arg2 = lower
arg3 = upper
শেষে temp result return করছে, যাতে parent expression এটা ব্যবহার করতে পারে।
কেন tac_emit3 দরকার:

সাধারণ tac_emit শুধু 2টা arg নেয়।
between operation inherently 3-operand, তাই extended emitter প্রয়োজন।*/
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
        /*argument list নেয়, তারপর nargs বের করে।
প্রতিটি arg recursively evaluate করে (nested expression support)।
প্রতিটি evaluated arg এর জন্য TAC_PARAM emit করে।
call convention অনুযায়ী arg pass preparation।
return type ঠিক করে:
semantic pass থেকে type জানা থাকলে সেটা
না থাকলে fallback TYPE_NUMBER।
return value ধরার জন্য নতুন temp নেয় (t)।
TAC_CALL emit করে:
result = dst (যেখানে return value যাবে)
arg1 = function name
arg2 = nargs
শেষে ওই temp return করে, যাতে outer expression call result use করতে পারে।*/
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
            /*t0 = LIST_CREATE 3
LIST_APPEND t0, e0
LIST_APPEND t0, e1
LIST_APPEND t0, e2
result operand: t0*/
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
    /* নিরাপত্তা: statement node না থাকলে কিছুই করার নেই। */
    if (!node) return;

    /* প্রোগ্রাম কনটেক্সট থেকে temp/label তৈরির হ্যান্ডেল নিই। */
    TACProgram  *prog = ctx->program;
    /* বর্তমানে যে function-এ TAC emit হবে, সেই function হ্যান্ডেল। */
    TACFunction *func = ctx->current_func;

    /* statement টাইপ দেখে নির্দিষ্ট lowering পথ বেছে নেওয়া হয়। */
    switch (node->type) {

        /* ---- Variable Declaration ---- */
        case AST_VAR_DECL: {
            /* ঘোষিত variable-এর semantic type (number/text/list ইত্যাদি)। */
            DataType vt = node->data.var_decl.var_type;
            /* declaration target variable operand বানাই। */
            TACOperand var_op = tac_operand_var(node->data.var_decl.name, vt);

            /* IR-এ declaration marker emit করি: DECL <name> : <type> */
            tac_emit(func, TAC_DECL, var_op, tac_operand_none(), tac_operand_none());

            /* initializer থাকলে আগে expression evaluate করি, পরে assign করি। */
            if (node->data.var_decl.initializer) {
                /* initializer expression থেকে result operand (temp/var/literal) পাই। */
                TACOperand val = ir_gen_expression(ctx, node->data.var_decl.initializer);
                /* declaration target-এ computed value assign করি। */
                tac_emit(func, TAC_ASSIGN,
                         tac_operand_var(node->data.var_decl.name, vt),
                         val, tac_operand_none());
            }
            /* এই case-এর কাজ শেষ। */
            break;
        }

        /* ---- Assignment ---- */
        case AST_ASSIGN: {
            /* assignment-এর RHS আগে evaluate করি। */
            TACOperand val = ir_gen_expression(ctx, node->data.assign.value);

            /* assignment target identifier বা index-expression যেকোনোটা হতে পারে। */
            ASTNode *target = node->data.assign.target;
            /* list/index assignment: list[idx] = val */
            if (target->type == AST_INDEX) {
                /* target array/list expression evaluate করি। */
                TACOperand arr = ir_gen_expression(ctx, target->data.index_expr.array);
                /* target index expression evaluate করি। */
                TACOperand idx = ir_gen_expression(ctx, target->data.index_expr.index);
                /* LIST_SET pseudo-ternary opcode: result=list, arg1=idx, arg2=val */
                tac_emit3(func, TAC_LIST_SET, arr, idx, val, tac_operand_none());
            /* simple variable assignment: x = val */
            } else if (target->type == AST_IDENTIFIER) {
                /* target type semantic phase থেকে পেলে সেটা, নাহলে fallback number। */
                DataType dt = target->data_type != TYPE_UNKNOWN
                              ? target->data_type : TYPE_NUMBER;
                /* variable target-এ RHS result assign করি। */
                tac_emit(func, TAC_ASSIGN,
                         tac_operand_var(target->data.identifier.name, dt),
                         val, tac_operand_none());
            } else {
                /* fallback: unusual target হলে expression হিসেবে lower করে ASSIGN দিই। */
                TACOperand tgt = ir_gen_expression(ctx, target);
                tac_emit(func, TAC_ASSIGN, tgt, val, tac_operand_none());
            }
            /* assignment case শেষ। */
            break;
        }

        /* ---- Display ---- */
        case AST_DISPLAY: {
            /* display করার expression evaluate করি। */
            TACOperand val = ir_gen_expression(ctx, node->data.display_stmt.value);
            /* TAC_DISPLAY-এ arg1 হিসেবে printed value যায়। */
            tac_emit(func, TAC_DISPLAY, tac_operand_none(), val, tac_operand_none());
            /* display case শেষ। */
            break;
        }

        /* ---- Ask (input with prompt) ---- */
        case AST_ASK: {
            /* prompt থাকলে evaluate করি, না থাকলে none operand। */
            TACOperand prompt = node->data.ask_stmt.prompt
                                ? ir_gen_expression(ctx, node->data.ask_stmt.prompt)
                                : tac_operand_none();
            /* ask target variable string হিসেবে ধরা হচ্ছে (বর্তমান design)। */
            TACOperand var_op = tac_operand_var(node->data.ask_stmt.target_var, TYPE_TEXT);
            /* result = ask(prompt) টাইপ IR emit। */
            tac_emit(func, TAC_ASK, var_op, prompt, tac_operand_none());
            /* ask case শেষ। */
            break;
        }

        /* ---- Read (simple input) ---- */
        case AST_READ: {
            /* read target variable operand। */
            TACOperand var_op = tac_operand_var(node->data.read_stmt.target_var, TYPE_TEXT);
            /* simple input read করে target-এ রাখার IR। */
            tac_emit(func, TAC_READ, var_op, tac_operand_none(), tac_operand_none());
            /* read case শেষ। */
            break;
        }

        /* ---- If / Else ---- */
        case AST_IF: {
            /* condition expression evaluate করি। */
            TACOperand cond = ir_gen_expression(ctx, node->data.if_stmt.condition);

            /* else branch থাকলে দুইটি label লাগে: else_label, end_label */
            if (node->data.if_stmt.else_branch) {
                /* false হলে else-এ jump target। */
                int else_label = tac_new_label(prog);
                /* then/else শেষে common merge label। */
                int end_label  = tac_new_label(prog);

                /* condition false হলে else label-এ যাই। */
                tac_emit_if_false_goto(func, cond, else_label);
                /* then branch generate করি। */
                ir_gen_node(ctx, node->data.if_stmt.then_branch);
                /* then শেষে else block skip করে end label-এ যাই। */
                tac_emit_goto(func, end_label);
                /* else label mark করি। */
                tac_emit_label(func, else_label);
                /* else branch generate করি। */
                ir_gen_node(ctx, node->data.if_stmt.else_branch);
                /* merge/end label mark করি। */
                tac_emit_label(func, end_label);
            } else {
                /* else না থাকলে শুধু end label যথেষ্ট। */
                int end_label = tac_new_label(prog);
                /* false হলে সরাসরি end label-এ jump। */
                tac_emit_if_false_goto(func, cond, end_label);
                /* true path/then branch generate করি। */
                ir_gen_node(ctx, node->data.if_stmt.then_branch);
                /* then শেষে end label mark করি। */
                tac_emit_label(func, end_label);
            }
            /* if case শেষ। */
            break;
        }

        /* ---- While Loop ---- */
        case AST_WHILE: {
            /* loop start এবং loop end label allocate করি। */
            int loop_start = tac_new_label(prog);
            int loop_end   = tac_new_label(prog);

            /* nested loop safe রাখতে আগের break/continue context save করি। */
            int saved_break    = ctx->loop_break_label;
            int saved_continue = ctx->loop_continue_label;
            /* বর্তমান loop-এর break target = loop_end */
            ctx->loop_break_label    = loop_end;
            /* বর্তমান loop-এর continue target = loop_start */
            ctx->loop_continue_label = loop_start;
            /* loop nesting depth বাড়াই। */
            ctx->in_loop++;

            /* loop start label বসাই। */
            tac_emit_label(func, loop_start);
            /* while condition evaluate করি। */
            TACOperand cond = ir_gen_expression(ctx, node->data.while_stmt.condition);
            /* condition false হলে loop_end-এ exit। */
            tac_emit_if_false_goto(func, cond, loop_end);
            /* loop body generate করি। */
            ir_gen_node(ctx, node->data.while_stmt.body);
            /* body শেষে আবার loop_start-এ ফিরি। */
            tac_emit_goto(func, loop_start);
            /* loop end label mark করি। */
            tac_emit_label(func, loop_end);

            /* loop context pop/restore করি। */
            ctx->in_loop--;
            ctx->loop_break_label    = saved_break;
            ctx->loop_continue_label = saved_continue;
            /* while case শেষ। */
            break;
        }

        /* ---- Repeat N times ---- */
        case AST_REPEAT: {
            /* repeat count expression evaluate করে limit operand নেই। */
            TACOperand limit = ir_gen_expression(ctx, node->data.repeat_stmt.count);
            /* loop counter temp তৈরি করি (iter)। */
            int iter_t = tac_new_temp(prog);
            TACOperand iter = tac_operand_temp(iter_t, TYPE_NUMBER);

            /* iter = 0 initialize করি। */
            tac_emit(func, TAC_LOAD_INT, iter,
                     tac_operand_int(0), tac_operand_none());

            /* repeat loop-এর প্রয়োজনীয় labels তৈরি করি। */
            int loop_start = tac_new_label(prog);
            int loop_end   = tac_new_label(prog);
            int loop_inc   = tac_new_label(prog);

            /* nested loop safe রাখতে আগের loop context save করি। */
            int saved_break    = ctx->loop_break_label;
            int saved_continue = ctx->loop_continue_label;
            /* break হলে loop_end-এ যাবে। */
            ctx->loop_break_label    = loop_end;
            /* continue হলে increment label-এ যাবে। */
            ctx->loop_continue_label = loop_inc;
            /* loop depth বাড়াই। */
            ctx->in_loop++;

            /* loop start label। */
            tac_emit_label(func, loop_start);
            /* cond = (iter >= limit) evaluate করি। */
            int cond_t = tac_new_temp(prog);
            TACOperand cond = tac_operand_temp(cond_t, TYPE_FLAG);
            tac_emit(func, TAC_GTE, cond,
                     tac_operand_temp(iter_t, TYPE_NUMBER), limit);
            /* cond true হলে desired repeat count পূর্ণ, loop_end-এ exit। */
            tac_emit_if_goto(func, cond, loop_end);

            /* repeat body generate করি। */
            ir_gen_node(ctx, node->data.repeat_stmt.body);

            /* increment label mark করি (continue এখানেই আসে)। */
            tac_emit_label(func, loop_inc);
            /* iter = iter + 1 */
            tac_emit(func, TAC_ADD,
                     tac_operand_temp(iter_t, TYPE_NUMBER),
                     tac_operand_temp(iter_t, TYPE_NUMBER),
                     tac_operand_int(1));
            /* পরের iteration-এর জন্য loop_start-এ jump। */
            tac_emit_goto(func, loop_start);
            /* loop end label mark। */
            tac_emit_label(func, loop_end);

            /* loop context restore করি। */
            ctx->in_loop--;
            ctx->loop_break_label    = saved_break;
            ctx->loop_continue_label = saved_continue;
            /* repeat case শেষ। */
            break;
        }

        /* ---- For-Each ---- */
        case AST_FOR_EACH: {
            /* iterable expression evaluate করে list operand নেই। */
            TACOperand list = ir_gen_expression(ctx, node->data.for_each_stmt.iterable);

            /* index counter idx temp তৈরি ও initialize করি। */
            int idx_t = tac_new_temp(prog);
            TACOperand idx = tac_operand_temp(idx_t, TYPE_NUMBER);
            tac_emit(func, TAC_LOAD_INT, idx,
                     tac_operand_int(0), tac_operand_none());

            /* for-each loop labels তৈরি। */
            int loop_start = tac_new_label(prog);
            int loop_end   = tac_new_label(prog);
            int loop_inc   = tac_new_label(prog);

            /* loop context save + নতুন loop context set। */
            int saved_break    = ctx->loop_break_label;
            int saved_continue = ctx->loop_continue_label;
            ctx->loop_break_label    = loop_end;
            ctx->loop_continue_label = loop_inc;
            ctx->in_loop++;

            /* loop start label। */
            tac_emit_label(func, loop_start);

            /* list length রাখার জন্য len temp তৈরি করি। */
            int len_t = tac_new_temp(prog);
            TACOperand len = tac_operand_temp(len_t, TYPE_NUMBER);

            /* প্রথমে ভুলভাবে __list_length call emit হয়েছিল (arg count 0)। */
            tac_emit(func, TAC_CALL, len,
                     tac_operand_func("__list_length"),
                     tac_operand_int(0));

            /* সর্বশেষ ভুল CALL-টিকে dead mark করি। */
            func->last->is_dead = true;

            /* সঠিক call sequence: আগে list param push করি। */
            tac_emit(func, TAC_PARAM, tac_operand_none(), list, tac_operand_none());
            /* তারপর __list_length(list) call দিয়ে len temp-এ length রাখি। */
            tac_emit(func, TAC_CALL, tac_operand_temp(len_t, TYPE_NUMBER),
                     tac_operand_func("__list_length"),
                     tac_operand_int(1));

            /* loop condition: idx >= len ? */
            int cond_t = tac_new_temp(prog);
            TACOperand cond = tac_operand_temp(cond_t, TYPE_FLAG);
            tac_emit(func, TAC_GTE, cond,
                     tac_operand_temp(idx_t, TYPE_NUMBER),
                     tac_operand_temp(len_t, TYPE_NUMBER));
            /* condition true হলে loop_end-এ exit। */
            tac_emit_if_goto(func, cond, loop_end);

            /* iterator variable declare করি (বর্তমানে TYPE_NUMBER ধরে)। */
            TACOperand item_var = tac_operand_var(node->data.for_each_stmt.iterator_name, TYPE_NUMBER);
            tac_emit(func, TAC_DECL, item_var, tac_operand_none(), tac_operand_none());
            /* list[idx] element একটি temp-এ আনছি। */
            int elem_t = tac_new_temp(prog);
            tac_emit(func, TAC_LIST_GET,
                     tac_operand_temp(elem_t, TYPE_NUMBER),
                     list,
                     tac_operand_temp(idx_t, TYPE_NUMBER));
            /* iterator variable-এ temp element assign করি। */
            tac_emit(func, TAC_ASSIGN,
                     tac_operand_var(node->data.for_each_stmt.iterator_name, TYPE_NUMBER),
                     tac_operand_temp(elem_t, TYPE_NUMBER),
                     tac_operand_none());

            /* for-each body generate করি। */
            ir_gen_node(ctx, node->data.for_each_stmt.body);

            /* increment label mark (continue এখানে jump করে)। */
            tac_emit_label(func, loop_inc);
            /* idx = idx + 1 */
            tac_emit(func, TAC_ADD,
                     tac_operand_temp(idx_t, TYPE_NUMBER),
                     tac_operand_temp(idx_t, TYPE_NUMBER),
                     tac_operand_int(1));
            /* পরের iteration-এর জন্য loop_start-এ ফিরে যাই। */
            tac_emit_goto(func, loop_start);
            /* loop end label mark। */
            tac_emit_label(func, loop_end);

            /* loop context restore করি। */
            ctx->in_loop--;
            ctx->loop_break_label    = saved_break;
            ctx->loop_continue_label = saved_continue;
            /* for-each case শেষ। */
            break;
        }

        /* ---- Function Declaration ---- */
        case AST_FUNC_DECL: {
            /* AST function decl থেকে নতুন TACFunction container তৈরি। */
            TACFunction *new_func = tac_function_create(
                node->data.func_decl.name,
                node->data.func_decl.return_type);

            /* function parameters metadata কপি করি। */
            ASTNodeList *params = node->data.func_decl.params;
            if (params && params->count > 0) {
                /* param count সেট করি। */
                new_func->param_count = (int)params->count;
                /* param নামের জন্য dynamic array allocate। */
                new_func->param_names = calloc(params->count, sizeof(char *));
                /* param type-এর জন্য dynamic array allocate। */
                new_func->param_types = calloc(params->count, sizeof(DataType));
                /* প্রতিটি param নাম/টাইপ কপি করি। */
                for (size_t i = 0; i < params->count; i++) {
                    ASTNode *p = params->nodes[i];
                    new_func->param_names[i] = strdup(p->data.param_decl.name);
                    new_func->param_types[i] = p->data.param_decl.param_type;
                }
            }

            /* function শুরু marker emit। */
            tac_emit(new_func, TAC_FUNC_BEGIN,
                     tac_operand_func(node->data.func_decl.name),
                     tac_operand_none(), tac_operand_none());

            /* context switch: এখন থেকে emit new_func-এ হবে। */
            TACFunction *saved_func = ctx->current_func;
            int saved_in_func = ctx->in_function;
            ctx->current_func = new_func;
            ctx->in_function = 1;

            /* function body generate করি। */
            ir_gen_node(ctx, node->data.func_decl.body);

            /* function শেষ marker emit। */
            tac_emit(new_func, TAC_FUNC_END,
                     tac_operand_none(), tac_operand_none(), tac_operand_none());

            /* আগের context restore করি। */
            ctx->current_func = saved_func;
            ctx->in_function = saved_in_func;

            /* new function-কে program function list-এ যোগ করি। */
            tac_program_add_function(prog, new_func);
            /* function declaration case শেষ। */
            break;
        }

        /* ---- Return ---- */
        case AST_RETURN: {
            /* return value থাকলে value evaluate করে RETURN emit। */
            if (node->data.return_stmt.value) {
                TACOperand val = ir_gen_expression(ctx, node->data.return_stmt.value);
                tac_emit(func, TAC_RETURN, tac_operand_none(), val, tac_operand_none());
            } else {
                /* void/plain return হলে empty RETURN emit। */
                tac_emit(func, TAC_RETURN, tac_operand_none(),
                         tac_operand_none(), tac_operand_none());
            }
            /* return case শেষ। */
            break;
        }

        /* ---- Break / Continue ---- */
        /* loop-এর ভিতরে থাকলেই break বৈধ; তখন break label-এ goto। */
        case AST_BREAK:
            if (ctx->in_loop) {
                tac_emit_goto(func, ctx->loop_break_label);
            }
            break;

        /* loop-এর ভিতরে থাকলেই continue বৈধ; continue label-এ goto। */
        case AST_CONTINUE:
            if (ctx->in_loop) {
                tac_emit_goto(func, ctx->loop_continue_label);
            }
            break;

        /* ---- Secure Zone ---- */
        /* secure zone-কে begin/end marker + scope begin/end দিয়ে ঘিরে দিই। */
        case AST_SECURE_ZONE:
            tac_emit(func, TAC_SECURE_BEGIN,
                     tac_operand_none(), tac_operand_none(), tac_operand_none());
            tac_emit(func, TAC_SCOPE_BEGIN,
                     tac_operand_none(), tac_operand_none(), tac_operand_none());
            /* secure zone body generate করি। */
            ir_gen_node(ctx, node->data.secure_zone.body);
            tac_emit(func, TAC_SCOPE_END,
                     tac_operand_none(), tac_operand_none(), tac_operand_none());
            tac_emit(func, TAC_SECURE_END,
                     tac_operand_none(), tac_operand_none(), tac_operand_none());
            break;

        /* ---- Expression Statement ---- */
        /* expression statement-এর side effect থাকলে সেটাই লক্ষ্য; result discard। */
        case AST_EXPR_STMT:
            ir_gen_expression(ctx, node->data.expr_stmt.expr);
            break;

        /* ---- Block ---- */
        case AST_BLOCK: {
            /* lexical block শুরু marker emit। */
            tac_emit(func, TAC_SCOPE_BEGIN,
                     tac_operand_none(), tac_operand_none(), tac_operand_none());
            /* block statements list নেই। */
            ASTNodeList *stmts = node->data.block.statements;
            /* statements থাকলে একে একে statement lowering করি। */
            if (stmts) {
                for (size_t i = 0; i < stmts->count; i++) {
                    ir_gen_statement(ctx, stmts->nodes[i]);
                }
            }
            /* lexical block শেষ marker emit। */
            tac_emit(func, TAC_SCOPE_END,
                     tac_operand_none(), tac_operand_none(), tac_operand_none());
            /* block case শেষ। */
            break;
        }

        default:
            /* unsupported statement টাইপ হলে warning দিই। */
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
    /* নিরাপত্তা: node null হলে dispatch করার কিছু নেই। */
    if (!node) return;

    /* AST root যদি program হয়, তাহলে তার statement list iterate করি। */
    if (node->type == AST_PROGRAM) {
        /* program-এর top-level statements list নিই। */
        ASTNodeList *stmts = node->data.program.statements;
        /* list থাকলে প্রতিটি statement আলাদাভাবে lower করি। */
        if (stmts) {
            /* top-level statement গুলো sequence অনুযায়ী emit হয়। */
            for (size_t i = 0; i < stmts->count; i++) {
                /* প্রতিটি statement node কে statement lowerer-এ পাঠাই। */
                ir_gen_statement(ctx, stmts->nodes[i]);
            }
        }
    } else {
        /* program node না হলে single statement/block হিসেবে handle করি। */
        ir_gen_statement(ctx, node);
    }
}

/* ============================================================================
 * PUBLIC ENTRY POINT
 * ============================================================================
 */

TACProgram *ir_generate(ASTNode *ast) {
    /* entry guard: input AST না থাকলে IR generate করা সম্ভব নয়। */
    if (!ast) return NULL;

    /* IR generation context stack-এ তৈরি করি। */
    IRGenContext ctx;
    /* context পুরোটা zero-init করে deterministic state নিশ্চিত করি। */
    memset(&ctx, 0, sizeof(ctx));
    /* TAC program container allocate ও initialize করি। */
    ctx.program = tac_program_create();
    /* top-level code emit হবে implicit main function-এ। */
    ctx.current_func = ctx.program->main_func;

    /* AST root থেকে recursive lowering শুরু করি। */
    ir_gen_node(&ctx, ast);

    /* final statistics update: total instruction count precompute করি। */
    ctx.program->total_instructions = ir_count_total(ctx.program);

    /* generated TAC program caller-এ ফিরিয়ে দিই। */
    return ctx.program;
}
