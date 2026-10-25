const registry = new FinalizationRegistry(() => {});
const keep = {};
let sink = null;

function makeToken(index) {
  const token = {};
  token.a0 = index;
  token.a1 = index;
  token.a2 = index;
  token.a3 = index;
  token.a4 = index;
  token.a5 = index;
  return token;
}

function allocate(index) {
  const token = makeToken(index);
  registry.register(keep, index, token);
  const first = token.a4;
  const array = new Array(4000);
  const second = token.a5;
  sink = array;
  return first + second;
}

%PrepareFunctionForOptimization(makeToken);
%PrepareFunctionForOptimization(allocate);
for (let index = 0; index < 10; index++) {
  allocate(index);
}
%OptimizeFunctionOnNextCall(allocate);
let total = 0;
for (let index = 0; index < 2000; index++) {
  total += allocate(index);
}
