/*
 * NatureLang Compiler
 * Copyright (c) 2026
 * 
 * Code Generator Implementation
 * 
 * Translates validated AST to C source code.
 */
/*
ir_codegen.c = active/primary backend (IR -> C)
codegen.c = legacy বা alternate AST -> C backend (রাখা আছে, কিন্তু এখন main flow-এ use হচ্ছে না)
*/
#define _POSIX_C_SOURCE 200809L
#include "codegen.h"
#include "ast.h"
#include "symbol_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* Forward declarations */
/* internal dispatcher/helper prototypes আগে declare করি যাতে নিচের function-গুলোতে call করা যায়। */
static void codegen_node(CodegenContext *ctx, ASTNode *node);
/* statement-level code emission helper prototype। */
static void codegen_statement(CodegenContext *ctx, ASTNode *node);
/* expression-level code emission helper prototype। */
static void codegen_expression(CodegenContext *ctx, ASTNode *node);

/* Buffer management */
static void buffer_init(CodegenContext *ctx) {
    /* output buffer-এর initial capacity 8 KiB দিয়ে শুরু করি। */
    ctx->buffer_capacity = 8192;
    /* generated C code জমার জন্য heap buffer allocate। */
    ctx->buffer = malloc(ctx->buffer_capacity);
    /* শুরুতে buffer empty, তাই current size = 0। */
    ctx->buffer_size = 0;
    /* buffer-কে valid empty C-string করতে NUL terminator বসাই। */
    ctx->buffer[0] = '\0';
}

static void buffer_ensure_capacity(CodegenContext *ctx, size_t needed) {
    /* নতুন লেখা যোগ করলে capacity ছাড়িয়ে গেলে buffer grow করতে হবে। */
    if (ctx->buffer_size + needed >= ctx->buffer_capacity) {
        /* growth strategy: required size-এর দ্বিগুণ নিয়ে realloc frequency কমাই। */
        ctx->buffer_capacity = (ctx->buffer_size + needed) * 2;
        /* resized capacity অনুযায়ী buffer pointer update। */
        ctx->buffer = realloc(ctx->buffer, ctx->buffer_capacity);
    }
}

static void emit(CodegenContext *ctx, const char *fmt, ...) {
    /* variadic format arguments ধরার জন্য va_list। */
    va_list args;
    /* অস্থায়ী staging buffer যেখানে formatted text আগে তৈরি হবে। */
    char temp[4096];
    
    /* variadic argument reading শুরু। */
    va_start(args, fmt);
    /* fmt অনুযায়ী temp-এ render; len = লেখা character count। */
    int len = vsnprintf(temp, sizeof(temp), fmt, args);
    /* variadic argument reading শেষ। */
    va_end(args);
    
    /* valid output তৈরি হলে তবেই মূল output buffer-এ append করি। */
    if (len > 0) {
        /* নতুন text + NUL-এর জন্য পর্যাপ্ত capacity নিশ্চিত করি। */
        buffer_ensure_capacity(ctx, (size_t)len + 1);
        /* staging buffer থেকে destination buffer-এ bytes কপি। */
        memcpy(ctx->buffer + ctx->buffer_size, temp, (size_t)len);
        /* buffer cursor len পরিমাণ সামনে এগিয়ে দিই। */
        ctx->buffer_size += (size_t)len;
        /* C-string invariant বজায় রাখতে trailing NUL। */
        ctx->buffer[ctx->buffer_size] = '\0';
    }
}

static void emit_indent(CodegenContext *ctx) {
    /* মোট leading spaces = nesting depth × per-level indent size। */
    int spaces = ctx->indent_level * ctx->options.indent_size;
    /* গণনা করা space সংখ্যা অনুযায়ী একেকটি space emit করি। */
    for (int i = 0; i < spaces; i++) {
        emit(ctx, " ");
    }
}

static void emit_line(CodegenContext *ctx, const char *fmt, ...) {
    /* variadic formatting state holder। */
    va_list args;
    /* line body render করার temporary buffer। */
    char temp[4096];
    
    /* line শুরুতেই current indentation বসাই। */
    emit_indent(ctx);
    
    /* variadic argument reading শুরু। */
    va_start(args, fmt);
    /* fmt অনুযায়ী temp-এ line content render। */
    int len = vsnprintf(temp, sizeof(temp), fmt, args);
    /* variadic argument reading শেষ। */
    va_end(args);
    
    /* content থাকলে line + newline append করি। */
    if (len > 0) {
        /* content + newline + NUL-এর জন্য capacity নিশ্চিত। */
        buffer_ensure_capacity(ctx, (size_t)len + 2);
        /* rendered content main buffer-এ কপি। */
        memcpy(ctx->buffer + ctx->buffer_size, temp, (size_t)len);
        /* size cursor content length অনুযায়ী বাড়াই। */
        ctx->buffer_size += (size_t)len;
        /* line terminator newline append। */
        ctx->buffer[ctx->buffer_size++] = '\n';
        /* string terminator NUL বসাই। */
        ctx->buffer[ctx->buffer_size] = '\0';
    }
}

static void emit_newline(CodegenContext *ctx) {
    /* convenience helper: শুধু একটি blank newline emit করে। */
    emit(ctx, "\n");
}

/* Code generation options */
CodegenOptions codegen_default_options(void) {
    /* default codegen behavior নির্ধারণের জন্য options struct তৈরি। */
    CodegenOptions opts = {
        /* generated code-এ explanatory comments default-এ on। */
        .emit_comments = 1,
        /* debug info default-এ off রাখা হয়। */
        .emit_debug_info = 0,
        /* নিরাপদ helper/path usage default-এ on। */
        .use_safe_functions = 1,
        /* indentation width default 4 spaces। */
        .indent_size = 4
    };
    /* configured default options caller-কে ফেরত দিই। */
    return opts;
}

