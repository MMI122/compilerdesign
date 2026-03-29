/*
 * NatureLang Compiler
 * Copyright (c) 2026
 *
 * IR-based Code Generator Implementation
 *
 * Translates optimized Three-Address Code (TAC) IR into ANSI C.
 * This completes the classic textbook compiler pipeline:
 *
 *   Source → Lexer → Parser → AST → Semantic → IR → Optimize → Codegen → C
 *
 * Each TAC instruction maps directly to one or a few C statements.
 */
/*In ir_codegen.c:15, this macro is set before system headers so libc exposes POSIX declarations consistently. The value 200809L specifically targets the 2008 POSIX standard revision.

Why it matters here:

It makes functions like strdup available in a standards-clean way on many systems.
It avoids implicit declaration warnings/errors under strict C flags.
It keeps behavior predictable across different libc implementations.
*/
#define _POSIX_C_SOURCE 200809L
#include "ir_codegen.h"
#include "ir.h"
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ============================================================================
 * INTERNAL CONTEXT
 * ============================================================================
 */
typedef struct {
    char *buf;
    size_t size;
    size_t cap;
    int indent;
    int indent_size;
    int emit_comments;
    int error_count;
    char error_message[1024];

    /* Track features used */
    int needs_input_buffer;
    int needs_math;
    int needs_list;

    /* Variable type table: track declared types */
    #define MAX_VAR_TYPES 4096
    struct { char name[256]; DataType type; } var_types[MAX_VAR_TYPES];
    int var_type_count;

    /* Temporary type table */
    #define MAX_TEMP_TYPES 4096
    DataType temp_types[MAX_TEMP_TYPES];

    /* Function return type table */
    #define MAX_FUNC_TYPES 256
    struct { char name[256]; DataType ret_type; } func_types[MAX_FUNC_TYPES];
    int func_type_count;
} IRCGCtx;

static void ctx_init(IRCGCtx *ctx, int indent_size) {
    /* প্রাথমিক output buffer capacity fixed base value দিয়ে শুরু করি। 
    8192 = 8 KiB, ছোট/মাঝারি generated C code শুরুতেই ধরে ফেলার জন্য practical default।
    */
    ctx->cap = 8192;
    /* generated C code জমার জন্য heap buffer allocate। */
    ctx->buf = malloc(ctx->cap);
    /* শুরুতে কোনো লেখা নেই, তাই size = 0। */
    ctx->size = 0;
    /* buffer-কে empty C-string বানাতে NUL বসাই। */
    ctx->buf[0] = '\0';
    /* indentation depth zero থেকে শুরু। */
    ctx->indent = 0;
    /* caller-configured indent width সংরক্ষণ। */
    ctx->indent_size = indent_size;
    /* default-এ inline debug comments emit বন্ধ। */
    ctx->emit_comments = 0;
    /* codegen error counter শুরুতে শূন্য। */
    ctx->error_count = 0;
    /* feature flags শুরুতে false/0: scan phase এগুলো set করবে। */
    ctx->needs_input_buffer = 0;
    ctx->needs_math = 0;
    ctx->needs_list = 0;
    /* variable type table empty ধরে count reset। */
    ctx->var_type_count = 0;
    /* temp type table clean করি যেন TYPE_UNKNOWN/0 দিয়ে শুরু হয়। */
    memset(ctx->temp_types, 0, sizeof(ctx->temp_types));
    /* function return type registryও empty অবস্থায় শুরু। */
    ctx->func_type_count = 0;
}

static void ctx_register_func(IRCGCtx *ctx, const char *name, DataType ret_type) {
    /* আগে থেকে function entry থাকলে সেটি update করব। */
    for (int i = 0; i < ctx->func_type_count; i++) {
        if (strcmp(ctx->func_types[i].name, name) == 0) {
            /* একই function name পেলে return type overwrite/update। */
            ctx->func_types[i].ret_type = ret_type;
            /* update সম্পন্ন, নতুন entry দরকার নেই। */
            return;
        }
    }
    /* নতুন function হলে table capacity থাকলে append করি। */
    if (ctx->func_type_count < MAX_FUNC_TYPES) {
        /* bounded copy: name buffer overflow এড়াতে size-1 পর্যন্ত কপি। */
        strncpy(ctx->func_types[ctx->func_type_count].name, name,
                sizeof(ctx->func_types[0].name) - 1);
        /* function-এর declared return type সংরক্ষণ। */
        ctx->func_types[ctx->func_type_count].ret_type = ret_type;
        /* registry size এক ধাপ বাড়াই। */
        ctx->func_type_count++;
    }
}

static DataType ctx_lookup_func_ret(IRCGCtx *ctx, const char *name) {
    /* function return type table linear search। */
    for (int i = 0; i < ctx->func_type_count; i++) {
        if (strcmp(ctx->func_types[i].name, name) == 0) {
            /* match পেলে stored return type ফেরত। */
            return ctx->func_types[i].ret_type;
        }
    }
    /* না পেলে unknown type দিয়ে caller-কে fallback signal দিই। */
    return TYPE_UNKNOWN;
}

static void ctx_register_var(IRCGCtx *ctx, const char *name, DataType type) {
    /* Update if already present */
    /* variable আগে থাকলে redeclare না করে type update করি। */
    for (int i = 0; i < ctx->var_type_count; i++) {
        if (strcmp(ctx->var_types[i].name, name) == 0) {
            /* existing entry type refresh। */
            ctx->var_types[i].type = type;
            return;
        }
    }
    /* নতুন variable হলে capacity থাকলে table-এ append। */
    if (ctx->var_type_count < MAX_VAR_TYPES) {
        /* bounded copy দিয়ে name store করি। */
        strncpy(ctx->var_types[ctx->var_type_count].name, name,
                sizeof(ctx->var_types[0].name) - 1);
        /* resolved/declared variable type save। */
        ctx->var_types[ctx->var_type_count].type = type;
        /* table size increment। */
        ctx->var_type_count++;
    }
}

static DataType ctx_lookup_var_type(IRCGCtx *ctx, const char *name) {
    /* registered variable type table-এ linear search চালাই। */
    for (int i = 0; i < ctx->var_type_count; i++) {
        if (strcmp(ctx->var_types[i].name, name) == 0) {
            /* নাম match হলে stored DataType caller-কে ফেরত দিই। */
            return ctx->var_types[i].type;
        }
    }
    /* table-এ না পেলে unknown type ফেরত (fallback signal)। */
    return TYPE_UNKNOWN;
}

