/*
 * NatureLang Compiler
 * Copyright (c) 2026
 * 
 * Bison Parser Grammar
 * 
 * This file defines the grammar for NatureLang, a natural language style
 * programming language that compiles to C.
 * 
 * Example NatureLang code:
 *   create a number called x and set it to 42
 *   if x is greater than 10 then
 *       display "Large number!"
 *   end if
 */

/* Code that goes into the header file (naturelang.tab.h) */
%code requires {
#include "ast.h"
}

%{
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

/* Forward declarations */
extern int yylex(void);
extern int yylineno;
extern char *yytext;
extern FILE *yyin;

void yyerror(const char *s);

/* Parser state */
static ASTNode *parse_result = NULL;

/* Location helper */
static SourceLocation make_loc(void) {
    SourceLocation loc = {NULL, 0, 0, 0, 0};
    loc.first_line = yylineno;
    return loc;
}
%}

/* ============================================================================
 * TOKEN AND TYPE DEFINITIONS
 * Token names match exactly those in tokens.h for unified lexer usage
 * ============================================================================
 */

%union {
    long long int_val;
    double float_val;
    char *str_val;
    char char_val;
    int bool_val;
    ASTNode *node;
    ASTNodeList *list;
    DataType dtype;
    Operator oper;
}

/* Literals */
%token <int_val> TOK_INTEGER
%token <float_val> TOK_FLOAT
%token <str_val> TOK_STRING
%token <str_val> TOK_IDENTIFIER
%token <bool_val> TOK_TRUE TOK_FALSE TOK_YES TOK_NO

/* Keywords - Declaration (match tokens.h) */
%token TOK_CREATE TOK_A TOK_AN TOK_CALLED TOK_NAMED TOK_AND TOK_SET TOK_IT TOK_TO TOK_AS
%token TOK_BECOMES TOK_EQUALS TOK_MAKE TOK_EQUAL

/* Type keywords (match tokens.h - TYPE_ prefix) */
%token TOK_TYPE_NUMBER TOK_TYPE_DECIMAL TOK_TYPE_TEXT TOK_TYPE_FLAG TOK_TYPE_LIST TOK_TYPE_NOTHING

/* Control flow (match tokens.h) */
%token TOK_IF TOK_THEN TOK_OTHERWISE TOK_ELSE TOK_END
%token TOK_REPEAT TOK_TIMES TOK_WHILE TOK_DO TOK_FOR TOK_EACH TOK_IN TOK_FROM TOK_UNTIL
%token TOK_STOP TOK_SKIP

/* Functions (match tokens.h) */
%token TOK_DEFINE TOK_FUNCTION TOK_THAT TOK_TAKES TOK_RETURNS
%token TOK_GIVE TOK_BACK TOK_CALL TOK_WITH

/* I/O (match tokens.h) */
%token TOK_DISPLAY TOK_SHOW TOK_PRINT
%token TOK_ASK TOK_READ TOK_REMEMBER TOK_SAVE TOK_INTO

/* Security (match tokens.h) */
%token TOK_ENTER TOK_SECURE TOK_ZONE TOK_SAFE

/* Logical (match tokens.h) */
%token TOK_IS TOK_NOT TOK_OR

/* Comparison - single words (match tokens.h) */
%token TOK_GREATER TOK_LESS TOK_THAN

/* Comparison - multi-word tokens (match tokens.h) */
%token TOK_GREATER_THAN TOK_LESS_THAN TOK_EQUAL_TO TOK_NOT_EQUAL_TO
%token TOK_AT_LEAST TOK_AT_MOST TOK_BETWEEN

/* Arithmetic - natural words (match tokens.h) */
%token TOK_PLUS TOK_MINUS TOK_MULTIPLIED TOK_DIVIDED TOK_BY
%token TOK_MODULO TOK_REMAINDER TOK_OF TOK_POWER TOK_SQUARED TOK_SQUARE TOK_ROOT

/* Symbolic operators (match tokens.h) */
%token TOK_OP_PLUS TOK_OP_MINUS TOK_OP_STAR TOK_OP_SLASH TOK_OP_PERCENT TOK_OP_CARET
%token TOK_OP_EQ TOK_OP_EQEQ TOK_OP_NEQ TOK_OP_LT TOK_OP_GT TOK_OP_LTE TOK_OP_GTE
%token TOK_OP_AND TOK_OP_OR TOK_OP_NOT TOK_OP_ARROW TOK_OP_COLON

/* Punctuation (match tokens.h) */
%token TOK_LPAREN TOK_RPAREN TOK_LBRACKET TOK_RBRACKET TOK_LBRACE TOK_RBRACE
%token TOK_COMMA TOK_DOT TOK_SEMICOLON TOK_NEWLINE

/* Comments */
%token TOK_COMMENT TOK_BLOCK_COMMENT

