// Trigger a property-key lookup using a non-flat string whose two parts are
// direct strings. The lookup copies the key through the string-table buffer.
const left = 'a'.repeat(100);
const right = 'b'.repeat(100);
const key = left + right;
const target = Object.create(null);
if (target[key] !== undefined) throw new Error('unexpected property');
