let registries = [];
for (let i = 0; i < 128; ++i) {
  let registry = new FinalizationRegistry(() => {});
  registry.register({}, i, {});
  registries.push(registry);
}
let keep = [];
for (let i = 0; i < 300000; ++i) keep.push({i});
print(registries.length);
