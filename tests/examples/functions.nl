-- NatureLang Example: Functions
-- Demonstrates function definition and calling

-- Define a simple function
define a function greet that takes name and returns nothing
    display "Hello, "
    display name
    display "!"
end

-- Define a function with return value
define a function add that takes a, b and returns number
    create a number called sum
    sum becomes a plus b
    give back sum
end

-- Define a function to calculate factorial
define a function factorial that takes n and returns number
    if n is less than 2 then
        give back 1
    otherwise
        create a number called result
        result becomes n multiplied by factorial(n minus 1)
        give back result
    end
end

-- Define a function to check if number is even
define a function isEven that takes num and returns flag
    create a number called remainder
    remainder becomes num modulo 2
    if remainder equals 0 then
        give back true
    otherwise
        give back false
    end
end

-- Main program
display "=== Function Examples ==="

-- Call greet function
call greet with "World"

-- Call add function
create a number called sum_result
sum_result becomes add(5, 3)
display "5 + 3 = "
display sum_result

-- Call factorial
create a number called fact_result
fact_result becomes factorial(5)
display "5! = "
display fact_result

-- Call isEven
create a number called test_num and set it to 4
if isEven(test_num) then
    display "4 is even"
otherwise
    display "4 is odd"
end

display "=== Done ==="
