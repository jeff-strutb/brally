"""remaster_env_addlods.py -- add an artist's own coarser levels to a model
that remaster_env_import.py brought in from its finest level alone (the
glTF a model site serves carries only LOD0; the source archive has the
rest as separate files).  Run by Blender:

    blender --background --python ports/macos/tools/remaster_env_addlods.py -- \
        <model.blend> <asset> <LOD1 file> [<LOD2 file> ...]

Each file's meshes are joined into <asset>_a_LOD<n> in the model's LOD0
collection, fitted onto LOD0 (the same height, foot and centre: the two
exports need not share a unit or an origin), and given LOD0's materials by
name, so every level samples the same textures.  remaster_env_bake.py then
takes them as the foliage's levels.
"""
import bpy, sys, os, re
from mathutils import Matrix, Vector

argv = sys.argv[sys.argv.index("--") + 1:]
blend, asset, files = argv[0], argv[1], argv[2:]
bpy.ops.wm.open_mainfile(filepath=blend)
base = bpy.data.objects.get(f"{asset}_a_LOD0")
if base is None:
    raise SystemExit(f"no {asset}_a_LOD0 in {blend}")
col = base.users_collection[0]


def bounds(o):
    pts = [o.matrix_world @ v.co for v in o.data.vertices]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    return lo, hi


def key(name):
    """a material's name without the exporter's prefixes and level suffixes"""
    n = name.lower()
    n = re.sub(r"\.\d+$", "", n)
    n = re.sub(r"_?lod\d+", "", n)
    return re.sub(r"[\s_]+", "_", n).strip("_")


blo, bhi = bounds(base)
own = {key(m.name): m for m in base.data.materials if m}
for n, f in enumerate(files, start=1):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=f)
    new = [o for o in bpy.data.objects if o not in before]
    meshes = [o for o in new if o.type == "MESH"]
    if not meshes:
        print(f"LODFILE {f}: no meshes", flush=True)
        continue
    mws = [o.matrix_world.copy() for o in meshes]
    for o, mw in zip(meshes, mws):
        me = o.data.copy()
        me.transform(mw)
        o.data = me
    for o in meshes:
        o.parent = None
        o.matrix_basis = Matrix.Identity(4)
    if len(meshes) > 1:
        with bpy.context.temp_override(active_object=meshes[0], object=meshes[0],
                                       selected_objects=meshes, selected_editable_objects=meshes):
            bpy.ops.object.join()
    o = meshes[0]
    for x in new:
        if x is not o and x.name in bpy.data.objects:
            bpy.data.objects.remove(x, do_unlink=True)
    # fit onto LOD0: height, foot, centre
    lo, hi = bounds(o)
    s = (bhi.z - blo.z) / max(hi.z - lo.z, 1e-6)
    o.data.transform(Matrix.Scale(s, 4))
    lo, hi = bounds(o)
    t = Vector(((blo.x + bhi.x - lo.x - hi.x) / 2, (blo.y + bhi.y - lo.y - hi.y) / 2, blo.z - lo.z))
    o.data.transform(Matrix.Translation(t))
    # LOD0's materials, matched by name
    swapped = 0
    for i, m in enumerate(o.data.materials):
        if m is None:
            continue
        k = key(m.name)
        hit = own.get(k) or next((v for kk, v in own.items() if kk.endswith(k) or k.endswith(kk)), None)
        # a part LOD0 lacks (a longer or dead variant of a leaf): the nearest
        # name, dropping words from the end
        t = k.split("_")
        while hit is None and len(t) > 1:
            t = t[:-1]
            hit = next((v for kk, v in own.items() if kk.startswith("_".join(t))), None)
        if hit is not None:
            o.data.materials[i] = hit
            swapped += 1
    o.name = f"{asset}_a_LOD{n}"
    for c in list(o.users_collection):
        c.objects.unlink(o)
    col.objects.link(o)
    tri = sum(len(p.vertices) - 2 for p in o.data.polygons)
    print(f"LOD {n} {os.path.basename(f)}: {tri} tris, scale {s:.4f}, {swapped}/{len(o.data.materials)} materials matched", flush=True)

for m in list(bpy.data.materials):
    if m.users == 0:
        bpy.data.materials.remove(m)
bpy.ops.wm.save_as_mainfile(filepath=blend, compress=True)
print(f"ADDLODS_OK {asset}", flush=True)
