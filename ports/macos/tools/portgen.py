#!/usr/bin/env python3
"""portgen.py -- build the port's view of the decomp from src/ + port specs.

WHAT IT DOES: src/ and include/ hold exactly what MSVC 5.0 compiles for the
byte-matched build -- no BR_MATCHING_BUILD conditionals, no port arms.  Where
the Mac port needs something different from a module or header, that lives
here, in ports/, as a SPEC:

    ports/macos/patch/src/core/<dir>/<file>.c.port
    ports/macos/patch/include/<file>.h.port

A spec names top-level items of the decomp file and says what the port does
with them.  build.sh runs this script first; it writes the port's copy of
every spec'd module to build/port/src/core/..., and a mirror of include/
(spec'd headers rewritten, the rest copied) to build/port/include/, which is
the only header directory the port compiles against.

SPEC FORMAT (one op per line; text blocks end at a line holding only @end)

    # comment
    @drop KEY            the port does not compile this item
    @after KEY           insert the block below right after item KEY
    ...C text...         (KEY `^` means the top of the file)
    @end

A KEY is the item's kind and name, e.g. fn:BrFontLoad, proto:BrFontLoad,
var:g_brFont, type:BrFont, define:BR_FONT_PAGES, include:<stdio.h>,
if:#ifndef BR_FUNCPTR_DEFINED -- with #2, #3... when a key repeats.  A
@replace is spelled `@drop fn:X` plus `@after fn:X` (a drop keeps the
item's place as an anchor).  Keys are names, not text, so the decomp can
keep changing a body the port has dropped or replaced without the spec
going stale; a KEY that no longer exists is an error, never skipped.

    portgen.py            generate everything (build.sh calls this)
    portgen.py --list     print the core TUs build.sh compiles
"""
import os
import re
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))
PATCH = os.path.join(ROOT, 'ports', 'macos', 'patch')
OUT = os.path.join(ROOT, 'build', 'port')


# --------------------------------------------------------------- parsing ---

_ID = re.compile(r'[A-Za-z_]\w*')


def _skip_ws_comments(t, i):
    n = len(t)
    while i < n:
        if t[i].isspace():
            i += 1
        elif t.startswith('/*', i):
            j = t.find('*/', i + 2)
            i = n if j < 0 else j + 2
        elif t.startswith('//', i):
            j = t.find('\n', i)
            i = n if j < 0 else j
        else:
            break
    return i


def _line_end(t, i):
    """End of the (backslash-continued) line starting at or containing i."""
    n = len(t)
    while True:
        j = t.find('\n', i)
        if j < 0:
            return n
        k = j - 1
        if k >= 0 and t[k] == '\r':
            k -= 1
        if k >= 0 and t[k] == '\\':
            i = j + 1
            continue
        return j + 1


def _directive_at(t, i):
    """If a preprocessor directive starts on the line at i, return its word."""
    j = i
    while j < len(t) and t[j] in ' \t':
        j += 1
    if j < len(t) and t[j] == '#':
        m = re.match(r'#\s*(\w+)', t[j:])
        return m.group(1) if m else ''
    return None


def _cond_block_end(t, i):
    """i starts an #if/#ifdef/#ifndef line; return the offset after its #endif."""
    depth = 0
    n = len(t)
    while i < n:
        w = _directive_at(t, i)
        e = _line_end(t, i)
        if w in ('if', 'ifdef', 'ifndef'):
            depth += 1
        elif w == 'endif':
            depth -= 1
            if depth == 0:
                return e
        i = e
    return n


def _trailing(t, i):
    """Extend an item end over trailing blanks and a same-line comment."""
    n = len(t)
    j = i
    while j < n and t[j] in ' \t':
        j += 1
    if t.startswith('/*', j):
        k = t.find('*/', j + 2)
        if k >= 0 and '\n' not in t[j:k]:
            j = k + 2
            while j < n and t[j] in ' \t':
                j += 1
    elif t.startswith('//', j):
        j = t.find('\n', j)
        j = n if j < 0 else j
    if j < n and t[j] == '\n':
        return j + 1
    if j < n and t[j] == '\r' and t.startswith('\r\n', j):
        return j + 2
    return i


