/*
 * NatureLang Compiler
 * Copyright (c) 2026
 * 
 * Abstract Syntax Tree (AST) Node Definitions
 * 
 * This file defines all AST node types for representing
 * NatureLang programs after parsing.
 */

#ifndef NATURELANG_AST_H
#define NATURELANG_AST_H

#include <stddef.h>

/* ============================================================================
 * SOURCE LOCATION (for error reporting)
 * ============================================================================
 */
#ifndef SOURCELOCATION_DEFINED
#define SOURCELOCATION_DEFINED
typedef struct {
    const char *filename;   /* Source file name */
    int first_line;         /* Starting line (1-based) */
    int first_column;       /* Starting column (1-based) */
    int last_line;          /* Ending line */
    int last_column;        /* Ending column */
} SourceLocation;
#endif

/* ============================================================================
 * FORWARD DECLARATIONS
 * ============================================================================
 */
typedef struct ASTNode ASTNode;
typedef struct ASTNodeList ASTNodeList;
typedef struct ASTProgram ASTProgram;

/* ============================================================================
 * AST NODE TYPES
 * ============================================================================
 */
typedef enum {
    /* Program root */
    AST_PROGRAM,
    
    /* Declarations */
    AST_VAR_DECL,           /* Variable declaration */
    AST_FUNC_DECL,          /* Function declaration */
    AST_PARAM_DECL,         /* Function parameter */
    
    /* Statements */
    AST_BLOCK,              /* Block of statements */
    AST_ASSIGN,             /* Assignment statement */
    AST_IF,                 /* If/else statement */
    AST_WHILE,              /* While loop */
    AST_REPEAT,             /* Repeat N times loop */
    AST_FOR_EACH,           /* For each loop */
    AST_RETURN,             /* Return statement */
    AST_BREAK,              /* Break (stop) */
    AST_CONTINUE,           /* Continue (skip) */
    AST_EXPR_STMT,          /* Expression as statement */
    AST_SECURE_ZONE,        /* Secure zone block */
    
    /* I/O Statements */
    AST_DISPLAY,            /* Display/show/print */
    AST_ASK,                /* Ask and remember */
    AST_READ,               /* Read input */
    
    /* Expressions */
    AST_BINARY_OP,          /* Binary operation */
    AST_UNARY_OP,           /* Unary operation */
    AST_TERNARY_OP,         /* Ternary operation (e.g., is between) */
    AST_LITERAL_INT,        /* Integer literal */
    AST_LITERAL_FLOAT,      /* Float literal */
    AST_LITERAL_STRING,     /* String literal */
    AST_LITERAL_BOOL,       /* Boolean literal */
    AST_IDENTIFIER,         /* Variable/identifier reference */
    AST_FUNC_CALL,          /* Function call */
    AST_INDEX,              /* Array indexing */
    AST_LIST,               /* List literal */
    
    /* Type nodes */
    AST_TYPE,               /* Type specification */
    
    AST_NODE_COUNT
} ASTNodeType;

/* ============================================================================
 * DATA TYPE ENUMERATION
 * ============================================================================
 */
typedef enum {
    TYPE_UNKNOWN,
    TYPE_NUMBER,            /* int */
    TYPE_DECIMAL,           /* float */
    TYPE_TEXT,              /* string */
    TYPE_FLAG,              /* bool */
    TYPE_LIST,              /* list/array */
    TYPE_NOTHING,           /* void */
    TYPE_FUNCTION,          /* function type */
    TYPE_ERROR              /* error type for recovery */
} DataType;

/* ============================================================================
 * OPERATOR ENUMERATION
 * ============================================================================
 */
typedef enum {
    /* Arithmetic operators */
    OP_ADD,                 /* plus, + */
    OP_SUB,                 /* minus, - */
    OP_MUL,                 /* multiplied by, * */
    OP_DIV,                 /* divided by, / */
    OP_MOD,                 /* modulo, % */
    OP_POW,                 /* power, ^ */
    
    /* Comparison operators */
    OP_EQ,                  /* equal to, == */
    OP_NEQ,                 /* not equal to, != */
    OP_LT,                  /* less than, < */
    OP_GT,                  /* greater than, > */
    OP_LTE,                 /* at most, <= */
    OP_GTE,                 /* at least, >= */
    OP_BETWEEN,             /* is between (unique NatureLang operator) */
    
    /* Logical operators */
    OP_AND,                 /* and, && */
    OP_OR,                  /* or, || */
    OP_NOT,                 /* not, ! */
    
    /* Unary */
    OP_NEG,                 /* unary minus */
    OP_POS                  /* unary plus */
} Operator;

