#!/usr/bin/env python3
"""Fold each core file's alias header (ports/brally/alias/<file>.h) into the
file itself, so the source names its canonical objects directly and the
build needs no per-file force-include.

Every identifier token outside comments and string/char literals that the
file's alias header defines is replaced by the macro's expansion, expanded
the way the preprocessor would (other aliases inside it expanded too, a
name never re-expanded inside its own expansion).

Usage: inline_alias.py [--dry] [FILE...]
"""
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))
CORE = 'ports/brally/src/core/'
ALIAS = 'ports/brally/alias'
IDENT = re.compile(r'[A-Za-z_]\w*')


def tokens(text):
    """Yield (kind, s) where kind is 'id' for identifiers and 'x' otherwise."""
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith('//', i):
            j = text.find('\n', i)
            j = n if j < 0 else j
            yield 'x', text[i:j]
            i = j
        elif text.startswith('/*', i):
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            yield 'x', text[i:j]
            i = j
        elif c in '"\'':
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == '\\' else 1
            yield 'x', text[i:j + 1]
            i = j + 1
        elif c.isalpha() or c == '_':
            m = IDENT.match(text, i)
            yield 'id', m.group(0)
            i = m.end()
        elif c.isdigit():
            m = re.compile(r'[0-9A-Za-z_.]+').match(text, i)
            yield 'x', m.group(0)
            i = m.end()
        else:
            yield 'x', c
            i += 1


def expand(name, macros, off):
    body = macros[name]
    off = off | {name}
    return ''.join(expand(s, macros, off) if k == 'id' and s in macros and s not in off else s
                   for k, s in tokens(body))


def load(path):
    macros = {}
    for line in open(path, encoding='latin-1'):
        m = re.match(r'#define\s+(\w+)\s+(.*?)\s*(/\*.*\*/)?\s*$', line)
        if m:
            body = m.group(2)
            if m.group(3) and 'BR_LP64_BYTE_VIEW' in m.group(3):
                body += ' /* BR_LP64_BYTE_VIEW */'
            macros[m.group(1)] = body
    return macros


def main():
    os.chdir(ROOT)
    dry = '--dry' in sys.argv
    files = [a for a in sys.argv[1:] if not a.startswith('--')]
    if not files:
        files = []
        for dp, _, fs in os.walk(CORE):
            files += [os.path.join(dp, f) for f in fs if f.endswith(('.c', '.cpp'))]
    total = 0
    for f in sorted(files):
        key = f[len(CORE):].replace('/', '__')
        ah = os.path.join(ALIAS, key + '.h')
        if '--from-git' in sys.argv:
            import subprocess
            open(ah, 'w').write(subprocess.run(['git', 'show', 'HEAD:' + ah], capture_output=True,
                                               text=True).stdout)
        if not os.path.exists(ah):
            continue
        macros = load(ah)
        text = open(f, encoding='latin-1').read()
        # a name the file #defines itself: its own definition wins
        for own in re.findall(r'^\s*#\s*define\s+(\w+)', text, re.M):
            macros.pop(own, None)
        out, n = [], 0
        for k, s in tokens(text):
            if k == 'id' and s in macros:
                out.append(expand(s, macros, frozenset()))
                n += 1
            else:
                out.append(s)
        total += n
        if not dry and n:
            open(f, 'w', encoding='latin-1').write(''.join(out))
        if not dry:
            os.remove(ah)
    print('replaced %d alias uses in %d files' % (total, len(files)))


if __name__ == '__main__':
    main()
