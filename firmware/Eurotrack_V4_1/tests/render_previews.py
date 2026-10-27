#!/usr/bin/env python3
"""Convert frames rendered by the production firmware with real Adafruit GFX."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[1]
preview = root / 'preview'
files = sorted(preview.glob('[0-9][0-9]_*.pgm')) + sorted(preview.glob('menu_[0-9].pgm'))
labels = ['BAR/BEAT · pronto', 'BAR/BEAT · esterno', 'BAR/BEAT · massimo / pausa',
          'BAR/BEAT · interno', 'TIMER · pronto', 'TIMER · esterno',
          'TIMER · massimo / pausa', 'TIMER · interno']
try:
    font = ImageFont.truetype('/System/Library/Fonts/Supplemental/Arial.ttf', 18)
except OSError:
    font = ImageFont.load_default()
for source in preview.glob('*.pgm'):
    Image.open(source).convert('RGB').save(source.with_suffix('.png'))
canvas = Image.new('RGB', (840, ((len(files)+1)//2)*260), '#17212b')
draw = ImageDraw.Draw(canvas)
for i, source in enumerate(files):
    frame = Image.open(source).convert('RGB')
    x, y = 20+(i%2)*420, 16+(i//2)*260
    label = labels[i] if i < len(labels) else f'Menu {i-7}'
    draw.text((x,y), label, font=font, fill='white')
    canvas.paste(frame.resize((384,192), Image.Resampling.NEAREST), (x,y+30))
canvas.save(preview / 'display_overview.png')

comparison = Image.new('RGB', (840, 260), '#17212b')
draw = ImageDraw.Draw(comparison)
for col, (name, label) in enumerate([('02_external', 'BAR / BEAT'), ('06_timer_external', 'TIMER')]):
    x=20+col*420
    draw.text((x,16), label, font=font, fill='white')
    frame=Image.open(preview/(name+'.pgm')).convert('RGB')
    comparison.paste(frame.resize((384,192), Image.Resampling.NEAREST), (x,46))
comparison.save(preview/'display_main.png')
