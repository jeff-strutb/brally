"""remaster_glass_split.py -- splits a baked model's glass from its solid
parts by the model's own texture, for a model whose glass came out painted
(an opaque light grey) instead of see-through.  Run by Blender on a model
remaster_bake.py has written:

    blender --background --python ports/brally-wasm/tools/remaster_glass_split.py -- \
        <dir> <name> [min_value 0.70] [max_sat 0.10]

Each face is glass when most of the texels under its corners and centre are
light and colourless (value >= min_value, saturation <= max_sat): the glass
is the only light grey on such a model; iron, brass and paint are dark or
coloured.  Nothing moves: the faces are only sorted into two meshes, with
their UVs and normals as baked, so the solid part keeps the model's texture
and the glass part is drawn by the glass shader.

Writes <dir>/<name>_solid.glb and <dir>/<name>_glass.glb beside <name>.glb
and prints the share of faces in each.
"""
import bpy, bmesh, sys, os
import numpy as np

argv = sys.argv[sys.argv.index("--") + 1:]
d, name = argv[0], argv[1]
vmin = float(argv[2]) if len(argv) > 2 else 0.70
smax = float(argv[3]) if len(argv) > 3 else 0.10

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=os.path.join(d, f"{name}.glb"))
obj = [o for o in bpy.context.scene.objects if o.type == "MESH"][0]
img = bpy.data.images.load(os.path.join(d, f"{name}_base.png"))
w, h = img.size
px = np.empty(w * h * 4, np.float32)
img.pixels.foreach_get(px)
px = px.reshape(h, w, 4)[..., :3]
mx, mn = px.max(2), px.min(2)
glassy = (mx >= vmin) & ((mx - mn) <= smax * np.maximum(mx, 1e-6))

me = obj.data
uv = me.uv_layers.active.data
mask = np.zeros(len(me.polygons), bool)
for p in me.polygons:
    pts = [uv[l].uv for l in p.loop_indices]
    c = sum((q for q in pts), pts[0] * 0) / len(pts)
    votes = 0
    for q in pts + [c]:
        x = int(q.x % 1.0 * w) % w
        y = int(q.y % 1.0 * h) % h       # Blender images are stored bottom-up, as UVs are
        votes += glassy[y, x]
    mask[p.index] = votes * 2 > len(pts) + 1
print(f"GLASS {mask.sum()} of {len(mask)} faces ({mask.mean():.1%})")


def export(keep, suffix):
    o = obj.copy(); o.data = obj.data.copy(); bpy.context.scene.collection.objects.link(o)
    bm = bmesh.new(); bm.from_mesh(o.data); bm.faces.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if mask[f.index] != keep], context="FACES")
    bm.to_mesh(o.data); bm.free()
    for x in bpy.context.scene.objects:
        x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.export_scene.gltf(filepath=os.path.join(d, f"{name}_{suffix}.glb"), export_format="GLB",
                              use_selection=True, export_tangents=True, export_normals=True,
                              export_texcoords=True, export_materials="NONE", export_apply=True)
    bpy.data.objects.remove(o, do_unlink=True)


export(False, "solid")
export(True, "glass")
print(f"SPLIT_OK {name}")
