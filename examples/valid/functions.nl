function calculateTax(subtotal, rate) {
    let tax = subtotal * rate;
    return tax;
}

function processOrder(price, discount, taxRate) {
    let discounted = price - discount;
    let finalTax = calculateTax(discounted, taxRate);
    let total = discounted + finalTax;
    return total;
}

let result = processOrder(100, 15, 0.08);
