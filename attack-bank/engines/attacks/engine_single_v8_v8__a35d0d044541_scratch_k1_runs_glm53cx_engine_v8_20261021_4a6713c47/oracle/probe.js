const worker = new Worker(`
  postMessage("A".repeat(64 * 1024));
`, {type: 'string'});

worker.onmessage = () => {};
worker.getMessage();
