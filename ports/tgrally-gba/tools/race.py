"""race.py RAM OUT.c -- the race the GBA plays (gba/race.c on sim/): from a snapshot of the
console's memory at the race's start (tools/simref.py's ram.bin, big-endian):

  the track as the simulation meets it (the loaded track's header at 0x80025C00): its
      vertices and triangles, each triangle's collision plane as BrCollGridCellAcquire
      makes it (the normal of (v1 - v0) x (v2 - v0), its constant), the 64 by 64 grid's
      cell lists, the trigger lists, the extents, the ground ray's default normals
  the grip table (D_802A4A38)
  the cars on the grid: their records (BrCar) and pad records as the game set them,
      each car's lens offsets (its model's +0xB0), the lens scale

Numbers are 32.32 fixed point (sim/fx.h); the car records stay the game's bytes, read by
sim/simload.c."""
import math
import struct
import sys

CAR, CAR_SIZE, PAD_SIZE, CARS = 0x8031B760, 0x2090, 0x15C, 2


def main():
    ram_p, out_p = sys.argv[1:3]
    ram = open(ram_p, 'rb').read()
    u32 = lambda a: struct.unpack_from('>I', ram, a & 0x7FFFFF)[0]
    u16 = lambda a: struct.unpack_from('>H', ram, a & 0x7FFFFF)[0]
    f32 = lambda a: struct.unpack_from('>f', ram, a & 0x7FFFFF)[0]
    q = lambda v: '%dLL' % int(round(v * 4294967296.0))
    H = 0x80025C00
    nt, nv = u32(H + 0x08), u32(H + 0x10)
    verts = [[f32(u32(H + 0x14) + i * 12 + k * 4) for k in range(3)] for i in range(nv)]
    tris = [[u16(u32(H + 0x0C) + i * 8 + k * 2) for k in range(4)] for i in range(nt)]
    surf = [ram[(u32(H + 0x94) & 0x7FFFFF) + i] for i in range(nt)]
    cs = [u16(u32(H + 0x24) + i * 2) for i in range(4097)]
    ct = [u16(u32(H + 0x20) + i * 2) for i in range(cs[4096])]
    ttrg = [u16(u32(H + 0x90) + i * 2) for i in range(nt)]
    n = max(ttrg)
    while u16(u32(H + 0x8C) + n * 2) != 0:
        n += 1
    trg = [u16(u32(H + 0x8C) + i * 2) for i in range(n + 1)]
    o = ['/* the race (tools/race.py): the track as the simulation meets it, the grip table, the',
         '   cars on the grid as the game set them */', '#include "race.h"', '']
    o.append('static const fx s_verts[%d][3] = {%s};' % (nv, ','.join('{%s}' % ','.join(q(c) for c in v) for v in verts)))
    o.append('static const uint16_t s_tris[%d][4] = {%s};' % (nt, ','.join('{%s}' % ','.join(map(str, t)) for t in tris)))
    o.append('static const uint8_t s_surf[%d] = {%s};' % (nt, ','.join(map(str, surf))))
    planes = []
    for i, t in enumerate(tris):
        v0, v1, v2 = (verts[t[k]] for k in range(3))
        a = [v1[k] - v0[k] for k in range(3)]
        b = [v2[k] - v0[k] for k in range(3)]
        nn = [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
        ln = math.sqrt(sum(c * c for c in nn)) or 1.0
        nn = [c / ln for c in nn]
        d = -(nn[0] * v0[0] + nn[1] * v0[1] + nn[2] * v0[2])
        lo = [min(v0[k], v1[k], v2[k]) for k in range(3)]       # the bounds (sim_plane_bounds)
        hi = [max(v0[k], v1[k], v2[k]) for k in range(3)]
        cen = [(lo[k] + hi[k]) / 2 for k in range(3)]
        rad = max(math.sqrt(sum((v[k] - cen[k]) ** 2 for k in range(3))) for v in (v0, v1, v2)) + 0.01
        bx = []                                                  # sim_plane_bounds' eighths
        for k in range(3):
            bx += [math.floor(lo[k] * 8) - 2, math.floor(hi[k] * 8) + 3]
        planes.append('{{%s},%s,s_verts[%d],s_verts[%d],s_verts[%d],%d,%d,0,{%s},%s,%s,%s,%s,%s,{%s}}' % (
            ','.join(q(c) for c in nn), q(d), t[0], t[1], t[2], i, surf[i] & 7, ','.join(q(c) for c in cen), q(rad),
            q(lo[0] - 0.01), q(hi[0] + 0.01), q(lo[1] - 0.01), q(hi[1] + 0.01), ','.join(map(str, bx))))
    o.append('static const Plane s_planes[%d] = {%s};' % (nt, ','.join(planes)))
    o.append('static const uint16_t s_cellStart[4097] = {%s};' % ','.join(map(str, cs)))
    o.append('static const uint16_t s_cellTris[%d] = {%s};' % (max(1, len(ct)), ','.join(map(str, ct)) or '0'))
    o.append('static const uint16_t s_triggers[%d] = {%s};' % (len(trg), ','.join(map(str, trg))))
    o.append('static const uint16_t s_triTrigger[%d] = {%s};' % (nt, ','.join(map(str, ttrg))))
    o.append('const Track g_rt_track = { %d, %d, s_verts, s_tris, s_surf, s_planes, s_cellStart, s_cellTris, s_triggers,'
             ' s_triTrigger, %s, %s, %s, %s, {%s}, {%s} };' % (
                 nv, nt, q(f32(H + 0x28)), q(f32(H + 0x2C)), q(f32(H + 0x38)), q(f32(H + 0x3C)),
                 ','.join(q(f32(0x8028B318 + k * 4)) for k in range(3)),
                 ','.join(q(f32(0x8028B324 + k * 4)) for k in range(3))))
    o.append('const fx g_rt_grip[72] = {%s};' % ','.join(q(f32(0x802A4A38 + i * 4)) for i in range(72)))
    cars, pads, views = [], [], []
    for c in range(CARS):
        base = CAR + c * CAR_SIZE
        cars.append(ram[base & 0x7FFFFF:(base & 0x7FFFFF) + CAR_SIZE])
        pad = u32(base + 0x2074)
        pads.append(ram[pad & 0x7FFFFF:(pad & 0x7FFFFF) + PAD_SIZE])
        model = u32(base + 0x2078)
        views.append([f32(model + 0xB0 + k * 4) for k in range(3)])
    o.append('const uint8_t g_rt_cars[%d][%d] = {%s};' % (CARS, CAR_SIZE, ','.join('{%s}' % ','.join(map(str, c)) for c in cars)))
    o.append('const uint8_t g_rt_pads[%d][%d] = {%s};' % (CARS, PAD_SIZE, ','.join('{%s}' % ','.join(map(str, p)) for p in pads)))
    o.append('const fx g_rt_camView[%d][3] = {%s};' % (CARS, ','.join('{%s}' % ','.join(q(v) for v in vw) for vw in views)))
    o.append('const fx g_rt_lens = %s;' % q(f32(0x8028AAC0)))
    open(out_p, 'w').write('\n'.join(o) + '\n')
    print('%s: %d triangles, %d vertices, %d cell entries, %d cars' % (out_p, nt, nv, len(ct), CARS))


if __name__ == '__main__':
    main()
