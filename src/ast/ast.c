/*
 * NatureLang Compiler
 * Copyright (c) 2026
 * 
 * AST Implementation
 * 
 * Implementation of AST node creation, manipulation, and utilities.
 */

#define _POSIX_C_SOURCE 200809L
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * HELPER FUNCTIONS
 * ============================================================================
 */

static void *safe_malloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory (%zu bytes)\n", size);
        exit(1);
    }
    memset(ptr, 0, size);
    return ptr;
}

static char *safe_strdup(const char *str) {
    if (str == NULL) return NULL;
    char *dup = strdup(str);
    if (dup == NULL) {
        fprintf(stderr, "Error: Failed to duplicate string\n");
        exit(1);
    }
    return dup;
}

static ASTNode *create_node(ASTNodeType type, SourceLocation loc) {
    ASTNode *node = (ASTNode *)safe_malloc(sizeof(ASTNode));
    node->type = type;
    node->loc = loc;
    node->data_type = TYPE_UNKNOWN;
    return node;
}

/* ============================================================================
 * NODE LIST IMPLEMENTATION
 * ============================================================================
 */

#define INITIAL_LIST_CAPACITY 8

ASTNodeList *ast_node_list_create(void) {
    ASTNodeList *list = (ASTNodeList *)safe_malloc(sizeof(ASTNodeList));
    list->nodes = (ASTNode **)safe_malloc(sizeof(ASTNode *) * INITIAL_LIST_CAPACITY);
    list->count = 0;
    list->capacity = INITIAL_LIST_CAPACITY;
    return list;
}

void ast_node_list_append(ASTNodeList *list, ASTNode *node) {
    if (list == NULL) return;
    
    if (list->count >= list->capacity) {
        list->capacity *= 2;
        list->nodes = (ASTNode **)realloc(list->nodes, 
                                          sizeof(ASTNode *) * list->capacity);
        if (list->nodes == NULL) {
            fprintf(stderr, "Error: Failed to grow node list\n");
            exit(1);
        }
    }
    
    list->nodes[list->count++] = node;
}

void ast_node_list_free(ASTNodeList *list) {
    if (list == NULL) return;
    
    for (size_t i = 0; i < list->count; i++) {
        ast_free(list->nodes[i]);
    }
    
    free(list->nodes);
    free(list);
}

/* ============================================================================
 * PROGRAM
 * ============================================================================
 */

ASTNode *ast_create_program(ASTNodeList *statements, SourceLocation loc) {
    ASTNode *node = create_node(AST_PROGRAM, loc);
    node->data.program.statements = statements;
    return node;
}

/* ============================================================================
 * DECLARATIONS
 * ============================================================================
 */

ASTNode *ast_create_var_decl(const char *name, DataType type, ASTNode *init,
                              int is_const, SourceLocation loc) {
    ASTNode *node = create_node(AST_VAR_DECL, loc);
    node->data.var_decl.name = safe_strdup(name);
    node->data.var_decl.var_type = type;
    node->data.var_decl.initializer = init;
    node->data.var_decl.is_const = is_const;
    node->data_type = type;
    return node;
}

ASTNode *ast_create_func_decl(const char *name, ASTNodeList *params,
                               DataType return_type, ASTNode *body, SourceLocation loc) {
    ASTNode *node = create_node(AST_FUNC_DECL, loc);
    node->data.func_decl.name = safe_strdup(name);
    node->data.func_decl.params = params;
    node->data.func_decl.return_type = return_type;
    node->data.func_decl.body = body;
    node->data_type = TYPE_FUNCTION;
    return node;
}

ASTNode *ast_create_param_decl(const char *name, DataType type, SourceLocation loc) {
    ASTNode *node = create_node(AST_PARAM_DECL, loc);
    node->data.param_decl.name = safe_strdup(name);
    node->data.param_decl.param_type = type;
    node->data_type = type;
    return node;
}

/* ============================================================================
 * STATEMENTS
 * ============================================================================
 */

