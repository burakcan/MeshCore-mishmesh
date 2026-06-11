#!/usr/bin/env python3
"""Synthesize Latin Extended-A (Common EU Latin) glyphs onto the Nokia Cellphone
FC bitmap font, by compositing diacritics onto the existing base letters decoded
from a generated bwfont .c (Body.c / Subtitle.c).

Nokia already ships several marks via its Latin-1 letters (acute, grave,
circumflex, diaeresis, ring, cedilla); we extract and reuse those for visual
consistency. The remaining marks (caron, breve, double-acute, ogonek, dot-above,
stroke) are hand-designed per size.

Outputs: a preview PNG (for review) and, when --emit is given, a bwfont range
appended into the target .c (existing glyph arrays untouched).
"""
import re, sys, unicodedata

# ---- decode an existing bwfont .c into per-codepoint base bitmaps ----------
def parse_bwfont(path):
    src = open(path).read()
    def arr(name):
        m = re.search(r'\b'+re.escape(name)+r'\[\d*\]\s*(?:PROGMEM\s*)?=\s*\{(.*?)\}', src, re.S)
        return [int(x,0) for x in re.findall(r'0x[0-9a-fA-F]+|\d+', m.group(1))]
    ranges = []
    rblock = re.search(r'char_ranges\[\]\s*=\s*\{(.*?)\n\};', src, re.S).group(1)
    for rm in re.finditer(r'\{(.*?)\}', rblock, re.S):
        body = rm.group(1)
        nums = [int(n) for n in re.findall(r'^\s*(\d+),', body, re.M)][:7]
        names = re.findall(r'(mf_bwfont_\w+)', body)
        fc,cc,ox,oy,hb,hp,w = nums
        ranges.append(dict(first_char=fc,char_count=cc,off_x=ox,off_y=oy,hb=hb,hp=hp,
                           width=w,widths=arr(names[0]),offsets=arr(names[1]),data=arr(names[2])))
    return ranges

