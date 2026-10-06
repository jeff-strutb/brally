"""Rebuild the remaster renderer's source files by replaying, in order, every edit recorded in the session
transcript: Write/Edit tool calls, cat > / cat >> heredocs, python edit scripts that write files, sed -i
one-liners and cp between source files. Runs inside the original scratch path so absolute paths resolve."""
import json, re, os, subprocess, sys
T = sys.argv[1]; S = sys.argv[2]
SRC = re.compile(r'\.(py|c|sh)$')
def inside(p): return p.startswith(S)
log = open(os.path.join(S, '_replay.log'), 'w'); n = collections = 0
steps = []
for line in open(T):
    try: d = json.loads(line)
    except Exception: continue
    msg = d.get('message', {}); c = msg.get('content') if isinstance(msg, dict) else None
    if not isinstance(c, list): continue
    for b in c:
        if b.get('type') != 'tool_use': continue
        name = b.get('name'); inp = b.get('input', {}) or {}
        if name == 'Write' and inside(inp.get('file_path', '')) and SRC.search(inp['file_path']):
            steps.append(('write', inp['file_path'], inp.get('content', '')))
        elif name == 'Edit' and inside(inp.get('file_path', '')) and SRC.search(inp['file_path']):
            steps.append(('edit', inp['file_path'], inp.get('old_string', ''), inp.get('new_string', ''), inp.get('replace_all', False)))
        elif name == 'Bash':
            cmd = inp.get('command', '') or ''
            if S not in cmd and 'cd ${0:a:h}' not in cmd: continue
            # heredoc file writes
            for m in re.finditer(r"cat (>>?) (\S+) <<'(\w+)'\n(.*?)\n\3(?:\n|$)", cmd, re.S):
                op, f, _, body = m.groups()
                if SRC.search(f): steps.append(('cat', op, f, body + '\n'))
            # python edit scripts that write a source file
            for m in re.finditer(r"python3? - <<'(\w+)'\n(.*?)\n\1(?:\n|$)", cmd, re.S):
                body = m.group(2)
                if re.search(r"open\(['\"][^'\"]+\.(py|c|sh)['\"]\s*,\s*['\"]w", body): steps.append(('py', body))
            for m in re.finditer(r"(sed -i '' (?:'[^']*'|\"[^\"]*\"|\S+)(?: (?:-e )?(?:'[^']*'|\"[^\"]*\"))*\s+[\w./-]+\.(?:py|c|sh))", cmd):
                steps.append(('sh', m.group(1)))
            for m in re.finditer(r"(?:^|[;&\n] *)(cp (?:-f )?[\w./-]+\.(?:py|c) [\w./-]+\.(?:py|c))", cmd):
                steps.append(('sh', m.group(1)))
os.chdir(S); ok = bad = 0
for st in steps:
    try:
        if st[0] == 'write':
            os.makedirs(os.path.dirname(st[1]), exist_ok=True); open(st[1], 'w').write(st[2])
        elif st[0] == 'edit':
            s = open(st[1]).read()
            if st[2] not in s: raise ValueError('edit target missing in ' + st[1])
            s = s.replace(st[2], st[3]) if st[4] else s.replace(st[2], st[3], 1); open(st[1], 'w').write(s)
        elif st[0] == 'cat':
            open(st[2], 'a' if st[1] == '>>' else 'w').write(st[3])
        elif st[0] == 'py':
            r = subprocess.run([sys.executable, '-'], input=st[1], text=True, capture_output=True, cwd=S, timeout=60)
            if r.returncode: raise RuntimeError(r.stderr.strip().splitlines()[-1] if r.stderr.strip() else 'rc %d' % r.returncode)
        elif st[0] == 'sh':
            r = subprocess.run(st[1], shell=True, cwd=S, capture_output=True, text=True)
            if r.returncode: raise RuntimeError(r.stderr.strip())
        ok += 1
    except Exception as e:
        bad += 1; log.write('%s: %s\n' % (st[0], str(e)[:200]))
print('steps', len(steps), 'applied', ok, 'failed', bad)
