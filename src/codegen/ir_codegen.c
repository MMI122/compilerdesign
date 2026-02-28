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
    ctx->cap = 8192;
    ctx->buf = malloc(ctx->cap);
    ctx->size = 0;
    ctx->buf[0] = '\0';
    ctx->indent = 0;
    ctx->indent_size = indent_size;
    ctx->emit_comments = 0;
    ctx->error_count = 0;
    ctx->needs_input_buffer = 0;
    ctx->needs_math = 0;
    ctx->needs_list = 0;
    ctx->var_type_count = 0;
    memset(ctx->temp_types, 0, sizeof(ctx->temp_types));
    ctx->func_type_count = 0;
}

static void ctx_register_func(IRCGCtx *ctx, const char *name, DataType ret_type) {
    for (int i = 0; i < ctx->func_type_count; i++) {
        if (strcmp(ctx->func_types[i].name, name) == 0) {
            ctx->func_types[i].ret_type = ret_type;
            return;
        }
    }
    if (ctx->func_type_count < MAX_FUNC_TYPES) {
        strncpy(ctx->func_types[ctx->func_type_count].name, name,
                sizeof(ctx->func_types[0].name) - 1);
        ctx->func_types[ctx->func_type_count].ret_type = ret_type;
        ctx->func_type_count++;
    }
}

static DataType ctx_lookup_func_ret(IRCGCtx *ctx, const char *name) {
    for (int i = 0; i < ctx->func_type_count; i++) {
        if (strcmp(ctx->func_types[i].name, name) == 0) {
            return ctx->func_types[i].ret_type;
        }
    }
    return TYPE_UNKNOWN;
}

static void ctx_register_var(IRCGCtx *ctx, const char *name, DataType type) {
    /* Update if already present */
    for (int i = 0; i < ctx->var_type_count; i++) {
        if (strcmp(ctx->var_types[i].name, name) == 0) {
            ctx->var_types[i].type = type;
            return;
        }
    }
    if (ctx->var_type_count < MAX_VAR_TYPES) {
        strncpy(ctx->var_types[ctx->var_type_count].name, name,
                sizeof(ctx->var_types[0].name) - 1);
        ctx->var_types[ctx->var_type_count].type = type;
        ctx->var_type_count++;
    }
}

static DataType ctx_lookup_var_type(IRCGCtx *ctx, const char *name) {
    for (int i = 0; i < ctx->var_type_count; i++) {
        if (strcmp(ctx->var_types[i].name, name) == 0) {
            return ctx->var_types[i].type;
        }
    }
    return TYPE_UNKNOWN;
}

/* Resolve the effective type of an operand using context tables */
static DataType resolve_type(IRCGCtx *ctx, TACOperand *op) {
    /* If the operand already has a known type, use it */
    if (op->data_type != TYPE_UNKNOWN && op->data_type != TYPE_NUMBER) {
        return op->data_type;
    }
    switch (op->kind) {
        case OPERAND_STRING:
            return TYPE_TEXT;
        case OPERAND_FLOAT:
            return TYPE_DECIMAL;
        case OPERAND_BOOL:
            return TYPE_FLAG;
        case OPERAND_INT:
            return TYPE_NUMBER;
        case OPERAND_VAR:
            if (op->val.name) {
                DataType t = ctx_lookup_var_type(ctx, op->val.name);
                if (t != TYPE_UNKNOWN) return t;
            }
            return op->data_type;
        case OPERAND_TEMP:
            if (op->val.temp_id >= 0 && op->val.temp_id < MAX_TEMP_TYPES) {
                DataType t = ctx->temp_types[op->val.temp_id];
                if (t != TYPE_UNKNOWN) return t;
            }
            return op->data_type;
        default:
            return op->data_type;
    }
}

/* Record the type of a result operand.
 * For variables, only record if not already declared (DECL types are authoritative). */
