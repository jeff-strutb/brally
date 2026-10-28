"""Which Top Gear Rally cartridge the N64 tools work against.

    TGR_REGION=usa|eur|jpn   (default usa)
    TGR_ROM=<path>           overrides the ROM file; it must still be that region's

The decomp has one source tree.  USA is the base: every address written in
src/tgrally (an `@implements` VA, a func_/D_ name, config/tgrally/symbols_tgr.csv)
is a USA address.  Europe and Japan are sub-variants built from the same
source; their tools translate each USA address through
config/tgrally/<region>/addrmap.csv (tools/tgrally/regionmap.py writes it) and
grade against that region's ROM.

Layout, per ROM (verified against each cartridge's entry stub, which zeroes
.bss, and the end of the code before the libultra exception vectors' data):
one resident segment at 0x80200000 from ROM 0x1000, .text, then .data/.rodata
up to the start of .bss.
"""
import hashlib
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

REGIONS = {
    'usa': dict(name='Top Gear Rally (USA)', code='NGRE',
                sha1='bfc51f086ebdb78904dc24ad0c3a2d946e26d022',
                text_e=0x70AB0, bss_s=0x802AC400, bss_size=0xD67B0),
    'eur': dict(name='Top Gear Rally (Europe)', code='NTRP',
                sha1='9fae5d98fe3464f00d59034690df90118646eb95',
                text_e=0x72A80, bss_s=0x802AE0F0, bss_size=0xD6A40),
    'jpn': dict(name='Top Gear Rally (Japan)', code='NTRJ',
                sha1='e0415b87c5a5b69ac581e79137406bdaa79b354a',
                text_e=0x72530, bss_s=0x802ACB30, bss_size=0xD6C70),
}

BASE, ROMOFF = 0x80200000, 0x1000

REGION = os.environ.get('TGR_REGION', 'usa').lower()
if REGION not in REGIONS:
    raise SystemExit('TGR_REGION=%s: one of %s' % (REGION, ', '.join(REGIONS)))
R = REGIONS[REGION]

TEXT_S, TEXT_E = ROMOFF, R['text_e']                 # ROM offsets of .text
BSS_S, BSS_E = R['bss_s'], R['bss_s'] + R['bss_size']
DATA_E = BSS_S - BASE + ROMOFF                       # ROM end of .data/.rodata
DATA_VA = TEXT_E - ROMOFF + BASE                     # vram start of .data/.rodata


def rom_path(region=None):
    region = region or REGION
    if region == REGION and os.environ.get('TGR_ROM'):
        return os.environ['TGR_ROM']
    return os.path.join(ROOT, 'reference/tgrally', REGIONS[region]['name'] + '.z64')


ROM_PATH = rom_path()


def check_rom(path=ROM_PATH, region=None):
    """Refuse a ROM that is not this region's cartridge (.z64 byte order)."""
    region = region or REGION
    if not os.path.exists(path):
        raise SystemExit('%s: missing; %s needs %s.z64' % (path, region, REGIONS[region]['name']))
    got = hashlib.sha1(open(path, 'rb').read()).hexdigest()
    if got != REGIONS[region]['sha1']:
        raise SystemExit('%s is not %s (sha1 %s)' % (path, REGIONS[region]['name'], got))


def config_dir(region=None):
    """USA's records live in config/tgrally; a sub-variant's in config/tgrally/<region>."""
    region = region or REGION
    d = os.path.join(ROOT, 'config/tgrally')
    return d if region == 'usa' else os.path.join(d, region)


def build_dir(region=None):
    region = region or REGION
    return os.path.join(ROOT, 'build/tgrally', 'n64' if region == 'usa' else 'n64-' + region)


_MAP = None


def addrmap():
    """USA VA -> this region's VA (identity for USA).  Built by regionmap.py."""
    global _MAP
    if _MAP is None:
        _MAP = {}
        if REGION != 'usa':
            import csv
            p = os.path.join(config_dir(), 'addrmap.csv')
            if not os.path.exists(p):
                raise SystemExit('%s missing: run tools/tgrally/regionmap.py %s' % (p, REGION))
            for r in csv.DictReader(open(p)):
                if r['va']:
                    _MAP[int(r['usa'], 16)] = int(r['va'], 16)
    return _MAP


def to_region(usa_va):
    """A USA address in this region, or None when it has no counterpart."""
    if REGION == 'usa':
        return usa_va
    return addrmap().get(usa_va)


# ------------------------------------------------------------------- tags
# `@implements 0x80XXXXXX tgr Name` names a USA address: the function every
# region has (its body may differ under `#if TGR_REGION_<R>`).  A function
# only one region has is tagged with that region's own address,
# `@implements eur:0x80XXXXXX tgr Name`, and its definition sits under
# `#if TGR_REGION_EUR`; the other regions' tools skip the tag.
TAG = re.compile(r'@implements\s+(?:(usa|eur|jpn):)?(0x[0-9A-Fa-f]{8})\s+tgr\s+(\w+)')


def tags(src):
    """-> [(usa_va, va, name, line)] for every tag this region builds:
    usa_va is the written USA address (None for a region-only tag), va its
    home in this region's ROM (None when the region has no counterpart)."""
    out = []
    for i, l in enumerate(src.split('\n')):
        m = TAG.search(l)
        if not m:
            continue
        q, v = m.group(1), int(m.group(2), 16)
        if q is None:
            out.append((v, to_region(v), m.group(3), i + 1))
        elif q == REGION:
            out.append((None, v, m.group(3), i + 1))
    return out


T3TAG = re.compile(r'@t3\s+(?:(usa|eur|jpn):)?(0x[0-9A-Fa-f]{8})')


def t3_tags(src):
    """-> {this region's VA} for every `@t3` tag in src, written like the
    @implements tags (a USA address, or `eur:0x80XXXXXX` for a region-only
    function)."""
    out = set()
    for q, v in T3TAG.findall(src):
        v = int(v, 16)
        if q == '':
            v = to_region(v)
        elif q != REGION:
            continue
        if v is not None:
            out.add(v)
    return out


def tag_line(src, va):
    """The text offset of the @implements tag that builds this region's va."""
    for u, v, n, ln in tags(src):
        if v == va:
            return sum(len(l) + 1 for l in src.split('\n')[:ln - 1])
    return -1


def fenced():
    """This region's FENCED library functions (region VA -> library): the
    USA list (config/tgrally/fenced_tgr.csv) translated, plus the region's
    own (config/tgrally/<region>/fenced_tgr.csv, region addresses)."""
    import csv
    out = {}
    for r in csv.DictReader(open(os.path.join(ROOT, 'config/tgrally/fenced_tgr.csv'))):
        v = to_region(int(r['va'], 16))
        if v is not None:
            out[v] = r['library']
    p = os.path.join(config_dir(), 'fenced_tgr.csv')
    if REGION != 'usa' and os.path.exists(p):
        for r in csv.DictReader(open(p)):
            out[int(r['va'], 16)] = r['library']
    return out
