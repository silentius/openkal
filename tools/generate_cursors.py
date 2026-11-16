#!/usr/bin/env python3
"""Regenerate OpenKal's vector-designed cursor strips (Pillow and ImageMagick).

The committed 32-bit BMPs are runtime assets; these tools are only needed when
changing the artwork. Each transparent cell is 48x48 pixels. Arrow/sword strips
contain 30 frames for a one-second sweep (played every five seconds); the hand has 16 grip frames played forward and backward.
"""

import io
import math
from pathlib import Path
import subprocess

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SIZE = 48
DEFS = '''<defs>
<linearGradient id="gold" x1="0" y1="0" x2="1" y2="1">
<stop stop-color="#fff3b0"/><stop offset=".45" stop-color="#d9b65e"/>
<stop offset="1" stop-color="#80602b"/></linearGradient>
<linearGradient id="ivory" x1="0" y1="0" x2="1" y2="1">
<stop stop-color="#fffce6"/><stop offset=".5" stop-color="#eee2bc"/>
<stop offset="1" stop-color="#b39a6d"/></linearGradient>
<linearGradient id="steel" x1="0" y1="0" x2="1" y2="1">
<stop stop-color="#fff8d3"/><stop offset=".5" stop-color="#e5d199"/>
<stop offset="1" stop-color="#a58b50"/></linearGradient>
</defs>'''


def polygon(points, fill, stroke='#080a0d', width=1.5):
    coords = ' '.join(f'{x},{y}' for x, y in points)
    return (f'<polygon points="{coords}" fill="{fill}" stroke="{stroke}" '
            f'stroke-width="{width}" stroke-linejoin="round"/>')


def path(d, fill='none', stroke='#080a0d', width=1.4):
    return (f'<path d="{d}" fill="{fill}" stroke="{stroke}" '
            f'stroke-width="{width}" stroke-linecap="round" stroke-linejoin="round"/>')


def point_at(points, progress):
    lengths = [math.dist(a, b) for a, b in zip(points, points[1:])]
    distance = min(1, max(0, progress)) * sum(lengths)
    for a, b, length in zip(points, points[1:], lengths):
        if distance <= length:
            t = distance / length
            return a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t
        distance -= length
    return points[-1]


def trails(routes, phase):
    # Both heads arrive at the tip together. The tail drains before the next lap.
    head = phase * 1.25
    result = ''
    for route in routes:
        # A dark moving under-stroke keeps the upper trail legible on pale gold.
        for color, width in [('#342c22', 3.6), ('#ffffff', 1.9)]:
            for segment in range(30):
                a = head - .24 + segment * .008
                b = a + .008
                if a < 0 or b > 1:
                    continue
                x, y = point_at(route, a)
                u, v = point_at(route, b)
                opacity = (segment / 29) ** 1.4
                result += (f'<path d="M{x},{y} L{u},{v}" stroke="{color}" '
                           f'stroke-width="{width}" opacity="{opacity}" stroke-linecap="round"/>')

    return result


def arrow(phase):
    tip, left, notch, right = (6, 4), (6, 44), (19, 32), (35, 34)
    art = polygon([tip, right, notch, left], 'url(#gold)')
    art += polygon([tip, (14, 25), right], '#f2d57b', 'none')
    art += polygon([(14, 25), notch, left], '#917034', 'none')
    art += path('M6.6,5.7 L14,25 L33,33', stroke='#fff1b1', width=.65)
    # Inset traces preserve the dark outer silhouette on bright backgrounds.
    art += trails([[(19, 32), (7, 42), (7, 6)],
                   [(19, 32), (33, 33), (7, 6)]], phase)
    return art


