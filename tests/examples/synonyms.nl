-- ============================================================================
-- NatureLang Synonym Test File
-- This file demonstrates the various synonyms supported in NatureLang
-- ============================================================================

-- ============================================================================
-- 1. FILLER WORDS TEST
-- These sentences should all parse to the same thing
-- ============================================================================

-- Standard form
create a number called standardVar

-- With filler words (these are all equivalent)
-- Note: Uncomment one at a time to test individually
-- I want to create a number called fillerVar1
-- please create a number called fillerVar2
-- could you create a number called fillerVar3
-- let me create a number called fillerVar4
-- just create a number called fillerVar5


-- ============================================================================
-- 2. DECLARATION SYNONYMS TEST
-- ============================================================================

-- Using 'create' (canonical)
create a number called usingCreate

-- Using 'declare' (synonym)
declare a number called usingDeclare


-- ============================================================================
-- 3. TYPE SYNONYMS TEST
-- ============================================================================

-- Number type synonyms
create a number called numVar1
create an integer called numVar2
create an int called numVar3

-- Text type synonyms
create a text called textVar1
create a string called textVar2
create a word called textVar3

-- Decimal type synonyms
create a decimal called decVar1
create a float called decVar2

-- Boolean type synonyms
create a flag called flagVar1
create a boolean called flagVar2
create a bool called flagVar3

-- List type synonyms
create a list called listVar1
create an array called listVar2
create a collection called listVar3


-- ============================================================================
-- 4. CONTROL FLOW SYNONYMS TEST
-- ============================================================================

create a number called testValue and set it to 10

-- 'if' synonyms
if testValue is greater than 5 then
    display "Using if"
end

when testValue is greater than 5 then
    display "Using when"
end

whenever testValue is greater than 5 then
    display "Using whenever"
end

-- 'end' synonyms
if testValue is greater than 5 then
    display "End test"
finish

-- Loop synonym
create a number called loopCounter and set it to 0
loop 3 times
    loopCounter becomes loopCounter plus 1
done


-- ============================================================================
-- 5. COMPARISON SYNONYMS TEST
-- ============================================================================

create a number called compareVal and set it to 7

-- 'greater' synonyms
if compareVal is greater than 5 then
    display "greater than"
end

if compareVal is larger than 5 then
    display "larger than"
end

if compareVal is bigger than 5 then
    display "bigger than"
end

-- Less than with synonym
create a number called smallVal and set it to 3
if smallVal is smaller than 5 then
    display "smaller than"
end

if smallVal is fewer than 5 then
    display "fewer than"
end


-- ============================================================================
-- 6. I/O SYNONYMS TEST
-- ============================================================================

-- 'display' synonyms
display "Using display"
show "Using show"
print "Using print"
output "Using output"
say "Using say"


-- ============================================================================
-- 7. FUNCTION SYNONYMS TEST
-- ============================================================================

-- 'function' synonyms
define a function called myFunc:
    display "In function"
end function

define a method called myMethod:
    display "In method"
end function

define a procedure called myProc:
    display "In procedure"
end function

define a routine called myRoutine:
    display "In routine"
end function

-- 'that/which' synonym
define a function called funcWithWhich which takes x:
    display "Using which"
end function

-- 'takes' synonyms
define a function called funcAccepts that accepts x:
    display x
end function

define a function called funcReceives that receives y:
    display y
end function


-- ============================================================================
-- 8. LOGICAL VALUE SYNONYMS TEST
-- ============================================================================

-- Boolean value synonyms
create a flag called flag1 and set it to true
create a flag called flag2 and set it to yes
create a flag called flag3 and set it to correct

create a flag called flag4 and set it to false
create a flag called flag5 and set it to no
create a flag called flag6 and set it to wrong


-- ============================================================================
-- 9. ARITHMETIC SYNONYMS TEST
-- ============================================================================

create a number called num1 and set it to 10
create a number called num2 and set it to 3

-- 'plus' synonym
create a number called sum1 and set it to num1 plus num2
create a number called sum2 and set it to num1 added num2

-- 'minus' synonyms
create a number called diff1 and set it to num1 minus num2
create a number called diff2 and set it to num1 subtract num2
create a number called diff3 and set it to num1 subtracted num2

-- 'modulo' synonym
create a number called mod1 and set it to num1 modulo num2
create a number called mod2 and set it to num1 mod num2


-- ============================================================================
-- 10. CALL SYNONYMS TEST
-- ============================================================================

define a function called testFunc:
    display "Called!"
end function

call testFunc
invoke testFunc
execute testFunc
run testFunc


-- ============================================================================
-- FINAL MESSAGE
-- ============================================================================

display "All synonym tests completed successfully!"