/* Create code generator */
CodegenContext *codegen_create(SymbolTable *symtab, CodegenOptions *options) {
    /* context object zero-initialized heap allocation। */
    CodegenContext *ctx = calloc(1, sizeof(CodegenContext));
    /* allocation fail হলে NULL return। */
    if (!ctx) return NULL;
    
    /* semantic symbol table context-এ attach করি। */
    ctx->symtab = symtab;
    /* caller options থাকলে সেটি copy, নাহলে defaults ব্যবহার। */
    if (options) {
        ctx->options = *options;
    } else {
        ctx->options = codegen_default_options();
    }
    
    /* output buffer initialize। */
    buffer_init(ctx);
    /* nesting depth শুরুতে zero। */
    ctx->indent_level = 0;
    /* temp variable naming counter reset। */
    ctx->temp_var_counter = 0;
    /* label naming counter reset। */
    ctx->label_counter = 0;
    /* error counter reset। */
    ctx->error_count = 0;
    /* function scope state শুরুতে false। */
    ctx->in_function = 0;
    /* loop scope state শুরুতে false। */
    ctx->in_loop = 0;
    /* input buffer feature flag শুরুতে off। */
    ctx->needs_input_buffer = 0;
    /* list support feature flag শুরুতে off। */
    ctx->needs_list_support = 0;
    
    /* fully initialized context caller-কে ফেরত দিই। */
    return ctx;
}

/* Destroy code generator */
void codegen_destroy(CodegenContext *ctx) {
    /* defensive guard: NULL হলে কিছুই করার নেই। */
    if (ctx) {
        /* generated source buffer memory free। */
        free(ctx->buffer);
        /* context object নিজেও মুক্ত করি। */
        free(ctx);
    }
}

/* Type conversion */
const char *naturelang_type_to_c(DataType type) {
    /* NatureLang datatype কে target C type string-এ map। */
    switch (type) {
        case TYPE_NUMBER:  return "long long";
        case TYPE_DECIMAL: return "double";
        case TYPE_TEXT:    return "char*";
        case TYPE_FLAG:    return "int";
        case TYPE_LIST:    return "NLList*";
        case TYPE_NOTHING: return "void";
        default:           return "int";  /* Unknown -> default to int */
    }
}

/* Generate temporary variable */
char *codegen_temp_var(CodegenContext *ctx) {
    /* temporary identifier string-এর জন্য ছোট heap buffer allocate। */
    char *name = malloc(32);
    /* monotonically increasing counter দিয়ে unique temp name বানাই। */
    snprintf(name, 32, "_nl_tmp%d", ctx->temp_var_counter++);
    /* caller এই allocated string পরে free করবে। */
    return name;
}

/* Generate label */
char *codegen_label(CodegenContext *ctx, const char *prefix) {
    /* label string-এর জন্য heap buffer allocate। */
    char *name = malloc(64);
    /* prefix + unique counter দিয়ে stable label নাম generate। */
    snprintf(name, 64, "_nl_%s%d", prefix, ctx->label_counter++);
    /* caller-side lifecycle management-এর জন্য pointer return। */
    return name;
}

/* Report error */
static void codegen_error(CodegenContext *ctx, const char *fmt, ...) {
    /* variadic error message format handling শুরু। */
    va_list args;
    va_start(args, fmt);
    /* latest error text context-এর error_message buffer-এ লিখি। */
    vsnprintf(ctx->error_message, sizeof(ctx->error_message), fmt, args);
    /* variadic processing শেষ। */
    va_end(args);
    /* মোট error counter এক ধাপ বাড়াই। */
    ctx->error_count++;
}

/* Emit standard headers and runtime includes */
static void emit_headers(CodegenContext *ctx) {
    /* generated file preamble comment block emit। */
    emit_line(ctx, "/*");
    emit_line(ctx, " * Generated by NatureLang Compiler");
    emit_line(ctx, " * Do not edit this file directly.");
    emit_line(ctx, " */");
    /* comment block শেষে visual spacing। */
    emit_newline(ctx);
    /* প্রয়োজনীয় standard/runtime headers emit। */
    emit_line(ctx, "#include <stdio.h>");
    emit_line(ctx, "#include <stdlib.h>");
    emit_line(ctx, "#include <string.h>");
    emit_line(ctx, "#include <stdbool.h>");
    emit_line(ctx, "#include <math.h>");
    emit_line(ctx, "#include \"naturelang_runtime.h\"");
    emit_newline(ctx);
}

/* Emit input buffer if needed */
static void emit_input_buffer(CodegenContext *ctx) {
    /* input statement থাকলে global input buffer declaration emit করি। */
    if (ctx->needs_input_buffer) {
        emit_line(ctx, "/* Input buffer for reading user input */");
        emit_line(ctx, "static char _nl_input_buffer[4096];");
        emit_newline(ctx);
    }
}

/* Convert identifier to valid C name */
static void emit_identifier(CodegenContext *ctx, const char *name) {
    /* Replace spaces with underscores for multi-word identifiers */
    /* source identifier-এর প্রতিটি character sanitize করে emit। */
    while (*name) {
        /* space character C identifier-safe underscore-এ map। */
        if (*name == ' ') {
            emit(ctx, "_");
        } else {
            /* অন্য character সরাসরি emit। */
            emit(ctx, "%c", *name);
        }
        /* পরের character-এ যাই। */
        name++;
    }
}

/* Escape string for C */
static void emit_string_literal(CodegenContext *ctx, const char *str) {
    /* C string literal opening quote। */
    emit(ctx, "\"");
    /* input string শেষ না হওয়া পর্যন্ত একে একে escape/emit করি। */
    while (*str) {
        switch (*str) {
            /* quote নিজেকে escape না করলে literal ভেঙে যাবে। */
            case '"':  emit(ctx, "\\\""); break;
            /* backslash character-ও escaped form-এ লাগবে। */
            case '\\': emit(ctx, "\\\\"); break;
            /* control chars readable escape sequence-এ map। */
            case '\n': emit(ctx, "\\n"); break;
            case '\t': emit(ctx, "\\t"); break;
            case '\r': emit(ctx, "\\r"); break;
            /* সাধারণ printable character as-is emit। */
            default:   emit(ctx, "%c", *str); break;
        }
        /* পরের source character। */
        str++;
    }
    /* C string literal closing quote। */
    emit(ctx, "\"");
}

