// Witness for Bug 2064314 residual defect (jit/arm64 storePtr nullptr fix).
//
// A capturing group with a bounded quantifier compiles to an irregexp
// counter loop whose counter register is initialized to zero. That init
// lowers to storePtr(ImmWord(0), <counter slot>) during native regexp
// codegen (reachable even with --no-baseline --no-ion, since the JIT
// backend is present and native regexp stays enabled).
//
// Plain scripts and regexps without a counted repetition behave normally.
print("probe: start");

// An input longer than 1000 chars makes irregexp tier up to native code
// (Jitcode) eagerly on the first execution instead of interpreting bytecode.
var re = /(a){2,50}b/;
var s = "";
for (var i = 0; i < 2000; i++) {
  s += "a";
}
s += "b";

print("probe: matching");
var ok = re.test(s);
print("probe: done " + ok);
