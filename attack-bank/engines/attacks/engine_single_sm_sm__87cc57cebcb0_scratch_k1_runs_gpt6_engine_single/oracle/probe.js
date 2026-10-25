const input = "\u0100".repeat(2048);
const pattern = /(\u0100+)/;

if (!pattern.test(input)) {
  throw new Error("RegExp match failed");
}
