// Flags: --allow-natives-syntax
//
// Composition probe (arm: multi) for the edited maint-batch tree.
//
// A maglev-virtualized holey FixedDoubleArray carries a hole-NaN Float64
// constant element. When the optimized frame deopts, the array is rebuilt by
// TranslatedState::MaterializeFixedDoubleArray.
//
//   * commit 06fa9d17b8f (edited, CreateFixedDoubleArray): stores an inflated
//     FixedArrayBase length field whenever a hole-NaN Float64 element is seen.
//   * commit 56e1f11af79 (edited, TranslatedValue::GetDoubleValue): stops
//     CHECK-validating unexpected value kinds during double materialization.
//
// With BOTH edits the inflated length drives the materialization loop past the
// end of TranslatedFrame::values_ (a base::SmallVector on the C++ heap),
// producing an AddressSanitizer heap-buffer-overflow read. With neither edit,
// or either edit alone, this probe does not report AddressSanitizer.
function opt(deopt) {
  const a = [1.1, , 3.3, 4.4, 5.5, 6.6, 7.7, 8.8];
  if (deopt) {
    deopt.z;  // eager deopt point: `a` is materialized here
    return a[0] + a[7];
  }
  return 0;
}

%PrepareFunctionForOptimization(opt);
for (let i = 0; i < 10; i++) opt(false);
%OptimizeMaglevOnNextCall(opt);
opt({ z: 1 });
print("DONE");