/* Generate expression */
static void codegen_expression(CodegenContext *ctx, ASTNode *node) {
    /* null expression node হলে কোনো code emit করব না। */
    if (!node) return;
    
    /* AST expression kind অনুযায়ী target C expression emit। */
    switch (node->type) {
        case AST_LITERAL_INT:
            /* integer literal সরাসরি emit। */
            emit(ctx, "%lld", node->data.literal_int.value);
            break;
            
        case AST_LITERAL_FLOAT:
            /* floating literal compact %g format-এ emit। */
            emit(ctx, "%g", node->data.literal_float.value);
            break;
            
        case AST_LITERAL_STRING:
            /* string literal safe escaped form-এ emit helper call। */
            emit_string_literal(ctx, node->data.literal_string.value);
            break;
            
        case AST_LITERAL_BOOL:
            /* boolean-কে C truthy int (1/0) হিসেবে emit। */
            emit(ctx, node->data.literal_bool.value ? "1" : "0");
            break;
            
        case AST_IDENTIFIER:
            /* identifier sanitize করে emit। */
            emit_identifier(ctx, node->data.identifier.name);
            break;
            
        case AST_BINARY_OP: {
            /* operator string এবং আচরণ flags প্রস্তুত করি। */
            const char *op_str;
            int needs_parens = 1;
            int is_comparison = 0;
            int is_string_concat = 0;
            /* binary op metadata shortcuts। */
            Operator op = node->data.binary_op.op;
            ASTNode *left = node->data.binary_op.left;
            ASTNode *right = node->data.binary_op.right;
            
            /* Check for string concatenation */
            if (op == OP_ADD && 
                (left->data_type == TYPE_TEXT || right->data_type == TYPE_TEXT)) {
                /* text operand থাকলে '+' কে concat semantics ধরব। */
                is_string_concat = 1;
            }
            
            if (is_string_concat) {
                /* Use runtime string concatenation */
                /* nl_concat(left, right) call emit; non-text হলে আগে string-এ convert। */
                emit(ctx, "nl_concat(");
                if (left->data_type == TYPE_TEXT) {
                    codegen_expression(ctx, left);
                } else {
                    /* non-text left operand কে nl_to_string দিয়ে wrap। */
                    emit(ctx, "nl_to_string(");
                    codegen_expression(ctx, left);
                    emit(ctx, ")");
                }
                emit(ctx, ", ");
                if (right->data_type == TYPE_TEXT) {
                    codegen_expression(ctx, right);
                } else {
                    /* non-text right operand-ও string conversion করে concat। */
                    emit(ctx, "nl_to_string(");
                    codegen_expression(ctx, right);
                    emit(ctx, ")");
                }
                emit(ctx, ")");
                /* concat path শেষ, binary op switch case শেষ। */
                break;
            }
            
            /* non-concat path: operator token map। */
            switch (op) {
                case OP_ADD: op_str = "+"; break;
                case OP_SUB: op_str = "-"; break;
                case OP_MUL: op_str = "*"; break;
                case OP_DIV: op_str = "/"; break;
                case OP_MOD: op_str = "%%"; break;
                case OP_POW:
                    /* Use pow() for exponentiation */
                    /* exponentiation-এ infix নয়, pow(left, right) call emit। */
                    emit(ctx, "pow(");
                    codegen_expression(ctx, left);
                    emit(ctx, ", ");
                    codegen_expression(ctx, right);
                    emit(ctx, ")");
                    /* OP_POW case-এ immediate return; নিচের path লাগবে না। */
                    return;
                case OP_EQ:  op_str = "=="; is_comparison = 1; break;
                case OP_NEQ: op_str = "!="; is_comparison = 1; break;
                case OP_LT:  op_str = "<"; is_comparison = 1; break;
                case OP_LTE: op_str = "<="; is_comparison = 1; break;
                case OP_GT:  op_str = ">"; is_comparison = 1; break;
                case OP_GTE: op_str = ">="; is_comparison = 1; break;
                case OP_AND: op_str = "&&"; break;
                case OP_OR:  op_str = "||"; break;
                default:     op_str = "?"; break;
            }
            
            /* String comparisons need strcmp */
            if (is_comparison && 
                (left->data_type == TYPE_TEXT || right->data_type == TYPE_TEXT)) {
                /* string comparison-এ lexical compare করতে strcmp ব্যবহার। */
                emit(ctx, "(strcmp(");
                codegen_expression(ctx, left);
                emit(ctx, ", ");
                codegen_expression(ctx, right);
                emit(ctx, ") %s 0)", op_str);
            } else {
                /* সাধারণ arithmetic/logical/comparison infix expression emit। */
                if (needs_parens) emit(ctx, "(");
                codegen_expression(ctx, left);
                emit(ctx, " %s ", op_str);
                codegen_expression(ctx, right);
                if (needs_parens) emit(ctx, ")");
            }
            break;
        }
        
        case AST_UNARY_OP: {
            /* unary operator metadata extract। */
            Operator op = node->data.unary_op.op;
            ASTNode *operand = node->data.unary_op.operand;
            
            switch (op) {
                case OP_NEG:
                    /* numeric negation wrapper emit। */
                    emit(ctx, "(-");
                    codegen_expression(ctx, operand);
                    emit(ctx, ")");
                    break;
                case OP_NOT:
                    /* logical NOT wrapper emit। */
                    emit(ctx, "(!");
                    codegen_expression(ctx, operand);
                    emit(ctx, ")");
                    break;
                default:
                    /* unknown unary হলে operand as-is emit। */
                    codegen_expression(ctx, operand);
                    break;
            }
            break;
        }
            
        case AST_TERNARY_OP: {
            /* is between operator: value >= low && value <= high */
            /* between expression-এর তিন operand local alias। */
            ASTNode *operand = node->data.ternary_op.operand;
            ASTNode *lower = node->data.ternary_op.lower;
            ASTNode *upper = node->data.ternary_op.upper;
            
            emit(ctx, "((");
            codegen_expression(ctx, operand);
            emit(ctx, " >= ");
            codegen_expression(ctx, lower);
            emit(ctx, ") && (");
            codegen_expression(ctx, operand);
            emit(ctx, " <= ");
            codegen_expression(ctx, upper);
            emit(ctx, "))");
            break;
        }
            
        case AST_FUNC_CALL: {
            /* function call name emit। */
            emit_identifier(ctx, node->data.func_call.name);
            /* argument list open। */
            emit(ctx, "(");
            ASTNodeList *args = node->data.func_call.args;
            if (args) {
                /* argument list comma-separated emit। */
                for (size_t i = 0; i < args->count; i++) {
                    if (i > 0) emit(ctx, ", ");
                    codegen_expression(ctx, args->nodes[i]);
                }
            }
            /* argument list close। */
            emit(ctx, ")");
            break;
        }
        
        case AST_LIST: {
            /* list literal elements metadata। */
            ASTNodeList *elements = node->data.list_literal.elements;
            size_t count = elements ? elements->count : 0;
            /* runtime list creation call শুরু; element count first arg। */
            emit(ctx, "nl_list_create(%zu", count);
            if (elements) {
                /* প্রতিটি element অতিরিক্ত argument হিসেবে append। */
                for (size_t i = 0; i < elements->count; i++) {
                    emit(ctx, ", ");
                    codegen_expression(ctx, elements->nodes[i]);
                }
            }
            emit(ctx, ")");
            /* list runtime support লাগবে, feature flag সেট। */
            ctx->needs_list_support = 1;
            break;
        }
        
        case AST_INDEX: {
            /* list index access-কে runtime helper call-এ নামাই। */
            emit(ctx, "nl_list_get(");
            codegen_expression(ctx, node->data.index_expr.array);
            emit(ctx, ", ");
            codegen_expression(ctx, node->data.index_expr.index);
            emit(ctx, ")");
            break;
        }
        
        default:
            /* unsupported expression type -> error record + placeholder emit। */
            codegen_error(ctx, "Unknown expression node type: %d", node->type);
            emit(ctx, "/* ERROR: unknown expression */");
            break;
    }
}

