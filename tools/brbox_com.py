"""brbox_com.py -- the COM objects the boxed game talks to.

A DirectX 6 machine as the game sees it: DirectDraw (only to be probed for its
version by BrDxDetect 0x1001D8A0), and DirectInput with a keyboard and a
mouse, both fed from the scripted input timeline.  By default no joystick is
attached and EnumDevices never calls back; a script's `joystick ffb` directive
attaches a force-feedback wheel (steered from the same scripted arrow keys).

Every method is a trap with the interface's exact stdcall argument count
(`this` included) -- a wrong count would leave the caller's esp off by the
difference, so methods are declared per interface, never guessed.  A method
declared with fn=None is UNMODELLED and stops the run naming itself.

Object state (reference counts, device formats) lives in HostState.com so the
live oracle's capture/restore covers it.
"""
import os
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
    'IDirectInputEffect': IUNK + [
        ('Initialize', 4), ('GetEffectGuid', 2), ('GetParameters', 3), ('SetParameters', 3),
        ('Start', 3), ('Stop', 1), ('GetEffectStatus', 2), ('Download', 1), ('Unload', 1),
        ('Escape', 2)],
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
    '26C66A70-B367-11CF-A02400AA006157AC': 'IDirectPlayLobbyA',
    '1BB4AF80-A303-11D0-9C4F00A0C905425E': 'IDirectPlayLobby2A',
    '2DB72491-652C-11D1-A7A80000F803ABFC': 'IDirectPlayLobby3A',
}

GUID_SYSKEYBOARD = '6F1D2B61-D5A0-11CF-BFC7444553540000'
GUID_SYSMOUSE = '6F1D2B60-D5A0-11CF-BFC7444553540000'
# the attached wheel's instance and product GUIDs (any fixed values will do)
GUID_WHEEL = 'B0B1B2B3-0001-11D2-8000444553540000'
GUID_WHEEL_PRODUCT = 'B0B1B2B3-0000-0000-0000504944564944'
DIDEVTYPE_JOYSTICK = 4
DIDEVTYPEJOYSTICK_WHEEL = 3
DIEDFL_FORCEFEEDBACK = 0x100

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
            if _TRACE and iface.startswith(_TRACE):
                fn = _traced(iface, name, fn, nargs)
            box.wr32(vt + 4 * i, box.trap('%s::%s' % (iface, name), 4 * nargs, fn, nargs))
        box.vtables[iface] = vt
    obj = box.host_alloc(16, 4)
    box.wr32(obj, vt)
    st = {'iface': iface, 'ref': 1}
    st.update(state)
    box.hs.com[obj] = st
    return obj


_TRACE = os.environ.get('BRBOX_COMTRACE')


def _traced(iface, name, fn, nargs):
    import inspect

    def wrap(box, a):
        args = [a[k] for k in range(nargs)]
        r = fn(box, a)
        if inspect.isgenerator(r):
            r = yield from r
        print('com%d f%d %s::%s(%s) -> %08X' % (getattr(box, 'net_index', 0), box.hs.frame, iface, name, ' '.join('%08X' % x for x in args),
                                          (r or 0) & 0xFFFFFFFF))
        return r
    return wrap


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
    # every lobby interface is a prefix of IDirectPlayLobby3A's vtable
    'IDirectPlayLobby3A': ('IDirectPlayLobbyA', 'IDirectPlayLobby2A'),
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
        real = {'IDirectInputDevice2A': 'IDirectInputDeviceA',
                'IDirectPlayLobbyA': 'IDirectPlayLobby3A',
                'IDirectPlayLobby2A': 'IDirectPlayLobby3A'}.get(want, want)
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
    if g == GUID_WHEEL and getattr(box.hs, 'joystick', None):
        kind = 'joystick'
    if kind is None:
        box.wr32(a[2], 0)
        return DIERR_DEVICENOTREG
    box.wr32(a[2], new_object(box, 'IDirectInputDeviceA', kind=kind, acquired=False))
    return S_OK