def sword(phase):
    art = polygon([(27, 29), (41, 39), (38, 43), (26, 32)], '#63432a')
    art += path('M31,32 L29,35 M34,34 L32,37 M37,36 L35,39', stroke='#dfc47d', width=1.3)
    art += polygon([(6, 4), (24, 15), (32, 27), (26, 32), (15, 24)], 'url(#steel)')
    art += polygon([(6, 4), (20, 21), (26, 32), (15, 24)], '#a48b4e', 'none')
    art += path('M7,5 L20,21 L28,28', stroke='#fff8df', width=1)
    art += polygon([(20, 34), (33, 21), (37, 23), (36, 27), (25, 38), (21, 38)], 'url(#gold)')
    art += polygon([(37, 38), (41, 38), (43, 41), (40, 45), (36, 42)], 'url(#gold)')
    art += trails([[(39, 42), (30, 34), (23, 37), (22, 35), (26, 31), (16, 24), (7, 6)],
                   [(39, 42), (39, 40), (31, 32), (35, 26), (35, 23), (31, 27), (24, 16), (7, 6)]], phase)
    return art


def hand(progress):
    # Ease into a grasp; playback reverses these frames to reopen the hand.
    t = progress * progress * (3 - 2 * progress)
    art = ''
    for base_x, tip_x, tip_y, closed_y in [(17, 12, 10, 25), (23, 21, 5, 23),
                                         (29, 29, 7, 24), (35, 37, 13, 27)]:
        x = tip_x + (base_x - tip_x) * t
        y = tip_y + (closed_y - tip_y) * t
        art += path(f'M{base_x-3},31 L{x-3},{y+3} Q{x-3},{y-1} {x},{y-1} '
                    f'Q{x+3},{y-1} {x+3},{y+3} L{base_x+3},32 Z', 'url(#ivory)')
        art += path(f'M{x-1},{y+3} L{x-.5},{y+7}', stroke='#fffef1', width=.9)
    art += path('M14,27 Q23,24 37,27 L37,35 Q35,40 32,41 L19,41 '
                'Q14,36 14,32 Z', 'url(#ivory)')
    thumb_x, thumb_y = 5 + 13*t, 27 + 4*t
    art += path(f'M18,37 Q12,36 {thumb_x},{thumb_y+4} '
                f'Q{thumb_x-3},{thumb_y} {thumb_x},{thumb_y-1} '
                f'Q{thumb_x+2},{thumb_y-2} {thumb_x+5},{thumb_y+1} '
                f'L23,30 Q26,32 24,35', 'url(#ivory)')
    art += path('M26,32 Q30,30 34,31 M26,35 Q30,34 33,35', stroke='#b39e77', width=.7)
    art += path('M18,40 Q26,42 34,39 L35,44 Q27,47 18,45 Z', 'url(#gold)')
    if t > .4:
        for x in [19, 25, 31]:
            art += path(f'M{x},27 q2,-1 3,1', stroke='#a89067', width=.7*t)
    return art


def render(art):
    svg = f'<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">{DEFS}{art}</svg>'
    png = subprocess.run(['convert', '-background', 'none', '-density', '384', 'svg:-',
                          '-resize', '48x48', 'png:-'], input=svg.encode(),
                         stdout=subprocess.PIPE, check=True).stdout
    return Image.open(io.BytesIO(png)).convert('RGBA')


def main():
    directory = ROOT / 'config/ui/cursors'
    directory.mkdir(parents=True, exist_ok=True)
    previews = []
    for index, count, draw in [(0, 30, arrow), (1, 30, sword), (2, 16, hand)]:
        frames = [render(draw(i / (count - 1) if index == 2 else i / count))
                  for i in range(count)]
        strip = Image.new('RGBA', (SIZE * count, SIZE))
        for i, frame in enumerate(frames):
            strip.paste(frame, (i * SIZE, 0))
        strip.save(directory / f'cursor{index}_strip.bmp')
        previews.append(frames)
    # A disposable inspection sheet, not a shipped resource.
    sheet = Image.new('RGB', (480, 288), '#29323d')
    for row, frames in enumerate(previews):
        for col, fraction in enumerate([0, .2, .4, .6, 1]):
            frame = frames[min(len(frames)-1, int(fraction * (len(frames)-1)))]
            frame = frame.resize((96, 96), Image.NEAREST)
            sheet.paste(frame, (col*96, row*96), frame)
    sheet.save('/tmp/openkal-cursor-preview.png')


if __name__ == '__main__':
    main()