def split_items(t):
    """Split C source into top-level items: [(start, end, body_start)].

    An item runs from the end of the previous one (so leading blank lines
    and comments belong to it) to the end of its declaration, definition or
    directive, plus a comment on the same line.  body_start is where the
    item's own code begins, after its leading comments.  A top-level
    #if...#endif block is one item.  Trailing comments at end of file form a
    last item with body_start == end.
    """
    items = []
    n = len(t)
    start = 0
    i = 0
    while True:
        i = _skip_ws_comments(t, i)
        if i >= n:
            if start < n:
                items.append((start, n, n))
            return items
        body = i
        # a directive line: its own item (a conditional block, whole)
        ls = t.rfind('\n', 0, i) + 1
        if t[ls:i].strip() == '' and t[i] == '#':
            w = _directive_at(t, i)
            end = _cond_block_end(t, i) if w in ('if', 'ifdef', 'ifndef') \
                else _line_end(t, i)
            end = _trailing(t, end) if end > 0 and t[end - 1] != '\n' else end
            items.append((start, end, body))
            start = i = end
            continue
        # a declaration or definition
        depth = paren = 0
        fnbody = False
        prev = ''
        while i < n:
            c = t[i]
            if t.startswith('/*', i) or t.startswith('//', i):
                i = _skip_ws_comments(t, i)
                continue
            if c in '"\'':
                j = i + 1
                while j < n and t[j] != c:
                    j += 2 if t[j] == '\\' else 1
                i = j + 1
                prev = c
                continue
            if c == '#' and depth > 0:
                # a directive inside a body: skip the whole line
                i = _line_end(t, i)
                continue
            if c == '(':
                paren += 1
            elif c == ')':
                paren -= 1
            elif c == '{':
                if depth == 0 and paren == 0:
                    fnbody = (prev == ')' or bool(_ID.fullmatch(prev) and
                                                  prev not in ('struct', 'union', 'enum')
                                                  and _fn_header(t[body:i])))
                depth += 1
            elif c == '}':
                depth -= 1
                if depth == 0 and paren == 0 and fnbody:
                    i = _trailing(t, i + 1)
                    break
            elif c == ';' and depth == 0 and paren == 0:
                i = _trailing(t, i + 1)
                break
            if not c.isspace():
                if _ID.match(c):
                    m = _ID.match(t, i)
                    prev = m.group(0)
                    i = m.end()
                    continue
                prev = c
            i += 1
        items.append((start, i, body))
        start = i


def _fn_header(h):
    """A function definition header ends in `)` plus attribute-like words."""
    h = re.sub(r'/\*.*?\*/', ' ', h, flags=re.S)
    h = re.sub(r'\s+', ' ', h).strip()
    return bool(re.search(r'\)[\s\w]*$', h)) and '=' not in h


def _strip(code):
    code = re.sub(r'/\*.*?\*/', ' ', code, flags=re.S)
    code = re.sub(r'//[^\n]*', ' ', code)
    code = re.sub(r'"(\\.|[^"\\])*"', '""', code)
    return code


_KW = {'static', 'extern', 'const', 'volatile', 'inline', '__inline',
       'struct', 'union', 'enum', 'unsigned', 'signed', 'register', 'auto',
       'void', 'char', 'short', 'int', 'long', 'float', 'double',
       '__cdecl', '__stdcall', '__fastcall', '__declspec', 'typedef',
       'dllimport', 'dllexport', 'naked'}


def item_key(code):
    """kind:name for an item's own code (leading comments removed)."""
    s = code.strip()
    if s.startswith('#'):
        m = re.match(r'#\s*(\w+)\s*(.*)', s)
        w, rest = m.group(1), m.group(2)
        if w == 'define' or w == 'undef':
            return '%s:%s' % (w, _ID.match(rest).group(0))
        if w == 'include':
            return 'include:' + rest.split('/*')[0].strip()
        first = s.split('\n', 1)[0].strip()
        if w in ('if', 'ifdef', 'ifndef'):
            return 'if:' + ' '.join(first.split())
        return w + ':' + ' '.join(first.split())
    c = _strip(s)
    if not c.strip():
        return 'comment:'
    head = c.split('{', 1)[0] if '{' in c else c
    # function definition or prototype: NAME ( ... ) with the name outside
    # any parenthesis -- `void (*pfn)(void)` is a variable, not a prototype.
    flat = re.sub(r'\s+', ' ', head)
    m = re.search(r'([A-Za-z_]\w*)\s*\(', flat)
    if m and m.group(1) not in _KW and flat[:m.start()].count('(') == 0 \
            and '=' not in flat[:m.start()]:
        kind = 'fn' if ('{' in c and _fn_header(c.split('{', 1)[0])) else 'proto'
        if flat.lstrip().startswith('typedef'):
            kind = 'type'
        return '%s:%s' % (kind, m.group(1))
    if flat.lstrip().startswith('typedef'):
        ids = [x for x in _ID.findall(re.sub(r'\{.*\}', ' ', c, flags=re.S))
               if x not in _KW]
        return 'type:' + (ids[-1] if ids else '?')
    t = re.sub(r'\{.*\}', '{}', c, flags=re.S)
    t = t.split('=', 1)[0]
    m = re.match(r'\s*(struct|union|enum)\s+(\w+)\s*\{\}\s*;', t)
    if m:
        return 'type:%s %s' % (m.group(1), m.group(2))
    t = re.sub(r'\[[^\]]*\]', '', t)
    ids = [x for x in _ID.findall(t) if x not in _KW]
    return 'var:' + (ids[-1] if ids else '?')


