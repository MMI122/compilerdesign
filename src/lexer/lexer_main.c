/*
 * NatureLang Compiler
 * Copyright (c) 2026
 * 
 * Lexer Test Driver
 * 
 * A standalone program to test the lexer by tokenizing
 * NatureLang source files and displaying the results.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "tokens.h"
#include "lexer.h"

/* ============================================================================
 * COMMAND LINE OPTIONS
 * ============================================================================
 */

typedef struct {
    int verbose;        /* Show detailed token info */
    int quiet;          /* Only show errors */
    int show_comments;  /* Include comment tokens */
    int show_location;  /* Show source locations */
    int interactive;    /* Interactive REPL mode */
    int debug;          /* Debug mode with extra info */
    const char *input_file;
} LexerOptions;

static void print_usage(const char *program_name) {
    printf("NatureLang Lexer Test Driver\n\n");
    printf("Usage: %s [options] [input_file]\n\n", program_name);
    printf("Options:\n");
    printf("  -v, --verbose      Show detailed token information\n");
    printf("  -q, --quiet        Only show errors (suppress normal output)\n");
    printf("  -c, --comments     Include comment tokens in output\n");
    printf("  -l, --location     Show source location for each token\n");
    printf("  -i, --interactive  Interactive mode (read from stdin)\n");
    printf("  -d, --debug        Enable debug output\n");
    printf("  -h, --help         Show this help message\n");
    printf("\n");
    printf("If no input file is specified, reads from stdin.\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s program.nl           # Tokenize a file\n", program_name);
    printf("  %s -v program.nl        # Verbose output\n", program_name);
    printf("  %s -i                   # Interactive mode\n", program_name);
    printf("  echo 'create a number called x' | %s\n", program_name);
}

static LexerOptions parse_arguments(int argc, char *argv[]) {
    LexerOptions opts = {0};
    
    static struct option long_options[] = {
        {"verbose",     no_argument, 0, 'v'},
        {"quiet",       no_argument, 0, 'q'},
        {"comments",    no_argument, 0, 'c'},
        {"location",    no_argument, 0, 'l'},
        {"interactive", no_argument, 0, 'i'},
        {"debug",       no_argument, 0, 'd'},
        {"help",        no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "vqclidh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'v': opts.verbose = 1; break;
            case 'q': opts.quiet = 1; break;
            case 'c': opts.show_comments = 1; break;
            case 'l': opts.show_location = 1; break;
            case 'i': opts.interactive = 1; break;
            case 'd': opts.debug = 1; break;
            case 'h':
                print_usage(argv[0]);
                exit(0);
            default:
                print_usage(argv[0]);
                exit(1);
        }
    }
    
    if (optind < argc) {
        opts.input_file = argv[optind];
    }
    
    return opts;
}

/* ============================================================================
 * TOKEN DISPLAY FUNCTIONS
 * ============================================================================
 */

static void print_token_simple(Token *token) {
    printf("%-20s %s\n", 
           token_type_to_string(token->type),
           token->lexeme ? token->lexeme : "");
}

static void print_token_with_location(Token *token) {
    printf("%4d:%-3d  %-20s %s\n",
           token->loc.first_line,
           token->loc.first_column,
           token_type_to_string(token->type),
           token->lexeme ? token->lexeme : "");
}

static void print_token_verbose(Token *token) {
    token_print_debug(token);
}

/* ============================================================================
 * LEXER TEST MODES
 * ============================================================================
 */

