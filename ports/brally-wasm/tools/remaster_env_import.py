"""remaster_env_import.py -- bring a downloaded third-party model (FBX, OBJ,
glTF, Collada, .blend) into the shape remaster_env_bake.py reads: one .blend
whose variants are meshes named <asset>_<variant>_LOD0 in an <asset>_LOD0
collection, images packed in.  Run by Blender:

    blender --background --python ports/brally-wasm/tools/remaster_env_import.py -- \
        <source file> <asset> <out.blend>

Each top-level object of the file becomes one variant: its meshes are joined
(materials kept, UVs kept, no geometry changed).  Images that the importer
left unresolved are looked for by name anywhere under the source's folder.
"""
import bpy, sys, os, re

argv = sys.argv[sys.argv.index("--") + 1:]
src, asset, out = argv[0], argv[1], argv[2]
# "one": the whole file is one plant (pieces joined); "pack": one plant per
# top-level group of the file, as its author grouped them
mode = argv[3] if len(argv) > 3 else "one"
root = os.path.dirname(os.path.abspath(src))
while os.path.basename(os.path.dirname(root)) and len(os.path.basename(root)) != 32 and root != "/":
    root = os.path.dirname(root)          # up to the model's download folder

ext = os.path.splitext(src)[1].lower()
if ext == ".blend":
    bpy.ops.wm.open_mainfile(filepath=src)
else:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    if ext == ".fbx":
        bpy.ops.import_scene.fbx(filepath=src)
    elif ext == ".obj":
        bpy.ops.wm.obj_import(filepath=src)
    elif ext in (".gltf", ".glb"):
        bpy.ops.import_scene.gltf(filepath=src)
    elif ext == ".dae":
        bpy.ops.wm.collada_import(filepath=src)
    else:
        raise SystemExit("unsupported " + ext)

# images: resolve missing files by basename under the download folder
index = {}
for dp, dn, fn in os.walk(root):
    for f in fn:
        index.setdefault(f.lower(), os.path.join(dp, f))
missing = 0
for img in bpy.data.images:
    if img.packed_file or img.source != "FILE":
        continue
    p = bpy.path.abspath(img.filepath)
    if not os.path.exists(p):
        q = index.get(os.path.basename(p).lower())
        if q:
            img.filepath = q
        else:
            missing += 1
            continue
    try:
        img.pack()
    except Exception:
        missing += 1

scene = bpy.context.scene
meshes = [o for o in scene.objects if o.type == "MESH"]
if not meshes:
    raise SystemExit("no meshes in " + src)

def has_mesh(o):
    return o.type == "MESH" or any(has_mesh(c) for c in o.children)

def variant_roots():
    """Sketchfab's glTF wraps the whole file in a chain of single nodes
    (Sketchfab_model > root > GLTF_SceneRootNode): the variants are the
    children where that chain first branches."""
    roots = [o for o in scene.objects if o.parent is None and has_mesh(o)]
    while len(roots) == 1 and roots[0].type != "MESH":
        kids = [c for c in roots[0].children if has_mesh(c)]
        if not kids:
            break
        roots = kids
    return roots

def meshes_under(o):
    out = [o] if o.type == "MESH" else []
    for c in o.children:
        out += meshes_under(c)
    return out

from mathutils import Vector, Matrix

def bbox(objs):
    pts = [o.matrix_world @ Vector(c) for o in objs for c in o.bound_box]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    return lo, hi

# one plant = the pieces whose footprints overlap: a tree's trunk and fronds
# stand on one spot, a pack's separate plants are laid out apart
# a pack's own lower levels (..._LOD1, _LOD2 ...) go: the bake makes its own
for o in list(scene.objects):
    if o.type == "MESH" and re.search(r"LOD[1-9]", o.name):
        bpy.data.objects.remove(o, do_unlink=True)
roots = variant_roots()
parts = [m for m in (meshes_under(r) for r in roots) if m]
boxes = [bbox(m) for m in parts]

# 1. outliers go first: a pack's ground disc or backdrop is flat, or far
#    wider than the file's typical piece (sizes are the artist's units)
def width(bx):
    d = bx[1] - bx[0]
    return max(d.x, d.y)
