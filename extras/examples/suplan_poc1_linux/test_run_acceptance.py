# SPDX-FileCopyrightText: 2026 AC SOFTWARE SP. Z O.O.
# SPDX-License-Identifier: GPL-2.0-or-later

import unittest
from unittest import mock
from run_acceptance import check_data_comparison, check_data_retry_timing


class DataComparisonTests(unittest.TestCase):
    def check_response(self, previous, fresh):
        logs = {"B": list(previous)}
        commands = []

        def send(role, command):
            commands.append((role, command))

        def drain(timeout, predicate):
            self.assertFalse(predicate())
            logs["B"].extend(fresh)
            return predicate()

        result = check_data_comparison("B", logs, send, drain)
        self.assertEqual(commands, [("B", "show-data-comparison")])
        return result

    def test_old_pass_cannot_hide_current_failure(self):
        self.assertFalse(self.check_response(
            ["DATA_COMPARE=PASS"], ["DATA_COMPARE=FAIL"]))

    def test_old_pass_cannot_hide_missing_response(self):
        self.assertFalse(self.check_response(["DATA_COMPARE=PASS"], []))

    def test_old_pass_cannot_hide_pending_capture(self):
        self.assertFalse(self.check_response(
            ["DATA_COMPARE=PASS"], ["DATA_COMPARE=PENDING"]))

    def test_current_pass_after_previous_failure(self):
        self.assertTrue(self.check_response(
            ["DATA_COMPARE=FAIL"], ["other output", "DATA_COMPARE=PASS"]))

    def test_first_current_result_is_authoritative(self):
        self.assertFalse(self.check_response(
            [], ["DATA_COMPARE=FAIL", "DATA_COMPARE=PASS"]))


class DataRetryTimingTests(unittest.TestCase):
    def check_response(self, fresh, previous=()):
        logs = {"B": list(previous)}
        commands = []

        def send(role, command):
            commands.append((role, command))

        def drain(timeout, predicate):
            self.assertFalse(predicate())
            # The harness already completed delivery before diagnostics arrive.
            logs["B"].extend(["EVENT B ACK resource=50002", *fresh])
            return predicate()

        # Reading the runner's clock cannot establish peer retry timing.
        with mock.patch("run_acceptance.time.monotonic",
                        side_effect=AssertionError("runner clock used")):
            result = check_data_retry_timing("B", logs, send, drain)
        self.assertEqual(commands, [("B", "show-data-retry-timing")])
        return result

    def test_delayed_diagnostics_preserve_valid_peer_measurement(self):
        self.assertTrue(self.check_response(
            ["DATA_RETRY_TIMING delay_ms=150"]))
        self.assertTrue(self.check_response(
            ["DATA_RETRY_TIMING delay_ms=240"]))

    def test_immediate_or_early_retry_still_fails(self):
        for delay in (0, 149):
            with self.subTest(delay=delay):
                self.assertFalse(self.check_response(
                    [f"DATA_RETRY_TIMING delay_ms={delay}"]))

    def test_stale_valid_measurement_cannot_hide_current_failure(self):
        self.assertFalse(self.check_response(
            ["DATA_RETRY_TIMING delay_ms=0"],
            ["DATA_RETRY_TIMING delay_ms=150"]))

    def test_missing_pending_and_malformed_measurements_fail(self):
        for fresh in ([], ["DATA_RETRY_TIMING=PENDING"],
                      ["DATA_RETRY_TIMING delay_ms=invalid"]):
            with self.subTest(fresh=fresh):
                self.assertFalse(self.check_response(
                    fresh, ["DATA_RETRY_TIMING delay_ms=150"]))


if __name__ == "__main__":
    unittest.main()
