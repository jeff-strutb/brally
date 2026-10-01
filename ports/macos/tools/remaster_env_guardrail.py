"""remaster_env_guardrail.py -- the roadside guardrail as a real model: one
2 m bay of steel W-beam on a timber post with its spacer block and bolts, the
way the old card paints it (a grey beam across a brown post).  The placement
tool lays bays end to end along each card's line.  Run by Blender:

    blender --background --python ports/macos/tools/remaster_env_guardrail.py -- <out.blend>

Writes a .blend in the shape remaster_env_bake.py reads (collection
guardrail_LOD0, mesh guardrail_a_LOD0) with Poly Haven's corrugated_iron
(galvanised steel) and rough_wood sets wired by their file names.  Axes: x along the rail, y toward
the road (the beam's face), z up; the origin is the beam's centre over the
post's centre line.  The post runs 0.3 m into the ground: deeper, on a crest
whose bank falls toward the road, its foot would show through the bank.
"""
import bpy, bmesh, math, os, sys
from mathutils import Vector

out = sys.argv[sys.argv.index("--") + 1]
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
TEX = os.path.join(ROOT, "ports", "common", "models", "env", "polyhaven", "textures")

BAY = 2.0                 # metres between posts
OVER = 0.08               # each bay's beam laps the next one's
H, D, T = 0.31, 0.083, 0.003   # W-beam height, depth, sheet
POST_W, POST_D = 0.15, 0.20
BLOCK_D = 0.15            # spacer between post and beam
BELOW = 0.85              # post length below the beam's centre (0.3 m into the ground)
ABOVE = 0.02              # the post ends level with the beam's top

bpy.ops.wm.read_factory_settings(use_empty=True)

def material(name, set_, metal, normal=True):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    def img(role, colour):
        p = os.path.join(TEX, set_, f"{set_}_{role}_4k." + ("png" if role in ("nor_gl", "disp") else "jpg"))
        n = nt.nodes.new("ShaderNodeTexImage")
        n.image = bpy.data.images.load(p)
        if not colour:
            n.image.colorspace_settings.name = "Non-Color"
        return n
    c = img("diff", True); nt.links.new(c.outputs["Color"], b.inputs["Base Color"])
    r = img("rough", False); nt.links.new(r.outputs["Color"], b.inputs["Roughness"])
    if metal > 0:                        # its metal channel says metal: only where it is
        img("arm", False)
    if normal:
        n = img("nor_gl", False)
        nm = nt.nodes.new("ShaderNodeNormalMap"); nt.links.new(n.outputs["Color"], nm.inputs["Color"])
        nt.links.new(nm.outputs["Normal"], b.inputs["Normal"])
    b.inputs["Metallic"].default_value = metal
    m.use_backface_culling = True
    return m

# galvanised zinc: the corrugated-iron scan's colour and roughness (its
# normal map is the corrugation, which the beam has as geometry instead)
# Dull galvanising scatters like a pale grey paint; as a full metal it
# would only show reflections, which the environment renderer does not draw
# (measured 2026-10-01: the beam read near black)
steel = material("guardrail_steel", "corrugated_iron", 0.0, normal=False)
wood = material("guardrail_post", "rough_wood", 0.0)

bm = bmesh.new()
uvl = bm.loops.layers.uv.new("UVMap")

def quad(vs, uvs, mat):
    f = bm.faces.new([bm.verts.new(v) for v in vs])
    f.material_index = mat
    for l, uv in zip(f.loops, uvs):
        l[uvl].uv = uv
    return f

# ---- the beam: a W profile swept along x, front and back faces and its rim --
N = 28
prof = []
for k in range(N + 1):
    s = k / N
    z = -H / 2 + H * s
    # flat flanges top and bottom, two ridges between (the "W")
    w = 0.0 if s < 0.06 or s > 0.94 else (1 - math.cos(4 * math.pi * (s - 0.06) / 0.88)) / 2
    prof.append((D * (1 - w), z))          # y: 0 at the post side's valley, D at the ridges' face
