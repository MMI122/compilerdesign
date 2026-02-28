/*
 * NatureLang Compiler
 * Copyright (c) 2026
 *
 * IR Optimization Header
 *
 * Defines optimization passes that transform TAC IR
 * to produce more efficient code. Classic textbook
 * optimizations for a compiler course.
 */

#ifndef NATURELANG_OPTIMIZER_H
#define NATURELANG_OPTIMIZER_H

#include "ir.h"
#include <stdbool.h>

/* ============================================================================
 * OPTIMIZATION LEVELS
 * ============================================================================
 */
typedef enum {
    OPT_LEVEL_0,    /* No optimization (identity) */
    OPT_LEVEL_1,    /* Basic: constant folding, dead code elimination */
    OPT_LEVEL_2     /* Full: + constant propagation, strength reduction,
                             algebraic simplification */
} OptLevel;

/* ============================================================================
 * OPTIMIZATION OPTIONS
 * ============================================================================
 */
typedef struct {
    OptLevel level;

    /* Individual pass toggles (all enabled at appropriate level) */
    bool constant_folding;       /* Evaluate constant expressions at compile time */
    bool constant_propagation;   /* Replace variables with known constant values */
    bool dead_code_elimination;  /* Remove instructions whose results are unused */
    bool algebraic_simplification; /* x+0 -> x, x*1 -> x, etc. */
    bool strength_reduction;     /* pow(x,2) -> x*x, x*2 -> x+x, etc. */
    bool redundant_load_elimination; /* Remove consecutive loads of same value */

    /* Reporting */
    bool verbose;                /* Print what each pass does */
} OptOptions;

/* ============================================================================
 * OPTIMIZATION STATISTICS
 * ============================================================================
 */
typedef struct {
    int constants_folded;
    int constants_propagated;
    int dead_instructions_removed;
    int algebraic_simplifications;
    int strength_reductions;
    int redundant_loads_removed;
    int total_instructions_before;
    int total_instructions_after;
    int passes_run;
} OptStats;

/* ============================================================================
 * PUBLIC API
 * ============================================================================
 */

/* Get default optimization options for a given level */
OptOptions opt_default_options(OptLevel level);

/* Run all enabled optimization passes on a TAC program.
 * Modifies the program in place.
 * Returns optimization statistics. */
OptStats ir_optimize(TACProgram *program, OptOptions *options);

/* ============================================================================
 * INDIVIDUAL OPTIMIZATION PASSES
 * (Can be called directly for fine-grained control)
 * ============================================================================
 */

/* Constant Folding: evaluate constant expressions at compile time
 *   t0 = 3 + 4   -->   t0 = 7 */
int opt_constant_folding(TACFunction *func, bool verbose);

/* Constant Propagation: replace variables with known constant values
 *   t0 = 5; t1 = t0 + 1   -->   t0 = 5; t1 = 5 + 1 */
int opt_constant_propagation(TACFunction *func, bool verbose);

/* Dead Code Elimination: remove instructions whose results are never used
 *   t0 = 3 + 4; (t0 never read) --> removed */
int opt_dead_code_elimination(TACFunction *func, bool verbose);

/* Algebraic Simplification: simplify known algebraic identities
 *   x + 0 -> x,  x * 1 -> x,  x * 0 -> 0,  x - 0 -> x */
int opt_algebraic_simplification(TACFunction *func, bool verbose);

/* Strength Reduction: replace expensive operations with cheaper ones
 *   x * 2 -> x + x,  x * 4 -> x << 2,  pow(x,2) -> x * x */
int opt_strength_reduction(TACFunction *func, bool verbose);

/* Redundant Load Elimination: remove duplicate loads of the same constant
 *   t0 = 5; t1 = 5;  -->  t0 = 5; t1 = t0 */
int opt_redundant_load_elimination(TACFunction *func, bool verbose);

/* ============================================================================
 * UTILITY
 * ============================================================================
 */

/* Remove instructions marked as dead (is_dead == true) from a function.
 * Returns number of instructions actually removed. */
int opt_sweep_dead(TACFunction *func);

/* Print optimization statistics */
void opt_print_stats(OptStats *stats);

#endif /* NATURELANG_OPTIMIZER_H */