/* ============================================================================
 * AST NODE LIST
 * ============================================================================
 */
struct ASTNodeList {
    ASTNode **nodes;
    size_t count;
    size_t capacity;
};

/* ============================================================================
 * AST NODE STRUCTURE
 * ============================================================================
 */
struct ASTNode {
    ASTNodeType type;
    SourceLocation loc;
    DataType data_type;     /* Resolved type (filled by semantic analysis) */
    
    union {
        /* AST_PROGRAM */
        struct {
            ASTNodeList *statements;
        } program;
        
        /* AST_VAR_DECL */
        struct {
            char *name;
            DataType var_type;
            ASTNode *initializer;   /* May be NULL */
            int is_const;           /* 1 if const/immutable */
        } var_decl;
        
        /* AST_FUNC_DECL */
        struct {
            char *name;
            ASTNodeList *params;    /* List of AST_PARAM_DECL */
            DataType return_type;
            ASTNode *body;          /* AST_BLOCK */
        } func_decl;
        
        /* AST_PARAM_DECL */
        struct {
            char *name;
            DataType param_type;
        } param_decl;
        
        /* AST_BLOCK */
        struct {
            ASTNodeList *statements;
        } block;
        
        /* AST_ASSIGN */
        struct {
            ASTNode *target;        /* Identifier or index expression */
            ASTNode *value;
        } assign;
        
        /* AST_IF */
        struct {
            ASTNode *condition;
            ASTNode *then_branch;   /* AST_BLOCK */
            ASTNode *else_branch;   /* AST_BLOCK or NULL */
        } if_stmt;
        
        /* AST_WHILE */
        struct {
            ASTNode *condition;
            ASTNode *body;          /* AST_BLOCK */
        } while_stmt;
        
        /* AST_REPEAT */
        struct {
            ASTNode *count;         /* Number expression */
            ASTNode *body;          /* AST_BLOCK */
        } repeat_stmt;
        
        /* AST_FOR_EACH */
        struct {
            char *iterator_name;
            ASTNode *iterable;
            ASTNode *body;          /* AST_BLOCK */
        } for_each_stmt;
        
        /* AST_RETURN */
        struct {
            ASTNode *value;         /* May be NULL */
        } return_stmt;
        
        /* AST_DISPLAY */
        struct {
            ASTNode *value;
        } display_stmt;
        
        /* AST_ASK */
        struct {
            ASTNode *prompt;        /* String expression */
            char *target_var;       /* Variable to store result */
        } ask_stmt;
        
        /* AST_READ */
        struct {
            char *target_var;
        } read_stmt;
        
        /* AST_SECURE_ZONE */
        struct {
            ASTNode *body;          /* AST_BLOCK */
            int is_safe;            /* 1 for 'safe zone', 0 for 'secure zone' */
        } secure_zone;
        
        /* AST_BINARY_OP */
        struct {
            Operator op;
            ASTNode *left;
            ASTNode *right;
        } binary_op;
        
        /* AST_UNARY_OP */
        struct {
            Operator op;
            ASTNode *operand;
        } unary_op;
        
        /* AST_TERNARY_OP - for "is between" operator */
        struct {
            Operator op;
            ASTNode *operand;       /* The value being tested */
            ASTNode *lower;         /* Lower bound */
            ASTNode *upper;         /* Upper bound */
        } ternary_op;
        
        /* AST_LITERAL_INT */
        struct {
            long long value;
        } literal_int;
        
        /* AST_LITERAL_FLOAT */
        struct {
            double value;
        } literal_float;
        
        /* AST_LITERAL_STRING */
        struct {
            char *value;
        } literal_string;
        
        /* AST_LITERAL_BOOL */
        struct {
            int value;              /* 0 or 1 */
        } literal_bool;
        
        /* AST_IDENTIFIER */
        struct {
            char *name;
        } identifier;
        
        /* AST_FUNC_CALL */
        struct {
            char *name;
            ASTNodeList *args;
        } func_call;
        
        /* AST_INDEX */
        struct {
            ASTNode *array;
            ASTNode *index;
        } index_expr;
        
        /* AST_LIST */
        struct {
            ASTNodeList *elements;
        } list_literal;
        
        /* AST_EXPR_STMT */
        struct {
            ASTNode *expr;
        } expr_stmt;
        
    } data;
};

