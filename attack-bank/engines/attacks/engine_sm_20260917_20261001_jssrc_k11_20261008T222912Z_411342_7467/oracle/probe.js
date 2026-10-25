const length = 767;
const chars = new Array(length).fill("x");
chars[length - 2] = "a";
chars[length - 1] = "b";

const input = chars.join("");
const regexp = /a(bc)d/y;
regexp.lastIndex = length - 2;
if (regexp.exec(input) !== null) {
  throw new Error("unexpected match");
}
