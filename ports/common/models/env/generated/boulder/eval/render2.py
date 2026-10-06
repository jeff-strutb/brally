import bpy, sys, math, mathutils
argv = sys.argv[sys.argv.index("--") + 1:]
glb, out = argv[0], argv[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=glb)
objs = [o for o in bpy.context.scene.objects if o.type == "MESH"]
tris = sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in objs)
imgs = [(i.name, i.size[0], i.size[1]) for i in bpy.data.images]
print("STATS tris", tris, "images", imgs, flush=True)
# fit: scale to 2.5 m long, foot on z=0
pts = [o.matrix_world @ v.co for o in objs for v in o.data.vertices]
lo = mathutils.Vector([min(p[i] for p in pts) for i in range(3)]); hi = mathutils.Vector([max(p[i] for p in pts) for i in range(3)])
s = 2.5 / max(hi.x - lo.x, hi.y - lo.y)
root = bpy.data.objects.new("root", None); bpy.context.scene.collection.objects.link(root)
for o in objs:
    if o.parent is None: o.parent = root
root.scale = (s, s, s); root.location = (-(lo.x + hi.x) / 2 * s, -(lo.y + hi.y) / 2 * s, -lo.z * s + 0.0)
bpy.context.view_layer.update()
H = (hi.z - lo.z) * s
# ground
bpy.ops.mesh.primitive_plane_add(size=40)
g = bpy.context.object; m = bpy.data.materials.new("ground"); m.use_nodes = True
m.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.09, 0.1, 0.06, 1)
m.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.95
g.data.materials.append(m)
# sky + sun
w = bpy.data.worlds.new("w"); bpy.context.scene.world = w; w.use_nodes = True
nt = w.node_tree; env = nt.nodes.new("ShaderNodeTexEnvironment"); env.image = bpy.data.images.load(argv[2])
nt.links.new(env.outputs["Color"], nt.nodes["Background"].inputs["Color"]); nt.nodes["Background"].inputs["Strength"].default_value = 1.0
bpy.ops.object.light_add(type="SUN", rotation=(math.radians(55), 0, math.radians(35))); bpy.context.object.data.energy = 2.0
bpy.context.object.data.angle = math.radians(1.5)
sc = bpy.context.scene; sc.render.engine = "CYCLES"; sc.cycles.samples = 96; sc.cycles.device = "GPU"
try:
    p = bpy.context.preferences.addons["cycles"].preferences; p.compute_device_type = "METAL"; p.get_devices()
    for d in p.devices: d.use = True
except Exception as e: print("gpu", e)
sc.render.resolution_x, sc.render.resolution_y = 1600, 1000
sc.view_settings.view_transform = "Standard"; sc.view_settings.exposure = -0.6
cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam")); sc.collection.objects.link(cam); sc.camera = cam
cam.data.lens = 50
for k, (az, el, d) in enumerate([(35, 12, 6.0), (215, 18, 6.0), (80, 25, 3.2)]):
    a, e = math.radians(az), math.radians(el)
    tgt = mathutils.Vector((0, 0, H * 0.45))
    cam.location = tgt + mathutils.Vector((math.cos(a) * math.cos(e), math.sin(a) * math.cos(e), math.sin(e))) * d
    cam.rotation_euler = (tgt - cam.location).to_track_quat("-Z", "Y").to_euler()
    sc.render.filepath = f"{out}_{k}.png"; bpy.ops.render.render(write_still=True)
print("RENDER_OK", flush=True)