@method('IDirectInputA', 'EnumDevices')
def _enumdev(box, a):
    # (this, dwDevType, lpCallback, pvRef, dwFlags).  Keyboard and mouse are
    # system devices the game creates by GUID; the enumeration it performs is
    # for joysticks (DIDEVTYPE_JOYSTICK 4).  Only a `joystick` machine has one.
    joy = getattr(box.hs, 'joystick', None)
    if a[1] not in (0, DIDEVTYPE_JOYSTICK) or not joy:
        return S_OK
    if a[4] & DIEDFL_FORCEFEEDBACK and joy != 'ffb':
        return S_OK
    # DIDEVICEINSTANCEA: dwSize, guidInstance, guidProduct, dwDevType,
    # tszInstanceName[260], tszProductName[260], guidFFDriver, wUsagePage, wUsage
    name = b'Force Feedback Wheel'
    inst = struct.pack('<I', 0x244) + _guid_bytes(GUID_WHEEL) + _guid_bytes(GUID_WHEEL_PRODUCT) + \
        struct.pack('<I', DIDEVTYPE_JOYSTICK | (DIDEVTYPEJOYSTICK_WHEEL << 8) | 0x10000) + \
        name.ljust(260, b'\0') + name.ljust(260, b'\0') + \
        (_guid_bytes(GUID_WHEEL_PRODUCT) if joy == 'ffb' else bytes(16)) + struct.pack('<HH', 1, 4)
    p = box.host_alloc(len(inst), 4)
    box.wr(p, inst)
    yield ('call', a[2], [p, a[3]])
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
    return 1       # DI_NOEFFECT: nothing here needs polling


@method('IDirectInputDeviceA', 'GetCapabilities')
def _getcaps(box, a):
    # DIDEVCAPS: dwSize, dwFlags, dwDevType, dwAxes, dwButtons, dwPOVs,
    # dwFFSamplePeriod, dwFFMinTimeResolution, dwFirmwareRevision,
    # dwHardwareRevision, dwFFDriverVersion
    st = _st(box, a[0])
    size = box.rd32(a[1])
    ff = st['kind'] == 'joystick' and getattr(box.hs, 'joystick', None) == 'ffb'
    caps = struct.pack('<11I', size, 0x1 | (0x100 if ff else 0),
                       DIDEVTYPE_JOYSTICK | (DIDEVTYPEJOYSTICK_WHEEL << 8), 2, 8, 0,
                       1000 if ff else 0, 1000 if ff else 0, 1, 1, 1 if ff else 0)
    box.wr(a[1], caps[:max(size, 8)])
    return S_OK


@method('IDirectInputDeviceA', 'CreateEffect')
def _createeffect(box, a):
    # (this, rguid, lpeff, ppdeff, punkOuter)
    if getattr(box.hs, 'joystick', None) != 'ffb':
        return 0x80040154                  # DIERR_DEVICENOTREG
    box.wr32(a[3], new_object(box, 'IDirectInputEffect', guid=box.rd(a[1], 16)))
    return S_OK


@method('IDirectInputDeviceA', 'SendForceFeedbackCommand')
def _sendffcmd(box, a):
    return S_OK


@method('IDirectInputDeviceA', 'GetForceFeedbackState')
def _getffstate(box, a):
    box.wr32(a[1], 0x40 | 0x200)           # DIGFFS_POWERON | DIGFFS_ACTUATORSON
    return S_OK


for _m in ('Initialize', 'SetParameters', 'Start', 'Stop', 'Download', 'Unload', 'Escape'):
    METHODS[('IDirectInputEffect', _m)] = lambda box, a: S_OK


