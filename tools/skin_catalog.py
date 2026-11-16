"""Shared access to curated, stable-ID cosmetic review catalogs."""

import json
from functools import lru_cache
from pathlib import Path


def load_name_catalog(path):
    data = json.loads(path.read_text(encoding='utf-8'))
    if data.get('schema') != 'openkal.skin-names/v1':
        raise ValueError('unsupported skin name catalog schema')
    entries = {}
    for section in ('entries', 'ambiguous'):
        for entry in data[section]:
            index = entry['index']
            if index in entries:
                raise ValueError(f'duplicate name catalog ID: {index}')
            entries[index] = entry
    return data['source'], entries


def matches_name_model(definition, entry):
    return (definition.get('index') == entry['index'] and
            definition.get('limit', {}).get('class') == entry['class'] and
            definition.get('class') == ['skin', entry['part']] and
            definition.get('action') == entry['action'])


def load_descriptive_name_catalog(path):
    """Read OpenKal names with explicit stable-ID/class/slot/model guards."""
    data = json.loads(path.read_text(encoding='utf-8'))
    if not isinstance(data, dict) or data.get('schema') != 'openkal.skin-descriptive-names/v1':
        raise ValueError('unsupported descriptive name catalog schema')
    if not isinstance(data.get('source'), dict) or not isinstance(data.get('entries'), list):
        raise ValueError('invalid descriptive name source or entries')
    entries = {}
    for entry in data['entries']:
        if not isinstance(entry, dict):
            raise ValueError('invalid descriptive name entry')
        index, action, name = entry.get('index'), entry.get('action'), entry.get('name')
        if (type(index) is not int or not 25000 <= index <= 29999 or
                not isinstance(action, list) or len(action) != 2 or
                any(type(v) is not int or v < 0 for v in action) or action[1] < 1 or
                entry.get('class') not in ('knight', 'magician', 'archer', 'thief') or
                entry.get('part') not in ('body', 'weapon', 'shield', 'helmet', 'upper',
                                          'gauntlet', 'pants', 'boots', 'face') or
                not isinstance(name, str) or not name.strip() or
                any(ord(c) < 32 for c in name) or index in entries):
            raise ValueError('invalid or duplicate descriptive name mapping')
        entries[index] = entry
    return data['source'], entries


def apply_descriptive_name(definition, entry, community_entry=None):
    """Apply a guarded visual label without replacing a resolved community name."""
    if community_entry and 'name' in community_entry:
        raise ValueError('descriptive name would replace a community name')
    if not matches_name_model(definition, entry):
        raise ValueError('descriptive name model does not match the definition')
    definition['name'] = entry['name']
    group, variant = entry['action']
    note = (f"OpenKal descriptive name; model {entry['class']}/{entry['part']} "
            f"{group:02d}-{variant:02d}.")
    description = definition.get('description', '')
    if note not in description:
        definition['description'] = (description + ' ' + note).strip()


def costume_replaces_face(job, group):
    """Whole-body meshes observed with complete faces in local head-isolation renders."""
    return group in (7, 8, 29, 30)


def load_weapon_templates(path):
    """Read guarded, manually reviewed template coordinates without inspecting game art."""
    data = json.loads(path.read_text(encoding='utf-8'))
    if data.get('schema') != 'openkal.skin-weapon-templates/v1':
        raise ValueError('unsupported weapon template catalog schema')
    entries = {}
    coordinates = set()
    def valid_action(value):
        return (isinstance(value, list) and len(value) == 2 and
                all(type(v) is int and v >= 0 for v in value) and value[1] > 0)

    references = set()
    for reference in data['additional_references']:
        if (not valid_action(reference.get('action')) or
                reference.get('class') != 'knight' or reference.get('part') != 'weapon' or
                reference.get('weapon_type') not in ('sword', 'sword2h') or
                tuple(reference['action']) in references):
            raise ValueError('invalid weapon reference coordinates or type')
        references.add(tuple(reference['action']))
    for entry in data['entries']:
        shape = entry.get('expected_shape_fingerprint')
        if (not valid_action(entry.get('action')) or
                not valid_action(entry.get('reference_action')) or
                type(entry.get('index')) is not int or not 25000 <= entry['index'] <= 29999 or
                not isinstance(shape, str) or len(shape) != 16 or
                any(c not in '0123456789abcdef' for c in shape) or
                type(entry.get('expected_shape_meshes')) is not int or
                entry['expected_shape_meshes'] < 1):
            raise ValueError('invalid weapon template coordinates, ID or expected shape')
        key = tuple(entry['action'])
        if (entry['index'] in entries or key in coordinates or
                entry['class'] != 'knight' or entry['part'] != 'weapon'):
            raise ValueError('duplicate or incompatible weapon template mapping')
        coordinates.add(key)
        entries[entry['index']] = entry
    return data, entries