/* Generate variable declaration */
static void codegen_var_decl(CodegenContext *ctx, ASTNode *node) {
    /* statement line-এর শুরুতে indentation। */
    emit_indent(ctx);
    
    /* Determine type */
    DataType type = node->data.var_decl.var_type;
    /* language type থেকে C type string map। */
    const char *c_type = naturelang_type_to_c(type);
    
    if (node->data.var_decl.is_const) {
        /* const declaration হলে qualifier emit। */
        emit(ctx, "const ");
    }
    
    emit(ctx, "%s ", c_type);
    /* declaration identifier sanitize করে emit। */
    emit_identifier(ctx, node->data.var_decl.name);
    
    /* Initialize if provided */
    if (node->data.var_decl.initializer) {
        /* explicit initializer থাকলে সেটির expression emit। */
        emit(ctx, " = ");
        codegen_expression(ctx, node->data.var_decl.initializer);
    } else {
        /* Default initialization */
        /* initializer না থাকলে type অনুযায়ী safe default দিই। */
        switch (type) {
            case TYPE_NUMBER:
            case TYPE_DECIMAL:
                emit(ctx, " = 0");
                break;
            case TYPE_TEXT:
                emit(ctx, " = \"\"");
                break;
            case TYPE_FLAG:
                emit(ctx, " = 0");
                break;
            default:
                break;
        }
    }
    
    emit(ctx, ";\n");
}

/* Generate assignment */
static void codegen_assignment(CodegenContext *ctx, ASTNode *node) {
    /* assignment statement indentation। */
    emit_indent(ctx);
    /* target lvalue emit। */
    codegen_expression(ctx, node->data.assign.target);
    emit(ctx, " = ");
    /* rhs expression emit। */
    codegen_expression(ctx, node->data.assign.value);
    /* statement terminate। */
    emit(ctx, ";\n");
}

/* Generate display statement */
static void codegen_display(CodegenContext *ctx, ASTNode *node) {
    /* display statement indentation। */
    emit_indent(ctx);
    
    /* display value operand বের করি। */
    ASTNode *value = node->data.display_stmt.value;
    if (!value) {
        /* value না থাকলে শুধু newline print। */
        emit(ctx, "printf(\"\\n\");\n");
        return;
    }
    
    /* Determine format based on expression type */
    DataType type = value->data_type;
    
    /* value type অনুযায়ী format string নির্বাচন। */
    switch (type) {
        case TYPE_NUMBER:
            emit(ctx, "printf(\"%%lld\\n\", (long long)");
            codegen_expression(ctx, value);
            emit(ctx, ");\n");
            break;
        case TYPE_DECIMAL:
            emit(ctx, "printf(\"%%g\\n\", (double)");
            codegen_expression(ctx, value);
            emit(ctx, ");\n");
            break;
        case TYPE_TEXT:
            emit(ctx, "printf(\"%%s\\n\", ");
            codegen_expression(ctx, value);
            emit(ctx, ");\n");
            break;
        case TYPE_FLAG:
            /* boolean true/false কে yes/no text হিসেবে দেখাই। */
            emit(ctx, "printf(\"%%s\\n\", ");
            codegen_expression(ctx, value);
            emit(ctx, " ? \"yes\" : \"no\");\n");
            break;
        default:
            /* unknown/custom type হলে runtime display helper fallback। */
            emit(ctx, "nl_display(");
            codegen_expression(ctx, value);
            emit(ctx, ");\n");
            break;
    }
}