/* ============================================================================
 * AST CONSTRUCTION FUNCTIONS
 * ============================================================================
 */

/* Node list management */
ASTNodeList *ast_node_list_create(void);
void ast_node_list_append(ASTNodeList *list, ASTNode *node);
void ast_node_list_free(ASTNodeList *list);

/* Program */
ASTNode *ast_create_program(ASTNodeList *statements, SourceLocation loc);

/* Declarations */
ASTNode *ast_create_var_decl(const char *name, DataType type, ASTNode *init, 
                              int is_const, SourceLocation loc);
ASTNode *ast_create_func_decl(const char *name, ASTNodeList *params, 
                               DataType return_type, ASTNode *body, SourceLocation loc);
ASTNode *ast_create_param_decl(const char *name, DataType type, SourceLocation loc);

/* Statements */
ASTNode *ast_create_block(ASTNodeList *statements, SourceLocation loc);
ASTNode *ast_create_assign(ASTNode *target, ASTNode *value, SourceLocation loc);
ASTNode *ast_create_if(ASTNode *cond, ASTNode *then_branch, ASTNode *else_branch, 
                        SourceLocation loc);
ASTNode *ast_create_while(ASTNode *cond, ASTNode *body, SourceLocation loc);
ASTNode *ast_create_repeat(ASTNode *count, ASTNode *body, SourceLocation loc);
ASTNode *ast_create_for_each(const char *iter_name, ASTNode *iterable, 
                              ASTNode *body, SourceLocation loc);
ASTNode *ast_create_return(ASTNode *value, SourceLocation loc);
ASTNode *ast_create_break(SourceLocation loc);
ASTNode *ast_create_continue(SourceLocation loc);
ASTNode *ast_create_expr_stmt(ASTNode *expr, SourceLocation loc);

/* I/O Statements */
ASTNode *ast_create_display(ASTNode *value, SourceLocation loc);
ASTNode *ast_create_ask(ASTNode *prompt, const char *target_var, SourceLocation loc);
ASTNode *ast_create_read(const char *target_var, SourceLocation loc);

/* Secure zone */
ASTNode *ast_create_secure_zone(ASTNode *body, int is_safe, SourceLocation loc);

/* Expressions */
ASTNode *ast_create_binary_op(Operator op, ASTNode *left, ASTNode *right, 
                               SourceLocation loc);
ASTNode *ast_create_unary_op(Operator op, ASTNode *operand, SourceLocation loc);
ASTNode *ast_create_ternary_op(Operator op, ASTNode *operand, ASTNode *lower, 
                                ASTNode *upper, SourceLocation loc);
ASTNode *ast_create_literal_int(long long value, SourceLocation loc);
ASTNode *ast_create_literal_float(double value, SourceLocation loc);
ASTNode *ast_create_literal_string(const char *value, SourceLocation loc);
ASTNode *ast_create_literal_bool(int value, SourceLocation loc);
ASTNode *ast_create_identifier(const char *name, SourceLocation loc);
ASTNode *ast_create_func_call(const char *name, ASTNodeList *args, SourceLocation loc);
ASTNode *ast_create_index(ASTNode *array, ASTNode *index, SourceLocation loc);
ASTNode *ast_create_list(ASTNodeList *elements, SourceLocation loc);

/* ============================================================================
 * AST UTILITY FUNCTIONS
 * ============================================================================
 */

/* Free an AST node and all children */
void ast_free(ASTNode *node);

/* Get string representation of node type */
const char *ast_node_type_to_string(ASTNodeType type);

/* Get string representation of data type */
const char *ast_data_type_to_string(DataType type);

/* Get string representation of operator */
const char *ast_operator_to_string(Operator op);

/* Print AST for debugging (with indentation) */
void ast_print(ASTNode *node, int indent);

/* Deep clone an AST node */
ASTNode *ast_clone(ASTNode *node);

/* ============================================================================
 * AST VISITOR PATTERN (for traversal)
 * ============================================================================
 */
typedef struct ASTVisitor ASTVisitor;

typedef void (*ASTVisitFunc)(ASTVisitor *visitor, ASTNode *node);

struct ASTVisitor {
    void *user_data;
    ASTVisitFunc visit_pre;     /* Called before visiting children */
    ASTVisitFunc visit_post;    /* Called after visiting children */
};

/* Visit all nodes in the AST */
void ast_visit(ASTNode *node, ASTVisitor *visitor);

#endif /* NATURELANG_AST_H */
