import bpy, sys, math, mathutils
d = sys.argv[sys.argv.index("--") + 1]
bpy.ops.wm.read_factory_settings(use_empty=True)
for n, col in (("body", (0.6, 0.6, 0.6, 1)), ("glass", (1, 0.1, 0.1, 1))):
    bpy.ops.wm.ply_import(filepath=f"{d}/{n}.ply")
    o = bpy.context.selected_objects[0]
    m = bpy.data.materials.new(n); m.diffuse_color = col; o.data.materials.append(m)
sc = bpy.context.scene
sc.render.engine = "BLENDER_WORKBENCH"; sc.display.shading.color_type = "MATERIAL"
sc.display.shading.show_xray = False
sc.render.resolution_x = 1200; sc.render.resolution_y = 800
cam = bpy.data.objects.new("c", bpy.data.cameras.new("c")); sc.collection.objects.link(cam); sc.camera = cam
cam.data.lens = 50
for name, eye in (("side", (0, 9, 0.6)), ("front34", (6, 5, 2.5)), ("rear34", (-6, 5, 2.5)), ("front", (9, 0, 1.2)), ("rear", (-9, 0, 1.6)), ("top", (0.01, 0, 11))):
    cam.location = eye
    dvec = mathutils.Vector((0, 0, 0.45)) - mathutils.Vector(eye)
    cam.rotation_euler = dvec.to_track_quat("-Z", "Y").to_euler()
    sc.render.filepath = f"{d}/{name}.png"
    bpy.ops.render.render(write_still=True)