static void record_result_type(IRCGCtx *ctx, TACOperand *result, DataType type) {
    if (result->kind == OPERAND_TEMP && result->val.temp_id >= 0 &&
        result->val.temp_id < MAX_TEMP_TYPES) {
        ctx->temp_types[result->val.temp_id] = type;
    } else if (result->kind == OPERAND_VAR && result->val.name) {
        /* Only set if not already declared */
        DataType existing = ctx_lookup_var_type(ctx, result->val.name);
        if (existing == TYPE_UNKNOWN) {
            ctx_register_var(ctx, result->val.name, type);
        }
    }
}

static void ctx_free(IRCGCtx *ctx) {
    free(ctx->buf);
}

static void ensure_cap(IRCGCtx *ctx, size_t n) {
    if (ctx->size + n >= ctx->cap) {
        ctx->cap = (ctx->size + n) * 2;
        ctx->buf = realloc(ctx->buf, ctx->cap);
    }
}

static void emit(IRCGCtx *ctx, const char *fmt, ...) {
    va_list args;
    char tmp[4096];
    va_start(args, fmt);
    int len = vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);
    if (len > 0) {
        ensure_cap(ctx, (size_t)len + 1);
        memcpy(ctx->buf + ctx->size, tmp, (size_t)len);
        ctx->size += (size_t)len;
        ctx->buf[ctx->size] = '\0';
    }
}

static void emit_indent(IRCGCtx *ctx) {
    int sp = ctx->indent * ctx->indent_size;
    for (int i = 0; i < sp; i++) emit(ctx, " ");
}

static void emit_line(IRCGCtx *ctx, const char *fmt, ...) {
    va_list args;
    char tmp[4096];
    emit_indent(ctx);
    va_start(args, fmt);
    int len = vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);
    if (len > 0) {
        ensure_cap(ctx, (size_t)len + 2);
        memcpy(ctx->buf + ctx->size, tmp, (size_t)len);
        ctx->size += (size_t)len;
        ctx->buf[ctx->size++] = '\n';
        ctx->buf[ctx->size] = '\0';
    }
}

/* ============================================================================
 * OPERAND → C EXPRESSION
 * ============================================================================
 */
