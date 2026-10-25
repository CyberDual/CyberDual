// Regression probe for a reachable memory-safety defect that emerges only from
// the composition of two maintenance commits in this window:
//
//   * 81ec4c2ba08  [regexp] Share matcher case equivalence when factoring alternatives
//   * f76a45ea413  [regexp] Don't sort alternatives inside lookbehinds
//
// Mechanism:
//   A case-insensitive lookbehind disjunction with >=3 alternatives that share
//   a (case-folded) leading character. Commit f76a's edit keeps
//   RationalizeConsecutiveAtoms running while reading backward, and skips the
//   per-atom shared-prefix-length tightening for atoms whose leading character
//   is not bit-for-bit equal to the common prefix. Commit 81ec's edit makes the
//   case-equivalence comparison pull U+017F ('LATIN SMALL LETTER LONG S') into
//   the 's' run under legacy /i. Together, the "ſbbb" alternative joins the run
//   but never tightens prefix_length, so the prefix-refinement loop reads past
//   that atom's zone-allocated character buffer -> out-of-bounds read ->
//   AddressSanitizer abort (use-after-poison in
//   Disjunction::RationalizeConsecutiveAtoms).
//
//   With only one of the two edits applied the probe is clean:
//     - edit f76a only: U+017F is not grouped with 's', the run is too short
//       to be factored, so no refinement happens.
//     - edit 81ec only: lookbehind disjunctions are not rationalized at all.
//
// Driven entirely through the public d8 shell; no flags required.
//
// First atom "sbbbb" (length 5) is the longest; "ſbbb" (length 4) shares
// the trailing "bbb" with it, so the refinement loop does not stop early and
// walks off the end of the 4-char atom while prefix_length is still 5.
const re = /(?<=sbbbb|ſbbb|sbbbc)!/i;
re.test("sbbbb!");
