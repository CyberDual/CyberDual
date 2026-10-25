const registry = new FinalizationRegistry(() => {});
const target = {};
let sink;

function makeToken(i) {
  const token = [];
  token.a0 = i;
  token.a1 = i;
  token.a2 = i;
  token.a3 = i;
  token.a4 = i;
  token.a5 = i;
  token.a6 = i;
  token.a7 = i;
  return token;
}

function registerOne(i) {
  const token = makeToken(i);
  registry.register(target, i, token);
  const x = token.a5;
  const array = new Array(4000);
  const y = token.a6;
  sink = array;
  return x + y;
}

%PrepareFunctionForOptimization(makeToken);
%PrepareFunctionForOptimization(registerOne);
for (let i = 0; i < 10; i++) registerOne(i);
%OptimizeFunctionOnNextCall(registerOne);
let sum = 0;
for (let i = 0; i < 2000; i++) sum += registerOne(i);
print(sum);
