/*
 * NatureLang Compiler - naturec Driver
 * Copyright (c) 2026
 *
 * End-to-end compiler driver:
 *   .nl source → Lex → Parse → AST → IR → Optimize → Codegen → .c file
 *
 * Commands:
 *   naturec build <file.nl>    Compile to C (and optionally to binary)
 *   naturec run  <file.nl>     Compile to C, compile with gcc, and run
 *   naturec check <file.nl>    Parse and type-check only
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/stat.h>
#include <unistd.h>

#include "parser.h"
#include "ast.h"
#include "ir.h"
#include "optimizer.h"
#include "ir_codegen.h"

/* External from Bison */
extern FILE *yyin;

/* ============================================================================
 * CONFIGURATION
 * ============================================================================
 */
typedef struct {
    const char *input_file;
    const char *output_file;      /* NULL = auto-derived */
    int opt_level;                /* 0, 1, 2 */
    int compile_c;                /* Also compile the generated .c to binary */
    int run_after;                /* Compile and run */
    int check_only;               /* Parse + check only, no codegen */
    int verbose;
    int keep_c;                   /* Keep .c file after compiling to binary */
    int emit_comments;
} NaturecConfig;

static void print_usage(const char *prog) {
    printf("NatureLang Compiler v0.1\n\n");
    printf("Usage: %s <command> [options] <file.nl>\n\n", prog);
    printf("Commands:\n");
    printf("  build   Compile .nl to C source (and optionally to binary)\n");
    printf("  run     Compile and execute immediately\n");
    printf("  check   Parse and validate only (no code output)\n");
    printf("\nOptions:\n");
    printf("  -o, --output <file>   Output file name\n");
    printf("  -O, --optimize <N>    Optimization level (0, 1, 2) [default: 1]\n");
    printf("  -c, --compile         Also compile generated C to binary with gcc\n");
    printf("  -k, --keep            Keep .c file when compiling to binary\n");
    printf("  -v, --verbose         Verbose output\n");
    printf("  --comments            Include TAC comments in generated C\n");
    printf("  -h, --help            Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s build hello.nl             → hello.c\n", prog);
    printf("  %s build -c hello.nl          → hello.c + hello (binary)\n", prog);
    printf("  %s build -c -O2 hello.nl      → optimized binary\n", prog);
    printf("  %s run hello.nl               → compile + run\n", prog);
    printf("  %s check hello.nl             → parse/validate only\n", prog);
}

/* Derive output filename from input: foo.nl → foo.c */
static char *derive_output(const char *input, const char *ext) {
    const char *dot = strrchr(input, '.');
    const char *slash = strrchr(input, '/');
    /* Use just the basename */
    const char *base = slash ? slash + 1 : input;
    size_t base_len = dot && dot > base ? (size_t)(dot - base) : strlen(base);
    size_t ext_len = strlen(ext);
    char *out = malloc(base_len + ext_len + 1);
    memcpy(out, base, base_len);
    memcpy(out + base_len, ext, ext_len + 1);
    return out;
}

/* ============================================================================
 * PIPELINE STAGES
 * ============================================================================
 */

/* Stage 1: Parse input file → AST */
static ASTNode *stage_parse(const char *filename, int verbose) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Error: cannot open '%s'\n", filename);
        return NULL;
    }
    if (verbose) fprintf(stderr, "[1/4] Parsing %s...\n", filename);

    ASTNode *ast = naturelang_parse(f);
    fclose(f);

    if (!ast) {
        fprintf(stderr, "Error: parsing failed for '%s'\n", filename);
        return NULL;
    }
    if (verbose) {
        size_t n = (ast->type == AST_PROGRAM && ast->data.program.statements)
                   ? ast->data.program.statements->count : 0;
        fprintf(stderr, "       %zu top-level statement(s)\n", n);
    }
    return ast;
}

