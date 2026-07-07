#!/usr/bin/env python3
# Rasterise Pixelarticons (MIT, rect-path SVGs on a 24-grid) into a 16x16 1-bit
# BDF that mcufont's import_bdf consumes. Pure-PIL: no SVG engine needed.
import os, re, urllib.request, sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter, ImageChops

DILATE = 0   # supersample-scale dilation; >0 over-thickens at 12px, keep crisp 1px

SS = 8                    # supersample factor over the 24px grid
GRID = 24
SIZE = 12                 # output glyph size (24-grid downscales 2:1)

# (enum name, pixelarticons svg, codepoint)
ICONS = [
    ("Home",        "home",          0xE000),
    ("Settings",    "settings-2",    0xE001),
    ("Clock",       "clock",         0xE002),
    ("Menu",        "menu",          0xE003),
    ("User",        "user",          0xE004),
    ("Users",       "users",         0xE005),
    ("Mail",        "mail",          0xE006),
    ("Message",     "message",       0xE007),
    ("Bell",        "bell",          0xE008),
    ("Wifi",        "wifi",          0xE009),
    ("BatteryFull", "battery-full",  0xE00A),
    ("BatteryLow",  "battery-low",   0xE00B),
    ("Warning",     "warning-diamond", 0xE00C),
    ("Back",        "chevron-left",  0xE00D),
    ("ArrowLeft",   "arrow-left",    0xE00E),
    ("Reload",      "reload",        0xE00F),
    ("Power",       "power",         0xE010),
    ("Gps",         "gps",           0xE011),
    ("Radio",       "radio",         0xE012),
    ("Comment",     "comment",       0xE013),
    ("Chip",        "cpu",           0xE014),
    ("Star",        "star",          0xE015),
    ("Search",      "search",        0xE016),
    ("Plus",        "plus",          0xE017),
    ("Backspace",   "delete",        0xE018),
    ("ArrowRight",  "arrow-right",   0xE019),
    ("Check",       "check",         0xE01A),
    ("Feather",     "feather",       0xE01B),   # quill = compose/write
    ("Zap",         "zap",           0xE01C),   # lightning = quick replies
    ("Bluetooth",   "bluetooth",     0xE01D),
    ("Volume",     "volume-2",  0xE01E),   # speaker + wave: sound tile / audible
    ("VolumeMute", "volume-x",  0xE01F),   # muted state (top bar + sound tile)
    ("Moon",       "moon",      0xE020),   # theme tile: dark mode
    ("Sun",        "sun",       0xE021),   # theme tile: light mode
    ("Hourglass",  "hourglass",   0xE022),   # timer (vendored: not on github master)
    ("AlarmClock", "alarm-clock", 0xE023),   # alarm (vendored: not on github master)
    ("Globe",      "globe",       0xE024),   # world clock
    ("Grid",       "grid-3x3-sharp", 0xE025),   # 3x3 grid = 2048 board
    ("Coffee",     "coffee",      0xE026),   # coffee cup = support / About screen
]

# Iconify-sourced compound paths whose inner subpaths are holes (even-odd),
# unlike the github-master SVGs where separate strokes just union.
EVENODD = {"hourglass"}

NUM = re.compile(r'[-+]?\d*\.?\d+')

def subpaths(d):
    # Yield polygons (lists of (x,y)) for an SVG path of M/h/v/H/V/L/l/z commands.
    toks = re.findall(r'[MmLlHhVvZz]|[-+]?\d*\.?\d+', d)
    i, x, y, start, poly, out = 0, 0.0, 0.0, (0, 0), [], []
    cmd = None
    while i < len(toks):
        t = toks[i]
        if t in 'MmLlHhVvZz':
            cmd = t; i += 1
            if cmd in 'Zz':
                if poly: out.append(poly); poly = []
                x, y = start
            continue
        n = float(t)
        if cmd in 'Mm':
            x = n if cmd == 'M' else x + n; y = float(toks[i+1]) if cmd=='M' else y+float(toks[i+1]); i += 2
            start = (x, y); poly = [(x, y)]; cmd = 'L' if cmd=='M' else 'l'
        elif cmd in 'Ll':
            nx = n if cmd=='L' else x+n; ny = float(toks[i+1]) if cmd=='L' else y+float(toks[i+1]); i += 2
            x, y = nx, ny; poly.append((x, y))
        elif cmd in 'Hh':
            x = n if cmd=='H' else x+n; i += 1; poly.append((x, y))
        elif cmd in 'Vv':
            y = n if cmd=='V' else y+n; i += 1; poly.append((x, y))
    if poly: out.append(poly)
    return out

