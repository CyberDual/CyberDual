const realm = newGlobal({newCompartment: true});
const wrapper = realm.eval("({})");
nukeCCW(wrapper);
new WeakMap().set(wrapper, 1);