ASTNode *ast_create_block(ASTNodeList *statements, SourceLocation loc) {
    ASTNode *node = create_node(AST_BLOCK, loc);
    node->data.block.statements = statements;
    return node;
}

ASTNode *ast_create_assign(ASTNode *target, ASTNode *value, SourceLocation loc) {
    ASTNode *node = create_node(AST_ASSIGN, loc);
    node->data.assign.target = target;
    node->data.assign.value = value;
    return node;
}

ASTNode *ast_create_if(ASTNode *cond, ASTNode *then_branch, ASTNode *else_branch,
                        SourceLocation loc) {
    ASTNode *node = create_node(AST_IF, loc);
    node->data.if_stmt.condition = cond;
    node->data.if_stmt.then_branch = then_branch;
    node->data.if_stmt.else_branch = else_branch;
    return node;
}

ASTNode *ast_create_while(ASTNode *cond, ASTNode *body, SourceLocation loc) {
    ASTNode *node = create_node(AST_WHILE, loc);
    node->data.while_stmt.condition = cond;
    node->data.while_stmt.body = body;
    return node;
}

ASTNode *ast_create_repeat(ASTNode *count, ASTNode *body, SourceLocation loc) {
    ASTNode *node = create_node(AST_REPEAT, loc);
    node->data.repeat_stmt.count = count;
    node->data.repeat_stmt.body = body;
    return node;
}

ASTNode *ast_create_for_each(const char *iter_name, ASTNode *iterable,
                              ASTNode *body, SourceLocation loc) {
    ASTNode *node = create_node(AST_FOR_EACH, loc);
    node->data.for_each_stmt.iterator_name = safe_strdup(iter_name);
    node->data.for_each_stmt.iterable = iterable;
    node->data.for_each_stmt.body = body;
    return node;
}

ASTNode *ast_create_return(ASTNode *value, SourceLocation loc) {
    ASTNode *node = create_node(AST_RETURN, loc);
    node->data.return_stmt.value = value;
    return node;
}

ASTNode *ast_create_break(SourceLocation loc) {
    return create_node(AST_BREAK, loc);
}

ASTNode *ast_create_continue(SourceLocation loc) {
    return create_node(AST_CONTINUE, loc);
}

ASTNode *ast_create_expr_stmt(ASTNode *expr, SourceLocation loc) {
    ASTNode *node = create_node(AST_EXPR_STMT, loc);
    node->data.expr_stmt.expr = expr;
    return node;
}

/* ============================================================================
 * I/O STATEMENTS
 * ============================================================================
 */

ASTNode *ast_create_display(ASTNode *value, SourceLocation loc) {
    ASTNode *node = create_node(AST_DISPLAY, loc);
    node->data.display_stmt.value = value;
    return node;
}

ASTNode *ast_create_ask(ASTNode *prompt, const char *target_var, SourceLocation loc) {
    ASTNode *node = create_node(AST_ASK, loc);
    node->data.ask_stmt.prompt = prompt;
    node->data.ask_stmt.target_var = safe_strdup(target_var);
    return node;
}

ASTNode *ast_create_read(const char *target_var, SourceLocation loc) {
    ASTNode *node = create_node(AST_READ, loc);
    node->data.read_stmt.target_var = safe_strdup(target_var);
    return node;
}

/* ============================================================================
 * SECURE ZONE
 * ============================================================================
 */

ASTNode *ast_create_secure_zone(ASTNode *body, int is_safe, SourceLocation loc) {
    ASTNode *node = create_node(AST_SECURE_ZONE, loc);
    node->data.secure_zone.body = body;
    node->data.secure_zone.is_safe = is_safe;
    return node;
}

/* ============================================================================
 * EXPRESSIONS
 * ============================================================================
 */

ASTNode *ast_create_binary_op(Operator op, ASTNode *left, ASTNode *right,
                               SourceLocation loc) {
    ASTNode *node = create_node(AST_BINARY_OP, loc);
    node->data.binary_op.op = op;
    node->data.binary_op.left = left;
    node->data.binary_op.right = right;
    return node;
}

