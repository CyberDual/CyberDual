function probe(value) {
  try {
    return value;
  } catch (error) {
    return 0;
  }
}

probe(1);
const bytecode = %GetBytecode(probe);
%InstallBytecode(probe, bytecode);