/* Special */
%token TOK_EOF TOK_ERROR TOK_UNKNOWN TOK_CHAR TOK_THE TOK_VALUE TOK_CONSTANT TOK_RETURN TOK_STORE
%token TOK_BEGIN TOK_SAFELY TOK_RISKY TOK_USER TOK_CHANGE
%token TOK_ADD TOK_REMOVE TOK_GET TOK_ITEM TOK_AT TOK_POSITION TOK_LENGTH TOK_SIZE TOK_APPEND
%token TOK_FIRST TOK_LAST

/* Type declarations for non-terminals */
%type <node> program statement statement_list
%type <node> declaration assignment
%type <node> if_statement while_statement repeat_statement for_each_statement
%type <node> function_decl return_statement
%type <node> display_statement ask_statement read_statement
%type <node> secure_zone_statement
%type <node> expression term factor primary
%type <node> comparison logic_expr
%type <node> opt_else
%type <node> function_call list_literal
%type <list> statement_block param_list arg_list expr_list
%type <dtype> type_specifier
%type <oper> add_op mul_op rel_op

/* Helper rules - no semantic value needed */

/* Operator precedence (lowest to highest) */
%left TOK_OR TOK_OP_OR
%left TOK_AND TOK_OP_AND
%nonassoc TOK_NOT TOK_OP_NOT
%nonassoc TOK_IS TOK_EQUALS TOK_GREATER TOK_LESS TOK_OP_EQEQ TOK_OP_NEQ TOK_OP_LT TOK_OP_GT TOK_OP_LTE TOK_OP_GTE
%nonassoc TOK_GREATER_THAN TOK_LESS_THAN TOK_EQUAL_TO TOK_NOT_EQUAL_TO TOK_AT_LEAST TOK_AT_MOST
%left TOK_PLUS TOK_MINUS TOK_OP_PLUS TOK_OP_MINUS
%left TOK_MULTIPLIED TOK_DIVIDED TOK_MODULO TOK_OP_STAR TOK_OP_SLASH TOK_OP_PERCENT TOK_BY
%right TOK_POWER TOK_OP_CARET
%right TOK_SQUARED

/* Expect conflicts - NatureLang's flexible syntax causes many valid shift/reduce conflicts.
 * These are intentional due to optional terminators and flexible expression syntax.
 * We use a GLR parser to handle reduce/reduce conflicts from ambiguous rules. */
%glr-parser
%expect 88
%expect-rr 66

/* Merge function to resolve ambiguous GLR parses - just pick the first one */
%define parse.error verbose

%start program

%code {
/* GLR merge function - when two ambiguous parses produce the same result, pick one */
static YYSTYPE merge_nodes(YYSTYPE x0, YYSTYPE x1);
}

%%

/* ============================================================================
 * GRAMMAR RULES
 * ============================================================================
 */

program
    : statement_list
        {
            ASTNodeList *stmts = ast_node_list_create();
            if ($1 != NULL) {
                ast_node_list_append(stmts, $1);
            }
            parse_result = ast_create_program(stmts, make_loc());
            $$ = parse_result;
        }
    ;

statement_list
    : /* empty */
        { $$ = NULL; }
    | statement_list statement opt_terminator
        {
            if ($1 == NULL) {
                $$ = $2;
            } else if ($2 != NULL) {
                /* Create a block to hold multiple statements */
                if ($1->type == AST_BLOCK) {
                    ast_node_list_append($1->data.block.statements, $2);
                    $$ = $1;
                } else {
                    ASTNodeList *stmts = ast_node_list_create();
                    ast_node_list_append(stmts, $1);
                    ast_node_list_append(stmts, $2);
                    $$ = ast_create_block(stmts, make_loc());
                }
            } else {
                $$ = $1;
            }
        }
    ;

opt_terminator
    : /* empty */
    | TOK_DOT
    | TOK_NEWLINE
    | TOK_SEMICOLON
    ;

statement
    : declaration
    | assignment
    | if_statement
    | while_statement
    | repeat_statement
    | for_each_statement
    | function_decl
    | return_statement
    | display_statement
    | ask_statement
    | read_statement
    | secure_zone_statement
    | function_call
        { $$ = ast_create_expr_stmt($1, make_loc()); }
    | TOK_STOP
        { $$ = ast_create_break(make_loc()); }
    | TOK_SKIP
        { $$ = ast_create_continue(make_loc()); }
    ;

/* ============================================================================
 * DECLARATIONS
 * ============================================================================
 */

