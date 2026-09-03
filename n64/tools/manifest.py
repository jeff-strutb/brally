"""Document every Top Gear Rally function: what it is, and which module it is in.

The N64 target has no source tree of its own -- the code is the PC decomp's
source compiled a second time -- so "filing a function into its module" here
means recording the assignment, not moving a file.  This writes that record.

  .venv/bin/python n64/tools/manifest.py

  n64/config/functions_tgr.csv   every function in .text, one row
  n64/docs/modules.md            the architectural map, with denominators

A function's module is inherited from the PC source file it matched, so it is
evidence, not guesswork.  Functions the sweep has not located get a module by
CONTIGUITY: IDO emits one translation unit's functions consecutively, so a gap
bracketed by confirmed members of the same module is very probably more of that
module.  Those rows are marked `inferred` and are never counted as confirmed.

Columns: vram, size, module, how, name, status, pc_va, source
  how     confirmed (matched into that module) | inferred (bracketed) | -
  status  EXACT (byte-exact) | SHAPE (located) | - (not located)
"""
import sys, os, csv, collections, argparse

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'n64/tools'))
os.environ.setdefault('TGR_ROM', os.path.join(ROOT, 'reference/tgrally/Top Gear Rally (USA).z64'))
import n64rom  # noqa: E402

REPORT = os.path.join(ROOT, 'build/n64/report.csv')
OUT_CSV = os.path.join(ROOT, 'n64/config/functions_tgr.csv')
OUT_MD = os.path.join(ROOT, 'n64/docs/modules.md')

# What each architectural area of the shared engine is, in one line.  These are
# the src/core/ folder names the PC decomp already files into; the N64 build is
# the same engine, so the same areas apply.
AREAS = {
    'geometry':  'vector, matrix and quaternion math',
    'drawing':   'display-list building, sprites, fonts, image blitting',
    'scene':     'scene graph, entities, culling, track scenery',
    'racing':    'race state machine, laps, gates, timing, results',
    'driving':   'car physics, collision, surfaces, AI drivers',
    'audio':     'music and sound-effect mixing, sequencing, output',
    'menus':     'front-end menu pages, navigation, UI widgets',
    'controls':  'controller input and control-name mapping',
    'gamedata':  'save files, config, string resources, CRT helpers',
    'settings':  'video/audio settings and their persistence',
    'startup':   'boot, init and the main loop',
    'unfiled':   'located, but its PC twin still lives in an unfiled address '
                 'batch (a sliceN file) rather than a named module',
    'backend':   'original platform layer -- on PC this is Glide/D3D/Win32, so '
                 'an N64 hit here means the shared part of that seam',
}


