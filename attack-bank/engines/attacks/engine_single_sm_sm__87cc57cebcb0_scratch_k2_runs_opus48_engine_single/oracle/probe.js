// Witness for the RegExp output-buffer sizing defect on the interrupt-retry
// path. `interruptRegexp` is a debug-only shell helper that sets the
// "simulate interrupt" flag and runs a RegExp, forcing execution through the
// interrupt-handling retry loop in RegExpShared::execute.
//
// The subject is long enough (> 1000 chars) that the pattern is JIT-compiled
// on first use, and the pattern backtracks while scanning, so the simulated
// interrupt is taken. On retry the match buffer is re-sized incorrectly, so the
// recompiled native code is invoked with an undersized output vector.
//
// Expected: crash on the patched tree; clean run (no match) on the parent.

if (typeof interruptRegexp !== "function") {
  throw new Error("interruptRegexp unavailable (needs a debug shell)");
}

// Make the interrupt callback resume execution (return true) so the regexp
// engine takes the retry path instead of aborting the script.
setInterruptCallback("true");

var subject = "A".repeat(256 * 1024);
var re = /a(bc|bd)(ef|eg)/;

interruptRegexp(re, subject);
