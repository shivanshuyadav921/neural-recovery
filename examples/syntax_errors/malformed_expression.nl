# Malformed binary expression with missing right operand.

function calculate() {
    let result = 10 + * 2;
    return result;
}
