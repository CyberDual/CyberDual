const pattern = /(?<=\u212A|x|y)z/i;
if (!pattern.test('\u212Az')) throw new Error('unexpected regexp result');
