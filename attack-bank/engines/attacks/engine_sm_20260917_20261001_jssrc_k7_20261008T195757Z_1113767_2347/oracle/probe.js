const prefix = "x".repeat(65526);
const subject = prefix + "a";
const re = /abcd/y;
re.lastIndex = prefix.length;
if (re.exec(subject) !== null) {
  throw new Error("unexpected regexp match");
}
