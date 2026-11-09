#!/usr/bin/env python3
"""PNG previews from the production firmware's actual Adafruit GFX frames."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
root=Path(__file__).resolve().parents[1]
p=root/'preview'
try:
 font=ImageFont.truetype('/System/Library/Fonts/Supplemental/Arial.ttf',18)
except OSError:
 font=ImageFont.load_default()
for source in sorted(p.glob('*.pgm')):
 Image.open(source).convert('RGB').save(source.with_suffix('.png'))

def board(name, entries):
 canvas=Image.new('RGB',(840,((len(entries)+1)//2)*260),'#17212b')
 draw=ImageDraw.Draw(canvas)
 for i,(source,label) in enumerate(entries):
  x,y=20+(i%2)*420,16+(i//2)*260
  draw.text((x,y),label,font=font,fill='white')
  image=Image.open(p/(source+'.png')).convert('RGB')
  canvas.paste(image.resize((384,192),Image.Resampling.NEAREST),(x,y+30))
 canvas.save(p/name)

board('features_preview.png',[
 ('run_running_bar','RUN HIGH - vista BAR / BEAT'),('run_stopped_timer','RUN LOW - durata conservata'),
 ('menu_run_gate','RESET IN: RUN GATE'),('menu_output_2','MEAS OUT: SWING'),
 ('menu_9','Swing dal 50% al 75%'),('menu_output_3','Preset SAMBA'),
 ('menu_output_4','Preset BOSSA'),('menu_output_5','Preset SON 3-2'),
 ('menu_output_6','Preset TRESILLO'),('menu_10','Salva ed esci')])
all_entries=[(f.stem,f.stem.replace('_',' ')) for f in sorted(p.glob('*.pgm'))]
board('all_screens.png',all_entries)
