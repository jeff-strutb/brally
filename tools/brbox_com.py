"""brbox_com.py -- the COM objects the boxed game talks to.

A DirectX 6 machine as the game sees it: DirectDraw (only to be probed for its
version by BrDxDetect 0x1001D8A0), and DirectInput with a keyboard and a
mouse, both fed from the scripted input timeline.  No joystick is attached:
EnumDevices never calls back.

Every method is a trap with the interface's exact stdcall argument count
(`this` included) -- a wrong count would leave the caller's esp off by the
difference, so methods are declared per interface, never guessed.  A method
declared with fn=None is UNMODELLED and stops the run naming itself.

Object state (reference counts, device formats) lives in HostState.com so the
live oracle's capture/restore covers it.
"""
import struct

from brbox import GuestFault

IUNK = [('QueryInterface', 3), ('AddRef', 1), ('Release', 1)]

IFACES = {
    'IDirectDraw': IUNK + [
        ('Compact', 1), ('CreateClipper', 4), ('CreatePalette', 5), ('CreateSurface', 4),
        ('DuplicateSurface', 3), ('EnumDisplayModes', 5), ('EnumSurfaces', 5),
        ('FlipToGDISurface', 1), ('GetCaps', 3), ('GetDisplayMode', 2), ('GetFourCCCodes', 3),
        ('GetGDISurface', 2), ('GetMonitorFrequency', 2), ('GetScanLine', 2),
        ('GetVerticalBlankStatus', 2), ('Initialize', 2), ('RestoreDisplayMode', 1),
        ('SetCooperativeLevel', 3), ('SetDisplayMode', 4), ('WaitForVerticalBlank', 3)],
    'IDirectDraw2': None,          # IDirectDraw's methods with a 6-arg SetDisplayMode + GetAvailableVidMem
    'IDirectDrawSurface': IUNK + [
        ('AddAttachedSurface', 2), ('AddOverlayDirtyRect', 2), ('Blt', 6), ('BltBatch', 4),
        ('BltFast', 6), ('DeleteAttachedSurface', 3), ('EnumAttachedSurfaces', 3),
        ('EnumOverlayZOrders', 4), ('Flip', 3), ('GetAttachedSurface', 3), ('GetBltStatus', 2),
        ('GetCaps', 2), ('GetClipper', 2), ('GetColorKey', 3), ('GetDC', 2), ('GetFlipStatus', 2),
        ('GetOverlayPosition', 3), ('GetPalette', 2), ('GetPixelFormat', 2),
        ('GetSurfaceDesc', 2), ('Initialize', 3), ('IsLost', 1), ('Lock', 5), ('ReleaseDC', 2),
        ('Restore', 1), ('SetClipper', 2), ('SetColorKey', 3), ('SetOverlayPosition', 3),
        ('SetPalette', 2), ('Unlock', 2), ('UpdateOverlay', 6), ('UpdateOverlayDisplay', 2),
        ('UpdateOverlayZOrder', 3)],
    'IDirectInputA': IUNK + [
        ('CreateDevice', 4), ('EnumDevices', 5), ('GetDeviceStatus', 2),
        ('RunControlPanel', 3), ('Initialize', 3)],
    'IDirectInputDeviceA': IUNK + [
        ('GetCapabilities', 2), ('EnumObjects', 4), ('GetProperty', 3), ('SetProperty', 3),
        ('Acquire', 1), ('Unacquire', 1), ('GetDeviceState', 3), ('GetDeviceData', 5),
        ('SetDataFormat', 2), ('SetEventNotification', 2), ('SetCooperativeLevel', 3),
        ('GetObjectInfo', 4), ('GetDeviceInfo', 2), ('RunControlPanel', 3), ('Initialize', 4),
        # IDirectInputDevice2A
        ('CreateEffect', 5), ('EnumEffects', 4), ('GetEffectInfo', 3),
        ('GetForceFeedbackState', 2), ('SendForceFeedbackCommand', 2),
        ('EnumCreatedEffectObjects', 4), ('Escape', 2), ('Poll', 1), ('SendDeviceData', 5)],
    'IDirectPlayLobby3A': IUNK + [
        ('Connect', 4), ('CreateAddress', 7), ('EnumAddress', 5), ('EnumAddressTypes', 5),
        ('EnumLocalApplications', 4), ('GetConnectionSettings', 4), ('ReceiveLobbyMessage', 6),
        ('RunApplication', 5), ('SendLobbyMessage', 5), ('SetConnectionSettings', 4),
        ('SetLobbyMessageEvent', 4), ('CreateCompoundAddress', 5), ('ConnectEx', 5),
        ('RegisterApplication', 3), ('UnregisterApplication', 3),
        ('WaitForConnectionSettings', 2)],
    'IDirectPlay4A': IUNK + [
        ('AddPlayerToGroup', 3), ('Close', 1), ('CreateGroup', 6), ('CreatePlayer', 7),
        ('DeletePlayerFromGroup', 3), ('DestroyGroup', 2), ('DestroyPlayer', 2),
        ('EnumGroupPlayers', 6), ('EnumGroups', 5), ('EnumPlayers', 5), ('EnumSessions', 6),
        ('GetCaps', 3), ('GetGroupData', 5), ('GetGroupName', 4), ('GetMessageCount', 3),
        ('GetPlayerAddress', 4), ('GetPlayerCaps', 4), ('GetPlayerData', 5),
        ('GetPlayerName', 4), ('GetSessionDesc', 3), ('Initialize', 2), ('Open', 3),
        ('Receive', 6), ('Send', 6), ('SetGroupData', 5), ('SetGroupName', 4),
        ('SetPlayerData', 5), ('SetPlayerName', 4), ('SetSessionDesc', 3),
        # IDirectPlay3
        ('AddGroupToGroup', 3), ('CreateGroupInGroup', 7), ('DeleteGroupFromGroup', 3),
        ('EnumConnections', 5), ('EnumGroupsInGroup', 6), ('GetGroupConnectionSettings', 5),
        ('InitializeConnection', 3), ('SecureOpen', 5), ('SendChatMessage', 5),
        ('SetGroupConnectionSettings', 4), ('StartSession', 3), ('GetGroupFlags', 3),
        ('GetGroupParent', 3), ('GetPlayerAccount', 5), ('GetPlayerFlags', 3),
        # IDirectPlay4
        ('GetGroupOwner', 3), ('SetGroupOwner', 3), ('SendEx', 10), ('GetMessageQueue', 6),
        ('CancelMessage', 3), ('CancelPriority', 4)],
    'IDirectSound': IUNK + [
        ('CreateSoundBuffer', 4), ('GetCaps', 2), ('DuplicateSoundBuffer', 3),
        ('SetCooperativeLevel', 3), ('Compact', 1), ('GetSpeakerConfig', 2),
        ('SetSpeakerConfig', 2), ('Initialize', 2)],
    'IDirectSoundBuffer': IUNK + [
        ('GetCaps', 2), ('GetCurrentPosition', 3), ('GetFormat', 4), ('GetVolume', 2),
        ('GetPan', 2), ('GetFrequency', 2), ('GetStatus', 2), ('Initialize', 3), ('Lock', 8),
        ('Play', 4), ('SetCurrentPosition', 2), ('SetFormat', 2), ('SetVolume', 2),
        ('SetPan', 2), ('SetFrequency', 2), ('Stop', 1), ('Unlock', 5), ('Restore', 1)],
}
IFACES['IDirectDraw2'] = IFACES['IDirectDraw'][:21] + [('SetDisplayMode', 6),
                                                       ('WaitForVerticalBlank', 3),
                                                       ('GetAvailableVidMem', 4)]
