-- NatureLang Example: Secure Zone
-- Demonstrates the secure block feature for safe execution

create a number called globalCounter and set it to 0

-- Variables created outside secure zone
create a number called publicValue and set it to 100

-- Enter a secure zone where certain operations are restricted
enter secure zone
    -- Can create local variables
    create a number called localTemp and set it to 5
    create a number called localResult
    
    -- Can do arithmetic
    localResult becomes localTemp plus 10
    
    -- The following would be COMPILE-TIME ERRORS in secure zones:
    -- publicValue becomes 50      -- ERROR: Cannot reassign external variable
    -- display "hello"             -- ERROR: No I/O allowed
    -- ask "input?" and remember x -- ERROR: No I/O allowed
    -- call someFunction()         -- ERROR: No function calls allowed
    
    -- Pure computation is allowed
    localResult becomes localResult multiplied by 2
end

-- After secure zone, normal operations resume
publicValue becomes publicValue plus 1
display "Secure zone example completed"
display publicValue

-- Another secure zone example for safe computation
create a number called a and set it to 10
create a number called b and set it to 20
create a number called safeResult

enter safe zone
    create a number called tempA and set it to a
    create a number called tempB and set it to b
    create a number called tempSum
    tempSum becomes tempA plus tempB
    -- Note: We can't assign to safeResult here because it's external
end

-- Do the assignment outside the secure zone
safeResult becomes a plus b
display "Safe result: "
display safeResult
