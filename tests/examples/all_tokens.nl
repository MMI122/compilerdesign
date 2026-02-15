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
create a number called alpha and set it to 10
create a number called beta and set it to 3
create a number called result

result becomes alpha plus beta
result becomes alpha minus beta
result becomes alpha multiplied by beta
result becomes alpha divided by beta
result becomes alpha modulo beta
result becomes alpha power 2

-- === OPERATORS - Symbolic ===
result becomes alpha + beta
result becomes alpha - beta
result becomes alpha * beta
result becomes alpha / beta
result becomes alpha % beta
result becomes alpha ^ 2

-- === COMPARISONS - Natural ===
create a flag called cmp
cmp becomes alpha is greater than beta
cmp becomes alpha is less than beta
cmp becomes alpha is equal to beta
cmp becomes alpha is not equal to beta
cmp becomes alpha is at least beta
cmp becomes alpha is at most beta

-- === COMPARISONS - Symbolic ===
cmp becomes alpha > beta
cmp becomes alpha < beta
cmp becomes alpha == beta
cmp becomes alpha != beta
cmp becomes alpha >= beta
cmp becomes alpha <= beta
cmp becomes alpha <> beta

-- === LOGICAL ===
create a flag called flagX and set it to true
create a flag called flagY and set it to false
create a flag called flagZ

flagZ becomes flagX and flagY
flagZ becomes flagX or flagY
flagZ becomes not flagX
flagZ becomes flagX && flagY
flagZ becomes flagX || flagY
flagZ becomes !flagX

-- === CONTROL FLOW ===
if flagX then
    display "x is true"
otherwise
    display "x is false"
end if

if alpha greater than 5 then
    display "big"
otherwise
    display "small"
end if

repeat 3 times
    display "loop"
end repeat

while alpha is less than 20 do
    alpha becomes alpha plus 1
end while

for each element in listVar do
    display element
end for

-- === FUNCTIONS ===
define a function testFunc that takes param1, param2 and returns number
    create a number called funcResult
    funcResult becomes param1 plus param2
    give back funcResult
end function

create a number called callResult
callResult becomes call testFunc with 5, 10

-- === SECURE ZONES ===
begin secure zone
    create a number called safeVar and set it to 100
end secure zone

safely do
    create a number called anotherSafe and set it to 200
end safely

-- === PUNCTUATION ===
create a list called myList and set it to [1, 2, 3]
create a number called indexed and set it to myList[0]

-- === I/O ===
display "Testing output"
show "Alternative output"
print "Another alternative"

create a number called userInput
ask "Enter a number: " and store in userInput
read userInput

-- === SPECIAL KEYWORDS ===
stop
skip

-- === END OF TEST FILE ===
display "All tokens tested successfully!"
