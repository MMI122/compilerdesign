-- ============================================================================
-- NatureLang Natural Writing Test File
-- This file demonstrates how users can write code in a natural, 
-- conversational style and have it parse successfully.
-- ============================================================================

-- ============================================================================
-- SCENARIO 1: A Friendly Greeting Program
-- Written as if speaking to a friend
-- ============================================================================

I want to create a text called userName
please display "Hello! What's your name?"
ask "Enter your name: " and store in userName
now display "Nice to meet you, "
display userName

-- ============================================================================
-- SCENARIO 2: Simple Calculator
-- Written in a casual, natural way
-- ============================================================================

let me create an integer called firstNumber and set it to 25
could you create an integer called secondNumber and set it to 10

-- Let's do some math
I want to create a number called sum and set it to firstNumber plus secondNumber
please create a number called difference and set it to firstNumber minus secondNumber
now create a number called product and set it to firstNumber multiplied by secondNumber

-- Show the results
simply display "Math Results:"
just display sum
display difference
display product

-- ============================================================================
-- SCENARIO 3: Age Checker
-- Written like you're explaining to someone
-- ============================================================================

I want to create an integer called userAge and set it to 21

when userAge is bigger than 18 then
    please display "You are an adult!"
    
    whenever userAge is larger than 65 then
        just display "You qualify for senior discounts!"
    otherwise
        simply display "Welcome!"
    finish
otherwise
    could you display "You are still young!"
finish

-- ============================================================================
-- SCENARIO 4: Shopping List Counter
-- Written in everyday language
-- ============================================================================

let's create a number called itemCount and set it to 0
I want to create a text called listStatus

-- Count some items
go ahead and loop 5 times
    itemCount becomes itemCount plus 1
    now display "Adding item number:"
    simply display itemCount
done

-- Check if we have enough
when itemCount is greater than 3 then
    listStatus becomes "Ready to shop!"
otherwise
    listStatus becomes "Need more items"
finish

please display listStatus

-- ============================================================================
-- SCENARIO 5: Temperature Converter
-- Written as instructions to a helper
-- ============================================================================

could you create a decimal called celsius and set it to 25.0
let me create a decimal called fahrenheit

-- The formula: F = C * 9/5 + 32
-- We'll simplify for this demo
fahrenheit becomes celsius multiplied by 2
fahrenheit becomes fahrenheit plus 30

now display "Temperature in Fahrenheit:"
just display fahrenheit

-- ============================================================================
-- SCENARIO 6: Simple Grading System
-- Written like talking to a student
-- ============================================================================

I want to create a number called score and set it to 85
let's create a text called grade

when score is bigger than 90 then
    grade becomes "A - Excellent!"
otherwise
    whenever score is larger than 80 then
        grade becomes "B - Good job!"
    otherwise
        when score is greater than 70 then
            grade becomes "C - Keep trying!"
        otherwise
            grade becomes "Need improvement"
        finish
    finish
finish

please display "Your grade is:"
simply display grade

-- ============================================================================
-- SCENARIO 7: Defining Helper Functions
-- Written naturally
-- ============================================================================

-- Let me define a greeting function
I want to define a method called sayHello:
    please display "Hello there!"
    simply display "Welcome to NatureLang!"
end function

-- Now let's use it
could you call sayHello
just invoke sayHello

-- ============================================================================
-- SCENARIO 8: Boolean Logic
-- Written with natural true/false synonyms
-- ============================================================================

let me create a flag called isHappy and set it to yes
I want to create a boolean called isWorking and set it to correct
please create a bool called needsBreak and set it to no

when isHappy is equal to yes then
    simply display "Keep smiling!"
finish

when needsBreak is equal to wrong then
    just display "You're doing great, keep going!"
finish

-- ============================================================================
-- SCENARIO 9: Countdown Timer
-- Written like giving instructions
-- ============================================================================

please create a number called countdown and set it to 5

display "Starting countdown..."

loop 5 times
    now display countdown
    countdown becomes countdown minus 1
done

go ahead and display "Blast off!"

-- ============================================================================
-- SCENARIO 10: Final Message
-- Natural conclusion
-- ============================================================================

simply display "============================================"
just display "Congratulations!"
please display "You've seen how NatureLang lets you write"
now display "code in a natural, conversational way!"
I want to display "============================================"