ASTNode *ast_create_unary_op(Operator op, ASTNode *operand, SourceLocation loc) {
    ASTNode *node = create_node(AST_UNARY_OP, loc);
    node->data.unary_op.op = op;
    node->data.unary_op.operand = operand;
    return node;
}

ASTNode *ast_create_ternary_op(Operator op, ASTNode *operand, ASTNode *lower, 
                                ASTNode *upper, SourceLocation loc) {
    ASTNode *node = create_node(AST_TERNARY_OP, loc);
    node->data.ternary_op.op = op;
    node->data.ternary_op.operand = operand;
    node->data.ternary_op.lower = lower;
    node->data.ternary_op.upper = upper;
    node->data_type = TYPE_FLAG;  /* "is between" returns boolean */
    return node;
}

ASTNode *ast_create_literal_int(long long value, SourceLocation loc) {
    ASTNode *node = create_node(AST_LITERAL_INT, loc);
    node->data.literal_int.value = value;
    node->data_type = TYPE_NUMBER;
    return node;
}

ASTNode *ast_create_literal_float(double value, SourceLocation loc) {
    ASTNode *node = create_node(AST_LITERAL_FLOAT, loc);
    node->data.literal_float.value = value;
    node->data_type = TYPE_DECIMAL;
    return node;
}

ASTNode *ast_create_literal_string(const char *value, SourceLocation loc) {
    ASTNode *node = create_node(AST_LITERAL_STRING, loc);
    node->data.literal_string.value = safe_strdup(value);
    node->data_type = TYPE_TEXT;
    return node;
}

ASTNode *ast_create_literal_bool(int value, SourceLocation loc) {
    ASTNode *node = create_node(AST_LITERAL_BOOL, loc);
    node->data.literal_bool.value = value ? 1 : 0;
    node->data_type = TYPE_FLAG;
    return node;
}

ASTNode *ast_create_identifier(const char *name, SourceLocation loc) {
    ASTNode *node = create_node(AST_IDENTIFIER, loc);
    node->data.identifier.name = safe_strdup(name);
    return node;
}

ASTNode *ast_create_func_call(const char *name, ASTNodeList *args, SourceLocation loc) {
    ASTNode *node = create_node(AST_FUNC_CALL, loc);
    node->data.func_call.name = safe_strdup(name);
    node->data.func_call.args = args;
    return node;
}

ASTNode *ast_create_index(ASTNode *array, ASTNode *index, SourceLocation loc) {
    ASTNode *node = create_node(AST_INDEX, loc);
    node->data.index_expr.array = array;
    node->data.index_expr.index = index;
    return node;
}

ASTNode *ast_create_list(ASTNodeList *elements, SourceLocation loc) {
    ASTNode *node = create_node(AST_LIST, loc);
    node->data.list_literal.elements = elements;
    node->data_type = TYPE_LIST;
    return node;
}

/* ============================================================================
 * AST FREE
 * ============================================================================
 */

