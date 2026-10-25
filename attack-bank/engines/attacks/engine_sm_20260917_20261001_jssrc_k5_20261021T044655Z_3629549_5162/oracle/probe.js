let subject = String.fromCharCode(...new Array(25).fill(97));
gc();

const pattern = /^abcd/g;
pattern.lastIndex = 22;
pattern.test(subject);
