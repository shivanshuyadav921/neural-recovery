function checkThreshold(val {
    if (val > 50) {
        return true;
    }
    return false;
}

let ok = checkThreshold(75);
