// Witness for a heap-use-after-free in d8's worker message deserialization.
//
// A child worker serializes and posts a structured message to the parent. When
// the parent reads it with getMessage(), Shell::DeserializeValue builds a
// ValueDeserializer that reads directly from the SerializationData's wire
// payload buffer. On the patched tree, Deserializer::ReadValue frees that
// payload (SerializationData::ReleasePayload) *before* the ValueDeserializer has
// finished parsing the value, so the subsequent parse reads freed heap memory.
//
// On the parent (unpatched) tree the payload is kept alive for the whole parse,
// so no error is reported.

const workerScript = `
  onmessage = function(e) {};
  // A structured payload large enough that parsing it touches many bytes of the
  // serialized wire buffer after the header has been consumed.
  const payload = {
    tag: "worker-message-payload",
    values: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10],
    nested: { a: "alpha", b: "bravo", c: "charlie", d: [true, false, null] },
    text: "the quick brown fox jumps over the lazy dog".repeat(8),
  };
  postMessage(payload);
`;

const w = new Worker(workerScript, { type: "string" });

// Reading the message runs the deserializer on the parent isolate, which frees
// the wire payload mid-parse on the patched tree -> ASan heap-use-after-free.
const message = w.getMessage();

// Touch the result so nothing is optimized away.
print(typeof message, message && message.tag);

w.terminate();
