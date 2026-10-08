function compute(x, y) {
    let sum = x + y;
    let prod = x * y;
    if (sum > 10) {
        return prod;
    } else {
        return sum;
    }
}

let result = compute(5, 7);
