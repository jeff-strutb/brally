"""remaster_bake.py -- decimates a raw (un-remeshed) textured model for the
Remastered car with Blender's Collapse Decimate, keeping the model's own UVs
and textures.  Run by Blender:

    blender --background --python ports/macos/tools/remaster_bake.py -- \
        <raw.glb> <out_dir> <name> <target_tris> [crease_deg 45]

The model is Collapse-decimated and its normals are recomputed smooth by
angle (creases above crease_deg stay sharp): the same steps as the web
project's validated Blender route.
Decimate is the only operation that changes the shape.  A source that was
already remeshed to a polygon budget is lumpy at the triangle scale; the
full-density source decimated here is not.  Nothing is unwrapped or
re-baked: the model's UVs and texture set go through unchanged.

Writes <name>.glb (geometry, normals, MikkTSpace tangents, UVs) and the raw
model's images beside it: <name>_base.png, <name>_mr.png (metal/roughness,
glTF channels) and <name>_normal.png when the model has them.
"""
import bpy, sys, os, math

argv = sys.argv[sys.argv.index("--") + 1:]
src_path, out_dir, name, target = argv[0], argv[1], argv[2], int(argv[3])
crease = float(argv[4]) if len(argv) > 4 else 45.0
os.makedirs(out_dir, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src_path)
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
if len(meshes) != 1:
    raise SystemExit(f"expected one mesh in {src_path}, found {len(meshes)}")
obj = meshes[0]
src_tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
print(f"SRC {src_tris} tris")

# the model's images, by the principled BSDF input each one feeds
bsdf = next(n for n in obj.active_material.node_tree.nodes if n.type == "BSDF_PRINCIPLED")


def image_behind(socket):
    stack = [l.from_node for l in socket.links]
    while stack:
        n = stack.pop()
        if n.type == "TEX_IMAGE" and n.image:
            return n.image
        for i in n.inputs:
            stack += [l.from_node for l in i.links]
    return None


for suffix, sock in (("base", "Base Color"), ("mr", "Roughness"), ("normal", "Normal")):
    img = image_behind(bsdf.inputs[sock])
    if img is None:
        print(f"NO {suffix} image")
        continue
    img.filepath_raw = os.path.join(out_dir, f"{name}_{suffix}.png")
    img.file_format = "PNG"
    img.save()
    print(f"SAVED {name}_{suffix}.png {img.size[0]}x{img.size[1]}")

for o in bpy.context.scene.objects:
    o.select_set(False)
obj.select_set(True)
bpy.context.view_layer.objects.active = obj
# no weld: the validated route welds only meshes it re-bakes; welding lets
# Collapse merge vertices across the texture's seams, which drags the texture
# (measured: the tail lights warped at the 150k level)
dm = obj.modifiers.new("dec", "DECIMATE")
dm.decimate_type = "COLLAPSE"
dm.ratio = min(1.0, target / max(1, src_tris))
bpy.ops.object.modifier_apply(modifier="dec")
# crease normals from the decimated surface: the kept vertices otherwise carry
# the full-density model's normals, 15-23 degrees off the coarser surface,
# which shades as lumps
bpy.ops.object.shade_smooth_by_angle(angle=math.radians(crease))
out_tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
print(f"DECIMATED {out_tris} tris (ratio {target / max(1, src_tris):.4f})")

bpy.ops.export_scene.gltf(filepath=os.path.join(out_dir, f"{name}.glb"), export_format="GLB",
                          use_selection=True, export_tangents=True, export_normals=True,
                          export_texcoords=True, export_materials="NONE", export_apply=True)
print(f"BAKE_OK {name} {out_tris} tris")
