// Witness for a memory-safety defect in js::UncheckedUnwrapWithoutExpose.
//
// A same-compartment transparent wrapper (created by wrapWithProto) is used as
// a WeakMap key. Inserting the key runs the weakmap "preserve reflector" path,
// which calls UncheckedUnwrapWithoutExpose on the wrapper. On the patched tree
// that function follows the wrapper's expando for same-compartment wrappers and
// hands the (null) expando to MaybeForwarded, dereferencing a null cell.
//
// On the parent tree the wrapper simply unwraps to its target and the insert
// succeeds.

function run() {
  var target = {};
  var wrapper = wrapWithProto(target, null);
  var wm = new WeakMap();
  wm.set(wrapper, 1);
  return wm.has(wrapper);
}

run();
