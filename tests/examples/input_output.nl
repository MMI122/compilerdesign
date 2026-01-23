-- NatureLang Example: Input/Output
-- Demonstrates reading input and displaying output

display "Welcome to NatureLang I/O Demo!"
display ""

-- Ask for user input
create a text called userName
ask "What is your name? " and remember userName
display "Hello, "
display userName
display "!"

-- Read a number
create a number called userAge
ask "How old are you? " and remember userAge

-- Conditional based on input
if userAge is greater than 17 then
    display "You are an adult."
else
    display "You are young!"
end

-- Calculate birth year (assuming current year is 2026)
create a number called birthYear
birthYear becomes 2026 minus userAge
display "You were probably born around "
display birthYear

-- Multiple inputs for calculation
display ""
display "=== Calculator ==="
create a number called num1
create a number called num2

ask "Enter first number: " and remember num1
ask "Enter second number: " and remember num2

-- Show all operations
display "Results:"

create a number called sum
sum becomes num1 plus num2
display "Sum: "
display sum

create a number called diff
diff becomes num1 minus num2
display "Difference: "
display diff

create a number called prod
prod becomes num1 multiplied by num2
display "Product: "
display prod

-- Check for division by zero
if num2 is not equal to 0 then
    create a decimal called quot
    quot becomes num1 divided by num2
    display "Quotient: "
    display quot
otherwise
    display "Cannot divide by zero!"
end

display ""
display "Thank you for using NatureLang!"
