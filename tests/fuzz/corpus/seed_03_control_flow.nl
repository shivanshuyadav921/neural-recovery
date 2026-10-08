fn loop_test(n) {
  let count = 0;
  while (count < n) {
    if (count == 5) {
      let temp = count * 2;
    } else {
      let temp = count + 1;
    }
    count = count + 1;
  }
  return count;
}
