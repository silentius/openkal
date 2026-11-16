#!/usr/bin/env python3
"""Audit a skin catalog and optional local name references without changing sources.

External names and model inspection results are written only to a new local
output directory. The C++ probe uses OpenKal's normal model and texture readers.
"""

import argparse
import collections
import csv
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

from skin_catalog import (apply_descriptive_name, apply_weapon_compatibility, knight_reference_types,
                          knight_weapon_type,
                          load_descriptive_name_catalog, load_name_catalog, load_weapon_templates,
                          load_weapon_compatibility, matches_name_model, weapon_skin_type)


REPO = Path(__file__).resolve().parent.parent
JOBS = {"knight": ("w", "s"), "magician": ("r", "g"),
        "archer": ("m", "b"), "thief": ("t", "d")}
ALIASES = {"mage": "magician", **{job: job for job in JOBS}}
PARTS = {"helmet": "h", "upper": "a", "pants": "p", "boots": "s",
         "gauntlet": "g", "face": "f"}
TOKEN = re.compile(r'"(?:\\.|[^"\\])*"|;[^\n]*|[()]|[^\s();]+')


def write_json(path, value):
    """Keep numeric arrays on one line, including negative bounds and floats."""
    text = json.dumps(value, indent=2, ensure_ascii=False, allow_nan=False)
    numeric = r'-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?'
    pattern = rf'\[\s*({numeric}(?:\s*,\s*{numeric})*)\s*\]'
    text = re.sub(pattern, lambda m: '[' + ', '.join(re.split(r'\s*,\s*', m[1])) + ']', text)
    path.write_text(text + '\n', encoding='utf-8')


def clean_reference_name(text):
    text = re.sub(r'#[0-9bBnN]', '', text)
    text = re.sub(r'[\[(](?:knight|kinght|archer|mage|magician|thief)[\])]', '', text,
                  flags=re.I)
    text = re.sub(r'\(event\)', '', text, flags=re.I)
    text = re.sub(r'\s+', ' ', text).strip(' .')
    text = re.sub(r'\bcostume\b', 'Costume', text, flags=re.I)
    return text[:1].upper() + text[1:]


def read_name_reference(path):
    """Read item data only; skip commented records and never execute source text."""
    raw = path.read_bytes()
    if raw.startswith((b'\xff\xfe', b'\xfe\xff')):
        text = raw.decode('utf-16')
    else:
        try:
            text = raw.decode('utf-8-sig')
        except UnicodeDecodeError:
            text = raw.decode('cp1252')
    provenance = {"file": path.name, "sha256": hashlib.sha256(raw).hexdigest(),
                  "read_at": datetime.now(timezone.utc).isoformat(timespec='seconds'),
                  "origin": "user-supplied local item configuration",
                  "author": "not established", "license": "not established; local use only"}
    stack = []
    comment = ''
    record_comment = ''
    record_line = 0
    line = 1
    previous_end = 0
    records = []
    for match in TOKEN.finditer(text):
        line += text.count('\n', previous_end, match.start())
        token = match[0]
        if token.startswith(';'):
            if not stack:
                candidate = token[1:].strip()
                comment = '' if candidate.startswith('(item') else candidate
        elif token == '(':
            if not stack:
                record_comment, record_line = comment, line
                comment = ''
            stack.append([])
        elif token == ')':
            if not stack:
                raise ValueError(f'{path.name}:{line}: unmatched closing parenthesis')
            node = stack.pop()
            if stack:
                stack[-1].append(node)
            elif node and str(node[0]).lower() == 'item':
                fields = {str(field[0]).lower(): field[1:] for field in node[1:]
                          if isinstance(field, list) and field}
                clazz = [str(v).lower() for v in fields.get('class', [])]
                limit = fields.get('limit', [])
                action = fields.get('action', [])
                inline = fields.get('name', [])
                name = inline[0] if inline and not str(inline[0]).isdigit() else record_comment
                if len(action) == 2 and limit and name:
                    job = ALIASES.get(str(limit[0]).lower())
                    part = None
                    if clazz == ['defense', 'cost']:
                        part = 'costume'
                    elif clazz[:1] == ['weapon']:
                        part = 'weapon'
                    # Ordinary armor is not a costume-name reference.
                    if job and part:
                        try:
                            group, variant = map(int, action)
                        except (ValueError, TypeError):
                            continue
                        cleaned = clean_reference_name(str(name))
                        if cleaned:
                            records.append({"key": (job, part, group, variant), "name": cleaned,
                                            "line": record_line, **provenance})
        elif stack:
            if token.startswith('"'):
                token = token[1:-1].replace('\\"', '"').replace('\\\\', '\\')
            stack[-1].append(token)
        line += token.count('\n')
        previous_end = match.end()
    if stack:
        raise ValueError(f'{path.name}:{record_line}: unterminated item record')
    return records


