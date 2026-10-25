if (typeof d8.test.createIterableInterceptorObject === "function") {
  const interceptor = d8.test.createIterableInterceptorObject();
  const values = Array.from(interceptor);

  if (values.length !== 3 || values[0] !== 11 || values[1] !== 22 ||
      values[2] !== 33) {
    throw new Error("unexpected iterable interceptor result");
  }
}
