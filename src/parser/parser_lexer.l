/*
 * NatureLang Compiler
 * Copyright (c) 2026
 * 
 * Flex Lexer Specification for Parser
 * 
 * This file defines the lexical analyzer for NatureLang parser.
 * It is a comprehensive lexer that supports natural language style programming.
 */

%{
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "ast.h"
#include "naturelang.tab.h"

/* Explicit declaration for strdup */
extern char *strdup(const char *s);

/* Track location for error reporting */
static int current_line = 1;
static int current_column = 1;

/* String literal buffer */
static char string_buffer[4096];
static int string_buffer_len = 0;

/* Forward declarations */
static void update_location(void);
static void handle_newline(void);
static void report_lexer_error(const char *msg);

/* Macro to update column position */
#define YY_USER_ACTION update_location();

%}

/* Flex options */
%option noyywrap
%option case-insensitive
%option yylineno

/* Start conditions for different lexer states */
%x STRING_STATE
%x LINE_COMMENT
%x BLOCK_COMMENT

/* Regular expression definitions */
/*
 * Backslash guide for these macros:
 *
 * 1) \t in [ \t\r]+
 *    - Tab character.
 *    - Real input sample: "create<TAB>a<TAB>number".
 *    - The TABs are matched by {WHITESPACE}.
 *
 * 2) \r in [ \t\r]+
 *    - Carriage return character (common in CRLF files).
 *    - Real line ending bytes in Windows files: "\r\n".
 *    - Here, \r can be consumed by {WHITESPACE}.
 *
 * 3) \n in NEWLINE
 *    - Newline character.
 *    - Real input sample:
 *      display "A"
 *      display "B"
 *      The separator between lines is one \n.
 *
 * 4) [eE][+-]? in EXPONENT
 *    - No backslash needed there; + and - are literal sign chars in [].
 *    - Example matches: e10, E-2, e+7.
 */
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
  * ============================================================================
  */

"--"                    { BEGIN(LINE_COMMENT); }
 /*
 * [^\n]*:
 * - ^ at start of [] means negation.
 * - \n means newline.
 * - So [^\n]* = consume everything until newline.
 * - Example: in "-- abc def\n", this matches " abc def".
 */
<LINE_COMMENT>[^\n]*    { /* consume comment content */ }
 /*
 * <LINE_COMMENT>\n:
 * - Matches the actual end-of-line for line comments.
 * - Example: "-- comment\n" triggers this rule at final \n.
 */
<LINE_COMMENT>\n        { handle_newline(); BEGIN(INITIAL); }

"{-"                    { BEGIN(BLOCK_COMMENT); }
<BLOCK_COMMENT>"-}"     { BEGIN(INITIAL); }
 /*
 * <BLOCK_COMMENT>\n:
 * - Tracks real newlines inside block comments for accurate line numbers.
 * - Example: "{- a\nb -}" increments current_line at \n.
 */
<BLOCK_COMMENT>\n       { handle_newline(); }
<BLOCK_COMMENT>.        { /* consume block comment content */ }
<BLOCK_COMMENT><<EOF>>  { report_lexer_error("Unterminated block comment"); return 0; }

 /* ============================================================================
  * STRING LITERALS
  * ============================================================================
  */

 /*
 * String regex escape cheat-sheet (Flex regex level):
 * - \" matches a literal double-quote (").
 * - \\ matches a literal backslash (\).
 *
 * Real examples:
 * - Source: "hello\\nworld" -> lexer sees '\\'+'n' and stores one newline.
 * - Source with real line break before closing quote:
 *     "hello
 *      world"
 *   -> triggers unterminated string rule.
 */

\"                      { 
                            string_buffer_len = 0; 
                            string_buffer[0] = '\0';
                            BEGIN(STRING_STATE); 
                        }

