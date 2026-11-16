#!/usr/bin/env python3
"""Emit a source-pinned test bridge for the embedded Doggebi controller.

Read only the checked-out source, never Git history. Generated C++ is a temporary
old-behavior adapter; the frozen fixture later invokes the real extracted helpers.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import textwrap

PINNED_HOST = "777db759f403a931e354a5805bbf439bb13ffea24acf8626250fcc98cc563e3a"
PINNED_SCHEDULED = "612109ff4b31f0b8a16062343d2c591c04674c932759c627ef216c9ffff7e153"
STATE = (
    ("doggebi_lord_id", "lord_id"),
    ("doggebi_lord_despawn_ms", "lord_despawn_ms"),
    ("doggebi_last_countdown_broadcast_ms", "last_countdown_broadcast_ms"),
    ("doggebi_evil_spirit_ids", "spirit_ids"),
    ("doggebi_evil_spirit_max", "spirit_max"),
    ("doggebi_evil_spirit_spawn_cursor", "spawn_cursor"),
    ("doggebi_skill_cast", "cast"),
    ("doggebi_skill_timers", "timers"),
    ("doggebi_skill_random", "random"),
    ("doggebi_lord_ascending", "ascending"),
    ("doggebi_lord_ascension_next_pulse_ms", "ascension_next_pulse_ms"),
    ("doggebi_lord_ascension_pulse_count", "ascension_pulse_count"),
)

def digest(text):
    return hashlib.sha256(text.encode()).hexdigest()

def tokens(text):
    return re.findall(r"[A-Za-z_][A-Za-z_0-9]*|[0-9]+|[^\s]", text)

def unique(source, begin, end):
    if source.count(begin) != 1:
        raise ValueError("expected one unique start anchor: " + begin.strip())
    start = source.index(begin)
    if source.count(end, start) != 1:
        raise ValueError("expected one unique following end anchor: " + end.strip())
    finish = source.index(end, start)
    return source[start:finish], start, finish

def generate(source):
    if digest(source) != PINNED_HOST:
        raise ValueError("Host differs from the pinned pre-H5 source; rebase explicitly")
    declarations, ds, de = unique(source, "        std::uint32_t doggebi_lord_id = 0;\n",
                                    "        auto reset_doggebi_skill_controller = [&]() {\n")
    reset, rs, re_ = unique(source, "        auto reset_doggebi_skill_controller = [&]() {\n",
                                  '        chat_service.commands().register_command("fly",')
    presentation, ps, pe = unique(source, "        auto send_doggebi_ritual_state = [&](ClientSession& dest) {\n",
                                        "        auto revive_at_saved_point = [&](ClientSession& c) {\n")
    end = source.index("                debug_dummies.tick(unix_now_sec());\n")
    start = source.rfind("                if (m_doggebi_ritual.has_value()) {\n", 0, end)
    if start < 0:
        raise ValueError("scheduled optional guard anchor missing")
    scheduled = source[start:end]
    if digest(scheduled) != PINNED_SCHEDULED:
        raise ValueError("scheduled body differs from H5 body")
    if presentation.count("steady_ms_now()") != 1:
        raise ValueError("presentation clock binding changed")
    if reset.count("steady_ms_now()") or scheduled.count("steady_ms_now()"):
        raise ValueError("unexpected direct clock read outside the presentation body")
    member_declarations = textwrap.dedent(declarations).strip() + "\n"
    declared_names = []
    for old, new in STATE:
        pattern = rf"\b{old}\b"
        if len(re.findall(pattern, member_declarations)) != 1:
            raise ValueError("state declaration binding is not unique: " + old)
        member_declarations = re.sub(pattern, new, member_declarations)
        declared_names.append(old)
    if len([line for line in member_declarations.splitlines() if line.strip()]) != 12:
        raise ValueError("expected exactly twelve existing state declarations")
    restored = member_declarations
    for old, new in STATE:
        restored = re.sub(rf"\b{new}\b", old, restored)
    if tokens(restored) != tokens(declarations):
        raise ValueError("state declaration reverse-binding proof failed")
    aliases = "".join(f"        auto& {old} = m_state.{new};\n" for old, new in STATE)
    pieces = (("reset", reset, rs, re_), ("presentation", presentation, ps, pe),
              ("scheduled", scheduled, start, end))
    emitted = "".join(textwrap.indent(textwrap.dedent(body), "        ") for _, body, _, _ in pieces)
    if tokens(emitted) != tokens(reset + presentation + scheduled):
        raise ValueError("emitted body tokens changed")
    phase_body = scheduled[len("                if (m_doggebi_ritual.has_value()) {\n"):-len("                }\n")]
    phase_starts = [m.start() for m in re.finditer("^" + re.escape("                    if (doggebi_lord_id != 0) {\n"), phase_body, re.MULTILINE)]
    if len(phase_starts) != 2 or phase_starts[0] != 0:
        raise ValueError("expected two consecutive lord phase boundaries")
    progression = phase_body.index("                    const DoggebiRitualTickResult ritual_tick = m_doggebi_ritual->tick(now_ms);\n")
    phase_ranges = (("ascension", 0, phase_starts[1]),
                    ("combat", phase_starts[1], progression),
                    ("ritual_progression", progression, len(phase_body)))
    evidence = {
        "status": "source-processing draft only; not compiled or executed as gameplay",
        "source_binding": "checked-out Host source, no Git or deep-history lookup",
        "host_sha256": digest(source), "scheduled_sha256": digest(scheduled),
        "clock_binding": "tick parameter named steady_ms_now shadows only the presentation closure's one direct read; previously defined downstream helpers retain their own clocks",
        "state_aliases": dict(STATE), "state_defaults_reverse_tokens_equal": True,
        "regions": {name: {"lines": [source.count('\n',0,begin)+1,source.count('\n',0,finish)],
                            "raw_sha256": digest(body),"tokens":len(tokens(body)),
                            "emitted_tokens_identical":True}
                    for name,body,begin,finish in pieces},
        "phases":{name:{"raw_sha256":digest(phase_body[begin:finish]),
                         "tokens":len(tokens(phase_body[begin:finish]))}
                  for name,begin,finish in phase_ranges},
        "all_shared_references":{old:[source.count('\n',0,m.start())+1 for m in re.finditer(rf'\b{old}\b',source)] for old,_ in STATE},
        "limits":["No compiler proof or executed coverage.","Source pin must be reviewed after C7 before integration.","The old adapter is not an independent expected-value oracle."]}
    gpl = source[:source.index("*/")+2]
    output = gpl + "\n\n// Generated from pinned project-owned source. Temporary old-behavior bridge.\nnamespace h5_baseline {\n"
    output += "struct EncounterState {\n" + textwrap.indent(member_declarations,"    ") + "};\n\n"
    output += """/** Borrows real owned fixture state without copying any live ClientSession. */
