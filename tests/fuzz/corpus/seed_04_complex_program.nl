fn helper(x, y) {
  return x * y + 1;
}

fn process(limit) {
  let acc = 0;
  let i = 0;
  while (i < limit) {
    if (i % 2 == 0) {
      acc = acc + helper(i, 2);
    } else {
      acc = acc - 1;
    }
    i = i + 1;
  }
  return acc;
}

fn main() {
  let ans = process(10);
  return ans;
}