def keyed_items(t):
    """[(key, start, end)] with #n suffixes on repeated keys."""
    seen = {}
    out = []
    for s, e, b in split_items(t):
        k = item_key(t[b:e]) if b < e else 'comment:'
        seen[k] = seen.get(k, 0) + 1
        if seen[k] > 1:
            k = '%s#%d' % (k, seen[k])
        out.append((k, s, e))
    return out


# ------------------------------------------------------------------ specs ---

def parse_spec(text, where='spec'):
    """-> (drops:set, afters:{key: [text,...]})"""
    drops, afters = set(), {}
    lines = text.splitlines(keepends=True)
    i = 0
    while i < len(lines):
        ln = lines[i].rstrip('\r\n')
        i += 1
        if not ln.strip() or ln.startswith('#'):
            continue
        if ln.startswith('@drop '):
            drops.add(ln[6:].strip())
        elif ln.startswith('@after '):
            key = ln[7:].strip()
            buf = []
            while i < len(lines) and lines[i].rstrip('\r\n') != '@end':
                buf.append(lines[i])
                i += 1
            if i >= len(lines):
                raise SystemExit('%s: @after %s has no @end' % (where, key))
            i += 1
            afters.setdefault(key, []).append(''.join(buf))
        else:
            raise SystemExit('%s: bad line: %s' % (where, ln))
    return drops, afters


def apply_spec(src, spec, where='spec'):
    drops, afters = parse_spec(spec, where)
    items = keyed_items(src)
    keys = {k for k, _, _ in items}
    missing = sorted((drops | set(afters)) - keys - {'^'})
    if missing:
        raise SystemExit('%s: no such item in the decomp file: %s -- the '
                         'decomp changed under this spec; update it'
                         % (where, ', '.join(missing)))
    out = ''.join(afters.get('^', []))
    for k, s, e in items:
        if k not in drops:
            out += src[s:e]
        for blk in afters.get(k, []):
            out += blk
    return out


# ------------------------------------------------------------- the build ---

def spec_for(rel):
    p = os.path.join(PATCH, rel + '.port')
    return p if os.path.exists(p) else None


def _gen(rel):
    src = open(os.path.join(ROOT, rel), newline='').read()
    sp = spec_for(rel)
    text = apply_spec(src, open(sp, newline='').read(),
                      os.path.relpath(sp, ROOT)) if sp else src
    dst = os.path.join(OUT, rel)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    old = open(dst, newline='').read() if os.path.exists(dst) else None
    if old != text:                       # keep mtimes of unchanged output
        with open(dst, 'w', newline='') as f:
            f.write(text)


def core_units():
    """(key, path, extra_flags) for every core TU build.sh compiles."""
    out = []
    core = os.path.join(ROOT, 'src', 'core')
    for dp, _, fs in os.walk(core):
        for f in fs:
            if not f.endswith('.c') or f.startswith('_'):
                continue
            rel = os.path.relpath(os.path.join(dp, f), ROOT)
            sub = os.path.relpath(os.path.join(dp, f), core)
            if os.path.exists(os.path.join(ROOT, 'ports', 'macos', 'core', sub)):
                continue
            extra = []
            if rel == 'src/core/geometry/br_mat3.c':
                extra = ['-include', 'ports/macos/include/br_mat3_port.h']
            if spec_for(rel):
                out.append((rel, os.path.relpath(os.path.join(OUT, rel), ROOT),
                            extra + ['-iquote', os.path.dirname(rel)]))
            else:
                out.append((rel, rel, extra))
    for sub in ('core', 'legacy'):
        for dp, _, fs in os.walk(os.path.join(ROOT, 'ports', 'macos', sub)):
            for f in fs:
                if f.endswith('.c'):
                    rel = os.path.relpath(os.path.join(dp, f), ROOT)
                    out.append((rel, rel, []))
    return sorted(out)


def generate():
    """Write build/port/: spec'd modules, and the whole include/ mirror."""
    for dp, _, fs in os.walk(PATCH):
        for f in fs:
            if f.endswith('.port'):
                rel = os.path.relpath(os.path.join(dp, f), PATCH)[:-5]
                if rel.startswith('src/'):
                    _gen(rel)
    inc = os.path.join(ROOT, 'include')
    want = set()
    for f in os.listdir(inc):
        if f.endswith('.h'):
            want.add(f)
            _gen('include/' + f)
    have = os.path.join(OUT, 'include')
    for f in os.listdir(have):                 # a header deleted from include/
        if f not in want:
            os.unlink(os.path.join(have, f))


if __name__ == '__main__':
    if sys.argv[1:] == ['--list']:
        # "OBJNAME PATH FLAGS..." -- OBJNAME is the key's path under its
        # root (src/core, ports/macos/core, ports/macos/legacy), '/' -> '_'.
        for k, p, x in core_units():
            obj = re.sub(r'^(src/core|ports/macos/core|ports/macos/legacy)/', '', k)
            print(' '.join([obj[:-2].replace('/', '_'), p] + x))
    else:
        generate()
