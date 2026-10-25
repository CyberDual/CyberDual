// |jit-test| --fast-warmup; --no-threads

var dom = new FakeDOMObject();
var last = -1;

function f(obj, n) {
  for (var i = 0; i < n; i++) {
    obj.pendingGlobalProperties;
    last = i;
  }
}

for (let i = 0; i < 50; i++) {
  f(dom, 100);
}

dom.pendingGlobalProperties = 1;
f(dom, 1);
assertEq(last, 0);