ws = sorted(width(bx) for bx in boxes)
typical = ws[len(ws) // 2] if ws else 1
keep = []
for m, bx in zip(parts, boxes):
    d = bx[1] - bx[0]
    tri = sum(len(p.vertices) - 2 for o in m for p in o.data.polygons)
    if (tri < 200 and len(parts) > 1) or d.z < 0.05 * max(d.x, d.y):
        print(f"DROPPED {m[0].name} {tri} tris dims {tuple(round(x, 2) for x in d)}", flush=True)
        for o in m:
            bpy.data.objects.remove(o, do_unlink=True)
    else:
        keep.append((m, bx))
parts = [k[0] for k in keep]; boxes = [k[1] for k in keep]

# 2. one plant per name: pieces whose names differ only by Blender's
#    duplicate suffix (".001") are one object split up (King Coconut's
#    fronds: defaultMaterial, defaultMaterial.001, ...); differently named
#    pieces are a pack's separate plants
roots_kept = []
for m in parts:
    r = m[0]
    while r.parent is not None and r.parent not in roots:
        r = r.parent
    roots_kept.append(r)
merged = {}
for r, m in zip(roots_kept, parts):
    key = re.sub(r"\.\d{3}$", "", r.name)
    merged.setdefault(key, []).extend(m)
# 3. a piece whose footprint lies mostly inside another's is part of it (a
#    trunk under its crown); plants of a pack stand beside one another
#    Only for loose meshes: a file that groups its pieces under named empties
#    has already said which pieces make a plant (a banana under a palm in a
#    composed pack is still its own plant)
items = [(k, v, bbox(v)) for k, v in merged.items()]
changed = not any(r.type != "MESH" for r in roots_kept)
while changed:
    changed = False
    for i in range(len(items)):
        for j in range(len(items)):
            if i == j:
                continue
            (_, _, (a0, a1)), (_, _, (b0, b1)) = items[i], items[j]
            ix = max(0.0, min(a1.x, b1.x) - max(a0.x, b0.x)); iy = max(0.0, min(a1.y, b1.y) - max(a0.y, b0.y))
            area_i = max(1e-9, (a1.x - a0.x) * (a1.y - a0.y))
            if ix * iy / area_i > 0.7 and area_i < (b1.x - b0.x) * (b1.y - b0.y):
                k, v = items[j][0], items[j][1] + items[i][1]
                items[j] = (k, v, bbox(v))
                del items[i]
                changed = True
                break
        if changed:
            break
groups = {v[0]: v for _, v, _ in items}
if mode == "one":
    allp = [o for v in groups.values() for o in v]
    groups = {allp[0]: allp} if allp else {}
else:
    groups = {}
    for r, m in zip(roots_kept, parts):
        groups.setdefault(r, []).extend(m)
    if mode == "packc":
        # a pack split by material (a tree's bark and its foliage as separate
        # groups): groups whose footprints mostly overlap are one plant
        its = [(k, v, bbox(v)) for k, v in groups.items()]
        again = True
        while again:
            again = False
            for i in range(len(its)):
                for j in range(i + 1, len(its)):
                    (a0, a1), (b0, b1) = its[i][2], its[j][2]
                    ix = max(0.0, min(a1.x, b1.x) - max(a0.x, b0.x)); iy = max(0.0, min(a1.y, b1.y) - max(a0.y, b0.y))
                    small = min((a1.x - a0.x) * (a1.y - a0.y), (b1.x - b0.x) * (b1.y - b0.y))
                    if ix * iy > 0.5 * max(small, 1e-9):
                        v = its[i][1] + its[j][1]
                        its[i] = (its[i][0], v, bbox(v)); del its[j]; again = True; break
                if again: break
        groups = {k: v for k, v, _ in its}
for k, v in groups.items():
    print("GROUP", k.name, [o.name[:30] for o in v][:6], flush=True)

col = bpy.data.collections.new(asset + "_LOD0")
scene.collection.children.link(col)
names = []
for k, (t, objs) in enumerate(sorted(groups.items(), key=lambda kv: kv[0].name)):
    # world transforms baked in (all read before any is changed: unparenting
    # one piece moves its children), then joined into one mesh per variant
    mws = [o.matrix_world.copy() for o in objs]
    for o, mw in zip(objs, mws):
        me = o.data.copy()
        me.transform(mw)
        o.data = me
    for o in objs:
        o.parent = None
        # assigned, not .identity() on the returned copy (which changed
        # nothing and left the parent's scale undone: ferns came out 40x)
        o.matrix_basis = Matrix.Identity(4)
    # exactly these pieces: the operator otherwise also joins whatever the
    # selection still holds (measured: each variant swallowed the previous one)
    if len(objs) > 1:
        with bpy.context.temp_override(active_object=objs[0], object=objs[0],
                                       selected_objects=objs, selected_editable_objects=objs):
            bpy.ops.object.join()
    o = objs[0]
    tri = sum(len(p.vertices) - 2 for p in o.data.polygons)
    v = chr(ord("a") + len(names)) if len(names) < 26 else f"v{len(names)}"
    o.name = f"{asset}_{v}_LOD0"
    for c in list(o.users_collection):
        c.objects.unlink(o)
    col.objects.link(o)
    names.append((o.name, tri, tuple(round(d, 2) for d in o.dimensions)))

# everything else goes
for o in list(scene.objects):
    if o.name not in [n for n, _, _ in names]:
        bpy.data.objects.remove(o, do_unlink=True)

# Z up, metres: an importer leaves a model lying down or at centimetre scale;
# report it, the placement tool scales to the card anyway
for n, t, d in names:
    print(f"VARIANT {n} {t} tris dims {d}", flush=True)
print(f"IMAGES missing {missing}", flush=True)
bpy.ops.wm.save_as_mainfile(filepath=out, compress=True)
print(f"IMPORT_OK {asset} {len(names)} variants", flush=True)
