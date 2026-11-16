#!/usr/bin/env python3
"""Measure the explicitly reviewed optimization scope using GCC JSON coverage.

The default gate is 100% executed lines and branches, including exception arcs.
No source exclusion comments or branch suppression are supported. Repeated
compilations are merged only when their branch topology and function agree.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import html
import json
from pathlib import Path
import re
import subprocess
import sys


def read_scope(root: Path, manifest: Path) -> dict:
    """Validate source fingerprints before accepting any counters."""
    scope = json.loads(manifest.read_text(encoding="utf-8"))
    if scope.get("schema") != "openkal.coverage/v1" or not scope.get("areas"):
        raise ValueError("invalid or empty coverage scope")
    identifiers = set()
    for area in scope["areas"]:
        if area["id"] in identifiers:
            raise ValueError("duplicate scope identifier: " + area["id"])
        identifiers.add(area["id"])
        if not area.get("sites") and area.get("kind") != "build-verification":
            raise ValueError("scope has no sites: " + area["id"])
        for site in area.get("sites", []):
            relative = Path(site["path"])
            if relative.is_absolute() or ".." in relative.parts:
                raise ValueError("scope path must stay inside the repository")
            source = root / relative
            digest = hashlib.sha256(source.read_bytes()).hexdigest()
            if digest != site["sha256"]:
                raise ValueError("source fingerprint changed: " + str(relative))
            length = len(source.read_text(encoding="utf-8").splitlines())
            if not site.get("ranges"):
                raise ValueError("site has no ranges: " + str(relative))
            for first, last in site["ranges"]:
                if not 1 <= first <= last <= length:
                    raise ValueError("invalid source range: " + str(relative))
    return scope


def source_name(root: Path, cwd: str, filename: str) -> str | None:
    path = Path(filename)
    if not path.is_absolute():
        path = Path(cwd) / path
    try:
        return path.resolve().relative_to(root.resolve()).as_posix()
    except ValueError:
        return None


def merge_records(root: Path, records: list[dict], wanted: set[str]) -> dict:
    """Sum compatible object counters without conflating different functions."""
    files: dict = {}
    for record in records:
        for source in record.get("files", []):
            name = source_name(root, record["current_working_directory"], source["file"])
            if name not in wanted:
                continue
            target = files.setdefault(name, {"lines": {}, "branches": {}})
            for line in source["lines"]:
                number = line["line_number"]
                target["lines"][number] = target["lines"].get(number, 0) + line["count"]
                topology = tuple((b["fallthrough"], b["throw"]) for b in line["branches"])
                for index, branch in enumerate(line["branches"]):
                    key = (number, line.get("function_name", ""), topology, index)
                    if key not in target["branches"]:
                        target["branches"][key] = {
                            "line": number, "function": key[1], "arc": index,
                            "count": 0, "throw": branch["throw"],
                            "fallthrough": branch["fallthrough"],
                        }
                    target["branches"][key]["count"] += branch["count"]
    return files


def summarize(scope: dict, files: dict) -> dict:
    """Keep missing instrumentation and uncovered branches as explicit failures."""
    areas = []
    for area in scope["areas"]:
        if area.get("kind") == "build-verification":
            areas.append({"id": area["id"], "title": area["title"],
                          "kind": "build-verification", "status": "manual verification required"})
            continue
        sites = []
        for site in area["sites"]:
            data = files.get(site["path"])
            in_scope = lambda line: any(a <= line <= b for a, b in site["ranges"])
            lines = {n: count for n, count in data["lines"].items() if in_scope(n)} if data else {}
            branches = [b for b in data["branches"].values() if in_scope(b["line"])] if data else []
            unmapped = [[first, last] for first, last in site["ranges"]
                        if not any(first <= line <= last for line in lines)]
            missing = not lines or bool(unmapped)
            sites.append({
                "path": site["path"], "ranges": site["ranges"], "missing": missing,
                "unmapped_ranges": unmapped,
                "lines_total": len(lines), "lines_covered": sum(c > 0 for c in lines.values()),
                "branches_total": len(branches),
                "branches_covered": sum(b["count"] > 0 for b in branches),
                "exception_arcs_total": sum(b["throw"] for b in branches),
                "exception_arcs_covered": sum(b["throw"] and b["count"] > 0 for b in branches),
                "uncovered_lines": sorted(n for n, count in lines.items() if count == 0),
                "uncovered_branches": sorted((b for b in branches if b["count"] == 0),
                                              key=lambda b: (b["line"], b["function"], b["arc"])),
            })
        totals = {key: sum(s[key] for s in sites) for key in (
            "lines_total", "lines_covered", "branches_total", "branches_covered",
            "exception_arcs_total", "exception_arcs_covered")}
        complete = all(not s["missing"] and s["lines_total"] == s["lines_covered"]
                       and s["branches_total"] == s["branches_covered"] for s in sites)
        areas.append({"id": area["id"], "title": area["title"], "sites": sites,
                      "status": "complete" if complete else "incomplete", **totals})
    return {"schema": "openkal.coverage-report/v1", "valid": True, "source_revision": scope["source_revision"],
            "all_runtime_areas_complete": all(a["status"] == "complete" for a in areas
                                               if a.get("kind") != "build-verification"),
            "areas": areas}


def run_gcov(note: Path, gcov: str) -> dict:
    result = subprocess.run([gcov, "--json-format", "--branch-counts", "--branch-probabilities",
                             "--stdout", str(note)], capture_output=True, text=True)
    if result.returncode or any(word in result.stderr.lower() for word in (
            "stamp mismatch", "version mismatch", "profile mismatch")):
        raise ValueError(f"gcov failed for {note}: {result.stderr.strip()}")
    try:
        return json.loads(result.stdout)
    except json.JSONDecodeError as error:
        raise ValueError(f"gcov returned invalid JSON for {note}") from error


def collect(root: Path, build: Path, scope: dict, gcov: str, jobs: int) -> dict:
    wanted = {s["path"] for a in scope["areas"] for s in a.get("sites", [])}
    names = {Path(s).name + ".gcno" for s in wanted}
    # Test-only inclusion targets also contain counters for unchanged source.
    notes = sorted(n for n in build.rglob("*.gcno")
                   if n.name in names or n.name.startswith("Optimization"))
    if not notes:
        raise ValueError("no matching instrumented objects; configure OPENKAL_ENABLE_COVERAGE=ON")
    records = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        for result in pool.map(lambda n: run_gcov(n, gcov), notes):
            validate_note_freshness(root, result)
            records.append(result)
    return merge_records(root, records, wanted)


def validate_note_freshness(root: Path, record: dict) -> None:
    """Reject notes older than any repository source they compiled or included."""
    note = Path(record["data_file"])
    timestamp = note.stat().st_mtime_ns
    for source in record.get("files", []):
        name = source_name(root, record["current_working_directory"], source["file"])
        if name is not None and (root / name).stat().st_mtime_ns > timestamp:
            raise ValueError(f"stale instrumented object {note}: rebuild after changing {name}")


def percentage(covered: int, total: int) -> str:
    return f"{100 * covered / total:.2f}% ({covered}/{total})" if total else "no branches"


def formatted_json(value: dict) -> str:
    """Keep numeric arrays on one line, matching repository JSON style."""
    text = json.dumps(value, indent=2)
    return re.sub(r"\[\s*(-?\d+(?:\s*,\s*-?\d+)*)\s*\]",
                  lambda match: "[" + ", ".join(re.findall(r"-?\d+", match[1])) + "]", text) + "\n"


def write_report(output: Path, report: dict, root: Path) -> None:
    output.mkdir(parents=True, exist_ok=True)
    (output / "coverage.json").write_text(formatted_json(report), encoding="utf-8")
    rows, details = [], []
    for area in report["areas"]:
        if area.get("kind") == "build-verification":
            rows.append(f'<tr><td>{area["id"]}</td><td>{html.escape(area["title"])}</td>'
                        '<td colspan="3">Build verification; not executable C++ coverage</td></tr>')
            continue
        line = percentage(area["lines_covered"], area["lines_total"])
        branch = percentage(area["branches_covered"], area["branches_total"])
        rows.append(f'<tr><td><a href="#{area["id"]}">{area["id"]}</a></td>'
                    f'<td>{html.escape(area["title"])}</td><td>{line}</td><td>{branch}</td>'
                    f'<td>{area["status"]}</td></tr>')
        body = []
        for site in area["sites"]:
            ranges = ", ".join(f"{a}–{b}" for a, b in site["ranges"])
            body.append(f'<h3>{html.escape(site["path"])} · lines {ranges}</h3>')
            if site["missing"]:
                body.append('<p class="gap">Missing instrumentation: cannot pass.</p>')
            body.append('<p>Uncovered executable lines: ' +
                        (", ".join(map(str, site["uncovered_lines"])) or "none") + '</p>')
            for first, last in site["ranges"]:
                source = (root / site["path"]).read_text(encoding="utf-8").splitlines()
                snippets = []
                missed = set(site["uncovered_lines"])
                for number in range(first, last + 1):
                    css = ' class="gap"' if number in missed else ''
                    snippets.append(f'<div{css}>{number:5} {html.escape(source[number - 1])}</div>')
                body.append('<details><summary>Source and uncovered lines</summary><pre>' +
                            "".join(snippets) + '</pre></details>')
            arcs = site["uncovered_branches"]
            if arcs:
                body.append('<details><summary>Uncovered branch arcs (including exceptions)</summary><pre>' +
                            html.escape("\n".join(f'{b["line"]}: arc {b["arc"]} '
                                                  f'{"exception" if b["throw"] else "normal"} '
                                                  f'{b["function"]}' for b in arcs)) + '</pre></details>')
        details.append(f'<section id="{area["id"]}"><h2>{area["id"]} · '
                       f'{html.escape(area["title"])}</h2>{"".join(body)}</section>')
    page = '''<!doctype html><meta charset="utf-8"><title>OpenKal optimization coverage</title>
<style>body{font:15px system-ui;margin:32px;max-width:1300px;color:#222}table{border-collapse:collapse;width:100%}
th,td{text-align:left;padding:9px;border-bottom:1px solid #ccc}section{margin-top:36px}pre{overflow:auto;font-size:12px}
.gap{background:#ffe6e6}a{color:#125ea4}summary{cursor:pointer;padding:8px 0}</style>
<h1>OpenKal · optimization-scope coverage</h1>
<p>Source: ''' + html.escape(report["source_revision"]) + '''</p>
<p>Unfiltered GCC line and branch counts, including exception arcs and compiler cleanup.
The status column compares against a diagnostic 100% threshold; it is not the acceptance rule for this work.
The agreed goal is useful reachable behavior coverage, with remaining gaps documented in OptimizationCoverage.md.
Execution alone does not prove assertion quality or correctness. R11 requires separate build validation.</p>
<table><tr><th>Area</th><th>Planned change</th><th>Lines</th><th>Branches</th><th>Raw 100% status</th></tr>''' + \
           "".join(rows) + '</table>' + "".join(details)
    (output / "index.html").write_text(page, encoding="utf-8")


def write_error(output: Path, error: str) -> None:
    """Invalidate previous results instead of leaving a stale successful report."""
    output.mkdir(parents=True, exist_ok=True)
    report = {"schema": "openkal.coverage-report/v1", "valid": False,
              "all_runtime_areas_complete": False, "error": error, "areas": []}
    (output / "coverage.json").write_text(formatted_json(report), encoding="utf-8")
    page = ('<!doctype html><meta charset="utf-8"><title>Invalid coverage measurement</title>'
            '<h1>Coverage measurement invalid</h1><p>No coverage gate passed.</p><pre>' +
            html.escape(error) + '</pre><p>Rebuild stale objects and rerun the affected tests.</p>')
    (output / "index.html").write_text(page, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--gcov", default="gcov")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--report-only", action="store_true",
                        help="write gaps without failing the command; never marks them complete")
    args = parser.parse_args()
    try:
        scope = read_scope(args.root, args.manifest)
        files = collect(args.root, args.build_dir, scope, args.gcov, max(1, args.jobs))
        if read_scope(args.root, args.manifest) != scope:
            raise ValueError("coverage scope changed during collection")
        report = summarize(scope, files)
        missing = [f'{area["id"]}: {site["path"]} {site["unmapped_ranges"]}'
                   for area in report["areas"] for site in area.get("sites", [])
                   if site["missing"]]
        if missing:
            raise ValueError("missing instrumentation for scope: " + "; ".join(missing))
        write_report(args.output_dir, report, args.root)
        for area in report["areas"]:
            if area.get("kind") == "build-verification":
                print(area["id"], area["status"])
            else:
                print(area["id"], "lines", percentage(area["lines_covered"], area["lines_total"]),
                      "branches", percentage(area["branches_covered"], area["branches_total"]), area["status"])
        print("Diagnostic raw 100% threshold:", "PASS" if report["all_runtime_areas_complete"] else "NOT REACHED")
        print("Report:", args.output_dir / "index.html")
        return 0 if args.report_only or report["all_runtime_areas_complete"] else 1
    except (OSError, KeyError, ValueError) as error:
        write_error(args.output_dir, str(error))
        print("Coverage error:", error, file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