def module_of(src):
    """PC source path -> architectural area."""
    if not src:
        return ''
    parts = src.replace('\\', '/').split('/')
    base = parts[-1]
    if base.startswith('slice') or base in ('tiny_stubs.c', 'ghidra_batch.c'):
        return 'unfiled'
    if 'backends' in parts:
        return 'backend'
    if 'core' in parts:
        i = parts.index('core')
        if i + 1 < len(parts) - 1:
            return parts[i + 1]
    return 'unfiled'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--report', default=REPORT)
    args = ap.parse_args()

    if not os.path.exists(args.report):
        sys.exit("no sweep report at %s -- run n64/tools/n64match.py --all first"
                 % args.report)

    # ---- the ROM's own function list, with sizes
    F, end = n64rom.F, n64rom.r2v(n64rom.TEXT_E)
    size = {v: (F[i + 1] if i + 1 < len(F) else end) - v for i, v in enumerate(F)}

    # ---- what the sweep located
    found = {}
    for r in csv.DictReader(open(args.report)):
        if r['n64_va']:
            found[int(r['n64_va'], 16)] = r

    rows = []
    for v in F:
        r = found.get(v)
        if r:
            rows.append(dict(vram='%08X' % v, size=size[v],
                             module=module_of(r['file']), how='confirmed',
                             name=r['fn'], status=r['status'],
                             pc_va='', source=r['file']))
        else:
            rows.append(dict(vram='%08X' % v, size=size[v], module='', how='',
                             name='', status='', pc_va='', source=''))

    # ---- contiguity fill: a gap bracketed by one module belongs to it
    idx = {r['vram']: i for i, r in enumerate(rows)}
    i = 0
    while i < len(rows):
        if rows[i]['module']:
            i += 1
            continue
        j = i
        while j < len(rows) and not rows[j]['module']:
            j += 1
        before = rows[i - 1]['module'] if i > 0 else None
        after = rows[j]['module'] if j < len(rows) else None
        if before and before == after and before != 'unfiled':
            for k in range(i, j):
                rows[k]['module'] = before
                rows[k]['how'] = 'inferred'
        i = j

    os.makedirs(os.path.dirname(OUT_CSV), exist_ok=True)
    with open(OUT_CSV, 'w', newline='') as f:
        w = csv.DictWriter(f, ['vram', 'size', 'module', 'how', 'name',
                               'status', 'pc_va', 'source'])
        w.writeheader()
        w.writerows(rows)

    # ---- the architectural map
    TEXT = n64rom.TEXT_E - n64rom.TEXT_S
    agg = collections.defaultdict(lambda: collections.Counter())
    for r in rows:
        m = r['module'] or '(unidentified)'
        a = agg[m]
        a['fns'] += 1
        a['bytes'] += r['size']
        if r['how'] == 'confirmed':
            a['confirmed'] += 1
            a['confirmed_bytes'] += r['size']
        if r['status'] == 'EXACT':
            a['exact'] += 1
            a['exact_bytes'] += r['size']

    order = sorted(agg, key=lambda m: -agg[m]['bytes'])
    with open(OUT_MD, 'w') as f:
        f.write("# Top Gear Rally (N64) — architectural map\n\n")
        f.write("Generated by `n64/tools/manifest.py`; the per-function record is\n"
                "`n64/config/functions_tgr.csv`. Do not hand-edit either.\n\n")
        f.write("The N64 build has no source tree of its own — it is the PC decomp's\n"
                "source compiled a second time — so a function's module is inherited\n"
                "from the PC source file it matched. `confirmed` means exactly that.\n"
                "`inferred` rows are unlocated functions bracketed on both sides by one\n"
                "module, which IDO's contiguous per-TU emission makes likely but does\n"
                "not prove; they are never counted as confirmed.\n\n")
        f.write("**Denominator: `.text` is %s bytes across %d functions.**\n\n"
                % (format(TEXT, ','), len(F)))
        f.write("| module | what it is | fns | bytes | of .text | confirmed | byte-exact |\n")
        f.write("|---|---|---:|---:|---:|---:|---:|\n")
        for m in order:
            a = agg[m]
            f.write("| `%s` | %s | %d | %s | %.2f%% | %d | %d |\n"
                    % (m, AREAS.get(m, '—'), a['fns'], format(a['bytes'], ','),
                       100.0 * a['bytes'] / TEXT, a['confirmed'], a['exact']))
        tot = sum(agg[m]['confirmed'] for m in agg)
        totb = sum(agg[m]['confirmed_bytes'] for m in agg)
        ex = sum(agg[m]['exact'] for m in agg)
        exb = sum(agg[m]['exact_bytes'] for m in agg)
        f.write("\n**Confirmed into a module: %d functions, %s bytes (%.2f%% of .text).**\n"
                % (tot, format(totb, ','), 100.0 * totb / TEXT))
        f.write("**Byte-exact: %d functions, %s bytes (%.2f%% of .text).**\n"
                % (ex, format(exb, ','), 100.0 * exb / TEXT))

        f.write("\n## Byte-exact functions\n\n")
        f.write("| vram | size | module | name |\n|---|---:|---|---|\n")
        for r in rows:
            if r['status'] == 'EXACT':
                f.write("| `%s` | %d | `%s` | `%s` |\n"
                        % (r['vram'], r['size'], r['module'], r['name']))

    # ---- architectural folders, one per area actually present in the ROM.
    # They mirror src/core/'s areas so the N64 target browses like the PC one.
    # Each carries a generated README: what the area is, what is confirmed in
    # the ROM, and where the shared source that produced it actually lives.
    # N64-only platform code (RSP/RDP, audio ucode, OS glue) has no PC twin and
    # will be written into these folders as it is recovered.
    srcroot = os.path.join(ROOT, 'n64/src')
    os.makedirs(srcroot, exist_ok=True)
    with open(os.path.join(srcroot, 'README.md'), 'w') as f:
        f.write("# n64/src/ — architectural areas of the Top Gear Rally build\n\n"
                "One folder per area of the engine, matching `src/core/`'s layout so\n"
                "the two targets browse the same way.\n\n"
                "**These folders are mostly empty by design.** The shared engine is one\n"
                "source, and it lives in `src/core/` — the N64 build compiles that same\n"
                "source with IDO. Duplicating it here would create a second copy to keep\n"
                "in sync, which is exactly what the single-source premise avoids. Each\n"
                "folder's README records what is confirmed in the ROM for that area and\n"
                "points at the source that produced it.\n\n"
                "What *will* be written here is **N64-only platform code** — display-list\n"
                "building against the RSP, the audio driver, OS glue — which has no PC\n"
                "twin to share. Of `.text`, %.2f%% is still unidentified and that is\n"
                "where most of it lives.\n\n"
                "Generated by `n64/tools/manifest.py`. See `../docs/modules.md`.\n"
                % (100.0 * agg['(unidentified)']['bytes'] / TEXT))
    for m in order:
        if m in ('(unidentified)',):
            continue
        a = agg[m]
        dd = os.path.join(srcroot, m)
        os.makedirs(dd, exist_ok=True)
        srcs = sorted({r['source'] for r in rows
                       if r['module'] == m and r['source']})
        with open(os.path.join(dd, 'README.md'), 'w') as f:
            f.write("# %s — %s\n\n" % (m, AREAS.get(m, '')))
            f.write("Confirmed in the Top Gear Rally ROM: **%d functions, %s bytes "
                    "(%.2f%% of the %s-byte `.text`)**, of which **%d are byte-exact**.\n"
                    % (a['confirmed'], format(a['confirmed_bytes'], ','),
                       100.0 * a['confirmed_bytes'] / TEXT, format(TEXT, ','),
                       a['exact']))
            if a['fns'] > a['confirmed']:
                f.write("\nA further %d function(s) in this address range are "
                        "`inferred` — unlocated, but bracketed by confirmed members "
                        "of this module. Not counted as confirmed.\n"
                        % (a['fns'] - a['confirmed']))
            f.write("\nThe source that produced these is shared with the PC decomp "
                    "and lives in:\n\n")
            for s in srcs:
                f.write("  - `%s`\n" % s)
            f.write("\nN64-only code for this area, once recovered, belongs in this "
                    "folder. Per-function detail is in `n64/config/functions_tgr.csv`.\n")

    print("wrote %s (%d functions)" % (os.path.relpath(OUT_CSV, ROOT), len(rows)))
    print("wrote %s" % os.path.relpath(OUT_MD, ROOT))
    print("wrote n64/src/ — %d area folders" % (len(order) - 1))
    print()
    for m in order:
        a = agg[m]
        print("  %-16s %4d fns  %8s B  %5.2f%%   confirmed %3d  exact %2d"
              % (m, a['fns'], format(a['bytes'], ','),
                 100.0 * a['bytes'] / TEXT, a['confirmed'], a['exact']))


if __name__ == '__main__':
    main()
