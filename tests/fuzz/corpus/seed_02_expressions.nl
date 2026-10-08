fn compute(a, b, c) {
  let res = (a + b) * (c - 10) / 2;
  let flag = (a > b) && (b <= c) || (c == 42);
  let neg = -res;
  let inverse = !flag;
  return res + compute(1, 2, 3);
}