@method('IDirectInputEffect', 'GetEffectStatus')
def _effstatus(box, a):
    box.wr32(a[1], 0)
    return S_OK


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
    if st['kind'] == 'joystick':
        # DIJOYSTATE(2): lX lY lZ lRx lRy lRz rglSlider[2] rgdwPOV[4]
        # rgbButtons[32] (+ the DIJOYSTATE2 extension, zero).  The wheel turns
        # with the scripted arrow keys; UP/DOWN are the pedals on lY.
        dk = box.hs.dikeys
        x = (-128 if 0xCB in dk else 0) + (128 if 0xCD in dk else 0)
        y = (-128 if 0xC8 in dk else 0) + (128 if 0xD0 in dk else 0)
        js = struct.pack('<8i', x, y, 0, 0, 0, 0, 0, 0) + struct.pack('<4I', *([0xFFFFFFFF] * 4)) + \
            bytes(0x80 if k in dk else 0 for k in (0x39, 0x1C, 0x1D, 0x2A, 0, 0, 0, 0)) + bytes(24)
        box.wr(a[2], (js + bytes(n))[:n])
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


DPERR_BUFFERTOOSMALL = 0x8877001E


@method('IDirectPlayLobby3A', 'CreateCompoundAddress')
def _createcompound(box, a):
    # (this, lpElements, dwElementCount, lpAddress, lpdwAddressSize).  A
    # DPCOMPOUNDADDRESSELEMENT is {GUID guidDataType; DWORD dwDataSize;
    # LPVOID lpData}; the address is each as a DPADDRESS chunk
    # {GUID; DWORD size} followed by its data, in order.
    out = b''
    for k in range(a[2]):
        e = a[1] + 24 * k
        guid, size, data = box.rd(e, 16), box.rd32(e + 16), box.rd32(e + 20)
        out += guid + struct.pack('<I', size) + (box.rd(data, size) if size else b'')
    have = box.rd32(a[4])
    box.wr32(a[4], len(out))
    if not a[3] or have < len(out):
        return DPERR_BUFFERTOOSMALL
    box.wr(a[3], out)
    return S_OK


@method('IDirectPlayLobby3A', 'GetConnectionSettings')
def _getconn(box, a):
    return DPERR_NOTLOBBIED


DPERR_INVALIDPLAYER = 0x88770096
DPERR_NOMESSAGES = 0x887700BE
DPERR_NOSESSIONS = 0x887700D2
DPERR_NOCONNECTION = 0x887700AA
DPERR_BUFFERTOOSMALL_DP = 0x8877001E
DPOPEN_JOIN = 0x1
DPOPEN_CREATE = 0x2
DPPLAYER_SERVERPLAYER = 0x100    # also DPENUMPLAYERS_SERVERPLAYER
DPENUMPLAYERS_LOCAL, DPENUMPLAYERS_REMOTE, DPENUMPLAYERS_SESSION = 0x8, 0x10, 0x80
DPID_SERVERPLAYER = 1
DPESC_TIMEDOUT = 0x1
DPRECEIVE_TOPLAYER, DPRECEIVE_FROMPLAYER, DPRECEIVE_PEEK = 0x2, 0x4, 0x8
DPPLAYERTYPE_PLAYER = 1

# DirectPlay over brbox_net: each box's view of the session lives in its
# HostState (brbox_net.dp_state).  Without a peer box it is a DirectPlay that
# works but finds nobody -- no sessions to join, the only players are the ones
# the game creates, and no message ever arrives.  With a peer, what one box
# sends in frame N is in the other's queue at frame N+1, and each side learns
# of the other's players through DPSYS_CREATEPLAYERORGROUP, as on a real LAN.


def _dp(box, a=None):
    import brbox_net
    d = brbox_net.dp_state(box)
    if box.net is not None:
        # A DirectPlay call takes time like a clock query does: a loop that
        # polls the session waiting for the peer (the race start handshake)
        # must let virtual time -- and with it the peer -- move on.
        box.tick()                   # publishes, takes in what is due
    return d


def _dp_send(box, kind, payload):
    import brbox_net
    brbox_net.send(box, kind, payload)


def _dp_players(d):
    import brbox_net
    ps = dict(brbox_net.remote_players(d))
    ps.update(d['local'])
    return ps


def _dp_cstr(s):
    return (s or b'') + b'\0'


def _dp_name_blob(p, base):
    """DPNAME {dwSize, dwFlags, lpszShortNameA, lpszLongNameA} + strings,
    laid out for a guest buffer at `base`."""
    sn, ln = _dp_cstr(p.get('short')), _dp_cstr(p.get('long'))
    return struct.pack('<IIII', 16, 0, base + 16, base + 16 + len(sn)) + sn + ln


