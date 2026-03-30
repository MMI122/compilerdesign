-- NatureLang Comprehensive Feature Demo
-- Covers: types, assignment, arithmetic, conditions, between, loops,
-- functions and output.

display "=== NatureLang Full Feature Demo ==="

-- 1) Types and declarations
create a number called x and set it to 10
create a number called y and set it to 25
create a decimal called ratio and set it to 2.5
create a text called label and set it to "demo"
create a flag called ok and set it to true

display "Section 1: declarations done"

-- 2) Arithmetic (natural + symbolic)
create a number called sum
sum becomes x plus y

display "sum ="
display sum

create a number called product
product becomes x * y

display "product ="
display product

create a decimal called division
division becomes y / x

display "division ="
display division

create a number called complexResult
complexResult becomes x plus y multiplied by 2

display "complex ="
display complexResult

-- 3) Comparisons and logical values
if x is less than y then
    display "x < y is true"
else
    display "x < y is false"
end

if ok then
    display "ok flag is true"
else
    display "ok flag is false"
end

-- 4) Unique between operator
create a number called score and set it to 85

if score is between 80 and 89 then
    display "Grade band: B"
end

-- 5) Repeat loop
create a number called counter and set it to 0
repeat 3 times
    counter becomes counter plus 1
    display "repeat counter:"
    display counter
end

-- 6) While loop
create a number called i and set it to 0
while i is less than 2 do
    display "while iteration"
    i becomes i plus 1
end

-- 7) Functions

define a function greet that takes who
    display "Hello,"
    display who
end function

define a function passthrough that takes n and returns number
    create a number called out
    out becomes n
    give back out
end function

call greet with 2026

create a number called fnResult
fnResult becomes passthrough(42)

display "function result ="
display fnResult

display "=== Demo Complete ==="