@lru_cache(maxsize=1)
def _weapon_templates():
    return load_weapon_templates(
        Path(__file__).resolve().parent.parent / 'config/skins/weapon-templates.json')


def knight_reference_types(template_data):
    """Configured Knight categories, independent of display-name wording."""
    known = {
        (21, 1): 'sword',
        (21, 2): 'sword2h',
        (21, 3): 'sword',
        (21, 4): 'sword2h',
        (22, 1): 'sword',
        (22, 2): 'sword2h',
        (22, 3): 'sword',
        (22, 4): 'sword2h',
        (23, 1): 'sword',
        (23, 3): 'sword',
        (23, 4): 'sword2h',
        (40, 1): 'sword',
        (100, 5): 'sword2h',
        (109, 1): 'sword',
    }
    for entry in template_data['additional_references']:
        key = tuple(entry['action'])
        if key in known:
            raise ValueError('additional weapon reference overrides an existing classification')
        known[key] = entry['weapon_type']
    return known


def knight_weapon_type(group, variant):
    """Explicit Knight categories and guarded identical-geometry model mappings."""
    data, templates = _weapon_templates()
    known = knight_reference_types(data)
    key = (group, variant)
    if key in known:
        return known[key]
    for entry in templates.values():
        if tuple(entry['action']) == key:
            reference = known.get(tuple(entry['reference_action']), '')
            if not reference:
                raise ValueError('untyped weapon template reference')
            return reference
    return ''


def weapon_skin_type(definition):
    """Match runtime defaults without guessing Knight categories from model numbers."""
    configured = definition.get('skin', {}).get('weapon_type', '')
    if configured:
        return configured if configured in ('sword', 'sword2h', 'wand', 'bow', 'dagger') else ''
    return {'magician': 'wand', 'archer': 'bow', 'thief': 'dagger'}.get(
        definition.get('limit', {}).get('class'), '')


def load_weapon_compatibility(path):
    """Read OpenKal grip/pose choices, separately from evidenced references."""
    data = json.loads(path.read_text(encoding='utf-8'))
    if (not isinstance(data, dict) or
            data.get('schema') != 'openkal.skin-weapon-compatibility/v1'):
        raise ValueError('unsupported weapon compatibility catalog schema')
    if not isinstance(data.get('source'), dict) or not isinstance(data.get('entries'), list):
        raise ValueError('invalid weapon compatibility source or entries')
    entries, coordinates = {}, set()
    for entry in data['entries']:
        if not isinstance(entry, dict):
            raise ValueError('invalid weapon compatibility entry')
        index, action = entry.get('index'), entry.get('action')
        shape, reason = entry.get('expected_shape_fingerprint'), entry.get('reason')
        if (type(index) is not int or not 25000 <= index <= 29999 or
                entry.get('class') != 'knight' or entry.get('part') != 'weapon' or
                not isinstance(action, list) or len(action) != 2 or
                any(type(v) is not int or v < 0 for v in action) or action[1] < 1 or
                entry.get('weapon_type') not in ('sword', 'sword2h') or
                not _valid_shape_fingerprint(shape) or
                type(entry.get('expected_shape_meshes')) is not int or
                entry['expected_shape_meshes'] < 1 or
                not isinstance(reason, str) or not reason.strip() or
                any(ord(c) < 32 for c in reason)):
            raise ValueError('invalid weapon compatibility coordinates, type or review')
        if index in entries or tuple(action) in coordinates:
            raise ValueError('duplicate weapon compatibility mapping')
        if knight_weapon_type(*action):
            raise ValueError('weapon compatibility would replace an evidenced reference type')
        entries[index] = entry
        coordinates.add(tuple(action))
    return data['source'], entries


def _valid_shape_fingerprint(value):
    """Recognize the asset probe's lowercase 64-bit change detector."""
    return (isinstance(value, str) and len(value) == 16 and
            all(c in '0123456789abcdef' for c in value))


def apply_weapon_compatibility(definition, entry):
    """Apply a guarded project-defined type without changing stats or reference evidence."""
    if not matches_name_model(definition, entry):
        raise ValueError('weapon compatibility model does not match the definition')
    if knight_weapon_type(*entry['action']):
        raise ValueError('weapon compatibility would replace an evidenced reference type')
    appearance = dict(definition.get('skin', {}))
    if appearance.get('weapon_type', entry['weapon_type']) != entry['weapon_type']:
        raise ValueError('weapon compatibility conflicts with the configured type')
    appearance['weapon_type'] = entry['weapon_type']
    definition['skin'] = appearance
    label = 'one-hand sword' if entry['weapon_type'] == 'sword' else 'two-hand sword'
    note = f'OpenKal weapon compatibility: {label}; reference type unverified.'
    description = definition.get('description', '')
    if note not in description:
        definition['description'] = (description + ' ' + note).strip()
