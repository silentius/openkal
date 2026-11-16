#!/usr/bin/env python3
# OpenKal - an open source KalOnline client and server.
# Copyright (C) 2026 OpenKal contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

"""Audit quest references without changing curated configuration.

Reachability is structural, not proof that a condition can be satisfied in game.
NPC quest/HTML entries are roots. Journals and monster drops are assignments but
not interaction roots. Explicit client-script roots can be supplied with --root.
"""
import argparse
import collections
import json
from pathlib import Path


def audit(root, extra_roots=()):
    data = {}
    paths = {}
    for kind in ('quests', 'dialogs', 'tasks', 'npcs', 'monsters', 'items', 'groups', 'itemgroups'):
        data[kind], paths[kind] = {}, {}
        for path in sorted((root / 'config' / kind).rglob('*.json')):
            value = json.loads(path.read_text())
            if kind == 'quests':
                key = (value['index'], value['flag'])
            elif kind == 'tasks':
                key = value['key']
            else:
                key = value.get('index', value.get('id'))
            if key is None:
                continue
            data[kind][key] = value
            paths[kind][key] = str(path.relative_to(root))
    edges = collections.defaultdict(set)
    inbound = collections.defaultdict(set)
    issues = collections.defaultdict(list)
    roots = set(extra_roots)
    assignments = collections.defaultdict(set)
    qnode = lambda key: 'quest:%s/%s' % tuple(key)
    dnode = lambda key: f'dialog:{key}'
    existing = {qnode(k) for k in data['quests']} | {dnode(k) for k in data['dialogs']}

    def edge(source, target):
        if target in edges[source]:
            return
        edges[source].add(target)
        inbound[target].add(source)
        if target not in existing:
            issues['missing_targets'].append({'source': source, 'target': target})

    supported_conditions = {'class', 'level', 'specialty', 'quest', 'clear', 'notclear', 'gstate', 'item'}
    supported_actions = {'html', 'link', 'save', 'clear', 'exp', 'contribute', 'supoint', 'specialty', 'teleport', 'gstate', 'item', 'itemrand'}
    for key, quest in data['quests'].items():
        source = qnode(key)
        if not (0 <= key[0] <= 65535 and 0 <= key[1] <= 255):
            issues['invalid_protocol_keys'].append(source)
        for case in quest['cases']:
            for condition in case['if']:
                if 'quest' in condition:
                    assignments[condition['quest']].add(source)
                if condition['op'] not in supported_conditions or 'raw' in condition:
                    issues['unsupported_conditions'].append({'source': source, 'condition': condition})
            for action in case['then']:
                op = action['op']
                if op == 'link':
                    edge(source, qnode((action['quest'], action['flag'])))
                elif op == 'html' and action.get('id', 0) > 0:
                    edge(source, dnode(action['id']))
                if 'quest' in action:
                    assignments[action['quest']].add(source)
                if op not in supported_actions or 'raw' in action:
                    issues['unsupported_actions'].append({'source': source, 'action': action})
                if op == 'item':
                    for field, width in [('in', 3), ('out', 2)]:
                        for row in action.get(field, []):
                            if len(row) != width or row[-1] <= 0:
                                issues['malformed_items'].append({'source': source, field: row})
                            elif row[0] not in data['items']:
                                issues['missing_items'].append({'source': source, 'item': row[0]})
        if quest['cases'] == [{'if': [], 'then': [{'op': 'html', 'raw': []}]}]:
            issues['placeholder_steps'].append(source)
    for key, dialog in data['dialogs'].items():
        source = dnode(key)
        if not any(line.strip() for line in dialog.get('lines', [])) and not dialog.get('links'):
            issues['empty_dialogs'].append(source)
        for link in dialog.get('links', []):
            if link.get('type') == 'quest':
                edge(source, qnode(link['quest']))
                assignments[link['quest'][0]].add(source)
            elif str(link.get('type', '')).isdigit():
                edge(source, dnode(int(link['type'])))
    for key, npc in data['npcs'].items():
        source = f'npc:{key}'
        # Mirror Host's quest-vs-HTML selection, including fallback quest NPCs.
        quest = npc.get('quest', [0, 0])
        html = npc.get('html', 0)
        if quest != [0, 0]:
            assignments[quest[0]].add(source)
        if npc.get('kind') == 3 and quest != [0, 0]:
            target = qnode(quest)
        elif html:
            target = dnode(html)
        elif quest != [0, 0]:
            target = qnode(quest)
        else:
            continue
        roots.add(target)
        edge(source, target)
        if target.startswith('quest:') and tuple(quest) in data['quests'] and data['quests'][tuple(quest)]['linked']:
            issues['npc_linked_steps'].append({'npc': key, 'target': target})
    for key, task in data['tasks'].items():
        assignments[key].add(f'journal:{key}')
        if not any(q[0] == key for q in data['quests']):
            issues['journals_without_quest'].append(key)
        for stage in task.get('flags', []):
            npc = stage.get('npcIndex', 0)
            if npc and npc not in data['npcs']:
                issues['journal_missing_npcs'].append({'quest': key, 'stage': stage['key'], 'npc': npc})
    for key, monster in data['monsters'].items():
        rows = monster.get('stats', {}).get('quests', [])
        if rows and not isinstance(rows[0], list):
            rows = [rows]
        for row in rows:
            assignments[row[0]].add(f'monster:{key}')
            if row[2] not in data['itemgroups'] or not data['itemgroups'][row[2]].get('entries'):
                issues['missing_drop_tables'].append({'monster': key, 'quest': row[0], 'flag': row[1], 'itemgroup': row[2]})
    reachable = set()
    pending = list(roots)
    while pending:
        node = pending.pop()
        if node in reachable:
            continue
        reachable.add(node)
        pending.extend(edges[node] - reachable)
    quest_ids = sorted({k[0] for k in data['quests']})
    unreachable_quests = [k for k in quest_ids if not any(qnode(q) in reachable for q in data['quests'] if q[0] == k)]
    unassigned_quests = [k for k in unreachable_quests if not (assignments[k] - {qnode(q) for q in data['quests'] if q[0] == k})]
    issues['unreachable_quest_ids'] = unreachable_quests
    issues['unassigned_quest_ids'] = unassigned_quests
    issues['unreachable_dialog_ids'] = sorted(k for k in data['dialogs'] if dnode(k) not in reachable)
    issues['dialog_ids_without_inbound_references'] = sorted(k for k in data['dialogs'] if not inbound[dnode(k)] and dnode(k) not in roots)
    issues['reachable_missing_targets'] = [v for v in issues['missing_targets'] if v['source'] in reachable or v['source'].startswith('npc:')]
    issues['reachable_unsupported_actions'] = [v for v in issues['unsupported_actions'] if v['source'] in reachable]
    return {'counts': {k: len(v) for k, v in data.items()}, 'issue_counts': {k: len(v) for k, v in issues.items()}, 'issues': dict(issues), 'reachable': sorted(reachable), 'scope': 'All config JSON; NPC interaction roots. Conditions, active map availability, externally triggered UI scripts, and server events are not simulated.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--output', type=Path)
    parser.add_argument('--root', action='append', default=[], help='Additional root, e.g. quest:6001/1')
    args = parser.parse_args()
    result = audit(args.repo, args.root)
    print(json.dumps({'counts': result['counts'], 'issue_counts': result['issue_counts']}, indent=2))
    if args.output:
        args.output.write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()
