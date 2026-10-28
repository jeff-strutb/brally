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
