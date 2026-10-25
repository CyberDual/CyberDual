let value = {};
for (let index = 0; index < 1000000; ++index) {
  value = new Proxy(value, {});
}
const wrapper = wrapWithProto(value, null);
const map = new WeakMap();
map.set(wrapper, 1);
print("survived");
