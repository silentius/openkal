#!/usr/bin/env python3
"""Render local costume comparisons from an existing skin audit.

Optional game art stays in the new output directory. Source assets and catalog
definitions are never modified. Images show static bind poses, not gameplay.
"""

import argparse
import html
import json
from pathlib import Path
import subprocess

from PIL import Image, ImageDraw, ImageFont

from audit_skins import JOBS, REPO, asset_index, write_json


def appearance_paths(member, files):
    costume = files.get(member['model'].lower())
    if not costume:
        raise ValueError(f"Model from audit no longer exists: {member['model']}")
    letter = JOBS[member['class']][0]
    # Match the runtime's whole-body and head-only composition with default clothes.
    if member['part'] == 'body':
        appearance = member.get('skin_appearance', {})
        parts = (() if appearance.get('replace_face') else ('f',))
        if appearance.get('keep_hair'):
            parts += ('h',)
    else:
        parts = ('s', 'g', 'p', 'a', 'f')
    context = [files.get(f'model/clothes/c{letter}_0_{part}01.gb') for part in parts]
    missing = [part for part, path in zip(parts, context) if path is None]
    paths = [str(path) for path in context if path]
    return ([str(costume)] + paths if member['part'] == 'body' else paths + [str(costume)]), missing


def costume_groups(report):
    return [family for family in report['families'] if family['kind'] == 'costume']


def group_title(group, members):
    names = {members[index]['name_match'].get('resolved', '').rsplit(' (', 1)[0]
             for index in group['items'].values()
             if members[index]['name_match']['status'] == 'community_mapping'}
    group_id, variant = group['action']
    if not names:
        names = {members[index]['name_match'].get('resolved', '').rsplit(' (', 1)[0]
                 for index in group['items'].values()
                 if members[index]['name_match']['status'] == 'openkal_descriptive'}
    title = ' / '.join(sorted(names)) if names else 'Costume comparison'
    return f'{title} | Model {group_id:02d}-{variant:02d}'


def font_path(argument):
    if argument:
        return argument
    candidates = [Path('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'),
                  Path('/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf')]
    for path in candidates:
        if path.is_file():
            return path
    raise ValueError('No TrueType font found; supply --font FONT.ttf')


def wrapped(draw, text, font, width, lines=3):
    result = []
    current = ''
    for word in text.split():
        candidate = (current + ' ' + word).strip()
        if current and draw.textlength(candidate, font=font) > width:
            result.append(current)
            current = word
        else:
            current = candidate
    if current:
        result.append(current)
    if len(result) > lines:
        result = result[:lines]
        result[-1] = result[-1].rstrip('.') + ' ...'
    return '\n'.join(result)


def compose_sheet(group, members, results, output, font, width, height):
    gap, heading, cell_heading, footer = 12, 84, 92, 32
    sheet = Image.new('RGB', (width * 4 + gap * 5,
                             heading + (height + cell_heading) * 2 + gap * 3 + footer), '#171a20')
    draw = ImageDraw.Draw(sheet)
    title_font = ImageFont.truetype(str(font), 25)
    label_font = ImageFont.truetype(str(font), 18)
    small_font = ImageFont.truetype(str(font), 15)
    status_font = ImageFont.truetype(str(font), 13)
    title = group_title(group, members)
    draw.text((gap, 12), title, font=title_font, fill='#e6e9ef')
    draw.text((gap, 49), 'Static bind pose | default clothing context | framing fitted per model',
              font=small_font, fill='#b1b8c6')
    for column, job in enumerate(JOBS):
        index = group['items'].get(job)
        member = members.get(index)
        for row, view in enumerate(('front', 'back')):
            x = gap + column * (width + gap)
            y = heading + row * (height + cell_heading + gap)
            draw.rectangle((x, y, x + width, y + cell_heading + height), fill='#20232a')
            draw.text((x + 10, y + 7), f'{job.title()} | {index or "unavailable"} | {view.title()}',
                      font=label_font, fill='#e6e9ef')
            if member:
                name = member['name_match'].get('resolved', member['name'])
                draw.multiline_text((x + 10, y + 35), wrapped(draw, name, small_font, width - 20, 2),
                                    font=small_font, fill='#c4cad5', spacing=3)
                status = (member['name_match']['status'] + '; ' +
                          member['visual_review']['status']).replace('_', ' ')
                draw.text((x + 10, y + 74), status, font=status_font, fill='#aeb8c9')
            capture = results.get(index, {})
            path = output / 'renders' / f'{index}-{view}.png'
            if member and capture.get('rendered') and path.is_file():
                with Image.open(path) as image:
                    sheet.paste(image.convert('RGB'), (x, y + cell_heading))
            else:
                reason = 'No model for this class in the catalog.'
                if member:
                    reason = ('Technical check: ' + ', '.join(member['errors']) if member['errors']
                              else capture.get('error', 'Render unavailable'))
                draw.multiline_text((x + 16, y + cell_heading + 35),
                                    wrapped(draw, reason, label_font, width - 32, 9),
                                    font=label_font, fill='#e3bc85', spacing=8)
                if member:
                    names = [texture['name'] for texture in member['inspection']['textures']
                             if not texture['loaded']]
                    draw.multiline_text((x + 16, y + cell_heading + 190),
                                        wrapped(draw, ', '.join(names), small_font, width - 32, 4),
                                        font=small_font, fill='#c4cad5', spacing=5)
    draw.text((gap, sheet.height - footer + 5),
              'Appearance comparison only. Motion, clipping and attachment still need in-game tests.',
              font=small_font, fill='#b1b8c6')
    key = f"costume-{group['action'][0]:02d}-{group['action'][1]:02d}"
    path = output / 'sheets' / f'{key}.png'
    sheet.save(path)
    return {'key': key, 'title': title, 'sheet': 'sheets/' + path.name,
            'items': group['items'], 'missing_classes': group['missing_classes']}