static void emit_operand(IRCGCtx *ctx, TACOperand *op) {
    switch (op->kind) {
        case OPERAND_TEMP:
            emit(ctx, "_t%d", op->val.temp_id);
            break;
        case OPERAND_VAR:
            /* Replace spaces with underscores for multi-word NatureLang names */
            if (op->val.name) {
                const char *p = op->val.name;
                while (*p) {
                    emit(ctx, "%c", *p == ' ' ? '_' : *p);
                    p++;
                }
            }
            break;
        case OPERAND_INT:
            emit(ctx, "%lldLL", op->val.int_val);
            break;
        case OPERAND_FLOAT:
            emit(ctx, "%g", op->val.float_val);
            break;
        case OPERAND_STRING:
            emit(ctx, "\"");
            if (op->val.str_val) {
                const char *s = op->val.str_val;
                while (*s) {
                    switch (*s) {
                        case '"':  emit(ctx, "\\\""); break;
                        case '\\': emit(ctx, "\\\\"); break;
                        case '\n': emit(ctx, "\\n"); break;
                        case '\t': emit(ctx, "\\t"); break;
                        case '\r': emit(ctx, "\\r"); break;
                        default:   emit(ctx, "%c", *s); break;
                    }
                    s++;
                }
            }
            emit(ctx, "\"");
            break;
        case OPERAND_BOOL:
            emit(ctx, "%d", op->val.bool_val ? 1 : 0);
            break;
        case OPERAND_FUNC:
            if (op->val.name) {
                const char *p = op->val.name;
                while (*p) {
                    emit(ctx, "%c", *p == ' ' ? '_' : *p);
                    p++;
                }
            }
            break;
        case OPERAND_LABEL:
            emit(ctx, "L%d", op->val.label_id);
            break;
        case OPERAND_NONE:
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
    for (TACInstr *i = func->first; i; i = i->next) {
        if (i->is_dead) continue;
        switch (i->opcode) {
            case TAC_ASK: case TAC_READ:
                ctx->needs_input_buffer = 1;
                break;
            case TAC_POW:
                ctx->needs_math = 1;
                break;
            case TAC_LIST_CREATE: case TAC_LIST_APPEND:
            case TAC_LIST_GET: case TAC_LIST_SET:
                ctx->needs_list = 1;
                break;
            default:
                break;
        }
    }
}

/* ============================================================================
 * EMIT HEADERS
 * ============================================================================
 */
static void emit_headers(IRCGCtx *ctx) {
    emit_line(ctx, "/*");
    emit_line(ctx, " * Generated by NatureLang Compiler (IR pipeline)");
    emit_line(ctx, " * Do not edit this file directly.");
    emit_line(ctx, " */");
    emit(ctx, "\n");
    emit_line(ctx, "#define _POSIX_C_SOURCE 200809L");
    emit_line(ctx, "#include <stdio.h>");
    emit_line(ctx, "#include <stdlib.h>");
    emit_line(ctx, "#include <string.h>");
    emit_line(ctx, "#include <stdbool.h>");
    if (ctx->needs_math) {
        emit_line(ctx, "#include <math.h>");
    }
    emit_line(ctx, "#include \"naturelang_runtime.h\"");
    emit(ctx, "\n");

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
    for (TACInstr *i = func->first; i; i = i->next) {
        if (i->is_dead) continue;
        switch (i->opcode) {
            case TAC_LOAD_INT:
                record_result_type(ctx, &i->result, TYPE_NUMBER);
                break;
            case TAC_LOAD_FLOAT:
                record_result_type(ctx, &i->result, TYPE_DECIMAL);
                break;
            case TAC_LOAD_STRING:
                record_result_type(ctx, &i->result, TYPE_TEXT);
                break;
            case TAC_LOAD_BOOL:
                record_result_type(ctx, &i->result, TYPE_FLAG);
                break;
            case TAC_DECL:
                if (i->result.kind == OPERAND_VAR && i->result.val.name)
                    ctx_register_var(ctx, i->result.val.name, i->result.data_type);
                break;
            case TAC_CONCAT:
                record_result_type(ctx, &i->result, TYPE_TEXT);
                break;
            case TAC_ASK: case TAC_READ:
                record_result_type(ctx, &i->result, TYPE_TEXT);
                break;
            case TAC_EQ: case TAC_NEQ: case TAC_LT: case TAC_GT:
            case TAC_LTE: case TAC_GTE: case TAC_AND: case TAC_OR:
            case TAC_NOT: case TAC_BETWEEN:
                record_result_type(ctx, &i->result, TYPE_FLAG);
                break;
            default:
                break;
        }
    }
    /* Second pass: also propagate via assigns */
    for (TACInstr *i = func->first; i; i = i->next) {
        if (i->is_dead) continue;
        if (i->opcode == TAC_ASSIGN) {
            DataType src = resolve_type(ctx, &i->arg1);
            if (src != TYPE_UNKNOWN)
                record_result_type(ctx, &i->result, src);
        }
    }

    /* Collect all temp IDs used and their resolved types */
    #define MAX_TEMPS 4096
    int seen[MAX_TEMPS];
    DataType types[MAX_TEMPS];
    int count = 0;

    for (TACInstr *i = func->first; i; i = i->next) {
        if (i->is_dead) continue;
        TACOperand *ops[] = { &i->result, &i->arg1, &i->arg2, &i->arg3 };
        for (int j = 0; j < 4; j++) {
            if (ops[j]->kind == OPERAND_TEMP) {
                int tid = ops[j]->val.temp_id;
                int found = 0;
                for (int k = 0; k < count; k++) {
                    if (seen[k] == tid) { found = 1; break; }
                }
                if (!found && count < MAX_TEMPS) {
                    seen[count] = tid;
                    /* Use resolved type from context */
                    if (tid >= 0 && tid < MAX_TEMP_TYPES && ctx->temp_types[tid] != TYPE_UNKNOWN) {
                        types[count] = ctx->temp_types[tid];
                    } else {
                        types[count] = ops[j]->data_type;
                    }
                    count++;
                }
            }
        }
    }

    if (count > 0) {
        if (ctx->emit_comments) {
            emit_indent(ctx);
            emit(ctx, "/* temporaries */\n");
        }
        for (int i = 0; i < count; i++) {
            emit_indent(ctx);
            DataType dt = types[i];
            if (dt == TYPE_TEXT) {
                emit(ctx, "char* _t%d = NULL;\n", seen[i]);
            } else {
                emit(ctx, "%s _t%d = 0;\n", type_to_c(dt), seen[i]);
            }
        }
        emit(ctx, "\n");
    }
    #undef MAX_TEMPS
}

/* ============================================================================
 * EMIT A PRINTF FOR A DISPLAY INSTRUCTION
 *
 * Determine the type from the operand and use the appropriate format.
 * ============================================================================
 */
static void emit_display(IRCGCtx *ctx, TACOperand *val) {
    DataType effective_type = resolve_type(ctx, val);
    emit_indent(ctx);
    switch (effective_type) {
        case TYPE_NUMBER:
            emit(ctx, "printf(\"%%lld\\n\", (long long)");
            emit_operand(ctx, val);
            emit(ctx, ");\n");
            break;
        case TYPE_DECIMAL:
            emit(ctx, "printf(\"%%g\\n\", (double)");
            emit_operand(ctx, val);
            emit(ctx, ");\n");
            break;
        case TYPE_TEXT:
            emit(ctx, "printf(\"%%s\\n\", ");
            emit_operand(ctx, val);
            emit(ctx, ");\n");
            break;
        case TYPE_FLAG:
            emit(ctx, "printf(\"%%s\\n\", ");
            emit_operand(ctx, val);
            emit(ctx, " ? \"yes\" : \"no\");\n");
            break;
        default:
            /* Generic: try as number */
            emit(ctx, "printf(\"%%lld\\n\", (long long)");
            emit_operand(ctx, val);
            emit(ctx, ");\n");
            break;
    }
}

/* ============================================================================
 * EMIT A SINGLE TAC INSTRUCTION AS C CODE
 * ============================================================================
 */
static void emit_instruction(IRCGCtx *ctx, TACInstr *instr) {
    if (!instr || instr->is_dead) return;

    /* Record type information for result operands */
    switch (instr->opcode) {
        case TAC_LOAD_INT:
            record_result_type(ctx, &instr->result, TYPE_NUMBER);
            break;
        case TAC_LOAD_FLOAT:
            record_result_type(ctx, &instr->result, TYPE_DECIMAL);
            break;
        case TAC_LOAD_STRING:
            record_result_type(ctx, &instr->result, TYPE_TEXT);
            break;
        case TAC_LOAD_BOOL:
            record_result_type(ctx, &instr->result, TYPE_FLAG);
            break;
        case TAC_DECL:
            if (instr->result.kind == OPERAND_VAR && instr->result.val.name) {
                ctx_register_var(ctx, instr->result.val.name, instr->result.data_type);
            }
            break;
        case TAC_ASSIGN: {
            DataType src_type = resolve_type(ctx, &instr->arg1);
            if (src_type != TYPE_UNKNOWN)
                record_result_type(ctx, &instr->result, src_type);
            break;
        }
        case TAC_CONCAT:
            record_result_type(ctx, &instr->result, TYPE_TEXT);
            break;
        case TAC_ASK: case TAC_READ:
            record_result_type(ctx, &instr->result, TYPE_TEXT);
            break;
        case TAC_EQ: case TAC_NEQ: case TAC_LT: case TAC_GT:
        case TAC_LTE: case TAC_GTE: case TAC_AND: case TAC_OR:
        case TAC_NOT: case TAC_BETWEEN:
            record_result_type(ctx, &instr->result, TYPE_FLAG);
            break;
        case TAC_ADD: case TAC_SUB: case TAC_MUL: case TAC_DIV:
        case TAC_MOD: case TAC_NEG: case TAC_POW: {
            DataType lt = resolve_type(ctx, &instr->arg1);
            DataType rt = resolve_type(ctx, &instr->arg2);
            DataType res = (lt == TYPE_DECIMAL || rt == TYPE_DECIMAL)
                           ? TYPE_DECIMAL : TYPE_NUMBER;
            record_result_type(ctx, &instr->result, res);
            break;
        }
        default:
            break;
    }

    switch (instr->opcode) {

        /* ---- Labels ---- */
        case TAC_LABEL:
            /* Un-indent labels */
            emit(ctx, "L%d:;\n", instr->result.val.label_id);
            break;

        case TAC_GOTO:
            emit_indent(ctx);
            emit(ctx, "goto L%d;\n", instr->result.val.label_id);
            break;

        case TAC_IF_GOTO:
            emit_indent(ctx);
            emit(ctx, "if (");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, ") goto L%d;\n", instr->result.val.label_id);
            break;

        case TAC_IF_FALSE_GOTO:
            emit_indent(ctx);
            emit(ctx, "if (!(");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, ")) goto L%d;\n", instr->result.val.label_id);
            break;

        /* ---- Loads ---- */
        case TAC_LOAD_INT:
        case TAC_LOAD_FLOAT:
        case TAC_LOAD_BOOL:
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = ");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, ";\n");
            break;

        case TAC_LOAD_STRING:
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = ");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, ";\n");
            break;

        /* ---- Assignment ---- */
        case TAC_ASSIGN:
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = ");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, ";\n");
            break;

        /* ---- Binary arithmetic ---- */
        case TAC_ADD:
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = ");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, " + ");
            emit_operand(ctx, &instr->arg2);
            emit(ctx, ";\n");
            break;

        case TAC_SUB:
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = ");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, " - ");
            emit_operand(ctx, &instr->arg2);
            emit(ctx, ";\n");
            break;

        case TAC_MUL:
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = ");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, " * ");
            emit_operand(ctx, &instr->arg2);
            emit(ctx, ";\n");
            break;

        case TAC_DIV:
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = ");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, " / ");
            emit_operand(ctx, &instr->arg2);
            emit(ctx, ";\n");
            break;

        case TAC_MOD:
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = ");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, " %% ");
            emit_operand(ctx, &instr->arg2);
            emit(ctx, ";\n");
            break;

        case TAC_POW:
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
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = -(");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, ");\n");
            break;

        case TAC_NOT:
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = !(");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, ");\n");
            break;

        /* ---- Comparison ---- */
        case TAC_EQ:  case TAC_NEQ: case TAC_LT: case TAC_GT:
        case TAC_LTE: case TAC_GTE: {
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
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = (");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, " && ");
            emit_operand(ctx, &instr->arg2);
            emit(ctx, ");\n");
            break;

        case TAC_OR:
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
            emit_indent(ctx);
            emit(ctx, "%s ", type_to_c(instr->result.data_type));
            emit_operand(ctx, &instr->result);
            /* Default initialization */
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
            emit_display(ctx, &instr->arg1);
            break;

        case TAC_ASK:
            ctx->needs_input_buffer = 1;
            /* Print prompt */
            if (instr->arg1.kind != OPERAND_NONE) {
                emit_indent(ctx);
                emit(ctx, "printf(\"%%s\", ");
                emit_operand(ctx, &instr->arg1);
                emit(ctx, "); fflush(stdout);\n");
            }
            /* Read input */
            emit_indent(ctx);
            emit(ctx, "fgets(_nl_input_buffer, sizeof(_nl_input_buffer), stdin); ");
            emit(ctx, "_nl_input_buffer[strcspn(_nl_input_buffer, \"\\n\")] = 0; ");
            emit_operand(ctx, &instr->result);
            emit(ctx, " = strdup(_nl_input_buffer);\n");
            break;

        case TAC_READ:
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
            break;
        case TAC_FUNC_END:
            break;

        case TAC_PARAM:
            /* Parameters are collected by the CALL handler,
               but in our linear IR they're emitted as standalone.
               We emit them as comments; the calling convention 
               passes args inline in the CALL expansion. */
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
            int nargs = 0;
            if (instr->arg2.kind == OPERAND_INT) {
                nargs = (int)instr->arg2.val.int_val;
            }

            /* Check if function returns void (don't assign result) */
            int is_void_call = 0;
            if (instr->arg1.kind == OPERAND_FUNC && instr->arg1.val.name) {
                DataType ret = ctx_lookup_func_ret(ctx, instr->arg1.val.name);
                if (ret == TYPE_NOTHING) is_void_call = 1;
            }

            emit_indent(ctx);
            if (instr->result.kind != OPERAND_NONE && !is_void_call) {
                emit_operand(ctx, &instr->result);
                emit(ctx, " = ");
            }

            /* Function name */
            if (instr->arg1.kind == OPERAND_FUNC &&
                instr->arg1.val.name &&
                strcmp(instr->arg1.val.name, "__list_length") == 0) {
                /* Special: list length */
                emit(ctx, "nl_list_length(");
            } else {
                emit_operand(ctx, &instr->arg1);
                emit(ctx, "(");
            }

            /* Collect arguments from preceding PARAM instructions */
            if (nargs > 0) {
                /* Find the nargs PARAM instructions before this CALL */
                TACInstr *params[64];
                int found = 0;
                TACInstr *scan = instr->prev;
                while (scan && found < nargs) {
                    if (scan->is_dead) { scan = scan->prev; continue; }
                    if (scan->opcode == TAC_PARAM) {
                        params[found++] = scan;
                    }
                    scan = scan->prev;
                }
                /* Emit in reverse order (first param first) */
                for (int i = found - 1; i >= 0; i--) {
                    if (i < found - 1) emit(ctx, ", ");
                    emit_operand(ctx, &params[i]->arg1);
                }
            }

            emit(ctx, ");\n");
            break;
        }

        case TAC_RETURN:
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
            emit_indent(ctx);
            emit(ctx, "{\n");
            ctx->indent++;
            break;

        case TAC_SCOPE_END:
            ctx->indent--;
            emit_indent(ctx);
            emit(ctx, "}\n");
            break;

        case TAC_SECURE_BEGIN:
            if (ctx->emit_comments) {
                emit_indent(ctx);
                emit(ctx, "/* BEGIN SECURE ZONE */\n");
            }
            break;

        case TAC_SECURE_END:
            if (ctx->emit_comments) {
                emit_indent(ctx);
                emit(ctx, "/* END SECURE ZONE */\n");
            }
            break;

        /* ---- List operations ---- */
        case TAC_LIST_CREATE:
            ctx->needs_list = 1;
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = nl_list_create(");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, ");\n");
            break;

        case TAC_LIST_APPEND:
            emit_indent(ctx);
            emit(ctx, "nl_list_append(");
            emit_operand(ctx, &instr->result);
            emit(ctx, ", ");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, ");\n");
            break;

        case TAC_LIST_GET:
            emit_indent(ctx);
            emit_operand(ctx, &instr->result);
            emit(ctx, " = nl_list_get_num(");
            emit_operand(ctx, &instr->arg1);
            emit(ctx, ", ");
            emit_operand(ctx, &instr->arg2);
            emit(ctx, ");\n");
            break;

        case TAC_LIST_SET:
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
            break;

        default:
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
    emit(ctx, "%s ", type_to_c(func->return_type));

    /* Name */
    const char *p = func->name;
    while (p && *p) {
        emit(ctx, "%c", *p == ' ' ? '_' : *p);
        p++;
    }

    /* Parameters */
    emit(ctx, "(");
    if (func->param_count > 0) {
        for (int i = 0; i < func->param_count; i++) {
            if (i > 0) emit(ctx, ", ");
            emit(ctx, "%s ", type_to_c(func->param_types[i]));
            const char *pn = func->param_names[i];
            while (pn && *pn) {
                emit(ctx, "%c", *pn == ' ' ? '_' : *pn);
                pn++;
            }
        }
    } else {
        emit(ctx, "void");
    }

    emit(ctx, ") {\n");
    ctx->indent++;

    /* Declare temporaries */
    emit_temp_declarations(ctx, func);

    /* Emit instructions (skip FUNC_BEGIN/FUNC_END) */
    for (TACInstr *instr = func->first; instr; instr = instr->next) {
        if (instr->opcode == TAC_FUNC_BEGIN || instr->opcode == TAC_FUNC_END)
            continue;
        emit_instruction(ctx, instr);
    }

    ctx->indent--;
    emit(ctx, "}\n\n");
}