IFACES['IDirectDrawSurface3'] = IFACES['IDirectDrawSurface'] + [
    ('GetDDInterface', 2), ('PageLock', 2), ('PageUnlock', 2), ('SetSurfaceDesc', 3)]
IFACES['IDirectDrawSurface4'] = IFACES['IDirectDrawSurface3'] + [
    ('SetPrivateData', 5), ('GetPrivateData', 4), ('FreePrivateData', 2),
    ('GetUniquenessValue', 2), ('ChangeUniquenessValue', 1)]


def _guid(b):
    d1, d2, d3 = struct.unpack_from('<IHH', b, 0)
    return '%08X-%04X-%04X-%s' % (d1, d2, d3, bytes(b[8:16]).hex().upper())


IID = {
    '00000000-0000-0000-C000000000000046': 'IUnknown',
    '6C14DB80-A733-11CE-A5210020AF0BE560': 'IDirectDraw',
    'B3A6F3E0-2B43-11CF-A2DE00AA00B93356': 'IDirectDraw2',
    '6C14DB81-A733-11CE-A5210020AF0BE560': 'IDirectDrawSurface',
    'DA044E00-69B2-11D0-A1D500AA00B8DFBB': 'IDirectDrawSurface3',
    '0B2B8630-AD35-11D0-8EA600609797EA5B': 'IDirectDrawSurface4',
    '5944E680-C92E-11CF-BFC7444553540000': 'IDirectInputDeviceA',
    '5944E682-C92E-11CF-BFC7444553540000': 'IDirectInputDevice2A',
}

