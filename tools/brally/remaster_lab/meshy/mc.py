import bpy, sys, mathutils
a = sys.argv[sys.argv.index("--") + 1:]
glb, tag, d = a[0], a[1], a[2]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=glb)
o = [x for x in bpy.context.scene.objects if x.type == "MESH"][0]
sc = bpy.context.scene; sc.render.engine = "BLENDER_WORKBENCH"
sh = sc.display.shading; sh.light = "MATCAP"; sh.studio_light = "check_normal+y.exr" if False else "metal_carpaint.exr"; sh.color_type = "SINGLE"; sh.single_color = (0.1, 0.5, 0.2)
sc.render.resolution_x = 1000; sc.render.resolution_y = 700
cam = bpy.data.objects.new("c", bpy.data.cameras.new("c")); sc.collection.objects.link(cam); sc.camera = cam
bb = [o.matrix_world @ mathutils.Vector(c) for c in o.bound_box]
c = sum(bb, mathutils.Vector()) / 8; L = max((max(v[i] for v in bb) - min(v[i] for v in bb)) for i in range(3))
for name, dirv in (("rear34", (1.0, 0.8, 0.5)), ("side", (0.05, 1.0, 0.15)), ("front34", (-1.0, 0.8, 0.5))):
    dv = mathutils.Vector(dirv).normalized(); cam.location = c + dv * L * 1.5
    cam.rotation_euler = (c - cam.location).to_track_quat("-Z", "Y").to_euler(); cam.data.lens = 50
    sc.render.filepath = f"{d}/{tag}_{name}.png"; bpy.ops.render.render(write_still=True)
