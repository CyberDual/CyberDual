for (let i = 0; i < 8; ++i) {
  new Worker(`
    new Worker(\`
      const buffer = new ArrayBuffer(1024);
      postMessage(buffer, [buffer]);
    \`, {type: "string"});
  `, {type: "string"});

  new Worker("", {type: "string"});
}
