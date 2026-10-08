# Missing semicolon after return (demo case 1).
# Not parsed until Phase 3; not recovered until Phase 5+.

function calculateTotal(price, tax) {
    return price + tax
}

let total = calculateTotal(100, 20);
