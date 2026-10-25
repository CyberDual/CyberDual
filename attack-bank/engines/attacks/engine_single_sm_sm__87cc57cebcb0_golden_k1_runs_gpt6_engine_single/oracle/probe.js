gczeal(0);

const warm = /(.........).*?\1/;
for (let i = 0; i < 10; i++) {
  assertEq(warm.test("abcdefghiabcdefghi"), true);
}

const input = "abcdefghi" + "abcdefgh".repeat(10) + "abcdefghi";
setInterruptCallback('startgc(10, "shrinking"); finishgc(); true;');

const result = interruptRegexp(/(.........).*?\1/, input);
assertEq(result[0], input);
assertEq(result[1], "abcdefghi");
