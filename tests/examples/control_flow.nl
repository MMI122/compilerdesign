-- NatureLang Example: Control Flow
-- Demonstrates if/else and loops

create a number called age and set it to 18
create a text called status

-- Conditional with natural comparison
if age is greater than 17 then
    status becomes "adult"
    display "You are an adult!"
otherwise
    status becomes "minor"
    display "You are a minor."
end

-- Another conditional with symbolic operator
create a number called score and set it to 85

if score >= 90 then
    display "Grade: A"
else
    if score >= 80 then
        display "Grade: B"
    else
        if score >= 70 then
            display "Grade: C"
        else
            display "Grade: D or below"
        end
    end
end

-- Loop example: repeat N times
create a number called counter and set it to 0

repeat 5 times
    counter becomes counter plus 1
    display counter
end

-- While loop
create a number called i and set it to 0

while i is less than 3 do
    display "Loop iteration"
    i becomes i plus 1
end

display "Done!"
