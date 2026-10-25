// Witness for a use-after-free of an ArrayBufferExtension (and its backing
// store) that survives on the parent commit but is reclaimed on the patched
// tree.
//
// On the patched tree, MarkingVisitorBase::VisitExternalPointer skips marking
// the ArrayBufferExtension of resizable (length-tracking) ArrayBuffers during a
// full GC. The extension and its page-backed store are therefore finalized by
// the ArrayBufferSweeper even though the owning JSArrayBuffer is still alive.
// A subsequent full GC re-visits the dangling external-pointer entry (reading
// the freed ArrayBufferExtension), and any access through the typed array hits
// the freed backing store.
//
// On the parent commit the extension is marked normally, so everything below
// runs without error.

function main() {
  // Resizable, non-shared ArrayBuffer: its extension has is_resizable_by_js().
  const ab = new ArrayBuffer(0x1000, {maxByteLength: 0x40000});
  const ta = new Uint8Array(ab);  // length-tracking view
  for (let i = 0; i < 0x1000; i++) ta[i] = (i & 0xff);

  // Keep both strongly reachable so the JSArrayBuffer itself survives GC.
  globalThis.__ab = ab;
  globalThis.__ta = ta;

  // First full GC: on the patched tree the extension is left unmarked and the
  // ArrayBufferSweeper finalizes it (freeing the extension + backing store).
  %CollectGarbage(null);
  // Second full GC: re-visits the now-dangling external-pointer entry.
  %CollectGarbage(null);
  %CollectGarbage(null);

  // Touch the backing store through the (still-"alive") typed array.
  let acc = 0;
  for (let i = 0; i < 0x1000; i++) {
    ta[i] = (ta[i] + 1) & 0xff;
    acc += ta[i];
  }
  print("completed without fault, acc=" + acc);
}

main();
