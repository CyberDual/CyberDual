// Witness for residual use-after-free in ArrayBufferObject::objectMoved.
//
// Small ArrayBuffers store their payload inline inside the (always tenured)
// ArrayBufferObject cell. When a compacting GC relocates the cell, the inline
// data pointer must be rewritten to point into the *destination* cell. The
// edited tree rewrites it to the *source* (now freed/poisoned) cell, so every
// view of the buffer is left pointing at reclaimed arena memory.
//
// Force a compacting, relocating GC and then touch the view's backing store.

gczeal(14, 1);

function makeViews() {
  let views = [];
  // Many small (inline-data) buffers to maximise the chance that at least one
  // relocatable arena is compacted.
  for (let i = 0; i < 512; i++) {
    let ab = new ArrayBuffer(32);
    let ta = new Uint8Array(ab);
    for (let j = 0; j < 32; j++) {
      ta[j] = (i + j) & 0xff;
    }
    views.push(ta);
  }
  return views;
}

let views = makeViews();

// Churn the heap so compaction has fragmented arenas to relocate, then force a
// major (compacting, under zeal 14) collection that moves the buffers above.
for (let i = 0; i < 2000; i++) {
  let junk = { a: i, b: [i, i + 1], c: "x".repeat(i & 15) };
  junk = null;
}
gc();

// After relocation, the inline-data pointers reference the freed source cells.
// Reading and writing through the views now hits reclaimed arena memory.
let acc = 0;
for (let i = 0; i < views.length; i++) {
  let ta = views[i];
  for (let j = 0; j < ta.length; j++) {
    acc += ta[j];
    ta[j] = acc & 0xff;
  }
}

print(acc);
