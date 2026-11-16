#!/usr/bin/env python3
"""Generate strictly pinned project-owned event delivery, without Git history."""
import argparse,hashlib,json,textwrap
from pathlib import Path
HOST='81097a2e7b2feb74d49233d8ebe92a2ee8d1f225a0a4d07c7f493cfa0c305735'
PHASE='459f6afd7362e96fd633766d4f4724f30c674452e735c954af0c914831c7f5ac'
sha=lambda s:hashlib.sha256(s.encode()).hexdigest()
def generate(s):
 if sha(s)!=HOST:raise ValueError('Host changed; explicitly rebase after tested H5-H8')
 start='                for (const auto& cev : chests) {\n'
 assert s.count(start)==1
 a=s.index(start);b=s.index('            }\n            if (runtime_metrics) {\n',a)
 phase=s[a:b];raw_phase=phase
 assert sha(raw_phase)=='3437ac36fc312944f8d32f55fd3ff92be2f74f02c4326e62fc15967aa137fe30'
 assert raw_phase.count('doggebi.lord_id')==1 and 'doggebi_lord_id' not in raw_phase
 phase=raw_phase.replace('doggebi.lord_id','doggebi_lord_id')
 assert sha(phase)==PHASE
 despawn=phase.index('                for (const std::uint32_t id : despawned_items) {\n')
 lambda_start=phase.index('                auto broadcast_monster_moves = [&](const std::vector<MonsterMoveEvent>& evs) {\n')
 move_start=phase.index('                    for (const auto& ev : evs) {\n',lambda_start)
 move_end=phase.index('                };\n',move_start)
 attack_start=phase.index('                for (const auto& ev : attacks) {\n',move_end)
 sections={'chests':(phase[:despawn],a,a+despawn),'despawns':(phase[despawn:lambda_start],a+despawn,a+lambda_start),'moves':(phase[move_start:move_end],a+move_start,a+move_end),'attacks':(phase[attack_start:],a+attack_start,b)}
 out=s[:s.index('*/')+2]+'''\n\n// Generated from pinned project-owned source; temporary old execution only.
namespace h9_baseline {
class Bridge final {
  public:
    Bridge(std::vector<ClientSession>& clients, GameWorld& world, std::mutex& mutex,
           std::atomic<std::uint32_t>& next_iid, std::unique_ptr<IDataStore>& store,
           const ServerConfig& config)
        : m_clients(clients), m_world(world), m_world_mutex(mutex), m_next_iid(next_iid),
          m_store(store), m_config(config) {}
    void chests(const std::vector<ChestOpenEvent>& chests) {
        auto& clients = m_clients;
'''+textwrap.indent(textwrap.dedent(sections['chests'][0]),'        ')+'''    }
    void despawns(const std::vector<std::uint32_t>& despawned_items) {
        auto& clients = m_clients;
'''+textwrap.indent(textwrap.dedent(sections['despawns'][0]),'        ')+'''    }
    void moves(const std::vector<MonsterMoveEvent>& evs) {
        auto& clients = m_clients;
'''+textwrap.indent(textwrap.dedent(sections['moves'][0]),'        ')+'''    }
    void attacks(const std::vector<MonsterAttackEvent>& attacks, std::uint32_t doggebi_lord_id) {
        auto& clients = m_clients;
        auto broadcast_monster_moves = [&](const std::vector<MonsterMoveEvent>& evs) {
            moves(evs);
        };
'''+textwrap.indent(textwrap.dedent(sections['attacks'][0]),'        ')+'''    }
  private:
    std::vector<ClientSession>& m_clients;
    GameWorld& m_world;
    std::mutex& m_world_mutex;
    std::atomic<std::uint32_t>& m_next_iid;
    std::unique_ptr<IDataStore>& m_store;
    const ServerConfig& m_config;
};
}  // namespace h9_baseline
'''
 skeleton=phase
 for name,(body,_,__) in sections.items():skeleton=skeleton.replace(body,'/* H9_'+name.upper()+'_BODY */\n',1)
 evidence={'status':'source_only_UNCOMPILED_UNRUN','source_sha256':sha(s),'phase_sha256':PHASE,'raw_current_phase_sha256':sha(raw_phase),'prior_H5_binding_reversed':{'from':'doggebi.lord_id','to':'doggebi_lord_id','occurrences':1},'phase_lines':[s.count('\n',0,a)+1,s.count('\n',0,b)],'sections':{n:{'sha256':sha(t),'lines':[s.count('\n',0,x)+1,s.count('\n',0,y)],'bytes':len(t)} for n,(t,x,y) in sections.items()},'coordinator_skeleton':skeleton,'bindings':['borrowed clients/world/mutex/IID/unique_ptr store/const config','same event vector names bound as const borrowed parameters','lord ID copied at attack call, no mutation','old shared move body becomes moves(evs); new local attack adapter forwards to that exact operation'],'clock_bindings':[],'no_formula_or_control_flow_substitution':True,'no_Git_history_lookup':True,'required_rebase':'After tested H5-H8; do not overwrite newer test/Host source with old-base drafts.'}
 return out,evidence
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--evidence',type=Path,required=True);a=p.parse_args();out,proof=generate(a.source.read_text());a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(out);a.evidence.write_text(json.dumps(proof,indent=2)+'\n')
if __name__=='__main__':main()
