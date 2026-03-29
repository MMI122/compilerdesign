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
    /* ইনপুট .nl source ফাইল path। */
    const char *input_file;
    /* আউটপুট ফাইল path; NULL হলে input থেকে auto derive হবে। */
    const char *output_file;      /* NULL = auto-derived */
    /* optimization level: 0 = off, 1 = basic, 2 = aggressive। */
    int opt_level;                /* 0, 1, 2 */
    /* true হলে generated C-কে gcc দিয়ে binary-তেও compile করা হবে। */
    int compile_c;                /* Also compile the generated .c to binary */
    /* true হলে compile শেষেই binary run করা হবে। */
    int run_after;                /* Compile and run */
    /* true হলে parse/check পর্যন্ত গিয়ে থামবে; codegen হবে না। */
    int check_only;               /* Parse + check only, no codegen */
    /* pipeline progress stderr-এ print করার flag। */
    int verbose;
    /* compile_c mode-এ .c intermediate file রাখবে কিনা। */
    int keep_c;                   /* Keep .c file after compiling to binary */
    /* generated C-তে TAC debugging comments include করবে কিনা। */
    int emit_comments;
} NaturecConfig;

/*
 * CLI help printer
 * কী করে: naturec কমান্ডের usage, command list, option list, example দেখায়।
 * example: naturec build hello.nl
 */
static void print_usage(const char *prog) {
    /* সংস্করণ/টুল পরিচিতি header print। */
    printf("NatureLang Compiler v0.1\n\n");
    /* generic command syntax দেখাই। */
    printf("Usage: %s <command> [options] <file.nl>\n\n", prog);
    /* command group title। */
    printf("Commands:\n");
    /* build command summary। */
    printf("  build   Compile .nl to C source (and optionally to binary)\n");
    /* run command summary। */
    printf("  run     Compile and execute immediately\n");
    /* check command summary। */
    printf("  check   Parse and validate only (no code output)\n");
    /* section separator newline। */
    printf("\nOptions:\n");
    /* output file override option। */
    printf("  -o, --output <file>   Output file name\n");
    /* optimization level selector। */
    printf("  -O, --optimize <N>    Optimization level (0, 1, 2) [default: 1]\n");
    /* generated C কে gcc দিয়ে compile option। */
    printf("  -c, --compile         Also compile generated C to binary with gcc\n");
    /* intermediate .c retain option। */
    printf("  -k, --keep            Keep .c file when compiling to binary\n");
    /* verbose logging option। */
    printf("  -v, --verbose         Verbose output\n");
    /* TAC comments emit option। */
    printf("  --comments            Include TAC comments in generated C\n");
    /* help option। */
    printf("  -h, --help            Show this help message\n");
    /* example section heading। */
    printf("\nExamples:\n");
    /* build command example -> hello.c। */
    printf("  %s build hello.nl             → hello.c\n", prog);
    /* build + compile binary example। */
    printf("  %s build -c hello.nl          → hello.c + hello (binary)\n", prog);
    /* optimized compile example। */
    printf("  %s build -c -O2 hello.nl      → optimized binary\n", prog);
    /* run example। */
    printf("  %s run hello.nl               → compile + run\n", prog);
    /* check example। */
    printf("  %s check hello.nl             → parse/validate only\n", prog);
}

/* Derive output filename from input: foo.nl → foo.c */
/*
 * output name derivation helper
 * কী করে: input path থেকে basename নিয়ে extension বসিয়ে output নাম বানায়।
 * example: "/tmp/hello.nl" + ".c" => "hello.c"
 */
