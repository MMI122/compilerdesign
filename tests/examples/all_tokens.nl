-- NatureLang Example: All Token Types
-- This file tests all token types for lexer validation

{- 
   Block comment test
   This spans multiple lines
   And should be handled correctly
-}

-- === TYPES ===
create a number called intVar and set it to 42
create a decimal called floatVar and set it to 3.14159
create a text called stringVar and set it to "Hello, World!"
create a flag called boolVar and set it to true
create a list called listVar

-- === NUMERIC LITERALS ===
create a number called dec and set it to 255
create a decimal called scientific and set it to 1.5e10
create a decimal called smallNum and set it to 0.001

-- === STRING ESCAPES ===
create a text called escaped and set it to "Line1\nLine2\tTabbed"
create a text called quoted and set it to "She said \"Hello!\""

-- === OPERATORS - Natural Words ===
create a number called a and set it to 10
create a number called b and set it to 3
create a number called r

r becomes a plus b
r becomes a minus b
r becomes a multiplied by b
r becomes a divided by b
r becomes a modulo b
r becomes a power 2

-- === OPERATORS - Symbolic ===
r becomes a + b
r becomes a - b
r becomes a * b
r becomes a / b
r becomes a % b
r becomes a ^ 2

-- === COMPARISONS - Natural ===
create a flag called cmp
cmp becomes a is greater than b
cmp becomes a is less than b
cmp becomes a is equal to b
cmp becomes a is not equal to b
cmp becomes a is at least b
cmp becomes a is at most b

-- === COMPARISONS - Symbolic ===
cmp becomes a > b
cmp becomes a < b
cmp becomes a == b
cmp becomes a != b
cmp becomes a >= b
cmp becomes a <= b
cmp becomes a <> b

-- === LOGICAL ===
create a flag called x and set it to true
create a flag called y and set it to false
create a flag called z

z becomes x and y
z becomes x or y
z becomes not x
z becomes x && y
z becomes x || y
z becomes !x

-- === CONTROL FLOW ===
if x then
    display "x is true"
otherwise
    display "x is false"
end

if a greater than 5 then
    display "big"
else
    display "small"
end

repeat 3 times
    display "loop"
end

while a is less than 20 do
    a becomes a plus 1
end

for each item in listVar do
    display item
end

-- === FUNCTIONS ===
define a function testFunc that takes param1, param2 and returns number
    create a number called result
    result becomes param1 plus param2
    give back result
end

create a number called funcResult
funcResult becomes call testFunc with 5, 10

-- === SECURE ZONES ===
enter secure zone
    create a number called safeVar and set it to 100
end

enter safe zone
    create a number called anotherSafe and set it to 200
end

-- === PUNCTUATION ===
create a list called myList and set it to [1, 2, 3]
create a number called indexed and set it to myList[0]

-- === I/O ===
display "Testing output"
show "Alternative output"
print "Another alternative"

create a number called input
ask "Enter a number: " and remember input
read input
save input into "file.txt"

-- === SPECIAL KEYWORDS ===
stop
skip

-- === END OF TEST FILE ===
display "All tokens tested successfully!"