/* Resolve the effective type of an operand using context tables */
static DataType resolve_type(IRCGCtx *ctx, TACOperand *op) {
    /* If the operand already has a known type, use it */
    /* operand-এর type যদি আগেই concrete হয়, context lookup ছাড়াই সেটিই authoritative। */
    if (op->data_type != TYPE_UNKNOWN && op->data_type != TYPE_NUMBER) {
        return op->data_type;
    }
    /* operand kind অনুযায়ী effective type infer/resolve করি। */
    switch (op->kind) {
        case OPERAND_STRING:
            /* string literal/value সর্বদা TYPE_TEXT। */
            return TYPE_TEXT;
        case OPERAND_FLOAT:
            /* floating operand হলে decimal type। */
            return TYPE_DECIMAL;
        case OPERAND_BOOL:
            /* boolean operand হলে flag type। */
            return TYPE_FLAG;
        case OPERAND_INT:
            /* integer operand হলে number type। */
            return TYPE_NUMBER;
        case OPERAND_VAR:
            /* named variable-এর জন্য variable table থেকে type খুঁজি। */
            if (op->val.name) {
                DataType t = ctx_lookup_var_type(ctx, op->val.name);
                /* table-এ পাওয়া গেলে inferred type ব্যবহার করি। */
                if (t != TYPE_UNKNOWN) return t;
            }
            /* না পেলে operand metadata-তে থাকা type-এ fallback। */
            return op->data_type;
        case OPERAND_TEMP:
            /* temp id valid range-এ হলে temp type table consult করি। */
            if (op->val.temp_id >= 0 && op->val.temp_id < MAX_TEMP_TYPES) {
                DataType t = ctx->temp_types[op->val.temp_id];
                /* recorded temp type থাকলে সেটি return। */
                if (t != TYPE_UNKNOWN) return t;
            }
            /* temp table-এ অজানা হলে operand-local type fallback। */
            return op->data_type;
        default:
            /* unsupported/none operand kind হলে safest fallback: existing metadata। */
            return op->data_type;
    }
}

/* Record the type of a result operand.
 * For variables, only record if not already declared (DECL types are authoritative). */
static void record_result_type(IRCGCtx *ctx, TACOperand *result, DataType type) {
    /* result যদি temp হয় এবং id valid range-এ থাকে, temp type table-এ সরাসরি record করি। */
    if (result->kind == OPERAND_TEMP && result->val.temp_id >= 0 &&
        result->val.temp_id < MAX_TEMP_TYPES) {
        /* temp id slot-এ inferred/known type লিখে রাখি ভবিষ্যৎ resolution-এর জন্য। */
        ctx->temp_types[result->val.temp_id] = type;
    } else if (result->kind == OPERAND_VAR && result->val.name) {
        /* Only set if not already declared */
        /* variable already declared কিনা দেখে authoritative DECL type preserve করি। */
        DataType existing = ctx_lookup_var_type(ctx, result->val.name);
        if (existing == TYPE_UNKNOWN) {
            /* আগে declaration না থাকলে এখন inferred type registry-তে যোগ করি। */
            ctx_register_var(ctx, result->val.name, type);
        }
    }
}

static void ctx_free(IRCGCtx *ctx) {
    /* context output buffer lifecycle শেষ হলে heap memory মুক্ত করি। */
    free(ctx->buf);
}

static void ensure_cap(IRCGCtx *ctx, size_t n) {
    /* append করার পর যদি capacity ছাড়িয়ে যায়, buffer grow করতে হবে। */
    if (ctx->size + n >= ctx->cap) {
        /* growth strategy: প্রয়োজনীয় আকারের দ্বিগুণ নিয়ে realloc frequency কমাই। */
        ctx->cap = (ctx->size + n) * 2;
        /* resized buffer pointer update। */
        ctx->buf = realloc(ctx->buf, ctx->cap);
    }
}

static void emit(IRCGCtx *ctx, const char *fmt, ...) {
    /* variadic formatting-এর জন্য argument list handler। */
    va_list args;
    /* ছোট staging buffer-এ formatted text তৈরি করি। */
    char tmp[4096];
    /* variadic arguments capture শুরু। */
    va_start(args, fmt);
    /* format string অনুযায়ী tmp-তে render করি; len = লেখা বাইট সংখ্যা। */
    int len = vsnprintf(tmp, sizeof(tmp), fmt, args);
    /* variadic processing শেষ। */
    va_end(args);
    /* valid output থাকলেই append path-এ যাই। */
    if (len > 0) {
        /* মূল buffer-এ len+NUL বসানোর জন্য যথেষ্ট capacity নিশ্চিত করি। */
        ensure_cap(ctx, (size_t)len + 1);
        /* formatted bytes staging buffer থেকে main buffer-এ কপি। */
        memcpy(ctx->buf + ctx->size, tmp, (size_t)len);
        /* current size cursor আগাই। */
        ctx->size += (size_t)len;
        /* buffer-কে valid C-string রাখতে শেষে NUL বসাই। */
        ctx->buf[ctx->size] = '\0';
    }
}
/*
ir_codegen.c:267
ctx থেকে current indentation state নেয়।

ir_codegen.c:269
sp = ctx->indent * ctx->indent_size
মানে:

ctx->indent = nested block level (যেমন 0, 1, 2, ...)
ctx->indent_size = প্রতি level এ কয়টা space (যেমন 4)
তাই effective leading space = level × width.

ir_codegen.c:271
for loop sp বার চালিয়ে প্রতিবার " " emit করে।
ফলাফল: line শুরুতে যতটা indentation দরকার ঠিক ততটাই বসে।
*/

static void emit_indent(IRCGCtx *ctx) {
    /* effective leading spaces = indent level × indent width। */
    int sp = ctx->indent * ctx->indent_size;
    /* computed space count অনুযায়ী single-space emit করি। */
    for (int i = 0; i < sp; i++) emit(ctx, " ");
}

static void emit_line(IRCGCtx *ctx, const char *fmt, ...) {
    /* line formatting-এর জন্য local va_list। */
    va_list args;
    /* formatted line-এর temporary staging buffer। */
    char tmp[4096];
    /* line শুরুতে indentation emit করি। */
    emit_indent(ctx);
    /* variadic arguments capture শুরু। */
    va_start(args, fmt);
    /* fmt অনুযায়ী tmp-তে line body render করি। */
    int len = vsnprintf(tmp, sizeof(tmp), fmt, args);
    /* variadic processing শেষ। */
    va_end(args);
    /* output থাকলে line body + newline append করি। */
    if (len > 0) {
        /* body + newline + NUL এর জন্য capacity ensure। */
        ensure_cap(ctx, (size_t)len + 2);
        /* rendered body main buffer-এ কপি। */
        memcpy(ctx->buf + ctx->size, tmp, (size_t)len);
        /* size cursor body length অনুযায়ী আপডেট। */
        ctx->size += (size_t)len;
        /* line terminator newline append। */
        ctx->buf[ctx->size++] = '\n';
        /* C-string invariants বজায় রাখতে trailing NUL। */
        ctx->buf[ctx->size] = '\0';
    }
}

/* ============================================================================
 * OPERAND → C EXPRESSION
 * ============================================================================
 */
