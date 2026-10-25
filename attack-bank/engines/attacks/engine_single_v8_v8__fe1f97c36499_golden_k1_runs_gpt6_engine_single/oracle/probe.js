function probe() {
  return Number.parseInt('vvvvvvvvvvvvvvvvvvvv', 32);
}

%PrepareFunctionForOptimization(probe);
probe();
probe();
%OptimizeFunctionOnNextCall(probe);
probe();
