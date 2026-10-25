let global = newGlobal({newCompartment: true});
let wrapper = wrapWithProto({}, null);
global.wrapper = wrapper;
global.eval("let map = new WeakMap(); map.set(wrapper, {});");
gc();