/* ============================================================================
 * EMIT FORWARD DECLARATIONS
 * ============================================================================
 */
static void emit_forward_decls(IRCGCtx *ctx, TACProgram *prog) {
    TACFunction *f = prog->functions;
    if (!f) return;

    emit_line(ctx, "/* Forward declarations */");
    while (f) {
        emit(ctx, "%s ", type_to_c(f->return_type));
        const char *p = f->name;
        while (p && *p) {
            emit(ctx, "%c", *p == ' ' ? '_' : *p);
            p++;
        }
        emit(ctx, "(");
        if (f->param_count > 0) {
            for (int i = 0; i < f->param_count; i++) {
                if (i > 0) emit(ctx, ", ");
                emit(ctx, "%s ", type_to_c(f->param_types[i]));
                const char *pn = f->param_names[i];
                while (pn && *pn) {
                    emit(ctx, "%c", *pn == ' ' ? '_' : *pn);
                    pn++;
                }
            }
        } else {
            emit(ctx, "void");
        }
        emit(ctx, ");\n");
        f = f->next;
    }
    emit(ctx, "\n");
}

/* ============================================================================
 * EMIT MAIN FUNCTION (top-level code)
 * ============================================================================
 */
static void emit_main_func(IRCGCtx *ctx, TACFunction *main_func) {
    emit_line(ctx, "int main(int argc, char *argv[]) {");
    ctx->indent++;
    emit_line(ctx, "(void)argc; (void)argv;");
    emit(ctx, "\n");

    /* Declare temporaries */
    emit_temp_declarations(ctx, main_func);

    /* Emit instructions */
    for (TACInstr *instr = main_func->first; instr; instr = instr->next) {
        emit_instruction(ctx, instr);
    }

    emit(ctx, "\n");
    emit_indent(ctx);
    emit(ctx, "return 0;\n");
    ctx->indent--;
    emit_line(ctx, "}");
}

