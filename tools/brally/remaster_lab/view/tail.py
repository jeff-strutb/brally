import bpy, sys, math, mathutils
a = sys.argv[sys.argv.index("--") + 1:]
glb, tex, out = a[0], a[1], a[2]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=glb)
o = [x for x in bpy.context.scene.objects if x.type == "MESH"][0]
m = bpy.data.materials.new("t"); m.use_nodes = True
nt = m.node_tree; bs = nt.nodes["Principled BSDF"]; im = nt.nodes.new("ShaderNodeTexImage")
im.image = bpy.data.images.load(tex); nt.links.new(im.outputs["Color"], bs.inputs["Base Color"])
o.data.materials.clear(); o.data.materials.append(m)
sc = bpy.context.scene; sc.render.engine = "BLENDER_WORKBENCH"; sc.display.shading.color_type = "TEXTURE"; sc.display.shading.light = "FLAT"
sc.render.resolution_x = 900; sc.render.resolution_y = 600
cam = bpy.data.objects.new("c", bpy.data.cameras.new("c")); sc.collection.objects.link(cam); sc.camera = cam
bb = [o.matrix_world @ mathutils.Vector(c) for c in o.bound_box]
lo = mathutils.Vector((min(v.x for v in bb), min(v.y for v in bb), min(v.z for v in bb))); hi = mathutils.Vector((max(v.x for v in bb), max(v.y for v in bb), max(v.z for v in bb)))
# Blender import is Z up; the car's length runs along x; the rear is +x (glTF -z... measured: rear at +x_gltf -> blender x)
L = hi - lo; tgt = mathutils.Vector((hi.x, lo.y + L.y * 0.72, lo.z + L.z * 0.42))
eye = tgt + mathutils.Vector((L.x * 0.55, L.y * 0.25, L.z * 0.15))
cam.location = eye; cam.rotation_euler = (tgt - eye).to_track_quat("-Z", "Y").to_euler(); cam.data.lens = 60
sc.render.filepath = out; bpy.ops.render.render(write_still=True)