GUID_SYSKEYBOARD = '6F1D2B61-D5A0-11CF-BFC7444553540000'
GUID_SYSMOUSE = '6F1D2B60-D5A0-11CF-BFC7444553540000'

S_OK = 0
E_NOINTERFACE = 0x80004002
DIERR_NOTFOUND = 0x80070002
DIERR_DEVICENOTREG = 0x80040154


def new_object(box, iface, **state):
    """Allocate a COM object: [vtbl][pad], its vtable of traps, its state."""
    meths = IFACES[iface]
    vt = box.vtables.get(iface)
    if vt is None:
        vt = box.host_alloc(4 * len(meths), 4)
        for i, (name, nargs) in enumerate(meths):
            fn = METHODS.get((iface, name)) or METHODS.get(('*', name)) or _unmodelled(iface, name)
            box.wr32(vt + 4 * i, box.trap('%s::%s' % (iface, name), 4 * nargs, fn, nargs))
        box.vtables[iface] = vt
    obj = box.host_alloc(16, 4)
    box.wr32(obj, vt)
    st = {'iface': iface, 'ref': 1}
    st.update(state)
    box.hs.com[obj] = st
    return obj


def _unmodelled(iface, name):
    def fn(box, a):
        raise GuestFault('unmodelled COM method %s::%s' % (iface, name))
    return fn


METHODS = {}


def method(iface, name):
    def deco(fn):
        METHODS[(iface, name)] = fn
        return fn
    return deco


def _st(box, this):
    st = box.hs.com.get(this)
    if st is None:
        raise GuestFault('COM call on unknown object %08X' % this)
    return st


@method('*', 'AddRef')
def _addref(box, a):
    st = _st(box, a[0])
    st['ref'] += 1
    return st['ref']


@method('*', 'Release')
def _release(box, a):
    st = _st(box, a[0])
    st['ref'] -= 1
    return max(st['ref'], 0)


QI_MAP = {
    'IDirectDraw': ('IDirectDraw2',),
    'IDirectDraw2': ('IDirectDraw',),
    'IDirectDrawSurface': ('IDirectDrawSurface3', 'IDirectDrawSurface4'),
    'IDirectInputDeviceA': ('IDirectInputDevice2A',),
}


