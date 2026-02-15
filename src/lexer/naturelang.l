/*
 * NatureLang Compiler
 * Copyright (c) 2026
 * 
 * Flex Lexer Specification
 * 
 * This file defines the lexical analyzer for NatureLang.
 * It handles natural language tokens, multi-word operators,
 * string literals, numbers, comments, and error recovery.
 *
 * UNIFIED LEXER ARCHITECTURE:
 * This single lexer file is used both for:
 * 1. Standalone lexer testing (without USE_BISON_TOKENS)
 * 2. Parser integration (with USE_BISON_TOKENS defined)
 */

%{
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Ensure strdup is declared (POSIX function) */
#ifndef strdup
extern char *strdup(const char *s);
#endif

/* Token definitions:
 * - USE_BISON_TOKENS: Use Bison-generated header (for parser build)
 * - Otherwise: Use tokens.h (for standalone lexer testing)
 * 
 * Important: We only include ONE source of token definitions to avoid
 * enum redeclaration conflicts.
 */
#ifdef USE_BISON_TOKENS
#include "naturelang.tab.h"
/* When using Bison, use yylval directly */
#define SET_INT_VAL(v)    yylval.int_val = (v)
#define SET_FLOAT_VAL(v)  yylval.float_val = (v)
#define SET_STRING_VAL(v) yylval.str_val = strdup(v)
#define SET_CHAR_VAL(v)   yylval.char_val = (v)
/* Create TokenType alias since Bison uses different enum name */
typedef int TokenType;
/* Forward declare lookup_keyword (implemented in tokens.c) */
TokenType lookup_keyword(const char *str);
#else
#include "tokens.h"
/* Standalone mode - use global variables */
#define SET_INT_VAL(v)    yylval_int = (v)
#define SET_FLOAT_VAL(v)  yylval_float = (v)
#define SET_STRING_VAL(v) /* handled in lexer_next_token */
#define SET_CHAR_VAL(v)   yylval_char = (v)
#endif

#include "lexer.h"

/* Track location for error reporting */
static int current_line = 1;
static int current_column = 1;
static const char *current_filename = "<stdin>";

/* Buffer for multi-word token handling */
static char multi_word_buffer[256];
static int multi_word_pending = 0;
static TokenType pending_token_type;

/* String literal buffer */
static char string_buffer[4096];
static int string_buffer_len = 0;

/* Error buffer */
static char error_buffer[512];

/* Forward declarations */
static void update_location(void);
static void handle_newline(void);
static char *process_escape_sequences(const char *str);
static void report_lexer_error(const char *msg);

#ifndef USE_BISON_TOKENS
static SourceLocation make_location(void);
#endif

/* Macro to update column position */
#define YY_USER_ACTION update_location();

%}

/* Flex options */
%option noyywrap
%option case-insensitive
%option yylineno

/* Start conditions for different lexer states */
%x STRING_STATE
%x CHAR_STATE
%x LINE_COMMENT
%x BLOCK_COMMENT
%x MULTI_WORD

/* Regular expression definitions */
DIGIT       [0-9]
LETTER      [a-zA-Z]
ALPHANUM    [a-zA-Z0-9_]
IDENTIFIER  [a-zA-Z_][a-zA-Z0-9_]*
INTEGER     {DIGIT}+
FLOAT       {DIGIT}+"."{DIGIT}*|"."{DIGIT}+
EXPONENT    [eE][+-]?{DIGIT}+
WHITESPACE  [ \t\r]+
NEWLINE     \n

%%

 /* ============================================================================
  * COMMENTS
  * Comments are silently consumed and not returned to the parser.
  * In standalone lexer mode, we could optionally return them, but for
  * parser compatibility, we just skip them.
  * ============================================================================
  */

"--"                    { BEGIN(LINE_COMMENT); }
<LINE_COMMENT>[^\n]*    { /* consume comment content */ }
<LINE_COMMENT>\n        { handle_newline(); BEGIN(INITIAL); /* no return - skip comment */ }