@method('IDirectPlay4A', 'GetPlayerName')
def _dpgetplayername(box, a):
    # (this, idPlayer, lpData, lpdwDataSize)
    p = _dp_players(_dp(box)).get(a[1])
    if p is None:
        return DPERR_INVALIDPLAYER
    blob = _dp_name_blob(p, a[2])
    have = box.rd32(a[3])
    box.wr32(a[3], len(blob))
    if not a[2] or have < len(blob):
        return DPERR_BUFFERTOOSMALL_DP
    box.wr(a[2], blob)
    return S_OK


@method('IDirectPlay4A', 'GetPlayerData')
def _dpgetplayerdata(box, a):
    # (this, idPlayer, lpData, lpdwDataSize, dwFlags): nobody sets player data
    if a[1] not in _dp_players(_dp(box)):
        return DPERR_INVALIDPLAYER
    box.wr32(a[3], 0)
    return S_OK


def _dp_player_query(box, a):
    return DPERR_INVALIDPLAYER if a[1] not in _dp_players(_dp(box)) else S_OK


for _m in ('GetPlayerAddress', 'GetPlayerCaps', 'GetPlayerAccount', 'GetPlayerFlags'):
    METHODS[('IDirectPlay4A', _m)] = _dp_player_query


@method('IDirectPlay4A', 'InitializeConnection')
def _dpinitconn(box, a):
    _dp(box)['connection'] = True
    return S_OK


def _dp_desc_in_guest(box, desc, name):
    sd = box.host_alloc(len(desc), 4)
    d = bytearray(desc)
    nm = box.host_str(name.decode('latin1')) if name else 0
    d[0x30:0x34] = struct.pack('<I', nm)
    d[0x34:0x38] = b'\0\0\0\0'
    box.wr(sd, bytes(d))
    return sd


@method('IDirectPlay4A', 'EnumSessions')
def _dpenumsessions(box, a):
    # (this, lpsd, dwTimeout, lpEnumSessionsCallback2, lpContext, dwFlags):
    # every session another box hosts, then the time-out
    d = _dp(box)
    to = box.host_alloc(4, 4)
    for _g, (desc, name) in sorted(d['sessions'].items()):
        box.wr32(to, a[2])
        r = yield ('call', a[3], [_dp_desc_in_guest(box, desc, name), to, 0, a[4]])
        if not r:
            return S_OK
    box.wr32(to, a[2])
    yield ('call', a[3], [0, to, DPESC_TIMEDOUT, a[4]])
    return S_OK


def _dp_read_name(box, va):
    if not va:
        return b''
    out = bytearray()
    while len(out) < 256:
        c = box.rd(va + len(out), 1)
        if c == b'\0':
            break
        out += c
    return bytes(out)


@method('IDirectPlay4A', 'Open')
def _dpopen(box, a):
    # (this, lpsd, dwFlags)
    d = _dp(box)
    if a[2] & DPOPEN_CREATE:
        desc = bytearray(box.rd(a[1], 0x50))
        # guidInstance: DirectPlay makes one up; per box, so peers differ
        desc[8:24] = struct.pack('<IIII', 0x5E55100 + getattr(box, 'net_index', 0), 0, 0, 1)
        d['session'] = bytes(desc)
        d['name'] = _dp_read_name(box, struct.unpack_from('<I', desc, 0x30)[0])
        d['host'] = True
        _dp_send(box, 'session', (d['session'][8:24], d['session'], d['name']))
        return S_OK
    want = box.rd(a[1] + 8, 16)
    if want in d['sessions']:
        d['session'], d['name'] = d['sessions'][want]
        d['host'] = False
        return S_OK
    return DPERR_NOSESSIONS


@method('IDirectPlay4A', 'SecureOpen')
def _dpsecureopen(box, a):
    return _dpopen(box, a)