static char *derive_output(const char *input, const char *ext) {
    /* শেষ dot খুঁজি extension কাটার জন্য। */
    const char *dot = strrchr(input, '.');
    /* শেষ slash খুঁজি basename আলাদা করার জন্য। */
    const char *slash = strrchr(input, '/');
    /* Use just the basename */
    /* slash থাকলে slash-এর পরের অংশ, নাহলে পুরো input-ই basename। */
    const char *base = slash ? slash + 1 : input;
    /* dot basename-এর পরে থাকলে dot-এর আগ পর্যন্ত length, নাহলে পুরো base length। */
    size_t base_len = dot && dot > base ? (size_t)(dot - base) : strlen(base);
    /* target extension length (যেমন .c হলে 2)। */
    size_t ext_len = strlen(ext);
    /* basename + extension + '\0' এর জন্য memory allocate। */
    char *out = malloc(base_len + ext_len + 1);
    /* basename অংশ copy। */
    memcpy(out, base, base_len);
    /* extension সহ trailing NUL copy। */
    memcpy(out + base_len, ext, ext_len + 1);
    /* caller-owned string pointer ফেরত। */
    return out;
}

/* ============================================================================
 * PIPELINE STAGES
 * ============================================================================
 */

/* Stage 1: Parse input file → AST */
/*
 * stage_parse
 * কী করে: source file খুলে parser চালিয়ে AST তৈরি করে।
 * example: "hello.nl" -> AST_PROGRAM root node
 */
static ASTNode *stage_parse(const char *filename, int verbose) {
    /* source file read mode-এ open করি। */
    FILE *f = fopen(filename, "r");
    /* open fail হলে error print করে stage fail return। */
    if (!f) {
        fprintf(stderr, "Error: cannot open '%s'\n", filename);
        return NULL;
    }
    /* verbose mode হলে current stage progress log। */
    if (verbose) fprintf(stderr, "[1/4] Parsing %s...\n", filename);

    /* parser frontend চালিয়ে AST তৈরি করি। */
    ASTNode *ast = naturelang_parse(f);
    /* parse শেষ, input FILE handle close করি। */
    fclose(f);

    /* parser failure হলে error report সহ NULL return। */
    if (!ast) {
        fprintf(stderr, "Error: parsing failed for '%s'\n", filename);
        return NULL;
    }
    /* verbose summary: top-level statement count দেখাই। */
    if (verbose) {
        size_t n = (ast->type == AST_PROGRAM && ast->data.program.statements)
                   ? ast->data.program.statements->count : 0;
        fprintf(stderr, "       %zu top-level statement(s)\n", n);
    }
    /* সফল AST caller-কে ফেরত। */
    return ast;
}

/* Stage 2: AST → IR */
/*
 * stage_ir
 * কী করে: parsed AST থেকে TAC/IR program তৈরি করে।
 * example: assignment AST -> TAC_LOAD + TAC_STORE টাইপ instruction set
 */
static TACProgram *stage_ir(ASTNode *ast, int verbose) {
    /* verbose mode-এ IR stage শুরু log। */
    if (verbose) fprintf(stderr, "[2/4] Generating IR...\n");
    /* AST থেকে IR generator invoke। */
    TACProgram *ir = ir_generate(ast);
    /* IR generation fail হলে error return। */
    if (!ir) {
        fprintf(stderr, "Error: IR generation failed\n");
        return NULL;
    }
    /* verbose mode-এ মোট instruction সংখ্যা print। */
    if (verbose) {
        fprintf(stderr, "       %d instructions generated\n",
                ir_count_total(ir));
    }
    /* valid IR program pointer return। */
    return ir;
}

/* Stage 3: Optimize IR */
/*
 * stage_optimize
 * কী করে: opt level > 0 হলে IR optimization pass চালায়।
 * example: dead temporary assignments eliminate করা
 */
