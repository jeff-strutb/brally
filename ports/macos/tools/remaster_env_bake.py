"""remaster_env_bake.py -- one Remastered environment model (a tree, a bush, a
rock, a clump of grass) from its source .blend, by the same Blender route as
the car: Collapse Decimate to 200k, Subdivision Surface level 3, Collapse
Decimate to the target, normals smooth by angle.  Run by Blender:

    blender --background --python ports/macos/tools/remaster_env_bake.py -- \
        <source.blend> <asset> <models_out> <lod0_tris> <lod1_tris> <lod2_tris>

The source's own variants are kept (a model file usually holds several: tree
a, b and c).  They are the meshes of its "<asset>_LOD0" collection, or, when
it has none, every mesh that is not a scatter or preview helper.  Each variant
is decimated from its full-detail source to each of the three targets; a
variant already under a target is kept as it is.  Nothing is unwrapped,
re-baked or remeshed: the model's UVs and textures go through unchanged.

Writes, under <models_out>/<asset>/:
    <variant>_lod<k>_m<j>.rcm   one per variant, level and material
                                ("RCM1", nv, ni, nv x {pos3 nrm3 uv2 tan4}, ni u32)
    bake.json                   variants, their bounds and triangle counts,
                                and each material's source images by role
remaster_env_pack.py turns bake.json's images into the runtime textures.
"""
import bpy, bmesh, sys, os, math, json, struct
from mathutils import Matrix, Vector

argv = sys.argv[sys.argv.index("--") + 1:]
src, asset, out_root = argv[0], argv[1], argv[2]
targets = [int(a) for a in argv[3:6]]
out = os.path.join(out_root, asset)
os.makedirs(out, exist_ok=True)
CREASE = 45.0

bpy.ops.wm.open_mainfile(filepath=src)
scene = bpy.context.scene

def tris_of(o):
    return sum(len(p.vertices) - 2 for p in o.data.polygons)

# ---- the variants -----------------------------------------------------------
lod0 = bpy.data.collections.get(asset + "_LOD0")
if lod0 is not None:
    srcs = [o for o in lod0.all_objects if o.type == "MESH"]
else:
    helper = ("geometry_nodes", "geonodes", "_LOD1", "_LOD2", "_LOD3", "_LOD4")
    srcs = [o for o in scene.objects if o.type == "MESH" and not o.modifiers and
            not any(h in c.name for c in o.users_collection for h in helper) and tris_of(o) > 0]
if not srcs:
    raise SystemExit(f"{asset}: no source meshes")
srcs.sort(key=lambda o: o.name)

def variant_name(o):
    n = o.name
    if n.startswith(asset):
        n = n[len(asset):]
    n = n.replace("_LOD0", "").strip("_")
    return n or "a"

# ---- the materials' images ---------------------------------------------------
# Poly Haven names every map <material>_<role>_<res>.<ext> and wires them
# through node groups, so each material's set is found by name among the
# images its node tree (groups included) uses.
ROLES = {"diff": "base", "alpha": "alpha", "rough": "rough", "nor_gl": "normal", "arm": "arm", "ao": "ao",
         "metal": "metal", "translucent": "trans", "trans": "trans"}

def images_of(tree, acc):
    for n in tree.nodes:
        if n.type == "TEX_IMAGE" and n.image:
            acc.append(n.image)
        elif n.type == "GROUP" and n.node_tree:
            images_of(n.node_tree, acc)
    return acc

def role_of(path):
    stem = os.path.splitext(os.path.basename(path))[0]
    parts = stem.split("_")
    if len(parts) > 2 and parts[-1].endswith("k"):
        parts = parts[:-1]
    for k in sorted(ROLES, key=len, reverse=True):
        kk = k.split("_")
        if parts[-len(kk):] == kk:
            return ROLES[k]
    return None