/* Generate ask statement (input with prompt) */
static void codegen_ask(CodegenContext *ctx, ASTNode *node) {
    /* ask/read path-এ shared input buffer দরকার। */
    ctx->needs_input_buffer = 1;
    /* current line indentation। */
    emit_indent(ctx);
    
    /* Get target variable type */
    DataType type = TYPE_TEXT;  /* Default */
    /* target variable identifier। */
    const char *target = node->data.ask_stmt.target_var;
    /* symbol table থেকে target এর declared type খুঁজি। */
    Symbol *sym = symtab_lookup(ctx->symtab, target);
    if (sym) {
        /* symbol পাওয়া গেলে actual type override। */
        type = sym->type;
    }
    
    /* Print prompt if provided */
    if (node->data.ask_stmt.prompt) {
        /* prompt থাকলে আগে সেটি print করে flush করি। */
        emit(ctx, "printf(\"%%s\", ");
        codegen_expression(ctx, node->data.ask_stmt.prompt);
        emit(ctx, "); fflush(stdout);\n");
        /* read statement লাইনের জন্য পুনরায় indentation। */
        emit_indent(ctx);
    }
    
    /* Read input based on type */
    switch (type) {
        case TYPE_NUMBER:
            /* string input পড়ে atoll দিয়ে integer parse। */
            emit(ctx, "fgets(_nl_input_buffer, sizeof(_nl_input_buffer), stdin); ");
            emit_identifier(ctx, target);
            emit(ctx, " = atoll(_nl_input_buffer);\n");
            break;
        case TYPE_DECIMAL:
            /* decimal target হলে atof parse ব্যবহার। */
            emit(ctx, "fgets(_nl_input_buffer, sizeof(_nl_input_buffer), stdin); ");
            emit_identifier(ctx, target);
            emit(ctx, " = atof(_nl_input_buffer);\n");
            break;
        case TYPE_TEXT:
            /* text input newline trim করে strdup দিয়ে assign। */
            emit(ctx, "fgets(_nl_input_buffer, sizeof(_nl_input_buffer), stdin); ");
            emit(ctx, "_nl_input_buffer[strcspn(_nl_input_buffer, \"\\n\")] = 0; ");
            emit_identifier(ctx, target);
            emit(ctx, " = strdup(_nl_input_buffer);\n");
            break;
        default:
            /* fallback: raw input read, conversion ছাড়া। */
            emit(ctx, "fgets(_nl_input_buffer, sizeof(_nl_input_buffer), stdin);\n");
            break;
    }
}

/* Generate read statement (simple input) */
static void codegen_read(CodegenContext *ctx, ASTNode *node) {
    /* read statement-এও shared input buffer প্রয়োজন। */
    ctx->needs_input_buffer = 1;
    /* generated line indentation। */
    emit_indent(ctx);
    
    const char *target = node->data.read_stmt.target_var;
    
    /* Get target variable type */
    DataType type = TYPE_TEXT;  /* Default */
    /* target variable type symbol table থেকে resolve। */
    Symbol *sym = symtab_lookup(ctx->symtab, target);
    if (sym) {
        type = sym->type;
    }
    
    /* Read input based on type */
    switch (type) {
        case TYPE_NUMBER:
            /* integer parse path। */
            emit(ctx, "fgets(_nl_input_buffer, sizeof(_nl_input_buffer), stdin); ");
            emit_identifier(ctx, target);
            emit(ctx, " = atoll(_nl_input_buffer);\n");
            break;
        case TYPE_DECIMAL:
            /* decimal parse path। */
            emit(ctx, "fgets(_nl_input_buffer, sizeof(_nl_input_buffer), stdin); ");
            emit_identifier(ctx, target);
            emit(ctx, " = atof(_nl_input_buffer);\n");
            break;
        case TYPE_TEXT:
            /* text path: newline strip + strdup assignment। */
            emit(ctx, "fgets(_nl_input_buffer, sizeof(_nl_input_buffer), stdin); ");
            emit(ctx, "_nl_input_buffer[strcspn(_nl_input_buffer, \"\\n\")] = 0; ");
            emit_identifier(ctx, target);
            emit(ctx, " = strdup(_nl_input_buffer);\n");
            break;
        default:
            /* unknown type fallback raw read। */
            emit(ctx, "fgets(_nl_input_buffer, sizeof(_nl_input_buffer), stdin);\n");
            break;
    }
}

/* Generate if statement */
static void codegen_if(CodegenContext *ctx, ASTNode *node) {
    /* if statement line indentation। */
    emit_indent(ctx);
    /* condition expression সহ if header emit। */
    emit(ctx, "if (");
    codegen_expression(ctx, node->data.if_stmt.condition);
    emit(ctx, ") {\n");
    
    /* then-branch block-এ ঢুকে indentation বাড়াই। */
    ctx->indent_level++;
    if (node->data.if_stmt.then_branch) {
        /* then অংশ থাকলে recursive node generation। */
        codegen_node(ctx, node->data.if_stmt.then_branch);
    }
    /* then-branch শেষ, indentation restore। */
    ctx->indent_level--;
    
    /* then block close brace। */
    emit_indent(ctx);
    emit(ctx, "}");
    
    if (node->data.if_stmt.else_branch) {
        /* else branch থাকলে else block header emit। */
        emit(ctx, " else {\n");
        ctx->indent_level++;
        codegen_node(ctx, node->data.if_stmt.else_branch);
        ctx->indent_level--;
        emit_indent(ctx);
        emit(ctx, "}");
    }
    /* if statement সম্পূর্ণ করতে newline। */
    emit(ctx, "\n");
}

/* Generate while loop */
static void codegen_while(CodegenContext *ctx, ASTNode *node) {
    /* while header indentation। */
    emit_indent(ctx);
    /* condition expression সহ while loop header emit। */
    emit(ctx, "while (");
    codegen_expression(ctx, node->data.while_stmt.condition);
    emit(ctx, ") {\n");
    
    /* loop body scope begin। */
    ctx->indent_level++;
    /* nested loop tracking counter increase। */
    ctx->in_loop++;
    if (node->data.while_stmt.body) {
        codegen_node(ctx, node->data.while_stmt.body);
    }
    /* loop body শেষ; loop state ও indentation restore। */
    ctx->in_loop--;
    ctx->indent_level--;
    
    /* while closing brace emit। */
    emit_indent(ctx);
    emit(ctx, "}\n");
}