<STRING_STATE>\"        { 
                            BEGIN(INITIAL);
                            string_buffer[string_buffer_len] = '\0';
                            yylval.str_val = strdup(string_buffer);
                            return TOK_STRING;
                        }

 /*
 * Escape-handling rules inside STRING_STATE:
 *
 * 1) Core idea:
 *    - \\ in the regex means one literal backslash in source input.
 *
 * 2) <STRING_STATE>\\n
 *    - Real source: "line1\\nline2"
 *    - yytext at this step: "\\n"
 *    - stored char: '\n'
 *
 * 3) <STRING_STATE>\\t
 *    - Real source: "col1\\tcol2" -> stores '\t'
 *
 * 4) <STRING_STATE>\\r
 *    - Real source: "A\\rB" -> stores '\r'
 *
 * 5) <STRING_STATE>\\\\
 *    - Real source: "C:\\\\temp" -> "\\\\" stores one '\\'
 *
 * 6) <STRING_STATE>\\\"
 *    - Real source: "He said: \\\"ok\\\"" -> each "\\\"" stores '"'
 */
<STRING_STATE>\\n       { string_buffer[string_buffer_len++] = '\n'; }
<STRING_STATE>\\t       { string_buffer[string_buffer_len++] = '\t'; }
<STRING_STATE>\\r       { string_buffer[string_buffer_len++] = '\r'; }
<STRING_STATE>\\\\      { string_buffer[string_buffer_len++] = '\\'; }
<STRING_STATE>\\\"      { string_buffer[string_buffer_len++] = '"'; }
 /*
 * <STRING_STATE>\\.:
 * - \\ matches literal backslash.
 * - . matches one following char.
 * - Real source: "cost\\q" -> yytext is "\\q", stores 'q'.
 */
<STRING_STATE>\\.       { string_buffer[string_buffer_len++] = yytext[1]; }
 /*
 * <STRING_STATE>\n catches real newline in an unterminated string.
 * This is NOT the same as "\\n" escape sequence above.
 * Real failing source:
 *   "hello
 *    world"
 * Newline after hello triggers this rule.
 */
<STRING_STATE>\n        { 
                            report_lexer_error("Unterminated string");
                            handle_newline();
                            BEGIN(INITIAL);
                            return 0;
                        }
<STRING_STATE><<EOF>>   { 
                            report_lexer_error("Unterminated string");
                            return 0;
                        }
 /*
 * <STRING_STATE>[^\\\"\n]+:
 * - Negated class: match runs of chars that are NOT
 *   backslash, quote, or newline.
 * - \\ excludes backslash.
 * - \" excludes quote.
 * - \n excludes newline.
 * - Real source: "name=alex\\n"
 *   This rule first matches "name=alex", then stops at backslash.
 */
<STRING_STATE>[^\\\"\n]+ {
                            int len = strlen(yytext);
                            if (string_buffer_len + len < sizeof(string_buffer) - 1) {
                                strcpy(string_buffer + string_buffer_len, yytext);
                                string_buffer_len += len;
                            }
                        }

 /* ============================================================================
  * MULTI-WORD OPERATORS (Natural Language Style)
  * These MUST come before single-word keywords to match first
  * ============================================================================
  */

