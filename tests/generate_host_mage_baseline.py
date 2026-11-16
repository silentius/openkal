#!/usr/bin/env python3
"""Generate a strictly pinned old Mage phase adapter from checked-out source only."""
import argparse,hashlib,json,textwrap
from pathlib import Path
HOST='d6cd93b91a8a3edeae94484c48195b936b90807362fc14eb7507a50099b37e78'
PHASE='763d691344f55daf1dfd08f2006296d4ec6bcb100ff209c7c6f968792e29dbfc'
sha=lambda s:hashlib.sha256(s.encode()).hexdigest()
def generate(s):
 if sha(s)!=HOST:raise ValueError('Host changed; explicitly rebase after tested H5/H6')
 def region(a,b,start=0):
  if s.count(a,start)!=1:raise ValueError('nonunique start '+a.strip())
  lo=s.index(a,start)
  if s.count(b,lo)!=1:raise ValueError('nonunique end '+b.strip())
  hi=s.index(b,lo);return s[lo:hi],lo,hi
 hit,hs,he=region('                struct PersistentAreaHit {\n','                std::vector<PersistentAreaHit> persistent_hits;\n')
 phase_start=hs
 mark_start=he+len('                std::vector<PersistentAreaHit> persistent_hits;\n                std::vector<MonsterMoveEvent> persistent_moves;\n')
 assert s[mark_start:mark_start+18]=='                {\n'
 area_start=s.index('                for (auto area_it = mage_areas.begin();',mark_start)
 delivery_start=s.index('                for (const PersistentAreaHit& hit : persistent_hits)',area_start)
 end=s.index('                std::vector<PlayerPose> poses;\n',delivery_start)
 marks=s[mark_start:area_start];areas=s[area_start:delivery_start];delivery=s[delivery_start:end]
 live,ls,le=region('                const auto find_live_player =\n','                tick_archer_timed_damage(\n')
 assert sha(s[phase_start:end])==PHASE
 sections={'hit_type':(hit,hs,he),'live_lookup':(live,ls,le),'marks':(marks,mark_start,area_start),'areas':(areas,area_start,delivery_start),'delivery':(delivery,delivery_start,end)}
 gpl=s[:s.index('*/')+2]
 out=gpl+'\n\n// Generated from pinned project-owned source; temporary old execution only.\nnamespace h7_baseline {\n'+textwrap.dedent(hit)+'\n'
 out+='''class Bridge final {
  public:
    Bridge(std::vector<ClientSession>& clients, GameWorld& world, std::mutex& mutex,
           std::atomic<std::uint32_t>& next_iid, std::unique_ptr<IDataStore>& store,
           std::vector<MagePersistentArea>& areas, std::vector<MageLightningMark>& marks)
        : m_clients(clients), m_world(world), m_world_mutex(mutex), m_next_iid(next_iid),
          m_store(store), m_areas(areas), m_marks(marks) {}

    void prune(std::uint64_t now_ms) {
        auto& clients = m_clients;
        auto& lightning_marks = m_marks;
'''
 out+=textwrap.indent(textwrap.dedent(live+marks),'        ')
 out+='''    }
    void produce(std::uint64_t now_ms, std::vector<PersistentAreaHit>& hits,
                 std::vector<MonsterMoveEvent>& moves) {
        auto& clients = m_clients;
        auto& mage_areas = m_areas;
        auto& persistent_hits = hits;
        auto& persistent_moves = moves;
'''+textwrap.indent(textwrap.dedent(areas),'        ')
 out+='''    }
    void deliver(const std::vector<PersistentAreaHit>& hits,
                 const std::vector<MonsterMoveEvent>& moves) {
        auto& clients = m_clients;
        const auto& persistent_hits = hits;
        const auto& persistent_moves = moves;
'''+textwrap.indent(textwrap.dedent(delivery),'        ')
 out+='''    }
  private:
    std::vector<ClientSession>& m_clients;
    GameWorld& m_world;
    std::mutex& m_world_mutex;
    std::atomic<std::uint32_t>& m_next_iid;
    std::unique_ptr<IDataStore>& m_store;
    std::vector<MagePersistentArea>& m_areas;
    std::vector<MageLightningMark>& m_marks;
};
}  // namespace h7_baseline
'''
 evidence={'status':'source_processing_only_not_compiled_or_run','source_sha256':sha(s),'phase_sha256':PHASE,'phase_lines':[s.count('\n',0,phase_start)+1,s.count('\n',0,end)],'sections':{n:{'sha256':sha(t),'lines':[s.count('\n',0,a)+1,s.count('\n',0,b)],'bytes':len(t)} for n,(t,a,b) in sections.items()},'bindings':['borrowed clients/world/mutex/IID/unique_ptr store','borrowed startup areas/marks vectors','caller-owned hits then moves vectors retained through all calls'],'clock_bindings':[],'Unix_clock_policy':'Exact old lazy per-target66 unix_now_sec call retained; test brackets affected five-second expiries, no new production seam.','no_formula_or_control_flow_substitution':True,'no_Git_history_lookup':True,'mandatory_rebase_after_H5_H6':True}
 return out,evidence
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--evidence',type=Path,required=True);a=p.parse_args()
 o,e=generate(a.source.read_text());a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(o);a.evidence.write_text(json.dumps(e,indent=2)+'\n')
if __name__=='__main__':main()