/* create a number called x and set it to 42 */
/* create a text called name */
/* make x a constant number with value 100 */
/* Note: TOK_SET matches "set it to" as a single token */
declaration
    : TOK_CREATE article type_specifier TOK_CALLED TOK_IDENTIFIER TOK_AND TOK_SET expression
        {
            $$ = ast_create_var_decl($5, $3, $8, 0, make_loc());
            free($5);
        }
    | TOK_CREATE article type_specifier TOK_CALLED TOK_IDENTIFIER
        {
            $$ = ast_create_var_decl($5, $3, NULL, 0, make_loc());
            free($5);
        }
    | TOK_CREATE article type_specifier TOK_NAMED TOK_IDENTIFIER TOK_AND TOK_SET expression
        {
            $$ = ast_create_var_decl($5, $3, $8, 0, make_loc());
            free($5);
        }
    | TOK_CREATE article type_specifier TOK_NAMED TOK_IDENTIFIER
        {
            $$ = ast_create_var_decl($5, $3, NULL, 0, make_loc());
            free($5);
        }
    | TOK_MAKE TOK_IDENTIFIER article TOK_CONSTANT type_specifier TOK_WITH TOK_VALUE expression
        {
            $$ = ast_create_var_decl($2, $5, $8, 1, make_loc());
            free($2);
        }
    | TOK_CREATE article TOK_CONSTANT type_specifier TOK_CALLED TOK_IDENTIFIER TOK_AND TOK_SET expression
        {
            $$ = ast_create_var_decl($6, $4, $9, 1, make_loc());
            free($6);
        }
    | TOK_CREATE article TOK_TYPE_LIST TOK_OF type_specifier TOK_CALLED TOK_IDENTIFIER
        {
            $$ = ast_create_var_decl($7, TYPE_LIST, NULL, 0, make_loc());
            free($7);
        }
    | TOK_CREATE article TOK_TYPE_LIST TOK_CALLED TOK_IDENTIFIER TOK_WITH expr_list
        {
            ASTNode *init = ast_create_list($7, make_loc());
            $$ = ast_create_var_decl($5, TYPE_LIST, init, 0, make_loc());
            free($5);
        }
    ;

article
    : TOK_A
    | TOK_AN
    | TOK_THE
    | /* empty */
    ;

/* Type specifiers using TYPE_ prefixed tokens from tokens.h */
type_specifier
    : TOK_TYPE_NUMBER    { $$ = TYPE_NUMBER; }
    | TOK_TYPE_DECIMAL   { $$ = TYPE_DECIMAL; }
    | TOK_TYPE_TEXT      { $$ = TYPE_TEXT; }
    | TOK_TYPE_FLAG      { $$ = TYPE_FLAG; }
    | TOK_TYPE_LIST      { $$ = TYPE_LIST; }
    | TOK_TYPE_NOTHING   { $$ = TYPE_NOTHING; }
    ;

/* ============================================================================
 * ASSIGNMENTS
 * ============================================================================
 */

/* set x to 10 */
/* change the value of x to 20 */
/* set it to 42 */
/* x becomes 10 */
assignment
    : TOK_SET TOK_IDENTIFIER TOK_TO expression
        {
            ASTNode *target = ast_create_identifier($2, make_loc());
            $$ = ast_create_assign(target, $4, make_loc());
            free($2);
        }
    | TOK_CHANGE TOK_THE TOK_VALUE TOK_OF TOK_IDENTIFIER TOK_TO expression
        {
            ASTNode *target = ast_create_identifier($5, make_loc());
            $$ = ast_create_assign(target, $7, make_loc());
            free($5);
        }
    | TOK_SET TOK_IDENTIFIER TOK_AT expression TOK_TO expression
        {
            ASTNode *arr = ast_create_identifier($2, make_loc());
            ASTNode *target = ast_create_index(arr, $4, make_loc());
            $$ = ast_create_assign(target, $6, make_loc());
            free($2);
        }
    | TOK_IDENTIFIER TOK_BECOMES expression
        {
            ASTNode *target = ast_create_identifier($1, make_loc());
            $$ = ast_create_assign(target, $3, make_loc());
            free($1);
        }
    | TOK_IDENTIFIER TOK_OP_EQ expression
        {
            ASTNode *target = ast_create_identifier($1, make_loc());
            $$ = ast_create_assign(target, $3, make_loc());
            free($1);
        }
    ;

/* ============================================================================
 * CONTROL FLOW
 * ============================================================================
 */

/* if x is greater than 10 then ... end if */
/* if condition then ... otherwise ... end if */
/* if condition then ... else ... end */
if_statement
    : TOK_IF expression TOK_THEN statement_block opt_else TOK_END TOK_IF
        {
            ASTNode *then_block = ast_create_block($4, make_loc());
            $$ = ast_create_if($2, then_block, $5, make_loc());
        }
    | TOK_IF expression TOK_THEN statement_block opt_else TOK_END
        {
            ASTNode *then_block = ast_create_block($4, make_loc());
            $$ = ast_create_if($2, then_block, $5, make_loc());
        }
    ;

opt_else
    : /* empty */
        { $$ = NULL; }
    | TOK_OTHERWISE statement_block
        {
            $$ = ast_create_block($2, make_loc());
        }
    | TOK_ELSE statement_block
        {
            $$ = ast_create_block($2, make_loc());
        }
    ;