/* Generate repeat loop */
static void codegen_repeat(CodegenContext *ctx, ASTNode *node) {
    /* repeat construct wrapper indentation। */
    emit_indent(ctx);
    
    /* helper temp vars: iterator এবং limit। */
    char *iter_var = codegen_temp_var(ctx);
    char *limit_var = codegen_temp_var(ctx);
    
    /* local scope শুরু যাতে temp vars scoped থাকে। */
    emit(ctx, "{\n");
    ctx->indent_level++;
    
    /* repeat count expression evaluate করে limit temp-এ রাখি। */
    emit_indent(ctx);
    emit(ctx, "long long %s = ", limit_var);
    codegen_expression(ctx, node->data.repeat_stmt.count);
    emit(ctx, ";\n");
    
    /* counted for-loop emit: 0 থেকে limit পর্যন্ত। */
    emit_indent(ctx);
    emit(ctx, "for (long long %s = 0; %s < %s; %s++) {\n", 
         iter_var, iter_var, limit_var, iter_var);
    
    /* loop body context setup। */
    ctx->indent_level++;
    ctx->in_loop++;
    if (node->data.repeat_stmt.body) {
        codegen_node(ctx, node->data.repeat_stmt.body);
    }
    /* loop body context teardown। */
    ctx->in_loop--;
    ctx->indent_level--;
    
    /* for loop close। */
    emit_indent(ctx);
    emit(ctx, "}\n");
    
    /* wrapper scope close। */
    ctx->indent_level--;
    emit_indent(ctx);
    emit(ctx, "}\n");
    
    /* temp name buffers মুক্ত করি। */
    free(iter_var);
    free(limit_var);
}

/* Generate foreach loop */
static void codegen_foreach(CodegenContext *ctx, ASTNode *node) {
    /* foreach wrapper indentation। */
    emit_indent(ctx);
    
    /* Generate iteration over list */
    char *iter_var = codegen_temp_var(ctx);
    char *list_var = codegen_temp_var(ctx);
    
    /* foreach translation-কে scoped block-এ রাখি। */
    emit(ctx, "{\n");
    ctx->indent_level++;
    
    /* iterable expression evaluate করে list temp-এ store। */
    emit_indent(ctx);
    emit(ctx, "NLList* %s = ", list_var);
    codegen_expression(ctx, node->data.for_each_stmt.iterable);
    emit(ctx, ";\n");
    
    /* index-based traversal loop emit। */
    emit_indent(ctx);
    emit(ctx, "for (int %s = 0; %s < %s->length; %s++) {\n", 
         iter_var, iter_var, list_var, iter_var);
    
    ctx->indent_level++;
    
    /* বর্তমান element iterator variable-এ bind (numeric assumption path)। */
    emit_indent(ctx);
    emit(ctx, "long long ");  /* Assume numeric for now */
    emit_identifier(ctx, node->data.for_each_stmt.iterator_name);
    emit(ctx, " = nl_list_get_num(%s, %s);\n", list_var, iter_var);
    
    /* foreach body emit। */
    ctx->in_loop++;
    if (node->data.for_each_stmt.body) {
        codegen_node(ctx, node->data.for_each_stmt.body);
    }
    ctx->in_loop--;
    
    ctx->indent_level--;
    emit_indent(ctx);
    emit(ctx, "}\n");
    
    ctx->indent_level--;
    emit_indent(ctx);
    emit(ctx, "}\n");
    
    /* allocated temp names cleanup। */
    free(iter_var);
    free(list_var);
}

/* Generate function declaration */
static void codegen_function(CodegenContext *ctx, ASTNode *node) {
    /* Return type */
    /* function return datatype থেকে C return type resolve। */
    const char *ret_type = naturelang_type_to_c(node->data.func_decl.return_type);
    emit(ctx, "%s ", ret_type);
    /* function name sanitize করে emit। */
    emit_identifier(ctx, node->data.func_decl.name);
    /* signature parameter list open। */
    emit(ctx, "(");
    
    /* Parameters */
    ASTNodeList *params = node->data.func_decl.params;
    if (params && params->count > 0) {
        /* parameter declarations comma-separated emit। */
        for (size_t i = 0; i < params->count; i++) {
            if (i > 0) emit(ctx, ", ");
            ASTNode *param = params->nodes[i];
            const char *param_type = naturelang_type_to_c(param->data.param_decl.param_type);
            emit(ctx, "%s ", param_type);
            emit_identifier(ctx, param->data.param_decl.name);
        }
    } else {
        /* no-arg function হলে explicit void parameter list। */
        emit(ctx, "void");
    }
    
    /* function body start। */
    emit(ctx, ") {\n");
    
    /* function scope state setup। */
    ctx->indent_level++;
    ctx->in_function = 1;
    
    if (node->data.func_decl.body) {
        /* function body AST recursively emit। */
        codegen_node(ctx, node->data.func_decl.body);
    }
    
    /* function scope state restore। */
    ctx->in_function = 0;
    ctx->indent_level--;
    
    /* function closing brace + extra newline। */
    emit(ctx, "}\n\n");
}

/* Generate return statement */
static void codegen_return(CodegenContext *ctx, ASTNode *node) {
    /* return statement indentation। */
    emit_indent(ctx);
    if (node->data.return_stmt.value) {
        /* value সহ return। */
        emit(ctx, "return ");
        codegen_expression(ctx, node->data.return_stmt.value);
        emit(ctx, ";\n");
    } else {
        /* void return। */
        emit(ctx, "return;\n");
    }
}

/* Generate break statement */
static void codegen_break(CodegenContext *ctx, ASTNode *node) {
    (void)node;  /* Unused */
    /* break statement indentation + emit। */
    emit_indent(ctx);
    emit(ctx, "break;\n");
}

/* Generate continue statement */
static void codegen_continue(CodegenContext *ctx, ASTNode *node) {
    (void)node;  /* Unused */
    /* continue statement indentation + emit। */
    emit_indent(ctx);
    emit(ctx, "continue;\n");
}

/* Generate secure zone (no special C handling, just emit body) */
static void codegen_secure_zone(CodegenContext *ctx, ASTNode *node) {
    /* option enabled হলে secure zone start comment emit। */
    if (ctx->options.emit_comments) {
        emit_indent(ctx);
        emit(ctx, "/* BEGIN SECURE ZONE */\n");
    }
    
    /* secure zone body wrapper block open। */
    emit_indent(ctx);
    emit(ctx, "{\n");
    ctx->indent_level++;
    
    if (node->data.secure_zone.body) {
        /* secure zone ভেতরের statements/code emit। */
        codegen_node(ctx, node->data.secure_zone.body);
    }
    
    ctx->indent_level--;
    emit_indent(ctx);
    emit(ctx, "}\n");
    
    /* option enabled হলে secure zone end comment emit। */
    if (ctx->options.emit_comments) {
        emit_indent(ctx);
        emit(ctx, "/* END SECURE ZONE */\n");
    }
}

