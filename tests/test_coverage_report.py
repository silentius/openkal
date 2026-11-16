#!/usr/bin/env python3
"""Owned regression tests for the strict coverage gate and counter merging."""

import hashlib
import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

spec = importlib.util.spec_from_file_location("coverage_report", Path(__file__).parents[1] / "tools/coverage_report.py")
coverage = importlib.util.module_from_spec(spec)
spec.loader.exec_module(coverage)


class CoverageGateTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "Subject.cpp"
        self.source.write_text("int subject(int n) {\n return n ? 1 : 0;\n}\n", encoding="utf-8")
        self.scope = {"schema": "openkal.coverage/v1", "source_revision": "owned-fixture",
                      "areas": [{"id": "R01", "title": "Subject", "sites": [{
                          "path": "Subject.cpp", "sha256": hashlib.sha256(self.source.read_bytes()).hexdigest(),
                          "ranges": [[1, 3]]}]}]}

    def record(self, left, right, throw=False, function="subject"):
        return {"current_working_directory": str(self.root), "files": [{"file": "Subject.cpp", "lines": [
            {"line_number": 2, "count": left + right, "function_name": function, "branches": [
                {"count": left, "fallthrough": True, "throw": False},
                {"count": right, "fallthrough": False, "throw": throw}]}]}]}

    def result(self, *records):
        files = coverage.merge_records(self.root, list(records), {"Subject.cpp"})
        return coverage.summarize(self.scope, files)

    def test_unexecuted_branch_fails_even_with_all_lines_executed(self):
        result = self.result(self.record(1, 0))
        self.assertFalse(result["all_runtime_areas_complete"])
        self.assertEqual(result["areas"][0]["lines_covered"], 1)
        self.assertEqual(result["areas"][0]["branches_covered"], 1)

    def test_compatible_object_instances_merge_positive_counters(self):
        result = self.result(self.record(1, 0), self.record(0, 1))
        self.assertTrue(result["all_runtime_areas_complete"])
        self.assertEqual(result["areas"][0]["branches_total"], 2)

    def test_exception_arc_is_not_suppressed(self):
        result = self.result(self.record(1, 0, throw=True))
        self.assertFalse(result["all_runtime_areas_complete"])
        self.assertEqual(result["areas"][0]["exception_arcs_total"], 1)
        self.assertEqual(result["areas"][0]["exception_arcs_covered"], 0)

    def test_different_functions_and_topologies_do_not_mask_gaps(self):
        self.assertFalse(self.result(self.record(1, 0), self.record(0, 1, function="lambda"))[
            "all_runtime_areas_complete"])
        self.assertFalse(self.result(self.record(1, 0), self.record(0, 1, throw=True))[
            "all_runtime_areas_complete"])

    def test_missing_instrumentation_fails_closed(self):
        self.assertFalse(coverage.summarize(self.scope, {})["all_runtime_areas_complete"])

    def test_one_instrumented_range_does_not_hide_an_unmapped_range(self):
        self.source.write_text("int subject(int n) {\n return n ? 1 : 0;\n}\n" + "// owned range\n" * 57)
        self.scope["areas"][0]["sites"][0]["ranges"].append([50, 60])
        result = self.result(self.record(1, 1))
        self.assertFalse(result["all_runtime_areas_complete"])
        self.assertEqual(result["areas"][0]["sites"][0]["unmapped_ranges"], [[50, 60]])

    def test_stale_object_is_rejected_even_when_manifest_hash_is_current(self):
        note = self.root / "Subject.cpp.gcno"
        note.touch()
        record = self.record(1, 1)
        record["data_file"] = str(note)
        os.utime(note, ns=(1_000_000_000, 1_000_000_000))
        os.utime(self.source, ns=(2_000_000_000, 2_000_000_000))
        with self.assertRaisesRegex(ValueError, "stale instrumented object"):
            coverage.validate_note_freshness(self.root, record)
        os.utime(note, ns=(3_000_000_000, 3_000_000_000))
        coverage.validate_note_freshness(self.root, record)

    def test_scope_fingerprint_and_bounds_cannot_silently_drift(self):
        manifest = self.root / "scope.json"
        manifest.write_text(json.dumps(self.scope), encoding="utf-8")
        coverage.read_scope(self.root, manifest)
        self.source.write_text("changed\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "fingerprint"):
            coverage.read_scope(self.root, manifest)

    def test_report_keeps_missing_source_visible_and_escapes_markup(self):
        self.scope["areas"][0]["title"] = "<script>"
        result = coverage.summarize(self.scope, {})
        output = self.root / "report"
        coverage.write_report(output, result, self.root)
        page = (output / "index.html").read_text()
        self.assertIn("Missing instrumentation", page)
        self.assertIn("&lt;script&gt;", page)
        self.assertNotIn("<script>", page)

    def test_failed_collection_invalidates_a_previous_success_report(self):
        output = self.root / "report"
        coverage.write_report(output, self.result(self.record(1, 1)), self.root)
        self.assertTrue(json.loads((output / "coverage.json").read_text())["all_runtime_areas_complete"])
        coverage.write_error(output, "stale <object>")
        result = json.loads((output / "coverage.json").read_text())
        self.assertFalse(result["valid"])
        self.assertFalse(result["all_runtime_areas_complete"])
        self.assertIn("stale &lt;object&gt;", (output / "index.html").read_text())

    def test_report_only_cannot_accept_missing_instrumentation(self):
        manifest = self.root / "scope.json"
        manifest.write_text(json.dumps(self.scope), encoding="utf-8")
        output = self.root / "report"
        arguments = ["coverage_report", "--root", str(self.root), "--build-dir", str(self.root),
                     "--manifest", str(manifest), "--output-dir", str(output), "--report-only"]
        with mock.patch("sys.argv", arguments), mock.patch.object(coverage, "collect", return_value={}), \
                mock.patch("sys.stderr"):
            self.assertEqual(coverage.main(), 2)
        result = json.loads((output / "coverage.json").read_text())
        self.assertFalse(result["valid"])
        self.assertIn("missing instrumentation", result["error"])


if __name__ == "__main__":
    unittest.main()
