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

# Parser sources (for future)
PARSER_BISON = $(PARSER_DIR)/naturelang.y
PARSER_GEN_C = $(BUILD_DIR)/naturelang.tab.c
PARSER_GEN_H = $(BUILD_DIR)/naturelang.tab.h

# ============================================================================
# OUTPUT BINARIES
# ============================================================================

LEXER_TEST = $(BUILD_DIR)/lexer_test
COMPILER = $(BUILD_DIR)/naturec

# ============================================================================
# DEFAULT TARGET
# ============================================================================

.PHONY: all clean lexer parser test help dirs

all: dirs lexer

help:
	@echo "NatureLang Compiler Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build everything (default)"
	@echo "  lexer      - Build lexer test program"
	@echo "  parser     - Build parser (future)"
	@echo "  test       - Run all tests"
	@echo "  test-lexer - Run lexer tests"
	@echo "  clean      - Remove build artifacts"
	@echo "  help       - Show this message"
	@echo ""
	@echo "Options:"
	@echo "  BUILD_TYPE=debug    - Debug build with sanitizers (default)"
	@echo "  BUILD_TYPE=release  - Optimized release build"
	@echo ""
	@echo "Examples:"
	@echo "  make                      - Build lexer (debug)"
	@echo "  make BUILD_TYPE=release   - Build lexer (release)"
	@echo "  make test-lexer           - Run lexer tests"

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
# TESTING
# ============================================================================

.PHONY: test test-lexer test-examples

test: test-lexer

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

test-interactive: lexer
	@echo "Starting interactive lexer mode..."
	$(LEXER_TEST) -i

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
