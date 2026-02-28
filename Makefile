# NatureLang Compiler - Makefile
# Copyright (c) 2026
#
# Build system for NatureLang compiler and tools

# ============================================================================
# COMPILER AND TOOL SETTINGS
# ============================================================================

CC = gcc
FLEX = flex
BISON = bison

# Compiler flags
CFLAGS = -Wall -Wextra -std=c11 -g -I$(INCLUDE_DIR)
LDFLAGS = -lfl

# Debug/Release configurations
DEBUG_FLAGS = -O0 -DDEBUG -fsanitize=address -fsanitize=undefined
RELEASE_FLAGS = -O2 -DNDEBUG

# Default to debug build
BUILD_TYPE ?= debug
ifeq ($(BUILD_TYPE),release)
    CFLAGS += $(RELEASE_FLAGS)
else
    CFLAGS += $(DEBUG_FLAGS)
    LDFLAGS += -fsanitize=address -fsanitize=undefined
endif

# ============================================================================
# DIRECTORIES
# ============================================================================

SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
LEXER_DIR = $(SRC_DIR)/lexer
PARSER_DIR = $(SRC_DIR)/parser
AST_DIR = $(SRC_DIR)/ast
SEMANTIC_DIR = $(SRC_DIR)/semantic
IR_DIR = $(SRC_DIR)/ir
CODEGEN_DIR = $(SRC_DIR)/codegen
TESTS_DIR = tests
EXAMPLES_DIR = $(TESTS_DIR)/examples

# ============================================================================
# SOURCE FILES
# ============================================================================

# Lexer sources
LEXER_FLEX = $(LEXER_DIR)/naturelang.l
LEXER_GEN = $(BUILD_DIR)/lex.yy.c
LEXER_SRCS = $(LEXER_DIR)/tokens.c $(LEXER_DIR)/lexer_utils.c
LEXER_MAIN = $(LEXER_DIR)/lexer_main.c
LEXER_OBJS = $(BUILD_DIR)/lex.yy.o $(BUILD_DIR)/tokens.o $(BUILD_DIR)/lexer_utils.o

# Parser sources - UNIFIED LEXER ARCHITECTURE
# The parser uses the same naturelang.l lexer as standalone mode,
# compiled with USE_BISON_TOKENS to use Bison-generated token values
PARSER_BISON = $(PARSER_DIR)/naturelang.y
PARSER_GEN_C = $(BUILD_DIR)/naturelang.tab.c
PARSER_GEN_H = $(BUILD_DIR)/naturelang.tab.h
PARSER_LEXER_GEN = $(BUILD_DIR)/parser_lex.yy.c
PARSER_MAIN = $(PARSER_DIR)/parser_main.c
# Note: parser uses parser_tokens.o (built with USE_BISON_TOKENS) instead of tokens.o
PARSER_OBJS = $(BUILD_DIR)/naturelang.tab.o $(BUILD_DIR)/parser_lex.yy.o \
              $(BUILD_DIR)/parser_tokens.o $(BUILD_DIR)/ast.o

# AST sources
AST_SRC = $(AST_DIR)/ast.c
AST_HDR = $(INCLUDE_DIR)/ast.h

# Semantic analysis sources
SEMANTIC_SRCS = $(SEMANTIC_DIR)/symbol_table.c $(SEMANTIC_DIR)/semantic.c
SEMANTIC_HDRS = $(INCLUDE_DIR)/symbol_table.h $(INCLUDE_DIR)/semantic.h
SEMANTIC_OBJS = $(BUILD_DIR)/symbol_table.o $(BUILD_DIR)/semantic.o

# Code generation sources
CODEGEN_SRCS = $(CODEGEN_DIR)/codegen.c
CODEGEN_HDRS = $(INCLUDE_DIR)/codegen.h
CODEGEN_OBJS = $(BUILD_DIR)/codegen.o

# IR sources
IR_SRCS = $(IR_DIR)/ir.c
IR_HDRS = $(INCLUDE_DIR)/ir.h
IR_OBJS = $(BUILD_DIR)/ir.o

