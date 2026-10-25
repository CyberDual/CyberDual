const prefix = "x".repeat(1 << 10);
const subject = prefix + "abce";
const regexp = /ab[cd]e/y;
regexp.lastIndex = prefix.length;
if (!regexp.test(subject)) {
  throw new Error("unexpected regexp result");
}