"greater"{WHITESPACE}+"than"                    { return TOK_GT; }
"less"{WHITESPACE}+"than"                       { return TOK_LT; }
"equal"{WHITESPACE}+"to"                        { return TOK_EQ; }
"not"{WHITESPACE}+"equal"{WHITESPACE}+"to"      { return TOK_NEQ; }
"at"{WHITESPACE}+"least"                        { return TOK_GTE; }
"at"{WHITESPACE}+"most"                         { return TOK_LTE; }
"greater"{WHITESPACE}+"than"{WHITESPACE}+"or"{WHITESPACE}+"equal"{WHITESPACE}+"to" { return TOK_GTE; }
"less"{WHITESPACE}+"than"{WHITESPACE}+"or"{WHITESPACE}+"equal"{WHITESPACE}+"to"    { return TOK_LTE; }
"multiplied"{WHITESPACE}+"by"                   { return TOK_MUL_BY; }
"divided"{WHITESPACE}+"by"                      { return TOK_DIV_BY; }
"added"{WHITESPACE}+"to"                        { return TOK_ADD_TO; }
"subtracted"{WHITESPACE}+"from"                 { return TOK_SUB_FROM; }
"raised"{WHITESPACE}+"to"{WHITESPACE}+"the"{WHITESPACE}+"power"{WHITESPACE}+"of" { return TOK_POW_OF; }
"square"{WHITESPACE}+"root"{WHITESPACE}+"of"    { return TOK_SQRT; }
"give"{WHITESPACE}+"back"                       { return TOK_RETURN; }
"set"{WHITESPACE}+"it"{WHITESPACE}+"to"         { return TOK_SET_TO; }
"secure"{WHITESPACE}+"zone"                     { return TOK_SECURE_ZONE; }
"safe"{WHITESPACE}+"zone"                       { return TOK_SECURE_ZONE; }
"end"{WHITESPACE}+"if"                          { return TOK_END_IF; }
"end"{WHITESPACE}+"while"                       { return TOK_END_WHILE; }
"end"{WHITESPACE}+"repeat"                      { return TOK_END_REPEAT; }
"end"{WHITESPACE}+"for"                         { return TOK_END_FOR; }
"end"{WHITESPACE}+"function"                    { return TOK_END_FUNCTION; }
"end"{WHITESPACE}+"secure"                      { return TOK_END_SECURE; }
"end"{WHITESPACE}+"zone"                        { return TOK_END_ZONE; }

 /* ============================================================================
  * KEYWORDS - Declaration and Assignment
  * ============================================================================
  */

"create"        { return TOK_CREATE; }
"a"             { return TOK_A; }
"an"            { return TOK_AN; }
"the"           { return TOK_THE; }
"called"        { return TOK_CALLED; }
"named"         { return TOK_CALLED; }  /* synonym */
"and"           { return TOK_AND; }
"set"           { return TOK_SET; }
"it"            { return TOK_IT; }
"to"            { return TOK_TO; }
"as"            { return TOK_TO; }      /* synonym */
"becomes"       { return TOK_BECOMES; }
"make"          { return TOK_MAKE; }
"constant"      { return TOK_CONSTANT; }
"with"          { return TOK_WITH; }
"change"        { return TOK_CHANGE; }
"value"         { return TOK_VALUE; }
"of"            { return TOK_OF; }

 /* ============================================================================
  * TYPE KEYWORDS
  * ============================================================================
  */

"number"        { return TOK_NUMBER; }
"decimal"       { return TOK_DECIMAL; }
"text"          { return TOK_TEXT; }
"flag"          { return TOK_FLAG; }
"list"          { return TOK_LIST; }
"nothing"       { return TOK_NOTHING; }

 /* ============================================================================
  * BOOLEAN LITERALS
  * ============================================================================
  */

"true"          { yylval.bool_val = 1; return TOK_TRUE; }
"yes"           { yylval.bool_val = 1; return TOK_TRUE; }  /* synonym */
"false"         { yylval.bool_val = 0; return TOK_FALSE; }
"no"            { yylval.bool_val = 0; return TOK_FALSE; } /* synonym */

 /* ============================================================================
  * CONTROL FLOW
  * ============================================================================
  */

"if"            { return TOK_IF; }
"then"          { return TOK_THEN; }
"otherwise"     { return TOK_OTHERWISE; }
"else"          { return TOK_ELSE; }
"end"           { return TOK_END; }
"while"         { return TOK_WHILE; }
"repeat"        { return TOK_REPEAT; }
"times"         { return TOK_TIMES; }
"for"           { return TOK_FOR; }
"each"          { return TOK_EACH; }
"in"            { return TOK_IN; }
"from"          { return TOK_FROM; }
"until"         { return TOK_UNTIL; }
"do"            { return TOK_DO; }
"stop"          { return TOK_STOP; }
"break"         { return TOK_STOP; }    /* synonym */
"continue"      { return TOK_CONTINUE; }
"skip"          { return TOK_CONTINUE; } /* synonym */
"looping"       { return TOK_LOOPING; }

 /* ============================================================================
  * FUNCTIONS
  * ============================================================================
  */

