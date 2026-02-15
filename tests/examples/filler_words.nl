-- ============================================================================
-- NatureLang Filler Words Test File
-- This file tests that all filler words are correctly ignored by the lexer
-- Each line should parse successfully with the filler words stripped
-- ============================================================================

-- ============================================================================
-- TEST 1: "I want to" filler
-- ============================================================================
I want to create a number called testVar1

-- ============================================================================
-- TEST 2: "I want" filler (without "to")
-- ============================================================================
I want display "Hello from I want"

-- ============================================================================
-- TEST 3: "want to" filler
-- ============================================================================
want to create a number called testVar2

-- ============================================================================
-- TEST 4: "want" filler (standalone)
-- ============================================================================
want display "Hello from want"

-- ============================================================================
-- TEST 5: "please" filler
-- ============================================================================
please create a number called testVar3
please display "Hello from please"

-- ============================================================================
-- TEST 6: "can you" filler
-- ============================================================================
can you create a number called testVar4
can you display "Hello from can you"

-- ============================================================================
-- TEST 7: "can" filler (standalone)
-- ============================================================================
can create a number called testVar5
can display "Hello from can"

-- ============================================================================
-- TEST 8: "could you" filler
-- ============================================================================
could you create a number called testVar6
could you display "Hello from could you"

-- ============================================================================
-- TEST 9: "could" filler (standalone)
-- ============================================================================
could create a number called testVar7
could display "Hello from could"

-- ============================================================================
-- TEST 10: "would you" filler
-- ============================================================================
would you create a number called testVar8
would you display "Hello from would you"

-- ============================================================================
-- TEST 11: "would" filler (standalone)
-- ============================================================================
would create a number called testVar9
would display "Hello from would"

-- ============================================================================
-- TEST 12: "let me" filler
-- ============================================================================
let me create a number called testVar10
let me display "Hello from let me"

-- ============================================================================
-- TEST 13: "let us" filler
-- ============================================================================
let us create a number called testVar11
let us display "Hello from let us"

-- ============================================================================
-- TEST 14: "let's" filler
-- ============================================================================
let's create a number called testVar12
let's display "Hello from let's"

-- ============================================================================
-- TEST 15: "let" filler (standalone)
-- ============================================================================
let create a number called testVar13
let display "Hello from let"

-- ============================================================================
-- TEST 16: "now" filler
-- ============================================================================
now create a number called testVar14
now display "Hello from now"

-- ============================================================================
-- TEST 17: "just" filler
-- ============================================================================
just create a number called testVar15
just display "Hello from just"

-- ============================================================================
-- TEST 18: "simply" filler
-- ============================================================================
simply create a number called testVar16
simply display "Hello from simply"

-- ============================================================================
-- TEST 19: "go ahead and" filler
-- ============================================================================
go ahead and create a number called testVar17
go ahead and display "Hello from go ahead and"

-- ============================================================================
-- TEST 20: "proceed to" filler
-- ============================================================================
proceed to create a number called testVar18
proceed to display "Hello from proceed to"

-- ============================================================================
-- TEST 21: Combined fillers (multiple in sequence)
-- ============================================================================
please just create a number called testVar19
now simply display "Hello from combined fillers"

-- ============================================================================
-- TEST 22: Fillers with control flow
-- ============================================================================
create a number called conditionVar and set it to 10

please if conditionVar is greater than 5 then
    just display "Condition met!"
end

-- ============================================================================
-- TEST 23: Fillers with loops
-- ============================================================================
create a number called loopVar and set it to 0

now repeat 3 times
    loopVar becomes loopVar plus 1
    simply display loopVar
end

-- ============================================================================
-- FINAL SUCCESS MESSAGE
-- ============================================================================
display "All filler word tests passed successfully!"
