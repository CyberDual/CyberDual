// Witness for the regressed Scavenger / FinalizationRegistry unregister-token
// path. Registering multiple targets under a single unregister token builds a
// multi-entry key list in the registry's key map. When that token dies during a
// scavenge, the key list is collapsed while the shell processes the weak cells.
//
// On the parent tree this proceeds cleanly. On the patched tree it corrupts the
// key-map access, producing an out-of-bounds write / fatal crash during the
// young-generation GC.

const registry = new FinalizationRegistry(function () {});

// Keep the registered targets strongly reachable so that only the shared
// unregister tokens become unreachable. This forces the dead-unregister-token
// path (rather than the dead-target path) during scavenging.
const liveTargets = [];

function buildEntries(count) {
  for (let i = 0; i < count; i++) {
    const a = {};
    const b = {};
    liveTargets.push(a, b);
    // Same token object for two different targets -> two WeakCells share one
    // key-map entry and are linked through the per-key list.
    const token = {};
    registry.register(a, i, token);
    registry.register(b, i + 0.5, token);
    // `token` is not retained anywhere else and dies at the end of the loop
    // iteration.
  }
}

buildEntries(400);

// Fill up new space and then allocate, forcing a scavenge in which the shared
// unregister tokens are collected and their key lists are collapsed.
for (let round = 0; round < 8; round++) {
  %SimulateNewspaceFull();
  for (let i = 0; i < 2048; i++) {
    const filler = [i, i + 1, i + 2];
    liveTargets.length > 20000 && liveTargets.pop();
  }
}

// Allocate more young garbage to keep scavenges coming.
for (let i = 0; i < 200000; i++) {
  const t = { x: i };
}

print("done");
