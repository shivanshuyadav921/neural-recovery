function isEven(n) {
    if (n % 2 == 0) {
        return true;
    } else {
        return false;
    }
}

function countUp(limit) {
    let count = 0;
    while (count < limit) {
        count = count + 1;
    }
    return count;
}

let totalCount = countUp(10);
let evenFlag = isEven(totalCount);
