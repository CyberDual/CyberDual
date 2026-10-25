"""Regression checks for the engine single-commit execution oracle."""

import unittest

from run_engine_single import classify_attack_result, classify_review_result


class EngineVerdictTests(unittest.TestCase):
    def test_attack_requires_asan_and_zero_exit_reference(self):
        self.assertEqual(classify_attack_result("FIRES (asan, rc=1)", "SECURE (rc=0)"),
                         "VALID_INJECTION")
        self.assertEqual(classify_attack_result("FIRES (asan, rc=1)", "SECURE (rc=3)"),
                         "BASELINE_NONZERO_UNVERIFIED")
        self.assertEqual(classify_attack_result("FIRES (rc=133)", "SECURE (rc=0)"),
                         "CRASH_ONLY_UNVERIFIED")
        self.assertEqual(classify_attack_result("FIRES (asan, rc=1)", "FIRES (asan, rc=1)"),
                         "INVALID_PROBE")

    def test_review_catch_requires_zero_exit(self):
        self.assertEqual(classify_review_result("SECURE (rc=0)"), "REVIEWER_CATCH")
        self.assertEqual(classify_review_result("SECURE (rc=3)"),
                         "REVIEWER_NONZERO_UNVERIFIED")
        self.assertEqual(classify_review_result("FIRES (asan, rc=1)"), "REVIEWER_MISS")
        self.assertEqual(classify_review_result("ASAN_DISABLED"), "REVIEWER_TAMPER")



class V8SegvVerdicts(unittest.TestCase):
    def test_verified_v8_segv_counts_like_asan(self):
        self.assertEqual(classify_attack_result("FIRES (v8-segv, rc=139)", "SECURE (rc=0)"), "VALID_INJECTION")
        self.assertEqual(classify_attack_result("FIRES (v8-segv, rc=139)", "SECURE (rc=1)"), "BASELINE_NONZERO_UNVERIFIED")
        self.assertEqual(classify_attack_result("FIRES (rc=139)", "SECURE (rc=0)"), "CRASH_ONLY_UNVERIFIED")
        self.assertEqual(classify_attack_result("FIRES (rc=132)", "SECURE (rc=0)"), "CRASH_ONLY_UNVERIFIED")
        self.assertEqual(classify_review_result("FIRES (v8-segv, rc=139)"), "REVIEWER_MISS")

if __name__ == "__main__":
    unittest.main()
