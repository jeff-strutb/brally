#!/usr/bin/env python3
"""C++ files: make C's implicit pointer conversions explicit.

A C file passes an `int *` where a `char *` is expected and the conversion is
implicit; C++ requires the cast. For every
  cannot initialize a parameter of type 'P' with an rvalue of type 'A'
  assigning to 'P' from incompatible type 'A'
  cannot initialize a variable of type 'P' with an rvalue of type 'A'
where both P and A are object-pointer types, the argument / right-hand side
is wrapped in `(P)( ... )` -- exactly the conversion C performs.

Usage: cppcast.py [FILE...]   (default: failing .cpp files of the last build)
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rawscan  # noqa: E402

ROOT = rawscan.ROOT


def is_obj_ptr(t):
    t = t.strip()
    return t.endswith('*') and '(' not in t.replace('(*)', '')


def main():
    os.chdir(ROOT)
    files = sys.argv[1:] or [l.split(None, 1)[1].strip() for l in open('build/portable/compile.txt')
                             if l.startswith('FAIL ') and l.strip().endswith('.cpp')]
    total = 0
    for f in files:
        p = subprocess.run(['clang'] + rawscan.flags_for(f) + ['-ferror-limit=0', '-x', 'c++', '-std=c++98',
                            '-fdiagnostics-print-source-range-info', f],
                           capture_output=True, text=True, errors='replace', cwd=ROOT)
        text = open(f, encoding='latin-1').read()
        lines = text.split('\n')
        starts = [0]
        for l in lines:
            starts.append(starts[-1] + len(l) + 1)
        edits = []
        rx = re.compile(r'^%s:(\d+):(\d+):(\{[^\n]*?\})?: error: (?:cannot initialize a (?:parameter|variable) of type|'
                        r"assigning to) '([^']+)'(?: \(aka '[^']+'\))? (?:with an rvalue of type|from incompatible type) "
                        r"'([^']+)'" % re.escape(f), re.M)
        found = [(m.group(1), m.group(2), m.group(3), m.group(4), m.group(5)) for m in rx.finditer(p.stderr)]
        rx2 = re.compile(r'^%s:(\d+):(\d+):(\{[^\n]*?\})?: error: incompatible pointer types (?:assigning to|initializing) '
                         r"'([^']+)'(?: \(aka '[^']+'\))? (?:from|with an expression of type) '([^']+)'" % re.escape(f), re.M)
        found += [(m.group(1), m.group(2), m.group(3), m.group(4), m.group(5)) for m in rx2.finditer(p.stderr)]
        rx3 = re.compile(r'^%s:(\d+):(\d+):(\{[^\n]*?\})?: error: (?:incompatible pointer types )?passing '
                         r"'([^']+)'(?: \(aka '[^']+'\))? to parameter of type '([^']+)'" % re.escape(f), re.M)
        found += [(m.group(1), m.group(2), m.group(3), m.group(5), m.group(4)) for m in rx3.finditer(p.stderr)]
        # overload resolution failures: cast the named argument
        for m in re.finditer(r'^%s:(\d+):(\d+):(\{[^\n]*?\})?: error: no matching function for call to \'\w+\'\n'
                             r'(?:.*\n)*?.*note: candidate function not viable: no known conversion from \'([^\']+)\''
                             r'(?: \(aka [^)]*\))? to \'([^\']+)\'(?: \(aka [^)]*\))? for (\d+)\w\w argument' % re.escape(f),
                             p.stderr, re.M):
            ln, col, at, pt, nth = int(m.group(1)), int(m.group(2)), m.group(4), m.group(5), int(m.group(6))
            if not (is_obj_ptr(pt) and is_obj_ptr(at)):
                continue
            pos = starts[ln - 1] + col - 1
            k = text.find('(', pos)
            if k == -1:
                continue
            depth, start, args, j = 0, k + 1, [], k
            while j < len(text):
                c = text[j]
                if c in '([{':
                    depth += 1
                elif c in ')]}':
                    depth -= 1
                    if depth == 0:
                        args.append((start, j))
                        break
                elif c == ',' and depth == 1:
                    args.append((start, j))
                    start = j + 1
                j += 1
            if len(args) >= nth:
                a, z = args[nth - 1]
                while a < z and text[a] == ' ':
                    a += 1
                edits.append((a, z, '(%s)(' % pt, ')'))
        for g1, g2, g3, pt, at in found:
            class _M:
                pass
            m = _M()
            m.group = (lambda i, v=(None, g1, g2, g3, pt, at): v[i])
            if not (is_obj_ptr(pt) and (is_obj_ptr(at) or re.search(r'\(\*\)\[', at))):
                continue
            rng = m.group(3)
            if not rng:
                continue
            r = re.findall(r'\{(\d+):(\d+)-(\d+):(\d+)\}', rng)
            if not r:
                continue
            l1, c1, l2, c2 = map(int, r[-1])
            a = starts[l1 - 1] + c1 - 1
            z = starts[l2 - 1] + c2 - 1
            edits.append((a, z, '(%s)(' % pt, ')'))
        for a, z, pre, post in sorted(set(edits), reverse=True):
            text = text[:a] + pre + text[a:z] + post + text[z:]
        if edits:
            open(f, 'w', encoding='latin-1').write(text)
            total += len(set(edits))
            print('%s: %d casts' % (f, len(set(edits))))
    print('cppcast: %d casts' % total)


if __name__ == '__main__':
    main()
