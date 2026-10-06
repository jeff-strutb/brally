#!/usr/bin/env python3
"""Rewrite C++ placeholder callees (EnterFn, PrepFn, ...) to the real callee,
declared under its true symbol so the call/address relocation resolves by name.
Plan: build/brally/win32/brbox/gap/cpp_plan.json {file: {placeholder_mangled: target}}."""
import json, re, sys

plan = json.load(open('build/brally/win32/brbox/gap/cpp_plan.json'))
MANG = re.compile(r'^\?(\w+)@(?:(\w+)@)?@YAH(?:PAV(\w+)@@)?@Z$')
changed = []
for f, d in plan.items():
    t = open(f).read()
    for ph, tgt in d.items():
        p = ph[1:].split('@')[0]                           # EnterFn
        m = re.search(r'^([^\n;{}#]*?)\b' + p + r'\s*\(([^)]*)\)\s*;[^\n]*\n', t, re.M)
        if not m:
            print('NO PROTOTYPE', f, p); continue
        ret, params = m.group(1).strip(), m.group(2).strip()
        if tgt.startswith('C:'):
            name = tgt[2:]
            decl = 'extern "C" void %s(void);' % name
            ref = name
            what = 'C function %s' % name
        else:
            mm = MANG.match(tgt)
            if not mm:
                print('UNPARSED', f, tgt); continue
            name, ns, cls = mm.groups()
            args = '%s *' % cls if cls else 'void'
            decl = ('class %s;\n' % cls if cls else '')
            if ns:
                decl += 'namespace %s { int %s(%s); }' % (ns, name, args)
                ref = '%s::%s' % (ns, name)
            else:
                decl += 'int %s(%s);' % (name, args)
                ref = name
            what = '%s (%s)' % (ref, tgt)
        cast = '((%s (*)(%s))%s)' % (ret, params or 'void', ref)
        repl = ('/* %s was a stand-in; the original calls %s.  Declared under\n'
                ' * its real symbol so the relocation resolves by name. */\n'
                '%s\n#define %s %s\n') % (p, what, decl, p, cast)
        t = t[:m.start()] + repl + t[m.end():]
        changed.append((f, p))
    open(f, 'w').write(t)
print(len(changed), 'placeholders rewritten in', len(plan), 'files')