# Runtime library sources
RUNTIME_DIR = runtime
RUNTIME_SRCS = $(RUNTIME_DIR)/naturelang_runtime.c
RUNTIME_HDRS = $(RUNTIME_DIR)/naturelang_runtime.h
RUNTIME_OBJS = $(BUILD_DIR)/naturelang_runtime.o

# ============================================================================
# OUTPUT BINARIES
# ============================================================================

LEXER_TEST = $(BUILD_DIR)/lexer_test
PARSER_TEST = $(BUILD_DIR)/parser_test
COMPILER = $(BUILD_DIR)/naturec

# ============================================================================
# DEFAULT TARGET
# ============================================================================

.PHONY: all clean lexer parser test help dirs

all: dirs lexer parser

help:
	@echo "NatureLang Compiler Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build everything (default)"
	@echo "  lexer      - Build lexer test program"
	@echo "  parser     - Build parser test program"
	@echo "  test       - Run all tests"
	@echo "  test-lexer - Run lexer tests"
	@echo "  test-parser- Run parser tests"
	@echo "  test-ir    - Run IR generation tests"
	@echo "  clean      - Remove build artifacts"
	@echo "  help       - Show this message"
	@echo ""
	@echo "Options:"
	@echo "  BUILD_TYPE=debug    - Debug build with sanitizers (default)"
	@echo "  BUILD_TYPE=release  - Optimized release build"
	@echo ""
	@echo "Examples:"
	@echo "  make                      - Build all (debug)"
	@echo "  make BUILD_TYPE=release   - Build all (release)"
	@echo "  make lexer                - Build only lexer"
	@echo "  make parser               - Build only parser"
	@echo "  make test-lexer           - Run lexer tests"
	@echo "  make test-parser          - Run parser tests"

# ============================================================================
# DIRECTORY CREATION
# ============================================================================

dirs:
	@mkdir -p $(BUILD_DIR)

# ============================================================================
# LEXER BUILD
# ============================================================================

lexer: dirs $(LEXER_TEST)
	@echo "✓ Lexer built successfully: $(LEXER_TEST)"

# Generate lexer C code from Flex specification
$(LEXER_GEN): $(LEXER_FLEX)
	@echo "Generating lexer from $(LEXER_FLEX)..."
	$(FLEX) -o $@ $<

# Compile generated lexer
$(BUILD_DIR)/lex.yy.o: $(LEXER_GEN)
	@echo "Compiling generated lexer..."
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-sign-compare -c $< -o $@

# Compile token implementation
$(BUILD_DIR)/tokens.o: $(LEXER_DIR)/tokens.c $(INCLUDE_DIR)/tokens.h
	@echo "Compiling tokens.c..."
	$(CC) $(CFLAGS) -c $< -o $@

# Compile lexer utilities
$(BUILD_DIR)/lexer_utils.o: $(LEXER_DIR)/lexer_utils.c $(INCLUDE_DIR)/lexer.h $(INCLUDE_DIR)/tokens.h
	@echo "Compiling lexer_utils.c..."
	$(CC) $(CFLAGS) -c $< -o $@

# Compile lexer main
$(BUILD_DIR)/lexer_main.o: $(LEXER_MAIN) $(INCLUDE_DIR)/lexer.h $(INCLUDE_DIR)/tokens.h
	@echo "Compiling lexer_main.c..."
	$(CC) $(CFLAGS) -c $< -o $@

# Link lexer test program
$(LEXER_TEST): $(LEXER_OBJS) $(BUILD_DIR)/lexer_main.o
	@echo "Linking lexer test program..."
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# ============================================================================
# PARSER BUILD
# ============================================================================

parser: dirs $(PARSER_TEST)
	@echo "✓ Parser built successfully: $(PARSER_TEST)"

# Generate parser C code from Bison specification
$(PARSER_GEN_C) $(PARSER_GEN_H): $(PARSER_BISON) $(INCLUDE_DIR)/ast.h $(INCLUDE_DIR)/tokens.h
	@echo "Generating parser from $(PARSER_BISON)..."
	$(BISON) -d -o $(PARSER_GEN_C) $<