@method('*', 'QueryInterface')
def _qi(box, a):
    st = _st(box, a[0])
    want = IID.get(_guid(box.rd(a[1], 16)), _guid(box.rd(a[1], 16)))
    if want == st['iface'] or want == 'IUnknown':
        st['ref'] += 1
        box.wr32(a[2], a[0])
        return S_OK
    if want in QI_MAP.get(st['iface'], ()):
        real = 'IDirectInputDeviceA' if want == 'IDirectInputDevice2A' else want
        o = new_object(box, real, parent=a[0], **{k: v for k, v in st.items()
                                                    if k not in ('iface', 'ref')})
        box.wr32(a[2], o)
        return S_OK
    box.wr32(a[2], 0)
    return E_NOINTERFACE


# ------------------------------------------------------------- DirectDraw --
for _dd in ('IDirectDraw', 'IDirectDraw2'):
    METHODS[(_dd, 'SetCooperativeLevel')] = lambda box, a: S_OK

    def _cs(box, a):
        o = new_object(box, 'IDirectDrawSurface', caps=box.rd32(a[1] + 0x68))
        box.wr32(a[2], o)
        return S_OK
    METHODS[(_dd, 'CreateSurface')] = _cs


def directdraw_create(box, a):
    # DirectDrawCreate(lpGUID, lplpDD, pUnkOuter)
    box.wr32(a[1], new_object(box, 'IDirectDraw'))
    return S_OK


# ----------------------------------------------------------- DirectInput --
def directinput_create(box, a):
    # DirectInputCreateA(hinst, dwVersion, lplpDirectInput, punkOuter)
    box.wr32(a[2], new_object(box, 'IDirectInputA', version=a[1]))
    return S_OK


@method('IDirectInputA', 'CreateDevice')
def _createdev(box, a):
    g = _guid(box.rd(a[1], 16))
    kind = {GUID_SYSKEYBOARD: 'keyboard', GUID_SYSMOUSE: 'mouse'}.get(g)
    if kind is None:
        box.wr32(a[2], 0)
        return DIERR_DEVICENOTREG
    box.wr32(a[2], new_object(box, 'IDirectInputDeviceA', kind=kind, acquired=False))
    return S_OK


@method('IDirectInputA', 'EnumDevices')
def _enumdev(box, a):
    # Keyboard and mouse are system devices the game creates by GUID; the
    # enumeration it performs is for joysticks (DIDEVTYPE_JOYSTICK 4), and
    # this machine has none -- the callback is never called.
    if a[1] not in (4, 0):
        box.log('EnumDevices(type=%d) -> none' % a[1])
    return S_OK


@method('IDirectInputDeviceA', 'SetDataFormat')
def _setfmt(box, a):
    _st(box, a[0])['format'] = a[1]
    return S_OK


@method('IDirectInputDeviceA', 'SetCooperativeLevel')
def _devcoop(box, a):
    return S_OK


@method('IDirectInputDeviceA', 'SetProperty')
def _setprop(box, a):
    return S_OK


@method('IDirectInputDeviceA', 'Acquire')
def _acquire(box, a):
    st = _st(box, a[0])
    was = st['acquired']
    st['acquired'] = True
    return 1 if was else S_OK       # S_FALSE when already acquired


@method('IDirectInputDeviceA', 'Unacquire')
def _unacquire(box, a):
    st = _st(box, a[0])
    was = st['acquired']
    st['acquired'] = False
    return S_OK if was else 1


@method('IDirectInputDeviceA', 'Poll')
def _poll(box, a):
    return 1       # DI_NOEFFECT: keyboard/mouse need no polling


@method('IDirectInputDeviceA', 'GetDeviceState')
def _getstate(box, a):
    st = _st(box, a[0])
    if not st['acquired']:
        return 0x8007000C              # DIERR_NOTACQUIRED
    n = a[1]
    if st['kind'] == 'keyboard':
        buf = bytearray(n)
        for dik in box.hs.dikeys:
            if dik < n:
                buf[dik] = 0x80
        box.wr(a[2], bytes(buf))
        return S_OK
    # mouse: DIMOUSESTATE -- relative motion since the last read, buttons
    hs = box.hs
    st_ = struct.pack('<iii', hs.mouse[0], hs.mouse[1], 0) + bytes(
        0x80 if b in hs.mouse_btn else 0 for b in range(4))
    hs.mouse[0] = hs.mouse[1] = 0
    box.wr(a[2], (st_ + b'\0' * n)[:n])
    return S_OK


