const pattern = /(?<=(😀|😁|😂))x/u;
if (!pattern.test('😀x')) throw new Error('unexpected regexp result');
