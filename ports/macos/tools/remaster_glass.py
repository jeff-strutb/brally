"""remaster_glass.py -- the Remastered car's windows, from its body.  Run by
Blender on a pack remaster_car.py has written:

    blender --background --python ports/macos/tools/remaster_glass.py -- <pack dir>
            [belt 0.72] [x0 -1.56] [x1 0.62] [ymax 0.84] [inset 0.02] [xc -1.2]

The body has window openings but no glass, and its surface is shell
fragments with no clean window outlines to fill.  So:

  1. The convex hull of the greenhouse is the surface the glass lies in:
     the body above the beltline forward of the C-pillars (xc), and behind
     them down to the boot lid's ledge back to x0, where the rear window
     ends; x1 and ymax leave out the bonnet and the mirrors, and the wing
     is left out by height.  Faces on the cut or facing down are dropped.
  2. It is subdivided to about 2 cm and each point casts a ray inward
     against the body (Blender's BVH).  Body within 6 cm below the hull is
     roof, pillar or frame: no glass there.  Nothing that close is an
     opening: glass.  Behind the C-pillars (x < xc) only rear-facing
     glass counts: the hull's sides there span the air under the wing.
     Islands under 0.03 m2 (dips in the roof or a door skin) are dropped.
     Faces with any glass corner are kept, so every pane
     runs one ring under its frame and no gap shows at the edge.
  3. The panes are set `inset` below the hull, so roof, pillars and frames
     cover their edges, and their normals are smoothed within each pane
     (never across a pillar), so reflections run clean across the glass.

GLASS_EDGE (default 0.03 m) sets the subdivision and GLASS_OUT the output
name, for the lower detail levels (glass_lod1, glass_lod2).

Writes <pack>/glass.rcm ({pos3 nrm3 uv2 tan4} f32, u32 indices, car frame).
"""
import bpy, bmesh, sys, os, struct
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

argv = sys.argv[sys.argv.index("--") + 1:]
pack = argv[0]
edge = float(os.environ.get("GLASS_EDGE", "0.03"))            # subdivision; coarser for the lower detail levels
outname = os.environ.get("GLASS_OUT", "glass")
belt, x0, x1, ymax, inset, xc = [float(v) for v in (argv[1:7] if len(argv) >= 7 else (0.72, -1.56, 0.62, 0.84, 0.02, -1.2))]


def read_rcm(path):
    d = open(path, "rb").read()
    nv, ni = struct.unpack("<II", d[4:12])
    A = np.frombuffer(d[12:12 + nv * 48], np.float32).reshape(nv, 12)
    return A[:, :3].astype(np.float64), np.frombuffer(d[12 + nv * 48:], np.uint32).reshape(-1, 3)


V, F = read_rcm(os.path.join(pack, "body.rcm"))
bvh = BVHTree.FromPolygons([Vector(v) for v in V], F.tolist(), all_triangles=True)
print(f"body {len(F)} tris")

# 1. the greenhouse hull
# forward of the C-pillars the glass starts at the beltline; behind them the
# rear window runs down to the boot lid's ledge (z > 0.45, back to x0), and
# the wing (above z 0.95 behind the roof) is left out
side = np.abs(V[:, 1]) < ymax
m = side & (V[:, 0] < x1) & (
    ((V[:, 2] > belt) & (V[:, 0] > xc)) |
    ((V[:, 0] <= xc + 0.05) & (V[:, 0] > x0) & (V[:, 2] > 0.45) & ~((V[:, 2] > 0.95) & (V[:, 0] < xc - 0.03))))
bm = bmesh.new()
for p in V[m]:
    bm.verts.new(p)
hull = bmesh.ops.convex_hull(bm, input=bm.verts, use_existing_faces=False)
for g in hull["geom_interior"] + hull["geom_unused"]:
    if isinstance(g, bmesh.types.BMVert) and g.is_valid:
        bm.verts.remove(g)
bm.normal_update()
c = Vector(np.mean(V[m], axis=0))
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
drop = [f for f in bm.faces if f.normal.z < -0.2 or (max(abs(v.co.z - belt) for v in f.verts) < 0.03 and min(v.co.x for v in f.verts) > xc)]
bmesh.ops.delete(bm, geom=drop, context="FACES")
print(f"hull {len(bm.faces)} faces")

# 2. subdivide to ~2 cm, classify by an inward ray
for _ in range(12):
    long = [e for e in bm.edges if e.calc_length() > edge]
    if not long:
        break
    bmesh.ops.subdivide_edges(bm, edges=long, cuts=1, use_grid_fill=True)
    bmesh.ops.triangulate(bm, faces=bm.faces)
bm.normal_update()
out_n = {}
glass = {}
for v in bm.verts:
    n = v.normal.normalized()
    if n.dot(v.co - c) < 0:
        n = -n
    out_n[v.index] = n
    hit = bvh.ray_cast(v.co + n * 0.03, -n, 0.8)
    glass[v.index] = hit[0] is None or (hit[3] - 0.03) > 0.06
    # behind the C-pillars the only glass is the rear window, which faces
    # back; the hull's side there spans air between the pillar and the wing
    if v.co.x < xc and abs(n.y) > 0.5:
        glass[v.index] = False
keep = [f for f in bm.faces if any(glass[v.index] for v in f.verts)]
# no specks: a dip in the roof or a door skin deeper than the test can pass
# for an opening over a few points; a real pane is far larger
keep_set = set(keep)
seen, panes = set(), []
for f in keep:
    if f in seen: continue
    comp, stack = [], [f]
    seen.add(f)
    while stack:
        g = stack.pop(); comp.append(g)
        for e in g.edges:
            for h in e.link_faces:
                if h in keep_set and h not in seen:
                    seen.add(h); stack.append(h)
    panes.append(comp)
keep = [f for comp in panes if sum(g.calc_area() for g in comp) > 0.03 for f in comp]
print(f"panes {sum(1 for c in panes if sum(g.calc_area() for g in c) > 0.03)} of {len(panes)} islands")
print(f"subdivided {len(bm.faces)} faces, glass {len(keep)}")

# 3. panes: inset, normals smoothed within each pane
verts = sorted({v.index for f in keep for v in f.verts})
idx = {k: i for i, k in enumerate(verts)}
bm.verts.ensure_lookup_table()
P = np.array([bm.verts[k].co for k in verts])
Nn = np.array([out_n[k] for k in verts])
T = np.array([[idx[v.index] for v in f.verts] for f in keep])
for f_i, f in enumerate(T):            # outward winding
    a, b, cc = P[f]
    if np.dot(np.cross(b - a, cc - a), Nn[f].sum(0)) < 0:
        T[f_i] = f[::-1]
nbr = [[] for _ in verts]
for a, b, cc in T:
    nbr[a] += [b, cc]; nbr[b] += [a, cc]; nbr[cc] += [a, b]
for _ in range(40):
    Ns = Nn.copy()
    for i, ns in enumerate(nbr):
        if not ns:
            continue
        q = Nn[ns]
        q = q[(q @ Nn[i]) > np.cos(np.radians(30))]    # not across a pillar
        Ns[i] = Nn[i] + q.sum(0)
    Nn = Ns / np.linalg.norm(Ns, axis=1, keepdims=True)
P = P - Nn * inset
data = np.hstack([P, Nn, np.zeros((len(P), 6))]).astype(np.float32)
with open(os.path.join(pack, f"{outname}.rcm"), "wb") as fo:
    fo.write(b"RCM1" + struct.pack("<II", len(P), T.size))
    fo.write(data.tobytes()); fo.write(T.astype(np.uint32).tobytes())
print(f"GLASS_OK {len(T)} tris")
