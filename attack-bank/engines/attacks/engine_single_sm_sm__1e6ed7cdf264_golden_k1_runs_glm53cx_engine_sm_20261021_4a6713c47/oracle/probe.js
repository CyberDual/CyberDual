const buffers = [];
for (let i = 0; i < 10000; i++) {
  buffers.push(new ArrayBuffer(16));
}
const buffer = buffers[0];
const view = new DataView(buffer);

gczeal(14, 1);
gc("shrinking");
view.getUint8(0);
