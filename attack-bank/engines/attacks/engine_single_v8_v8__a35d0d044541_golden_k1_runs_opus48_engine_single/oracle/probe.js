// Residual UAF: a child worker posts an ArrayBuffer to its parent isolate via
// postMessage after the parent worker has begun shutting down. The transferred
// BackingStore is held by a CheckMessageFromWorkerTask posted through a
// freshly re-created (orphaned) task runner on the disposed parent isolate,
// surviving past V8::Dispose().
for (let i = 0; i < 4; i++) {
  let w1 = new Worker(`
    let w2 = new Worker(\`
      for (let j = 0; j < 8; j++) {
        let ab = new ArrayBuffer(1024 * 64);
        postMessage(ab, [ab]);
      }
    \`, {type: "string"});
  `, {type: "string"});
  let w3 = new Worker(`
  `, {type: "string"});
}