static void emit_operand(IRCGCtx *ctx, TACOperand *op) {
    /* operand kind অনুযায়ী C expression/text representation emit করি। */
    switch (op->kind) {
        case OPERAND_TEMP:
            /* compiler temp operand কে generated symbol _t<id> আকারে লিখি। */
            emit(ctx, "_t%d", op->val.temp_id);
            break;
        case OPERAND_VAR:
            /* Replace spaces with underscores for multi-word NatureLang names */
            /* named variable থাকলে character-by-character sanitize করে emit করি। */
            if (op->val.name) {
                const char *p = op->val.name;
                /* space চরিত্রকে '_' এ map করি, বাকিগুলো keep it separate just */
                while (*p) {
                    emit(ctx, "%c", *p == ' ' ? '_' : *p);
                    p++;
                }
            }
            break;
        case OPERAND_INT:
            /* integer literal C long long suffix সহ emit। */
            emit(ctx, "%lldLL", op->val.int_val);
            break;
        case OPERAND_FLOAT:
            /* floating literal compact %g format-এ emit। */
            emit(ctx, "%g", op->val.float_val);
            break;
        case OPERAND_STRING:
            /* string literal শুরুতে opening quote বসাই। */
            emit(ctx, "\"");
            if (op->val.str_val) {
                const char *s = op->val.str_val;
                /* C string safety-এর জন্য special char escape করে লিখি। */
                while (*s) {
                    switch (*s) {
                        /*
                        " হলে \" emit করে
                        \ হলে \\ emit করে
                        newline হলে \n
                        tab হলে \t
                        carriage return হলে \r
                        অন্য character হলে as-is emit
                        */
                        /* quote escape না করলে generated C ভেঙে যাবে। */
                        case '"':  emit(ctx, "\\\""); break;
                        /* backslash নিজেকেও escape করতে হয়। */
                        case '\\': emit(ctx, "\\\\"); break;
                        /* control characters visible escape sequence-এ map। */
                        case '\n': emit(ctx, "\\n"); break;
                        case '\t': emit(ctx, "\\t"); break;
                        case '\r': emit(ctx, "\\r"); break;
                        /* সাধারণ character direct emit। */
                        default:   emit(ctx, "%c", *s); break;
                    }
                    s++;
                }
            }
            /* string literal শেষে closing quote বসাই। */
            emit(ctx, "\"");
            break;
        case OPERAND_BOOL:
            /* boolean operand-কে C int truth value (0/1) হিসেবে emit করি। */
            emit(ctx, "%d", op->val.bool_val ? 1 : 0);
            break;
        case OPERAND_FUNC:
            /* function symbol-ও variable name-এর মতো sanitize করে emit করি। */
            if (op->val.name) {
                const char *p = op->val.name;
                while (*p) {
                    emit(ctx, "%c", *p == ' ' ? '_' : *p);
                    p++;
                }
            }
            break;
        case OPERAND_LABEL:
            /* label operand-কে L<id> form-এ emit (goto target style)। */
            emit(ctx, "L%d", op->val.label_id);
            break;
        case OPERAND_NONE:
            /* empty operand: কিছু emit করার নেই। */
            break;
    }
}

/* Get C type string for a DataType */
static const char *type_to_c(DataType dt) {
    switch (dt) {
        case TYPE_NUMBER:  return "long long";
        case TYPE_DECIMAL: return "double";
        case TYPE_TEXT:    return "char*";
        case TYPE_FLAG:    return "int";
        case TYPE_LIST:    return "NLList*";
        case TYPE_NOTHING: return "void";
        default:           return "long long";
    }
}

/* ============================================================================
 * FIRST PASS: scan IR for features used (input, math, lists)
 * ============================================================================
 */
static void scan_features(IRCGCtx *ctx, TACFunction *func) {
    /* function-এর TAC instruction list একবার স্ক্যান করে runtime/header প্রয়োজন নির্ধারণ করি। */
    for (TACInstr *i = func->first; i; i = i->next) {
        /* dead instruction feature detection-এ প্রাসঙ্গিক নয়, তাই skip। */
        if (i->is_dead) continue;
        /* opcode দেখে কোন runtime সহায়তা দরকার তা flag করি। */
        switch (i->opcode) {
            case TAC_ASK: case TAC_READ:
                /* input-reading instruction থাকলে shared input buffer প্রয়োজন। */
                ctx->needs_input_buffer = 1;
                break;
            case TAC_POW:
                /* pow ব্যবহার হলে generated C-তে math header/function লাগবে। */
                ctx->needs_math = 1;
                break;
            case TAC_LIST_CREATE: case TAC_LIST_APPEND:
            case TAC_LIST_GET: case TAC_LIST_SET:
                /* list op পাওয়া গেলে list runtime support include করতে হবে। */
                ctx->needs_list = 1;
                break;
            default:
                /* এই opcode-গুলো feature flag পরিবর্তন করে না। */
                break;
        }
    }
}

/* ============================================================================
 * EMIT HEADERS
 * ============================================================================
 */
static void emit_headers(IRCGCtx *ctx) {
    /* generated C file header comment block শুরু। */
    emit_line(ctx, "/*");
    emit_line(ctx, " * Generated by NatureLang Compiler (IR pipeline)");
    emit_line(ctx, " * Do not edit this file directly.");
    emit_line(ctx, " */");
    /* readability-এর জন্য এক লাইনের ফাঁকা স্পেস। */
    emit(ctx, "\n");
    /* feature macro এবং core standard headers emit করি। */
    emit_line(ctx, "#define _POSIX_C_SOURCE 200809L");
    emit_line(ctx, "#include <stdio.h>");
    emit_line(ctx, "#include <stdlib.h>");
    emit_line(ctx, "#include <string.h>");
    emit_line(ctx, "#include <stdbool.h>");
    /* pow() দরকার হলে তবেই math.h include করি (feature-driven include)। */
    if (ctx->needs_math) {
        emit_line(ctx, "#include <math.h>");
    }
    /* runtime helper API header সবসময় include। */
    emit_line(ctx, "#include \"naturelang_runtime.h\"");
    emit(ctx, "\n");

    /* input opcode থাকলে shared input buffer declaration emit। */
    if (ctx->needs_input_buffer) {
        emit_line(ctx, "static char _nl_input_buffer[4096];");
        emit(ctx, "\n");
    }
}

/* ============================================================================
 * EMIT TEMP DECLARATIONS
 *
 * Scan a function's instructions and declare all temporaries at the top.
 * ============================================================================
 */