/* ============================================================================
 * PUBLIC API
 * ============================================================================
 */

IRCodegenOptions ir_codegen_default_options(void) {
    IRCodegenOptions opts = {
        .emit_comments = 1,
        .emit_debug_info = 0,
        .indent_size = 4,
    };
    return opts;
}

IRCodegenResult ir_codegen_generate(TACProgram *program, IRCodegenOptions *opts) {
    IRCodegenResult result;
    memset(&result, 0, sizeof(result));

    if (!program) {
        result.success = 0;
        snprintf(result.error_message, sizeof(result.error_message),
                 "NULL program");
        return result;
    }

    IRCodegenOptions options = opts ? *opts : ir_codegen_default_options();

    IRCGCtx ctx;
    ctx_init(&ctx, options.indent_size);
    ctx.emit_comments = options.emit_comments;

    /* Pass 1: scan all functions for features and register return types */
    scan_features(&ctx, program->main_func);
    TACFunction *f = program->functions;
    while (f) {
        scan_features(&ctx, f);
        if (f->name) {
            ctx_register_func(&ctx, f->name, f->return_type);
        }
        f = f->next;
    }

    /* Emit headers */
    emit_headers(&ctx);

    /* Forward declarations */
    emit_forward_decls(&ctx, program);

    /* User functions */
    f = program->functions;
    while (f) {
        emit_function(&ctx, f);
        f = f->next;
    }

    /* Main */
    emit_main_func(&ctx, program->main_func);

    /* Build result */
    result.success = (ctx.error_count == 0);
    result.generated_code = strdup(ctx.buf);
    result.code_length = ctx.size;
    result.error_count = ctx.error_count;

    ctx_free(&ctx);
    return result;
}

int ir_codegen_to_file(TACProgram *program, IRCodegenOptions *opts,
                       const char *filename) {
    IRCodegenResult result = ir_codegen_generate(program, opts);
    if (!result.success) {
        free(result.generated_code);
        return 0;
    }

    FILE *f = fopen(filename, "w");
    if (!f) {
        free(result.generated_code);
        return 0;
    }

    fputs(result.generated_code, f);
    fclose(f);
    free(result.generated_code);
    return 1;
}

void ir_codegen_result_free(IRCodegenResult *result) {
    if (result) {
        free(result->generated_code);
        result->generated_code = NULL;
    }
}