# Generate parser lexer from the SAME Flex specification as standalone lexer
# This ensures ONE UNIFIED LEXER for the entire project
$(PARSER_LEXER_GEN): $(LEXER_FLEX) $(PARSER_GEN_H)
	@echo "Generating parser lexer from $(LEXER_FLEX) (unified lexer)..."
	$(FLEX) -o $@ $<

# Compile generated parser
$(BUILD_DIR)/naturelang.tab.o: $(PARSER_GEN_C) $(PARSER_GEN_H) $(INCLUDE_DIR)/ast.h $(INCLUDE_DIR)/tokens.h
	@echo "Compiling generated parser..."
	$(CC) $(CFLAGS) -I$(BUILD_DIR) -Wno-unused-function -c $(PARSER_GEN_C) -o $@

# Compile parser lexer with USE_BISON_TOKENS to use Bison token definitions
$(BUILD_DIR)/parser_lex.yy.o: $(PARSER_LEXER_GEN) $(PARSER_GEN_H)
	@echo "Compiling parser lexer (with Bison tokens)..."
	$(CC) $(CFLAGS) -DUSE_BISON_TOKENS -I$(BUILD_DIR) -Wno-unused-function -Wno-sign-compare -c $< -o $@

# Compile tokens.c for parser with USE_BISON_TOKENS
$(BUILD_DIR)/parser_tokens.o: $(LEXER_DIR)/tokens.c $(PARSER_GEN_H) $(INCLUDE_DIR)/tokens.h
	@echo "Compiling tokens.c for parser (with Bison tokens)..."
	$(CC) $(CFLAGS) -DUSE_BISON_TOKENS -I$(BUILD_DIR) -c $< -o $@

# Compile AST implementation
$(BUILD_DIR)/ast.o: $(AST_SRC) $(AST_HDR)
	@echo "Compiling ast.c..."
	$(CC) $(CFLAGS) -c $(AST_SRC) -o $@

# ============================================================================
# IR BUILD
# ============================================================================

# Compile IR implementation
$(BUILD_DIR)/ir.o: $(IR_DIR)/ir.c $(INCLUDE_DIR)/ir.h $(INCLUDE_DIR)/ast.h
	@echo "Compiling ir.c..."
	$(CC) $(CFLAGS) -c $< -o $@

# Build IR (for other targets to depend on)
ir: dirs $(IR_OBJS)
	@echo "✓ IR module built successfully"

# ============================================================================
# SEMANTIC ANALYSIS BUILD
# ============================================================================

# Compile symbol table
$(BUILD_DIR)/symbol_table.o: $(SEMANTIC_DIR)/symbol_table.c $(INCLUDE_DIR)/symbol_table.h $(INCLUDE_DIR)/ast.h
	@echo "Compiling symbol_table.c..."
	$(CC) $(CFLAGS) -c $< -o $@

# Compile semantic analyzer
$(BUILD_DIR)/semantic.o: $(SEMANTIC_DIR)/semantic.c $(INCLUDE_DIR)/semantic.h $(INCLUDE_DIR)/symbol_table.h $(INCLUDE_DIR)/ast.h
	@echo "Compiling semantic.c..."
	$(CC) $(CFLAGS) -c $< -o $@

# Build semantic analyzer components (for other targets to depend on)
semantic: dirs $(SEMANTIC_OBJS)
	@echo "✓ Semantic analyzer built successfully"

# ============================================================================
# CODE GENERATOR BUILD
# ============================================================================

# Compile code generator
$(BUILD_DIR)/codegen.o: $(CODEGEN_DIR)/codegen.c $(INCLUDE_DIR)/codegen.h $(INCLUDE_DIR)/ast.h $(INCLUDE_DIR)/symbol_table.h
	@echo "Compiling codegen.c..."
	$(CC) $(CFLAGS) -c $< -o $@

# Build code generator (for other targets to depend on)
codegen: dirs $(CODEGEN_OBJS)
	@echo "✓ Code generator built successfully"

