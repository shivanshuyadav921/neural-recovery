function compute(x, y) {
    if (x > 0) {
        if (y > 0) {
            let z = x * y;
            return z;
        } else {
            let z = x - y;
            return z;
        }
    }
    return 0;
}

let ans = compute(5, 12);
