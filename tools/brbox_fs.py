"""brbox_fs.py -- the virtual disk the boxed game sees.

Two drives, both backed by the retail disc tree extracted into
build/brbox/cd (tools/brbox.py `CD_ROOT`):

    C:  the install drive (DRIVE_FIXED).  The registry's install path points
        at C:\\BOSSRALLY\\, and every file the game looks for there resolves to
        the disc tree -- the installer only copies disc files anyway.
    D:  the CD-ROM (DRIVE_CDROM), volume label "Boss Rally", the same tree.

Lookups are case-insensitive, like FAT.  Writes never touch the host disk:
they land in HostState.written, keyed by canonical path, so a save written
during a run can be read back later in the same run, and the live oracle's
capture/restore covers them like any other host state.
"""
import os

_INDEX = {}        # root -> {lowercase relpath ('' for root) -> real path}


def _index(root):
    ix = _INDEX.get(root)
    if ix is None:
        ix = {'': root}
        for dp, dns, fns in os.walk(root):
            rel = os.path.relpath(dp, root)
            rel = '' if rel == '.' else rel.replace(os.sep, '\\').lower()
            for n in dns + fns:
                k = (rel + '\\' + n.lower()) if rel else n.lower()
                ix[k] = os.path.join(dp, n)
        _INDEX[root] = ix
    return ix


DRIVES = {'C': 3, 'D': 5}          # GetDriveType: 3 = DRIVE_FIXED, 5 = DRIVE_CDROM
VOLUME = {'D': 'Boss Rally', 'C': 'SYSTEM'}
INSTALL_DIR = 'C:\\BOSSRALLY\\'


class VFS(object):
    def __init__(self, root, hs):
        self.root = root
        self.hs = hs
        self.ix = _index(root)

    # ------------------------------------------------------------- paths --
    def canon(self, path):
        """Absolute, backslashed, upper-drive, '..'-resolved form of `path`
        relative to the current directory.  Returns (drive, relpath_lower,
        display) or None for a drive that does not exist."""
        p = path.replace('/', '\\')
        cwd = self.hs.cwd
        if len(p) >= 2 and p[1] == ':':
            drive = p[0].upper()
            rest = p[2:]
            if not rest.startswith('\\'):
                # drive-relative: only the current drive has a cwd
                base = cwd[2:] if cwd[0].upper() == drive else '\\'
                rest = base.rstrip('\\') + '\\' + rest
        elif p.startswith('\\'):
            drive, rest = cwd[0].upper(), p
        else:
            drive, rest = cwd[0].upper(), cwd[2:].rstrip('\\') + '\\' + p
        if drive not in DRIVES:
            return None
        parts = []
        for seg in rest.split('\\'):
            if seg in ('', '.'):
                continue
            if seg == '..':
                if parts:
                    parts.pop()
                continue
            parts.append(seg)
        rel = '\\'.join(parts)
        # C:\BOSSRALLY\ is the install directory; strip it to reach the tree
        low = rel.lower()
        if drive == 'C':
            if low == 'bossrally':
                low = ''
            elif low.startswith('bossrally\\'):
                low = low[len('bossrally\\'):]
            else:
                return (drive, None, drive + ':\\' + rel)
        return (drive, low, drive + ':\\' + rel)

    def key(self, path):
        c = self.canon(path)
        return None if c is None else c[2].lower()

    def exists_dir(self, path):
        c = self.canon(path)
        if c is None:
            return False
        drive, low, _ = c
        if low is None:
            return c[2].rstrip('\\').upper() == 'C:'      # the bare C:\ root
        real = self.ix.get(low)
        return real is not None and os.path.isdir(real)

    def read(self, path):
        """File bytes, or None.  An in-run write shadows the disc."""
        k = self.key(path)
        if k is None:
            return None
        if k in self.hs.written:
            return bytes(self.hs.written[k])
        c = self.canon(path)
        if c[1] is None:
            return None
        real = self.ix.get(c[1])
        if real is None or os.path.isdir(real):
            return None
        with open(real, 'rb') as f:
            return f.read()

    def write(self, path, data, append=False):
        k = self.key(path)
        if k is None:
            return False
        if append and k in self.hs.written:
            self.hs.written[k] += data
        elif append:
            old = self.read(path) or b''
            self.hs.written[k] = bytearray(old + data)
        else:
            self.hs.written[k] = bytearray(data)
        return True

    def listdir(self, pattern):
        """Names matching a _findfirst pattern (dir\\*.ext), directory first
        entries '.'/'..' omitted, in the disc's (sorted) order."""
        import fnmatch
        p = pattern.replace('/', '\\')
        d, _, pat = p.rpartition('\\')
        c = self.canon(d + '\\x' if d else 'x')
        if c is None or c[1] is None:
            return []
        dlow = c[1][:-1].rstrip('\\') if c[1].endswith('x') else c[1]
        out = []
        for k, real in self.ix.items():
            parent, _, name = k.rpartition('\\')
            if parent != dlow or not k:
                continue
            nm = os.path.basename(real)
            if fnmatch.fnmatch(nm.lower(), pat.lower()):
                out.append((nm, real))
        out.sort(key=lambda t: t[0].lower())
        return out
