#!/usr/bin/env python3
"""provcheck.py -- provenance hygiene for the repository.

The repository records what the project found and how; it carries no
tooling provenance.  This check refuses a fixed vocabulary of tool, vendor
and workflow names (kept encoded below so this file does not trip itself),
wiki-style note links, em and en dashes, a reserved set of path names, and,
in binary files, embedded content-credential blocks.  The racing game's own
computer-driven opponents and the N64 audio interface are not affected.

    provcheck.py                 every tracked file
    provcheck.py PATH...         these files or directories (tracked or not)
    provcheck.py --staged        the index (pre-commit hook)
    provcheck.py --msg FILE      a commit message (commit-msg hook)
    provcheck.py --history       every commit message and ref name in the repository
Exit 1 when anything is found; each finding prints as path:line: [terms] text.
"""
import base64, os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

def _rx(b64, flags=0):
    return re.compile(base64.b64decode(b64).decode(), flags)

WORDS = _rx('Y2xhdWRlfGFudGhyb3BpY3xvcGVuYWl8Y2hhdC0/Z3B0fGdwdC0/WzAtOV1bYS16MC05Ll0qfGdyb2t8Z2VtaW5pfGNvcGlsb3R8bGxtcz98bGFyZ2UgbGFuZ3VhZ2UgbW9kZWxzP3xsYW5ndWFnZSBtb2RlbHM/fGFydGlmaWNpYWwgaW50ZWxsaWdlbmNlfHN1Yi0/YWdlbnRzP3xhZ2VudHM/fHNjcmF0Y2hwYWRzP3xjby1hdXRob3JlZC1ieXxnZW5lcmF0ZWQgd2l0aHxvcmlnaW5zZXNzaW9uaWR8dG9vbC1yZXN1bHRzfGNsYXVkZVwubWR8c29ubmV0fG9wdXN8ZmFibGU=')
WORDS = re.compile(r'(?i)(?<!user-)\b(?:' + WORDS.pattern + r')\b')
TOOL = _rx('XGIoPzphbnxieXx2aWF8d2l0aHx1c2luZ3xvdXIpIEFJXGIoPyEgKD86Y2FyfGNhcnN8c2xvdHxzbG90c3xvbmV8b25lc3xkcml2ZXJ8ZHJpdmVyc3xvcHBvbmVudHxvcHBvbmVudHN8ZW50aXR5fGVudGl0aWVzfHBsYXllcnxwbGF5ZXJzfHJhY2VyfHJhY2Vyc3xnbG9iYWx8Z2xvYmFscykpfFxiQUlbLSBdKD86YXNzaXN0ZWR8Z2VuZXJhdGVkfHdyaXR0ZW58YXV0aG9yZWR8bW9kZWxzP3xhZ2VudHM/fHRvb2xzP3x0b29saW5nfGFzc2lzdGFudHM/fHBhc3N8c2Vzc2lvbnM/fGhlbHBlcnM/KVxi', re.I)
BIN = re.compile(base64.b64decode('KD9pKWMycGF8anVtYmZ8Y29udGVudGF1dGh8Y2xhdWRlfGFudGhyb3BpY3xvcGVuYWl8dHJhaW5lZEFsZ29yaXRobWljTWVkaWE='))
PATHS = _rx('KD9pKV4oPzpcLmNsYXVkZXxjbGF1ZGVcLm1kKSQ=')
REFS = _rx('KD9pKWNsYXVkZXwvYWlbLy1dfGFnZW50')
WIKI = re.compile(r'\[\[[a-z0-9]+(?:-[a-z0-9]+)+\]\]')
DASH = re.compile('[%s]' % ''.join(map(chr, (0x2012, 0x2013, 0x2014, 0x2015))))
SKIP_DIRS = {'.git', '__pycache__', 'venv', '.venv', 'node_modules'}

def git(*a):
    return subprocess.run(['git', '-C', ROOT] + list(a), capture_output=True).stdout

def check_text(name, text):
    found = []
    for i, line in enumerate(text.splitlines(), 1):
        hits = [m.group(0) for rx in (WORDS, TOOL, WIKI) for m in rx.finditer(line)]
        if DASH.search(line):
            hits.append('dash')
        if hits:
            found.append('%s:%d: [%s] %s' % (name, i, ', '.join(sorted(set(hits))), line.strip()[:160]))
    return found

def check_file(rel, data):
    found = ['%s:0: [path]' % rel] if any(PATHS.match(x) for x in rel.replace('\\', '/').split('/')) else []
    if b'\0' not in data[:8192]:
        return found + check_text(rel, data.decode('utf-8', 'replace'))
    m = BIN.search(data)
    return found + (['%s:0: [binary: %s]' % (rel, m.group(0).decode(errors='replace'))] if m else [])

def files_under(paths):
    for p in paths:
        if os.path.isdir(p):
            for dp, dn, fn in os.walk(p):
                dn[:] = [d for d in dn if d not in SKIP_DIRS]
                for f in fn:
                    q = os.path.join(dp, f)
                    if os.path.isfile(q):
                        yield q
        elif os.path.isfile(p):
            yield p

def main(argv):
    findings = []
    if argv[:1] == ['--msg']:
        text = '\n'.join(l for l in open(argv[1], encoding='utf-8', errors='replace').read().splitlines()
                         if not l.startswith('#'))
        findings = check_text('commit message', text)
    elif argv[:1] == ['--history']:
        log = git('log', '--all', '--format=%H%x00%B%x01').decode('utf-8', 'replace')
        for rec in log.split('\x01'):
            if '\0' in rec:
                h, body = rec.strip('\n').split('\0', 1)
                findings += check_text('commit ' + h[:10], body)
        findings += ['ref %s: [ref name]' % n for n in git('for-each-ref', '--format=%(refname)').decode().split()
                     if REFS.search(n)]
    elif argv[:1] == ['--staged']:
        for rel in [p for p in git('diff', '--cached', '--name-only', '--diff-filter=ACMR', '-z').decode().split('\0') if p]:
            findings += check_file(rel, git('show', ':' + rel))
    elif argv:
        for p in files_under(argv):
            findings += check_file(p, open(p, 'rb').read())
    else:
        for rel in [p for p in git('ls-files', '-z').decode().split('\0') if p]:
            p = os.path.join(ROOT, rel)
            if os.path.isfile(p) and not os.path.islink(p):
                findings += check_file(rel, open(p, 'rb').read())
    for f in findings:
        print(f)
    if findings:
        print('provcheck: %d finding(s)' % len(findings), file=sys.stderr)
    return 1 if findings else 0

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