"define"        { return TOK_DEFINE; }
"function"      { return TOK_FUNCTION; }
"that"          { return TOK_THAT; }
"takes"         { return TOK_TAKES; }
"returns"       { return TOK_RETURNS; }
"return"        { return TOK_RETURN; }
"returning"     { return TOK_RETURNS; } /* synonym */
"call"          { return TOK_CALL; }

 /* ============================================================================
  * I/O
  * ============================================================================
  */

"display"       { return TOK_DISPLAY; }
"print"         { return TOK_DISPLAY; } /* synonym */
"show"          { return TOK_DISPLAY; } /* synonym */
"say"           { return TOK_DISPLAY; } /* synonym */
"ask"           { return TOK_ASK; }
"read"          { return TOK_READ; }
"store"         { return TOK_STORE; }
"into"          { return TOK_INTO; }
"user"          { return TOK_USER; }
"input"         { return TOK_USER; }    /* synonym */

 /* ============================================================================
  * SECURE ZONE
  * ============================================================================
  */

"begin"         { return TOK_BEGIN; }
"enter"         { return TOK_BEGIN; }   /* synonym */
"secure"        { return TOK_SECURE; }
"zone"          { return TOK_ZONE; }
"safely"        { return TOK_SAFELY; }
"risky"         { return TOK_RISKY; }

 /* ============================================================================
  * COMPARISON AND LOGIC KEYWORDS
  * ============================================================================
  */

"is"            { return TOK_IS; }
"are"           { return TOK_ARE; }
"equals"        { return TOK_EQUALS; }
"equal"         { return TOK_EQUAL; }
"not"           { return TOK_NOT; }
"greater"       { return TOK_GREATER; }
"less"          { return TOK_LESS; }
"than"          { return TOK_THAN; }
"or"            { return TOK_OR; }
"both"          { return TOK_BOTH; }
"either"        { return TOK_EITHER; }

 /* ============================================================================
  * ARITHMETIC KEYWORDS (Natural Language)
  * ============================================================================
  */

"plus"          { return TOK_PLUS; }
"minus"         { return TOK_MINUS; }
"multiplied"    { return TOK_MULTIPLIED; }
"divided"       { return TOK_DIVIDED; }
"by"            { return TOK_BY; }
"modulo"        { return TOK_MODULO; }
"remainder"     { return TOK_MODULO; }  /* synonym */
"power"         { return TOK_POWER; }
"squared"       { return TOK_SQUARED; }

 /* ============================================================================
  * LIST OPERATIONS
  * ============================================================================
  */

"add"           { return TOK_ADD; }
"remove"        { return TOK_REMOVE; }
"get"           { return TOK_GET; }
"item"          { return TOK_ITEM; }
"at"            { return TOK_AT; }
"position"      { return TOK_POSITION; }
"length"        { return TOK_LENGTH; }
"size"          { return TOK_SIZE; }
"append"        { return TOK_APPEND; }
"first"         { return TOK_FIRST; }
"last"          { return TOK_LAST; }

 /* ============================================================================
  * SYMBOLIC OPERATORS
  * ============================================================================
  */

