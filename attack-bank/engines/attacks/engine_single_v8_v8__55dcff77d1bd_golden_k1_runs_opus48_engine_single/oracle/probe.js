// Flags: --allow-natives-syntax
//
// Witness for: Scavenger hash lookup can observe an unresolved forwarding
// pointer for a JSWeakRef unregister token.
//
// `f` is optimized so that, after `o` is registered as a FinalizationRegistry
// unregister token, TurboFan keeps only `o`'s fast-property backing store
// (a PropertyArray) live across the scavenge triggered by `new Array(4000)`,
// while the token object `o` itself becomes unreachable. The scavenger
// evacuates and installs a forwarding pointer on the live PropertyArray, but
// never visits the dead token. During weakness clearing the registry looks up
// the dead token's identity hash, which reads the PropertyArray's map word --
// still an unresolved forwarding pointer -- and dereferences it as a map.

const reg = new FinalizationRegistry(() => {});
const keep = {};
let sink = null;

function mk(i) {
  const o = {};
  o.a0 = i;
  o.a1 = i;
  o.a2 = i;
  o.a3 = i;
  o.a4 = i;
  o.a5 = i;
  o.a6 = i;
  o.a7 = i;
  return o;
}

function f(i) {
  const o = mk(i);
  reg.register(keep, i, o);
  const x = o.a5;
  const arr = new Array(4000);
  const y = o.a6;
  sink = arr;
  return x + y;
}

%PrepareFunctionForOptimization(mk);
%PrepareFunctionForOptimization(f);
for (let i = 0; i < 10; i++) f(i);
%OptimizeFunctionOnNextCall(f);

let s = 0;
for (let i = 0; i < 200000; i++) s += f(i);