static void emit_temp_declarations(IRCGCtx *ctx, TACFunction *func) {
    /* First: do a pre-pass to compute types for all temps and vars */
    /* pass-1: opcode semantics দেখে result type table populate করি। */
    for (TACInstr *i = func->first; i; i = i->next) {
        /* dead instruction declaration analysis-এ ধরা হয় না। */
        if (i->is_dead) continue;
        /* instruction class অনুযায়ী result type infer/record। */
        switch (i->opcode) {
            case TAC_LOAD_INT:
                /* integer load → number temp/var type। */
                record_result_type(ctx, &i->result, TYPE_NUMBER);
                break;
            case TAC_LOAD_FLOAT:
                /* float load → decimal type। */
                record_result_type(ctx, &i->result, TYPE_DECIMAL);
                break;
            case TAC_LOAD_STRING:
                /* string load → text type। */
                record_result_type(ctx, &i->result, TYPE_TEXT);
                break;
            case TAC_LOAD_BOOL:
                /* bool load → flag type। */
                record_result_type(ctx, &i->result, TYPE_FLAG);
                break;
            case TAC_DECL:
                /* explicit declaration পাওয়া গেলে variable table authoritative update। */
                if (i->result.kind == OPERAND_VAR && i->result.val.name)
                    ctx_register_var(ctx, i->result.val.name, i->result.data_type);
                break;
            case TAC_CONCAT:
                /* concat result সবসময় text। */
                record_result_type(ctx, &i->result, TYPE_TEXT);
                break;
            case TAC_ASK: case TAC_READ:
                /* ask/read এর result runtime string input → text। */
                record_result_type(ctx, &i->result, TYPE_TEXT);
                break;
            case TAC_EQ: case TAC_NEQ: case TAC_LT: case TAC_GT:
            case TAC_LTE: case TAC_GTE: case TAC_AND: case TAC_OR:
            case TAC_NOT: case TAC_BETWEEN:
                /* comparison/logical পরিবার → boolean flag result। */
                record_result_type(ctx, &i->result, TYPE_FLAG);
                break;
            default:
                /* এই pass-এ unhandled opcode থেকে type update করি না। */
                break;
        }
    }
    /* Second pass: also propagate via assigns */
    /* pass-2: assignment chain ধরে source type result-এ propagate করি। */
    for (TACInstr *i = func->first; i; i = i->next) {
        if (i->is_dead) continue;
        if (i->opcode == TAC_ASSIGN) {
            /* arg1-এর effective type resolve করি। */
            DataType src = resolve_type(ctx, &i->arg1);
            /* source type জানা থাকলে result operand-এ record। */
            if (src != TYPE_UNKNOWN)
                record_result_type(ctx, &i->result, src);
        }
    }

    /* Collect all temp IDs used and their resolved types */
    /* local scratch array: unique temp id ও তাদের chosen type জমাতে। */
    #define MAX_TEMPS 4096
    int seen[MAX_TEMPS];
    DataType types[MAX_TEMPS];
    /* কতগুলো unique temp পাওয়া গেছে। */
    int count = 0;

    /* instruction scan করে result/arg1/arg2/arg3 সব operand থেকে temp collect। */
    for (TACInstr *i = func->first; i; i = i->next) {
        if (i->is_dead) continue;
        TACOperand *ops[] = { &i->result, &i->arg1, &i->arg2, &i->arg3 };
        /* ৪টি operand slot একে একে পরীক্ষা। */
        for (int j = 0; j < 4; j++) {
            if (ops[j]->kind == OPERAND_TEMP) {
                int tid = ops[j]->val.temp_id;
                /* tid আগে collect হয়েছে কিনা দেখি (dedup)। */
                int found = 0;
                for (int k = 0; k < count; k++) {
                    if (seen[k] == tid) { found = 1; break; }
                }
                if (!found && count < MAX_TEMPS) {
                    /* নতুন temp id তালিকায় যোগ করি। */
                    seen[count] = tid;
                    /* Use resolved type from context */
                    /* context table-এ type থাকলে সেটি priority পায়। */
                    if (tid >= 0 && tid < MAX_TEMP_TYPES && ctx->temp_types[tid] != TYPE_UNKNOWN) {
                        types[count] = ctx->temp_types[tid];
                    } else {
                        /* নাহলে operand metadata fallback type ব্যবহার। */
                        types[count] = ops[j]->data_type;
                    }
                    /* unique temp count বাড়াই। */
                    count++;
                }
            }
        }
    }

    /* unique temp পাওয়া গেলে function top-এ declaration emit করি। */
    if (count > 0) {
        /* option enable থাকলে declaration section comment emit। */
        if (ctx->emit_comments) {
            emit_indent(ctx);
            emit(ctx, "/* temporaries */\n");
        }
        /* প্রতিটি temp-এর জন্য inferred type অনুযায়ী declaration emit। */
        for (int i = 0; i < count; i++) {
            emit_indent(ctx);
            DataType dt = types[i];
            if (dt == TYPE_TEXT) {
                /* text temp pointer হওয়ায় NULL init করা নিরাপদ। */
                emit(ctx, "char* _t%d = NULL;\n", seen[i]);
            } else {
                /* numeric/flag/list ইত্যাদি type default 0 init। */
                emit(ctx, "%s _t%d = 0;\n", type_to_c(dt), seen[i]);
            }
        }
        /* declaration block শেষে একটি ফাঁকা লাইন। */
        emit(ctx, "\n");
    }
    /* local helper macro scope close। */
    #undef MAX_TEMPS
}

/* ============================================================================
 * EMIT A PRINTF FOR A DISPLAY INSTRUCTION
 *
 * Determine the type from the operand and use the appropriate format.
 * ============================================================================
 */
static void emit_display(IRCGCtx *ctx, TACOperand *val) {
    /* operand-এর effective data type context/type-table দেখে resolve করি। */
    DataType effective_type = resolve_type(ctx, val);
    /* display statement line শুরুতে current block indentation বসাই। */
    emit_indent(ctx);
    /* resolved type অনুযায়ী সঠিক printf format/selective rendering বেছে নিই। */
    switch (effective_type) {
        case TYPE_NUMBER:
            /* integer/number type হলে long long হিসেবে print করি। */
            emit(ctx, "printf(\"%%lld\\n\", (long long)");
            /* value operand-কে C expression হিসেবে emit করি। */
            emit_operand(ctx, val);
            /* printf call close করে statement terminate। */
            emit(ctx, ");\n");
            break;
        case TYPE_DECIMAL:
            /* decimal/floating type হলে %g format ব্যবহার করি। */
            emit(ctx, "printf(\"%%g\\n\", (double)");
            /* operand expression inline বসাই। */
            emit_operand(ctx, val);
            /* generated C line শেষ করি। */
            emit(ctx, ");\n");
            break;
        case TYPE_TEXT:
            /* text/string value হলে %s formatter দিয়ে print। */
            emit(ctx, "printf(\"%%s\\n\", ");
            /* string operand emit করি (escaped literal/variable/temp)। */
            emit_operand(ctx, val);
            /* printf statement complete। */
            emit(ctx, ");\n");
            break;
        case TYPE_FLAG:
            /* boolean/flag type human-readable yes/no হিসেবে দেখাই। */
            emit(ctx, "printf(\"%%s\\n\", ");
            /* condition অংশে boolean operand বসাই। */
            emit_operand(ctx, val);
            /* ternary দিয়ে true->yes, false->no map করে line শেষ। */
            emit(ctx, " ? \"yes\" : \"no\");\n");
            break;
        default:
            /* Generic: try as number */
            /* fallback path: unknown type হলে number cast ধরে print করার চেষ্টা। */
            emit(ctx, "printf(\"%%lld\\n\", (long long)");
            /* unknown operandও generic expression হিসেবে emit। */
            emit_operand(ctx, val);
            /* fallback printf statement terminate। */
            emit(ctx, ");\n");
            break;
    }
}

/* ============================================================================
 * EMIT A SINGLE TAC INSTRUCTION AS C CODE
 * ============================================================================
 */

