-- NatureLang Example: Secure Zones
-- Demonstrates secure zone feature for safety-critical code

-- Create some global variables
create a number called globalCounter and set it to 0
create a text called globalData and set it to "initial"

-- Enter a secure zone where certain operations are restricted
begin secure zone
    -- Can create local variables
    create a number called localTemp and set it to 5
    create a number called localResult
    
    -- Can do arithmetic
    localResult becomes localTemp plus 10
    
    -- The following would be COMPILE-TIME ERRORS in secure zones:
    -- - No system calls (like running external commands)
    -- - No file operations
    -- - No network operations
    -- - Only local variable access
    
    display "Secure computation complete"
    display localResult
end secure zone

-- Alternative syntax: safely do
safely do
    create a number called safeCalc
    safeCalc becomes 100 multiplied by 2
    display "Safe calculation: "
    display safeCalc
end safely

-- Back in normal mode - can do anything
globalCounter becomes globalCounter plus 1
display "Normal mode counter: "
display globalCounter

-- End message
display "=== Secure Zone Demo Complete ==="