@method('IDirectPlay4A', 'CreatePlayer')
def _dpcreateplayer(box, a):
    # (this, lpidPlayer, lpPlayerName, hEvent, lpData, dwDataSize, dwFlags)
    d = _dp(box)
    if a[6] & DPPLAYER_SERVERPLAYER:
        pid = DPID_SERVERPLAYER
    else:
        # unique across boxes without shared state: the box index is the
        # high part, a per-box counter the low
        pid = 0x100 * (getattr(box, 'net_index', 0) + 1) + 0x10 + d['next_pid']
        d['next_pid'] += 1
    sn = ln = b''
    if a[2]:
        sn = _dp_read_name(box, box.rd32(a[2] + 8))
        ln = _dp_read_name(box, box.rd32(a[2] + 12))
    d['local'][pid] = {'short': sn, 'long': ln, 'flags': a[6], 'event': a[3]}
    if d['session'] is not None:
        _dp_send(box, 'player+', (pid, {'short': sn, 'long': ln, 'flags': a[6],
                                        'guid': d['session'][8:24]}))
    box.wr32(a[1], pid)
    return S_OK


@method('IDirectPlay4A', 'EnumPlayers')
def _dpenumplayers(box, a):
    # (this, lpguidInstance, lpEnumPlayersCallback2, lpContext, dwFlags)
    d = _dp(box)
    if d['session'] is None:
        return DPERR_NOSESSIONS
    for pid, p in sorted(_dp_players(d).items()):
        if pid == DPID_SERVERPLAYER and not a[4] & DPPLAYER_SERVERPLAYER:
            continue
        blob = _dp_name_blob(p, 0)
        name = box.host_alloc(len(blob), 4)
        box.wr(name, _dp_name_blob(p, name))
        fl = (DPENUMPLAYERS_LOCAL if pid in d['local'] else DPENUMPLAYERS_REMOTE) | \
            (DPPLAYER_SERVERPLAYER if pid == DPID_SERVERPLAYER else 0)
        r = yield ('call', a[2], [pid, DPPLAYERTYPE_PLAYER, name, fl, a[3]])
        if not r:
            break
    return S_OK


@method('IDirectPlay4A', 'GetSessionDesc')
def _dpgetsessiondesc(box, a):
    # (this, lpData, lpdwDataSize)
    d = _dp(box)
    if d['session'] is None:
        return DPERR_NOCONNECTION
    desc = bytearray(d['session'])
    desc[0x2C:0x30] = struct.pack('<I', len(_dp_players(d)))    # dwCurrentPlayers
    nm = _dp_cstr(d['name'])
    size = len(desc) + len(nm)
    have = box.rd32(a[2])
    box.wr32(a[2], size)
    if a[1] == 0 or have < size:
        return DPERR_BUFFERTOOSMALL_DP
    desc[0x30:0x34] = struct.pack('<I', a[1] + len(desc))
    desc[0x34:0x38] = b'\0\0\0\0'
    box.wr(a[1], bytes(desc) + nm)
    return S_OK


@method('IDirectPlay4A', 'SetSessionDesc')
def _dpsetsessiondesc(box, a):
    # (this, lpSessDesc, dwFlags) -- only the host of an open session may
    d = _dp(box)
    if d['session'] is None:
        return DPERR_NOCONNECTION
    desc = bytearray(box.rd(a[1], 0x50))
    desc[8:24] = d['session'][8:24]
    d['session'] = bytes(desc)
    d['name'] = _dp_read_name(box, struct.unpack_from('<I', desc, 0x30)[0]) or d['name']
    if d['host']:
        _dp_send(box, 'session', (d['session'][8:24], d['session'], d['name']))
    return S_OK


@method('IDirectPlay4A', 'DestroyPlayer')
def _dpdestroyplayer(box, a):
    d = _dp(box)
    if d['local'].pop(a[1], None) is not None:
        _dp_send(box, 'player-', a[1])
    return S_OK


