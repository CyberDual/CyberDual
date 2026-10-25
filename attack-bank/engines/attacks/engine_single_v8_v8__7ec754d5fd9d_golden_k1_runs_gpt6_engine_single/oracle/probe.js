const left = JSON.parse('"' + '\u0100'.repeat(200) + '"');
const right = JSON.parse('"' + '\u0101'.repeat(200) + '"');
const key = left + right;
Object.prototype.hasOwnProperty.call({}, key);
