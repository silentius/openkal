#!/usr/bin/env python3
"""Generate a checked, test-only bridge for upkeep still embedded in the old Host.

The input is the checked-out source itself; no Git history is consulted. The
fixture driver uses this bridge before extraction and the real helpers afterward.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import textwrap


PINNED_SOURCE_SHA256 = "ff784f825b81048bf6a4c241a32c2774890bd24119e3b439cd723a63e315c6eb"
BINDINGS = {
    "m_store.get()": "store",
    "m_world": "world",
    "m_next_iid": "next_iid",
    "m_world_mutex": "world_mutex",
}
REGIONS = (
    (
        "collection",
        "                        bool expired_collection = false;\n",
        "                        c.attack_cooldown = std::max(0.0f, c.attack_cooldown - elapsed);\n",
        "ClientSession& c, GameWorld& world, IDataStore* store, std::uint64_t now_ms, "
        "std::int64_t (*unix_seconds_now)()",
        {"m_store.get()": 1, "m_world": 2, "m_next_iid": 0, "m_world_mutex": 0},
    ),
    (
        "autopick",
        "                        const int pickup_count =\n",
        "                        tick_fishing(\n",
        "ClientSession& c, std::vector<ClientSession>& clients, GameWorld& world, "
        "IDataStore* store, std::atomic<std::uint32_t>& next_iid, PartyService& parties, "
        "std::mutex& world_mutex, std::uint64_t now_ms",
        {"m_store.get()": 1, "m_world": 2, "m_next_iid": 1, "m_world_mutex": 1},
    ),
    (
        "autosave",
        "                        if (c.last_autosave_ms == 0 || now_ms - c.last_autosave_ms >= kAutoSaveMs) {\n",
        "                    }\n                }\n                if (m_doggebi_ritual.has_value()) {\n",
        "ClientSession& c, IDataStore* store, std::uint64_t now_ms",
        {"m_store.get()": 3, "m_world": 0, "m_next_iid": 0, "m_world_mutex": 0},
    ),
)


def sha256(value):
    return hashlib.sha256(value.encode("utf-8")).hexdigest()


def tokens(value):
    # Exact non-whitespace token spelling/order; no brace deletion or arithmetic normalization.
    return re.findall(r"[A-Za-z_][A-Za-z_0-9]*|[0-9]+|[^\s]", value)


def generate(source):
    if sha256(source) != PINNED_SOURCE_SHA256:
        raise ValueError("Host source differs from the pinned pre-extraction baseline")
    evidence = {
        "source_sha256": sha256(source),
        "source_binding": "checked-out Host.cpp; no Git object lookup",
        "unix_binding": "collection wrapper parameter shadows unix_seconds_now only in that body",
        "regions": {},
    }
    output = [
        source[:source.index("*/") + 2] + "\n\n",
        "// Generated from the pinned project-owned Host source. Do not edit.\n",
        "namespace h1_baseline {\n",
    ]
    for name, start, end, parameters, expected_counts in REGIONS:
        if source.count(start) != 1:
            raise ValueError(f"{name}: expected one start anchor")
        begin = source.index(start)
        if source.count(end, begin) != 1:
            raise ValueError(f"{name}: expected one end anchor after start")
        finish = source.index(end, begin)
        original = textwrap.dedent(source[begin:finish])
        body = original
        counts = {}
        for old, new in BINDINGS.items():
            pattern = re.escape(old) if old.endswith("()") else rf"\b{old}\b"
            body, counts[old] = re.subn(pattern, new, body)
        if counts != expected_counts:
            raise ValueError(f"{name}: unexpected member-reference bindings: {counts}")
        # Reverse the explicit substitutions; all other source tokens must match exactly.
        restored = body
        for old, new in reversed(tuple(BINDINGS.items())):
            if expected_counts[old]:
                restored = re.sub(rf"\b{new}\b", lambda _: old, restored)
        if tokens(restored) != tokens(original):
            raise ValueError(f"{name}: reverse-binding token proof failed")
        if "m_store" in body or "m_world" in body or "m_next_iid" in body:
            raise ValueError(f"{name}: unbound Host member remains")
        output.extend([f"void {name}({parameters}) {{\n", textwrap.indent(body, "    "), "}\n"])
        evidence["regions"][name] = {
            "start_line": source.count("\n", 0, begin) + 1,
            "end_line": source.count("\n", 0, finish),
            "original_sha256": sha256(original),
            "bound_sha256": sha256(body),
            "tokens": len(tokens(original)),
            "bindings": counts,
            "reverse_binding_tokens_equal": True,
        }
    output.append("}  // namespace h1_baseline\n")
    return "".join(output), evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    args = parser.parse_args()
    generated, evidence = generate(args.source.read_text(encoding="utf-8"))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.evidence.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(generated, encoding="utf-8")
    args.evidence.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
