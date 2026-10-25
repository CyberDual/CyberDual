// Witness for a residual defect in js::UncheckedUnwrapWithoutExpose.
//
// A cross-compartment wrapper whose target is itself a (non-wrapper) scripted
// Proxy is used as a WeakMap key. Inserting the key runs the weakmap delegate
// lookup, which unwraps the key. On the second unwrap step the target is a
// plain proxy that is not a WrapperObject; the edited tree still treats it as
// one and dereferences it, which the real fix does not do.
var g = newGlobal();
var crossCompartmentProxy = g.evaluate("new Proxy({}, {});");

var wm = new WeakMap();
wm.set(crossCompartmentProxy, 1);

print("no crash");
