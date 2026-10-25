const memory = new WebAssembly.Memory({
  initial: 1,
  maximum: 32769,
  shared: true,
});

const worker = new Worker(function () {
  onmessage = function ({data}) {
    data.grow(32768);
    postMessage(data.buffer.byteLength);
  };
}, {type: "function"});

worker.postMessage(memory);
const grownBytes = worker.getMessage();
worker.terminate();

if (grownBytes !== 2147549184) {
  throw new Error("shared memory did not grow");
}

const moduleBytes = new Uint8Array([
  0,97,115,109,1,0,0,0,1,5,1,96,0,1,127,2,16,1,1,109,6,109,101,109,111,114,
  121,2,3,0,129,128,2,3,2,1,0,7,9,1,5,112,114,111,98,101,0,0,10,18,1,16,0,65,
  128,128,128,128,120,65,0,66,0,254,1,2,0,11,0,15,4,110,97,109,101,1,8,1,0,5,
  112,114,111,98,101,
]);

const instance = new WebAssembly.Instance(
    new WebAssembly.Module(moduleBytes), {m: {memory}});
instance.exports.probe();