"+"             { return TOK_PLUS; }
"-"             { return TOK_MINUS; }
"*"             { return TOK_MULTIPLY; }
"/"             { return TOK_DIVIDE; }
"%"             { return TOK_MODULO; }
"^"             { return TOK_POWER; }
"=="            { return TOK_EQ; }
"!="            { return TOK_NEQ; }
"<>"            { return TOK_NEQ; }     /* alternative not-equal */
">="            { return TOK_GTE; }
"<="            { return TOK_LTE; }
">"             { return TOK_GT; }
"<"             { return TOK_LT; }
"="             { return TOK_ASSIGN; }
"&&"            { return TOK_AND_SYM; }
"||"            { return TOK_OR_SYM; }
"!"             { return TOK_NOT_SYM; }
"->"            { return TOK_ARROW; }

 /* ============================================================================
  * PUNCTUATION
  * ============================================================================
  */

","             { return TOK_COMMA; }
"."             { return TOK_PERIOD; }
":"             { return TOK_COLON; }
";"             { return TOK_SEMICOLON; }
"("             { return TOK_LPAREN; }
")"             { return TOK_RPAREN; }
"["             { return TOK_LBRACKET; }
"]"             { return TOK_RBRACKET; }
"{"             { return TOK_LBRACE; }
"}"             { return TOK_RBRACE; }

 /* ============================================================================
  * NUMBERS
  * ============================================================================
  */

{INTEGER}{EXPONENT}     { 
                            yylval.float_val = strtod(yytext, NULL);
                            return TOK_FLOAT; 
                        }

{FLOAT}{EXPONENT}?      { 
                            yylval.float_val = strtod(yytext, NULL);
                            return TOK_FLOAT; 
                        }

{INTEGER}               { 
                            yylval.int_val = strtoll(yytext, NULL, 10);
                            return TOK_INTEGER; 
                        }

 /* ============================================================================
  * IDENTIFIERS
  * ============================================================================
  */

{IDENTIFIER}            { 
                            yylval.str_val = strdup(yytext);
                            return TOK_IDENTIFIER; 
                        }

 /* ============================================================================
  * WHITESPACE AND NEWLINES
  * ============================================================================
  */

 /*
 * WHITESPACE macro is [ \t\r]+:
 * - space, tab (\t), and carriage return (\r), one or more.
 *
 * NEWLINE macro is \n (line-feed character).
 *
 * Real input sample: "create   a\tnumber\r\n"
 * - "   \t" -> {WHITESPACE}
 * - "\r"    -> {WHITESPACE}
 * - "\n"    -> {NEWLINE}
 */

{WHITESPACE}            { /* Skip whitespace */ }
{NEWLINE}               { handle_newline(); /* Skip newlines */ }

 /* ============================================================================
  * END OF FILE
  * ============================================================================
  */

<<EOF>>                 { return 0; }

 /* ============================================================================
  * ERROR - UNKNOWN CHARACTERS
  * ============================================================================
  */

.                       { 
                            char msg[64];
                            snprintf(msg, sizeof(msg), "Unknown character: '%s' (0x%02X)", 
                                     yytext, (unsigned char)yytext[0]);
                            report_lexer_error(msg);
                        }

%%

/* ============================================================================
 * HELPER FUNCTIONS
 * ============================================================================
 */

static void update_location(void) {
  /*
   * C escape meanings in this helper (not Flex regex):
   * - '\0' : NUL terminator of C string
   * - '\n' : newline character
   */
    for (int i = 0; yytext[i] != '\0'; i++) {
        if (yytext[i] == '\n') {
            current_line++;
            current_column = 1;
        } else {
            current_column++;
        }
    }
}

static void handle_newline(void) {
    current_line++;
    current_column = 1;
}

static void report_lexer_error(const char *msg) {
    fprintf(stderr, "Lexer error at line %d, column %d: %s\n",
            current_line, current_column, msg);
}

/* ============================================================================
 * PUBLIC FUNCTIONS
 * ============================================================================
 */

void lexer_set_input_file(FILE *file) {
    yyin = file;
    current_line = 1;
    current_column = 1;
}

int lexer_get_line(void) {
    return current_line;
}

int lexer_get_column(void) {
    return current_column;
}