/* while x is less than 100 do ... end while */
/* while condition do ... end */
while_statement
    : TOK_WHILE expression TOK_DO statement_block TOK_END TOK_WHILE
        {
            ASTNode *body = ast_create_block($4, make_loc());
            $$ = ast_create_while($2, body, make_loc());
        }
    | TOK_WHILE expression TOK_DO statement_block TOK_END
        {
            ASTNode *body = ast_create_block($4, make_loc());
            $$ = ast_create_while($2, body, make_loc());
        }
    ;

/* repeat 10 times ... end repeat */
/* repeat N times ... end */
repeat_statement
    : TOK_REPEAT expression TOK_TIMES statement_block TOK_END TOK_REPEAT
        {
            ASTNode *body = ast_create_block($4, make_loc());
            $$ = ast_create_repeat($2, body, make_loc());
        }
    | TOK_REPEAT expression TOK_TIMES statement_block TOK_END
        {
            ASTNode *body = ast_create_block($4, make_loc());
            $$ = ast_create_repeat($2, body, make_loc());
        }
    ;

/* for each item in mylist do ... end for */
for_each_statement
    : TOK_FOR TOK_EACH TOK_IDENTIFIER TOK_IN expression TOK_DO statement_block TOK_END TOK_FOR
        {
            ASTNode *body = ast_create_block($7, make_loc());
            $$ = ast_create_for_each($3, $5, body, make_loc());
            free($3);
        }
    | TOK_FOR TOK_EACH TOK_IDENTIFIER TOK_IN expression TOK_DO statement_block TOK_END
        {
            ASTNode *body = ast_create_block($7, make_loc());
            $$ = ast_create_for_each($3, $5, body, make_loc());
            free($3);
        }
    ;

statement_block
    : /* empty */
        { $$ = ast_node_list_create(); }
    | statement_block statement opt_terminator
        {
            ast_node_list_append($1, $2);
            $$ = $1;
        }
    ;

/* ============================================================================
 * FUNCTIONS
 * ============================================================================
 */

/* define a function called add that takes number a and number b and returns number */
/* define a function called greet that takes text name */
/* define a function greet that takes name and returns nothing */
function_decl
    : TOK_DEFINE article TOK_FUNCTION TOK_CALLED TOK_IDENTIFIER TOK_THAT TOK_TAKES param_list TOK_AND TOK_RETURNS type_specifier TOK_OP_COLON statement_block TOK_END TOK_FUNCTION
        {
            ASTNode *body = ast_create_block($13, make_loc());
            $$ = ast_create_func_decl($5, $8, $11, body, make_loc());
            free($5);
        }
    | TOK_DEFINE article TOK_FUNCTION TOK_CALLED TOK_IDENTIFIER TOK_THAT TOK_TAKES param_list TOK_OP_COLON statement_block TOK_END TOK_FUNCTION
        {
            ASTNode *body = ast_create_block($10, make_loc());
            $$ = ast_create_func_decl($5, $8, TYPE_NOTHING, body, make_loc());
            free($5);
        }
    | TOK_DEFINE article TOK_FUNCTION TOK_CALLED TOK_IDENTIFIER TOK_OP_COLON statement_block TOK_END TOK_FUNCTION
        {
            ASTNode *body = ast_create_block($7, make_loc());
            $$ = ast_create_func_decl($5, NULL, TYPE_NOTHING, body, make_loc());
            free($5);
        }
    /* Flexible syntax without "called" - "define a function NAME that takes..." */
    | TOK_DEFINE article TOK_FUNCTION TOK_IDENTIFIER TOK_THAT TOK_TAKES param_list TOK_AND TOK_RETURNS type_specifier statement_block TOK_END TOK_FUNCTION
        {
            ASTNode *body = ast_create_block($11, make_loc());
            $$ = ast_create_func_decl($4, $7, $10, body, make_loc());
            free($4);
        }
    | TOK_DEFINE article TOK_FUNCTION TOK_IDENTIFIER TOK_THAT TOK_TAKES param_list statement_block TOK_END TOK_FUNCTION
        {
            ASTNode *body = ast_create_block($8, make_loc());
            $$ = ast_create_func_decl($4, $7, TYPE_NOTHING, body, make_loc());
            free($4);
        }
    | TOK_DEFINE article TOK_FUNCTION TOK_IDENTIFIER statement_block TOK_END TOK_FUNCTION
        {
            ASTNode *body = ast_create_block($5, make_loc());
            $$ = ast_create_func_decl($4, NULL, TYPE_NOTHING, body, make_loc());
            free($4);
        }
    ;