void ast_free(ASTNode *node) {
    if (node == NULL) return;
    
    switch (node->type) {
        case AST_PROGRAM:
            ast_node_list_free(node->data.program.statements);
            break;
            
        case AST_VAR_DECL:
            free(node->data.var_decl.name);
            ast_free(node->data.var_decl.initializer);
            break;
            
        case AST_FUNC_DECL:
            free(node->data.func_decl.name);
            ast_node_list_free(node->data.func_decl.params);
            ast_free(node->data.func_decl.body);
            break;
            
        case AST_PARAM_DECL:
            free(node->data.param_decl.name);
            break;
            
        case AST_BLOCK:
            ast_node_list_free(node->data.block.statements);
            break;
            
        case AST_ASSIGN:
            ast_free(node->data.assign.target);
            ast_free(node->data.assign.value);
            break;
            
        case AST_IF:
            ast_free(node->data.if_stmt.condition);
            ast_free(node->data.if_stmt.then_branch);
            ast_free(node->data.if_stmt.else_branch);
            break;
            
        case AST_WHILE:
            ast_free(node->data.while_stmt.condition);
            ast_free(node->data.while_stmt.body);
            break;
            
        case AST_REPEAT:
            ast_free(node->data.repeat_stmt.count);
            ast_free(node->data.repeat_stmt.body);
            break;
            
        case AST_FOR_EACH:
            free(node->data.for_each_stmt.iterator_name);
            ast_free(node->data.for_each_stmt.iterable);
            ast_free(node->data.for_each_stmt.body);
            break;
            
        case AST_RETURN:
            ast_free(node->data.return_stmt.value);
            break;
            
        case AST_DISPLAY:
            ast_free(node->data.display_stmt.value);
            break;
            
        case AST_ASK:
            ast_free(node->data.ask_stmt.prompt);
            free(node->data.ask_stmt.target_var);
            break;
            
        case AST_READ:
            free(node->data.read_stmt.target_var);
            break;
            
        case AST_SECURE_ZONE:
            ast_free(node->data.secure_zone.body);
            break;
            
        case AST_BINARY_OP:
            ast_free(node->data.binary_op.left);
            ast_free(node->data.binary_op.right);
            break;
            
        case AST_UNARY_OP:
            ast_free(node->data.unary_op.operand);
            break;
            
        case AST_LITERAL_STRING:
            free(node->data.literal_string.value);
            break;
            
        case AST_IDENTIFIER:
            free(node->data.identifier.name);
            break;
            
        case AST_FUNC_CALL:
            free(node->data.func_call.name);
            ast_node_list_free(node->data.func_call.args);
            break;
            
        case AST_INDEX:
            ast_free(node->data.index_expr.array);
            ast_free(node->data.index_expr.index);
            break;
            
        case AST_LIST:
            ast_node_list_free(node->data.list_literal.elements);
            break;
            
        case AST_EXPR_STMT:
            ast_free(node->data.expr_stmt.expr);
            break;
            
        default:
            /* Nodes with no dynamic data */
            break;
    }
    
    free(node);
}

/* ============================================================================
 * STRING CONVERSION FUNCTIONS
 * ============================================================================
 */

const char *ast_node_type_to_string(ASTNodeType type) {
    static const char *names[] = {
        [AST_PROGRAM] = "Program",
        [AST_VAR_DECL] = "VarDecl",
        [AST_FUNC_DECL] = "FuncDecl",
        [AST_PARAM_DECL] = "ParamDecl",
        [AST_BLOCK] = "Block",
        [AST_ASSIGN] = "Assign",
        [AST_IF] = "If",
        [AST_WHILE] = "While",
        [AST_REPEAT] = "Repeat",
        [AST_FOR_EACH] = "ForEach",
        [AST_RETURN] = "Return",
        [AST_BREAK] = "Break",
        [AST_CONTINUE] = "Continue",
        [AST_EXPR_STMT] = "ExprStmt",
        [AST_SECURE_ZONE] = "SecureZone",
        [AST_DISPLAY] = "Display",
        [AST_ASK] = "Ask",
        [AST_READ] = "Read",
        [AST_BINARY_OP] = "BinaryOp",
        [AST_UNARY_OP] = "UnaryOp",
        [AST_LITERAL_INT] = "LiteralInt",
        [AST_LITERAL_FLOAT] = "LiteralFloat",
        [AST_LITERAL_STRING] = "LiteralString",
        [AST_LITERAL_BOOL] = "LiteralBool",
        [AST_IDENTIFIER] = "Identifier",
        [AST_FUNC_CALL] = "FuncCall",
        [AST_INDEX] = "Index",
        [AST_LIST] = "List",
        [AST_TYPE] = "Type",
    };
    
    if (type >= 0 && type < AST_NODE_COUNT) {
        return names[type] ? names[type] : "Unknown";
    }
    return "Invalid";
}

const char *ast_data_type_to_string(DataType type) {
    switch (type) {
        case TYPE_UNKNOWN: return "unknown";
        case TYPE_NUMBER: return "number";
        case TYPE_DECIMAL: return "decimal";
        case TYPE_TEXT: return "text";
        case TYPE_FLAG: return "flag";
        case TYPE_LIST: return "list";
        case TYPE_NOTHING: return "nothing";
        case TYPE_FUNCTION: return "function";
        case TYPE_ERROR: return "error";
        default: return "invalid";
    }
}

