#!/bin/bash
# NatureLang Integration Test Suite
# Tests the full pipeline: .nl → parse → IR → optimize → codegen → compile → run
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
NATUREC="$ROOT_DIR/build/naturec"
EXAMPLES="$ROOT_DIR/tests/examples"
OUT_DIR="$ROOT_DIR/build/integration"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

passed=0
failed=0
skipped=0

inc_passed()  { passed=$((passed + 1)); }
inc_failed()  { failed=$((failed + 1)); }
inc_skipped() { skipped=$((skipped + 1)); }

mkdir -p "$OUT_DIR"

echo ""
echo "=== NatureLang Integration Tests ==="
echo ""

# Check that naturec exists
if [ ! -x "$NATUREC" ]; then
    echo -e "${RED}Error: naturec not found at $NATUREC${NC}"
    echo "Run 'make all' first."
    exit 1
fi

# Test function: compile and optionally run a .nl file
run_test() {
    local nl_file="$1"
    local expected_output="$2"
    local needs_input="$3"
    local base=$(basename "$nl_file" .nl)

    printf "  %-25s " "$base.nl"

    # Generate C code
    local c_file="$OUT_DIR/$base.c"
    if ! ASAN_OPTIONS=detect_leaks=0 "$NATUREC" build -O1 -o "$c_file" "$nl_file" 2>/dev/null; then
        echo -e "${RED}FAIL (codegen)${NC}"
        inc_failed
        return
    fi

    # Compile with gcc
    local bin_file="$OUT_DIR/$base"
    if ! gcc -std=c11 -O2 -o "$bin_file" "$c_file" \
         -I"$ROOT_DIR/runtime" "$ROOT_DIR/runtime/naturelang_runtime.c" -lm 2>/dev/null; then
        echo -e "${RED}FAIL (gcc compile)${NC}"
        inc_failed
        return
    fi

    # If no expected output and needs input, just verify it compiled
    if [ -n "$needs_input" ]; then
        echo -e "${YELLOW}PASS (compile-only, needs input)${NC}"
        inc_skipped
        return
    fi

    # Run and check output
    if [ -n "$expected_output" ]; then
        local actual
        actual=$(timeout 5 "$bin_file" 2>&1 || true)
        # Compare first line of output
        local first_line=$(echo "$actual" | head -1)
        if [ "$first_line" = "$expected_output" ]; then
            echo -e "${GREEN}PASS${NC} → $first_line"
            inc_passed
        else
            echo -e "${RED}FAIL${NC} (expected: '$expected_output', got: '$first_line')"
            inc_failed
        fi
    else
        # Just verify it runs without crashing
        if timeout 5 "$bin_file" >/dev/null 2>&1; then
            echo -e "${GREEN}PASS (runs ok)${NC}"
            inc_passed
        else
            echo -e "${RED}FAIL (crash)${NC}"
            inc_failed
        fi
    fi
}

# ---- Tests ----

# hello.nl: should print "Hello, World!"
run_test "$EXAMPLES/hello.nl" "Hello, World!"

# arithmetic.nl: first output should be 35 (10+25)
run_test "$EXAMPLES/arithmetic.nl" "35"

# control_flow.nl: first meaningful output - compiles and runs
run_test "$EXAMPLES/control_flow.nl" ""

# functions.nl: first output should be "=== Function Examples ==="
run_test "$EXAMPLES/functions.nl" "=== Function Examples ==="

# between_operator.nl: compiles and runs
run_test "$EXAMPLES/between_operator.nl" ""

# filler_words.nl: compiles and runs  
run_test "$EXAMPLES/filler_words.nl" ""

# synonyms.nl: compiles and runs
run_test "$EXAMPLES/synonyms.nl" ""

# natural_writing.nl: needs user input (asks for name)
run_test "$EXAMPLES/natural_writing.nl" "" "needs_input"

# secure_zone.nl: compiles and runs
run_test "$EXAMPLES/secure_zone.nl" ""

# input_output.nl: needs user input, compile-only test
run_test "$EXAMPLES/input_output.nl" "" "needs_input"

# all_tokens.nl: needs user input (asks for input)
run_test "$EXAMPLES/all_tokens.nl" "" "needs_input"

# ---- Summary ----
echo ""
echo "=== Summary ==="
echo -e "  Passed:  ${GREEN}$passed${NC}"
echo -e "  Failed:  ${RED}$failed${NC}"
echo -e "  Skipped: ${YELLOW}$skipped${NC}"
echo ""

if [ "$failed" -gt 0 ]; then
    echo -e "${RED}Some tests failed!${NC}"
    exit 1
fi

echo -e "${GREEN}✓ All integration tests passed!${NC}"