def attach_review(sheets, review):
    """Keep static observations separate and reject stale family/ID mappings."""
    if review.get('schema') != 'openkal.skin-preview-review/v1':
        raise ValueError('unsupported preview review schema')
    entries = {}
    for entry in review['families']:
        key = f"costume-{entry['action'][0]:02d}-{entry['action'][1]:02d}"
        if key in entries:
            raise ValueError(f'duplicate preview review family: {key}')
        entries[key] = entry
    for sheet in sheets:
        entry = entries.get(sheet['key'])
        if entry:
            if entry['items'] != sheet['items']:
                raise ValueError(f"stale preview review IDs for {sheet['key']}")
            sheet['review'] = entry


def build_html(sheets, review=None):
    runtime_note = '<p>' + html.escape(review['runtime_update']) + '</p>' \
        if review and review.get('runtime_update') else ''
    sections = []
    for entry in sheets:
        inspection = entry.get('review', {})
        notes = inspection.get('observations', [])
        description = inspection.get('description', '')
        search = ' '.join([entry['title'], description,
                           ' '.join(map(str, entry['items'].values())),
                           ' '.join(note['kind'] + ' ' + note['note'] for note in notes)])
        findings = ''.join('<li>' + html.escape(note['note']) + ' <small>IDs: ' +
                           ', '.join(map(str, note['items'])) + '</small></li>' for note in notes)
        status = inspection.get('status', 'not_reviewed').replace('_', ' ')
        flagged = bool(notes)
        sections.append(f'''<section data-search="{html.escape(search.lower(), quote=True)}" data-flagged="{str(flagged).lower()}">
<h2>{html.escape(entry['title'])}</h2>
<p><strong>{html.escape(description)}</strong> <span class="status">{html.escape(status)}</span></p>
{'<ul class="findings">' + findings + '</ul>' if findings else ''}
<a href="{html.escape(entry['sheet'], quote=True)}" target="_blank"><img src="{html.escape(entry['sheet'], quote=True)}" loading="lazy"
alt="{html.escape(entry['title'], quote=True)}: available classes, front and back"></a>
</section>''')
    test_plan = ''
    if review:
        rounds = []
        for round_entry in review.get('test_rounds', []):
            cases = []
            for case in round_entry['cases']:
                if case.get('status', 'pending') != 'pending':
                    label = case['status'].replace('_', ' ') + ': ' + ', '.join(map(str, case['items']))
                    cases.append('<li><strong>' + html.escape(label) + '</strong><p>' +
                                 html.escape(case['result']) + '</p></li>')
                    continue
                commands = '\n'.join(f'/getitemid {index} 0 1' for index in case['items'])
                cases.append('<li><pre>' + html.escape(commands) + '</pre>' +
                             html.escape(case['check']) + '</li>')
            expanded = round_entry.get('open', round_entry['id'] == 'first')
            rounds.append('<details' + (' open' if expanded else '') +
                          '><summary>' + html.escape(round_entry['description']) +
                          '</summary><ol>' + ''.join(cases) + '</ol></details>')
        test_plan = '<aside><h2>Focused in-game tests</h2>' + ''.join(rounds) + '</aside>'
    return '''<!doctype html><html lang="en"><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>OpenKal costume comparisons</title><style>
body{margin:0;background:#171a20;color:#e6e9ef;font:16px system-ui,sans-serif}
header,main{max-width:1600px;margin:auto;padding:20px}header{background:#171a20}
h1{font-size:24px;margin:0 0 12px}h2{font-size:20px;font-weight:500}p{color:#bac1ce;line-height:1.5}
input[type=search]{box-sizing:border-box;width:100%;padding:12px;background:#252932;color:inherit;border:1px solid #626b7d;font:inherit}
section{margin-bottom:35px}img{display:block;width:100%;height:auto}a{color:#a7c7ed}
.status{margin-left:12px;color:#e3bc85}small{color:#bac1ce}li{line-height:1.6;margin-bottom:10px}
aside{border-bottom:1px solid #626b7d;padding-bottom:20px;margin-bottom:30px}
summary{cursor:pointer;padding:12px 0}pre{font:15px ui-monospace,monospace}label{display:block;margin:14px 0}
</style><header><h1>OpenKal costume comparisons</h1>
<p>Static bind poses, default clothing context, front and back. Click a sheet for full resolution.
Missing models and textures are marked. These images do not verify animation or clipping.</p>
<p>Visual descriptions are search labels, not official names. Static review notes are separate from
the earlier in-game status labels printed on the sheets.</p>
<input type="search" aria-label="Filter by name, model or item ID" placeholder="Filter by name, description, model or item ID" id="filter">
<label><input type="checkbox" id="flagged"> Only families with findings</label><p id="count"></p>
</header><main>''' + runtime_note + test_plan + '\n'.join(sections) + '''</main><script>
function filterSheets(){
const value=document.getElementById('filter').value.toLowerCase().trim();
const flagged=document.getElementById('flagged').checked;
let count=0;
document.querySelectorAll('section').forEach(section=>{
section.hidden=!section.dataset.search.includes(value)||(flagged&&section.dataset.flagged!=='true');
if(!section.hidden)count++;
});
document.getElementById('count').textContent=count+' families shown';
}
document.getElementById('filter').addEventListener('input',filterSheets);
document.getElementById('flagged').addEventListener('change',filterSheets);
filterSheets();</script></html>'''


