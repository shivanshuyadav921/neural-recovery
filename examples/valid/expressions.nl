function evaluate(a, b, c) {
    let sum = a + b * c;
    let grouped = (a + b) * c;
    let condition = !(a < b) && (c >= 0);
    if (condition) {
        return sum;
    }
    return grouped;
}

let res = evaluate(10, 20, 3);
