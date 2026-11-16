#!/usr/bin/env python3
"""Regenerate embedded DSi RGB555 artwork and 8x8 Latin-1 font arrays.
Requires Pillow on the host machine. This script is not part of the target build.
"""
from PIL import Image, ImageFont, ImageDraw
from pathlib import Path
import struct
ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "resources"
OUT = ROOT / "source"
def rgb555(image):
    data=[]
    for r,g,b in image.convert("RGB").getdata():
        data.append(0x8000 | ((r>>3)<<10) | ((g>>3)<<5) | (b>>3))
    return data
def write_array(name, vals):
    with (OUT/(name+".h")).open("w") as f:
        f.write("#pragma once\n#include <stdint.h>\n")
        f.write(f"static const uint16_t {name}[{len(vals)}] = {{\n")
        for i in range(0,len(vals),16): f.write(",".join(f"0x{x:04X}" for x in vals[i:i+16])+",\n")
        f.write("};\n")
write_array("boot_top",rgb555(Image.open(SRC/"boot_top_reference.png").resize((256,192),Image.Resampling.LANCZOS)))
write_array("boot_bottom",rgb555(Image.open(SRC/"boot_bottom_reference.png").resize((256,192),Image.Resampling.LANCZOS)))
cover=Image.open(SRC/"cover.png").convert("RGB").resize((64,64),Image.Resampling.LANCZOS)
write_array("cover_logo",rgb555(cover))
for name,fn in [("cover_prev",lambda r,g,b:(int(r*.45),min(255,int(g*.8+40)),min(255,int(b*1.1+30)))),("cover_next",lambda r,g,b:(min(255,int(r*1.15+35)),int(g*.4),min(255,int(b*.9+20))))]:
    im=cover.copy(); im.putdata([fn(r,g,b) for r,g,b in im.getdata()]); write_array(name,rgb555(im))
font_path="/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
font=ImageFont.truetype(font_path,8); glyphs=[]
for cp in range(32,256):
    im=Image.new("L",(8,8),0); ImageDraw.Draw(im).text((0,-2),chr(cp),font=font,fill=255)
    for y in range(8): glyphs.append(sum((1<<x) for x in range(8) if im.getpixel((x,y))>=96))
with (OUT/"font8x8.h").open("w") as f:
    f.write("#pragma once\n#include <stdint.h>\nstatic const uint8_t NOISE_FONT8X8[224][8] = {\n")
    for i in range(224): f.write("{"+",".join(f"0x{x:02X}" for x in glyphs[i*8:(i+1)*8])+"},\n")
    f.write("};\n")
