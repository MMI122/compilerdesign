/*
 * NatureLang Compiler
 * Copyright (c) 2026
 * 
 * Flex Lexer Specification
 * 
 * This file defines the lexical analyzer for NatureLang.
 * It handles natural language tokens, multi-word operators,
 * string literals, numbers, comments, and error recovery.
 */

%{
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "tokens.h"
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
static SourceLocation make_location(void);
static void handle_newline(void);
static char *process_escape_sequences(const char *str);
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
  * ============================================================================
  */

"--"                    { BEGIN(LINE_COMMENT); }
<LINE_COMMENT>[^\n]*    { /* consume comment content */ }
<LINE_COMMENT>\n        { handle_newline(); BEGIN(INITIAL); return TOK_COMMENT; }

"{-"                    { BEGIN(BLOCK_COMMENT); }
<BLOCK_COMMENT>"-}"     { BEGIN(INITIAL); return TOK_BLOCK_COMMENT; }
<BLOCK_COMMENT>\n       { handle_newline(); }
<BLOCK_COMMENT>.        { /* consume block comment content */ }
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
<CHAR_STATE>\\n\'       { BEGIN(INITIAL); yylval_char = '\n'; return TOK_CHAR; }
<CHAR_STATE>\\t\'       { BEGIN(INITIAL); yylval_char = '\t'; return TOK_CHAR; }
<CHAR_STATE>\\r\'       { BEGIN(INITIAL); yylval_char = '\r'; return TOK_CHAR; }
<CHAR_STATE>\\\\\'      { BEGIN(INITIAL); yylval_char = '\\'; return TOK_CHAR; }
<CHAR_STATE>\\'\'       { BEGIN(INITIAL); yylval_char = '\''; return TOK_CHAR; }
<CHAR_STATE>[^\\\'\n]\' { BEGIN(INITIAL); yylval_char = yytext[0]; return TOK_CHAR; }
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

 /* ============================================================================
  * KEYWORDS - Declaration and Assignment
  * ============================================================================
  */

"create"        { return TOK_CREATE; }
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

 /* ============================================================================
  * KEYWORDS - Types
  * ============================================================================
  */

"number"        { return TOK_TYPE_NUMBER; }
"text"          { return TOK_TYPE_TEXT; }
"decimal"       { return TOK_TYPE_DECIMAL; }
"flag"          { return TOK_TYPE_FLAG; }
"list"          { return TOK_TYPE_LIST; }
"nothing"       { return TOK_TYPE_NOTHING; }

 /* ============================================================================
  * KEYWORDS - Control Flow
  * ============================================================================
  */

"if"            { return TOK_IF; }
"then"          { return TOK_THEN; }
"otherwise"     { return TOK_OTHERWISE; }
"else"          { return TOK_ELSE; }
"end"           { return TOK_END; }
"repeat"        { return TOK_REPEAT; }
"times"         { return TOK_TIMES; }
"while"         { return TOK_WHILE; }
"do"            { return TOK_DO; }
"for"           { return TOK_FOR; }
"each"          { return TOK_EACH; }
"in"            { return TOK_IN; }
"from"          { return TOK_FROM; }
"until"         { return TOK_UNTIL; }
"stop"          { return TOK_STOP; }
"skip"          { return TOK_SKIP; }

 /* ============================================================================
  * KEYWORDS - Functions
  * ============================================================================
  */

"define"        { return TOK_DEFINE; }
"function"      { return TOK_FUNCTION; }
"that"          { return TOK_THAT; }
"takes"         { return TOK_TAKES; }
"returns"       { return TOK_RETURNS; }
"give"          { return TOK_GIVE; }
"back"          { return TOK_BACK; }
"call"          { return TOK_CALL; }
"with"          { return TOK_WITH; }

 /* ============================================================================
  * KEYWORDS - I/O
  * ============================================================================
  */

"display"       { return TOK_DISPLAY; }
"show"          { return TOK_SHOW; }
"print"         { return TOK_PRINT; }
"ask"           { return TOK_ASK; }
"read"          { return TOK_READ; }
"remember"      { return TOK_REMEMBER; }
"save"          { return TOK_SAVE; }
"into"          { return TOK_INTO; }

 /* ============================================================================
  * KEYWORDS - Security
  * ============================================================================
  */

"enter"         { return TOK_ENTER; }
"secure"        { return TOK_SECURE; }
"zone"          { return TOK_ZONE; }
"safe"          { return TOK_SAFE; }

 /* ============================================================================
  * KEYWORDS - Logical
  * ============================================================================
  */

"is"            { return TOK_IS; }
"not"           { return TOK_NOT; }
"or"            { return TOK_OR; }
"true"          { return TOK_TRUE; }
"false"         { return TOK_FALSE; }
"yes"           { return TOK_YES; }
"no"            { return TOK_NO; }

 /* ============================================================================
  * KEYWORDS - Comparison (single words, multi-word handled above)
  * ============================================================================
  */

"greater"       { return TOK_GREATER; }
"less"          { return TOK_LESS; }
"than"          { return TOK_THAN; }

 /* ============================================================================
  * KEYWORDS - Arithmetic (Natural Words)
  * ============================================================================
  */

"plus"          { return TOK_PLUS; }
"minus"         { return TOK_MINUS; }
"multiplied"    { return TOK_MULTIPLIED; }
"divided"       { return TOK_DIVIDED; }
"by"            { return TOK_BY; }
"modulo"        { return TOK_MODULO; }
"remainder"     { return TOK_REMAINDER; }
"of"            { return TOK_OF; }
"power"         { return TOK_POWER; }
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
                            yylval_float = atof(yytext);
                            return TOK_FLOAT;
                        }

{FLOAT}{EXPONENT}?      { 
                            yylval_float = atof(yytext);
                            return TOK_FLOAT;
                        }

{INTEGER}               { 
                            yylval_int = atoll(yytext);
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
                            /* yylval_string will be set in lexer_next_token using yytext */
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
    yylloc.first_line = current_line;
    yylloc.first_column = current_column;
    
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
    
    yylloc.last_line = current_line;
    yylloc.last_column = current_column;
}

/* Handle newline - update line counter */
static void handle_newline(void) {
    current_line++;
    current_column = 1;
}

/* Create a SourceLocation from current position */
static SourceLocation make_location(void) {
    SourceLocation loc;
    loc.filename = current_filename;
    loc.first_line = yylloc.first_line;
    loc.first_column = yylloc.first_column;
    loc.last_line = yylloc.last_line;
    loc.last_column = yylloc.last_column;
    return loc;
}

/* Report a lexer error */
static void report_lexer_error(const char *msg) {
    fprintf(stderr, "\033[1;31mLexer Error\033[0m at %s:%d:%d: %s\n",
            current_filename, current_line, current_column, msg);
    lexer_error_count++;
}

/* ============================================================================
 * PUBLIC LEXER API
 * ============================================================================
 */

/* Global variables for token values */
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
