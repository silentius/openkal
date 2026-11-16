#!/usr/bin/env python3
"""Generate seven pre-extraction NPC/activity boundaries from pinned owned source."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import textwrap


PINNED_SOURCE_SHA256 = "d62a90403673f0d805efdca79665c1cf8a126a76acf3514b1d730c1e2b863fe7"


def sha256(value):
    return hashlib.sha256(value.encode("utf-8")).hexdigest()


def anchor(opcode):
    return ("                    } else if (c.phase == SessionPhase::Playing &&\n"
            f"                               type == static_cast<std::uint8_t>(protocol::C2S::{opcode})) {{\n")


def between(source, first, last):
    if source.count(first) != 1:
        raise ValueError("First source anchor is not unique: " + first)
    start = source.index(first) + len(first)
    end = source.index(last, start)
    return source[start:end], start


def generate(source):
    if sha256(source) != PINNED_SOURCE_SHA256:
        raise ValueError("Host source differs from the pinned pre-routing-extraction baseline")
    ask, _ = between(source, anchor("AskNpc"), anchor("Quest"))
    quest, _ = between(source, anchor("Quest"), anchor("Cooking"))
    cooking, cooking_start = between(source, anchor("Cooking"), anchor("QuestJobEnd"))
    cancel, cancel_start = between(source, anchor("QuestJobEnd"), anchor("StorageInfo"))
    dialog_end = "                        if (m_doggebi_ritual.has_value() &&\n"
    dialog = ask[:ask.index(dialog_end)]
    portal_first = "                        const bool teleport_confirmed = c.pending_npc_teleport_id == npc->id;\n"
    portal_last = "                        // kind 3 = quest NPC"
    portal = ask[ask.index(portal_first):ask.index(portal_last)]
    process_first = "                        auto sink = make_quest_sink(\n"
    process = quest[quest.index(process_first):]
    validation_first = "                            const NpcEntity* manager = m_npcs.find(c.active_exp_event_npc_id);\n"
    validation_last = "                            const std::int64_t now_unix = unix_now_sec();\n"
    validate = quest[quest.index(validation_first):quest.index(validation_last)]
    collect_first = "                            const int available =\n"
    collect_last = "                            if (donation.activated || donation.extended) {\n"
    collect = quest[quest.index(collect_first):quest.index(collect_last)]
    records = [
        ("select", "const NpcEntity*", "ClientSession& c, const std::vector<std::uint8_t>& payload, const NpcRegistry& m_npcs, std::mutex& m_world_mutex, const ServerEventManager& m_exp_event", dialog, 2, "nullptr", "return npc;\n"),
        ("portal", "bool", "ClientSession& c, const NpcEntity* npc", portal, 1, "true", "return false;\n"),
        ("process", "void", "ClientSession& c, std::vector<ClientSession>& clients, GameWorld& m_world, const ServerConfig& m_config, std::unique_ptr<IDataStore>& m_store, std::atomic<std::uint32_t>& m_next_iid, const FishingCatalog& m_fishing, const std::unique_ptr<QuestEngine>& m_quest_engine, std::uint32_t key", process, 0, "", ""),
        ("validate_donation", "bool", "ClientSession& c, const NpcRegistry& m_npcs, const ServerEventManager& m_exp_event", validate, 1, "false", "return true;\n"),
        ("collect_donation", "std::optional<DonationCollection>", "ClientSession& c, std::uint32_t key, const GameWorld& m_world, std::unique_ptr<IDataStore>& m_store, ServerEventManager& m_exp_event, std::int64_t now_unix", collect, 3, "std::nullopt", "return DonationCollection{amount, donation};\n"),
        ("begin_cooking", "bool", "ClientSession& c, const std::vector<std::uint8_t>& payload, std::vector<ClientSession>& clients, const NpcRegistry& m_npcs", cooking, 3, "false", "return true;\n"),
        ("cancel", "void", "ClientSession& c, std::vector<ClientSession>& clients", cancel, 0, "", ""),
    ]
    output = [source[:source.index("*/") + 2], "\n\n// Generated pinned boundary bridge; do not edit.\nnamespace h4_baseline {\nstruct DonationCollection { int amount; ExpEventDonationResult donation; };\n"]
    proof = {"source_sha256": sha256(source), "source_binding": "checked-out Host.cpp; no Git lookup", "boundaries": {}, "retained_coordinator": "Not duplicated; source reconstruction and actual transport witness cover its precedence and lazy clocks."}
    for name, result, parameters, raw, expected, returned, suffix in records:
        body = textwrap.dedent(raw)
        count = len(re.findall(r"\bcontinue\s*;", body))
        if count != expected:
            raise ValueError(f"{name}: expected{expected} outer continues, saw{count}")
        moved = re.sub(r"\bcontinue\s*;", "return " + returned + ";", body)
        output.extend([result + " " + name + "(" + parameters + ") {\n", textwrap.indent(moved + suffix, "    "), "}\n\n"])
        proof["boundaries"][name] = {"raw_source_sha256": sha256(raw), "dedented_sha256": sha256(body), "outer_continue_returns": count, "parameter_substitutions": [], "added_terminal": suffix}
    proof["boundaries"]["collect_donation"]["coordinator_presentation"] = "Complete activated||extended announcement / progress-chat decision stays unchanged in thread_main; returned amount and donation only bind its existing locals."
    output.append("}  // namespace h4_baseline\n")
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