def reference_key(definition):
    job = ALIASES.get(definition.get('limit', {}).get('class'))
    part = definition.get('class', ['', ''])[1]
    costume = part == 'body' or definition.get('skin', {}).get('costume_mesh') is True
    return (job, 'costume' if costume else part, *definition.get('action', []))


def validate_definition(path, definition, policy):
    errors = []
    index = definition.get('index')
    if type(index) is not int or not policy['reserved_range'][0] <= index < policy['next_id']:
        errors.append('invalid_or_unallocated_id')
    elif any(first <= index <= last for first, last in policy['withdrawn_ranges']):
        errors.append('withdrawn_id_reused')
    if path.name.split('_')[0] != str(index):
        errors.append('filename_index_mismatch')
    clazz = definition.get('class')
    if not isinstance(clazz, list) or len(clazz) != 2 or clazz[0] != 'skin' or \
            clazz[1] not in {*PARTS, 'body', 'weapon', 'shield'}:
        errors.append('unsupported_skin_slot')
    job = definition.get('limit', {}).get('class')
    if job not in JOBS:
        errors.append('unsupported_class')
    action = definition.get('action')
    if not isinstance(action, list) or len(action) != 2 or \
            any(type(v) is not int for v in action) or action[0] < 0 or action[1] < 1:
        errors.append('invalid_model_coordinates')
    if definition.get('wear') != 1 or definition.get('stackable', 0) != 0:
        errors.append('invalid_equipment_flags')
    appearance = definition.get('skin', {})
    if not isinstance(appearance, dict) or any(
            key in appearance and type(appearance[key]) is not bool
            for key in ('costume_mesh', 'keep_hair', 'replace_face')):
        errors.append('invalid_skin_appearance_flags')
    if isinstance(appearance, dict) and 'weapon_type' in appearance and (
            appearance['weapon_type'] not in ('sword', 'sword2h', 'wand', 'bow', 'dagger') or
            clazz != ['skin', 'weapon']):
        errors.append('invalid_skin_weapon_type')
    return errors


def expected_ids(policy):
    allocated = set(range(policy['reserved_range'][0], policy['next_id']))
    for first, last in policy['withdrawn_ranges']:
        allocated.difference_update(range(first, last + 1))
    return allocated


def model_filename(definition):
    job = definition['limit']['class']
    letter, species = JOBS[job]
    part = definition['class'][1]
    group, variant = definition['action']
    if part in ('weapon', 'shield'):
        suffix = 'b' if part == 'shield' else 's'
        return f'Item/Weapon/p{species}_{suffix}{group}_{variant:02d}.gb'
    costume = part == 'body' or definition.get('skin', {}).get('costume_mesh') is True
    return f'Model/Clothes/c{letter}_{group}_{"c" if costume else PARTS[part]}{variant:02d}.gb'


