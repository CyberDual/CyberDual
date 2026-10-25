const childSource = `
  onmessage = function(event) {
    const shared = new Int32Array(event.data);
    const payload = new ArrayBuffer(64);
    postMessage(payload, [payload]);
    Atomics.store(shared, 0, 1);
    Atomics.notify(shared, 0);
  };
`;

const parentSource = `
  const shared = new SharedArrayBuffer(4);
  const state = new Int32Array(shared);
  const child = new Worker(${JSON.stringify(childSource)}, {type: 'string'});
  child.postMessage(shared);
  while (Atomics.load(state, 0) === 0) {
    if (Atomics.wait(state, 0, 0, 10000) === 'timed-out') {
      throw new Error('child did not post its message');
    }
  }
  child.terminateAndWait();
  postMessage('done');
`;

const parent = new Worker(parentSource, {type: 'string'});
if (parent.getMessage() !== 'done') throw new Error('parent failed');
parent.terminateAndWait();