const char *ast_operator_to_string(Operator op) {
    switch (op) {
        case OP_ADD: return "+";
        case OP_SUB: return "-";
        case OP_MUL: return "*";
        case OP_DIV: return "/";
        case OP_MOD: return "%";
        case OP_POW: return "^";
        case OP_EQ: return "==";
        case OP_NEQ: return "!=";
        case OP_LT: return "<";
        case OP_GT: return ">";
        case OP_LTE: return "<=";
        case OP_GTE: return ">=";
        case OP_AND: return "and";
        case OP_OR: return "or";
        case OP_NOT: return "not";
        case OP_NEG: return "-";
        case OP_POS: return "+";
        default: return "?";
    }
}

/* ============================================================================
 * AST PRINTING
 * ============================================================================
 */

static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

void ast_print(ASTNode *node, int indent) {
    if (node == NULL) {
        print_indent(indent);
        printf("(null)\n");
        return;
    }
    
    print_indent(indent);
    printf("%s", ast_node_type_to_string(node->type));
    
    switch (node->type) {
        case AST_PROGRAM:
            printf(" (%zu statements)\n", node->data.program.statements->count);
            for (size_t i = 0; i < node->data.program.statements->count; i++) {
                ast_print(node->data.program.statements->nodes[i], indent + 1);
            }
            break;
            
        case AST_VAR_DECL:
            printf(" name=%s type=%s const=%d\n",
                   node->data.var_decl.name,
                   ast_data_type_to_string(node->data.var_decl.var_type),
                   node->data.var_decl.is_const);
            if (node->data.var_decl.initializer) {
                print_indent(indent + 1);
                printf("initializer:\n");
                ast_print(node->data.var_decl.initializer, indent + 2);
            }
            break;
            
        case AST_FUNC_DECL:
            printf(" name=%s returns=%s\n",
                   node->data.func_decl.name,
                   ast_data_type_to_string(node->data.func_decl.return_type));
            if (node->data.func_decl.params && node->data.func_decl.params->count > 0) {
                print_indent(indent + 1);
                printf("params:\n");
                for (size_t i = 0; i < node->data.func_decl.params->count; i++) {
                    ast_print(node->data.func_decl.params->nodes[i], indent + 2);
                }
            }
            print_indent(indent + 1);
            printf("body:\n");
            ast_print(node->data.func_decl.body, indent + 2);
            break;
            
        case AST_PARAM_DECL:
            printf(" name=%s type=%s\n",
                   node->data.param_decl.name,
                   ast_data_type_to_string(node->data.param_decl.param_type));
            break;
            
        case AST_BLOCK:
            printf(" (%zu statements)\n", 
                   node->data.block.statements ? node->data.block.statements->count : 0);
            if (node->data.block.statements) {
                for (size_t i = 0; i < node->data.block.statements->count; i++) {
                    ast_print(node->data.block.statements->nodes[i], indent + 1);
                }
            }
            break;
            
        case AST_ASSIGN:
            printf("\n");
            print_indent(indent + 1);
            printf("target:\n");
            ast_print(node->data.assign.target, indent + 2);
            print_indent(indent + 1);
            printf("value:\n");
            ast_print(node->data.assign.value, indent + 2);
            break;
            
        case AST_IF:
            printf("\n");
            print_indent(indent + 1);
            printf("condition:\n");
            ast_print(node->data.if_stmt.condition, indent + 2);
            print_indent(indent + 1);
            printf("then:\n");
            ast_print(node->data.if_stmt.then_branch, indent + 2);
            if (node->data.if_stmt.else_branch) {
                print_indent(indent + 1);
                printf("else:\n");
                ast_print(node->data.if_stmt.else_branch, indent + 2);
            }
            break;
            
        case AST_WHILE:
            printf("\n");
            print_indent(indent + 1);
            printf("condition:\n");
            ast_print(node->data.while_stmt.condition, indent + 2);
            print_indent(indent + 1);
            printf("body:\n");
            ast_print(node->data.while_stmt.body, indent + 2);
            break;
            
        case AST_REPEAT:
            printf("\n");
            print_indent(indent + 1);
            printf("count:\n");
            ast_print(node->data.repeat_stmt.count, indent + 2);
            print_indent(indent + 1);
            printf("body:\n");
            ast_print(node->data.repeat_stmt.body, indent + 2);
            break;
            
        case AST_FOR_EACH:
            printf(" iterator=%s\n", node->data.for_each_stmt.iterator_name);
            print_indent(indent + 1);
            printf("iterable:\n");
            ast_print(node->data.for_each_stmt.iterable, indent + 2);
            print_indent(indent + 1);
            printf("body:\n");
            ast_print(node->data.for_each_stmt.body, indent + 2);
            break;
            
        case AST_RETURN:
            printf("\n");
            if (node->data.return_stmt.value) {
                ast_print(node->data.return_stmt.value, indent + 1);
            }
            break;
            
        case AST_DISPLAY:
            printf("\n");
            ast_print(node->data.display_stmt.value, indent + 1);
            break;
            
        case AST_ASK:
            printf(" target=%s\n", node->data.ask_stmt.target_var);
            print_indent(indent + 1);
            printf("prompt:\n");
            ast_print(node->data.ask_stmt.prompt, indent + 2);
            break;
            
        case AST_READ:
            printf(" target=%s\n", node->data.read_stmt.target_var);
            break;
            
        case AST_SECURE_ZONE:
            printf(" is_safe=%d\n", node->data.secure_zone.is_safe);
            ast_print(node->data.secure_zone.body, indent + 1);
            break;
            
        case AST_BINARY_OP:
            printf(" op=%s\n", ast_operator_to_string(node->data.binary_op.op));
            print_indent(indent + 1);
            printf("left:\n");
            ast_print(node->data.binary_op.left, indent + 2);
            print_indent(indent + 1);
            printf("right:\n");
            ast_print(node->data.binary_op.right, indent + 2);
            break;
            
        case AST_UNARY_OP:
            printf(" op=%s\n", ast_operator_to_string(node->data.unary_op.op));
            ast_print(node->data.unary_op.operand, indent + 1);
            break;
            
        case AST_LITERAL_INT:
            printf(" value=%lld\n", node->data.literal_int.value);
            break;
            
        case AST_LITERAL_FLOAT:
            printf(" value=%f\n", node->data.literal_float.value);
            break;
            
        case AST_LITERAL_STRING:
            printf(" value=\"%s\"\n", node->data.literal_string.value);
            break;
            
        case AST_LITERAL_BOOL:
            printf(" value=%s\n", node->data.literal_bool.value ? "true" : "false");
            break;
            
        case AST_IDENTIFIER:
            printf(" name=%s\n", node->data.identifier.name);
            break;
            
        case AST_FUNC_CALL:
            printf(" name=%s args=%zu\n", 
                   node->data.func_call.name,
                   node->data.func_call.args ? node->data.func_call.args->count : 0);
            if (node->data.func_call.args) {
                for (size_t i = 0; i < node->data.func_call.args->count; i++) {
                    ast_print(node->data.func_call.args->nodes[i], indent + 1);
                }
            }
            break;
            
        case AST_INDEX:
            printf("\n");
            print_indent(indent + 1);
            printf("array:\n");
            ast_print(node->data.index_expr.array, indent + 2);
            print_indent(indent + 1);
            printf("index:\n");
            ast_print(node->data.index_expr.index, indent + 2);
            break;
            
        case AST_LIST:
            printf(" (%zu elements)\n",
                   node->data.list_literal.elements ? node->data.list_literal.elements->count : 0);
            if (node->data.list_literal.elements) {
                for (size_t i = 0; i < node->data.list_literal.elements->count; i++) {
                    ast_print(node->data.list_literal.elements->nodes[i], indent + 1);
                }
            }
            break;
            
        case AST_EXPR_STMT:
            printf("\n");
            ast_print(node->data.expr_stmt.expr, indent + 1);
            break;
            
        default:
            printf("\n");
            break;
    }
}