# ============================================================================
# RUNTIME LIBRARY BUILD
# ============================================================================

# Compile runtime library
$(BUILD_DIR)/naturelang_runtime.o: $(RUNTIME_DIR)/naturelang_runtime.c $(RUNTIME_DIR)/naturelang_runtime.h
	@echo "Compiling naturelang_runtime.c..."
	$(CC) $(CFLAGS) -I$(RUNTIME_DIR) -c $< -o $@

# Build runtime library (for other targets to depend on)
runtime: dirs $(RUNTIME_OBJS)
	@echo "✓ Runtime library built successfully"

# Compile parser main
$(BUILD_DIR)/parser_main.o: $(PARSER_MAIN) $(INCLUDE_DIR)/parser.h $(INCLUDE_DIR)/ast.h $(INCLUDE_DIR)/ir.h
	@echo "Compiling parser_main.c..."
	$(CC) $(CFLAGS) -c $< -o $@

# Link parser test program (now includes IR for --ir mode)
$(PARSER_TEST): $(PARSER_OBJS) $(BUILD_DIR)/parser_main.o $(IR_OBJS)
	@echo "Linking parser test program..."
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# ============================================================================
# TESTING
# ============================================================================

.PHONY: test test-lexer test-parser test-examples

test: test-lexer test-parser test-ir

test-lexer: lexer
	@echo ""
	@echo "=== Running Lexer Tests ==="
	@echo ""
	@if [ -d "$(EXAMPLES_DIR)" ] && [ "$$(ls -A $(EXAMPLES_DIR)/*.nl 2>/dev/null)" ]; then \
		for f in $(EXAMPLES_DIR)/*.nl; do \
			echo "Testing: $$f"; \
			$(LEXER_TEST) -l "$$f" || exit 1; \
			echo ""; \
		done; \
		echo "✓ All lexer tests passed!"; \
	else \
		echo "No test files found in $(EXAMPLES_DIR)/"; \
		echo "Running lexer with sample input..."; \
		echo 'create a number called x and set it to 42' | $(LEXER_TEST); \
	fi

test-parser: parser
	@echo ""
	@echo "=== Running Parser Tests ==="
	@echo ""
	@if [ -d "$(EXAMPLES_DIR)" ] && [ "$$(ls -A $(EXAMPLES_DIR)/*.nl 2>/dev/null)" ]; then \
		for f in $(EXAMPLES_DIR)/*.nl; do \
			echo "Testing: $$f"; \
			$(PARSER_TEST) -t "$$f" || exit 1; \
			echo ""; \
		done; \
		echo "✓ All parser tests passed!"; \
	else \
		echo "No test files found in $(EXAMPLES_DIR)/"; \
		echo "Running parser with sample input..."; \
		echo 'display 42' | $(PARSER_TEST) -t; \
	fi

test-interactive: lexer
	@echo "Starting interactive lexer mode..."
	$(LEXER_TEST) -i

test-ir: parser
	@echo ""
	@echo "=== Running IR Generation Tests ==="
	@echo ""
	@if [ -d "$(EXAMPLES_DIR)" ] && [ "$$(ls -A $(EXAMPLES_DIR)/*.nl 2>/dev/null)" ]; then \
		for f in $(EXAMPLES_DIR)/*.nl; do \
			echo "--- IR for: $$f ---"; \
			$(PARSER_TEST) -r "$$f" || exit 1; \
			echo ""; \
		done; \
		echo "✓ All IR generation tests passed!"; \
	else \
		echo "No test files found in $(EXAMPLES_DIR)/"; \
	fi

# ============================================================================
# CLEANING
# ============================================================================

clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	rm -f lex.yy.c *.tab.c *.tab.h
	@echo "✓ Clean complete"

# ============================================================================
# DEBUG TARGETS
# ============================================================================

.PHONY: debug-vars

debug-vars:
	@echo "CC = $(CC)"
	@echo "CFLAGS = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"
	@echo "BUILD_DIR = $(BUILD_DIR)"
	@echo "LEXER_OBJS = $(LEXER_OBJS)"