@method('IDirectInputDeviceA', 'GetDeviceData')
def _getdata(box, a):
    # buffered input: nothing buffered (the game polls state)
    if a[3]:
        box.wr32(a[3], 0)
    return S_OK


# ------------------------------------------------------------ DirectPlay --
# A DirectX 6 machine with DirectPlay's four stock service providers, never
# launched from a lobby.
CLSID = {
    '2FE8F810-B2A5-11D0-A7870000F803ABFC': 'IDirectPlayLobby3A',   # CLSID_DirectPlayLobby
    'D1EB6D20-8923-11D0-9D9700A0C90A43CB': 'IDirectPlay4A',        # CLSID_DirectPlay
    '47D4D946-62E8-11CF-93BC444553540000': 'IDirectSound',          # CLSID_DirectSound
}
DPERR_NOTLOBBIED = 0x8877042E
DPAID_SERVICEPROVIDER = bytes.fromhex('C016D907AFE0CF119C4E00A0C905425E')
SERVICE_PROVIDERS = [
    ('685BC400-9D2C-11CF-A9CD00AA006886E3', 'IPX Connection For DirectPlay'),
    ('36E95EE0-8577-11CF-960C0080C7534E82', 'Internet TCP/IP Connection For DirectPlay'),
    ('44EAA760-CB68-11CF-9C4E00A0C905425E', 'Modem Connection For DirectPlay'),
    ('0F1D6860-88D9-11CF-9C4E00A0C905425E', 'Serial Connection For DirectPlay'),
]


def _guid_bytes(g):
    d1, d2, d3, tail = g.split('-')
    return struct.pack('<IHH', int(d1, 16), int(d2, 16), int(d3, 16)) + bytes.fromhex(tail)


def cocreate(box, clsid_va, iid_va, ppv):
    iface = CLSID.get(_guid(box.rd(clsid_va, 16)))
    if iface is None:
        return None
    box.wr32(ppv, new_object(box, iface))
    return S_OK


@method('IDirectPlayLobby3A', 'GetConnectionSettings')
def _getconn(box, a):
    return DPERR_NOTLOBBIED


DPERR_INVALIDPLAYER = 0x88770096


@method('IDirectPlay4A', 'GetPlayerName')
def _getplayername(box, a):
    # no session is open, so no player id is valid
    return DPERR_INVALIDPLAYER


@method('IDirectPlay4A', 'EnumConnections')
def _enumconn(box, a):
    # (this, lpguidApplication, lpEnumCallback, lpContext, dwFlags)
    cb, ctx = a[2], a[3]
    for guid, name in SERVICE_PROVIDERS:
        g = box.host_alloc(16, 4)
        box.wr(g, _guid_bytes(guid))
        # the connection is a DirectPlay address: one DPADDRESS chunk
        # (DPAID_ServiceProvider, 16 bytes) carrying the provider's GUID
        conn_full = DPAID_SERVICEPROVIDER + struct.pack('<I', 16) + _guid_bytes(guid)
        conn = box.host_alloc(len(conn_full), 4)
        box.wr(conn, conn_full)
        nm = box.host_str(name)
        dpname = box.host_alloc(16, 4)
        box.wr(dpname, struct.pack('<IIII', 16, 0, nm, nm))
        r = yield ('call', cb, [g, conn, len(conn_full), dpname, 1, ctx])   # DPCONNECTION_DIRECTPLAY
        if r == 0:
            break
    return S_OK


