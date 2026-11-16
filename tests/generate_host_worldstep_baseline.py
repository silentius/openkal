#!/usr/bin/env python3
"""Generate strictly pinned old world-step operations from checked-out Host source."""
import argparse,hashlib,json,textwrap
from pathlib import Path
HOST='e45b2a1f27e2593b27bfff123252ea38a4bc177a40898f6c253f9a6e02b2f0bb'
PHASE='a388bfa1d73d83663dd8285ffb537d6a49f6c7d3e88550d53ce2bdfb44f39396'
sha=lambda s:hashlib.sha256(s.encode()).hexdigest()
def generate(s):
 if sha(s)!=HOST:raise ValueError('Host changed: explicitly rebase after tested H5-H7')
 a=s.index('                std::vector<PlayerPose> poses;\n')
 b=s.index('                for (const auto& cev : chests) {\n',a)
 phase=s[a:b]
 # H8 ends with the old lock, immediately before chest-event consumption.
 assert sha(phase)==PHASE
 def cut(start,end):
  assert phase.count(start)==1 and phase.count(end)==1
  lo=phase.index(start);hi=phase.index(end,lo);return phase[lo:hi],a+lo,a+hi
 collect,cs,ce=cut('                for (const auto& c : clients) {\n','                std::vector<MonsterMoveEvent> moves;\n')
 refill,rs,re=cut('                    for (auto& c : clients) {\n                        if (c.phase != SessionPhase::Playing || c.player_id == 0) {\n','                    harvest_plants.update(\n')
 publish,ps,pe=cut('                    for (auto& monster : m_world.monsters()) {\n','                    const auto viewer_count =\n')
 sync_start=phase.index('                    const auto viewer_count =\n')
 # Remove only the enclosing coordinator lock's final brace, not the sync loop.
 sync_end=len(phase)-len('                }\n')
 assert phase.endswith('                    }\n                }\n')
 sync=phase[sync_start:sync_end]
 sections={'collect':(collect,cs,ce),'refill':(refill,rs,re),'publish':(publish,ps,pe),'sync':(sync,a+sync_start,a+sync_end)}
 out=s[:s.index('*/')+2]+'\n\n// Generated from pinned project-owned source; temporary old operations only.\nnamespace h8_baseline {\n'
 signatures={
 'collect':'void collect(const std::vector<ClientSession>& clients, std::uint64_t now_ms, std::vector<PlayerPose>& poses)',
 'refill':'void refill(std::vector<ClientSession>& clients, GameWorld& m_world, const std::vector<PlayerPose>& viewers, const ServerConfig& m_config, int radius, std::uint32_t& m_next_entity_id, std::uint64_t now_ms)',
 'publish':'void publish(GameWorld& m_world, std::vector<ClientSession>& clients, std::vector<MonsterCorpseEvent>& corpses)',
 'sync':'void sync(std::vector<ClientSession>& clients, GameWorld& m_world, const NpcRegistry& m_npcs, WorldInterestIndex& world_interest)'}
 for name,(body,_,__) in sections.items():out+='    '+signatures[name]+' {\n'+textwrap.indent(textwrap.dedent(body),'        ')+'    }\n'
 out+='}  // namespace h8_baseline\n'
 skeleton=phase
 for name,(body,_,__) in sections.items():skeleton=skeleton.replace(body,'/* H8_'+name.upper()+'_BODY */\n',1)
 evidence={'status':'source_only_no_Cpp_build_or_runtime','Host_sha256':sha(s),'phase_sha256':PHASE,'phase_lines':[s.count('\n',0,a)+1,s.count('\n',0,b)],'sections':{n:{'sha256':sha(t),'lines':[s.count('\n',0,lo)+1,s.count('\n',0,hi)],'bytes':len(t)} for n,(t,lo,hi) in sections.items()},'coordinator_skeleton':skeleton,'clock_bindings':[],'no_formulas_or_control_flow_substitutions':True,'source_only_no_Git_lookup':True,'borrowed_owners':['clients/world/config/viewers','corpses cleared in publication','caller poses and five event vectors','startup world_interest and NPC registry'],'mandatory_rebase':'After tested H5-H7, including all intervening fixture insertions.'}
 return out,evidence
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--evidence',type=Path,required=True);a=p.parse_args();o,e=generate(a.source.read_text());a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(o);a.evidence.write_text(json.dumps(e,indent=2)+'\n')
if __name__=='__main__':main()