param_list
    : type_specifier TOK_IDENTIFIER
        {
            $$ = ast_node_list_create();
            ASTNode *param = ast_create_param_decl($2, $1, make_loc());
            ast_node_list_append($$, param);
            free($2);
        }
    | TOK_IDENTIFIER
        {
            /* No type specifier - default to unknown */
            $$ = ast_node_list_create();
            ASTNode *param = ast_create_param_decl($1, TYPE_UNKNOWN, make_loc());
            ast_node_list_append($$, param);
            free($1);
        }
    | param_list TOK_COMMA type_specifier TOK_IDENTIFIER
        {
            ASTNode *param = ast_create_param_decl($4, $3, make_loc());
            ast_node_list_append($1, param);
            $$ = $1;
            free($4);
        }
    | param_list TOK_COMMA TOK_IDENTIFIER
        {
            ASTNode *param = ast_create_param_decl($3, TYPE_UNKNOWN, make_loc());
            ast_node_list_append($1, param);
            $$ = $1;
            free($3);
        }
    ;

/* return 42 */
/* return the result */
/* give back sum */
/* "give back" is matched as a single TOK_GIVE token by the lexer */
/* NOTE: We require expression for return statements to avoid ambiguity with
 * expression-as-statement. For void returns, use explicit "return nothing". */
return_statement
    : TOK_RETURN expression
        { $$ = ast_create_return($2, make_loc()); }
    | TOK_RETURN TOK_THE expression
        { $$ = ast_create_return($3, make_loc()); }
    | TOK_RETURN TOK_TYPE_NOTHING
        { $$ = ast_create_return(NULL, make_loc()); }
    | TOK_GIVE expression
        { $$ = ast_create_return($2, make_loc()); }
    | TOK_GIVE TOK_TYPE_NOTHING
        { $$ = ast_create_return(NULL, make_loc()); }
    ;

/* ============================================================================
 * I/O STATEMENTS
 * ============================================================================
 */

/* display "Hello, World!" */
/* display the value of x */
/* show "Hello" */
/* print "World" */
display_statement
    : TOK_DISPLAY expression
        { $$ = ast_create_display($2, make_loc()); }
    | TOK_DISPLAY TOK_THE TOK_VALUE TOK_OF expression
        { $$ = ast_create_display($5, make_loc()); }
    | TOK_SHOW expression
        { $$ = ast_create_display($2, make_loc()); }
    | TOK_PRINT expression
        { $$ = ast_create_display($2, make_loc()); }
    ;

/* ask "What is your name?" and store in name */
/* ask for user's name and store in name */
ask_statement
    : TOK_ASK expression TOK_AND TOK_STORE TOK_IN TOK_IDENTIFIER
        {
            $$ = ast_create_ask($2, $6, make_loc());
            free($6);
        }
    | TOK_ASK TOK_FROM TOK_USER expression TOK_AND TOK_STORE TOK_IN TOK_IDENTIFIER
        {
            $$ = ast_create_ask($4, $8, make_loc());
            free($8);
        }
    ;

/* read from user into name */
/* read name from user */
read_statement
    : TOK_READ TOK_FROM TOK_USER TOK_TO TOK_IDENTIFIER
        {
            $$ = ast_create_read($5, make_loc());
            free($5);
        }
    | TOK_READ TOK_IDENTIFIER TOK_FROM TOK_USER
        {
            $$ = ast_create_read($2, make_loc());
            free($2);
        }
    | TOK_READ TOK_IDENTIFIER
        {
            $$ = ast_create_read($2, make_loc());
            free($2);
        }
    ;

/* ============================================================================
 * SECURE ZONE
 * ============================================================================
 */

/* begin secure zone ... end secure zone */
/* safely do ... end safely */
/* Note: TOK_SECURE lexes "secure zone" as a single token */
secure_zone_statement
    : TOK_BEGIN TOK_SECURE TOK_OP_COLON statement_block TOK_END TOK_SECURE
        {
            ASTNode *body = ast_create_block($4, make_loc());
            $$ = ast_create_secure_zone(body, 1, make_loc());
        }
    | TOK_BEGIN TOK_SECURE statement_block TOK_END TOK_SECURE
        {
            ASTNode *body = ast_create_block($3, make_loc());
            $$ = ast_create_secure_zone(body, 1, make_loc());
        }
    | TOK_SAFELY TOK_DO statement_block TOK_END TOK_SAFELY
        {
            ASTNode *body = ast_create_block($3, make_loc());
            $$ = ast_create_secure_zone(body, 1, make_loc());
        }
    | TOK_RISKY TOK_DO statement_block TOK_END TOK_RISKY
        {
            ASTNode *body = ast_create_block($3, make_loc());
            $$ = ast_create_secure_zone(body, 0, make_loc());
        }
    | TOK_ENTER TOK_SECURE statement_block TOK_END TOK_SECURE
        {
            ASTNode *body = ast_create_block($3, make_loc());
            $$ = ast_create_secure_zone(body, 1, make_loc());
        }
    ;

/* ============================================================================
 * EXPRESSIONS
 * ============================================================================
 */