"{-"                    { BEGIN(BLOCK_COMMENT); }
<BLOCK_COMMENT>"-}"     { BEGIN(INITIAL); /* no return - skip comment */ }
<BLOCK_COMMENT>\n       { handle_newline(); }
<BLOCK_COMMENT>.        { /* consume block comment content */ }

 /* ============================================================================
  * FILLER WORDS (Silently Ignored)
  * These natural language filler words are consumed without producing tokens.
  * This allows users to write more naturally, e.g.:
  *   "I want to create a number called x" -> "create a number called x"
  *   "Please display the result" -> "display the result"
  *   "Can you show me the value" -> "show the value"
  * NOTE: Multi-word patterns must come BEFORE shorter patterns.
  * ============================================================================
  */

[Ii]{WHITESPACE}+"want"{WHITESPACE}+"to" { /* ignore "I want to" */ }
[Ii]{WHITESPACE}+"want" { /* ignore "I want" */ }
"want"{WHITESPACE}+"to" { /* ignore "want to" */ }
"want"                  { /* ignore */ }
"please"                { /* ignore */ }
"can"{WHITESPACE}+"you" { /* ignore "can you" */ }
"can"                   { /* ignore */ }
"could"{WHITESPACE}+"you" { /* ignore "could you" */ }
"could"                 { /* ignore */ }
"would"{WHITESPACE}+"you" { /* ignore "would you" */ }
"would"                 { /* ignore */ }
"let"{WHITESPACE}+"me"  { /* ignore "let me" */ }
"let"{WHITESPACE}+"us"  { /* ignore "let us" */ }
"let's"                 { /* ignore */ }
"let"                   { /* ignore */ }
"me"                    { /* ignore when standalone */ }
"us"                    { /* ignore when standalone */ }
"now"                   { /* ignore */ }
"just"                  { /* ignore */ }
"simply"                { /* ignore */ }
"go"{WHITESPACE}+"ahead"{WHITESPACE}+"and" { /* ignore "go ahead and" */ }
"proceed"{WHITESPACE}+"to" { /* ignore "proceed to" */ }
<BLOCK_COMMENT><<EOF>>  { report_lexer_error("Unterminated block comment"); return TOK_ERROR; }

 /* ============================================================================
  * STRING LITERALS
  * ============================================================================
  */

\"                      { 
                            string_buffer_len = 0; 
                            string_buffer[0] = '\0';
                            BEGIN(STRING_STATE); 
                        }

<STRING_STATE>\"        { 
                            BEGIN(INITIAL);
                            string_buffer[string_buffer_len] = '\0';
                            SET_STRING_VAL(string_buffer);
                            return TOK_STRING;
                        }

<STRING_STATE>\\n       { string_buffer[string_buffer_len++] = '\n'; }
<STRING_STATE>\\t       { string_buffer[string_buffer_len++] = '\t'; }
<STRING_STATE>\\r       { string_buffer[string_buffer_len++] = '\r'; }
<STRING_STATE>\\\\      { string_buffer[string_buffer_len++] = '\\'; }
<STRING_STATE>\\\"      { string_buffer[string_buffer_len++] = '"'; }
<STRING_STATE>\\[0-7]{1,3} {
                            int val = strtol(yytext + 1, NULL, 8);
                            if (val > 255) {
                                report_lexer_error("Octal escape sequence out of range");
                            }
                            string_buffer[string_buffer_len++] = (char)val;
                        }
<STRING_STATE>\\x[0-9a-fA-F]{1,2} {
                            int val = strtol(yytext + 2, NULL, 16);
                            string_buffer[string_buffer_len++] = (char)val;
                        }
<STRING_STATE>\\.       { 
                            report_lexer_error("Invalid escape sequence");
                            string_buffer[string_buffer_len++] = yytext[1];
                        }
<STRING_STATE>\n        { 
                            report_lexer_error("Unterminated string (newline in string)");
                            handle_newline();
                            BEGIN(INITIAL);
                            return TOK_ERROR;
                        }
<STRING_STATE><<EOF>>   { 
                            report_lexer_error("Unterminated string (end of file)");
                            return TOK_ERROR;
                        }
<STRING_STATE>[^\\\"\n]+ {
                            int len = strlen(yytext);
                            if (string_buffer_len + len >= sizeof(string_buffer) - 1) {
                                report_lexer_error("String too long");
                            } else {
                                strcpy(string_buffer + string_buffer_len, yytext);
                                string_buffer_len += len;
                            }
                        }

 /* ============================================================================
  * CHARACTER LITERALS
  * ============================================================================
  */

