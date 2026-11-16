#!/usr/bin/env python3
"""Generate exact old NPC shop bodies without consulting Git history."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import textwrap


PINNED_SOURCE_SHA256 = "5a6e64218e64cfb7b3d9f787d5961d74414eff551ad229eee71d18193f437b0f"
BRANCHES = (
    ("list", "ItemList", "BuyItem",
     "ClientSession& c, const std::vector<std::uint8_t>& payload, "
     "const GameWorld& m_world, const NpcRegistry& m_npcs, const ShopCatalog& m_shops", 3, 0),
    ("buy", "BuyItem", "SellItem",
     "ClientSession& c, const std::vector<std::uint8_t>& payload, "
     "GameWorld& m_world, const NpcRegistry& m_npcs, const ShopCatalog& m_shops, "
     "std::unique_ptr<IDataStore>& m_store, std::atomic<std::uint32_t>& m_next_iid", 7, 0),
    ("sell", "SellItem", "PutOnItem",
     "ClientSession& c, const std::vector<std::uint8_t>& payload, "
     "const GameWorld& m_world, const NpcRegistry& m_npcs, const ShopCatalog& m_shops, "
     "std::unique_ptr<IDataStore>& m_store", 6, 0),
)


def sha256(value):
    return hashlib.sha256(value.encode("utf-8")).hexdigest()


def tokens(value):
    return re.findall(r"[A-Za-z_][A-Za-z_0-9]*|[0-9]+|[^\s]", value)


def anchor(opcode):
    return ("                    } else if (c.phase == SessionPhase::Playing &&\n"
            f"                               type == static_cast<std::uint8_t>(protocol::C2S::{opcode})) {{\n")


def generate(source):
    if sha256(source) != PINNED_SOURCE_SHA256:
        raise ValueError("Host source differs from the pinned pre-shop-extraction baseline")
    output = [source[:source.index("*/") + 2],
              "\n\n// Generated exact source bridge; do not edit.\nnamespace h3_baseline {\n"]
    proof = {"source_sha256": sha256(source),
             "source_binding": "checked-out Host.cpp, no Git lookup and no body substitutions",
             "flow": "do/while(false): outer continue skips reached_end; list filtering retains its inner for continue",
             "regions": {}}
    for name, opcode, next_opcode, parameters, continues, breaks in BRANCHES:
        first, last = anchor(opcode), anchor(next_opcode)
        if source.count(first) != 1 or source.count(last) != 1:
            raise ValueError(f"{name}: request anchors are not unique")
        begin = source.index(first) + len(first)
        end = source.index(last, begin)
        raw = source[begin:end]
        body = textwrap.dedent(raw)
        if len(re.findall(r"\bcontinue\s*;", body)) != continues:
            raise ValueError(f"{name}: unexpected continue count")
        if len(re.findall(r"\bbreak\s*;", body)) != breaks:
            raise ValueError(f"{name}: unexpected break count")
        output.extend([f"bool {name}({parameters}) {{\n",
                       "    bool reached_end = false;\n    do {\n",
                       textwrap.indent(body, "        "),
                       "        reached_end = true;\n    } while (false);\n    return reached_end;\n}\n"])
        proof["regions"][name] = {
            "start_line": source.count("\n", 0, begin) + 1,
            "end_line": source.count("\n", 0, end),
            "raw_source_sha256": sha256(raw), "dedented_sha256": sha256(body),
            "tokens": len(tokens(body)), "continues": continues, "inner_breaks": breaks,
            "body_substitutions": [],
        }
    output.append("}  // namespace h3_baseline\n")
    return "".join(output), proof


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
