"""remaster_env_impostor.py -- the far level of a Remastered tree: pictures of
the tree, rendered by Blender from eight directions around it, that the game
draws on a card facing the camera once the tree is far enough away.  Run by
Blender, after remaster_env_bake.py:

    blender --background --python ports/brally-wasm/tools/remaster_env_impostor.py -- \
        <models_dir> <asset> [frame_px 256]

The tree is rebuilt in Blender from its own baked mesh (level 0, the .rcm files)
and textures, so the pictures show exactly what the game draws up close.  For
each variant, two strips of eight frames (one per 45 degrees of azimuth,
orthographic, seen level from the side):

    <variant>_imp_base.png    colour, and the cut-out alpha
    <variant>_imp_nrm.png     the surface normal in the frame's own space
                              (x right, y up, z toward the camera), 0..1

and in bake.json, per variant, "impostor": frame size and the box the frames
cover (half width, height).
"""
import bpy, sys, os, json, struct, math
import numpy as np

argv = sys.argv[sys.argv.index("--") + 1:]
models, asset = argv[0], argv[1]
PX = int(argv[2]) if len(argv) > 2 else 256
VIEWS = 8
mdir = os.path.join(models, asset)
tdir = os.path.join(os.path.dirname(os.path.abspath(models)), "textures", asset)
bake = json.load(open(os.path.join(mdir, "bake.json")))

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
# Cycles: Eevee's cut-out transparency mixed the background into the
# leaves' colour (measured: needles (88,84,51) came out (113,112,93))
scene.render.engine = "CYCLES"
scene.cycles.samples = int(os.environ.get("IMP_SAMPLES", "32"))
scene.cycles.use_denoising = False
scene.cycles.max_bounces = 0
scene.cycles.transparent_max_bounces = 64
scene.render.film_transparent = True
scene.render.image_settings.file_format = "PNG"
scene.render.image_settings.color_mode = "RGBA"
scene.render.image_settings.color_depth = "8"
scene.view_settings.view_transform = "Standard"
scene.display_settings.display_device = "sRGB"
scene.render.filter_size = 0.8

def read_rcm(path):
    d = open(path, "rb").read()
    nv, ni = struct.unpack_from("<II", d, 4)
    V = np.frombuffer(d, np.float32, nv * 12, 12).reshape(nv, 12)
    I = np.frombuffer(d, np.uint32, ni, 12 + nv * 48)
    return V, I

def material(j, mode):
    """mode 'base': the texture's colour, emitted, cut out by its alpha;
    mode 'nrm': the view-space normal as colour, cut out the same way."""
    m = bpy.data.materials.new(f"m{j}_{mode}")
    m.use_nodes = True
    nt = m.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    tex = nt.nodes.new("ShaderNodeTexImage")
    info = bake["materials"][j].get("textures", {})
    if "base" in info:
        tex.image = bpy.data.images.load(os.path.join(tdir, info["base"]), check_existing=True)
        tex.image.alpha_mode = "STRAIGHT"
    em = nt.nodes.new("ShaderNodeEmission")
    if mode == "base":
        nt.links.new(tex.outputs["Color"], em.inputs["Color"])
    else:
        # the camera-space normal, 0..1
        geo = nt.nodes.new("ShaderNodeNewGeometry")
        vt = nt.nodes.new("ShaderNodeVectorTransform")
        vt.vector_type = "NORMAL"
        vt.convert_from = "WORLD"
        vt.convert_to = "CAMERA"
        nt.links.new(geo.outputs["Normal"], vt.inputs["Vector"])
        mad = nt.nodes.new("ShaderNodeVectorMath")
        mad.operation = "MULTIPLY_ADD"
        mad.inputs[1].default_value = (0.5, 0.5, -0.5)   # Blender camera space: toward the camera is -z (measured); stored as +z
        mad.inputs[2].default_value = (0.5, 0.5, 0.5)
        nt.links.new(vt.outputs["Vector"], mad.inputs[0])
        nt.links.new(mad.outputs["Vector"], em.inputs["Color"])
    clip = bake["materials"][j]["alpha_clip"]
    if clip and tex.image:
        gt = nt.nodes.new("ShaderNodeMath")
        gt.operation = "GREATER_THAN"
        gt.inputs[1].default_value = 0.5
        nt.links.new(tex.outputs["Alpha"], gt.inputs[0])
        tr = nt.nodes.new("ShaderNodeBsdfTransparent")
        mix = nt.nodes.new("ShaderNodeMixShader")
        nt.links.new(gt.outputs[0], mix.inputs["Fac"])
        nt.links.new(tr.outputs[0], mix.inputs[1])
        nt.links.new(em.outputs[0], mix.inputs[2])
        nt.links.new(mix.outputs[0], out.inputs["Surface"])
        if hasattr(m, "blend_method"):
            m.blend_method = "CLIP"
        if hasattr(m, "surface_render_method"):
            m.surface_render_method = "DITHERED"
    else:
        nt.links.new(em.outputs[0], out.inputs["Surface"])
    return m