x0, x1 = -BAY / 2 - OVER / 2, BAY / 2 + OVER / 2
y_back = POST_D / 2 + BLOCK_D
arc = [0.0]
for k in range(1, len(prof)):
    arc.append(arc[-1] + math.dist(prof[k], prof[k - 1]))
for side, off in ((1, T), (-1, 0.0)):
    for k in range(N):
        (ya, za), (yb, zb) = prof[k], prof[k + 1]
        ya += y_back + off; yb += y_back + off
        v = [(x0, ya, za), (x1, ya, za), (x1, yb, zb), (x0, yb, zb)]
        uv = [(x0, arc[k]), (x1, arc[k]), (x1, arc[k + 1]), (x0, arc[k + 1])]
        if side < 0:
            v, uv = v[::-1], uv[::-1]
        quad(v, uv, 0)
for k, z in ((0, -H / 2), (N, H / 2)):     # top and bottom rims
    y = prof[k][0] + y_back
    v = [(x0, y, z), (x1, y, z), (x1, y + T, z), (x0, y + T, z)]
    quad(v if k == N else v[::-1], [(x0, 0), (x1, 0), (x1, T), (x0, T)], 0)

# ---- timber: the post and the spacer block --------------------------------
def box(cx, cy, cz, sx, sy, sz, mat):
    hx, hy, hz = sx / 2, sy / 2, sz / 2
    P = [(cx + a * hx, cy + b * hy, cz + c * hz) for a in (-1, 1) for b in (-1, 1) for c in (-1, 1)]
    F = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    for f in F:
        vs = [Vector(P[i]) for i in f]
        n = (vs[1] - vs[0]).cross(vs[2] - vs[0])
        # world-space box mapping, 1 unit = 1 m; the grain runs up the post
        if abs(n.z) > 0.5:
            uv = [(v.x, v.y) for v in vs]
        elif abs(n.x) > abs(n.y):
            uv = [(v.y, v.z) for v in vs]
        else:
            uv = [(v.x, v.z) for v in vs]
        quad(vs, [(u * 0.5, w * 0.5) for u, w in uv], mat)
post_top = H / 2 + ABOVE
box(0, 0, (post_top - BELOW) / 2, POST_W, POST_D, post_top + BELOW, 1)
box(0, POST_D / 2 + BLOCK_D / 2, 0, POST_W * 0.95, BLOCK_D, H * 1.15, 1)

# ---- the splice and post bolts: low domes on the beam's face --------------
def bolt(x, z):
    r, h, n = 0.012, 0.008, 8
    y = y_back + D * 0.0 + T          # in the valley, where the bolts go through
    c = bm.verts.new((x, y + h, z))
    ring = [bm.verts.new((x + r * math.cos(2 * math.pi * i / n), y, z + r * math.sin(2 * math.pi * i / n))) for i in range(n)]
    for i in range(n):
        f = bm.faces.new([ring[(i + 1) % n], ring[i], c])
        f.material_index = 0
        for l in f.loops:
            l[uvl].uv = (l.vert.co.x, l.vert.co.z)
for z in (-0.03, 0.03):
    bolt(0.0, z)
for x in (x0 + 0.05, x1 - 0.05):
    for z in (-0.06, 0.06):
        bolt(x, z)

me = bpy.data.meshes.new("guardrail_a_LOD0")
bm.normal_update()
bm.to_mesh(me)
bm.free()
me.materials.append(steel)
me.materials.append(wood)
o = bpy.data.objects.new("guardrail_a_LOD0", me)
col = bpy.data.collections.new("guardrail_LOD0")
bpy.context.scene.collection.children.link(col)
col.objects.link(o)
# sharp edges stay sharp (a pressed steel sheet and sawn timber)
for p in me.polygons:
    p.use_smooth = True
if hasattr(me, "set_sharp_from_angle"):
    me.set_sharp_from_angle(angle=math.radians(40))
for img in bpy.data.images:
    img.pack()
bpy.ops.wm.save_as_mainfile(filepath=out, compress=True)
print("GUARDRAIL_OK", len(me.polygons), "faces", flush=True)