def asset_index(root):
    files = {}
    for directory in ('Model/Clothes', 'Item/Weapon'):
        # Resolve directory casing too, rather than relying on the host filesystem.
        current = root
        for component in directory.split('/'):
            matches = [p for p in current.iterdir() if p.name.lower() == component.lower()]
            if len(matches) != 1 or not matches[0].is_dir():
                raise ValueError(f'missing or ambiguous asset directory: {directory}')
            current = matches[0]
        for path in current.iterdir():
            if path.is_file():
                key = f'{directory}/{path.name}'.lower()
                if key in files:
                    raise ValueError(f'case-ambiguous model: {key}')
                files[key] = path
    return files


def texture_index(root):
    files = collections.defaultdict(list)
    for directory in ('Model/Clothes', 'Item/Weapon'):
        current = root
        for component in directory.split('/'):
            current = next(path for path in current.iterdir()
                           if path.name.lower() == component.lower())
        for path in current.rglob('*'):
            if path.is_file() and path.suffix.lower() in ('.dds', '.gtx', '.png', '.tga', '.bmp'):
                files[path.stem.lower()].append(path.relative_to(root).as_posix())
    return files


def diagnose_textures(inspection, files):
    """Explain runtime failures without substituting or converting external assets."""
    diagnoses = set()
    for texture in inspection.get('textures', []):
        if texture['loaded']:
            continue
        stem = Path(texture['name'].replace('\\', '/')).stem.lower()
        candidates = sorted(files.get(stem, []))
        texture['candidate_files'] = candidates
        if not candidates:
            diagnosis = 'texture_not_found'
        elif all(Path(path).suffix.lower() not in ('.dds', '.gtx', '.tga', '.png') for path in candidates):
            diagnosis = 'unsupported_texture_extension'
        else:
            diagnosis = 'texture_not_resolved_or_decoded'
        texture['diagnosis'] = diagnosis
        diagnoses.add(diagnosis)
    return sorted(diagnoses)


def name_result(definition, references):
    matches = references.get(reference_key(definition), [])
    names = sorted({record['name'] for record in matches})
    if len(names) == 1:
        return {"status": "local_reference_match", "resolved": names[0] +
                f" ({definition['limit']['class'].title()})", "sources": matches}
    if names:
        return {"status": "ambiguous", "candidates": names, "sources": matches}
    part = definition['class'][1]
    sample = part not in ('body', 'weapon') and not definition.get('skin', {}).get('costume_mesh')
    return {"status": "test_sample" if sample else "unresolved",
            "resolved": definition.get('name', '')}


def coverage_warnings(row, references):
    """Coverage is a heuristic for review, never a proof of correct animation."""
    inspection = row['inspection']
    head = references.get('head:' + row['class'], {})
    boots = references.get('boots:' + row['class'], {})
    warnings = []
    if row['part'] == 'body':
        bones = set(inspection.get('bones', []))
        head_bones = set(head.get('bones', []))
        if bones and head_bones and bones <= head_bones:
            warnings.append('body_slot_contains_head_only')
        if head.get('loaded') and boots.get('loaded') and inspection.get('loaded'):
            full_height = head['bounds_max'][1] - boots['bounds_min'][1]
            height = inspection['bounds_max'][1] - inspection['bounds_min'][1]
            if full_height > 0 and (height < full_height * 0.7 or
                                   inspection['bounds_min'][1] >
                                   boots['bounds_min'][1] + full_height * 0.25):
                warnings.append('incomplete_body_coverage')
        else:
            warnings.append('body_reference_unavailable')
        if not bones:
            warnings.append('body_bone_coverage_unknown')
    return warnings


def verify_weapon_template(definition, entry, inspection, reference, expected_type):
    """Check a reviewed type reference against the active asset pack, not a previous audit."""
    if not matches_name_model(definition, entry):
        raise ValueError('weapon template model does not match the definition')
    errors = []
    if not reference.get('loaded'):
        errors.append('missing_weapon_reference')
    elif (reference.get('attachment_shape_meshes', 0) < 1 or
          any(code != 'missing_or_invalid_texture' for code in reference.get('errors', []))):
        errors.append('invalid_weapon_reference')
    shape = inspection.get('attachment_shape_fingerprint')
    matched = bool(shape and inspection.get('loaded') and
                   inspection.get('attachment_shape_meshes', 0) > 0 and
                   reference.get('attachment_shape_meshes') ==
                   inspection.get('attachment_shape_meshes') and
                   shape == reference.get('attachment_shape_fingerprint') and
                   shape == entry.get('expected_shape_fingerprint') and
                   inspection.get('attachment_shape_meshes') == entry.get('expected_shape_meshes'))
    if not matched and not errors:
        errors.append('weapon_reference_geometry_mismatch')
    if not expected_type or weapon_skin_type(definition) != expected_type:
        errors.append('weapon_reference_type_mismatch')
    return {'reference_action': entry['reference_action'], 'reference_type': expected_type,
            'shape_matches': matched}, errors


