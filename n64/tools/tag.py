"""Record each function's Top Gear Rally (N64) address in the shared source.

The PC decomp documents a function's identity in the code itself, with an
`@implements <va> <binary> <name>` line above the definition.  The N64 build is
the same source compiled a second time, so the twin address belongs in exactly
the same place -- next to the function, where someone reading the code will see
it, rather than only in a CSV.

  .venv/bin/python n64/tools/tag.py [--dry-run] [--undo]

Two tags, because the two strictness levels must never be mixed (rule 4):

  @n64 0x8022439C exact     the IDO build of THIS source reproduces the ROM's
                            bytes for this function
  @n64 0x80224894 located   the function was found in the ROM by structural
                            search, but our source does not yet reproduce its
                            bytes.  A pairing, not a match.

`exact` deliberately does NOT reuse the `@implements` keyword: that tag is
reserved for the PC binary the sweep tooling verifies against, and a second
binary's claim must not be readable as a claim about the first.

Tags are comments, so they cannot affect codegen -- they are removed in
translation phase 3, before the PC match is decided.  Files with uncommitted
edits are skipped, so this never collides with another lane's work in progress.
"""
import sys, os, csv, re, subprocess, argparse, collections

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
REPORT = os.path.join(ROOT, 'build/n64/report.csv')
TAG_RE = re.compile(r'^\s*/\*\s*@n64\s+0x[0-9A-Fa-f]+\s+\w+\s*\*/\s*$')


def dirty_files():
    """Paths another lane may be mid-edit on.

    A file whose only uncommitted change is @n64 tag lines is this tool's own
    previous run, not someone else's work in progress -- so it stays eligible
    and re-running is idempotent.
    """
    out = subprocess.run(['git', 'status', '--porcelain'], cwd=ROOT,
                         capture_output=True, text=True).stdout
    d = set()
    for line in out.splitlines():
        if len(line) <= 3:
            continue
        p = line[3:].strip()
        diff = subprocess.run(['git', 'diff', '--', p], cwd=ROOT,
                              capture_output=True, text=True).stdout
        body = [l for l in diff.split('\n')
                if l.startswith(('+', '-')) and not l.startswith(('+++', '---'))]
        if body and all('@n64' in l for l in body):
            continue                       # our own tags only
        d.add(p)
    return d


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--report', default=REPORT)
    ap.add_argument('--dry-run', action='store_true')
    ap.add_argument('--undo', action='store_true', help='strip every @n64 tag')
    args = ap.parse_args()

    dirty = dirty_files()

    # ---- collect the tag each function should carry
    want = collections.defaultdict(dict)      # file -> {fn name: tag line}
    if not args.undo:
        if not os.path.exists(args.report):
            sys.exit("no sweep report at %s -- run n64/tools/n64match.py --all"
                     % args.report)
        for r in csv.DictReader(open(args.report)):
            if r['status'] not in ('EXACT', 'SHAPE'):
                continue
            kind = 'exact' if r['status'] == 'EXACT' else 'located'
            want[r['file']][r['fn']] = '/* @n64 0x%s %s */' % (r['n64_va'], kind)

    files = sorted(want) if not args.undo else sorted(
        {r['file'] for r in csv.DictReader(open(args.report))} if
        os.path.exists(args.report) else [])
    stat = collections.Counter()
    for rel in files:
        if rel in dirty:
            stat['skipped_dirty'] += 1
            continue
        path = os.path.join(ROOT, rel)
        if not os.path.exists(path):
            continue
        lines = open(path).read().split('\n')

        out, i, changed = [], 0, False
        while i < len(lines):
            ln = lines[i]
            if TAG_RE.match(ln):                       # drop any existing tag
                changed = True
                i += 1
                continue
            if not args.undo:
                # Preferred anchor: the existing @implements line, so the two
                # binaries' claims sit together.
                m = re.search(r'@implements\s+\S+.*?(\w+)\s*\*/', ln)
                # A function can carry several @implements lines (the glide and
                # d3d addresses of the same code).  Tag after the LAST of them
                # so the run stays contiguous.
                # Look past any stale tag left by a previous run, which is
                # dropped above but is still present in `lines`.
                k = i + 1
                while k < len(lines) and TAG_RE.match(lines[k]):
                    k += 1
                nxt = lines[k] if k < len(lines) else ''
                if m and '@implements' in nxt:
                    m = None
                if m and want[rel].get(m.group(1)):
                    fn = m.group(1)
                    out.append(ln)
                    out.append(re.match(r'\s*', ln).group(0) + want[rel].pop(fn))
                    changed = True
                    stat['tagged'] += 1
                    i += 1
                    continue
                # Fallback for functions the PC side has not tagged: the
                # definition itself.  A definition starts at column 0 and its
                # line does not end in ';' (which would be a prototype).
                d = re.match(r'[A-Za-z_][\w \t\*]*?\b(\w+)\s*\(', ln)
                if d and not ln.rstrip().endswith(';') and want[rel].get(d.group(1)):
                    out.append(want[rel].pop(d.group(1)))
                    changed = True
                    stat['tagged'] += 1
            out.append(ln)
            i += 1

        if changed and not args.dry_run:
            open(path, 'w').write('\n'.join(out))
        stat['files'] += 1
        stat['untagged'] += len(want[rel])

    print("files touched      : %d" % stat['files'])
    print("functions tagged   : %d" % stat['tagged'])
    print("no @implements line: %d  (untagged on the PC side, so no anchor)"
          % stat['untagged'])
    if stat['skipped_dirty']:
        print("skipped (uncommitted edits by another lane): %d"
              % stat['skipped_dirty'])
    if args.dry_run:
        print("\n(dry run -- nothing written)")


if __name__ == '__main__':
    main()
