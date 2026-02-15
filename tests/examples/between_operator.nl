-- ============================================================================
-- NatureLang Unique "Is Between" Operator Test
-- 
-- The "is between" operator is a UNIQUE feature of NatureLang!
-- 
-- In most programming languages, to check if a value is within a range,
-- you need to write: (x > 5) && (x < 10) or x > 5 and x < 10
-- 
-- In NatureLang, you can simply write:
--     x is between 5 and 10
-- 
-- This is a TERNARY operator (takes 3 operands) that checks if the value
-- falls within the specified range (inclusive of bounds).
-- ============================================================================


-- ============================================================================
-- TEST 1: Basic "is between" usage
-- ============================================================================

create a number called temperature and set it to 72

if temperature is between 65 and 75 then
    display "Temperature is comfortable!"
end


-- ============================================================================
-- TEST 2: Using "is between" with variables
-- ============================================================================

create a number called minAge and set it to 18
create a number called maxAge and set it to 65
create a number called userAge and set it to 25

if userAge is between minAge and maxAge then
    display "You are of working age!"
end


-- ============================================================================
-- TEST 3: Multiple range checks
-- ============================================================================

create a number called score and set it to 85

if score is between 90 and 100 then
    display "Grade: A - Excellent!"
end

if score is between 80 and 89 then
    display "Grade: B - Good job!"
end

if score is between 70 and 79 then
    display "Grade: C - Keep trying!"
end

if score is between 60 and 69 then
    display "Grade: D - Needs improvement"
end

if score is between 0 and 59 then
    display "Grade: F - Please see instructor"
end


-- ============================================================================
-- TEST 4: "is between" with expressions
-- ============================================================================

create a number called baseValue and set it to 50
create a number called offset and set it to 10
create a number called testValue and set it to 55

-- Check if testValue is between (baseValue - offset) and (baseValue + offset)
create a number called lowerBound and set it to baseValue minus offset
create a number called upperBound and set it to baseValue plus offset

if testValue is between lowerBound and upperBound then
    display "Value is within tolerance range!"
end


-- ============================================================================
-- TEST 5: Time/Hour range check
-- ============================================================================

create a number called currentHour and set it to 14

if currentHour is between 6 and 12 then
    display "Good morning!"
end

if currentHour is between 12 and 17 then
    display "Good afternoon!"
end

if currentHour is between 17 and 21 then
    display "Good evening!"
end


-- ============================================================================
-- TEST 6: Negative number ranges
-- ============================================================================

create a number called altitude and set it to 0

if altitude is between 0 and 1000 then
    display "You are at low altitude"
end


-- ============================================================================
-- TEST 7: "between" without "is" (alternative syntax)
-- ============================================================================

create a number called percentage and set it to 75

if percentage between 50 and 100 then
    display "Majority achieved!"
end


-- ============================================================================
-- TEST 8: Nested conditions with "is between"
-- ============================================================================

create a number called income and set it to 55000
create a number called dependents and set it to 2

if income is between 30000 and 60000 then
    display "Middle income bracket"
    
    if dependents is between 1 and 3 then
        display "With a small family"
    end
end


-- ============================================================================
-- TEST 9: Combined with natural language filler words
-- ============================================================================

create a number called playerScore and set it to 8500

please if playerScore is between 8000 and 10000 then
    just display "You made it to the leaderboard!"
    simply display "Congratulations!"
end


-- ============================================================================
-- TEST 10: Real-world scenario - BMI Calculator Range Check
-- ============================================================================

create a number called bmi and set it to 22

display "Your BMI classification:"

if bmi is between 0 and 18 then
    display "Underweight"
end

if bmi is between 18 and 25 then
    display "Normal weight - Healthy!"
end

if bmi is between 25 and 30 then
    display "Overweight"
end

if bmi is between 30 and 100 then
    display "Obese"
end


-- ============================================================================
-- FINAL SUCCESS MESSAGE
-- ============================================================================

display "============================================"
display "All 'is between' operator tests passed!"
display "============================================"
display ""
display "This unique NatureLang operator lets you write:"
display "    x is between 5 and 10"
display ""
display "Instead of the traditional:"
display "    x > 5 and x < 10"
display "============================================"