def generate(args):
    if args.output.exists() and any(args.output.iterdir()):
        raise ValueError('output directory must be empty')
    report = json.loads(args.report.read_text(encoding='utf-8'))
    if report.get('schema') != 'openkal.skin-audit/v1':
        raise ValueError('unsupported audit report schema')
    groups = costume_groups(report)
    if not groups:
        raise ValueError('the report has no costume families')
    members = {member['id']: member for member in report['items']}
    files = asset_index(args.data_root)
    font = font_path(args.font)
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / 'renders').mkdir()
    (args.output / 'sheets').mkdir()
    jobs = []
    context_warnings = {}
    for group in groups:
        for index in group['items'].values():
            member = members[index]
            if member['errors']:
                continue
            paths, missing = appearance_paths(member, files)
            if missing:
                context_warnings[str(index)] = missing
            jobs.append({'id': index, 'paths': paths,
                         'front': str((args.output / 'renders' / f'{index}-front.png').resolve()),
                         'back': str((args.output / 'renders' / f'{index}-back.png').resolve())})
    manifest = args.output / 'render-manifest.json'
    write_json(manifest, {'width': args.width, 'height': args.height, 'jobs': jobs})
    run = subprocess.run([str(args.renderer.resolve()), str(manifest.resolve())],
                         capture_output=True, text=True)
    (args.output / 'renderer.log').write_text(run.stderr, encoding='utf-8')
    if run.returncode != 0:
        raise ValueError('renderer failed; see renderer.log in the output directory')
    results = {entry['id']: entry for entry in json.loads(run.stdout)}
    sheets = [compose_sheet(group, members, results, args.output, font, args.width, args.height)
              for group in groups]
    review = json.loads(args.review.read_text(encoding='utf-8')) if args.review else None
    if review:
        attach_review(sheets, review)
    write_json(args.output / 'previews.json', {
        'schema': 'openkal.skin-previews/v1', 'sheets': sheets,
        'rendered_members': sum(entry['rendered'] for entry in results.values()),
        'context_warnings': context_warnings, 'render_results': list(results.values()),
        'pose': 'static bind pose; no animation sampled'})
    (args.output / 'index.html').write_text(build_html(sheets, review), encoding='utf-8')
    lines = ['# Costume comparisons', '',
             'Static bind poses with default clothing context. Front and back views are framed',
             'per model. These images do not verify movement, clipping or attachment.', '',
             'Open `index.html` for an offline gallery with search, or open individual sheets:', '']
    lines.extend(f"- [{entry['title']}]({(args.output / entry['sheet']).resolve()})" for entry in sheets)
    (args.output / 'index.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    failures = sum(not entry['rendered'] for entry in results.values())
    print(f'Created {len(sheets)} comparison sheets; '
          f'{len(results) - failures} appearances rendered from front and back; '
          f'{failures} unexpected rendering failures.')
    return 1 if failures else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--data-root', type=Path, required=True)
    parser.add_argument('--renderer', type=Path,
                        default=REPO / 'build/bin/tools/skin_preview_renderer')
    parser.add_argument('--font', type=Path)
    parser.add_argument('--review', type=Path,
                        default=REPO / 'config/skins/preview-review.json')
    parser.add_argument('--width', type=int, default=384)
    parser.add_argument('--height', type=int, default=512)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        if not 128 <= args.width <= 2048 or not 128 <= args.height <= 2048:
            raise ValueError('image dimensions must be between 128 and 2048 pixels')
        return generate(args)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(2, f'Preview generation failed: {error}\n')


if __name__ == '__main__':
    raise SystemExit(main())