def glyph_cols(r, idx):
    """Boolean grid as list of columns; each column is a list of hp bools (top..bottom)."""
    hb,hp = r['hb'],r['hp']
    if r['width']:
        ncols=r['width']; start=r['width']*idx
    else:
        start=r['offsets'][idx]; ncols=r['offsets'][idx+1]-r['offsets'][idx]
    cols=[]
    for c in range(ncols):
        cb=r['data'][(start+c)*hb:(start+c)*hb+hb]
        cols.append([bool((cb[y//8]>>(y%8))&1) for y in range(hp)])
    return cols, (r['widths'][idx] if not r['width'] else r['width'])

def build_font(path):
    ranges = parse_bwfont(path)
    g = {}  # cp -> dict(cols=[[bool]*hp], adv, oy, hp)
    for r in ranges:
        for i in range(r['char_count']):
            cp = r['first_char']+i
            cols,adv = glyph_cols(r,i)
            g[cp] = dict(cols=cols, adv=adv, oy=r['off_y'], hp=r['hp'])
    return g, ranges

# ---- normalize a base glyph into a common 9-row (size-8) / taller grid ------
# We express every synthesized glyph in the *accented* cell: hp_acc, off_y=0,
# base content placed at the same absolute rows as in the ASCII cell.
def to_grid(g, cp, hp_acc):
    """Return (W x hp_acc) grid (list of columns of bools) with the base letter
    positioned to match its on-screen ascii row, leaving top rows for a mark."""
    e = g[cp]
    src = e['cols']; W=len(src)
    # absolute top row of ascii content = off_y of its range. Accented cell off_y=0,
    # so shift down by e['oy'] to keep the same screen position.
    shift = e['oy']
    grid=[[False]*hp_acc for _ in range(W)]
    for x in range(W):
        for y in range(e['hp']):
            ny=y+shift
            if 0<=ny<hp_acc: grid[x][ny]=src[x][y]
    return grid, e['adv']

def extents(grid):
    """min/max x with any set pixel; min/max set row."""
    xs=[x for x,col in enumerate(grid) if any(col)]
    ys=[y for col in grid for y,v in enumerate(col) if v]
    return (min(xs),max(xs),min(ys),max(ys)) if xs else (0,0,0,0)

# ---- extract a mark layer from an existing accented letter -----------------
def extract_mark(g, accented_cp, base_cp, hp_acc):
    """Diff accented vs base to isolate the mark pixels (as set of (x,y))."""
    ga,_ = to_grid(g, accented_cp, hp_acc)
    gb,_ = to_grid(g, base_cp, hp_acc)
    mark=set()
    W=min(len(ga),len(gb))
    for x in range(W):
        for y in range(hp_acc):
            if ga[x][y] and not gb[x][y]:
                mark.add((x,y))
    # also any extra columns in accented (rare)
    return mark

def stamp(grid, mark, dx=0, dy=0):
    for (x,y) in mark:
        nx,ny=x+dx,y+dy
        if 0<=nx<len(grid) and 0<=ny<len(grid[0]):
            grid[nx][ny]=True

def center_mark(grid, mark):
    """Center a mark (set of x,y) horizontally over the base glyph's ink."""
    x0,x1,_,_ = extents(grid)
    mxs=[x for x,_ in mark]
    if not mxs: return mark
    mc=(min(mxs)+max(mxs))/2; bc=(x0+x1)/2
    dx=round(bc-mc)
    return {(x+dx,y) for x,y in mark}

# ---- Common EU Latin target set (Latin Extended-A) ------------------------
# Polish, Czech/Slovak, Hungarian, Croatian, Romanian, Turkish.
TARGETS = (
    "ĄąĆćĘęŁłŃńŚśŹźŻż"      # Polish
    "ČčĎďĚěŇňŘřŠšŤťŮůŽž"    # Czech
    "ĹĺĽľŔŕ"                # Slovak extra
    "ŐőŰű"                  # Hungarian
    "Đđ"                    # Croatian extra
    "ĂăŞşŢţ"                # Romanian
    "Ğğİı"                  # Turkish extra
)

# combining-mark codepoint -> our mark name (from NFD decomposition)
COMB = {0x301:'acute',0x300:'grave',0x302:'circ',0x303:'tilde',0x308:'diaer',
        0x30A:'ring',0x327:'cedilla',0x30C:'caron',0x306:'breve',0x30B:'dacute',
        0x328:'ogonek',0x307:'dot'}

def hand_marks(hp):
    """Hand-designed marks for the size whose accented cell height is hp.
    Coordinates are (x,y) with a local origin; centered over the base later.
    Top marks live in rows 0..2; below marks near row hp-1."""
    if hp == 9:   # Body (size 8)
        return {
            'caron':  {(0,0),(2,0),(1,1)},          # ˇ
            'breve':  {(0,0),(2,0),(0,1),(2,1),(1,2)},  # ˘ (cup)
            'dacute': {(1,2),(2,0),(3,2),(4,0)},     # ˝ two acutes
            'dot':    {(1,0),(1,1)},                 # ˙
            'ogonek': {(0,7),(0,8),(1,8)},           # ˛ below (placed at base right)
        }
    # Subtitle (size larger); scale up roughly
    top = 0
    return {
        'caron':  {(0,top),(1,top),(3,top),(4,top),(1,top+1),(3,top+1),(2,top+2)},
        'breve':  {(0,top),(0,top+1),(4,top),(4,top+1),(1,top+2),(2,top+2),(3,top+2)},
        'dacute': {(1,top+2),(2,top),(4,top+2),(5,top)},
        'dot':    {(2,top),(3,top),(2,top+1),(3,top+1)},
        'ogonek': {(0,hp-2),(0,hp-1),(1,hp-1)},
    }

ASCENDERS = set("bdfhklťĺľ")  # letters tall enough that a mark-above won't fit

def synth(g, cp, hp):
    """Return (grid, adv) for one Extended-A codepoint, or None if unhandled."""
    ch = chr(cp)
    # specials without a clean NFD base+mark
    if ch == 'ł':           # l with stroke: widen by 1 col, add a right stub at mid
        grid,adv = to_grid(g, ord('l'), hp)
        x0,x1,y0,y1 = extents(grid)
        midy = (y0+y1)//2
        grid.append([False]*hp)
        grid[x1+1][midy]=True
        return grid, adv+1
    if ch == 'Ł':           # L with stroke through the stem
        grid,adv = to_grid(g, ord('L'), hp)
        x0,x1,y0,y1 = extents(grid)
        midy = (y0+y1)//2
        grid[x0+1][midy]=True
        if x0+2 < len(grid): grid[x0+2][midy-1]=True
        return grid,adv
    if ch in ('ď','ť','ľ'):  # Czech: caron rendered as apostrophe to the right
        base = {'ď':'d','ť':'t','ľ':'l'}[ch]
        grid,adv = to_grid(g, ord(base), hp)
        grid.append([False]*hp); grid.append([False]*hp)
        col = len(grid)-1
        grid[col][1]=True; grid[col][2]=True   # short vertical tick, top-right
        return grid, adv+1
    if ch in ('ĺ','Ĺ'):      # l/L with acute, kept clear of the stem (top-right)
        base = 'l' if ch=='ĺ' else 'L'
        grid,adv = to_grid(g, ord(base), hp)
        x0,x1,_,_ = extents(grid)
        grid.append([False]*hp)
        c=len(grid)-1
        grid[c][0]=True; grid[c-1][1]=True if c-1>=0 else False
        return grid, adv+1
    if ch == 'đ' or ch == 'Đ':
        src = ord('d' if ch=='đ' else 'D')
        grid,adv = to_grid(g, src, hp)
        x0,x1,y0,y1 = extents(grid)
        bar = (y0+ (1 if ch=='đ' else 1))
        for x in range(max(0,x0-1), min(len(grid), x1+1)):
            grid[x][bar]=True
        return grid,adv
    if ch == 'ı':  # dotless i
        grid,adv = to_grid(g, ord('i'), hp)
        x0,x1,y0,y1 = extents(grid)
        # remove the dot (topmost ink run)
        for x in range(len(grid)):
            if grid[x][y0]: grid[x][y0]=False
        return grid,adv
    if ch == 'İ':  # dotted capital I
        grid,adv = to_grid(g, ord('I'), hp)
        x0,x1,_,_ = extents(grid)
        grid[(x0+x1)//2][0]=True
        return grid,adv
    # generic: NFD -> base + combining mark
    d = unicodedata.normalize('NFD', ch)
    if len(d)<2: return None
    base = d[0]; marks=[COMB.get(ord(c)) for c in d[1:]]
    if ord(base) not in g or None in marks: return None
    grid,adv = to_grid(g, ord(base), hp)
    reuse = {'acute':(0xE9,ord('e')),'grave':(0xE0,ord('a')),'circ':(0xEA,ord('a')),
             'tilde':(0xF1,ord('n')),'diaer':(0xE4,ord('a')),'ring':(0xE5,ord('a')),
             'cedilla':(0xE7,ord('c'))}
    hm = hand_marks(hp)
    for mk in marks:
        if mk in reuse:
            acc,b = reuse[mk]; m = extract_mark(g, acc, b, hp)
        elif mk in hm:
            m = hm[mk]
        else:
            return None
        if mk in ('ogonek',):
            x0,x1,_,_ = extents(grid)
            m = {(x+x1-1,y) for x,y in m}   # anchor below-right
        else:
            m = center_mark(grid, m)
        stamp(grid, m)
    return grid,adv

def preview(g, hp, out):
    from PIL import Image, ImageDraw
    items=[]
    for ch in TARGETS:
        r=synth(g, ord(ch), hp)
        items.append((ch, r))
    SC=9; cols=16; pad=4; cellw=10*SC; cellh=(hp+4)*SC
    rows=(len(items)+cols-1)//cols
    img=Image.new('RGB',(cols*cellw+pad, rows*cellh+pad),(30,30,40))
    d=ImageDraw.Draw(img)
    for i,(ch,r) in enumerate(items):
        cx=(i%cols)*cellw+pad; cy=(i//cols)*cellh+pad
        d.rectangle([cx,cy,cx+cellw-2,cy+cellh-2],outline=(70,70,90))
        if r is None:
            d.text((cx+2,cy+2),'?',fill=(255,80,80)); continue
        grid,adv=r
        for x,col in enumerate(grid):
            for y,v in enumerate(col):
                if v:
                    px=cx+2+x*SC; py=cy+2+y*SC
                    d.rectangle([px,py,px+SC-2,py+SC-2],fill=(230,230,235))
        d.text((cx+2,cy+cellh-12),ch,fill=(150,200,255))
    img.save(out)
    print("wrote",out,"unhandled=",[ch for ch,r in items if r is None])

# ---- emit a bwfont range and merge it into the generated .c ----------------
EXTA_FIRST, EXTA_LAST = 0x100, 0x17F   # Latin Extended-A block (contiguous range)

def encode_range(g, hp):
    """Build (glyph_data, glyph_offsets, glyph_widths) for the Extended-A range.
    Non-target codepoints become empty (0 cols / 0 width) -> block fallback."""
    hb = (hp+7)//8
    data=[]; offsets=[0]; widths=[]
    for cp in range(EXTA_FIRST, EXTA_LAST+1):
        r = synth(g, cp, hp) if chr(cp) in TARGETS else None
        if r is None:
            widths.append(0); offsets.append(offsets[-1]); continue
        grid,adv = r
        for col in grid:
            for b in range(hb):
                byte=0
                for y in range(b*8, min(b*8+8, hp)):
                    if y < len(col) and col[y]: byte |= 1<<(y-b*8)
                data.append(byte)
        widths.append(adv); offsets.append(offsets[-1]+len(grid))
    return data, offsets, widths, hb

def _fmt(name, vals, typ, perline):
    out=[f"static const {typ} {name}[{len(vals)}] PROGMEM = {{"]
    for i in range(0,len(vals),perline):
        chunk=vals[i:i+perline]
        out.append("    "+", ".join(("0x%02x"%v if typ=='uint8_t' else str(v)) for v in chunk)+",")
    out.append("};")
    return "\n".join(out)

def merge(path, hp):
    g,ranges = build_font(path)
    short = re.search(r'mf_bwfont_(\w+)_glyph_data_0', open(path).read()).group(1)
    data,offsets,widths,hb = encode_range(g, hp)
    pfx=f"mf_bwfont_{short}"
    arrays = "\n".join([
        "/* [exta] Synthesized Latin Extended-A (Common EU Latin). build_exta.py */",
        _fmt(f"{pfx}_glyph_data_exta", data, 'uint8_t', 16),
        _fmt(f"{pfx}_glyph_offsets_exta", offsets, 'uint16_t', 8),
        _fmt(f"{pfx}_glyph_widths_exta", widths, 'uint8_t', 16),
        "/* [/exta] */",
    ])
    entry = "\n".join([
        "    /* [exta] */",
        "    {",
        f"        {EXTA_FIRST}, /* first char */",
        f"        {EXTA_LAST-EXTA_FIRST+1}, /* char count */",
        "        0, /* offset x */",
        "        0, /* offset y */",
        f"        {hb}, /* height in bytes */",
        f"        {hp}, /* height in pixels */",
        "        0, /* width */",
        f"        {pfx}_glyph_widths_exta, /* glyph widths */",
        f"        {pfx}_glyph_offsets_exta, /* glyph offsets */",
        f"        {pfx}_glyph_data_exta, /* glyph data */",
        "    },",
        "    /* [/exta] */",
    ])
    src = open(path).read()
    # strip any prior exta blocks (idempotent)
    src = re.sub(r'\n?/\* \[exta\].*?/\* \[/exta\] \*/\n', '\n', src, flags=re.S)
    # recompute range count from the actual table entries (+1 for the one we add)
    tbl0 = re.search(r'mf_bwfont_'+re.escape(short)+r'_char_ranges\[\]\s*=\s*\{(.*?)\n\};', src, re.S)
    base_count = len(re.findall(r'/\* first char \*/', tbl0.group(1)))
    rc_line = re.search(r'(\d+), /\* char range count \*/', src)
    src = src[:rc_line.start()] + f"{base_count+1}, /* char range count */" + src[rc_line.end():]
    # insert arrays before the char_ranges[] table
    anchor = re.search(r'static const struct mf_bwfont_char_range_s mf_bwfont_'+re.escape(short)+r'_char_ranges', src)
    src = src[:anchor.start()] + arrays + "\n\n" + src[anchor.start():]
    # insert range entry just before the table's closing "};"
    tbl = re.search(r'(mf_bwfont_'+re.escape(short)+r'_char_ranges\[\]\s*=\s*\{.*?)(\n\};)', src, re.S)
    src = src[:tbl.end(1)] + "\n" + entry + src[tbl.end(1):]
    open(path,'w').write(src)
    nsyn=sum(1 for w in widths if w)
    print(f"merged {nsyn} glyphs into {path} (range 0x{EXTA_FIRST:X}-0x{EXTA_LAST:X}, +{len(data)+2*len(offsets)+len(widths)} bytes)")

if __name__=='__main__':
    src = sys.argv[1] if len(sys.argv)>1 else 'Body.c'
    g,ranges = build_font(src)
    hp = max(r['hp'] for r in ranges)
    if '--emit' in sys.argv:
        merge(src, hp)
    else:
        print("hp_acc=",hp,"glyphs decoded=",len(g))
        preview(g, hp, '/tmp/exta_preview_'+src.replace('.c','')+'.png')