def raster(d, evenodd=False):
    polys = subpaths(d)
    if evenodd:
        acc = Image.new('1', (GRID*SS, GRID*SS), 0)
        for poly in polys:
            if len(poly) < 3: continue
            m = Image.new('1', acc.size, 0)
            ImageDraw.Draw(m).polygon([(px*SS, py*SS) for px, py in poly], fill=1)
            acc = ImageChops.logical_xor(acc, m)
        img = acc.convert('L').point(lambda v: 255 if v else 0)
    else:
        img = Image.new('L', (GRID*SS, GRID*SS), 0)
        dr = ImageDraw.Draw(img)
        for poly in polys:
            if len(poly) >= 3:
                dr.polygon([(px*SS, py*SS) for px, py in poly], fill=255)
    if DILATE:
        img = img.filter(ImageFilter.MaxFilter(DILATE * 2 + 1))   # bolden strokes
    return img.resize((SIZE, SIZE), Image.BOX)   # grayscale, averaged

def bdf_glyph(name, cp, img):
    rows = []
    px = img.load()
    nbytes = (SIZE + 7) // 8           # BDF rows are byte-padded, MSB-left
    field = nbytes * 8
    fmt = "%%0%dX" % (nbytes * 2)
    for yy in range(SIZE):
        bits = 0
        for xx in range(SIZE):
            if px[xx, yy] >= 96:       # threshold to 1-bit
                bits |= 1 << (field - 1 - xx)
        rows.append(fmt % bits)
    return (f"STARTCHAR {name}\nENCODING {cp}\nSWIDTH 1000 0\nDWIDTH {SIZE} 0\n"
            f"BBX {SIZE} {SIZE} 0 0\nBITMAP\n" + "\n".join(rows) + "\nENDCHAR\n")

_HERE = Path(__file__).parent

glyphs = []
for nm, svg, cp in ICONS:
    local = _HERE / f"{svg}.svg"
    if local.exists():
        data = local.read_text()
    else:
        url = f"https://github.com/halfmage/pixelarticons/raw/master/svg/{svg}.svg"
        data = urllib.request.urlopen(url).read().decode()
    # Pixelarticons may split a glyph across several <path> elements; render all
    # of them (each path starts with an absolute M, so the d strings concatenate).
    ds = re.findall(r'd="([^"]+)"', data)
    img = raster(" ".join(ds), evenodd=svg in EVENODD)
    g = bdf_glyph(nm, cp, img)
    glyphs.append(g)
    if nm in ("Home", "Clock"):
        print(f"--- {nm} ---", file=sys.stderr)
        for line in g.splitlines():
            if len(line) == 4 and all(c in "0123456789ABCDEF" for c in line):
                v = int(line, 16)
                print("".join('#' if v & (1 << (15-i)) else '.' for i in range(16)), file=sys.stderr)
    print("rendered", nm, svg, file=sys.stderr)

hdr = (f"STARTFONT 2.1\nFONT pixelicons\nSIZE {SIZE} 75 75\n"
       f"FONTBOUNDINGBOX {SIZE} {SIZE} 0 0\nSTARTPROPERTIES 2\n"
       f"FONT_ASCENT {SIZE}\nFONT_DESCENT 0\nENDPROPERTIES\nCHARS {len(glyphs)}\n")
open("Icons16.bdf", "w").write(hdr + "".join(glyphs) + "ENDFONT\n")
print("wrote Icons16.bdf", file=sys.stderr)