expression
    : logic_expr
    ;

logic_expr
    : logic_expr TOK_AND comparison
        { $$ = ast_create_binary_op(OP_AND, $1, $3, make_loc()); }
    | logic_expr TOK_OR comparison
        { $$ = ast_create_binary_op(OP_OR, $1, $3, make_loc()); }
    | logic_expr TOK_OP_AND comparison
        { $$ = ast_create_binary_op(OP_AND, $1, $3, make_loc()); }
    | logic_expr TOK_OP_OR comparison
        { $$ = ast_create_binary_op(OP_OR, $1, $3, make_loc()); }
    | TOK_NOT comparison
        { $$ = ast_create_unary_op(OP_NOT, $2, make_loc()); }
    | TOK_OP_NOT comparison
        { $$ = ast_create_unary_op(OP_NOT, $2, make_loc()); }
    | comparison
    ;

comparison
    : comparison TOK_IS TOK_EQUAL TOK_TO term
        { $$ = ast_create_binary_op(OP_EQ, $1, $5, make_loc()); }
    | comparison TOK_IS TOK_NOT TOK_EQUAL TOK_TO term
        { $$ = ast_create_binary_op(OP_NEQ, $1, $6, make_loc()); }
    | comparison TOK_EQUALS term
        { $$ = ast_create_binary_op(OP_EQ, $1, $3, make_loc()); }
    | comparison TOK_OP_EQEQ term
        { $$ = ast_create_binary_op(OP_EQ, $1, $3, make_loc()); }
    | comparison TOK_OP_NEQ term
        { $$ = ast_create_binary_op(OP_NEQ, $1, $3, make_loc()); }
    | comparison TOK_IS TOK_OP_GT term
        { $$ = ast_create_binary_op(OP_GT, $1, $4, make_loc()); }
    | comparison TOK_IS TOK_OP_LT term
        { $$ = ast_create_binary_op(OP_LT, $1, $4, make_loc()); }
    | comparison TOK_OP_GT term
        { $$ = ast_create_binary_op(OP_GT, $1, $3, make_loc()); }
    | comparison TOK_OP_LT term
        { $$ = ast_create_binary_op(OP_LT, $1, $3, make_loc()); }
    | comparison TOK_OP_GTE term
        { $$ = ast_create_binary_op(OP_GTE, $1, $3, make_loc()); }
    | comparison TOK_OP_LTE term
        { $$ = ast_create_binary_op(OP_LTE, $1, $3, make_loc()); }
    /* Natural language comparisons */
    | comparison TOK_IS TOK_GREATER TOK_THAN term
        { $$ = ast_create_binary_op(OP_GT, $1, $5, make_loc()); }
    | comparison TOK_IS TOK_LESS TOK_THAN term
        { $$ = ast_create_binary_op(OP_LT, $1, $5, make_loc()); }
    | comparison TOK_IS TOK_GREATER_THAN term
        { $$ = ast_create_binary_op(OP_GT, $1, $4, make_loc()); }
    | comparison TOK_IS TOK_LESS_THAN term
        { $$ = ast_create_binary_op(OP_LT, $1, $4, make_loc()); }
    | comparison TOK_GREATER_THAN term
        { $$ = ast_create_binary_op(OP_GT, $1, $3, make_loc()); }
    | comparison TOK_LESS_THAN term
        { $$ = ast_create_binary_op(OP_LT, $1, $3, make_loc()); }
    | comparison TOK_EQUAL_TO term
        { $$ = ast_create_binary_op(OP_EQ, $1, $3, make_loc()); }
    | comparison TOK_NOT_EQUAL_TO term
        { $$ = ast_create_binary_op(OP_NEQ, $1, $3, make_loc()); }
    | comparison TOK_IS TOK_EQUAL_TO term
        { $$ = ast_create_binary_op(OP_EQ, $1, $4, make_loc()); }
    | comparison TOK_IS TOK_NOT_EQUAL_TO term
        { $$ = ast_create_binary_op(OP_NEQ, $1, $4, make_loc()); }
    | comparison TOK_IS TOK_GREATER TOK_THAN TOK_OR TOK_EQUAL TOK_TO term
        { $$ = ast_create_binary_op(OP_GTE, $1, $8, make_loc()); }
    | comparison TOK_IS TOK_LESS TOK_THAN TOK_OR TOK_EQUAL TOK_TO term
        { $$ = ast_create_binary_op(OP_LTE, $1, $8, make_loc()); }
    | comparison TOK_IS TOK_AT_LEAST term
        { $$ = ast_create_binary_op(OP_GTE, $1, $4, make_loc()); }
    | comparison TOK_IS TOK_AT_MOST term
        { $$ = ast_create_binary_op(OP_LTE, $1, $4, make_loc()); }
    | comparison TOK_AT_LEAST term
        { $$ = ast_create_binary_op(OP_GTE, $1, $3, make_loc()); }
    | comparison TOK_AT_MOST term
        { $$ = ast_create_binary_op(OP_LTE, $1, $3, make_loc()); }
    /* UNIQUE NATURELANG OPERATOR: "is between X and Y" */
    | comparison TOK_IS TOK_BETWEEN term TOK_AND term
        { $$ = ast_create_ternary_op(OP_BETWEEN, $1, $4, $6, make_loc()); }
    | comparison TOK_BETWEEN term TOK_AND term
        { $$ = ast_create_ternary_op(OP_BETWEEN, $1, $3, $5, make_loc()); }
    | comparison rel_op term
        { $$ = ast_create_binary_op($2, $1, $3, make_loc()); }
    | term
    ;