/* Generate expression statement */
static void codegen_expr_stmt(CodegenContext *ctx, ASTNode *node) {
    /* standalone expression statement emit। */
    emit_indent(ctx);
    codegen_expression(ctx, node->data.expr_stmt.expr);
    emit(ctx, ";\n");
}

/* Generate a single statement */
static void codegen_statement(CodegenContext *ctx, ASTNode *node) {
    /* null statement guard। */
    if (!node) return;
    
    /* statement node type অনুযায়ী নির্দিষ্ট generator dispatch। */
    switch (node->type) {
        case AST_VAR_DECL:
            codegen_var_decl(ctx, node);
            break;
        case AST_ASSIGN:
            codegen_assignment(ctx, node);
            break;
        case AST_DISPLAY:
            codegen_display(ctx, node);
            break;
        case AST_ASK:
            codegen_ask(ctx, node);
            break;
        case AST_READ:
            codegen_read(ctx, node);
            break;
        case AST_IF:
            codegen_if(ctx, node);
            break;
        case AST_WHILE:
            codegen_while(ctx, node);
            break;
        case AST_REPEAT:
            codegen_repeat(ctx, node);
            break;
        case AST_FOR_EACH:
            codegen_foreach(ctx, node);
            break;
        case AST_FUNC_DECL:
            codegen_function(ctx, node);
            break;
        case AST_RETURN:
            codegen_return(ctx, node);
            break;
        case AST_BREAK:
            codegen_break(ctx, node);
            break;
        case AST_CONTINUE:
            codegen_continue(ctx, node);
            break;
        case AST_SECURE_ZONE:
            codegen_secure_zone(ctx, node);
            break;
        case AST_EXPR_STMT:
            codegen_expr_stmt(ctx, node);
            break;
        case AST_BLOCK: {
            /* block statement হলে child statements sequentially emit। */
            ASTNodeList *stmts = node->data.block.statements;
            if (stmts) {
                for (size_t i = 0; i < stmts->count; i++) {
                    codegen_statement(ctx, stmts->nodes[i]);
                }
            }
            break;
        }
        default:
            /* Try as expression */
            /* fallback path: unknown statement type-কে expression statement হিসেবে চেষ্টা। */
            emit_indent(ctx);
            codegen_expression(ctx, node);
            emit(ctx, ";\n");
            break;
    }
}

/* Generate node - dispatcher */
static void codegen_node(CodegenContext *ctx, ASTNode *node) {
    /* null node guard। */
    if (!node) return;
    
    /* node category অনুযায়ী traversal strategy নির্বাচন। */
    if (node->type == AST_PROGRAM) {
        ASTNodeList *stmts = node->data.program.statements;
        if (stmts) {
            /* program root-এর প্রতিটি top-level statement emit। */
            for (size_t i = 0; i < stmts->count; i++) {
                codegen_statement(ctx, stmts->nodes[i]);
            }
        }
    } else if (node->type == AST_BLOCK) {
        ASTNodeList *stmts = node->data.block.statements;
        if (stmts) {
            /* block child statements emit। */
            for (size_t i = 0; i < stmts->count; i++) {
                codegen_statement(ctx, stmts->nodes[i]);
            }
        }
    } else {
        /* non-container node হলে statement হিসেবে handle। */
        codegen_statement(ctx, node);
    }
}

/* Collect function declarations for forward declarations */
static void collect_functions(ASTNode *node, ASTNode **funcs, int *count, int max) {
    /* recursion guard: null node হলে return। */
    if (!node) return;
    
    /* function declaration পেলে array-তে collect (capacity limit সহ)। */
    if (node->type == AST_FUNC_DECL && *count < max) {
        funcs[(*count)++] = node;
    }
    
    if (node->type == AST_PROGRAM) {
        ASTNodeList *stmts = node->data.program.statements;
        if (stmts) {
            /* program children recursion। */
            for (size_t i = 0; i < stmts->count; i++) {
                collect_functions(stmts->nodes[i], funcs, count, max);
            }
        }
    } else if (node->type == AST_BLOCK) {
        ASTNodeList *stmts = node->data.block.statements;
        if (stmts) {
            /* block children recursion। */
            for (size_t i = 0; i < stmts->count; i++) {
                collect_functions(stmts->nodes[i], funcs, count, max);
            }
        }
    }
}

/* Emit forward declarations */
static void emit_forward_declarations(CodegenContext *ctx, ASTNode *ast) {
    /* function declaration pointers রাখার fixed-size scratch array। */
    ASTNode *funcs[100];
    /* collected function count। */
    int count = 0;
    
    /* AST traverse করে function declarations collect। */
    collect_functions(ast, funcs, &count, 100);
    
    if (count > 0) {
        /* forward declaration section header। */
        emit_line(ctx, "/* Forward declarations */");
        for (int i = 0; i < count; i++) {
            /* প্রতিটি collected function prototype emit। */
            ASTNode *func = funcs[i];
            const char *ret_type = naturelang_type_to_c(func->data.func_decl.return_type);
            emit_indent(ctx);
            emit(ctx, "%s ", ret_type);
            emit_identifier(ctx, func->data.func_decl.name);
            emit(ctx, "(");
            
            ASTNodeList *params = func->data.func_decl.params;
            if (params && params->count > 0) {
                /* prototype parameter list emit। */
                for (size_t j = 0; j < params->count; j++) {
                    if (j > 0) emit(ctx, ", ");
                    ASTNode *param = params->nodes[j];
                    const char *param_type = naturelang_type_to_c(param->data.param_decl.param_type);
                    emit(ctx, "%s ", param_type);
                    emit_identifier(ctx, param->data.param_decl.name);
                }
            } else {
                /* parameter না থাকলে void emit। */
                emit(ctx, "void");
            }
            
            /* prototype শেষ। */
            emit(ctx, ");\n");
        }
        /* section শেষে blank line। */
        emit_newline(ctx);
    }
}