mats = []
def mat_index(m):
    for j, e in enumerate(mats):
        if e["name"] == (m.name if m else ""):
            return j
    e = {"name": m.name if m else "", "images": {}}
    if m and m.use_nodes:
        # a material that blends two sets (a trunk over the shared bark) keeps its own
        own = lambda img: not os.path.basename(img.filepath).startswith(m.name + "_")
        for img in sorted(images_of(m.node_tree, []), key=own):
            r = role_of(img.filepath)
            if r and r not in e["images"]:
                e["images"][r] = img.name
    if m and m.use_nodes and "base" not in e["images"]:
        # not Poly Haven's naming (a glTF import): the images by the input
        # of the principled shader they feed
        b = next((n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
        def src_image(sock):
            stack = [(l.from_node, l.from_socket.name) for l in sock.links]
            while stack:
                n, out = stack.pop()
                if n.type == "TEX_IMAGE" and n.image:
                    return n.image, out
                for i in n.inputs:
                    stack += [(l.from_node, l.from_socket.name) for l in i.links]
            return None, None
        if not b:
            # an unlit export (photoscans often are): the colour image feeds an
            # emission shader instead
            em = next((n for n in m.node_tree.nodes if n.type == "EMISSION"), None)
            if em:
                img, _ = src_image(em.inputs["Color"])
                if img: e["images"]["base"] = img.name
        if b:
            img, _ = src_image(b.inputs["Base Color"])
            if img: e["images"]["base"] = img.name
            img, out = src_image(b.inputs["Alpha"])
            if img:
                e["images"]["alpha_from"] = img.name
                e["alpha_out"] = out
            img, _ = src_image(b.inputs["Roughness"])
            if img: e["images"]["gltf_mr"] = img.name     # glTF: G roughness, B metal
            img, _ = src_image(b.inputs["Normal"])
            if img: e["images"]["normal"] = img.name
            if not b.inputs["Alpha"].links and b.inputs["Alpha"].default_value < 0.99:
                e["images"].pop("alpha_from", None)
        e["blend"] = getattr(m, "blend_method", "OPAQUE")
    e["alpha_clip"] = "alpha" in e["images"] or "alpha_from" in e["images"]
    e["two_sided"] = e["alpha_clip"] or not (m and m.use_backface_culling)
    mats.append(e)
    return len(mats) - 1

# ---- the route ----------------------------------------------------------------
def select_only(o):
    for x in bpy.context.view_layer.objects:
        if x is not None:
            x.select_set(False)
    o.select_set(True)
    bpy.context.view_layer.objects.active = o

def decimate_to(o, n):
    t = tris_of(o)
    if t <= n:
        return
    d = o.modifiers.new("dec", "DECIMATE")
    d.decimate_type = "COLLAPSE"
    d.ratio = n / t
    select_only(o)
    bpy.ops.object.modifier_apply(modifier="dec")

PRE = int(os.environ.get("SUBSURF_PRE", "200000"))
LEVELS = int(os.environ.get("SUBSURF", "0"))   # 3 for a generated model (the car route); 0 for an authored one

def subsurf(o):
    select_only(o)
    s = o.modifiers.new("ss", "SUBSURF")
    s.levels = LEVELS
    s.render_levels = LEVELS
    bpy.ops.object.modifier_apply(modifier="ss")

def standard(o, target):
    """The car's route: Collapse to 200k, Subdivision Surface level 3,
    Collapse to the target (remaster_bake.py; about 25 GB at level 3, so
    bakes run one at a time)."""
    if tris_of(o) <= target:
        return
    decimate_to(o, PRE)
    if LEVELS > 0:
        subsurf(o)
    decimate_to(o, target)

def triangulate(o):
    """Triangles, keeping the mesh's own (authored) normals."""
    t = o.modifiers.new("tri", "TRIANGULATE")
    t.keep_custom_normals = True
    t.quad_method = "BEAUTY"
    t.ngon_method = "BEAUTY"
    select_only(o)
    bpy.ops.object.modifier_apply(modifier="tri")

def write_rcm(o, base):
    """One .rcm per material slot, vertices deduplicated per corner."""
    me = o.data
    if not me.uv_layers and "UVMap" in me.attributes:
        import numpy as np
        src = me.attributes["UVMap"]
        dim = 3 if src.data_type == "FLOAT_VECTOR" else 2
        buf = np.empty(len(me.loops) * dim, np.float32)
        src.data.foreach_get("vector", buf)
        me.attributes.remove(src)              # its name is the UV layer's
        lay = me.uv_layers.new(name="UVMap")
        lay.data.foreach_set("uv", buf.reshape(-1, dim)[:, :2].ravel())
    uv = me.uv_layers.active
    if uv:
        me.calc_tangents()
    per = {}
    for poly in me.polygons:
        j = mat_index(o.material_slots[poly.material_index].material if o.material_slots else None)
        dst = per.setdefault(j, ({}, [], []))
        for li in poly.loop_indices:
            lp = me.loops[li]
            v = me.vertices[lp.vertex_index].co
            n = lp.normal
            t = lp.tangent if uv else Vector((1.0, 0.0, 0.0))
            u = uv.data[li].uv if uv else (0.0, 0.0)
            key = (round(v.x, 5), round(v.y, 5), round(v.z, 5), round(n.x, 3), round(n.y, 3), round(n.z, 3),
                   round(u[0], 5), round(u[1], 5), round(t.x, 3), round(t.y, 3), round(t.z, 3), lp.bitangent_sign)
            idx = dst[0].get(key)
            if idx is None:
                idx = dst[0][key] = len(dst[1])
                # the texture's v runs down, the file's up
                dst[1].append((v.x, v.y, v.z, n.x, n.y, n.z, u[0], 1.0 - u[1], t.x, t.y, t.z, lp.bitangent_sign))
            dst[2].append(idx)
    files = []
    for j, (_, V, I) in sorted(per.items()):
        path = f"{base}_m{j}.rcm"
        with open(path, "wb") as f:
            f.write(b"RCM1" + struct.pack("<II", len(V), len(I)))
            for v in V:
                f.write(struct.pack("<12f", *v))
            f.write(struct.pack(f"<{len(I)}I", *I))
        files.append({"mat": j, "file": os.path.basename(path), "tris": len(I) // 3})
    return files

# only the variants and their authored levels stay in the scene: the file's
# previews and helpers (geometry-node scatters, leaf-card sources) carry
# modifiers that every operation would re-evaluate (jacaranda_tree's crashed
# Blender inside one, 2026-09-30)
keep = set(srcs)
for s_ in srcs:
    for n in range(1, 6):
        a = bpy.data.objects.get(s_.name.replace("_LOD0", f"_LOD{n}"))
        if a is not None:
            keep.add(a)
for o_ in list(bpy.data.objects):
    if o_ not in keep:
        bpy.data.objects.remove(o_, do_unlink=True)
for o_ in keep:
    o_.modifiers.clear()
    # editor state (selection, sculpt masks and face sets) is not model data;
    # jacaranda_tree stores ".select_poly" and ".sculpt_face_set" on the
    # vertices instead of the faces, and converting that mesh for Decimate
    # read past their end and crashed Blender
    me_ = o_.data
    for a_ in [a.name for a in me_.attributes if a.name.startswith((".select_", ".sculpt_", ".uv_select_"))]:
        me_.attributes.remove(me_.attributes[a_])

report = {"asset": asset, "source": os.path.basename(src), "targets": targets, "variants": []}
for s in srcs:
    vname = variant_name(s)
    # world space: a variant carries its own placement in the file, which is not
    # ours; its origin is moved to the base of its trunk / the middle of its foot
    mw = s.matrix_world.copy()
    bb = [mw @ v.co for v in s.data.vertices]
    minz = min(p.z for p in bb)
    cx = sum(p.x for p in bb) / len(bb)
    cy = sum(p.y for p in bb) / len(bb)
    ent = {"name": vname, "src_tris": tris_of(s), "lods": []}
    # the model's own hand-made levels of this variant: <asset>_<variant>_LOD<n>
    authored = [s]
    for n in range(1, 6):
        a = bpy.data.objects.get(s.name.replace("_LOD0", f"_LOD{n}"))
        if a is not None and a is not s and a.type == "MESH":
            authored.append(a)
    # foliage (a cut-out leaf or twig material) is never decimated: Collapse
    # moves a leaf card's corners across its texture, so the card samples the
    # atlas's transparent parts and the leaves vanish (measured, 2026-09-30).
    # It takes the artist's own levels, finest to coarsest; the far level is
    # an impostor (remaster_env_impostor.py)
    def is_foliage(ob):
        for sl in ob.material_slots:
            if sl.material and mats[mat_index(sl.material)]["alpha_clip"]:
                return True
        return False
    foliage = is_foliage(s)
    levels = sorted(authored, key=tris_of, reverse=True)
    for k, tgt in enumerate(targets):
        if foliage:
            # level k: the finest artist level at or under the budget, else the coarsest
            fits = [a for a in levels if tris_of(a) <= tgt * 1.15]
            src_o = fits[0] if fits else levels[-1]
        else:
            # an authored level within the budget is used as it is (its own
            # normals too); otherwise the nearest finer one is Collapse-decimated
            # to the budget, normals smooth by angle (the car route; no
            # subdivision for an authored model, whose surface is already clean)
            fits = [a for a in authored if tris_of(a) <= tgt * 1.15]
            src_o = max(fits, key=tris_of) if fits else min(authored, key=tris_of)
        o = src_o.copy()
        o.data = src_o.data.copy()
        scene.collection.objects.link(o)
        o.modifiers.clear()
        o.matrix_world = mw
        if foliage or fits:
            route = f"authored {src_o.name}"
        else:
            standard(o, tgt)
            select_only(o)
            bpy.ops.object.shade_smooth_by_angle(angle=math.radians(CREASE))
            route = f"collapse from {src_o.name}"
        lb = [min(v.co.z for v in o.data.vertices), max(v.co.z for v in o.data.vertices)]
        print(f"ROUTE {asset} {vname} {k}: {route} (local z {lb[0]:.3f}..{lb[1]:.3f})", flush=True)
        # bake the placement into the mesh, then centre it
        o.data.transform(o.matrix_world)
        o.matrix_world.identity()
        o.data.transform(Matrix.Translation((-cx, -cy, -minz)))
        triangulate(o)
        files = write_rcm(o, os.path.join(out, f"{vname}_lod{k}"))
        ent["lods"].append({"tris": tris_of(o), "files": files})
        if k == 0:
            xs = [v.co for v in o.data.vertices]
            ent["bounds"] = [[min(p.x for p in xs), min(p.y for p in xs), min(p.z for p in xs)],
                             [max(p.x for p in xs), max(p.y for p in xs), max(p.z for p in xs)]]
        print(f"LOD {asset} {vname} {k}: {tris_of(o)} tris", flush=True)
        bpy.data.objects.remove(o)
    report["variants"].append(ent)
# ---- the textures: base (colour + cut-out alpha), orm (occlusion, roughness,
# metal) and normal (OpenGL), 8-bit RGBA PNG, at most TEX px -------------------
import numpy as np
TEX = int(os.environ.get("ENV_TEX", "2048"))
tex_out = os.path.join(os.path.dirname(os.path.abspath(out_root)), "textures", asset)
os.makedirs(tex_out, exist_ok=True)

def pixels(name, size):
    img = bpy.data.images[name].copy()
    img.colorspace_settings.name = "Non-Color"      # the bytes as stored, no transfer
    w, h = img.size
    if max(w, h) > size:
        img.scale(max(1, w * size // max(w, h)), max(1, h * size // max(w, h)))
    w, h = img.size
    a = np.empty(w * h * 4, np.float32)
    img.pixels.foreach_get(a)
    bpy.data.images.remove(img)
    return a.reshape(h, w, 4)

def save(arr, path):
    h, w = arr.shape[:2]
    img = bpy.data.images.new("out", w, h, alpha=True)
    img.colorspace_settings.name = "Non-Color"
    img.pixels.foreach_set(np.clip(arr, 0, 1).astype(np.float32).ravel())
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    bpy.data.images.remove(img)

for j, e in enumerate(mats):
    im = e["images"]
    if "base" not in im:
        continue
    b = pixels(im["base"], TEX)
    h, w = b.shape[:2]
    def fit(role, ch=0, default=1.0):
        if role not in im:
            return np.full((h, w), default, np.float32)
        a = pixels(im[role], TEX)
        if a.shape[:2] != (h, w):
            img = bpy.data.images.new("tmp", a.shape[1], a.shape[0], alpha=True)
            img.pixels.foreach_set(a.ravel()); img.scale(w, h)
            a = np.empty(w * h * 4, np.float32); img.pixels.foreach_get(a); a = a.reshape(h, w, 4)
            bpy.data.images.remove(img)
        return a[:, :, ch]
    base = b.copy()
    if "alpha" in im:
        base[:, :, 3] = fit("alpha", 0, 1.0)
    elif "alpha_from" in im:
        base[:, :, 3] = fit("alpha_from", 3, 1.0)       # the image's own alpha channel
    else:
        base[:, :, 3] = 1.0
    if "gltf_mr" in im:
        orm = np.dstack([np.ones((h, w), np.float32), fit("gltf_mr", 1, 0.8), fit("gltf_mr", 2, 0.0), np.ones((h, w), np.float32)])
    elif "arm" in im:
        orm = np.dstack([fit("arm", 0), fit("arm", 1), fit("arm", 2), np.ones((h, w), np.float32)])
    else:
        orm = np.dstack([fit("ao", 0, 1.0), fit("rough", 0, 0.8), fit("metal", 0, 0.0), np.ones((h, w), np.float32)])
    save(base, os.path.join(tex_out, f"m{j}_base.png"))
    save(orm, os.path.join(tex_out, f"m{j}_orm.png"))
    if "normal" in im:
        nrm = pixels(im["normal"], TEX); nrm[:, :, 3] = 1
        save(nrm, os.path.join(tex_out, f"m{j}_nrm.png"))
    e["textures"] = {"base": f"m{j}_base.png", "orm": f"m{j}_orm.png"} | ({"nrm": f"m{j}_nrm.png"} if "normal" in im else {})
    print(f"TEX {asset} m{j} {e['name']} {w}x{h} {sorted(im)}", flush=True)
report["materials"] = mats
with open(os.path.join(out, "bake.json"), "w") as f:
    json.dump(report, f, indent=1)
print(f"BAKE_OK {asset} {len(report['variants'])} variants {len(mats)} materials", flush=True)