rel_op
    : TOK_GREATER TOK_THAN   { $$ = OP_GT; }
    | TOK_LESS TOK_THAN      { $$ = OP_LT; }
    ;

term
    : term add_op factor
        { $$ = ast_create_binary_op($2, $1, $3, make_loc()); }
    | term TOK_OP_PLUS factor
        { $$ = ast_create_binary_op(OP_ADD, $1, $3, make_loc()); }
    | term TOK_OP_MINUS factor
        { $$ = ast_create_binary_op(OP_SUB, $1, $3, make_loc()); }
    | factor
    ;

add_op
    : TOK_PLUS      { $$ = OP_ADD; }
    | TOK_MINUS     { $$ = OP_SUB; }
    ;

factor
    : factor mul_op primary
        { $$ = ast_create_binary_op($2, $1, $3, make_loc()); }
    | factor TOK_MULTIPLIED TOK_BY primary
        { $$ = ast_create_binary_op(OP_MUL, $1, $4, make_loc()); }
    | factor TOK_DIVIDED TOK_BY primary
        { $$ = ast_create_binary_op(OP_DIV, $1, $4, make_loc()); }
    | factor TOK_OP_STAR primary
        { $$ = ast_create_binary_op(OP_MUL, $1, $3, make_loc()); }
    | factor TOK_OP_SLASH primary
        { $$ = ast_create_binary_op(OP_DIV, $1, $3, make_loc()); }
    | factor TOK_OP_PERCENT primary
        { $$ = ast_create_binary_op(OP_MOD, $1, $3, make_loc()); }
    | factor TOK_POWER TOK_OF primary
        { $$ = ast_create_binary_op(OP_POW, $1, $4, make_loc()); }
    | factor TOK_POWER primary
        { $$ = ast_create_binary_op(OP_POW, $1, $3, make_loc()); }
    | factor TOK_OP_CARET primary
        { $$ = ast_create_binary_op(OP_POW, $1, $3, make_loc()); }
    | factor TOK_SQUARED
        { 
            ASTNode *two = ast_create_literal_int(2, make_loc());
            $$ = ast_create_binary_op(OP_POW, $1, two, make_loc()); 
        }
    | primary
    ;

mul_op
    : TOK_MULTIPLIED    { $$ = OP_MUL; }
    | TOK_DIVIDED       { $$ = OP_DIV; }
    | TOK_MODULO        { $$ = OP_MOD; }
    | TOK_REMAINDER     { $$ = OP_MOD; }
    ;