def _dp_sysmsg(kind, pid, p, ncur, base):
    """The DPMSG_CREATE/DESTROYPLAYERORGROUP bytes for a guest buffer at base."""
    if kind == 0x0003:
        head = 4 * 6 + 16 + 8
        nm = _dp_name_blob(p, base + head)
        dpname, strs = nm[:16], nm[16:]
        return struct.pack('<IIIIII', kind, DPPLAYERTYPE_PLAYER, pid, ncur, 0, 0) + dpname + \
            struct.pack('<II', 0, 0) + strs
    head = 4 * 7 + 16 + 8
    nm = _dp_name_blob(p, base + head)
    dpname, strs = nm[:16], nm[16:]
    return struct.pack('<IIIIIII', kind, DPPLAYERTYPE_PLAYER, pid, 0, 0, 0, 0) + dpname + \
        struct.pack('<II', 0, 0) + strs


@method('IDirectPlay4A', 'Receive')
def _dpreceive(box, a):
    # (this, lpidFrom, lpidTo, dwFlags, lpData, lpdwDataSize)
    d = _dp(box)
    flags = a[3]
    want_to = box.rd32(a[2]) if flags & DPRECEIVE_TOPLAYER else None
    want_from = box.rd32(a[1]) if flags & DPRECEIVE_FROMPLAYER else None
    for k, (frm, to, data) in enumerate(d['inbox']):
        if want_to is not None and to != want_to and frm != 0:
            continue
        if want_from is not None and frm != want_from:
            continue
        if isinstance(data, tuple):
            _t, kind, pid, p = data
            body = _dp_sysmsg(kind, pid, p, len(_dp_players(d)), a[4])
        else:
            body = data
        have = box.rd32(a[5])
        box.wr32(a[5], len(body))
        if not a[4] or have < len(body):
            return DPERR_BUFFERTOOSMALL_DP
        box.wr(a[4], body)
        box.wr32(a[1], frm)
        box.wr32(a[2], to)
        if not flags & DPRECEIVE_PEEK:
            del d['inbox'][k]
        return S_OK
    return DPERR_NOMESSAGES


@method('IDirectPlay4A', 'GetMessageCount')
def _dpmsgcount(box, a):
    # (this, idPlayer, lpdwCount)
    d = _dp(box)
    box.wr32(a[2], sum(1 for frm, to, _ in d['inbox'] if to == a[1] or frm == 0))
    return S_OK


@method('IDirectPlay4A', 'Send')
def _dpsend(box, a):
    # (this, idFrom, idTo, dwFlags, lpData, dwDataSize)
    d = _dp(box)
    if d['session'] is not None:
        _dp_send(box, 'msg', (d['session'][8:24], a[1], a[2], box.rd(a[4], a[5]) if a[5] else b''))
        if _TRACE:
            print('dp send box%d frame %d ret %08X %X->%X %s' % (getattr(box, 'net_index', 0), box.hs.frame, box.recent[-1][1],
                                                    a[1], a[2], d['outbox'][-1][2][3].hex()))
    return S_OK


@method('IDirectPlay4A', 'SendEx')
def _dpsendex(box, a):
    # (this, idFrom, idTo, dwFlags, lpData, dwDataSize, dwPriority, dwTimeout,
    #  lpContext, lpdwMsgID)
    d = _dp(box)
    if d['session'] is not None:
        _dp_send(box, 'msg', (d['session'][8:24], a[1], a[2], box.rd(a[4], a[5]) if a[5] else b''))
    if a[9]:
        box.wr32(a[9], 0)
    return S_OK


@method('IDirectPlay4A', 'Close')
def _dpclose(box, a):
    d = _dp(box)
    for pid in sorted(d['local']):
        if d['session'] is not None:
            _dp_send(box, 'player-', pid)
    if d['host'] and d['session'] is not None:
        _dp_send(box, 'session', (d['session'][8:24], None, None))
    d['session'] = None
    d['host'] = False
    d['local'] = {}
    d['inbox'] = []
    return S_OK


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


def dplobby_create(box, a):
    # DirectPlayLobbyCreateA(lpGUIDDSP, lplpDPL, lpUnk, lpData, dwDataSize)
    box.wr32(a[1], new_object(box, 'IDirectPlayLobby3A'))
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
    I.MODELS['DPLAYX.dll!#4'] = (20, dplobby_create, 5)