def verify_weapon_compatibility(definition, entry, inspection):
    """Validate an OpenKal choice against its reviewed model and explicit type."""
    if not matches_name_model(definition, entry):
        raise ValueError('weapon compatibility model does not match the definition')
    errors = []
    matched = bool(inspection.get('loaded') and
                   inspection.get('attachment_shape_fingerprint') ==
                   entry['expected_shape_fingerprint'] and
                   inspection.get('attachment_shape_meshes') == entry['expected_shape_meshes'])
    if not matched:
        errors.append('weapon_compatibility_geometry_mismatch')
    if weapon_skin_type(definition) != entry['weapon_type']:
        errors.append('weapon_compatibility_type_mismatch')
    else:
        annotated = dict(definition)
        apply_weapon_compatibility(annotated, entry)
        if annotated.get('description') != definition.get('description'):
            errors.append('weapon_compatibility_annotation_missing')
    return {'origin': 'openkal_grip_and_pose_review', 'weapon_type': entry['weapon_type'],
            'original_type_verified': False, 'shape_matches': matched,
            'reason': entry['reason']}, errors


def audit(args):
    if args.output.exists() and any(args.output.iterdir()):
        raise ValueError('output directory must be empty; previous audits are preserved')
    policy = json.loads(args.policy.read_text(encoding='utf-8'))
    name_source, curated_names = load_name_catalog(args.names)
    descriptive_source, descriptive_names = load_descriptive_name_catalog(args.descriptive_names)
    template_data, weapon_templates = load_weapon_templates(args.weapon_templates)
    template_types = knight_reference_types(template_data)
    compatibility_source, compatibility = load_weapon_compatibility(args.weapon_compatibility)
    references = collections.defaultdict(list)
    for path in args.legacy_items:
        for record in read_name_reference(path):
            references[record['key']].append(record)
    files = asset_index(args.data_root)
    textures = texture_index(args.data_root)
    rows = []
    definitions = {}
    seen = set()
    for path in sorted(args.items.rglob('*.json')):
        definition = json.loads(path.read_text(encoding='utf-8'))
        if not isinstance(definition, dict):
            raise ValueError(f'{path.name}: item definition must be an object')
        clazz = definition.get('class')
        if not isinstance(clazz, list) or not clazz or clazz[0] != 'skin':
            continue
        errors = validate_definition(path, definition, policy)
        if errors:
            raise ValueError(f"{path.name}: {', '.join(errors)}")
        index = definition.get('index')
        if index in seen:
            raise ValueError(f'duplicate item ID: {index}')
        seen.add(index)
        relative = '' if errors else model_filename(definition)
        resolved = files.get(relative.lower())
        name = name_result(definition, references) if not errors else {"status": "invalid"}
        entry = curated_names.get(index)
        if entry:
            if not matches_name_model(definition, entry):
                raise ValueError(f'{path.name}: curated name model does not match the definition')
            if 'name' in entry:
                name = {"status": "community_mapping", "resolved": entry['name']}
            elif entry.get('candidates'):
                name = {"status": "ambiguous", "candidates": entry['candidates']}
            else:
                name = {"status": "unresolved"}
        reference_name = name
        descriptive = descriptive_names.get(index)
        if descriptive:
            named = dict(definition)
            apply_descriptive_name(named, descriptive, entry)
            if (named['name'] != definition.get('name') or
                    named.get('description') != definition.get('description')):
                raise ValueError(f'{path.name}: descriptive name or model annotation is stale')
            name = {"status": "openkal_descriptive", "resolved": descriptive['name']}
        part = definition.get('class', ['', ''])[1]
        appearance = definition.get('skin', {})
        if not isinstance(appearance, dict):
            appearance = {}
        row = {"id": index, "name": definition.get('name'), "name_match": name,
               "reference_name_match": reference_name,
               "class": definition.get('limit', {}).get('class'), "part": part,
               "model": relative, "definition": path.relative_to(args.items).as_posix(),
               "errors": errors, "warnings": [],
               "skin_appearance": {key: appearance.get(key, False)
                                   for key in ('keep_hair', 'replace_face')},
               "visual_review": policy['observations'].get(str(index),
                                      {"status": "not_reviewed"})}
        row['catalog_kind'] = 'weapon_skin_candidate' if part == 'weapon' else 'costume_candidate'
        if part == 'weapon' and not errors:
            row['weapon_type'] = weapon_skin_type(definition)
            if not row['weapon_type']:
                row['weapon_type_origin'] = 'unclassified'
            elif index in compatibility:
                row['weapon_type_origin'] = 'openkal_grip_and_pose_review'
            elif row['class'] == 'knight':
                row['weapon_type_origin'] = ('reviewed_reference' if
                                             knight_weapon_type(*definition['action']) else
                                             'explicit_definition')
            else:
                row['weapon_type_origin'] = 'class_default'
            if not row['weapon_type']:
                row['warnings'].append('unclassified_weapon_type')
        if reference_name['status'] == 'test_sample':
            row['catalog_kind'] = 'appearance_sample' if part == 'face' else 'equipment_sample'
        if relative and not resolved:
            row['errors'].append('missing_model')
        row['_path'] = str(resolved) if resolved else ''
        rows.append(row)
        definitions[index] = definition
    if not rows:
        raise ValueError('no skin definitions found')
    missing_ids = sorted(expected_ids(policy) - seen)
    if missing_ids:
        raise ValueError(f'{len(missing_ids)} allocated definitions are missing; first IDs: '
                         f'{missing_ids[:16]}')
    requests = [{"key": str(row['id']), "path": row['_path']} for row in rows]
    template_keys = {}
    for row in rows:
        template = weapon_templates.get(row['id'])
        if not template:
            continue
        if not matches_name_model(definitions[row['id']], template):
            raise ValueError(f"{row['definition']}: weapon template model does not match the definition")
        action = tuple(template['reference_action'])
        if action not in template_keys:
            relative = model_filename({'class': ['skin', 'weapon'], 'action': list(action),
                                       'limit': {'class': 'knight'}})
            path = files.get(relative.lower())
            template_keys[action] = f'weapon-reference:{action[0]}:{action[1]}'
            requests.append({'key': template_keys[action], 'path': str(path) if path else ''})
    for job, (letter, _) in JOBS.items():
        for part, suffix in (('head', 'f'), ('boots', 's')):
            path = files.get(f'model/clothes/c{letter}_0_{suffix}01.gb')
            requests.append({"key": f'{part}:{job}', "path": str(path) if path else ''})
    with tempfile.TemporaryDirectory(prefix='openkal-skin-audit-') as temporary:
        manifest = Path(temporary) / 'models.json'
        write_json(manifest, requests)
        probe = subprocess.run([str(args.probe.resolve()), str(manifest)],
                               capture_output=True, text=True, check=True)
        inspections = {row['key']: row for row in json.loads(probe.stdout)}
    duplicates = collections.defaultdict(list)
    for row in rows:
        del row['_path']
        row['inspection'] = inspections[str(row['id'])]
        row['errors'].extend(row['inspection']['errors'])
        template = weapon_templates.get(row['id'])
        if template:
            reference = inspections[template_keys[tuple(template['reference_action'])]]
            result, errors = verify_weapon_template(
                definitions[row['id']], template, row['inspection'], reference,
                template_types.get(tuple(template['reference_action']), ''))
            row['weapon_template'] = result
            row['errors'].extend(errors)
        own_type = compatibility.get(row['id'])
        if own_type:
            result, errors = verify_weapon_compatibility(
                definitions[row['id']], own_type, row['inspection'])
            row['weapon_compatibility'] = result
            row['errors'].extend(errors)
        diagnoses = diagnose_textures(row['inspection'], textures)
        if diagnoses:
            row['errors'] = [code for code in row['errors'] if code != 'missing_or_invalid_texture']
            row['errors'].extend(diagnoses)
        row['warnings'].extend(coverage_warnings(row, inspections))
        if row['reference_name_match']['status'] == 'ambiguous':
            row['warnings'].append('ambiguous_name')
        if not row['errors']:
            key = (row['inspection']['geometry_fingerprint'],
                   row['inspection']['texture_fingerprint'])
            duplicates[key].append(row['id'])
    duplicate_groups = [ids for ids in duplicates.values() if len(ids) > 1]
    duplicate_ids = {index for group in duplicate_groups for index in group}
    for row in rows:
        if row['id'] in duplicate_ids:
            row['warnings'].append('duplicate_model_and_texture')
        row['technical_status'] = 'error' if row['errors'] else (
            'review' if row['warnings'] else 'passed')
    # Detect per-class gaps in the same model family; absence is not automatically an error.
    families = collections.defaultdict(dict)
    for row in rows:
        definition = definitions[row['id']]
        key = reference_key(definition)[1:]
        families[key][row['class']] = row['id']
    family_report = [{"kind": key[0], "action": list(key[1:]), "items": items,
                      "missing_classes": sorted(set(JOBS) - set(items))}
                     for key, items in sorted(families.items())]
    summary = {"items": len(rows),
               "by_class": dict(collections.Counter(row['class'] for row in rows)),
               "technical_status": dict(collections.Counter(row['technical_status'] for row in rows)),
               "name_status": dict(collections.Counter(row['name_match']['status'] for row in rows)),
               "visual_status": dict(collections.Counter(row['visual_review']['status'] for row in rows)),
               "duplicate_groups": len(duplicate_groups)}
    report = {"schema": "openkal.skin-audit/v1", "summary": summary, "items": rows,
              "curated_name_source": name_source,
              "descriptive_name_source": descriptive_source,
              "weapon_template_source": template_data['source'],
              "weapon_compatibility_source": compatibility_source,
              "diagnostic_asset_scopes": ['Model/Clothes', 'Item/Weapon'],
              "families": family_report, "duplicate_groups": duplicate_groups,
              "withdrawn_ranges": policy['withdrawn_ranges'],
              "limitations": ["Technical checks do not prove correct animation, clipping or appearance.",
                              "Texture diagnostics follow the current DDS/GTX/TGA/PNG runtime resolver; assets are not converted.",
                              "Local archive names are matches, not independently verified official names.",
                              "OpenKal descriptive names are visual labels, not official names or weapon-type evidence.",
                              "OpenKal grip/pose compatibility choices do not establish reference weapon categories or complete animation/effect correctness.",
                              "Missing class variants may be intentional.",
                              "Fingerprints identify candidates for review, not permission to remove items."]}
    args.output.mkdir(parents=True, exist_ok=True)
    write_json(args.output / 'report.json', report)
    with (args.output / 'catalog.csv').open('w', newline='', encoding='utf-8') as handle:
        writer = csv.writer(handle)
        writer.writerow(['ID', 'Class', 'Slot', 'Display name', 'Name status',
                         'Weapon type', 'Weapon type origin', 'Technical status',
                         'Visual status', 'Issues', 'Model'])
        for row in rows:
            writer.writerow([row['id'], row['class'], row['part'],
                             row['name_match'].get('resolved', row['name']), row['name_match']['status'],
                             row.get('weapon_type', ''), row.get('weapon_type_origin', ''),
                             row['technical_status'], row['visual_review']['status'],
                             '; '.join(row['errors'] + row['warnings']), row['model']])
    if args.named_items:
        target = args.output / 'named-items' / 'skins'
        target.mkdir(parents=True)
        for row in rows:
            definition = dict(definitions[row['id']])
            if row['name_match']['status'] in ('local_reference_match', 'community_mapping',
                                             'openkal_descriptive'):
                definition['name'] = row['name_match']['resolved']
            write_json(target / Path(row['definition']).name, definition)
    review_groups = []
    by_id = {row['id']: row for row in rows}
    for family in family_report:
        review_groups.append({**family, 'members': [
            {"id": index, "class": job, "name": by_id[index]['name_match'].get(
                'resolved', by_id[index]['name']), "technical_status": by_id[index]['technical_status'],
             "model": by_id[index]['model'], "visual_review": by_id[index]['visual_review']}
            for job, index in family['items'].items()]})
    write_json(args.output / 'review-groups.json', review_groups)
    lines = ['# Skin catalog review', '',
             'Local inspection of optional user assets; no source definitions were changed.', '',
             f"- Items checked: **{len(rows)}** across all four classes.",
             '- Technical results: ' + ', '.join(f'{value} {key}' for key, value in
                                                summary['technical_status'].items()) + '.',
             '- Name results: ' + ', '.join(f'{value} {key}' for key, value in
                                           summary['name_status'].items()) + '.',
             f"- Identical model/texture candidate pairs: {len(duplicate_groups)}.",
             f"- Grouped comparison sets: {len(family_report)}, including " +
             f"{sum(group['kind'] == 'costume' for group in family_report)} costume model families.",
             '', 'Technical errors can be investigated before any manual equipment tests.',
             'Each comparison group lists all available classes in `review-groups.json`.', '',
             '## Causes requiring attention', '']
    for code, count in collections.Counter(code for row in rows for code in row['errors']).most_common():
        lines.append(f'- **{code}**: {count} items.')
    lines += ['', '## Technical findings', '']
    for row in rows:
        if row['errors'] or row['warnings']:
            lines.append(f"- **{row['id']}** ({row['class']}, {row['part']}): " +
                         ', '.join(row['errors'] + row['warnings']))
    if not any(row['errors'] or row['warnings'] for row in rows):
        lines.append('No technical findings. Visual validation remains necessary.')
    lines += ['', '## Previously reported visual observations', '']
    for row in rows:
        if row['visual_review']['status'] != 'not_reviewed':
            lines.append(f"- **{row['id']}**: {row['visual_review']['status']} — " +
                         row['visual_review'].get('note', ''))
    lines += ['', '## Limits', '', *['- ' + note for note in report['limitations']], '']
    (args.output / 'review.md').write_text('\n'.join(lines), encoding='utf-8')
    print(json.dumps(summary, indent=2))
    return 1 if any(row['errors'] for row in rows) else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--items', type=Path, default=REPO / 'config/items/skins')
    parser.add_argument('--policy', type=Path, default=REPO / 'config/skins/catalog.json')
    parser.add_argument('--names', type=Path, default=REPO / 'config/skins/names.json')
    parser.add_argument('--descriptive-names', type=Path,
                        default=REPO / 'config/skins/descriptive-names.json')
    parser.add_argument('--weapon-templates', type=Path,
                        default=REPO / 'config/skins/weapon-templates.json')
    parser.add_argument('--weapon-compatibility', type=Path,
                        default=REPO / 'config/skins/weapon-compatibility.json')
    parser.add_argument('--data-root', type=Path, required=True)
    parser.add_argument('--probe', type=Path, default=REPO / 'build/bin/tools/skin_asset_probe')
    parser.add_argument('--legacy-items', type=Path, action='append', default=[])
    parser.add_argument('--named-items', action='store_true',
                        help='write a separate local item overlay with resolved display names')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        return audit(args)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(2, f'Catalog audit failed: {error}\n')


if __name__ == '__main__':
    raise SystemExit(main())
