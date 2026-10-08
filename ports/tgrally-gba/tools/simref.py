"""simref.py SCRIPT OUTDIR [CALLS] -- the race's simulation as the cartridge runs it, for
checking gba/sim against: the original ROM in the N64 box (tools/tgrally/n64box.py),
driven by a pad script, stopped at every entry to and return from a car's physics
frame (racing/cartick.c BrCarPhysTick).

  OUTDIR/ram.bin    all of RDRAM (8 MB, big-endian as the console has it) at the first
                    entry: the track, the cars, the tables the simulation reads
  OUTDIR/ticks.bin  per stop: 'I' (entry) or 'O' (return), the car's slot (byte), the
                    retrace (u32), then each car record (BrCar, 0x2090 bytes, slots
                    0..CARS-1), each car's pad record (0x15C bytes) and the globals in
                    GLOBALS (u32 each), each car's race entity flags (u32), all
                    big-endian as in RDRAM

CALLS stops after that many physics frames (default 2400)."""
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../../tools/tgrally'))
import n64box  # noqa: E402
from unicorn import UC_HOOK_CODE  # noqa: E402
import unicorn.mips_const as M  # noqa: E402

PHYS_TICK, PHYS_RET = 0x8021F998, 0x8021FDFC
CARS, CAR, CAR_SIZE = 2, 0x8031B760, 0x2090
PAD_SIZE = 0x15C
GLOBALS = (0x802A4A30,          # the collision cell's walk direction (flips each frame)
           0x8028AAD8,          # seconds this frame
           0x8028C800,          # the weather row (difficulty)
           0x8026FF18,          # the game mode
           0x8026FF08,          # players
           0x8028B7F0,          # race entities
           0x8028A8AC,          # the picture is mirrored
           0x8028B940,          # the track
           0x80315E78)          # the race state


def main():
    script, out = sys.argv[1], sys.argv[2]
    limit = int(sys.argv[3]) if len(sys.argv) > 3 else 2400
    os.makedirs(out, exist_ok=True)
    box = n64box.Box(script=script)
    rd = lambda a, n: bytes(box.uc.mem_read(box.phys(a), n))
    ticks = open(os.path.join(out, 'ticks.bin'), 'wb')
    state = dict(n=0, first=True)

    def stop(tag, car):
        if state['first']:
            open(os.path.join(out, 'ram.bin'), 'wb').write(rd(0x80000000, 0x800000))
            state['first'] = False
        rec = [tag, bytes([(car - CAR) // CAR_SIZE]), struct.pack('>I', box.frame)]
        for k in range(CARS):
            rec.append(rd(CAR + k * CAR_SIZE, CAR_SIZE))
        for k in range(CARS):
            pad = struct.unpack('>I', rd(CAR + k * CAR_SIZE + 0x2074, 4))[0]
            rec.append(rd(pad, PAD_SIZE))
        rec += [rd(g, 4) for g in GLOBALS]
        for k in range(CARS):                       # each car's race entity's flags (BrCar +0xED0, +0x68)
            link = struct.unpack('>I', rd(CAR + k * CAR_SIZE + 0xED0, 4))[0]
            rec.append(rd(link + 0x68, 4) if link else bytes(4))
        ticks.write(b''.join(rec))

    def on_entry(uc, addr, size, ud):
        stop(b'I', uc.reg_read(M.UC_MIPS_REG_A0) & 0xffffffff)
        state['car'] = uc.reg_read(M.UC_MIPS_REG_A0) & 0xffffffff

    def on_return(uc, addr, size, ud):
        stop(b'O', state['car'])
        state['n'] += 1
        if state['n'] >= limit:
            box.stop_reason = 'enough'
            uc.emu_stop()

    box.uc.hook_add(UC_HOOK_CODE, on_entry, begin=n64box.sx(PHYS_TICK), end=n64box.sx(PHYS_TICK))
    box.uc.hook_add(UC_HOOK_CODE, on_return, begin=n64box.sx(PHYS_RET), end=n64box.sx(PHYS_RET))
    r = box.run(100000)
    ticks.close()
    print('%s: %d physics frames, to retrace %d (%s)' % (out, state['n'], box.frame, r))


if __name__ == '__main__':
    main()