# ----------------------------------------------------------- DirectSound --
# Buffers are real guest memory the game writes PCM into; the play cursor
# advances with the virtual clock at the buffer's byte rate, and a
# non-looping buffer stops at its end -- the game's streaming and "has this
# sample finished" logic see exactly the timing the clock implies.
DSBCAPS_PRIMARYBUFFER = 0x1
DSBSTATUS_PLAYING, DSBSTATUS_LOOPING = 0x1, 0x4
DSERR_INVALIDPARAM = 0x80070057


@method('IDirectSound', 'Initialize')
def _ds_init(box, a):
    return S_OK


@method('IDirectSound', 'SetCooperativeLevel')
def _ds_coop(box, a):
    return S_OK


@method('IDirectSound', 'GetCaps')
def _ds_caps(box, a):
    # DSCAPS: dwSize then flags; a plain 16-bit stereo card, 32 hw buffers
    size = box.rd32(a[1])
    caps = bytearray(max(size, 96))
    struct.pack_into('<II', caps, 0, size, 0x0F0F)
    box.wr(a[1], bytes(caps[:size]))
    return S_OK


@method('IDirectSound', 'GetSpeakerConfig')
def _ds_spk(box, a):
    box.wr32(a[1], 4)                     # DSSPEAKER_STEREO
    return S_OK


@method('IDirectSound', 'CreateSoundBuffer')
def _ds_create(box, a):
    import brbox_imports as I
    desc = a[1]
    flags, nbytes, wfx = box.rd32(desc + 4), box.rd32(desc + 8), box.rd32(desc + 16)
    primary = bool(flags & DSBCAPS_PRIMARYBUFFER)
    if primary:
        fmt = struct.pack('<HHIIHH', 1, 2, 22050, 88200, 4, 16)
        nbytes = 0x4000
    else:
        if not wfx or nbytes == 0:
            box.wr32(a[2], 0)
            return DSERR_INVALIDPARAM
        fmt = box.rd(wfx, 16)
    mem = I.heap_alloc(box, nbytes, zero=True)
    ch, rate, avg, align, bits = struct.unpack_from('<HIIHH', fmt, 2)
    box.wr32(a[2], new_object(box, 'IDirectSoundBuffer', flags=flags, size=nbytes, mem=mem,
                              fmt=bytes(fmt), rate=rate, align=max(align, 1), primary=primary,
                              playing=False, looping=False, pos=0, t0=0.0, vol=0, pan=0))
    return S_OK


def _ds_pos(box, st):
    if not st['playing']:
        return st['pos']
    bytes_per_ms = st['rate'] * st['align'] / 1000.0
    p = st['pos'] + int((box.hs.ms - st['t0']) * bytes_per_ms)
    p -= p % st['align']
    if p >= st['size']:
        if st['looping']:
            p %= st['size']
        else:
            st['playing'] = False
            st['pos'] = 0
            return 0
    return p


@method('IDirectSoundBuffer', 'GetCurrentPosition')
def _dsb_pos(box, a):
    st = _st(box, a[0])
    p = _ds_pos(box, st)
    if a[1]:
        box.wr32(a[1], p)
    if a[2]:
        box.wr32(a[2], p)
    return S_OK


@method('IDirectSoundBuffer', 'GetStatus')
def _dsb_status(box, a):
    st = _st(box, a[0])
    _ds_pos(box, st)
    box.wr32(a[1], (DSBSTATUS_PLAYING if st['playing'] else 0) |
             (DSBSTATUS_LOOPING if st['playing'] and st['looping'] else 0))
    return S_OK


@method('IDirectSoundBuffer', 'GetCaps')
def _dsb_caps(box, a):
    st = _st(box, a[0])
    box.wr(a[1] + 4, struct.pack('<III', st['flags'], st['size'], 0))
    return S_OK