static int run_lexer_file(LexerOptions *opts) {
    if (lexer_init_file(opts->input_file) != 0) {
        fprintf(stderr, "Error: Could not open file '%s'\n", opts->input_file);
        return 1;
    }
    
    if (!opts->quiet) {
        printf("=== Tokenizing: %s ===\n\n", opts->input_file);
    }
    
    int token_count = 0;
    int error_count = 0;
    
    Token *token;
    while ((token = lexer_next_token()) != NULL) {
        /* Skip comments unless requested */
        if (!opts->show_comments && 
            (token->type == TOK_COMMENT || token->type == TOK_BLOCK_COMMENT)) {
            token_free(token);
            continue;
        }
        
        if (token->type == TOK_ERROR) {
            error_count++;
        }
        
        if (!opts->quiet) {
            if (opts->verbose || opts->debug) {
                print_token_verbose(token);
            } else if (opts->show_location) {
                print_token_with_location(token);
            } else {
                print_token_simple(token);
            }
        }
        
        token_count++;
        
        if (token->type == TOK_EOF) {
            token_free(token);
            break;
        }
        token_free(token);
    }
    
    if (!opts->quiet) {
        printf("\n=== Summary ===\n");
        printf("Total tokens: %d\n", token_count);
        printf("Errors: %d\n", error_count);
    }
    
    lexer_cleanup();
    return error_count > 0 ? 1 : 0;
}

static int run_lexer_stdin(LexerOptions *opts) {
    if (!opts->quiet) {
        printf("=== Reading from stdin ===\n");
        printf("(Enter NatureLang code, Ctrl+D to finish)\n\n");
    }
    
    /* Read all input into buffer */
    char buffer[65536];
    size_t total = 0;
    size_t bytes;
    
    while ((bytes = fread(buffer + total, 1, sizeof(buffer) - total - 1, stdin)) > 0) {
        total += bytes;
        if (total >= sizeof(buffer) - 1) break;
    }
    buffer[total] = '\0';
    
    if (total == 0) {
        if (!opts->quiet) {
            printf("No input received.\n");
        }
        return 0;
    }
    
    lexer_init_string(buffer);
    
    int token_count = 0;
    int error_count = 0;
    
    Token *token;
    while ((token = lexer_next_token()) != NULL) {
        if (!opts->show_comments && 
            (token->type == TOK_COMMENT || token->type == TOK_BLOCK_COMMENT)) {
            token_free(token);
            continue;
        }
        
        if (token->type == TOK_ERROR) {
            error_count++;
        }
        
        if (!opts->quiet) {
            if (opts->verbose || opts->debug) {
                print_token_verbose(token);
            } else if (opts->show_location) {
                print_token_with_location(token);
            } else {
                print_token_simple(token);
            }
        }
        
        token_count++;
        
        if (token->type == TOK_EOF) {
            token_free(token);
            break;
        }
        token_free(token);
    }
    
    if (!opts->quiet) {
        printf("\n=== Summary ===\n");
        printf("Total tokens: %d\n", token_count);
        printf("Errors: %d\n", error_count);
    }
    
    lexer_cleanup();
    return error_count > 0 ? 1 : 0;
}

static int run_interactive_mode(LexerOptions *opts) {
    printf("NatureLang Lexer - Interactive Mode\n");
    printf("Type NatureLang code and press Enter to tokenize.\n");
    printf("Type 'exit' or 'quit' to quit.\n\n");
    
    char line[4096];
    
    while (1) {
        printf("nl> ");
        fflush(stdout);
        
        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\nGoodbye!\n");
            break;
        }
        
        /* Remove trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        
        /* Check for exit commands */
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            printf("Goodbye!\n");
            break;
        }
        
        /* Skip empty lines */
        if (line[0] == '\0') {
            continue;
        }
        
        /* Tokenize the line */
        lexer_init_string(line);
        
        printf("Tokens:\n");
        Token *token;
        while ((token = lexer_next_token()) != NULL) {
            if (token->type != TOK_EOF) {
                printf("  %-18s %s\n", 
                       token_type_to_string(token->type),
                       token->lexeme ? token->lexeme : "");
            }
            
            if (token->type == TOK_EOF) {
                token_free(token);
                break;
            }
            token_free(token);
        }
        printf("\n");
        
        lexer_cleanup();
    }
    
    return 0;
}

/* ============================================================================
 * MAIN FUNCTION
 * ============================================================================
 */

int main(int argc, char *argv[]) {
    LexerOptions opts = parse_arguments(argc, argv);
    
    if (opts.interactive) {
        return run_interactive_mode(&opts);
    } else if (opts.input_file) {
        return run_lexer_file(&opts);
    } else {
        return run_lexer_stdin(&opts);
    }
}
