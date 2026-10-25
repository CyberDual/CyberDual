function victim(a) {
  a = new Uint8Array(65536);
  a[0] = 37;
  gc();
  return a[0];
}

for (let i = 0; i < 100; i++) {
  if (victim() !== 37) {
    throw new Error("unexpected result");
  }
}
