/*
 * NatureLang Compiler
 * Copyright (c) 2024
 * 
 * Parser Test Driver
 * 
 * Test program for the NatureLang parser.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "parser.h"
#include "ast.h"
#include "ir.h"
#include "optimizer.h"

/* External from Bison */
extern FILE *yyin;

/* ============================================================================
 * USAGE
 * ============================================================================
 */

static void print_usage(const char *prog) {
    printf("Usage: %s [options] [file]\n", prog);
    printf("\nOptions:\n");
    printf("  -h, --help       Show this help message\n");
    printf("  -v, --verbose    Enable verbose output\n");
    printf("  -t, --tree       Print the AST tree\n");
    printf("  -r, --ir         Generate and print TAC IR\n");
    printf("  -O, --optimize N Optimize IR (0=none, 1=basic, 2=full)\n");
    printf("  -q, --quiet      Suppress output (just check for errors)\n");
    printf("\nIf no file is specified, reads from stdin.\n");
    printf("\nExamples:\n");
    printf("  %s program.nl              Parse a file\n", prog);
    printf("  %s -t program.nl           Parse and print AST\n", prog);
    printf("  echo 'display 42' | %s     Parse from stdin\n", prog);
}

/* ============================================================================
 * MAIN
 * ============================================================================
 */

int main(int argc, char *argv[]) {
    int verbose = 0;
    int print_tree = 0;
    int print_ir = 0;
    int opt_level = -1;  /* -1 means not requested */
    int quiet = 0;
    const char *filename = NULL;
    
    /* Parse command line options */
    static struct option long_options[] = {
        {"help",     no_argument,       0, 'h'},
        {"verbose",  no_argument,       0, 'v'},
        {"tree",     no_argument,       0, 't'},
        {"ir",       no_argument,       0, 'r'},
        {"optimize", required_argument, 0, 'O'},
        {"quiet",    no_argument,       0, 'q'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "hvtrO:q", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'v':
                verbose = 1;
                break;
            case 't':
                print_tree = 1;
                break;
            case 'r':
                print_ir = 1;
                break;
            case 'O':
                opt_level = atoi(optarg);
                if (opt_level < 0 || opt_level > 2) {
                    fprintf(stderr, "Invalid optimization level: %s (use 0, 1, or 2)\n", optarg);
                    return 1;
                }
                print_ir = 1;  /* Implicitly show IR when optimizing */
                break;
            case 'q':
                quiet = 1;
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    /* Get input file */
    FILE *input = stdin;
    if (optind < argc) {
        filename = argv[optind];
        input = fopen(filename, "r");
        if (input == NULL) {
            fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
            return 1;
        }
        if (verbose) {
            printf("Parsing file: %s\n", filename);
        }
    } else if (verbose) {
        printf("Reading from stdin...\n");
    }
    
    /* Parse the input */
    if (verbose) {
        printf("Starting parser...\n");
    }
    
    ASTNode *ast = naturelang_parse(input);
    
    /* Close file if we opened one */
    if (filename != NULL) {
        fclose(input);
    }
    
    /* Check result */
    if (ast == NULL) {
        if (!quiet) {
            fprintf(stderr, "Parsing failed!\n");
        }
        return 1;
    }
    
    if (!quiet) {
        printf("Parsing successful!\n");
        
        if (ast->type == AST_PROGRAM) {
            size_t count = ast->data.program.statements ? 
                          ast->data.program.statements->count : 0;
            printf("Program has %zu top-level statement(s)\n", count);
        }
    }
    
    /* Print AST if requested */
    if (print_tree) {
        printf("\n=== Abstract Syntax Tree ===\n\n");
        ast_print(ast, 0);
        printf("\n");
    }

    /* Generate and print IR if requested */
    if (print_ir) {
        printf("\n");
        TACProgram *ir = ir_generate(ast);
        if (ir) {
            /* Optimize if requested */
            if (opt_level >= 0) {
                OptOptions opts = opt_default_options((OptLevel)opt_level);
                opts.verbose = verbose;
                OptStats stats = ir_optimize(ir, &opts);
                if (!quiet) {
                    opt_print_stats(&stats);
                    printf("\n");
                }
            }
            ir_print(ir);
            ir_free(ir);
        } else {
            fprintf(stderr, "IR generation failed!\n");
        }
    }
    
    /* Clean up */
    ast_free(ast);
    
    if (verbose) {
        printf("Done.\n");
    }
    
    return 0;
}
