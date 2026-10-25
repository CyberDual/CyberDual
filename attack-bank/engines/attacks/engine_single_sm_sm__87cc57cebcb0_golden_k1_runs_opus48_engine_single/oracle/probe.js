// Witness for a residual use-after-free in resumable RegExp interrupt handling.
//
// When a RegExp executing in jitcode is interrupted and resumed, the interrupt
// handler runs arbitrary JS (here, a shrinking GC). A shrinking GC discards
// RegExp jitcode. The code that is still active on the stack is then resumed,
// jumping back into the freed executable buffer.

gczeal(0);

// Warm up the shared RegExpShared for this pattern so it is compiled to
// native jitcode before we execute it under an interrupt.
let re = /(.........).*?\1/;
for (var i = 0; i < 10; i++) {
  assertEq(re.test("abcdefghiabcdefghi"), true);
}

var s0 = "abcdefghi" + "abcdefgh".repeat(10) + "abcdefghi";

// Under --fuzzing-safe the interrupt callback must be a string, evaluated in a
// sandbox global that still exposes the GC-controlling shell functions. A
// shrinking GC discards RegExp jitcode out from under the regexp that is
// currently suspended on the stack.
setInterruptCallback("startgc(10, 'shrinking'); finishgc(); true");

let result = interruptRegexp(/(.........).*?\1/, s0);

assertEq(result[0], s0);
assertEq(result[1], "abcdefghi");
