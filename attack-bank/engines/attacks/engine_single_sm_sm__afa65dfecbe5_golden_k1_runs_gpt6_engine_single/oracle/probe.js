setJitCompilerOption("baseline.enable", 1);
setJitCompilerOption("baseline.warmup.trigger", 10);

function iterate(obj) {
  for (const key in obj) {
    void key;
  }
}

for (let i = 0; i < 2000; i++) {
  iterate({a: i, b: i + 1});
  if ((i & 7) === 0) {
    gc();
  }
}

print("survived");
