import os, sys, runpy
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tools'))
import brbox_drive
brbox_drive.KEYS.update({'RSHIFT': (0xA1, 0x36), 'DELETE': (0x2E, 0xD3), 'RCTRL': (0xA3, 0x9D),
    'END': (0x23, 0xCF), 'PGDN': (0x22, 0xD1), 'INSERT': (0x2D, 0xD2), 'HOME': (0x24, 0xC7),
    'PGUP': (0x21, 0xC9), 'KP5': (0x65, 0x4C), 'KP7': (0x67, 0x47), 'KP9': (0x69, 0x49),
    'KP1': (0x61, 0x4F), 'KP3': (0x63, 0x51), 'KP0': (0x60, 0x52)})
if os.environ.get('PROBE_SAVES'):
    brbox_drive.SAVES = os.environ['PROBE_SAVES']
sys.argv = ['brbox_diff.py'] + sys.argv[1:]
runpy.run_path(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tools', 'brbox_diff.py'), run_name='__main__')
