const string = newString("x".repeat(31) + "a", {capacity: 32});
const regexp = /^abcd/y;

regexp.lastIndex = 0;
regexp.exec(string);
regexp.lastIndex = 31;
regexp.exec(string);
