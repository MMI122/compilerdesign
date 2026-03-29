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
/*
 * %code requires ব্লকের ভেতরের include/header declarations
 * generated parser header-এও যুক্ত হয়।
 *
 * কেন দরকার:
 * - YYSTYPE-এ ASTNode, ASTNodeList, DataType, Operator ব্যবহার হচ্ছে
 * - তাই parser header ব্যবহার করা যে কোনো C file-কে ast.h type-গুলো জানতে হবে
 */
%code requires {
#include "ast.h"
}

%{
/*
 * এই %{ ... %} অংশের C কোড parser implementation (.tab.c)-এ কপি হয়।
 * এখানে helper function, global parser state, extern symbols রাখা হয়।
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

/*
 * Forward declarations:
 * - yylex: parser token চাইলে lexer এই function দিয়ে next token দেয়
 * - yylineno: lexer maintain করা current line number
 * - yytext: শেষ match হওয়া token-এর raw text
 * - yyin: lexer কোন FILE* থেকে পড়বে
 */
extern int yylex(void);
extern int yylineno;
extern char *yytext;
extern FILE *yyin;

/* Bison parse error হলে yyerror callback invoke হয় */
void yyerror(const char *s);

/*
 * parse_result:
 * - yyparse() int return করে (success/fail), AST সরাসরি return করে না
 * - তাই root ASTNode parser state-এ ধরে রাখা হয়
 */
static ASTNode *parse_result = NULL;

/*
 * make_loc(): বর্তমান token অবস্থান থেকে SourceLocation বানায়।
 *
 * বর্তমানে line-level তথ্য (first_line) প্রধানত ভরা হচ্ছে;
 * ভবিষ্যতে চাইলে column/last_line/filename-ও lexer location tracking থেকে ভরা যাবে।
 */
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
    /* integer literal semantic value */
    long long int_val;
    /* float literal semantic value */
    double float_val;
    /* string/identifier token text */
    char *str_val;
    /* single character token value (প্রয়োজনে) */
    char char_val;
    /* boolean token value */
    int bool_val;
    /* AST node pointer */
    ASTNode *node;
    /* list of AST nodes */
    ASTNodeList *list;
    /* semantic data type */
    DataType dtype;
    /* operator enum value */
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
/*
 * %type mapping very important:
 * - কোন non-terminal reduce হয়ে YYSTYPE-এর কোন field ব্যবহার করবে সেটা নির্ধারণ করে
 * - semantic action-এ $$, $1, $2 type-safe ভাবে ব্যবহার করতে সাহায্য করে
 */
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
/*
 * precedence/associativity declarations conflict কমাতে সাহায্য করে:
 * - %left: left-associative operators
 * - %right: right-associative operators
 * - %nonassoc: একই precedence-এ chain disallow
 *
 * উপরে থেকে নিচে যেতে precedence শক্তিশালী হয়।
 */
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

/*
 * NOTE on $$ / $n notation (global cheat-sheet):
 * - $$ = বর্তমান production reduce হওয়ার final semantic value
 * - $1 = production-এর ১ম symbol-এর semantic value
 * - $2 = production-এর ২য় symbol-এর semantic value
 * - ...
 *
 * Example:
 *   A : B C D { $$ = f($1, $2, $3); }
 *
 * এখানে A-এর value হচ্ছে f(B_value, C_value, D_value)
 */

%%

/* ============================================================================
 * GRAMMAR RULES
 * ============================================================================
 */

/*
 * program হলো সম্পূর্ণ পার্সিং-এর root non-terminal।
 * এখানে statement_list থেকে পাওয়া স্টেটমেন্টগুলো একত্র করে
 * একটি AST_PROGRAM node তৈরি করা হয়।
 */

program
    : statement_list
        {
            /* $$ = এই rule (program)-এর final semantic value */
            /* $1 = statement_list non-terminal থেকে পাওয়া semantic value */
            ASTNodeList *stmts = ast_node_list_create();
            if ($1 != NULL) {
                /* single statement/ব্লক যাই হোক, program statement list-এ ঢোকাও */
                ast_node_list_append(stmts, $1);
            }
            /* parse_result-এ root AST_PROGRAM node ধরে রাখা হয় */
            parse_result = ast_create_program(stmts, make_loc());
            /* program rule-এর আউটপুট হিসেবে root node ফেরত */
            $$ = parse_result;
        }
    ;

/*
 * statement_list rule-টা এই grammar-এর খুব গুরুত্বপূর্ণ recursive rule।
 * সহজভাবে: "অনেকগুলো statement কীভাবে একটার পর একটা জড়ো হবে" সেটা এখানে নির্ধারিত।
 *
 * Case 1: empty
 *   - ইনপুটে এখনো কোনো statement পাওয়া না গেলে $$ = NULL করা হয়।
 *   - অর্থাৎ "এখনো list তৈরি হয়নি" এই অবস্থা বোঝানো হয়।
 *
 * Case 2: statement_list statement opt_terminator
 *   - $1 = আগের পর্যন্ত parse হওয়া statement_list-এর semantic value
 *   - $2 = নতুন parse হওয়া statement
 *   - $3 = optional terminator (dot/newline/semicolon/empty), সাধারণত AST বানাতে লাগে না
 *
 * Why recursion?
 *   - recursion ছাড়া unlimited সংখ্যক statement parse করা যেত না।
 *   - প্রতিবার একটি নতুন statement আসলে আগের ফলাফলের সাথে merge করা হয়।
 *
 * Merge strategy:
 *   - আগে কিছু না থাকলে ($1 == NULL), নতুন statement ($2)-ই current result
 *   - আগে থেকে AST_BLOCK থাকলে, $2 সেই block-এর ভিতরে append হয়
 *   - আগে block না থাকলে, নতুন AST_BLOCK তৈরি করে $1 এবং $2 দুটোই রাখা হয়
 *   - যদি $2 NULL হয় (edge case), আগের ফল ($1) 그대로 রাখা হয়
 */

statement_list
    : /* empty */
        /* empty statement list => কোনো AST statement নেই */
        { $$ = NULL; }
    | statement_list statement opt_terminator
        {
            /*
             * semantic value notation:
             *   - $1: production-এর প্রথম অংশ (statement_list)
             *   - $2: production-এর দ্বিতীয় অংশ (statement)
             *   - $3: production-এর তৃতীয় অংশ (opt_terminator)
             *   - $$: পুরো rule reduce হওয়ার পরে final output value
             */
            if ($1 == NULL) {
                /*
                 * এখন পর্যন্ত কোনো list তৈরি হয়নি।
                 * তাই বর্তমান statement-ই এই মুহূর্তে statement_list-এর result।
                 */
                $$ = $2;
            } else if ($2 != NULL) {
                /*
                 * এই branch মানে: আগেও কিছু ছিল, নতুন statement-ও আছে।
                 * এখন এগুলোকে একই container-এ রাখতে হবে।
                 */
                if ($1->type == AST_BLOCK) {
                /*
                     * $1 যদি আগে থেকেই AST_BLOCK হয়,
                     * তাহলে নতুন block বানানোর দরকার নেই।
                     * existing block-এ নতুন statement append করলেই হবে।
                     */
                    ast_node_list_append($1->data.block.statements, $2);
            $$ = $1;
            } else {
                    /*
                     * $1 যদি single statement হয় (block না হয়),
                     * তাহলে multi-statement represent করার জন্য এখন block বানাতে হবে।
                     * নতুন block list-এ প্রথমে $1, পরে $2 রাখা হয়।
                     */
                    ASTNodeList *stmts = ast_node_list_create();
                    ast_node_list_append(stmts, $1);
            ast_node_list_append(stmts, $2);
            $$ = ast_create_block(stmts, make_loc());
                }
            } else {
                /*
                 * defensive fallback: নতুন statement NULL হলে
                 * আগের accumulated result ($1) unchanged রাখা হয়।
                 */
                $$ = $1;
            }
        }
    ;

/*
 * opt_terminator language syntax-কে flexible করে।
 * মানে statement শেষ করার জন্য কড়া single symbol বাধ্যতামূলক নয়।
 *
 * allowed endings:
 * - empty: একই লাইনে বা grammar context থেকে boundary বোঝা গেলে
 * - TOK_DOT: sentence-style ending
 * - TOK_NEWLINE: line-by-line style
 * - TOK_SEMICOLON: traditional programming style
 */

opt_terminator
    : /* empty */
    | TOK_DOT
    | TOK_NEWLINE
    | TOK_SEMICOLON
    ;

/*
 * statement হলো "একটি executable unit"-এর umbrella rule।
 * parser যখন program body parse করে, প্রতিটি entry শেষ পর্যন্ত statement হিসেবে reduce হয়।
 *
 * direct pass-through cases:
 *   declaration, assignment, if/while/repeat/for_each, function_decl,
 *   return/display/ask/read, secure_zone
 *
 * special cases:
 * - function_call নিজে expression; কিন্তু standalone line হলে
 *   semantic AST-এ statement node দরকার হয়,
 *   তাই ast_create_expr_stmt(...) দিয়ে wrap করা হয়।
 * - TOK_STOP => break node
 * - TOK_SKIP => continue node
 */

statement
    /*
     * এই alternatives-এ সাধারণভাবে $$ implicit pass-through behavior নেয়,
     * অর্থাৎ child non-terminal থেকে পাওয়া ASTNode-ই statement result হয়।
     */
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
        /* function_call expression হলেও standalone statement হিসেবে wrap করা হয় */
        { $$ = ast_create_expr_stmt($1, make_loc()); }    | TOK_STOP
        /* stop => break statement AST */
        { $$ = ast_create_break(make_loc()); }
    | TOK_SKIP
        /* skip => continue statement AST */
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
/*
 * declaration rule ভ্যারিয়েবল/লিস্ট ডিক্লারেশন পার্স করে।
 * বিভিন্ন natural-language syntax (called/named/make/constant)
 * এক জায়গা থেকে AST_VAR_DECL node তৈরি করে।
 */
declaration
    : TOK_CREATE article type_specifier TOK_CALLED TOK_IDENTIFIER TOK_AND TOK_SET expression
        {
            /* $5=name, $3=type, $8=initializer expression, 0=non-constant */
            $$ = ast_create_var_decl($5, $3, $8, 0, make_loc());
            /* lexer allocated identifier string; AST constructor copy ধরে নেয় */
            free($5);
            }
    | TOK_CREATE article type_specifier TOK_CALLED TOK_IDENTIFIER
        {
            /* initializer নেই => NULL */
            $$ = ast_create_var_decl($5, $3, NULL, 0, make_loc());
            free($5);
            }
    | TOK_CREATE article type_specifier TOK_NAMED TOK_IDENTIFIER TOK_AND TOK_SET expression
        {
            /* called এর বদলে named syntax; mapping একই */
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
            /* make ... constant ... => is_const = 1 */
            $$ = ast_create_var_decl($2, $5, $8, 1, make_loc());
            free($2);
            }
    | TOK_CREATE article TOK_CONSTANT type_specifier TOK_CALLED TOK_IDENTIFIER TOK_AND TOK_SET expression
        {
            /* create constant syntax; type=$4, name=$6, init=$9 */
            $$ = ast_create_var_decl($6, $4, $9, 1, make_loc());
            free($6);
            }
    | TOK_CREATE article TOK_TYPE_LIST TOK_OF type_specifier TOK_CALLED TOK_IDENTIFIER
        {
            /* list declaration without explicit initializer */
            $$ = ast_create_var_decl($7, TYPE_LIST, NULL, 0, make_loc());
            free($7);
            }
    | TOK_CREATE article TOK_TYPE_LIST TOK_CALLED TOK_IDENTIFIER TOK_WITH expr_list
        {
            /* $7 = expr_list => list literal node বানিয়ে initializer হিসেবে দাও */
            ASTNode *init = ast_create_list($7, make_loc());
            $$ = ast_create_var_decl($5, TYPE_LIST, init, 0, make_loc());
            free($5);
            }
    ;

/*
 * article optional determiner (a, an, the) গ্রহণ করে।
 * natural language flexible রাখতে empty কেসও অনুমোদিত।
 */

article
    : TOK_A
    | TOK_AN
    | TOK_THE
    | /* empty */
    ;

/* Type specifiers using TYPE_ prefixed tokens from tokens.h */
/*
 * type_specifier lexed type token-কে DataType enum-এ map করে,
 * যাতে AST-তে canonical type সংরক্ষণ করা যায়।
 */
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
/*
 * assignment rule ভিন্ন ভিন্ন লিখনরীতি থেকে assignment AST তৈরি করে।
 * target হতে পারে simple identifier অথবা indexed element।
 */
assignment
    : TOK_SET TOK_IDENTIFIER TOK_TO expression
        {
            /* target variable node তৈরি */
            ASTNode *target = ast_create_identifier($2, make_loc());
            /* assignment node: target = value */
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
            /* array identifier */
            ASTNode *arr = ast_create_identifier($2, make_loc());
            /* indexed target: arr[index] */
            ASTNode *target = ast_create_index(arr, $4, make_loc());
            /* arr[index] = value */
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
/*
 * if_statement condition, then-block, optional else-block নিয়ে
 * AST_IF node তৈরি করে।
 */
if_statement
    : TOK_IF expression TOK_THEN statement_block opt_else TOK_END TOK_IF
        {
            /* $4 = statement_block (ASTNodeList*) => AST_BLOCK node */
            ASTNode *then_block = ast_create_block($4, make_loc());
            /* $2 = condition, $5 = optional else block */
            $$ = ast_create_if($2, then_block, $5, make_loc());
            }
    | TOK_IF expression TOK_THEN statement_block opt_else TOK_END
        {
            ASTNode *then_block = ast_create_block($4, make_loc());
            $$ = ast_create_if($2, then_block, $5, make_loc());
            }
    ;

/*
 * opt_else optional else/otherwise branch গ্রহণ করে।
 * branch থাকলে সেটাকে block node-এ রূপান্তর করা হয়।
 */

opt_else
    : /* empty */
        /* else branch না থাকলে NULL */
        { $$ = NULL; }
    | TOK_OTHERWISE statement_block
        {
            /* statement list-কে AST_BLOCK এ রূপান্তর */
            $$ = ast_create_block($2, make_loc());
            }
    | TOK_ELSE statement_block
        {
            $$ = ast_create_block($2, make_loc());
            }
    ;

/* while x is less than 100 do ... end while */
/* while condition do ... end */
/*
 * while_statement loop condition ও body block সংগ্রহ করে
 * AST_WHILE node তৈরি করে।
 */
while_statement
    : TOK_WHILE expression TOK_DO statement_block TOK_END TOK_WHILE
        {
            ASTNode *body = ast_create_block($4, make_loc());
            /* $2 = loop condition, body = loop block */
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
/*
 * repeat_statement গণনাভিত্তিক লুপের জন্য ব্যবহৃত হয়।
 * count expression এবং body block মিলে AST_REPEAT node হয়।
 */
repeat_statement
    : TOK_REPEAT expression TOK_TIMES statement_block TOK_END TOK_REPEAT
        {
            ASTNode *body = ast_create_block($4, make_loc());
            /* $2 বার body execute করার semantic */
            $$ = ast_create_repeat($2, body, make_loc());
            }
    | TOK_REPEAT expression TOK_TIMES statement_block TOK_END
        {
            ASTNode *body = ast_create_block($4, make_loc());
            $$ = ast_create_repeat($2, body, make_loc());
            }
    ;

/* for each item in mylist do ... end for */
/*
 * for_each_statement iterable-এর উপর iteration বোঝায়।
 * iterator নাম, iterable expression, body block নিয়ে AST_FOR_EACH বানায়।
 */
for_each_statement
    : TOK_FOR TOK_EACH TOK_IDENTIFIER TOK_IN expression TOK_DO statement_block TOK_END TOK_FOR
        {
            ASTNode *body = ast_create_block($7, make_loc());
            /* iterator নাম=$3, iterable expr=$5 */
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

/*
 * statement_block একটি compound block-এর ভেতরের statement সংগ্রহ করে।
 * empty block বৈধ; নতুবা recursive append দ্বারা list বড় হয়।
 */

statement_block
    : /* empty */
        /* খালি block => খালি statement list */
        { $$ = ast_node_list_create(); }
    | statement_block statement opt_terminator
        {
            /* $1 list-এ নতুন statement ($2) append */
            ast_node_list_append($1, $2);
            /* updated list-ই এই rule-এর result */
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
/*
 * function_decl rule function definition-এর একাধিক natural syntax সমর্থন করে।
 * parameters, return type (না থাকলে nothing), এবং body block নিয়ে
 * AST_FUNC_DECL node তৈরি হয়।
 */
function_decl
    : TOK_DEFINE article TOK_FUNCTION TOK_CALLED TOK_IDENTIFIER TOK_THAT TOK_TAKES param_list TOK_AND TOK_RETURNS type_specifier TOK_OP_COLON statement_block TOK_END TOK_FUNCTION
        {
            /* $13 = body statements => block node */
            ASTNode *body = ast_create_block($13, make_loc());
            /* name=$5, params=$8, return_type=$11 */
            $$ = ast_create_func_decl($5, $8, $11, body, make_loc());
            free($5);
            }
    | TOK_DEFINE article TOK_FUNCTION TOK_CALLED TOK_IDENTIFIER TOK_THAT TOK_TAKES param_list TOK_OP_COLON statement_block TOK_END TOK_FUNCTION
        {
            ASTNode *body = ast_create_block($10, make_loc());
            /* return type omitted => TYPE_NOTHING */
            $$ = ast_create_func_decl($5, $8, TYPE_NOTHING, body, make_loc());
            free($5);
            }
    | TOK_DEFINE article TOK_FUNCTION TOK_CALLED TOK_IDENTIFIER TOK_OP_COLON statement_block TOK_END TOK_FUNCTION
        {
            ASTNode *body = ast_create_block($7, make_loc());
            /* params omitted => NULL, return omitted => nothing */
            $$ = ast_create_func_decl($5, NULL, TYPE_NOTHING, body, make_loc());
            free($5);
            }
    /* Flexible syntax without "called" - "define a function NAME that takes..." */
    | TOK_DEFINE article TOK_FUNCTION TOK_IDENTIFIER TOK_THAT TOK_TAKES param_list TOK_AND TOK_RETURNS type_specifier statement_block TOK_END TOK_FUNCTION
        {
            /* symbol index recap: $4=name, $7=params, $10=return type, $11=body-list */
            ASTNode *body = ast_create_block($11, make_loc());
            $$ = ast_create_func_decl($4, $7, $10, body, make_loc());
            free($4);
            }
    | TOK_DEFINE article TOK_FUNCTION TOK_IDENTIFIER TOK_THAT TOK_TAKES param_list statement_block TOK_END TOK_FUNCTION
        {
            /* return type omitted => TYPE_NOTHING; body-list at $8 */
            ASTNode *body = ast_create_block($8, make_loc());
            $$ = ast_create_func_decl($4, $7, TYPE_NOTHING, body, make_loc());
            free($4);
            }
    | TOK_DEFINE article TOK_FUNCTION TOK_IDENTIFIER statement_block TOK_END TOK_FUNCTION
        {
            /* shortest form: only name + body; params=NULL, return=TYPE_NOTHING */
            ASTNode *body = ast_create_block($5, make_loc());
            $$ = ast_create_func_decl($4, NULL, TYPE_NOTHING, body, make_loc());
            free($4);
            }
    ;

/*
 * param_list function parameterগুলোর তালিকা তৈরি করে।
 * type থাকলে নির্দিষ্ট type, না থাকলে TYPE_UNKNOWN ব্যবহার করা হয়।
 */

param_list
    : type_specifier TOK_IDENTIFIER
        {
            /* প্রথম parameter আসলে নতুন list তৈরি */
            $$ = ast_node_list_create();
            /* $2=name, $1=type */
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
            /* বিদ্যমান list ($1)-এ নতুন typed parameter append */
            ASTNode *param = ast_create_param_decl($4, $3, make_loc());
            ast_node_list_append($1, param);
            $$ = $1;
            free($4);
            }
    | param_list TOK_COMMA TOK_IDENTIFIER
        {
            /* type না থাকায় TYPE_UNKNOWN ধরা হচ্ছে */
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
/*
 * return_statement function থেকে value ফেরত দেওয়ার rule।
 * value সহ return অথবা explicit nothing return উভয়ই সমর্থিত।
 */
return_statement
    : TOK_RETURN expression
        /* return <expr> */
        { $$ = ast_create_return($2, make_loc()); }    | TOK_RETURN TOK_THE expression
        /* return the <expr> */
        { $$ = ast_create_return($3, make_loc()); }    | TOK_RETURN TOK_TYPE_NOTHING
        /* explicit void return */
        { $$ = ast_create_return(NULL, make_loc()); }
    | TOK_GIVE expression
        /* synonym: give <expr> */
        { $$ = ast_create_return($2, make_loc()); }    | TOK_GIVE TOK_TYPE_NOTHING
        /* synonym form of void return */
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
/*
 * display_statement একাধিক synonym (display/show/print) সমর্থন করে
 * এবং AST_DISPLAY node তৈরি করে।
 *
 * semantic-value quick reference (এই rule-এর context):
 * - $$ : display_statement rule reduce হওয়ার পর final ASTNode* result
 * - $2 : দ্বিতীয় symbol-এর semantic value (সাধারণত expression node)
 * - $5 : পঞ্চম symbol-এর semantic value (expression node)
 * - make_loc() : current parse location (line/position) থেকে SourceLocation বানায়,
 *   যাতে AST node-এ error-reporting metadata থাকে
 */
display_statement
    : TOK_DISPLAY expression
        /*
         * Token mapping:
         * - TOK_DISPLAY = 'display' keyword
         * - expression   = যা দেখাতে হবে (ASTNode*)
         * এখানে expression দ্বিতীয় symbol, তাই value পাওয়া যায় $2 থেকে
         */
        /* $$ তে final display node রাখা হচ্ছে */
        { $$ = ast_create_display($2, make_loc()); }    | TOK_DISPLAY TOK_THE TOK_VALUE TOK_OF expression
        /*
         * Natural-language long form: "display the value of <expr>"
         * Symbols:
         *   1: TOK_DISPLAY
         *   2: TOK_THE
         *   3: TOK_VALUE
         *   4: TOK_OF
         *   5: expression
         * তাই expression-এর semantic value এখানে $5
         */
        /* $5 expression এবং make_loc() দিয়ে AST_DISPLAY node তৈরি */
        { $$ = ast_create_display($5, make_loc()); }    | TOK_SHOW expression
        /* 'show <expr>' variant; expression দ্বিতীয় symbol => $2 */
        { $$ = ast_create_display($2, make_loc()); }    | TOK_PRINT expression
        /* 'print <expr>' variant; expression দ্বিতীয় symbol => $2 */
        { $$ = ast_create_display($2, make_loc()); }    ;

/* ask "What is your name?" and store in name */
/* ask for user's name and store in name */
/*
 * ask_statement prompt expression নিয়ে user input নেওয়ার statement।
 * target variable-এ ইনপুট সংরক্ষণ করার AST_ASK node তৈরি হয়।
 */
ask_statement
    : TOK_ASK expression TOK_AND TOK_STORE TOK_IN TOK_IDENTIFIER
        {
            /* $2 prompt, $6 target variable name */
            $$ = ast_create_ask($2, $6, make_loc());
            free($6);
            }
    | TOK_ASK TOK_FROM TOK_USER expression TOK_AND TOK_STORE TOK_IN TOK_IDENTIFIER
        {
            /* long form-এ prompt expression পজিশন $4, target variable $8 */
            $$ = ast_create_ask($4, $8, make_loc());
            free($8);
            }
    ;

/* read from user into name */
/* read name from user */
/*
 * read_statement সরাসরি user input পড়ে target variable-এ রাখে,
 * এবং AST_READ node তৈরি করে।
 */
read_statement
    : TOK_READ TOK_FROM TOK_USER TOK_TO TOK_IDENTIFIER
        {
            /* read input and store into target variable */
            $$ = ast_create_read($5, make_loc());
            free($5);
            }
    | TOK_READ TOK_IDENTIFIER TOK_FROM TOK_USER
        {
            /* read x from user -> target identifier at $2 */
            $$ = ast_create_read($2, make_loc());
            free($2);
            }
    | TOK_READ TOK_IDENTIFIER
        {
            /* shortest form: read x */
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
/*
 * secure_zone_statement নিরাপদ/ঝুঁকিপূর্ণ execution block ঘোষণা করে।
 * is_safe flag দিয়ে semantic স্তরে নীতিগত আচরণ নিয়ন্ত্রণ করা যায়।
 */
secure_zone_statement
    : TOK_BEGIN TOK_SECURE TOK_OP_COLON statement_block TOK_END TOK_SECURE
        {
            ASTNode *body = ast_create_block($4, make_loc());
            /* 1 => safe mode */
            $$ = ast_create_secure_zone(body, 1, make_loc());
        }
    | TOK_BEGIN TOK_SECURE statement_block TOK_END TOK_SECURE
        {
            ASTNode *body = ast_create_block($3, make_loc());
            /* colon ছাড়া form-ও safe mode হিসেবে ধরা হয় */
            $$ = ast_create_secure_zone(body, 1, make_loc());
        }
    | TOK_SAFELY TOK_DO statement_block TOK_END TOK_SAFELY
        {
            ASTNode *body = ast_create_block($3, make_loc());
            /* safely do ... => safe mode */
            $$ = ast_create_secure_zone(body, 1, make_loc());
        }
    | TOK_RISKY TOK_DO statement_block TOK_END TOK_RISKY
        {
            ASTNode *body = ast_create_block($3, make_loc());
            /* 0 => risky mode */
            $$ = ast_create_secure_zone(body, 0, make_loc());
        }
    | TOK_ENTER TOK_SECURE statement_block TOK_END TOK_SECURE
        {
            ASTNode *body = ast_create_block($3, make_loc());
            /* enter secure ... => safe mode */
            $$ = ast_create_secure_zone(body, 1, make_loc());
        }
    ;

/* ============================================================================
 * EXPRESSIONS
 * ============================================================================
 */

/*
 * expression হলো expression parsing-এর entry point;
 * বর্তমান grammar-এ এটি logic_expr-এ delegate করে।
 */

expression
    : logic_expr
    ;

/*
 * logic_expr logical AND/OR/NOT expression পার্স করে।
 * precedence hierarchy বজায় রাখতে নিচে comparison non-terminal ব্যবহৃত হয়।
 */

logic_expr
    : logic_expr TOK_AND comparison
        /* $1 AND $3 */
        { $$ = ast_create_binary_op(OP_AND, $1, $3, make_loc()); }    | logic_expr TOK_OR comparison
        /* $1 OR $3 */
        { $$ = ast_create_binary_op(OP_OR, $1, $3, make_loc()); }    | logic_expr TOK_OP_AND comparison
        { $$ = ast_create_binary_op(OP_AND, $1, $3, make_loc()); }    | logic_expr TOK_OP_OR comparison
        { $$ = ast_create_binary_op(OP_OR, $1, $3, make_loc()); }    | TOK_NOT comparison
        /* NOT $2 */
        { $$ = ast_create_unary_op(OP_NOT, $2, make_loc()); }    | TOK_OP_NOT comparison
        { $$ = ast_create_unary_op(OP_NOT, $2, make_loc()); }    | comparison
    ;

/*
 * comparison rule equality, relational, এবং natural-language comparison
 * variantগুলোকে একত্রে parse করে।
 * "between" এর জন্য ternary operator node তৈরি করা হয়।
 */

comparison
    : comparison TOK_IS TOK_EQUAL TOK_TO term
        /* phrase: <lhs> is equal to <rhs>; lhs=$1, rhs=$5 */
        { $$ = ast_create_binary_op(OP_EQ, $1, $5, make_loc()); }    | comparison TOK_IS TOK_NOT TOK_EQUAL TOK_TO term
        /* phrase: <lhs> is not equal to <rhs>; rhs এখানে $6 */
        { $$ = ast_create_binary_op(OP_NEQ, $1, $6, make_loc()); }    | comparison TOK_EQUALS term
        /* shorthand equals */
        { $$ = ast_create_binary_op(OP_EQ, $1, $3, make_loc()); }    | comparison TOK_OP_EQEQ term
        { $$ = ast_create_binary_op(OP_EQ, $1, $3, make_loc()); }    | comparison TOK_OP_NEQ term
        { $$ = ast_create_binary_op(OP_NEQ, $1, $3, make_loc()); }    | comparison TOK_IS TOK_OP_GT term
        /* symbolic greater-than with 'is' */
        { $$ = ast_create_binary_op(OP_GT, $1, $4, make_loc()); }    | comparison TOK_IS TOK_OP_LT term
        { $$ = ast_create_binary_op(OP_LT, $1, $4, make_loc()); }    | comparison TOK_OP_GT term
        { $$ = ast_create_binary_op(OP_GT, $1, $3, make_loc()); }    | comparison TOK_OP_LT term
        { $$ = ast_create_binary_op(OP_LT, $1, $3, make_loc()); }    | comparison TOK_OP_GTE term
        { $$ = ast_create_binary_op(OP_GTE, $1, $3, make_loc()); }    | comparison TOK_OP_LTE term
        { $$ = ast_create_binary_op(OP_LTE, $1, $3, make_loc()); }    /* Natural language comparisons */
    | comparison TOK_IS TOK_GREATER TOK_THAN term
        { $$ = ast_create_binary_op(OP_GT, $1, $5, make_loc()); }    | comparison TOK_IS TOK_LESS TOK_THAN term
        { $$ = ast_create_binary_op(OP_LT, $1, $5, make_loc()); }    | comparison TOK_IS TOK_GREATER_THAN term
        { $$ = ast_create_binary_op(OP_GT, $1, $4, make_loc()); }    | comparison TOK_IS TOK_LESS_THAN term
        { $$ = ast_create_binary_op(OP_LT, $1, $4, make_loc()); }    | comparison TOK_GREATER_THAN term
        { $$ = ast_create_binary_op(OP_GT, $1, $3, make_loc()); }    | comparison TOK_LESS_THAN term
        { $$ = ast_create_binary_op(OP_LT, $1, $3, make_loc()); }    | comparison TOK_EQUAL_TO term
        { $$ = ast_create_binary_op(OP_EQ, $1, $3, make_loc()); }    | comparison TOK_NOT_EQUAL_TO term
        { $$ = ast_create_binary_op(OP_NEQ, $1, $3, make_loc()); }    | comparison TOK_IS TOK_EQUAL_TO term
        { $$ = ast_create_binary_op(OP_EQ, $1, $4, make_loc()); }    | comparison TOK_IS TOK_NOT_EQUAL_TO term
        { $$ = ast_create_binary_op(OP_NEQ, $1, $4, make_loc()); }    | comparison TOK_IS TOK_GREATER TOK_THAN TOK_OR TOK_EQUAL TOK_TO term
        /* natural form of >= ; rhs is $8 কারণ phrase দীর্ঘ */
        { $$ = ast_create_binary_op(OP_GTE, $1, $8, make_loc()); }    | comparison TOK_IS TOK_LESS TOK_THAN TOK_OR TOK_EQUAL TOK_TO term
        /* natural form of <= ; rhs is $8 */
        { $$ = ast_create_binary_op(OP_LTE, $1, $8, make_loc()); }    | comparison TOK_IS TOK_AT_LEAST term
        { $$ = ast_create_binary_op(OP_GTE, $1, $4, make_loc()); }    | comparison TOK_IS TOK_AT_MOST term
        { $$ = ast_create_binary_op(OP_LTE, $1, $4, make_loc()); }    | comparison TOK_AT_LEAST term
        { $$ = ast_create_binary_op(OP_GTE, $1, $3, make_loc()); }    | comparison TOK_AT_MOST term
        { $$ = ast_create_binary_op(OP_LTE, $1, $3, make_loc()); }    /* UNIQUE NATURELANG OPERATOR: "is between X and Y" */
    | comparison TOK_IS TOK_BETWEEN term TOK_AND term
        /* between form: operand=$1, lower=$4, upper=$6 */
        { $$ = ast_create_ternary_op(OP_BETWEEN, $1, $4, $6, make_loc()); }    | comparison TOK_BETWEEN term TOK_AND term
        /* shorthand between */
        { $$ = ast_create_ternary_op(OP_BETWEEN, $1, $3, $5, make_loc()); }    | comparison rel_op term
        /* rel_op rule থেকে $2 already OP_GT/OP_LT enum দিয়ে আসে */
        { $$ = ast_create_binary_op($2, $1, $3, make_loc()); }    | term
    ;

/*
 * rel_op helper non-terminal; token pair থেকে relational operator enum নেয়।
 */

rel_op
    : TOK_GREATER TOK_THAN
        /* greater than phrase => operator enum OP_GT */
        { $$ = OP_GT; }
    | TOK_LESS TOK_THAN
        /* less than phrase => operator enum OP_LT */
        { $$ = OP_LT; }
    ;

/*
 * term additive স্তরের expression (plus/minus) পার্স করে।
 * left recursion ব্যবহারে left-associative আচরণ পাওয়া যায়।
 */

term
    : term add_op factor
        /* operator $2 (OP_ADD/OP_SUB) দিয়ে binary node */
        { $$ = ast_create_binary_op($2, $1, $3, make_loc()); }    | term TOK_OP_PLUS factor
        { $$ = ast_create_binary_op(OP_ADD, $1, $3, make_loc()); }    | term TOK_OP_MINUS factor
        { $$ = ast_create_binary_op(OP_SUB, $1, $3, make_loc()); }    | factor
    ;

/*
 * add_op helper rule; keyword token-কে arithmetic operator enum-এ map করে।
 */

add_op
    : TOK_PLUS
        /* plus keyword => OP_ADD */
        { $$ = OP_ADD; }
    | TOK_MINUS
        /* minus keyword => OP_SUB */
        { $$ = OP_SUB; }
    ;

/*
 * factor multiplicative/power স্তরের expression parse করে:
 * multiply/divide/mod/power এবং squared shorthand সহ।
 */

factor
    : factor mul_op primary
        /* operator $2 (OP_MUL/OP_DIV/OP_MOD) দিয়ে binary node */
        { $$ = ast_create_binary_op($2, $1, $3, make_loc()); }    | factor TOK_MULTIPLIED TOK_BY primary
        { $$ = ast_create_binary_op(OP_MUL, $1, $4, make_loc()); }    | factor TOK_DIVIDED TOK_BY primary
        { $$ = ast_create_binary_op(OP_DIV, $1, $4, make_loc()); }    | factor TOK_OP_STAR primary
        { $$ = ast_create_binary_op(OP_MUL, $1, $3, make_loc()); }    | factor TOK_OP_SLASH primary
        { $$ = ast_create_binary_op(OP_DIV, $1, $3, make_loc()); }    | factor TOK_OP_PERCENT primary
        { $$ = ast_create_binary_op(OP_MOD, $1, $3, make_loc()); }    | factor TOK_POWER TOK_OF primary
        { $$ = ast_create_binary_op(OP_POW, $1, $4, make_loc()); }    | factor TOK_POWER primary
        { $$ = ast_create_binary_op(OP_POW, $1, $3, make_loc()); }    | factor TOK_OP_CARET primary
        { $$ = ast_create_binary_op(OP_POW, $1, $3, make_loc()); }    | factor TOK_SQUARED
        { 
            /* squared => power 2 */
            ASTNode *two = ast_create_literal_int(2, make_loc());
            $$ = ast_create_binary_op(OP_POW, $1, two, make_loc());
            }
    | primary
    ;

/*
 * mul_op helper rule; natural language multiplication/division/mod tokens map করে।
 */

mul_op
    : TOK_MULTIPLIED
        /* multiplied => OP_MUL */
        { $$ = OP_MUL; }
    | TOK_DIVIDED
        /* divided => OP_DIV */
        { $$ = OP_DIV; }
    | TOK_MODULO
        /* modulo => OP_MOD */
        { $$ = OP_MOD; }
    | TOK_REMAINDER
        /* remainder => OP_MOD (modulo-এর synonym) */
        { $$ = OP_MOD; }
    ;

/*
 * primary expression-এর সবচেয়ে atomic unitগুলো handle করে:
 * literals, identifiers, function call, list literal, index access,
 * unary minus, parenthesized expression ইত্যাদি।
 */

primary
    /* Example: 42 */
    : TOK_INTEGER
        /* literal integer token value = $1 */
        { $$ = ast_create_literal_int($1, make_loc()); }
    /* Example: 3.14 */
    | TOK_FLOAT
        /* literal float token value = $1 */
        { $$ = ast_create_literal_float($1, make_loc()); }
    /* Example: "hello" */
    | TOK_STRING
        {
            /* string literal copy করে AST node, তারপর lexer buffer মুক্ত */
            $$ = ast_create_literal_string($1, make_loc());
            free($1);
        }
    /* Example: true */
    | TOK_TRUE
        /* boolean literal true */
        { $$ = ast_create_literal_bool(1, make_loc()); }
    /* Example: false */
    | TOK_FALSE
        /* boolean literal false */
        { $$ = ast_create_literal_bool(0, make_loc()); }
    /* Example: yes */
    | TOK_YES
        /* yes synonym mapped to true */
        { $$ = ast_create_literal_bool(1, make_loc()); }
    /* Example: no */
    | TOK_NO
        /* no synonym mapped to false */
        { $$ = ast_create_literal_bool(0, make_loc()); }
    /* Example: total */
    | TOK_IDENTIFIER
        {
            /* identifier reference node */
            $$ = ast_create_identifier($1, make_loc());
            free($1);
        }
    /* Example: the value of total */
    | TOK_THE TOK_VALUE TOK_OF TOK_IDENTIFIER
        {
            /* verbose identifier form: the value of x => identifier(x) */
            $$ = ast_create_identifier($4, make_loc());
            free($4);
        }
    /* Example: call add with 5 and 10 */
    | function_call
    /* Example: [1, 2, 3] */
    | list_literal
    /* Example: arr[2] */
    | TOK_IDENTIFIER TOK_LBRACKET expression TOK_RBRACKET
        {
            ASTNode *arr = ast_create_identifier($1, make_loc());
            /* arr[index] access */
            $$ = ast_create_index(arr, $3, make_loc());
            free($1);
        }
    /* Example: arr at 2 */
    | TOK_IDENTIFIER TOK_AT expression
        {
            ASTNode *arr = ast_create_identifier($1, make_loc());
            /* alternate indexing form: arr at idx */
            $$ = ast_create_index(arr, $3, make_loc());
            free($1);
        }
    /* Example: item 2 of arr */
    | TOK_ITEM expression TOK_OF TOK_IDENTIFIER
        {
            ASTNode *arr = ast_create_identifier($4, make_loc());
            /* item <idx> of <arr> */
            $$ = ast_create_index(arr, $2, make_loc());
            free($4);
        }
    /* Example: get item 2 from arr */
    | TOK_GET TOK_ITEM expression TOK_FROM TOK_IDENTIFIER
        {
            ASTNode *arr = ast_create_identifier($5, make_loc());
            /* get item <idx> from <arr> */
            $$ = ast_create_index(arr, $3, make_loc());
            free($5);
        }
    /* Example: length of names */
    | TOK_LENGTH TOK_OF TOK_IDENTIFIER
        {
            /* length of x => builtin function call: length(x) */
            ASTNodeList *args = ast_node_list_create();
            ast_node_list_append(args, ast_create_identifier($3, make_loc()));
            $$ = ast_create_func_call("length", args, make_loc());
            free($3);
        }
    /* Example: size of names */
    | TOK_SIZE TOK_OF TOK_IDENTIFIER
        {
            /* size of x => same semantic as length(x) */
            ASTNodeList *args = ast_node_list_create();
            ast_node_list_append(args, ast_create_identifier($3, make_loc()));
            $$ = ast_create_func_call("length", args, make_loc());
            free($3);
        }
    /* Example: square root of 49 */
    | TOK_SQUARE TOK_ROOT TOK_OF primary
        {
            /* square root of expr => sqrt(expr) builtin call */
            ASTNodeList *args = ast_node_list_create();
            ast_node_list_append(args, $4);
            $$ = ast_create_func_call("sqrt", args, make_loc());
        }
    /* Example: root 49 */
    | TOK_ROOT primary
        {
            /* root expr => sqrt(expr) shorthand */
            ASTNodeList *args = ast_node_list_create();
            ast_node_list_append(args, $2);
            $$ = ast_create_func_call("sqrt", args, make_loc());
        }
    /* Example: (a + b) */
    | TOK_LPAREN expression TOK_RPAREN
        /* parentheses শুধু grouping; inner expression-ই result */
        { $$ = $2; }
    /* Example: -x (symbolic minus token) */
    | TOK_OP_MINUS primary
        /* unary negative */
        { $$ = ast_create_unary_op(OP_NEG, $2, make_loc()); }
    /* Example: minus x (word-form minus token) */
    | TOK_MINUS primary
        /* unary negative with word form */
        { $$ = ast_create_unary_op(OP_NEG, $2, make_loc()); }
    ;

/* call add with 5 and 10 */
/* call greet with "Hello" */
/*
 * function_call natural form (call name with args) এবং
 * conventional form (name(args)) উভয় syntax সমর্থন করে।
 */
function_call
    : TOK_CALL TOK_IDENTIFIER TOK_WITH arg_list
        {
            /* call name with arg_list */
            $$ = ast_create_func_call($2, $4, make_loc());
            free($2);
            }
    | TOK_CALL TOK_IDENTIFIER
        {
            /* no-arg call: call fname */
            $$ = ast_create_func_call($2, ast_node_list_create(), make_loc());
            free($2);
            }
    | TOK_IDENTIFIER TOK_LPAREN arg_list TOK_RPAREN
        {
            /* conventional C-like call: fname(args) */
            $$ = ast_create_func_call($1, $3, make_loc());
            free($1);
            }
    | TOK_IDENTIFIER TOK_LPAREN TOK_RPAREN
        {
            /* conventional no-arg call: fname() */
            $$ = ast_create_func_call($1, ast_node_list_create(), make_loc());
            free($1);
            }
    ;

/*
 * arg_list function argumentগুলো ASTNodeList আকারে জমায়।
 * 'and' অথবা comma — দুই separator-ই বৈধ।
 */

arg_list
    : expression
        {
            /* প্রথম argument এ নতুন list তৈরি */
            $$ = ast_node_list_create();
            ast_node_list_append($$, $1);
            }
    | arg_list TOK_AND expression
        {
            /* বিদ্যমান list ($1)-এ নতুন arg ($3) যোগ */
            ast_node_list_append($1, $3);
            $$ = $1;
            }
    | arg_list TOK_COMMA expression
        {
            /* comma-separated argument যোগ */
            ast_node_list_append($1, $3);
            $$ = $1;
            }
    ;

/* [1, 2, 3, 4, 5] */
/*
 * list_literal bracket notation থেকে list node তৈরি করে;
 * empty list এবং populated list দুটোই সমর্থিত।
 */
list_literal
    : TOK_LBRACKET expr_list TOK_RBRACKET
        /* populated list literal */
        { $$ = ast_create_list($2, make_loc()); }    | TOK_LBRACKET TOK_RBRACKET
        /* empty list literal [] */
        { $$ = ast_create_list(ast_node_list_create(), make_loc()); }
    ;

/*
 * expr_list list literal-এর ভেতরের element expression সংগ্রহ করে।
 * comma-separated expressionগুলো ক্রমানুসারে append হয়।
 */

expr_list
    : expression
        {
            /* প্রথম list element এ নতুন list তৈরি */
            $$ = ast_node_list_create();
            ast_node_list_append($$, $1);
            }
    | expr_list TOK_COMMA expression
        {
            /* list literal-এ পরের element append */
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
    /*
     * GLR parser ambiguity হলে একাধিক parse result আসতে পারে।
     * এই merge function policy হিসেবে প্রথম result (x0) বেছে নেয়।
     */
    (void)x1; /* suppress unused warning */
    return x0;
}

/* ============================================================================
 * ERROR HANDLING
 * ============================================================================
 */

void yyerror(const char *s) {
    /*
     * verbose error output:
     * - yylineno: কোন লাইনে parse সমস্যা
     * - s: bison generated error message
     * - yytext: যে token-এর কাছে parser আটকে গেছে
     */
    fprintf(stderr, "Parse error at line %d: %s (near '%s')\n", 
            yylineno, s, yytext);
}

/* ============================================================================
 * PUBLIC API
 * ============================================================================
 */

ASTNode *naturelang_parse(FILE *input) {
    /* parser শুরু করার আগে lexer input stream সেট */
    yyin = input;
    /* previous parse result clear করা জরুরি */
    parse_result = NULL;
    
    /* yyparse non-zero হলে parse failure */
    if (yyparse() != 0) {
        return NULL;
    }
    
    /* success হলে program rule থেকে তৈরি root node ফেরত */
    return parse_result;
}

ASTNode *naturelang_parse_string(const char *source) {
    /*
     * string input parse করতে lexer-এর FILE* interface reuse করা হয়েছে:
     * source string -> temporary file -> rewind -> existing FILE parser path
     */
    FILE *tmp = tmpfile();
    if (tmp == NULL) {
        fprintf(stderr, "Error: Could not create temporary file\n");
        return NULL;
    }
    
    /* source text temp file-এ লেখা */
    fputs(source, tmp);
    /* parse শুরু করার আগে file pointer শুরুতে নেওয়া */
    rewind(tmp);
    
    /* existing file-based parser reuse */
    ASTNode *result = naturelang_parse(tmp);
    
    /* temp handle cleanup */
    fclose(tmp);
    return result;
}

ASTNode *get_parse_result(void) {
    /* external caller চাইলে latest parse root state পড়তে পারে */
    return parse_result;
}