@method('IDirectSoundBuffer', 'GetFormat')
def _dsb_getfmt(box, a):
    st = _st(box, a[0])
    f = st['fmt'] + b'\0\0'
    if a[1]:
        box.wr(a[1], f[:min(a[2], len(f))] if a[2] else f)
    if a[3]:
        box.wr32(a[3], len(f))
    return S_OK


@method('IDirectSoundBuffer', 'SetFormat')
def _dsb_setfmt(box, a):
    st = _st(box, a[0])
    st['fmt'] = box.rd(a[1], 16)
    _c, st['rate'], _avg, align, _b = struct.unpack_from('<HIIHH', st['fmt'], 2)
    st['align'] = max(align, 1)
    return S_OK


@method('IDirectSoundBuffer', 'Lock')
def _dsb_lock(box, a):
    # (this, dwOffset, dwBytes, ppv1, pdw1, ppv2, pdw2, dwFlags)
    st = _st(box, a[0])
    off, n, flags = a[1], a[2], a[7]
    if flags & 1:                              # DSBLOCK_FROMWRITECURSOR
        off = _ds_pos(box, st)
    if flags & 2:                              # DSBLOCK_ENTIREBUFFER
        off, n = 0, st['size']
    off %= max(st['size'], 1)
    n = min(n, st['size'])
    n1 = min(n, st['size'] - off)
    box.wr32(a[3], st['mem'] + off)
    box.wr32(a[4], n1)
    if a[5]:
        box.wr32(a[5], st['mem'] if n > n1 else 0)
    if a[6]:
        box.wr32(a[6], n - n1)
    return S_OK


@method('IDirectSoundBuffer', 'Unlock')
def _dsb_unlock(box, a):
    return S_OK


@method('IDirectSoundBuffer', 'Play')
def _dsb_play(box, a):
    st = _st(box, a[0])
    if not st['playing']:
        st['t0'] = box.hs.ms
    else:
        st['pos'] = _ds_pos(box, st)
        st['t0'] = box.hs.ms
    st['playing'] = True
    st['looping'] = bool(a[3] & 1)             # DSBPLAY_LOOPING
    return S_OK


@method('IDirectSoundBuffer', 'Stop')
def _dsb_stop(box, a):
    st = _st(box, a[0])
    st['pos'] = _ds_pos(box, st)
    st['playing'] = False
    return S_OK


@method('IDirectSoundBuffer', 'SetCurrentPosition')
def _dsb_setpos(box, a):
    st = _st(box, a[0])
    st['pos'] = a[1] % max(st['size'], 1)
    st['t0'] = box.hs.ms
    return S_OK


for _k, _f in (('Volume', 'vol'), ('Pan', 'pan'), ('Frequency', 'freq')):
    def _set(box, a, _f=_f):
        st = _st(box, a[0])
        st[_f] = a[1]
        if _f == 'freq' and a[1]:
            st['rate'] = a[1]
        return S_OK

    def _get(box, a, _f=_f):
        st = _st(box, a[0])
        box.wr32(a[1], st.get(_f, st['rate'] if _f == 'freq' else 0))
        return S_OK
    METHODS[('IDirectSoundBuffer', 'Set' + _k)] = _set
    METHODS[('IDirectSoundBuffer', 'Get' + _k)] = _get


@method('IDirectSoundBuffer', 'Restore')
def _dsb_restore(box, a):
    return S_OK


def install(box):
    # Every vtable is built up front: built lazily, one first reached inside
    # a live-oracle sub-run would be allocated from host state that the
    # capture then rolls back, and the next allocation would overwrite it.
    box.vtables = {}
    for iface in IFACES:
        new_object(box, iface)
    box.hs.com.clear()
    import brbox_imports as I
    I.DYNAMIC['ddraw.dll']['DirectDrawCreate'] = (12, directdraw_create)
    I.DYNAMIC['dinput.dll']['DirectInputCreateA'] = (16, directinput_create)
    I.MODELS['DINPUT.dll!DirectInputCreateA'] = (16, directinput_create, 4)