/* ============================================================================
 * AST VISITOR
 * ============================================================================
 */

void ast_visit(ASTNode *node, ASTVisitor *visitor) {
    if (node == NULL || visitor == NULL) return;
    
    /* Pre-visit */
    if (visitor->visit_pre) {
        visitor->visit_pre(visitor, node);
    }
    
    /* Visit children based on node type */
    switch (node->type) {
        case AST_PROGRAM:
            if (node->data.program.statements) {
                for (size_t i = 0; i < node->data.program.statements->count; i++) {
                    ast_visit(node->data.program.statements->nodes[i], visitor);
                }
            }
            break;
            
        case AST_VAR_DECL:
            ast_visit(node->data.var_decl.initializer, visitor);
            break;
            
        case AST_FUNC_DECL:
            if (node->data.func_decl.params) {
                for (size_t i = 0; i < node->data.func_decl.params->count; i++) {
                    ast_visit(node->data.func_decl.params->nodes[i], visitor);
                }
            }
            ast_visit(node->data.func_decl.body, visitor);
            break;
            
        case AST_BLOCK:
            if (node->data.block.statements) {
                for (size_t i = 0; i < node->data.block.statements->count; i++) {
                    ast_visit(node->data.block.statements->nodes[i], visitor);
                }
            }
            break;
            
        case AST_ASSIGN:
            ast_visit(node->data.assign.target, visitor);
            ast_visit(node->data.assign.value, visitor);
            break;
            
        case AST_IF:
            ast_visit(node->data.if_stmt.condition, visitor);
            ast_visit(node->data.if_stmt.then_branch, visitor);
            ast_visit(node->data.if_stmt.else_branch, visitor);
            break;
            
        case AST_WHILE:
            ast_visit(node->data.while_stmt.condition, visitor);
            ast_visit(node->data.while_stmt.body, visitor);
            break;
            
        case AST_REPEAT:
            ast_visit(node->data.repeat_stmt.count, visitor);
            ast_visit(node->data.repeat_stmt.body, visitor);
            break;
            
        case AST_FOR_EACH:
            ast_visit(node->data.for_each_stmt.iterable, visitor);
            ast_visit(node->data.for_each_stmt.body, visitor);
            break;
            
        case AST_RETURN:
            ast_visit(node->data.return_stmt.value, visitor);
            break;
            
        case AST_DISPLAY:
            ast_visit(node->data.display_stmt.value, visitor);
            break;
            
        case AST_ASK:
            ast_visit(node->data.ask_stmt.prompt, visitor);
            break;
            
        case AST_SECURE_ZONE:
            ast_visit(node->data.secure_zone.body, visitor);
            break;
            
        case AST_BINARY_OP:
            ast_visit(node->data.binary_op.left, visitor);
            ast_visit(node->data.binary_op.right, visitor);
            break;
            
        case AST_UNARY_OP:
            ast_visit(node->data.unary_op.operand, visitor);
            break;
            
        case AST_FUNC_CALL:
            if (node->data.func_call.args) {
                for (size_t i = 0; i < node->data.func_call.args->count; i++) {
                    ast_visit(node->data.func_call.args->nodes[i], visitor);
                }
            }
            break;
            
        case AST_INDEX:
            ast_visit(node->data.index_expr.array, visitor);
            ast_visit(node->data.index_expr.index, visitor);
            break;
            
        case AST_LIST:
            if (node->data.list_literal.elements) {
                for (size_t i = 0; i < node->data.list_literal.elements->count; i++) {
                    ast_visit(node->data.list_literal.elements->nodes[i], visitor);
                }
            }
            break;
            
        case AST_EXPR_STMT:
            ast_visit(node->data.expr_stmt.expr, visitor);
            break;
            
        default:
            break;
    }
    
    /* Post-visit */
    if (visitor->visit_post) {
        visitor->visit_post(visitor, node);
    }
}