primary
    : TOK_INTEGER
        { $$ = ast_create_literal_int($1, make_loc()); }
    | TOK_FLOAT
        { $$ = ast_create_literal_float($1, make_loc()); }
    | TOK_STRING
        { 
            $$ = ast_create_literal_string($1, make_loc());
            free($1);
        }
    | TOK_TRUE
        { $$ = ast_create_literal_bool(1, make_loc()); }
    | TOK_FALSE
        { $$ = ast_create_literal_bool(0, make_loc()); }
    | TOK_YES
        { $$ = ast_create_literal_bool(1, make_loc()); }
    | TOK_NO
        { $$ = ast_create_literal_bool(0, make_loc()); }
    | TOK_IDENTIFIER
        {
            $$ = ast_create_identifier($1, make_loc());
            free($1);
        }
    | TOK_THE TOK_VALUE TOK_OF TOK_IDENTIFIER
        {
            $$ = ast_create_identifier($4, make_loc());
            free($4);
        }
    | function_call
    | list_literal
    | TOK_IDENTIFIER TOK_LBRACKET expression TOK_RBRACKET
        {
            ASTNode *arr = ast_create_identifier($1, make_loc());
            $$ = ast_create_index(arr, $3, make_loc());
            free($1);
        }
    | TOK_IDENTIFIER TOK_AT expression
        {
            ASTNode *arr = ast_create_identifier($1, make_loc());
            $$ = ast_create_index(arr, $3, make_loc());
            free($1);
        }
    | TOK_ITEM expression TOK_OF TOK_IDENTIFIER
        {
            ASTNode *arr = ast_create_identifier($4, make_loc());
            $$ = ast_create_index(arr, $2, make_loc());
            free($4);
        }
    | TOK_GET TOK_ITEM expression TOK_FROM TOK_IDENTIFIER
        {
            ASTNode *arr = ast_create_identifier($5, make_loc());
            $$ = ast_create_index(arr, $3, make_loc());
            free($5);
        }
    | TOK_LENGTH TOK_OF TOK_IDENTIFIER
        {
            ASTNodeList *args = ast_node_list_create();
            ast_node_list_append(args, ast_create_identifier($3, make_loc()));
            $$ = ast_create_func_call("length", args, make_loc());
            free($3);
        }
    | TOK_SIZE TOK_OF TOK_IDENTIFIER
        {
            ASTNodeList *args = ast_node_list_create();
            ast_node_list_append(args, ast_create_identifier($3, make_loc()));
            $$ = ast_create_func_call("length", args, make_loc());
            free($3);
        }
    | TOK_SQUARE TOK_ROOT TOK_OF primary
        {
            ASTNodeList *args = ast_node_list_create();
            ast_node_list_append(args, $4);
            $$ = ast_create_func_call("sqrt", args, make_loc());
        }
    | TOK_ROOT primary
        {
            ASTNodeList *args = ast_node_list_create();
            ast_node_list_append(args, $2);
            $$ = ast_create_func_call("sqrt", args, make_loc());
        }
    | TOK_LPAREN expression TOK_RPAREN
        { $$ = $2; }
    | TOK_OP_MINUS primary
        { $$ = ast_create_unary_op(OP_NEG, $2, make_loc()); }
    | TOK_MINUS primary
        { $$ = ast_create_unary_op(OP_NEG, $2, make_loc()); }
    ;

/* call add with 5 and 10 */
/* call greet with "Hello" */
function_call
    : TOK_CALL TOK_IDENTIFIER TOK_WITH arg_list
        {
            $$ = ast_create_func_call($2, $4, make_loc());
            free($2);
        }
    | TOK_CALL TOK_IDENTIFIER
        {
            $$ = ast_create_func_call($2, ast_node_list_create(), make_loc());
            free($2);
        }
    | TOK_IDENTIFIER TOK_LPAREN arg_list TOK_RPAREN
        {
            $$ = ast_create_func_call($1, $3, make_loc());
            free($1);
        }
    | TOK_IDENTIFIER TOK_LPAREN TOK_RPAREN
        {
            $$ = ast_create_func_call($1, ast_node_list_create(), make_loc());
            free($1);
        }
    ;

arg_list
    : expression
        {
            $$ = ast_node_list_create();
            ast_node_list_append($$, $1);
        }
    | arg_list TOK_AND expression
        {
            ast_node_list_append($1, $3);
            $$ = $1;
        }
    | arg_list TOK_COMMA expression
        {
            ast_node_list_append($1, $3);
            $$ = $1;
        }
    ;

/* [1, 2, 3, 4, 5] */
list_literal
    : TOK_LBRACKET expr_list TOK_RBRACKET
        { $$ = ast_create_list($2, make_loc()); }
    | TOK_LBRACKET TOK_RBRACKET
        { $$ = ast_create_list(ast_node_list_create(), make_loc()); }
    ;

expr_list
    : expression
        {
            $$ = ast_node_list_create();
            ast_node_list_append($$, $1);
        }
    | expr_list TOK_COMMA expression
        {
            ast_node_list_append($1, $3);
            $$ = $1;
        }
    ;

%%

/* ============================================================================
 * GLR MERGE FUNCTION
 * When ambiguous parses produce the same non-terminal, pick one
 * ============================================================================
 */

static YYSTYPE merge_nodes(YYSTYPE x0, YYSTYPE x1) {
    /* In case of ambiguous parses, prefer the first interpretation */
    (void)x1; /* suppress unused warning */
    return x0;
}

/* ============================================================================
 * ERROR HANDLING
 * ============================================================================
 */

void yyerror(const char *s) {
    fprintf(stderr, "Parse error at line %d: %s (near '%s')\n", 
            yylineno, s, yytext);
}

/* ============================================================================
 * PUBLIC API
 * ============================================================================
 */

ASTNode *naturelang_parse(FILE *input) {
    yyin = input;
    parse_result = NULL;
    
    if (yyparse() != 0) {
        return NULL;
    }
    
    return parse_result;
}

ASTNode *naturelang_parse_string(const char *source) {
    /* Create a temporary file for parsing */
    FILE *tmp = tmpfile();
    if (tmp == NULL) {
        fprintf(stderr, "Error: Could not create temporary file\n");
        return NULL;
    }
    
    fputs(source, tmp);
    rewind(tmp);
    
    ASTNode *result = naturelang_parse(tmp);
    
    fclose(tmp);
    return result;
}

ASTNode *get_parse_result(void) {
    return parse_result;
}
