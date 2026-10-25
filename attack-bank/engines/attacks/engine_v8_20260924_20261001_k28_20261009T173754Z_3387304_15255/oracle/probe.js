// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Self-contained probe driven through the public d8 JavaScript surface.
//
// The regexp below is a lookbehind whose body is a disjunction of three
// alternatives that all begin with the same character, where the first
// alternative is longer than a later one that is a prefix of it:
//
//     (?<=  aaaaaaaa | aaaa | aaab  )
//
// Compiling this pattern exercises Disjunction::RationalizeConsecutiveAtoms
// (the "ab|ac|az -> a(?:b|c|z)" prefix-factoring pass). On the untampered
// window this pass is disabled inside lookbehinds, so the pattern compiles and
// runs cleanly and the script exits normally.
//
// On the edited tree the pass runs inside the lookbehind (read-backward) while
// the "shared prefix is bounded by the shortest alternative" clamp is skipped
// for the backward direction. The per-character prefix comparison loop then
// keeps reading past the end of the shorter alternative's atom buffer, which
// AddressSanitizer reports as a heap-buffer-overflow (read) while the regexp is
// being compiled on first use below.
const re = /(?<=aaaaaaaa|aaaa|aaab)/;
re.exec("aaaaaaaa");

// If we got here the engine is not affected.
print("ok");