/* Stage 2: AST → IR */
static TACProgram *stage_ir(ASTNode *ast, int verbose) {
    if (verbose) fprintf(stderr, "[2/4] Generating IR...\n");
    TACProgram *ir = ir_generate(ast);
    if (!ir) {
        fprintf(stderr, "Error: IR generation failed\n");
        return NULL;
    }
    if (verbose) {
        fprintf(stderr, "       %d instructions generated\n",
                ir_count_total(ir));
    }
    return ir;
}

/* Stage 3: Optimize IR */
static int stage_optimize(TACProgram *ir, int level, int verbose) {
    if (level <= 0) return 1;
    if (verbose) fprintf(stderr, "[3/4] Optimizing (O%d)...\n", level);

    OptOptions opts = opt_default_options((OptLevel)level);
    opts.verbose = 0;
    OptStats stats = ir_optimize(ir, &opts);

    if (verbose) {
        int eliminated = stats.total_instructions_before - stats.total_instructions_after;
        fprintf(stderr, "       %d instructions eliminated (%.1f%% reduction)\n",
                eliminated,
                stats.total_instructions_before > 0
                    ? 100.0 * eliminated / stats.total_instructions_before
                    : 0.0);
    }
    return 1;
}

/* Stage 4: IR → C code */
static char *stage_codegen(TACProgram *ir, int emit_comments, int verbose) {
    if (verbose) fprintf(stderr, "[4/4] Generating C code...\n");

    IRCodegenOptions opts = ir_codegen_default_options();
    opts.emit_comments = emit_comments;

    IRCodegenResult result = ir_codegen_generate(ir, &opts);
    if (!result.success) {
        fprintf(stderr, "Error: code generation failed: %s\n",
                result.error_message);
        ir_codegen_result_free(&result);
        return NULL;
    }

    if (verbose) {
        fprintf(stderr, "       %zu bytes of C code generated\n",
                result.code_length);
    }

    char *code = result.generated_code;
    result.generated_code = NULL;  /* Transfer ownership */
    return code;
}

/* ============================================================================
 * MAIN
 * ============================================================================
 */