\'                      { BEGIN(CHAR_STATE); }
<CHAR_STATE>\\n\'       { BEGIN(INITIAL); SET_CHAR_VAL('\n'); return TOK_CHAR; }
<CHAR_STATE>\\t\'       { BEGIN(INITIAL); SET_CHAR_VAL('\t'); return TOK_CHAR; }
<CHAR_STATE>\\r\'       { BEGIN(INITIAL); SET_CHAR_VAL('\r'); return TOK_CHAR; }
<CHAR_STATE>\\\\\'      { BEGIN(INITIAL); SET_CHAR_VAL('\\'); return TOK_CHAR; }
<CHAR_STATE>\\'\'       { BEGIN(INITIAL); SET_CHAR_VAL('\''); return TOK_CHAR; }
<CHAR_STATE>[^\\\'\n]\' { BEGIN(INITIAL); SET_CHAR_VAL(yytext[0]); return TOK_CHAR; }
<CHAR_STATE>.           { 
                            report_lexer_error("Invalid character literal");
                            BEGIN(INITIAL);
                            return TOK_ERROR;
                        }

 /* ============================================================================
  * MULTI-WORD OPERATORS (Natural Language Style)
  * These need special handling to combine multiple words into single tokens
  * ============================================================================
  */

"greater"{WHITESPACE}+"than"        { return TOK_GREATER_THAN; }
"less"{WHITESPACE}+"than"           { return TOK_LESS_THAN; }
"equal"{WHITESPACE}+"to"            { return TOK_EQUAL_TO; }
"not"{WHITESPACE}+"equal"{WHITESPACE}+"to" { return TOK_NOT_EQUAL_TO; }
"at"{WHITESPACE}+"least"            { return TOK_AT_LEAST; }
"at"{WHITESPACE}+"most"             { return TOK_AT_MOST; }
"multiplied"{WHITESPACE}+"by"       { return TOK_MULTIPLIED; }
"divided"{WHITESPACE}+"by"          { return TOK_DIVIDED; }
"square"{WHITESPACE}+"root"         { return TOK_ROOT; }
"give"{WHITESPACE}+"back"           { return TOK_GIVE; }
"set"{WHITESPACE}+"it"{WHITESPACE}+"to" { return TOK_SET; }
"secure"{WHITESPACE}+"zone"         { return TOK_SECURE; }
"safe"{WHITESPACE}+"zone"           { return TOK_SAFE; }

 /* Synonym multi-word patterns */
"send"{WHITESPACE}+"back"           { return TOK_GIVE; }
"is"{WHITESPACE}+"now"              { return TOK_BECOMES; }
"is"{WHITESPACE}+"set"{WHITESPACE}+"to" { return TOK_SET; }
"as"{WHITESPACE}+"long"{WHITESPACE}+"as" { return TOK_WHILE; }
"is"{WHITESPACE}+"more"{WHITESPACE}+"than" { return TOK_GREATER_THAN; }
"is"{WHITESPACE}+"bigger"{WHITESPACE}+"than" { return TOK_GREATER_THAN; }
"is"{WHITESPACE}+"larger"{WHITESPACE}+"than" { return TOK_GREATER_THAN; }
"is"{WHITESPACE}+"smaller"{WHITESPACE}+"than" { return TOK_LESS_THAN; }
"is"{WHITESPACE}+"fewer"{WHITESPACE}+"than" { return TOK_LESS_THAN; }
"is"{WHITESPACE}+"the"{WHITESPACE}+"same"{WHITESPACE}+"as" { return TOK_EQUAL_TO; }
"is"{WHITESPACE}+"identical"{WHITESPACE}+"to" { return TOK_EQUAL_TO; }
"is"{WHITESPACE}+"different"{WHITESPACE}+"from" { return TOK_NOT_EQUAL_TO; }
"does"{WHITESPACE}+"not"{WHITESPACE}+"equal" { return TOK_NOT_EQUAL_TO; }
"exceeds"                           { return TOK_GREATER_THAN; }
"between"                           { return TOK_BETWEEN; }  /* unique NatureLang operator */

 /* ============================================================================
  * KEYWORDS - Declaration and Assignment
  * ============================================================================
  */

"create"        { return TOK_CREATE; }
"declare"       { return TOK_CREATE; }  /* synonym for create */
"a"             { return TOK_A; }
"an"            { return TOK_AN; }
"called"        { return TOK_CALLED; }
"named"         { return TOK_NAMED; }
"and"           { return TOK_AND; }
"set"           { return TOK_SET; }
"it"            { return TOK_IT; }
"to"            { return TOK_TO; }
"as"            { return TOK_AS; }
"becomes"       { return TOK_BECOMES; }
"equals"        { return TOK_EQUALS; }
"make"          { return TOK_MAKE; }
"equal"         { return TOK_EQUAL; }
"assign"        { return TOK_SET; }     /* synonym for set */

 /* ============================================================================
  * KEYWORDS - Types (with synonyms)
  * ============================================================================
  */

"number"        { return TOK_TYPE_NUMBER; }
"integer"       { return TOK_TYPE_NUMBER; }  /* synonym */
"int"           { return TOK_TYPE_NUMBER; }  /* synonym */
"whole"{WHITESPACE}+"number" { return TOK_TYPE_NUMBER; }  /* synonym */
"text"          { return TOK_TYPE_TEXT; }
"string"        { return TOK_TYPE_TEXT; }    /* synonym */
"word"          { return TOK_TYPE_TEXT; }    /* synonym */
"sentence"      { return TOK_TYPE_TEXT; }    /* synonym */
"decimal"       { return TOK_TYPE_DECIMAL; }
"float"         { return TOK_TYPE_DECIMAL; } /* synonym */
"real"          { return TOK_TYPE_DECIMAL; } /* synonym */
"flag"          { return TOK_TYPE_FLAG; }
"boolean"       { return TOK_TYPE_FLAG; }    /* synonym */
"bool"          { return TOK_TYPE_FLAG; }    /* synonym */
"list"          { return TOK_TYPE_LIST; }
"array"         { return TOK_TYPE_LIST; }    /* synonym */
"collection"    { return TOK_TYPE_LIST; }    /* synonym */
"nothing"       { return TOK_TYPE_NOTHING; }
"void"          { return TOK_TYPE_NOTHING; } /* synonym */

 /* ============================================================================
  * KEYWORDS - Control Flow (with synonyms)
  * ============================================================================
  */

"if"            { return TOK_IF; }
"when"          { return TOK_IF; }       /* synonym for if */
"whenever"      { return TOK_IF; }       /* synonym for if */
"then"          { return TOK_THEN; }
"otherwise"     { return TOK_OTHERWISE; }
"else"          { return TOK_ELSE; }
"end"           { return TOK_END; }
"finish"        { return TOK_END; }      /* synonym for end */
"done"          { return TOK_END; }      /* synonym for end */
"repeat"        { return TOK_REPEAT; }
"loop"          { return TOK_REPEAT; }   /* synonym for repeat */
"times"         { return TOK_TIMES; }
"while"         { return TOK_WHILE; }
"do"            { return TOK_DO; }
"for"           { return TOK_FOR; }
"each"          { return TOK_EACH; }
"every"         { return TOK_EACH; }     /* synonym for each */
"in"            { return TOK_IN; }
"inside"        { return TOK_IN; }       /* synonym for in */
"within"        { return TOK_IN; }       /* synonym for in */
"from"          { return TOK_FROM; }
"until"         { return TOK_UNTIL; }
"stop"          { return TOK_STOP; }
"break"         { return TOK_STOP; }     /* synonym for stop */
"halt"          { return TOK_STOP; }     /* synonym for stop */
"skip"          { return TOK_SKIP; }
"continue"      { return TOK_SKIP; }     /* synonym for skip */
"next"          { return TOK_SKIP; }     /* synonym for skip */

 /* ============================================================================
  * KEYWORDS - Functions (with synonyms)
  * ============================================================================
  */

"define"        { return TOK_DEFINE; }
"function"      { return TOK_FUNCTION; }
"method"        { return TOK_FUNCTION; } /* synonym for function */
"procedure"     { return TOK_FUNCTION; } /* synonym for function */
"routine"       { return TOK_FUNCTION; } /* synonym for function */
"that"          { return TOK_THAT; }
"which"         { return TOK_THAT; }     /* synonym for that */
"takes"         { return TOK_TAKES; }
"accepts"       { return TOK_TAKES; }    /* synonym for takes */
"receives"      { return TOK_TAKES; }    /* synonym for takes */
"requires"      { return TOK_TAKES; }    /* synonym for takes */
"returns"       { return TOK_RETURNS; }
"gives"         { return TOK_RETURNS; }  /* synonym for returns */
"outputs"       { return TOK_RETURNS; }  /* synonym for returns */
"give"          { return TOK_GIVE; }
"return"        { return TOK_GIVE; }     /* synonym for give */
"back"          { return TOK_BACK; }
"call"          { return TOK_CALL; }
"invoke"        { return TOK_CALL; }     /* synonym for call */
"execute"       { return TOK_CALL; }     /* synonym for call */
"run"           { return TOK_CALL; }     /* synonym for call */
"with"          { return TOK_WITH; }
"using"         { return TOK_WITH; }     /* synonym for with */

 /* ============================================================================
  * KEYWORDS - I/O (with synonyms)
  * ============================================================================
  */

"display"       { return TOK_DISPLAY; }
"show"          { return TOK_SHOW; }
"print"         { return TOK_PRINT; }
"output"        { return TOK_DISPLAY; }  /* synonym for display */
"write"         { return TOK_DISPLAY; }  /* synonym for display */
"say"           { return TOK_DISPLAY; }  /* synonym for display */
"tell"          { return TOK_DISPLAY; }  /* synonym for display */
"ask"           { return TOK_ASK; }
"prompt"        { return TOK_ASK; }      /* synonym for ask */
"request"       { return TOK_ASK; }      /* synonym for ask */
"read"          { return TOK_READ; }
"input"         { return TOK_READ; }     /* synonym for read */
"get"           { return TOK_GET; }
"receive"       { return TOK_READ; }     /* synonym for read */
"remember"      { return TOK_REMEMBER; }
"save"          { return TOK_SAVE; }
"store"         { return TOK_STORE; }
"into"          { return TOK_INTO; }

 /* ============================================================================
  * KEYWORDS - Security
  * ============================================================================
  */

"enter"         { return TOK_ENTER; }
"secure"        { return TOK_SECURE; }
"zone"          { return TOK_ZONE; }
"safe"          { return TOK_SAFE; }
"begin"         { return TOK_BEGIN; }
"safely"        { return TOK_SAFELY; }
"risky"         { return TOK_RISKY; }

 /* ============================================================================
  * KEYWORDS - Additional
  * ============================================================================
  */

"the"           { return TOK_THE; }
"value"         { return TOK_VALUE; }
"constant"      { return TOK_CONSTANT; }
 /* "return" handled in function synonyms as synonym for give */
 /* "store" handled in secure zone synonyms */
"user"          { return TOK_USER; }
"change"        { return TOK_CHANGE; }

 /* ============================================================================
  * KEYWORDS - List Operations
  * ============================================================================
  */

"add"           { return TOK_ADD; }
"remove"        { return TOK_REMOVE; }
 /* "get" handled earlier */
"item"          { return TOK_ITEM; }
"at"            { return TOK_AT; }
"position"      { return TOK_POSITION; }
"length"        { return TOK_LENGTH; }
"size"          { return TOK_SIZE; }
"append"        { return TOK_APPEND; }
"first"         { return TOK_FIRST; }
"last"          { return TOK_LAST; }

 /* ============================================================================
  * KEYWORDS - Logical (with synonyms)
  * ============================================================================
  */

"is"            { return TOK_IS; }
"not"           { return TOK_NOT; }
"or"            { return TOK_OR; }
"true"          { return TOK_TRUE; }
"correct"       { return TOK_TRUE; }     /* synonym for true */
"right"         { return TOK_TRUE; }     /* synonym for true */
"false"         { return TOK_FALSE; }
"incorrect"     { return TOK_FALSE; }    /* synonym for false */
"wrong"         { return TOK_FALSE; }    /* synonym for false */
"yes"           { return TOK_YES; }
"no"            { return TOK_NO; }

 /* ============================================================================
  * KEYWORDS - Comparison (single words, multi-word handled above)
  * ============================================================================
  */

"greater"       { return TOK_GREATER; }
"larger"        { return TOK_GREATER; }  /* synonym for greater */
"bigger"        { return TOK_GREATER; }  /* synonym for greater */
"more"          { return TOK_GREATER; }  /* synonym for greater */
"less"          { return TOK_LESS; }
"smaller"       { return TOK_LESS; }     /* synonym for less */
"fewer"         { return TOK_LESS; }     /* synonym for less */
"than"          { return TOK_THAN; }

 /* ============================================================================
  * KEYWORDS - Arithmetic (Natural Words with synonyms)
  * ============================================================================
  */

"plus"          { return TOK_PLUS; }
"added"         { return TOK_PLUS; }     /* synonym for plus */
"minus"         { return TOK_MINUS; }
"subtract"      { return TOK_MINUS; }    /* synonym for minus */
"subtracted"    { return TOK_MINUS; }    /* synonym for minus */
"multiplied"    { return TOK_MULTIPLIED; }
 /* "times" handled earlier in repeat loop */
"divided"       { return TOK_DIVIDED; }
"by"            { return TOK_BY; }
"modulo"        { return TOK_MODULO; }
"mod"           { return TOK_MODULO; }   /* synonym for modulo */
"remainder"     { return TOK_REMAINDER; }
"of"            { return TOK_OF; }
"power"         { return TOK_POWER; }
"raised"        { return TOK_POWER; }    /* synonym for power */
"squared"       { return TOK_SQUARED; }
"square"        { return TOK_SQUARE; }
"root"          { return TOK_ROOT; }

 /* ============================================================================
  * SYMBOLIC OPERATORS
  * ============================================================================
  */

"+"             { return TOK_OP_PLUS; }
"-"             { return TOK_OP_MINUS; }
"*"             { return TOK_OP_STAR; }
"/"             { return TOK_OP_SLASH; }
"%"             { return TOK_OP_PERCENT; }
"^"             { return TOK_OP_CARET; }
"=="            { return TOK_OP_EQEQ; }
"!="            { return TOK_OP_NEQ; }
"<>"            { return TOK_OP_NEQ; }     /* Alternative not-equal */
"<="            { return TOK_OP_LTE; }
">="            { return TOK_OP_GTE; }
"<"             { return TOK_OP_LT; }
">"             { return TOK_OP_GT; }
"="             { return TOK_OP_EQ; }
"&&"            { return TOK_OP_AND; }
"||"            { return TOK_OP_OR; }
"!"             { return TOK_OP_NOT; }
"->"            { return TOK_OP_ARROW; }
":"             { return TOK_OP_COLON; }

 /* ============================================================================
  * PUNCTUATION
  * ============================================================================
  */

"("             { return TOK_LPAREN; }
")"             { return TOK_RPAREN; }
"["             { return TOK_LBRACKET; }
"]"             { return TOK_RBRACKET; }
"{"             { return TOK_LBRACE; }
"}"             { return TOK_RBRACE; }
","             { return TOK_COMMA; }
"."             { return TOK_DOT; }
";"             { return TOK_SEMICOLON; }

 /* ============================================================================
  * NUMERIC LITERALS
  * ============================================================================
  */

{INTEGER}{EXPONENT}     { 
                            SET_FLOAT_VAL(atof(yytext));
                            return TOK_FLOAT;
                        }

{FLOAT}{EXPONENT}?      { 
                            SET_FLOAT_VAL(atof(yytext));
                            return TOK_FLOAT;
                        }

{INTEGER}               { 
                            SET_INT_VAL(atoll(yytext));
                            return TOK_INTEGER;
                        }

 /* ============================================================================
  * IDENTIFIERS
  * ============================================================================
  */

{IDENTIFIER}            {
                            /* Check if it's a keyword first (case-insensitive) */
                            TokenType kw = lookup_keyword(yytext);
                            if (kw != TOK_IDENTIFIER) {
                                return kw;
                            }
                            /* Set identifier value for parser */
                            SET_STRING_VAL(yytext);
                            return TOK_IDENTIFIER;
                        }

 /* ============================================================================
  * WHITESPACE AND NEWLINES
  * ============================================================================
  */

{NEWLINE}               { handle_newline(); /* optionally return TOK_NEWLINE; */ }
{WHITESPACE}            { /* ignore whitespace */ }

 /* ============================================================================
  * ERROR HANDLING - Unknown Characters
  * ============================================================================
  */

.                       {
                            snprintf(error_buffer, sizeof(error_buffer),
                                     "Unexpected character '%c' (0x%02X)", 
                                     yytext[0], (unsigned char)yytext[0]);
                            report_lexer_error(error_buffer);
                            return TOK_ERROR;
                        }

%%

/* ============================================================================
 * LEXER SUPPORT FUNCTIONS
 * ============================================================================
 */

/* Update location tracking after each token */
static void update_location(void) {
#ifndef USE_BISON_TOKENS
    yylloc.first_line = current_line;
    yylloc.first_column = current_column;
#endif
    
    for (int i = 0; yytext[i] != '\0'; i++) {
        if (yytext[i] == '\n') {
            current_line++;
            current_column = 1;
        } else if (yytext[i] == '\t') {
            current_column += 4 - (current_column - 1) % 4;
        } else {
            current_column++;
        }
    }
    
#ifndef USE_BISON_TOKENS
    yylloc.last_line = current_line;
    yylloc.last_column = current_column;
#endif
}

/* Handle newline - update line counter */
static void handle_newline(void) {
    current_line++;
    current_column = 1;
}

#ifndef USE_BISON_TOKENS
/* Create a SourceLocation from current position (standalone mode only) */
static SourceLocation make_location(void) {
    SourceLocation loc;
    loc.filename = current_filename;
    loc.first_line = yylloc.first_line;
    loc.first_column = yylloc.first_column;
    loc.last_line = yylloc.last_line;
    loc.last_column = yylloc.last_column;
    return loc;
}
#endif

/* Report a lexer error */
static void report_lexer_error(const char *msg) {
    fprintf(stderr, "\033[1;31mLexer Error\033[0m at %s:%d:%d: %s\n",
            current_filename, current_line, current_column, msg);
#ifndef USE_BISON_TOKENS
    lexer_error_count++;
#endif
}

/* ============================================================================
 * PUBLIC LEXER API
 * ============================================================================
 */

/* 
 * The following global variables and functions are only used in standalone
 * lexer mode. When building with Bison (USE_BISON_TOKENS), the parser uses
 * yylval directly through the %union mechanism.
 */
#ifndef USE_BISON_TOKENS

/* Global variables for token values (standalone mode) */
long long yylval_int = 0;
double yylval_float = 0.0;
char *yylval_string = NULL;
char yylval_char = '\0';

/* Location tracking */
SourceLocation yylloc = {NULL, 1, 1, 1, 1};

/* Error tracking */
int lexer_error_count = 0;

/* Initialize lexer with a file */
int lexer_init_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
        return -1;
    }
    
    yyin = file;
    current_filename = filename;
    current_line = 1;
    current_column = 1;
    lexer_error_count = 0;
    
    return 0;
}

