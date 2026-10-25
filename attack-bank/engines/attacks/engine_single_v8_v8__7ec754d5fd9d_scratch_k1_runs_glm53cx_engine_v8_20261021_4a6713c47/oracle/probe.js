const left = "a";
const right = "b".repeat(255);
const source = %ConstructConsString(left, right);
const object = {};
object[source] = 1;
