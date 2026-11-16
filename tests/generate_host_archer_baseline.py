#!/usr/bin/env python3
"""Generate a strictly pinned old-source Archer baseline adapter, without Git."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import textwrap
HOST_SHA='8ac1179364305dbff8482a64fb6aaa01f82830b4063f6efb23f4e7ec12c7234b'
def sha(s):return hashlib.sha256(s.encode()).hexdigest()
def select(source,a,b):
 if source.count(a)!=1:raise ValueError('start anchor not unique: '+a.strip())
 start=source.index(a)
 if source.count(b,start)!=1:raise ValueError('end anchor not unique: '+b.strip())
 end=source.index(b,start)
 return source[start:end],start,end
def generate(source):
 if sha(source)!=HOST_SHA:raise ValueError('Host differs from pinned pre-H6 source; rebase after H5 explicitly')
 hit,hs,he=select(source,'                struct ArcherTickHit {\n','                std::vector<ArcherTickHit> archer_tick_hits;\n')
 live,ls,le=select(source,'                const auto find_live_player =\n','                for (auto status_it = archer_timed_damage.begin();\n')
 buff,bs,be=select(source,'        auto find_player_buff_target = [&](std::uint32_t player_id) -> PlayerBuffTarget {\n','        auto send_dummy_creates = [&](ClientSession& dest) {\n')
 timed,ts,te=select(source,'                for (auto status_it = archer_timed_damage.begin();\n','                constexpr int kPoisonCloudTickMs = protocol::kArcherPoisonCloudTickMs;\n')
 areas,ars,are=select(source,'                constexpr int kPoisonCloudTickMs = protocol::kArcherPoisonCloudTickMs;\n','                for (auto boom_it = archer_explosions.begin();\n')
 booms,es,ee=select(source,'                for (auto boom_it = archer_explosions.begin();\n','                for (const ArcherTickHit& hit : archer_tick_hits) {\n')
 deliver,ds,de=select(source,'                for (const ArcherTickHit& hit : archer_tick_hits) {\n','                struct PersistentAreaHit {\n')
 entire=source[hs:de]
 if sha(entire)!='9171190511971e1ff484ccc90261fb77e6bf2a77bfe8d122028fc027baefdce9':raise ValueError('Archer region changed')
 sections={'hit_type':(hit,hs,he),'live_lookup':(live,ls,le),'player_or_dummy_lookup':(buff,bs,be),'timed':(timed,ts,te),'areas':(areas,ars,are),'explosions':(booms,es,ee),'delivery':(deliver,ds,de)}
 gpl=source[:source.index('*/')+2]
 out=gpl+'\n\n// Generated from pinned project-owned source. Temporary old execution adapter.\nnamespace h6_baseline {\n'
 out+=textwrap.dedent(hit)+'\n'
 out+='''class Bridge final {
  public:
    Bridge(std::vector<ClientSession>& clients, GameWorld& world, std::mutex& mutex,
           DebugDummyPlayers& dummies, std::atomic<std::uint32_t>& next_iid,
           std::unique_ptr<IDataStore>& store, std::vector<ArcherTimedDamage>& timed,
           std::vector<ArcherPersistentArea>& areas, std::vector<ArcherArrowExplosion>& explosions)
        : m_clients(clients), m_world(world), m_world_mutex(mutex), m_dummies(dummies),
          m_next_iid(next_iid), m_store(store), m_timed(timed), m_areas(areas), m_explosions(explosions) {}

    void produce(std::uint64_t now_ms, std::vector<ArcherTickHit>& hits) {
        auto& clients = m_clients;
        auto& debug_dummies = m_dummies;
        auto& archer_timed_damage = m_timed;
        auto& archer_areas = m_areas;
        auto& archer_explosions = m_explosions;
        auto& archer_tick_hits = hits;
'''
 for section in [buff,live,timed,areas,booms]:out+=textwrap.indent(textwrap.dedent(section),'        ')
 out+='''    }

    void deliver(const std::vector<ArcherTickHit>& hits) {
        auto& clients = m_clients;
        const auto& archer_tick_hits = hits;
'''
 out+=textwrap.indent(textwrap.dedent(deliver),'        ')
 out+='''    }

  private:
    std::vector<ClientSession>& m_clients;
    GameWorld& m_world;
    std::mutex& m_world_mutex;
    DebugDummyPlayers& m_dummies;
    std::atomic<std::uint32_t>& m_next_iid;
    std::unique_ptr<IDataStore>& m_store;
    std::vector<ArcherTimedDamage>& m_timed;
    std::vector<ArcherPersistentArea>& m_areas;
    std::vector<ArcherArrowExplosion>& m_explosions;
};
}  // namespace h6_baseline
'''
 evidence={'mode':'Source/text processing only; no compiler/gameplay/runtime claim','source_sha256':sha(source),'entire_phase_sha256':sha(entire),'sections':{name:{'sha256':sha(body),'lines':[source.count('\n',0,start)+1,source.count('\n',0,end)],'bytes':len(body)} for name,(body,start,end) in sections.items()},'bindings':['clients reference','debug_dummies reference','three persistent vectors by reference','caller-owned hits vector by reference','world/mutex/IID/store borrowed member names'],'clock_bindings':[],'no_formula_or_control_flow_substitutions':True,'hit_vector_owner':'Caller, outside both produce and deliver; no copies of ClientSession/world','source_only_no_git':True,'must_rebase_after_tested_H5':True}
 return out,evidence
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--evidence',type=Path,required=True);a=p.parse_args()
 out,evidence=generate(a.source.read_text());a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(out);a.evidence.write_text(json.dumps(evidence,indent=2)+'\n')
if __name__=='__main__':main()
