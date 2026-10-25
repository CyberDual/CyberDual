let first = JSON.parse('"' + 'A'.repeat(255) + '"');
let second = JSON.parse('"B"');
let cons = %ConstructConsString(first, second);

Object.prototype.hasOwnProperty.call({}, cons);
