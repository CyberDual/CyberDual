let re = /(.........).*?\1/;
for (let i = 0; i < 10; i++) {
  re.test("abcdefghiabcdefghi");
}

let input = "abcdefghi" + "abcdefgh".repeat(10) + "abcdefghi";
setInterruptCallback(`
  startgc(10, "shrinking");
  finishgc();
  true;
`);

let result = interruptRegexp(/(.........).*?\1/, input);
assertEq(result[0], input);
assertEq(result[1], "abcdefghi");