static void emit_instruction(IRCGCtx *ctx, TACInstr *instr) {
    /* null pointer বা optimizer-marked dead instruction হলে code emit করব না। */
    if (!instr || instr->is_dead) return;

    /* Record type information for result operands */
    /* opcode অনুযায়ী result operand-এর inferred type আগেই context table-এ record করি। */
    switch (instr->opcode) {
        case TAC_LOAD_INT:
            /* integer load result -> number type। */
            record_result_type(ctx, &instr->result, TYPE_NUMBER);
            break;
        case TAC_LOAD_FLOAT:
            /* float load result -> decimal type। */
            record_result_type(ctx, &instr->result, TYPE_DECIMAL);
            break;
        case TAC_LOAD_STRING:
            /* string load result -> text type। */
            record_result_type(ctx, &instr->result, TYPE_TEXT);
            break;
        case TAC_LOAD_BOOL:
            /* bool load result -> flag type। */
            record_result_type(ctx, &instr->result, TYPE_FLAG);
            break;
        case TAC_DECL:
            /* declaration instruction পেলে variable type table authoritative ভাবে update। */
            if (instr->result.kind == OPERAND_VAR && instr->result.val.name) {
                ctx_register_var(ctx, instr->result.val.name, instr->result.data_type);
            }
            break;
        case TAC_ASSIGN: {
            /* assignment source operand থেকে effective type resolve করি। */
            DataType src_type = resolve_type(ctx, &instr->arg1);
            /* source type জানা থাকলে assignment result-এ propagate করি। */
            if (src_type != TYPE_UNKNOWN)
                record_result_type(ctx, &instr->result, src_type);
            break;
        }
        case TAC_CONCAT:
            /* concat operation সবসময় text produce করে। */
            record_result_type(ctx, &instr->result, TYPE_TEXT);
            break;
        case TAC_ASK: case TAC_READ:
            /* input read-এর result string/text হিসাবে ধরি। */
            record_result_type(ctx, &instr->result, TYPE_TEXT);
            break;
        case TAC_EQ: case TAC_NEQ: case TAC_LT: case TAC_GT:
        case TAC_LTE: case TAC_GTE: case TAC_AND: case TAC_OR:
        case TAC_NOT: case TAC_BETWEEN:
            /* comparison/logical পরিবার boolean flag return করে। */
            record_result_type(ctx, &instr->result, TYPE_FLAG);
            break;
        case TAC_ADD: case TAC_SUB: case TAC_MUL: case TAC_DIV:
        case TAC_MOD: case TAC_NEG: case TAC_POW: {
            /* arithmetic result type: যেকোন operand decimal হলে decimal, নাহলে number। */
            DataType lt = resolve_type(ctx, &instr->arg1);
            DataType rt = resolve_type(ctx, &instr->arg2);
            DataType res = (lt == TYPE_DECIMAL || rt == TYPE_DECIMAL)
                           ? TYPE_DECIMAL : TYPE_NUMBER;
            record_result_type(ctx, &instr->result, res);
            break;
        }
        default:
            /* অন্য opcode এই pre-record pass-এ type update করে না। */
            break;
    }

    /* দ্বিতীয় switch-এ actual TAC -> C statement emission করা হয়। */
    switch (instr->opcode) {

        /* ---- Labels ---- */
        case TAC_LABEL:
            /* Un-indent labels */
            /* label block-level indentation ছাড়াই root column-এ emit করি। */
            emit(ctx, "L%d:;\n", instr->result.val.label_id);
            break;

        case TAC_GOTO:
            /* unconditional jump target label-এ goto emit। */
            emit_indent(ctx);
            emit(ctx, "goto L%d;\n", instr->result.val.label_id);
            break;

        case TAC_IF_GOTO:
            /* condition true হলে label-এ jump। */
            emit_indent(ctx);
            emit(ctx, "if (");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, ") goto L%d;\n", instr->result.val.label_id);
            break;

        case TAC_IF_FALSE_GOTO:
            /* condition false হলে label-এ jump। */
            emit_indent(ctx);
            emit(ctx, "if (!(");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, ")) goto L%d;\n", instr->result.val.label_id);
            break;

        /* ---- Loads ---- */
        case TAC_LOAD_INT:
        case TAC_LOAD_FLOAT:
        case TAC_LOAD_BOOL:
            /* scalar literal/value load: result = arg1; */
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = ");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, ";\n");
            break;

        case TAC_LOAD_STRING:
            /* string load-ও assignment pattern-এ emit। */
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = ");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, ";\n");
            break;

        /* ---- Assignment ---- */
        case TAC_ASSIGN:
            /* generic assignment translation। */
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = ");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, ";\n");
            break;

        /* ---- Binary arithmetic ---- */
        case TAC_ADD:
            /* addition expression emit। */
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = ");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, " + ");
            emit_operand(ctx, &instr->arg2);
            emit(ctx, ";\n");
            break;

        case TAC_SUB:
            /* subtraction expression emit। */
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = ");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, " - ");
            emit_operand(ctx, &instr->arg2);
            emit(ctx, ";\n");
            break;

        case TAC_MUL:
            /* multiplication expression emit। */
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = ");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, " * ");
            emit_operand(ctx, &instr->arg2);
            emit(ctx, ";\n");
            break;

        case TAC_DIV:
            /* division expression emit। */
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = ");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, " / ");
            emit_operand(ctx, &instr->arg2);
            emit(ctx, ";\n");
            break;

        case TAC_MOD:
            /* modulo expression emit (%% escape দরকার কারণ format string)। */
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = ");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, " %% ");
            emit_operand(ctx, &instr->arg2);
            emit(ctx, ";\n");
            break;

        case TAC_POW:
            /* pow() call ব্যবহারের জন্য math feature flag অন করি। */
            ctx->needs_math = 1;
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = pow(");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, ", ");
            emit_operand(ctx, &instr->arg2);
            emit(ctx, ");\n");
            break;

        /* ---- Unary ---- */
        case TAC_NEG:
            /* unary negation: result = -(arg1) */
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = -(");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, ");\n");
            break;

        case TAC_NOT:
            /* logical not: result = !(arg1) */
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = !(");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, ");\n");
            break;

        /* ---- Comparison ---- */
        case TAC_EQ:  case TAC_NEQ: case TAC_LT: case TAC_GT:
        case TAC_LTE: case TAC_GTE: {
            /* opcode থেকে C comparison operator string map করি। */
            const char *op_str = "==";
            switch (instr->opcode) {
                case TAC_EQ:  op_str = "=="; break;
                case TAC_NEQ: op_str = "!="; break;
                case TAC_LT:  op_str = "<";  break;
                case TAC_GT:  op_str = ">";  break;
                case TAC_LTE: op_str = "<="; break;
                case TAC_GTE: op_str = ">="; break;
                default: break;
            }
            /* result = (arg1 op arg2) form-এ boolean expression emit। */
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = (");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, " %s ", op_str);
            emit_operand(ctx, &instr->arg2);
            emit(ctx, ");\n");
            break;
        }

        /* ---- Logical ---- */
        case TAC_AND:
            /* logical AND expression emit। */
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = (");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, " && ");
            emit_operand(ctx, &instr->arg2);
            emit(ctx, ");\n");
            break;

        case TAC_OR:
            /* logical OR expression emit। */
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = (");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, " || ");
            emit_operand(ctx, &instr->arg2);
            emit(ctx, ");\n");
            break;

        /* ---- Concat ---- */
        case TAC_CONCAT:
            /* runtime helper nl_concat দিয়ে string concatenation emit। */
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = nl_concat(");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, ", ");
            emit_operand(ctx, &instr->arg2);
            emit(ctx, ");\n");
            break;

        /* ---- Between ---- */
        case TAC_BETWEEN:
            /* between check: low <= value <= high কে দুই comparison AND দিয়ে নামাই। */
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = ((");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, " >= ");
            emit_operand(ctx, &instr->arg2);
            emit(ctx, ") && (");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, " <= ");
            emit_operand(ctx, &instr->arg3);
            emit(ctx, "));\n");
            break;

        /* ---- Variable Declaration ---- */
        case TAC_DECL:
            /* declared datatype অনুযায়ী C variable declaration emit। */
            emit_indent(ctx);
            emit(ctx, "%s ", type_to_c(instr->result.data_type));
            emit_operand(ctx, &instr->result);
            /* Default initialization */
            /* type-specific safe default init দিই যাতে uninitialized use না হয়। */
            switch (instr->result.data_type) {
                case TYPE_NUMBER: case TYPE_DECIMAL: case TYPE_FLAG:
                    emit(ctx, " = 0");
                    break;
                case TYPE_TEXT:
                    emit(ctx, " = \"\"");
                    break;
                default:
                    break;
            }
            emit(ctx, ";\n");
            break;

        /* ---- I/O ---- */
        case TAC_DISPLAY:
            /* display emission helper-এ delegation। */
            emit_display(ctx, &instr->arg1);
            break;

        case TAC_ASK:
            /* ask/read path-এ shared input buffer লাগবে, তাই feature flag set। */
            ctx->needs_input_buffer = 1;
            /* Print prompt */
            /* optional prompt থাকলে আগে সেটি print করে flush করি। */
            if (instr->arg1.kind != OPERAND_NONE) {
                emit_indent(ctx);
                emit(ctx, "printf(\"%%s\", ");
                emit_operand(ctx, &instr->arg1);
                emit(ctx, "); fflush(stdout);\n");
            }
            /* Read input */
            /* stdin থেকে line নিয়ে newline trim করে strdup করে result-এ দিই। */
            emit_indent(ctx);
            emit(ctx, "fgets(_nl_input_buffer, sizeof(_nl_input_buffer), stdin); ");
            emit(ctx, "_nl_input_buffer[strcspn(_nl_input_buffer, \"\\n\")] = 0; ");
            emit_operand(ctx, &instr->result);
            emit(ctx, " = strdup(_nl_input_buffer);\n");
            break;

        case TAC_READ:
            /* prompt ছাড়া raw read path, ask-এর read অংশের সমতুল্য। */
            ctx->needs_input_buffer = 1;
            emit_indent(ctx);
            emit(ctx, "fgets(_nl_input_buffer, sizeof(_nl_input_buffer), stdin); ");
            emit(ctx, "_nl_input_buffer[strcspn(_nl_input_buffer, \"\\n\")] = 0; ");
            emit_operand(ctx, &instr->result);
            emit(ctx, " = strdup(_nl_input_buffer);\n");
            break;

        /* ---- Functions ---- */
        case TAC_FUNC_BEGIN:
            /* Handled at function level */
            /* function boundary instruction এখানে emit করা হয় না। */
            break;
        case TAC_FUNC_END:
            /* function boundary instruction এখানে emit করা হয় না। */
            break;

        case TAC_PARAM:
            /* Parameters are collected by the CALL handler,
               but in our linear IR they're emitted as standalone.
               We emit them as comments; the calling convention 
               passes args inline in the CALL expansion. */
            /* debug comments enable থাকলে PARAM instruction informational comment হিসেবে রাখি। */
            if (ctx->emit_comments) {
                emit_indent(ctx);
                emit(ctx, "/* param ");
                emit_operand(ctx, &instr->arg1);
                emit(ctx, " */\n");
            }
            break;

        case TAC_CALL: {
            /* Collect preceding PARAM instructions */
            /* We walk backward to find them */
            /* CALL-এর arg2-তে encoded argument count থাকে (int operand)। */
            int nargs = 0;
            if (instr->arg2.kind == OPERAND_INT) {
                nargs = (int)instr->arg2.val.int_val;
            }

            /* Check if function returns void (don't assign result) */
            /* return type void হলে assignment target বাদ দিতে হবে। */
            int is_void_call = 0;
            if (instr->arg1.kind == OPERAND_FUNC && instr->arg1.val.name) {
                DataType ret = ctx_lookup_func_ret(ctx, instr->arg1.val.name);
                if (ret == TYPE_NOTHING) is_void_call = 1;
            }

            emit_indent(ctx);
            /* non-void call এবং valid result operand থাকলে "result =" prefix emit। */
            if (instr->result.kind != OPERAND_NONE && !is_void_call) {
                emit_operand(ctx, &instr->result);
                emit(ctx, " = ");
            }

            /* Function name */
            /* internal list-length helper call-কে runtime symbol-এ remap করি। */
            if (instr->arg1.kind == OPERAND_FUNC &&
                instr->arg1.val.name &&
                strcmp(instr->arg1.val.name, "__list_length") == 0) {
                /* Special: list length */
                emit(ctx, "nl_list_length(");
            } else {
                /* generic function symbol emit করে call paren খুলি। */
                emit_operand(ctx, &instr->arg1);
                emit(ctx, "(");
            }

            /* Collect arguments from preceding PARAM instructions */
            if (nargs > 0) {
                /* Find the nargs PARAM instructions before this CALL */
                /* backward scan-এ param instruction collect করি। */
                TACInstr *params[64];
                int found = 0;
                TACInstr *scan = instr->prev;
                while (scan && found < nargs) {
                    /* dead PARAM/CALL artifacts skip। */
                    if (scan->is_dead) { scan = scan->prev; continue; }
                    if (scan->opcode == TAC_PARAM) {
                        params[found++] = scan;
                    }
                    scan = scan->prev;
                }
                /* Emit in reverse order (first param first) */
                /* backward collect হওয়ায় reverse iterate করে original order restore করি। */
                for (int i = found - 1; i >= 0; i--) {
                    if (i < found - 1) emit(ctx, ", ");
                    emit_operand(ctx, &params[i]->arg1);
                }
            }

            /* call expression close করে statement শেষ। */
            emit(ctx, ");\n");
            break;
        }

        case TAC_RETURN:
            /* return operand থাকলে return value সহ, নাহলে bare return emit। */
            emit_indent(ctx);
            if (instr->arg1.kind != OPERAND_NONE) {
                emit(ctx, "return ");
                emit_operand(ctx, &instr->arg1);
                emit(ctx, ";\n");
            } else {
                emit(ctx, "return;\n");
            }
            break;

        /* ---- Scope ---- */
        case TAC_SCOPE_BEGIN:
            /* lexical scope begin হলে '{' emit করে indent level বাড়াই। */
            emit_indent(ctx);
            emit(ctx, "{\n");
            ctx->indent++;
            break;

        case TAC_SCOPE_END:
            /* scope close-এর আগে indent কমিয়ে '}' সঠিক column-এ আনি। */
            ctx->indent--;
            emit_indent(ctx);
            emit(ctx, "}\n");
            break;

        case TAC_SECURE_BEGIN:
            /* secure zone markers শুধুই optional comments হিসেবে emit। */
            if (ctx->emit_comments) {
                emit_indent(ctx);
                emit(ctx, "/* BEGIN SECURE ZONE */\n");
            }
            break;

        case TAC_SECURE_END:
            /* secure zone end marker-ও optional comment। */
            if (ctx->emit_comments) {
                emit_indent(ctx);
                emit(ctx, "/* END SECURE ZONE */\n");
            }
            break;

        /* ---- List operations ---- */
        case TAC_LIST_CREATE:
            /* list runtime helper ব্যবহার হবে, তাই list feature flag set। */
            ctx->needs_list = 1;
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = nl_list_create(");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, ");\n");
            break;

        case TAC_LIST_APPEND:
            /* list append helper call emit। */
            emit_indent(ctx);
            emit(ctx, "nl_list_append(");
            emit_operand(ctx, &instr->result);
            emit(ctx, ", ");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, ");\n");
            break;

        case TAC_LIST_GET:
            /* list index access number-get helper দিয়ে emit। */
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = nl_list_get_num(");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, ", ");
            emit_operand(ctx, &instr->arg2);
            emit(ctx, ");\n");
            break;

        case TAC_LIST_SET:
            /* list set helper-এ list, index, value তিনটি argument পাঠাই। */
            emit_indent(ctx);
            emit(ctx, "nl_list_set(");
            emit_operand(ctx, &instr->result);
            emit(ctx, ", ");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, ", ");
            emit_operand(ctx, &instr->arg2);
            emit(ctx, ");\n");
            break;

        /* ---- No-ops ---- */
        case TAC_NOP:
        case TAC_BREAK:
        case TAC_CONTINUE:
            /* break/continue are lowered to gotos in IR */
            /* এই stage-এ explicit emit দরকার নেই। */
            break;

        default:
            /* unsupported opcode থাকলে generated code-এ diagnostic comment দিই। */
            emit_indent(ctx);
            emit(ctx, "/* unhandled TAC: %s */\n", tac_opcode_to_string(instr->opcode));
            break;
    }
}

/* ============================================================================
 * EMIT A USER FUNCTION
 * ============================================================================
 */
static void emit_function(IRCGCtx *ctx, TACFunction *func) {
    /* Return type */
    /* function signature-এর শুরুতে C return type emit করি। */
    emit(ctx, "%s ", type_to_c(func->return_type));

    /* Name */
    /* NatureLang function name sanitize করে (space -> underscore) emit। */
    const char *p = func->name;
    while (p && *p) {
        emit(ctx, "%c", *p == ' ' ? '_' : *p);
        p++;
    }

    /* Parameters */
    /* parameter list open parenthesis। */
    emit(ctx, "(");
    if (func->param_count > 0) {
        /* প্রতিটি parameter-এর type + sanitized name signature-এ লিখি। */
        for (int i = 0; i < func->param_count; i++) {
            /* দ্বিতীয় parameter থেকে comma separator যোগ করি। */
            if (i > 0) emit(ctx, ", ");
            /* parameter C type emit। */
            emit(ctx, "%s ", type_to_c(func->param_types[i]));
            /* parameter name-ও function নামের মতো sanitize করে লিখি। */
            const char *pn = func->param_names[i];
            while (pn && *pn) {
                emit(ctx, "%c", *pn == ' ' ? '_' : *pn);
                pn++;
            }
        }
    } else {
        /* no-parameter function হলে ANSI C style-এ explicit void। */
        emit(ctx, "void");
    }

    /* function body শুরু: closing signature + opening brace। */
    emit(ctx, ") {\n");
    /* body block-এ ঢোকার সাথে indentation depth ১ ধাপ বাড়াই। */
    ctx->indent++;

    /* Declare temporaries */
    /* function instructions-এ ব্যবহৃত temporaries top-এ declare করি। */
    emit_temp_declarations(ctx, func);

    /* Emit instructions (skip FUNC_BEGIN/FUNC_END) */
    /* linear TAC list iterate করে প্রতিটি instruction emit_instruction-এ পাঠাই। */
    for (TACInstr *instr = func->first; instr; instr = instr->next) {
        /* function boundary marker TAC এখানে skip করা হয়। */
        if (instr->opcode == TAC_FUNC_BEGIN || instr->opcode == TAC_FUNC_END)
            continue;
        /* বাকি সব TAC instruction-কে target C statements-এ নামাই। */
        emit_instruction(ctx, instr);
    }

    /* function body শেষ: indentation কমিয়ে closing brace emit। */
    ctx->indent--;
    /* readability-এর জন্য function শেষে extra newline রাখি। */
    emit(ctx, "}\n\n");
}

/* ============================================================================
 * EMIT FORWARD DECLARATIONS
 * ============================================================================
 */
static void emit_forward_decls(IRCGCtx *ctx, TACProgram *prog) {
    /* user-defined function list-এর head pointer নেই। */
    TACFunction *f = prog->functions;
    /* কোনো user function না থাকলে forward declaration section emit করার দরকার নেই। */
    if (!f) return;

    /* generated C-তে readability-এর জন্য section header comment। */
    emit_line(ctx, "/* Forward declarations */");
    /* linked list ধরে সব function prototype আগে থেকে emit করি। */
    while (f) {
        /* prototype-এর শুরুতে return type emit। */
        /*type_to_c(f->return_type) একটি C string রিটার্ন করে (যেমন "long long", "double", "void").
%s সেই string-টাই format string-এর মধ্যে বসায়।
%s-এর পরে যে space আছে ("%s "), সেটা ইচ্ছাকৃতভাবে type আর function name-এর মাঝে ফাঁকা জায়গা রাখে।*/
        emit(ctx, "%s ", type_to_c(f->return_type));
        /* function name sanitize: space কে underscore-এ map। */
        const char *p = f->name;
        while (p && *p) {
            emit(ctx, "%c", *p == ' ' ? '_' : *p);
            p++;
        }
        /* parameter list open parenthesis। */
        emit(ctx, "(");
        if (f->param_count > 0) {
            /* প্রত্যেক parameter-এর type + sanitized name prototype-এ লিখি। */
            for (int i = 0; i < f->param_count; i++) {
                /* parameter separator (দ্বিতীয় parameter থেকে)। */
                if (i > 0) emit(ctx, ", ");
                /* parameter C type emit। */
                emit(ctx, "%s ", type_to_c(f->param_types[i]));
                /* parameter name sanitize করে emit। */
                /*const char *pn = f->param_names[i];
: বর্তমান parameter-এর নাম string pointer হিসেবে নেয়।

while (pn && *pn)
: loop চলবে যতক্ষণ pointer NULL না এবং string শেষ না হয় (*pn != '\0')।

emit(ctx, "%c", *pn == ' ' ? '_' : *pn);
: একেকটা character output-এ লেখে।
: যদি character space হয়, তাহলে _ লেখা হয়।
: না হলে character যেমন আছে তেমনই লেখা হয়।

pn++;
: পরের character-এ যায়।*/
                const char *pn = f->param_names[i];
                while (pn && *pn) {
                    emit(ctx, "%c", *pn == ' ' ? '_' : *pn);
                    pn++;
                }
            }
        } else {
            /* no-arg function হলে explicit void parameter list। */
            emit(ctx, "void");
        }
        /* prototype terminator ';' + newline। */
        emit(ctx, ");\n");
        /* পরের function node-এ অগ্রসর হই। */
        f = f->next;
    }
    /* section শেষে readability-এর জন্য এক লাইন ফাঁকা রাখি। */
    emit(ctx, "\n");
}

/* ============================================================================
 * EMIT MAIN FUNCTION (top-level code)
 * ============================================================================
 */
static void emit_main_func(IRCGCtx *ctx, TACFunction *main_func) {
    /* generated program-এর entry point signature emit করি। */
    emit_line(ctx, "int main(int argc, char *argv[]) {");
    /* main body-তে ঢুকে indentation depth ১ ধাপ বাড়াই। */
    ctx->indent++;
    /* unused-parameter warning এড়াতে argc/argv explicitly consume করি। */
    emit_line(ctx, "(void)argc; (void)argv;");
    /* readability-এর জন্য এক লাইন ফাঁকা। */
    emit(ctx, "\n");

    /* Declare temporaries */
    /* main TAC block-এ দরকারি temporaries function top-এ declare করি। */
    emit_temp_declarations(ctx, main_func);

    /* Emit instructions */
    /* main function-এর TAC instruction list sequentially C code-এ নামাই। */
    for (TACInstr *instr = main_func->first; instr; instr = instr->next) {
        emit_instruction(ctx, instr);
    }

    /* return এর আগে visual separation রাখি। */
    emit(ctx, "\n");
    /* return statement current indentation অনুযায়ী align করি। */
    emit_indent(ctx);
    /* conventional successful process exit code emit। */
    emit(ctx, "return 0;\n");
    /* main block শেষ হওয়ায় indentation level restore করি। */
    ctx->indent--;
    /* closing brace emit করে main function সম্পন্ন করি। */
    emit_line(ctx, "}");
}

/* ============================================================================
 * PUBLIC API
 * ============================================================================
 */

IRCodegenOptions ir_codegen_default_options(void) {
    /* codegen-এর default configuration struct literal আকারে initialize করি। */
    IRCodegenOptions opts = {
        /* generated code-এ optional comments default-এ enabled। */
        .emit_comments = 1,
        /* debug info emission default-এ off রাখা হয়। */
        .emit_debug_info = 0,
        /* indentation width default 4 spaces। */
        .indent_size = 4,
    };
    /* caller-এর জন্য ready-to-use default options ফেরত দিই। */
    return opts;
}

IRCodegenResult ir_codegen_generate(TACProgram *program, IRCodegenOptions *opts) {
    /* API return object; success/code/error সব metadata এখানে জমা হবে। */
    IRCodegenResult result;
    /* শুরুতে সব field zero-initialize করে deterministic state নিশ্চিত করি। */
    memset(&result, 0, sizeof(result));

    /* invalid input guard: NULL program এ early failure ফেরত। */
    if (!program) {
        result.success = 0;
        /* caller-facing error message buffer-এ failure কারণ লিখি। */
        snprintf(result.error_message, sizeof(result.error_message),
                 "NULL program");
        return result;
    }

    /* caller options থাকলে সেটি, নাহলে project default options ব্যবহার। */
    IRCodegenOptions options = opts ? *opts : ir_codegen_default_options();

    /* internal codegen context stack-এ তৈরি ও initialize। */
    IRCGCtx ctx;
    ctx_init(&ctx, options.indent_size);
    /* option থেকে comment emission behavior context-এ propagate। */
    ctx.emit_comments = options.emit_comments;

    /* Pass 1: scan all functions for features and register return types */
    /* main function scan করে input/math/list feature flags নির্ধারণ। */
    scan_features(&ctx, program->main_func);
    /* user functions iterate করে feature scan + return type registry পূরণ। */
    TACFunction *f = program->functions;
    while (f) {
        scan_features(&ctx, f);
        if (f->name) {
            /* function নাম থাকলে return type lookup-table-এ register করি। */
            ctx_register_func(&ctx, f->name, f->return_type);
        }
        /* next function node-এ অগ্রসর হই। */
        f = f->next;
    }

    /* Emit headers */
    /* feature-aware C headers/runtime includes output buffer-এ emit। */
    emit_headers(&ctx);

    /* Forward declarations */
    /* user function prototypes main-এর আগে declare করি। */
    emit_forward_decls(&ctx, program);

    /* User functions */
    /* সব user-defined function body generated C-তে emit করি। */
    f = program->functions;
    while (f) {
        emit_function(&ctx, f);
        f = f->next;
    }

    /* Main */
    /* top-level TAC থেকে main() function emit। */
    emit_main_func(&ctx, program->main_func);

    /* Build result */
    /* context error count দেখে overall success flag নির্ধারণ। */
    result.success = (ctx.error_count == 0);
    /* context buffer-এর generated source caller-owned কপিতে transfer করি। */
    result.generated_code = strdup(ctx.buf);
    /* generated code length metadata। */
    result.code_length = ctx.size;
    /* internal error count caller-visible result-এ expose। */
    result.error_count = ctx.error_count;

    /* internal buffer/context lifecycle শেষ করে resource cleanup। */
    ctx_free(&ctx);
    return result;
}

int ir_codegen_to_file(TACProgram *program, IRCodegenOptions *opts,
                       const char *filename) {
    /* আগে in-memory generation চালাই; success হলে তবেই ফাইলে লিখব। */
    IRCodegenResult result = ir_codegen_generate(program, opts);
    /* generation fail হলে allocated code buffer release করে failure ফেরত। */
    if (!result.success) {
        free(result.generated_code);
        return 0;
    }

    /* target file write mode-এ open। */
    FILE *f = fopen(filename, "w");
    /* file open ব্যর্থ হলে generated buffer free করে failure ফেরত। */
    if (!f) {
        free(result.generated_code);
        return 0;
    }

    /* generated C source সম্পূর্ণভাবে ফাইলে লিখি। */
    fputs(result.generated_code, f);
    /* file handle flush/close। */
    fclose(f);
    /* temporary generated buffer মুক্ত করি। */
    free(result.generated_code);
    /* successful file emission signal। */
    return 1;
}

void ir_codegen_result_free(IRCodegenResult *result) {
    /* defensive guard: NULL pointer হলে কোনো কাজ নেই। */
    if (result) {
        /* API consumer-এর allocated generated_code memory release। */
        free(result->generated_code);
        /* dangling pointer এড়াতে field NULL করি। */
        result->generated_code = NULL;
    }
}