for v in bake["variants"]:
    # level 0 of the variant, every material
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    objs = []
    for f in v["lods"][0]["files"]:
        V, I = read_rcm(os.path.join(mdir, f["file"]))
        me = bpy.data.meshes.new(f["file"])
        me.vertices.add(len(V))
        me.vertices.foreach_set("co", V[:, 0:3].ravel())
        nt = len(I) // 3
        me.loops.add(len(I))
        me.loops.foreach_set("vertex_index", I.astype(np.int32))
        me.polygons.add(nt)
        me.polygons.foreach_set("loop_start", np.arange(0, len(I), 3, dtype=np.int32))
        me.polygons.foreach_set("loop_total", np.full(nt, 3, np.int32))
        uv = me.uv_layers.new(name="UVMap")
        UV = V[I][:, 6:8].copy()
        UV[:, 1] = 1.0 - UV[:, 1]          # the file's v runs down, Blender's up
        uv.data.foreach_set("uv", UV.ravel())
        me.update()
        ob = bpy.data.objects.new(f["file"], me)
        scene.collection.objects.link(ob)
        ob["mat"] = f["mat"]
        objs.append(ob)
    lo, hi = v["bounds"]
    half = max(abs(lo[0]), abs(hi[0]), abs(lo[1]), abs(hi[1])) * 1.02
    height = hi[2] * 1.02
    cam_d = bpy.data.cameras.new("cam")
    cam_d.type = "ORTHO"
    cam_d.ortho_scale = max(2 * half, height)
    cam = bpy.data.objects.new("cam", cam_d)
    scene.collection.objects.link(cam)
    scene.camera = cam
    # each frame: 2*half wide, height tall, at PX per (2*half)
    fw = PX
    fh = int(round(PX * height / (2 * half)))
    scene.render.resolution_x = fw
    scene.render.resolution_y = fh
    cam_d.ortho_scale = max(2 * half, height)
    cam_d.sensor_fit = "VERTICAL" if height > 2 * half else "HORIZONTAL"
    out = {}
    for mode in ("base", "nrm"):
        # colour through the sRGB view; the normal written as is
        scene.view_settings.view_transform = "Standard" if mode == "base" else "Raw"
        for ob in objs:
            ob.data.materials.clear()
            ob.data.materials.append(material(ob["mat"], mode))
        strip = np.zeros((fh, fw * VIEWS, 4), np.float32)
        for k in range(VIEWS):
            a = 2 * math.pi * k / VIEWS
            # the camera on the circle at azimuth a, looking at the tree's middle, level
            cam.location = (math.cos(a) * half * 4, math.sin(a) * half * 4, height / 2)
            cam.rotation_euler = (math.pi / 2, 0, a + math.pi / 2)
            path = os.path.join(mdir, f"_imp_tmp.png")
            scene.render.filepath = path
            bpy.ops.render.render(write_still=True)
            img = bpy.data.images.load(path)
            px = np.array(img.pixels[:], np.float32).reshape(fh, fw, 4)
            bpy.data.images.remove(img)
            strip[:, k * fw:(k + 1) * fw] = px
        img = bpy.data.images.new("strip", fw * VIEWS, fh, alpha=True)
        img.colorspace_settings.name = "sRGB" if mode == "base" else "Non-Color"
        img.pixels.foreach_set(strip.ravel())
        name = f"{v['name']}_imp_{mode}.png"
        img.filepath_raw = os.path.join(tdir, name)
        img.file_format = "PNG"
        img.save()
        bpy.data.images.remove(img)
        out[mode] = name
    os.remove(os.path.join(mdir, "_imp_tmp.png"))
    v["impostor"] = {"views": VIEWS, "frame": [fw, fh], "half": half, "height": height,
                     "base": out["base"], "nrm": out["nrm"]}
    print(f"IMPOSTOR {asset} {v['name']} {fw}x{fh} x{VIEWS}", flush=True)

json.dump(bake, open(os.path.join(mdir, "bake.json"), "w"), indent=1)
print(f"IMPOSTOR_OK {asset}", flush=True)