static int stage_optimize(TACProgram *ir, int level, int verbose) {
    /* O0 (বা negative) হলে optimization skip করে success ধরি। */
    if (level <= 0) return 1;
    /* verbose mode-এ optimization stage header। */
    if (verbose) fprintf(stderr, "[3/4] Optimizing (O%d)...\n", level);

    /* নির্বাচিত level থেকে optimizer option struct তৈরি। */
    OptOptions opts = opt_default_options((OptLevel)level);
    /* driver-level verbose output নিয়ন্ত্রণ করি (এখানে concise রাখছি)। */
    opts.verbose = 0;
    /* IR optimizer run করে stats পাই। */
    OptStats stats = ir_optimize(ir, &opts);

    /* verbose থাকলে before/after থেকে reduction summary print। */
    if (verbose) {
        int eliminated = stats.total_instructions_before - stats.total_instructions_after;
        fprintf(stderr, "       %d instructions eliminated (%.1f%% reduction)\n",
                eliminated,
                stats.total_instructions_before > 0
                    ? 100.0 * eliminated / stats.total_instructions_before
                    : 0.0);
    }
    /* optimization stage success (বর্তমানে fatal error path নেই)। */
    return 1;
}

/* Stage 4: IR → C code */
/*
 * stage_codegen
 * কী করে: IR থেকে final C source text বানিয়ে heap string হিসেবে ফেরত দেয়।
 * example: TAC_DISPLAY -> generated printf call
 */
static char *stage_codegen(TACProgram *ir, int emit_comments, int verbose) {
    /* verbose mode-এ codegen stage header। */
    if (verbose) fprintf(stderr, "[4/4] Generating C code...\n");

    /* codegen default options নিয়ে শুরু। */
    IRCodegenOptions opts = ir_codegen_default_options();
    /* CLI flag অনুযায়ী TAC comments include toggle। */
    opts.emit_comments = emit_comments;

    /* IR -> C generation run করি। */
    IRCodegenResult result = ir_codegen_generate(ir, &opts);
    /* generation ব্যর্থ হলে message print + result cleanup + NULL return। */
    if (!result.success) {
        fprintf(stderr, "Error: code generation failed: %s\n",
                result.error_message);
        ir_codegen_result_free(&result);
        return NULL;
    }

    /* verbose mode-এ generated C code length report। */
    if (verbose) {
        fprintf(stderr, "       %zu bytes of C code generated\n",
                result.code_length);
    }

    /* generated_code ownership local variable-এ নিই। */
    char *code = result.generated_code;
    /* double-free এড়াতে result থেকে ownership detach করি। */
    result.generated_code = NULL;  /* Transfer ownership */
    /* caller-owned C source buffer ফেরত। */
    return code;
}

/* ============================================================================
 * MAIN
 * ============================================================================
 */
