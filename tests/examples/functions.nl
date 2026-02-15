-- NatureLang Example: Functions
-- Demonstrates function definition and calling

-- Define a simple function (returns nothing)
define a function greet that takes name
    display "Hello, "
    display name
    display "!"
end function

-- Define a function with return value
define a function sum that takes x and returns number
    create a number called result
    result becomes x
    give back result
end function

-- Define a function to calculate factorial
define a function factorial that takes n and returns number
    if n is less than 2 then
        give back 1
    otherwise
        create a number called result
        result becomes n
        give back result
    end if
end function

-- Define a function to check if number is even
define a function isEven that takes num and returns number
    create a number called rest
    rest becomes num modulo 2
    if rest equals 0 then
        give back 1
    otherwise
        give back 0
    end if
end function

-- Main program
display "=== Function Examples ==="

-- Call greet function
call greet with "World"

-- Call sum function
create a number called sum_result
sum_result becomes sum(5)
display "sum(5) = "
display sum_result

-- Call factorial
create a number called fact_result
fact_result becomes factorial(5)
display "5! = "
display fact_result

-- Call isEven
create a number called test_num and set it to 4
if isEven(test_num) equals 1 then
    display "4 is even"
otherwise
    display "4 is odd"
end if

display "=== Done ==="