class Bridge final {
  public:
    /** Binds the explicit fixture references used by the member names.
     * @param state Owned controller values.
     * @param world Owned world.
     * @param clients Stable owned client vector.
     * @param mutex Existing world lock.
     * @param next_id Existing entity allocator.
     * @param ritual Owned optional domain state.
     */
    Bridge(EncounterState& state, GameWorld& world, std::vector<ClientSession>& clients,
           std::mutex& mutex, std::uint32_t& next_id, std::optional<DoggebiRitual>& ritual)
        : m_state(state), m_world(world), m_clients(clients), m_world_mutex(mutex),
          m_next_entity_id(next_id), m_doggebi_ritual(ritual) {}

    /** Runs the exact old scheduled statements and local closures.
     * @param now_ms Previous outer scheduler timestamp.
     * @param steady_ms_now Scripted clock only for direct presentation reads.
     */
    void tick(std::uint64_t now_ms, std::uint64_t (*steady_ms_now)()) {
        auto& clients = m_clients;
"""
    output += aliases + emitted
    output += """    }
  private:
    EncounterState& m_state;
    GameWorld& m_world;
    std::vector<ClientSession>& m_clients;
    std::mutex& m_world_mutex;
    std::uint32_t& m_next_entity_id;
    std::optional<DoggebiRitual>& m_doggebi_ritual;
};
}  // namespace h5_baseline
"""
    return output, evidence

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--evidence",type=Path,required=True)
    args=parser.parse_args()
    output,evidence=generate(args.source.read_text())
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.evidence.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(output)
    args.evidence.write_text(json.dumps(evidence,indent=2)+'\n')
if __name__ == "__main__":
    main()