/* Generate main function wrapper */
static void emit_main_wrapper(CodegenContext *ctx, ASTNode *ast, int has_user_main) {
    /* user main থাকলে generated main শুধু wrapper/caller হিসেবে কাজ করবে। */
    if (has_user_main) {
        /* User defined their own main, just call it */
        emit_line(ctx, "int main(int argc, char *argv[]) {");
        emit_line(ctx, "    (void)argc; (void)argv;");
        emit_line(ctx, "    nl_main();");
        emit_line(ctx, "    return 0;");
        emit_line(ctx, "}");
    } else {
        /* Wrap top-level code in main */
        /* user main না থাকলে top-level statements main-এর ভিতরে নামাই। */
        emit_line(ctx, "int main(int argc, char *argv[]) {");
        emit_line(ctx, "    (void)argc; (void)argv;");
        ctx->indent_level++;
        
        /* Generate top-level statements (skip function declarations) */
        if (ast->type == AST_PROGRAM) {
            ASTNodeList *stmts = ast->data.program.statements;
            if (stmts) {
                /* function decl বাদ দিয়ে executable top-level statements emit। */
                for (size_t i = 0; i < stmts->count; i++) {
                    ASTNode *stmt = stmts->nodes[i];
                    if (stmt->type != AST_FUNC_DECL) {
                        codegen_statement(ctx, stmt);
                    }
                }
            }
        }
        
        /* main footer return + close। */
        ctx->indent_level--;
        emit_line(ctx, "    return 0;");
        emit_line(ctx, "}");
    }
}

/* Check if there's a main function */
static int has_main_function(ASTNode *ast) {
    /* null AST হলে main পাওয়া যাবে না। */
    if (!ast) return 0;
    
    /* current node direct main declaration কিনা check। */
    if (ast->type == AST_FUNC_DECL && 
        ast->data.func_decl.name && 
        strcmp(ast->data.func_decl.name, "main") == 0) {
        return 1;
    }
    
    /* program children-এ recursive main search। */
    if (ast->type == AST_PROGRAM) {
        ASTNodeList *stmts = ast->data.program.statements;
        if (stmts) {
            for (size_t i = 0; i < stmts->count; i++) {
                if (has_main_function(stmts->nodes[i])) {
                    return 1;
                }
            }
        }
    }
    
    /* কোথাও main না পেলে false। */
    return 0;
}

/* Main code generation entry point */
CodegenResult codegen_generate(CodegenContext *ctx, ASTNode *ast) {
    /* API result object zero-init। */
    CodegenResult result = {0};
    
    /* invalid input guard। */
    if (!ctx || !ast) {
        result.success = 0;
        result.error_message = strdup("Invalid context or AST");
        return result;
    }
    
    /* Reset buffer */
    /* পূর্বের generation output মুছে fresh emit শুরু। */
    ctx->buffer_size = 0;
    ctx->buffer[0] = '\0';
    
    /* First pass: scan for features used */
    /* (This could be expanded to detect input/list usage) */
    /* আপাতত input buffer সবসময় include করা হচ্ছে। */
    ctx->needs_input_buffer = 1;  /* Always include for now */
    
    /* Emit headers */
    emit_headers(ctx);
    
    /* Emit global variables/buffers */
    emit_input_buffer(ctx);
    
    /* Emit forward declarations */
    emit_forward_declarations(ctx, ast);
    
    /* Check for user-defined main */
    /* user-provided main আছে কিনা detect। */
    int user_main = has_main_function(ast);
    
    /* Generate function declarations first */
    if (ast->type == AST_PROGRAM) {
        ASTNodeList *stmts = ast->data.program.statements;
        if (stmts) {
            for (size_t i = 0; i < stmts->count; i++) {
                ASTNode *stmt = stmts->nodes[i];
                if (stmt->type == AST_FUNC_DECL) {
                    /* Rename main to nl_main if user defined it */
                    /* user main থাকলে symbol clash এড়াতে সাময়িক rename করে emit। */
                    char *func_name = stmt->data.func_decl.name;
                    if (func_name && strcmp(func_name, "main") == 0) {
                        char *old_name = func_name;
                        stmt->data.func_decl.name = strdup("nl_main");
                        codegen_function(ctx, stmt);
                        /* temporary name cleanup করে original AST name restore। */
                        free(stmt->data.func_decl.name);
                        stmt->data.func_decl.name = old_name;
                    } else {
                        /* non-main function normal emission path। */
                        codegen_function(ctx, stmt);
                    }
                }
            }
        }
    }
    
    /* Generate main wrapper */
    emit_main_wrapper(ctx, ast, user_main);
    
    /* Prepare result */
    /* error count শূন্য হলে success true। */
    result.success = (ctx->error_count == 0);
    /* generated source caller-owned buffer-এ copy। */
    result.generated_code = strdup(ctx->buffer);
    /* output length metadata। */
    result.code_length = ctx->buffer_size;
    /* মোট error সংখ্যা result-এ expose। */
    result.error_count = ctx->error_count;
    if (ctx->error_count > 0) {
        /* error থাকলে latest message result-এ copy। */
        result.error_message = strdup(ctx->error_message);
    }
    
    return result;
}

/* Generate code to file */
int codegen_to_file(CodegenContext *ctx, ASTNode *ast, const char *filename) {
    /* প্রথমে in-memory generation চালাই। */
    CodegenResult result = codegen_generate(ctx, ast);
    
    /* generation fail হলে result buffers cleanup করে fail ফেরত। */
    if (!result.success) {
        free(result.generated_code);
        free(result.error_message);
        return 0;
    }
    
    /* output file write-mode-এ open। */
    FILE *f = fopen(filename, "w");
    if (!f) {
        /* file open fail হলে generated buffer free করে fail। */
        free(result.generated_code);
        return 0;
    }
    
    /* generated source ফাইলে লিখে close করি। */
    fputs(result.generated_code, f);
    fclose(f);
    
    /* temporary generated buffer মুক্ত করি। */
    free(result.generated_code);
    /* সফল write signal। */
    return 1;
}
