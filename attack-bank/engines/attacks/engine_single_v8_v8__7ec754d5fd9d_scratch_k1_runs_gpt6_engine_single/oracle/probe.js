const wide = String.fromCharCode(...Array(300).fill(0x100));
const narrow = String.fromCharCode(...Array(300).fill(0x61));
const key = wide + narrow;
Object.hasOwn({}, key);
