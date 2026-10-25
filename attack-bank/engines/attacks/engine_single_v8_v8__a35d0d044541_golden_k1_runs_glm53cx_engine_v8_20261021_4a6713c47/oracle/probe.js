for (let i = 0; i < 4; i++) {
  const worker = new Worker(`
    const child = new Worker(\`
      const buffer = new ArrayBuffer(4096);
      postMessage(buffer, [buffer]);
    \`, {type: 'string'});
  `, {type: 'string'});

  new Worker('', {type: 'string'});
}