int main(int argc, char *argv[]) {
    /*
     * main driver entry
     * কী করে: command parse করে 4-stage pipeline চালায় এবং প্রয়োজন হলে gcc/run করে।
     * example: naturec run hello.nl -> hello.c build + binary run
     */
    /* default config values দিয়ে runtime config initialize। */
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

    /* command না দিলে usage দেখিয়ে error exit। */
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    /* Parse command */
    /* argv[1]-এ subcommand (build/run/check/help) ধরি। */
    const char *command = argv[1];
    if (strcmp(command, "build") == 0) {
        /* default */
        /* build mode-এ default cfg যথেষ্ট; extra flag সেট দরকার নেই। */
    } else if (strcmp(command, "run") == 0) {
        /* run mode: compile + execute, তাই compile_c অন করি। */
        cfg.run_after = 1;
        cfg.compile_c = 1;
    } else if (strcmp(command, "check") == 0) {
        /* check mode: parse/validate only; codegen skip হবে। */
        cfg.check_only = 1;
    } else if (strcmp(command, "-h") == 0 || strcmp(command, "--help") == 0) {
        /* help request হলে usage print করে success exit। */
        print_usage(argv[0]);
        return 0;
    } else {
        /* অজানা command হলে error + usage দেখাই। */
        fprintf(stderr, "Unknown command: '%s'\n", command);
        print_usage(argv[0]);
        return 1;
    }

    /* Parse options (skip argv[0] and argv[1]) */
    /* getopt শুরু index command-এর পর (2) সেট করি। */
    optind = 2;
    /* long option table define করি getopt_long-এর জন্য। */
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

    /* parsed short option char code রাখার variable। */
    int opt;
    /* short options string: o/O arg নেয়, c/k/v/C/h arg নেয় না। */
    while ((opt = getopt_long(argc, argv, "o:O:ckvCh", long_options, NULL)) != -1) {
        /* option অনুযায়ী config mutate করি। */
        switch (opt) {
            case 'o':
                /* explicit output file override। */
                cfg.output_file = optarg;
                break;
            case 'O':
                /* optimization level text -> int parse। */
                cfg.opt_level = atoi(optarg);
                /* supported range guard (0..2)। */
                if (cfg.opt_level < 0 || cfg.opt_level > 2) {
                    fprintf(stderr, "Invalid optimization level (use 0, 1, or 2)\n");
                    return 1;
                }
                break;
            case 'c':
                /* generated C-কে binary-তেও compile করো। */
                cfg.compile_c = 1;
                break;
            case 'k':
                /* compile শেষে .c intermediate রেখে দাও। */
                cfg.keep_c = 1;
                break;
            case 'v':
                /* verbose pipeline/status logs অন। */
                cfg.verbose = 1;
                break;
            case 'C':
                /* generated C-তে TAC comments include করা। */
                cfg.emit_comments = 1;
                break;
            case 'h':
                /* help দেখিয়ে success return। */
                print_usage(argv[0]);
                return 0;
            default:
                /* getopt parse error হলে generic failure return। */
                return 1;
        }
    }

    /* Remaining arg is the input file */
    /* option parse শেষে input file না থাকলে hard error। */
    if (optind >= argc) {
        fprintf(stderr, "Error: no input file specified\n");
        return 1;
    }
    /* remaining positional arg-টিকে input .nl file হিসেবে নেই। */
    cfg.input_file = argv[optind];

    /* Verify input file exists */
    /* stat structure file existence/meta check-এর জন্য। */
    struct stat st;
    /* input path resolve না হলে compile শুরুই করব না। */
    if (stat(cfg.input_file, &st) != 0) {
        fprintf(stderr, "Error: cannot find '%s'\n", cfg.input_file);
        return 1;
    }

    /* ---- Pipeline begins ---- */

    /* Stage 1: Parse */
    /* parser stage চালিয়ে AST পাই। */
    ASTNode *ast = stage_parse(cfg.input_file, cfg.verbose);
    /* parse fail হলে non-zero exit। */
    if (!ast) return 1;

    /* If check-only, we're done */
    /* check mode: parse success summary দেখিয়ে clean exit। */
    if (cfg.check_only) {
        /* top-level statement count defensive ভাবে বের করি। */
        size_t n = (ast->type == AST_PROGRAM && ast->data.program.statements)
                   ? ast->data.program.statements->count : 0;
        fprintf(stderr, "OK: %s parsed successfully (%zu statements)\n",
                cfg.input_file, n);
        /* AST memory release করে return 0। */
        ast_free(ast);
        return 0;
    }

    /* Stage 2: IR */
    /* AST থেকে TAC/IR generate করি। */
    TACProgram *ir = stage_ir(ast, cfg.verbose);
    /* IR stage fail হলে AST free করে exit। */
    if (!ir) { ast_free(ast); return 1; }

    /* Stage 3: Optimize */
    /* নির্বাচিত level অনুযায়ী optimization pass চালাই। */
    if (!stage_optimize(ir, cfg.opt_level, cfg.verbose)) {
        /* optimize stage ব্যর্থ হলে দুই resource free করে exit। */
        ir_free(ir); ast_free(ast); return 1;
    }

    /* Stage 4: Codegen */
    /* IR থেকে generated C source string পাই। */
    char *c_code = stage_codegen(ir, cfg.emit_comments, cfg.verbose);
    /* codegen-এর পরে IR memory আর দরকার নেই। */
    ir_free(ir);
    /* AST-ও codegen শেষে release করি। */
    ast_free(ast);
    /* codegen fail হলে exit। */
    if (!c_code) return 1;

    /* Write .c file */
    /* output filename: explicit -o থাকলে সেটি, নাহলে auto derive। */
    char *c_file = cfg.output_file
                   ? strdup(cfg.output_file)
                   : derive_output(cfg.input_file, ".c");

    /* target .c file write mode-এ open। */
    FILE *out = fopen(c_file, "w");
    /* open fail হলে allocated buffer cleanup করে abort। */
    if (!out) {
        fprintf(stderr, "Error: cannot write '%s'\n", c_file);
        free(c_code); free(c_file);
        return 1;
    }
    /* generated C text file-এ লিখি। */
    fputs(c_code, out);
    /* write flush/close। */
    fclose(out);
    /* C source buffer free (file-এ persist হয়েছে)। */
    free(c_code);

    /* verbose হলে, অথবা শুধুই C generate mode হলে path report করি। */
    if (cfg.verbose || !cfg.compile_c) {
        fprintf(stderr, "Generated: %s\n", c_file);
    }

    /* Optionally compile to binary */
    /* compile বা run mode হলে gcc invocation দরকার। */
    if (cfg.compile_c || cfg.run_after) {
        /* binary output name derive (no extension append)। */
        char *bin_file = derive_output(cfg.input_file, "");
        /* Remove trailing dot if any */
        /* input name dot দিয়ে শেষ হলে trailing dot trim। */
        size_t bl = strlen(bin_file);
        if (bl > 0 && bin_file[bl - 1] == '.') bin_file[bl - 1] = '\0';

        /* gcc command string রাখার fixed buffer। */
        char cmd[4096];
        /* runtime support C file link করে native binary build command বানাই। */
        snprintf(cmd, sizeof(cmd),
                 "gcc -std=c11 -O2 -o %s %s -Iruntime runtime/naturelang_runtime.c -lm",
                 bin_file, c_file);

        /* verbose হলে full gcc command print। */
        if (cfg.verbose) fprintf(stderr, "Compiling: %s\n", cmd);

        /* shell দিয়ে gcc command run। */
        int rc = system(cmd);
        /* compile fail হলে status print + cleanup + exit। */
        if (rc != 0) {
            fprintf(stderr, "Error: gcc compilation failed (exit %d)\n", rc);
            free(c_file); free(bin_file);
            return 1;
        }

        /* verbose mode-এ binary path announce। */
        if (cfg.verbose) {
            fprintf(stderr, "Binary: %s\n", bin_file);
        }

        /* Remove .c file if not keeping */
        /* শুধু compile mode-এ (run নয়) keep_c false হলে .c delete করি। */
        if (!cfg.keep_c && !cfg.run_after) {
            unlink(c_file);
        }

        /* Run if requested */
        /* run command হলে freshly built binary execute করি। */
        if (cfg.run_after) {
            /* "./binary" run command তৈরি। */
            char run_cmd[4096];
            snprintf(run_cmd, sizeof(run_cmd), "./%s", bin_file);
            /* verbose mode-এ run command দেখাই। */
            if (cfg.verbose) fprintf(stderr, "Running: %s\n\n", run_cmd);
            /* program run করে exit status সংগ্রহ। */
            rc = system(run_cmd);
            /* Clean up */
            /* run শেষে binary remove করি। */
            unlink(bin_file);
            /* keep_c false হলে generated .c-ও remove করি। */
            if (!cfg.keep_c) unlink(c_file);
            /* filename buffers free করি। */
            free(c_file); free(bin_file);
            /* child program-এর exit status propagate করি। */
            return WEXITSTATUS(rc);
        }

        /* compile-only success summary print। */
        fprintf(stderr, "Compiled: %s → %s\n", cfg.input_file, bin_file);
        /* binary filename buffer release। */
        free(bin_file);
    }

    /* normal completion path-এ c_file string free করে exit 0। */
    free(c_file);
    return 0;
}