int main(int argc, char *argv[]) {
    NaturecConfig cfg = {
        .input_file = NULL,
        .output_file = NULL,
        .opt_level = 1,
        .compile_c = 0,
        .run_after = 0,
        .check_only = 0,
        .verbose = 0,
        .keep_c = 0,
        .emit_comments = 0,
    };

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    /* Parse command */
    const char *command = argv[1];
    if (strcmp(command, "build") == 0) {
        /* default */
    } else if (strcmp(command, "run") == 0) {
        cfg.run_after = 1;
        cfg.compile_c = 1;
    } else if (strcmp(command, "check") == 0) {
        cfg.check_only = 1;
    } else if (strcmp(command, "-h") == 0 || strcmp(command, "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    } else {
        fprintf(stderr, "Unknown command: '%s'\n", command);
        print_usage(argv[0]);
        return 1;
    }

    /* Parse options (skip argv[0] and argv[1]) */
    optind = 2;
    static struct option long_options[] = {
        {"output",   required_argument, 0, 'o'},
        {"optimize", required_argument, 0, 'O'},
        {"compile",  no_argument,       0, 'c'},
        {"keep",     no_argument,       0, 'k'},
        {"verbose",  no_argument,       0, 'v'},
        {"comments", no_argument,       0, 'C'},
        {"help",     no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "o:O:ckvCh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'o':
                cfg.output_file = optarg;
                break;
            case 'O':
                cfg.opt_level = atoi(optarg);
                if (cfg.opt_level < 0 || cfg.opt_level > 2) {
                    fprintf(stderr, "Invalid optimization level (use 0, 1, or 2)\n");
                    return 1;
                }
                break;
            case 'c':
                cfg.compile_c = 1;
                break;
            case 'k':
                cfg.keep_c = 1;
                break;
            case 'v':
                cfg.verbose = 1;
                break;
            case 'C':
                cfg.emit_comments = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                return 1;
        }
    }

    /* Remaining arg is the input file */
    if (optind >= argc) {
        fprintf(stderr, "Error: no input file specified\n");
        return 1;
    }
    cfg.input_file = argv[optind];

    /* Verify input file exists */
    struct stat st;
    if (stat(cfg.input_file, &st) != 0) {
        fprintf(stderr, "Error: cannot find '%s'\n", cfg.input_file);
        return 1;
    }

    /* ---- Pipeline begins ---- */

    /* Stage 1: Parse */
    ASTNode *ast = stage_parse(cfg.input_file, cfg.verbose);
    if (!ast) return 1;

    /* If check-only, we're done */
    if (cfg.check_only) {
        size_t n = (ast->type == AST_PROGRAM && ast->data.program.statements)
                   ? ast->data.program.statements->count : 0;
        fprintf(stderr, "OK: %s parsed successfully (%zu statements)\n",
                cfg.input_file, n);
        ast_free(ast);
        return 0;
    }

    /* Stage 2: IR */
    TACProgram *ir = stage_ir(ast, cfg.verbose);
    if (!ir) { ast_free(ast); return 1; }

    /* Stage 3: Optimize */
    if (!stage_optimize(ir, cfg.opt_level, cfg.verbose)) {
        ir_free(ir); ast_free(ast); return 1;
    }

    /* Stage 4: Codegen */
    char *c_code = stage_codegen(ir, cfg.emit_comments, cfg.verbose);
    ir_free(ir);
    ast_free(ast);
    if (!c_code) return 1;

    /* Write .c file */
    char *c_file = cfg.output_file
                   ? strdup(cfg.output_file)
                   : derive_output(cfg.input_file, ".c");

    FILE *out = fopen(c_file, "w");
    if (!out) {
        fprintf(stderr, "Error: cannot write '%s'\n", c_file);
        free(c_code); free(c_file);
        return 1;
    }
    fputs(c_code, out);
    fclose(out);
    free(c_code);

    if (cfg.verbose || !cfg.compile_c) {
        fprintf(stderr, "Generated: %s\n", c_file);
    }

    /* Optionally compile to binary */
    if (cfg.compile_c || cfg.run_after) {
        char *bin_file = derive_output(cfg.input_file, "");
        /* Remove trailing dot if any */
        size_t bl = strlen(bin_file);
        if (bl > 0 && bin_file[bl - 1] == '.') bin_file[bl - 1] = '\0';

        char cmd[4096];
        snprintf(cmd, sizeof(cmd),
                 "gcc -std=c11 -O2 -o %s %s -Iruntime runtime/naturelang_runtime.c -lm",
                 bin_file, c_file);

        if (cfg.verbose) fprintf(stderr, "Compiling: %s\n", cmd);

        int rc = system(cmd);
        if (rc != 0) {
            fprintf(stderr, "Error: gcc compilation failed (exit %d)\n", rc);
            free(c_file); free(bin_file);
            return 1;
        }

        if (cfg.verbose) {
            fprintf(stderr, "Binary: %s\n", bin_file);
        }

        /* Remove .c file if not keeping */
        if (!cfg.keep_c && !cfg.run_after) {
            unlink(c_file);
        }

        /* Run if requested */
        if (cfg.run_after) {
            char run_cmd[4096];
            snprintf(run_cmd, sizeof(run_cmd), "./%s", bin_file);
            if (cfg.verbose) fprintf(stderr, "Running: %s\n\n", run_cmd);
            rc = system(run_cmd);
            /* Clean up */
            unlink(bin_file);
            if (!cfg.keep_c) unlink(c_file);
            free(c_file); free(bin_file);
            return WEXITSTATUS(rc);
        }

        fprintf(stderr, "Compiled: %s → %s\n", cfg.input_file, bin_file);
        free(bin_file);
    }

    free(c_file);
    return 0;
}