/* Initialize lexer with a string */
int lexer_init_string(const char *input) {
    yy_scan_string(input);
    current_filename = "<string>";
    current_line = 1;
    current_column = 1;
    lexer_error_count = 0;
    
    return 0;
}

/* Clean up lexer resources */
void lexer_cleanup(void) {
    if (yyin != NULL && yyin != stdin) {
        fclose(yyin);
        yyin = NULL;
    }
    yylex_destroy();
}

/* Get next token as Token structure */
Token *lexer_next_token(void) {
    int tok = yylex();
    
    if (tok == 0) {
        return token_create(TOK_EOF, "", make_location());
    }
    
    SourceLocation loc = make_location();
    
    switch (tok) {
        case TOK_INTEGER:
            return token_create_int(yylval_int, yytext, loc);
        
        case TOK_FLOAT:
            return token_create_float(yylval_float, yytext, loc);
        
        case TOK_STRING:
            {
                /* Use string_buffer for string content */
                string_buffer[string_buffer_len] = '\0';
                Token *t = token_create_string(string_buffer, yytext, loc);
                return t;
            }
        
        case TOK_IDENTIFIER:
            {
                /* Use yytext directly - we don't use yylval_string anymore */
                Token *t = token_create_identifier(yytext, loc);
                return t;
            }
        
        case TOK_CHAR:
            {
                Token *t = token_create(TOK_CHAR, yytext, loc);
                t->value.char_value = yylval_char;
                return t;
            }
        
        case TOK_ERROR:
            return token_create_error(yytext, loc);
        
        default:
            return token_create((TokenType)tok, yytext, loc);
    }
}

/* Get current line number */
int lexer_get_line(void) {
    return current_line;
}

/* Get current column number */
int lexer_get_column(void) {
    return current_column;
}

/* Get current filename */
const char *lexer_get_filename(void) {
    return current_filename;
}

/* Get total error count */
int lexer_get_error_count(void) {
    return lexer_error_count;
}
#endif /* USE_BISON_TOKENS */