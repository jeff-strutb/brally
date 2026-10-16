"""menuicon.py: a menu icon for Top Gear Rally's front end, made from a high-poly
source model, in the game's own icon format (tgr/model.h: BrModel, its parts
one display list each), for the Remaster Options rows of the options screen
(ports/remastered/tgrally/src/menus/rm_options.c).

Run by Blender, one source at a time (each is large).  SRC is a .blend (its first mesh,
textured) or STL parts, comma-separated (one colour, MENUICON_COLOUR):

    blender -b -P ports/remastered/tools/menuicon.py -- SRC.blend NAME OUT.bin [TRIS] [HEIGHT|WIDTH=N] [PREVIEW]

(PREVIEW: renders of the result, PREVIEW_front.png, _side.png, _three.png.)

The game's icons are a few hundred triangles, smooth shaded, each part one
flat colour lit by the menu's two lights.  So the source is brought down to
that, the way the remastered cars are (Blender's Collapse decimate only):

  1. Collapse to 40k triangles, Subdivision Surface level 2 applied (a
     generated model's surface is lumpy: subdividing it first is what smooths
     it), Collapse again to a middle mesh (8k)
  2. each middle triangle's colour: the source's base-colour texture read
     where four points on it fall on the full-resolution mesh (its nearest
     face, that face's UVs), averaged (linear light)
  3. the colours quantized: k-means in Oklab, weighted by area, into the
     fewest (at most six) within about a just-noticeable step; a triangle its
     neighbours mostly disagree with takes theirs, and a colour under 0.4% of
     the surface goes to its neighbours, so no speckles; each colour one part.
     Then the last Collapse, to TRIS (default 700), keeping the vertices on
     the borders between colours (so no triangle straddles one), and smooth
     by angle (40 degrees); the normals then taken from a level-2 subdivided
     copy of it (a Data Transfer, as the cars' route), so it shades smooth
  4. sized as the game's icons are (centred, the source's own axes: z up,
     floating 250 units over the floor as theirs do), HEIGHT model units tall
     or WIDTH across

The output is the model as BrModelLoad leaves it before rebasing: big-endian,
every address an offset from the start (BrModelRebase adds the base).  Each
part: a 4-texel RGBA16 texture of its colour loaded as the game's own
one-colour parts load theirs, then its vertices (normals in the colour bytes,
for the lighting) in loads of up to 32 with F3DEX 1 TRI2 commands.  Last, the
shadow the game's icons cast on the ring: the outline from above at z 0, blended in the
second pass, with the game's own commands for it. """
import bmesh
import bpy
import math
import struct
import sys

import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

argv = sys.argv[sys.argv.index('--') + 1:]
SRC, NAME, OUT = argv[0], argv[1], argv[2]
TRIS = int(argv[3]) if len(argv) > 3 else 2000
SIZE = argv[4] if len(argv) > 4 else 'HEIGHT=1700'
KMAX = 6
import os
SUBSURF = int(os.environ.get('MENUICON_SUBSURF', '2'))      # 0 for a hard-surface source
COLOURFIRST = int(os.environ.get('MENUICON_COLOURFIRST', '1'))  # 0: decimate first, colour after
PROTECT = int(os.environ.get('MENUICON_PROTECT', '0'))      # keep the colours' borders in the last collapse
MID = int(os.environ.get('MENUICON_MID', '8000'))                                          # the middle mesh the colours are found on
KERR = 0.0012                                        # (Oklab: about a just-noticeable step)
MINAREA = 0.004


def log(*a):
    print('menuicon:', *a, flush=True)


if SRC.lower().endswith('.blend'):
    bpy.ops.wm.open_mainfile(filepath=SRC)
else:
    # STL parts (comma-separated, one model in pieces, as a printable model comes), joined;
    # MENUICON_YUP=1 turns a Y-up model Z-up.  No texture: one colour (MENUICON_COLOUR, sRGB)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for f in SRC.split(','):
        bpy.ops.wm.stl_import(filepath=f)
    parts_ = [o for o in bpy.data.objects if o.type == 'MESH']
    # MENUICON_ALIGN=z (an axis of the files): each part moved so its least on that axis is the
    # first part's (a print's halves laid out apart)
    ax_ = os.environ.get('MENUICON_ALIGN')
    if ax_:
        k_ = 'xyz'.index(ax_)
        lo_ = [min(v.co[k_] for v in o.data.vertices) for o in parts_]
        for o, l in zip(parts_, lo_):
            for v in o.data.vertices:
                v.co[k_] += lo_[0] - l
    bpy.context.view_layer.objects.active = parts_[0]
    for o in parts_:
        o.select_set(True)
    bpy.ops.object.join()
    if os.environ.get('MENUICON_YUP'):
        bpy.context.object.rotation_euler = (math.radians(90), 0, 0)
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
src = next(o for o in bpy.data.objects if o.type == 'MESH')
for o in list(bpy.data.objects):
    if o is not src:
        bpy.data.objects.remove(o)
bpy.context.view_layer.objects.active = src
src.select_set(True)

# ---- fitted to a textured model --------------------------------------------------------------
# MENUICON_FIT=X.blend: the shape stretched to that model's box (its proportions), turned the
# quarter-turn about z that lies closest on it, and its colours read from that model's texture
csrc = src
FIT = os.environ.get('MENUICON_FIT')
if FIT:
    with bpy.data.libraries.load(FIT) as (df_, dt_):
        dt_.objects = df_.objects
    fit = next(o for o in dt_.objects if o and o.type == 'MESH')
    bpy.context.scene.collection.objects.link(fit)
    mco = np.empty(len(fit.data.vertices) * 3)
    fit.data.vertices.foreach_get('co', mco)
    mco = mco.reshape(-1, 3)
    mmn, mmx = mco.min(0), mco.max(0)
    fit.data.calc_loop_triangles()
    ft_ = np.empty(len(fit.data.loop_triangles) * 3, dtype=np.int64)
    fit.data.loop_triangles.foreach_get('vertices', ft_)
    fbvh = BVHTree.FromPolygons([tuple(v) for v in mco], [tuple(t) for t in ft_.reshape(-1, 3)], all_triangles=True)
    sco = np.empty(len(src.data.vertices) * 3)
    src.data.vertices.foreach_get('co', sco)
    sco = sco.reshape(-1, 3)
    sc_ = (sco.min(0) + sco.max(0)) / 2
    best_ = None
    pick = np.random.default_rng(3).choice(len(sco), min(3000, len(sco)), replace=False)
    for q in range(4):
        a_ = math.radians(90 * q)
        R = np.array([[math.cos(a_), -math.sin(a_), 0], [math.sin(a_), math.cos(a_), 0], [0, 0, 1]])
        r_ = (sco - sc_) @ R.T
        r_ = (r_ - r_.min(0)) / (r_.max(0) - r_.min(0)) * (mmx - mmn) + mmn
        d_ = np.mean([fbvh.find_nearest(Vector(r_[i].tolist()))[3] for i in pick])
        log('fit: quarter-turn', q, 'mean distance', round(float(d_), 4))
        if best_ is None or d_ < best_[0]:
            best_ = (d_, r_)
    src.data.vertices.foreach_set('co', best_[1].reshape(-1))
    src.data.update()
    if os.environ.get('MENUICON_FIT_COLOURS', '1') != '0':
        csrc = fit
    log('fitted to', os.path.basename(FIT), 'box', (mmx - mmn).round(3).tolist())

# ---- the base colour texture -------------------------------------------------------------
FLAT = None
if not csrc.data.materials or not csrc.data.uv_layers:
    c_ = [float(x) for x in os.environ.get('MENUICON_COLOUR', '0.16,0.16,0.17').split(',')]
    FLAT = np.array([x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c_])
mat = csrc.data.materials[0] if csrc.data.materials else None
img = None
if FLAT is not None:
    mat = None
for n in (mat.node_tree.nodes if mat else []):
    if n.type == 'BSDF_PRINCIPLED':
        for l in n.inputs['Base Color'].links:
            if l.from_node.type == 'TEX_IMAGE':
                img = l.from_node.image
if FLAT is None:
    img = img or next(n.image for n in mat.node_tree.nodes if n.type == 'TEX_IMAGE' and n.image)
    W, H = img.size
    pix = np.empty(W * H * 4, dtype=np.float32)
    img.pixels.foreach_get(pix)
    pix = pix.reshape(H, W, 4)[:, :, :3]
    if img.colorspace_settings.name == 'sRGB':           # (pixels are the file's values: to linear)
        pix = np.where(pix <= 0.04045, pix / 12.92, ((pix + 0.055) / 1.055) ** 2.4)
    log('texture', img.name, W, H, img.colorspace_settings.name)
else:
    log('no texture: one colour', FLAT.round(3).tolist())

# ---- the full-resolution mesh, for the colours ------------------------------------------------
me0 = csrc.data
me0.calc_loop_triangles()
nt = len(me0.loop_triangles)
co0 = np.empty(len(me0.vertices) * 3, dtype=np.float64)
me0.vertices.foreach_get('co', co0)
co0 = co0.reshape(-1, 3)
tv0 = np.empty(nt * 3, dtype=np.int64)
me0.loop_triangles.foreach_get('vertices', tv0)
tv0 = tv0.reshape(-1, 3)
tl0 = np.empty(nt * 3, dtype=np.int64)
me0.loop_triangles.foreach_get('loops', tl0)
tl0 = tl0.reshape(-1, 3)
uv0 = np.zeros((len(me0.loops), 2))
if FLAT is None:
    me0.uv_layers.active.data.foreach_get('uv', uv0.reshape(-1))
bvh = BVHTree.FromPolygons([tuple(v) for v in co0], [tuple(t) for t in tv0], all_triangles=True)
log('source', len(me0.vertices), 'vertices', nt, 'triangles')


def colour_at(p):
    if FLAT is not None:
        return FLAT
    loc, _n, ti, _d = bvh.find_nearest(p)
    if ti is None:
        return None
    a, b, c = (co0[i] for i in tv0[ti])
    v0, v1, v2 = b - a, c - a, np.array(loc) - a
    d00, d01, d11 = v0 @ v0, v0 @ v1, v1 @ v1
    d20, d21 = v2 @ v0, v2 @ v1
    den = d00 * d11 - d01 * d01
    if abs(den) < 1e-30:
        w1 = w2 = 1 / 3
    else:
        w1 = (d11 * d20 - d01 * d21) / den
        w2 = (d00 * d21 - d01 * d20) / den
    w1, w2 = min(max(w1, 0), 1), min(max(w2, 0), 1)
    w0 = max(1 - w1 - w2, 0)
    uv = w0 * uv0[tl0[ti][0]] + w1 * uv0[tl0[ti][1]] + w2 * uv0[tl0[ti][2]]
    x = int((uv[0] % 1.0) * (W - 1) + 0.5)
    y = int((uv[1] % 1.0) * (H - 1) + 0.5)
    return pix[y, x]


# ---- 1. decimate to the middle: collapse, subdivide, collapse ----------------------------------------
lo = bpy.data.objects.new(NAME, src.data.copy())
bpy.context.scene.collection.objects.link(lo)
bpy.context.view_layer.objects.active = lo
for o in bpy.context.selected_objects:
    o.select_set(False)
lo.select_set(True)


def ntris():
    return sum(len(p.vertices) - 2 for p in lo.data.polygons)


def collapse(target, group=None):
    n = ntris()
    if n <= target:
        return
    m = lo.modifiers.new('collapse', 'DECIMATE')
    m.decimate_type = 'COLLAPSE'
    m.ratio = target / n
    if group:
        m.vertex_group = group
        m.invert_vertex_group = True                 # (its vertices kept: the colours' borders)
        m.vertex_group_factor = 1.0
    bpy.ops.object.modifier_apply(modifier=m.name)
    log('collapse', n, '->', ntris(), 'for', target)
    if os.environ.get('MENUICON_DEBUG'):
        import bmesh as _bm
        bm = _bm.new(); bm.from_mesh(lo.data)
        keys = {}
        for f in bm.faces:
            keys.setdefault(tuple(sorted(v.index for v in f.verts)), []).append(f.index)
        log('debug: duplicate faces', sum(len(v) - 1 for v in keys.values()), 'zero-area',
            sum(1 for f in bm.faces if f.calc_area() < 1e-12), 'non-manifold edges',
            sum(1 for e in bm.edges if not e.is_manifold), 'wire', sum(1 for e in bm.edges if e.is_wire),
            'verts', len(bm.verts), 'valence>12', sum(1 for v in bm.verts if len(v.link_edges) > 12))
        bm.free()


def triangulate():
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.quads_convert_to_tris()
    bpy.ops.object.mode_set(mode='OBJECT')


# the low mesh never uses UVs or the source's material: let go, else the collapse keeps every
# UV seam (a generated model has hundreds of islands) and stalls short of its target
while lo.data.uv_layers:
    lo.data.uv_layers.remove(lo.data.uv_layers[0])
lo.data.materials.clear()
# the source is split along its UV seams: welded first (the colours come from the full-resolution
# mesh, never from the low one's UVs), else the collapse cannot cross a seam and stalls or tears
bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.mesh.remove_doubles(threshold=max(lo.dimensions) * 2e-5)
bpy.ops.object.mode_set(mode='OBJECT')
log('welded to', len(lo.data.vertices), 'vertices')
EXTERIOR = int(os.environ.get('MENUICON_EXTERIOR', '0'))   # keep only the faces seen from outside
if EXTERIOR:
    # a generated model is full of surfaces no one sees (hollow walls, inner structure); the
    # collapse folds the outside into them.  At 200k triangles, a face is kept when a ray from
    # it (its normal, or one of 40 directions within 80 degrees of it) leaves the model; the
    # openings left are filled flat
    collapse(200000)
    me_ = lo.data
    nf_ = len(me_.polygons)
    cen_ = np.empty(nf_ * 3); me_.polygons.foreach_get('center', cen_); cen_ = cen_.reshape(-1, 3)
    nor_ = np.empty(nf_ * 3); me_.polygons.foreach_get('normal', nor_); nor_ = nor_.reshape(-1, 3)
    bvh_ = BVHTree.FromObject(lo, bpy.context.evaluated_depsgraph_get())
    dirs_ = np.random.default_rng(2).normal(size=(160, 3))
    dirs_ /= np.linalg.norm(dirs_, axis=1)[:, None]
    eps_ = max(lo.dimensions) * 1e-4
    seen_ = np.zeros(nf_, bool)
    for i in range(nf_):
        nv = nor_[i]
        p0 = Vector((cen_[i] + nv * eps_).tolist())
        if bvh_.ray_cast(p0, Vector(nv.tolist()))[0] is None:
            seen_[i] = True
            continue
        k = 0
        for d in dirs_:
            if d @ nv < 0.17:
                continue
            if bvh_.ray_cast(p0, Vector(d.tolist()))[0] is None:
                seen_[i] = True
                break
            k += 1
            if k >= 40:
                break
    log('seen from outside:', int(seen_.sum()), 'of', nf_, 'faces')
    bm = bmesh.new()
    bm.from_mesh(me_)
    bm.faces.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[bm.faces[i] for i in range(nf_) if not seen_[i]], context='FACES')
    loose = [v for v in bm.verts if not v.link_faces]
    bmesh.ops.delete(bm, geom=loose, context='VERTS')
    bm.to_mesh(me_)
    bm.free()
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.fill_holes(sides=0)
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.quads_convert_to_tris(quad_method='BEAUTY', ngon_method='BEAUTY')
    bpy.ops.object.mode_set(mode='OBJECT')
    log('outside kept and closed:', len(lo.data.polygons), 'triangles')
PRESMOOTH = int(os.environ.get('MENUICON_PRESMOOTH', '0'))   # Laplacian passes on the full source first
if PRESMOOTH:
    m = lo.modifiers.new('presmooth', 'LAPLACIANSMOOTH')
    m.iterations = PRESMOOTH
    m.lambda_factor = float(os.environ.get('MENUICON_LAMBDA', '1.0'))
    m.use_volume_preserve = True
    m.use_normalized = True
    bpy.ops.object.modifier_apply(modifier=m.name)
    log('source smoothed,', PRESMOOTH, 'passes')
VOXEL = float(os.environ.get('MENUICON_VOXEL', '0'))       # voxel remesh, a fraction of the size (0: none)
if VOXEL:
    # closed and turned outward first (a voxel remesh reads inside from outside by the
    # faces' winding: an open or inconsistently wound surface comes out a thin shell)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_mode(type='EDGE')
    bpy.ops.mesh.select_all(action='DESELECT')
    bpy.ops.mesh.select_non_manifold(extend=False, use_wire=True, use_boundary=False, use_multi_face=True,
                                     use_non_contiguous=True, use_verts=True)
    bpy.ops.mesh.delete(type='FACE')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.fill_holes(sides=0)
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode='OBJECT')
    m = lo.modifiers.new('remesh', 'REMESH')
    m.mode = 'VOXEL'
    m.voxel_size = max(lo.dimensions) * VOXEL
    bpy.ops.object.modifier_apply(modifier=m.name)
    triangulate()
    log('voxel remesh', len(lo.data.vertices), 'vertices')
    # a hollow source (thin walls, its inside showing through vents and slots) remeshes as
    # an outer surface and an inner one: only the outermost (the largest box) is kept
    bm = bmesh.new()
    bm.from_mesh(lo.data)
    bm.verts.ensure_lookup_table()
    comp, seen = [], set()
    for v in bm.verts:
        if v.index in seen:
            continue
        stack, cur = [v], []
        seen.add(v.index)
        while stack:
            x = stack.pop()
            cur.append(x)
            for e in x.link_edges:
                y = e.other_vert(x)
                if y.index not in seen:
                    seen.add(y.index)
                    stack.append(y)
        co = np.array([x.co[:] for x in cur])
        comp.append((float(np.prod(co.max(0) - co.min(0))), cur))
    comp.sort(key=lambda c: -c[0])
    keep = {x.index for x in comp[0][1]}
    if os.environ.get('MENUICON_KEEPFEET'):           # (other solid pieces outside the body's box)
        lo0, hi0 = np.array([x.co[:] for x in comp[0][1]]).min(0), np.array([x.co[:] for x in comp[0][1]]).max(0)
        for vol, cur in comp[1:]:
            c = np.array([x.co[:] for x in cur]).mean(0)
            if (c < lo0).any() or (c > hi0).any():
                keep |= {x.index for x in cur}
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.index not in keep], context='VERTS')
    bm.to_mesh(lo.data)
    bm.free()
    log('kept the outer surface:', len(lo.data.vertices), 'vertices of', len(comp), 'pieces')
    collapse(40000)
collapse(40000)
if SUBSURF:
    m = lo.modifiers.new('subsurf', 'SUBSURF')
    m.levels = m.render_levels = SUBSURF
    bpy.ops.object.modifier_apply(modifier=m.name)
    triangulate()
SMOOTH = int(os.environ.get('MENUICON_SMOOTH', '0'))       # Laplacian smoothing passes before the middle mesh
if SMOOTH:
    m = lo.modifiers.new('smooth', 'LAPLACIANSMOOTH')
    m.iterations = SMOOTH
    m.lambda_factor = float(os.environ.get('MENUICON_LAMBDA', '1.0'))
    m.use_volume_preserve = True
    m.use_normalized = True
    bpy.ops.object.modifier_apply(modifier=m.name)
    log('laplacian smooth', SMOOTH, 'passes')
QUADS = int(os.environ.get('MENUICON_QUADRIFLOW', '0'))    # a Quadriflow remesh to this many faces instead
if QUADS:
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.fill_holes(sides=0)
    bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.object.quadriflow_remesh(target_faces=QUADS, use_preserve_sharp=False, use_mesh_symmetry=False,
                                     use_preserve_boundary=False, seed=1)
    triangulate()
    log('quadriflow', len(lo.data.polygons), 'triangles')
else:
    collapse(MID)
    triangulate()
me = lo.data
log('middle', len(me.polygons), 'triangles')
NORMALS = int(os.environ.get('MENUICON_NORMALS', '0'))   # the final mesh's normals from a subdivided copy

# ---- colour space ----------------------------------------------------------------------------------
def to_oklab(c):
    l = 0.4122214708 * c[:, 0] + 0.5363325363 * c[:, 1] + 0.0514459929 * c[:, 2]
    m_ = 0.2119034982 * c[:, 0] + 0.6806995451 * c[:, 1] + 0.1073969566 * c[:, 2]
    s = 0.0883024619 * c[:, 0] + 0.2817188376 * c[:, 1] + 0.6299787005 * c[:, 2]
    l, m_, s = np.cbrt(l), np.cbrt(m_), np.cbrt(s)
    return np.stack([0.2104542553 * l + 0.7936177850 * m_ - 0.0040720468 * s,
                     1.9779984951 * l - 2.4285922050 * m_ + 0.4505937099 * s,
                     0.0259040371 * l + 0.7827717662 * m_ - 0.8086757660 * s], 1)


# ---- 2, 3. each triangle's colour, and the colours quantized ----------------------------------------
BARY = [(1 / 3, 1 / 3), (0.6, 0.2), (0.2, 0.6), (0.2, 0.2), (0.45, 0.45), (0.45, 0.1), (0.1, 0.45)]


def colourize(me, nb):
    nf = len(me.polygons)
    cols = np.zeros((nf, 3))
    area = np.zeros(nf)
    for i, f in enumerate(me.polygons):
        a, b, c = (me.vertices[v].co for v in f.vertices[:3])
        area[i] = f.area
        acc, k = np.zeros(3), 0
        for (u, v) in BARY[:nb]:
            s_ = colour_at(a + (b - a) * u + (c - a) * v)
            if s_ is not None:
                acc += s_
                k += 1
        cols[i] = acc / max(k, 1)

    lab = to_oklab(np.clip(cols, 0, None))
    rng = np.random.default_rng(1)


    def kmeans(k):
        best = None
        for _try in range(4):
            cent = lab[rng.choice(len(lab), k, replace=False, p=area / area.sum())]
            for _ in range(80):
                lbl = np.argmin(((lab[:, None, :] - cent[None]) ** 2).sum(2), 1)
                new = np.array([np.average(lab[lbl == j], 0, area[lbl == j]) if (lbl == j).any() else cent[j]
                                for j in range(k)])
                if np.allclose(new, cent, atol=1e-7):
                    break
                cent = new
            err = np.average(((lab - cent[lbl]) ** 2).sum(1), weights=area)
            if best is None or err < best[1]:
                best = (lbl, err)
        return best


    for k in range(1, KMAX + 1):
        lbl, err = kmeans(k)
        log('k', k, 'error', round(float(err), 5))
        if err < KERR:
            break
    # no speckles: a triangle whose neighbours mostly (by area) differ takes theirs, and a colour
    # that covers under MINAREA of the model goes to its neighbours
    edge_f = {}
    for i, f in enumerate(me.polygons):
        for e in f.edge_keys:
            edge_f.setdefault(e, []).append(i)
    nbr = [[] for _ in range(nf)]
    for fs in edge_f.values():
        for a_ in fs:
            for b_ in fs:
                if a_ != b_:
                    nbr[a_].append(b_)
    for it in range(10):
        tot = {j: area[lbl == j].sum() for j in set(lbl.tolist())}
        small = {j for j, t in tot.items() if t < MINAREA * area.sum()}
        new = lbl.copy()
        for i in range(nf):
            w = {}
            for j in nbr[i]:
                if lbl[j] not in small:
                    w[lbl[j]] = w.get(lbl[j], 0) + area[j]
            if not w:
                continue
            top = max(w, key=w.get)
            if lbl[i] in small or (top != lbl[i] and w.get(lbl[i], 0) < 0.45 * sum(w.values())):
                new[i] = top
        lbl = new
    parts = sorted(set(lbl.tolist()))
    pcol = {j: np.average(cols[lbl == j], 0, area[lbl == j]) for j in parts}
    log('colours', len(parts), [round(float(area[lbl == j].sum() / area.sum()), 3) for j in parts])
    return lbl, edge_f, parts, pcol


def paint(me, lbl, parts, pcol):
    # each colour a material (the source's own let go)
    me.materials.clear()
    for j in parts:
        mt = bpy.data.materials.new(f'c{j}')
        mt.diffuse_color = (*pcol[j], 1)
        me.materials.append(mt)
    slot = {j: k for k, j in enumerate(parts)}
    me.polygons.foreach_set('material_index', [slot[j] for j in lbl.tolist()])


DENSE = int(os.environ.get('MENUICON_DENSE', '0'))         # simple subdivision levels before colouring
if DENSE:
    # a model of large flat faces (CAD) split finely first, its shape unchanged, so the
    # colours' borders can follow the texture rather than its few big triangles
    m = lo.modifiers.new('dense', 'SUBSURF')
    m.subdivision_type = 'SIMPLE'
    m.levels = m.render_levels = DENSE
    bpy.ops.object.modifier_apply(modifier=m.name)
    triangulate()
    me = lo.data
    log('split to', len(me.polygons), 'triangles for the colours')
if COLOURFIRST:
    # coloured on the middle mesh; the last collapse keeps the colours' borders when PROTECT
    lbl, edge_f, parts, pcol = colourize(me, 4)
    paint(me, lbl, parts, pcol)
    border = set()
    for (va, vb), fs in edge_f.items():
        if len({lbl[f] for f in fs}) > 1:
            border.update((va, vb))
    vg = lo.vertex_groups.new(name='border')
    vg.add(list(border), 1.0, 'REPLACE')
    collapse(TRIS, 'border' if PROTECT else None)
    triangulate()
else:
    # the shape first, then the final triangles coloured (for a source whose colours are many
    # small regions: their borders would stop the collapse)
    collapse(TRIS)
    triangulate()
    me = lo.data
    lbl, edge_f, parts, pcol = colourize(me, 7)
    paint(me, lbl, parts, pcol)

# every face turned outward (the source's winding is not consistent: a flipped face is lit from
# behind, culled in the game, and reads as a 180-degree crease)
flipped = sum(1 for f in lo.data.polygons)
bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.mesh.normals_make_consistent(inside=False)
bpy.ops.object.mode_set(mode='OBJECT')
if lo.data.has_custom_normals:                   # (the source's own, meaningless once decimated)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.customdata_custom_splitnormals_clear()
    bpy.ops.object.mode_set(mode='OBJECT')
bpy.ops.object.shade_smooth_by_angle(angle=math.radians(40))
if NORMALS:
    # as the cars' route: a copy of the final mesh subdivided (level 2, its creases kept by the
    # same angle) and its normals taken onto the final one, which then shades as that smooth
    # surface rather than by its own facets
    ref = bpy.data.objects.new(NAME + '_ref', lo.data.copy())
    bpy.context.scene.collection.objects.link(ref)
    sm = ref.modifiers.new('subsurf', 'SUBSURF')
    sm.levels = sm.render_levels = 2
    bpy.context.view_layer.objects.active = ref
    for o in bpy.context.selected_objects:
        o.select_set(False)
    ref.select_set(True)
    bpy.ops.object.modifier_apply(modifier=sm.name)
    bpy.ops.object.shade_smooth_by_angle(angle=math.radians(40))
    ref.select_set(False)
    bpy.context.view_layer.objects.active = lo
    lo.select_set(True)
    m = lo.modifiers.new('normals', 'DATA_TRANSFER')
    m.object = ref
    m.use_loop_data = True
    m.data_types_loops = {'CUSTOM_NORMAL'}
    m.loop_mapping = 'POLYINTERP_NEAREST'
    bpy.ops.object.modifier_apply(modifier=m.name)
    bpy.data.objects.remove(ref)
me = lo.data
log('smooth', sum(p.use_smooth for p in me.polygons), 'of', len(me.polygons), 'faces; custom normals',
    me.has_custom_normals, '; sharp edges', sum(1 for e in me.edges if e.use_edge_sharp))
me.calc_loop_triangles()
tris = list(me.loop_triangles)
tri_part = [parts[me.polygons[t.polygon_index].material_index] for t in tris]
log('decimated to', len(tris), 'triangles,', len(me.vertices), 'vertices')

# MENUICON_GREY=r,g,b MENUICON_GREY_BOXES=x0,x1,y0,y1,z0,z1[,oval];...: the triangles wholly in
# one of the boxes (fractions of the model's box) in that colour: parts a model without a
# texture shows in another colour (a console's sockets and slot), measured off its own renders
GREY = os.environ.get('MENUICON_GREY')
if GREY:
    vco_ = np.array([v.co[:] for v in me.vertices])
    cmn, cmx = vco_.min(0), vco_.max(0)
    boxes = [[float(x) for x in b_.split(',')] for b_ in os.environ['MENUICON_GREY_BOXES'].split(';')]
    grey = max(parts) + 1
    nc = 0
    def inside(f, b_):
        # a box; with a seventh value 1, the oval inscribed across x and z (a round socket)
        if not (b_[2] <= f[1] <= b_[3]):
            return False
        if len(b_) > 6 and b_[6]:
            cx, cz = (b_[0] + b_[1]) / 2, (b_[4] + b_[5]) / 2
            rx, rz = (b_[1] - b_[0]) / 2, (b_[5] - b_[4]) / 2
            return ((f[0] - cx) / rx) ** 2 + ((f[2] - cz) / rz) ** 2 <= 1.0
        return b_[0] <= f[0] <= b_[1] and b_[4] <= f[2] <= b_[5]
    for i, t in enumerate(tris):
        # every corner in the same region (a long triangle reaching out of it stays as it is)
        fs = [(np.array(me.vertices[v].co[:]) - cmn) / (cmx - cmn) for v in t.vertices]
        if any(all(inside(f, b_) for f in fs) for b_ in boxes):
            tri_part[i] = grey
            nc += 1
    # an oval's socket faced: a solid flat oval of that colour filling the opening, flush with the
    # panel and a little larger than the hole (no dark ring), facing out the front
    ncap = 0
    bm = bmesh.new()
    bm.from_mesh(me)
    first_new = len(me.polygons)
    for b_ in boxes:
        if not (len(b_) > 6 and b_[6]):
            continue
        cx, cz = (b_[0] + b_[1]) / 2, (b_[4] + b_[5]) / 2
        rx, rz = (b_[1] - b_[0]) / 2, (b_[5] - b_[4]) / 2
        ring = []
        for v in vco_:
            f = (v - cmn) / (cmx - cmn)
            e = ((f[0] - cx) / rx) ** 2 + ((f[2] - cz) / rz) ** 2
            if 1.05 <= e <= 1.6 and f[1] <= b_[3]:
                ring.append(v[1])
        if not ring:                             # (no rim found: the panel's front there)
            ring = [v[1] for v in vco_ if b_[0] - rx <= (v[0] - cmn[0]) / (cmx[0] - cmn[0]) <= b_[1] + rx
                    and b_[4] - rz <= (v[2] - cmn[2]) / (cmx[2] - cmn[2]) <= b_[5] + rz]
        y = float(np.min(ring)) + 0.004 * (cmx[1] - cmn[1])   # flush, just behind the panel's face
        log('socket face at x %.3f z %.3f, depth %.3f of the box' % (cx, cz, (y - cmn[1]) / (cmx[1] - cmn[1])))
        def at(fx, fz):
            return (cmn[0] + fx * (cmx[0] - cmn[0]), y, cmn[2] + fz * (cmx[2] - cmn[2]))
        c = bm.verts.new(at(cx, cz))
        rim_v = [bm.verts.new(at(cx + 1.06 * rx * math.cos(a), cz + 1.06 * rz * math.sin(a)))
                 for a in np.linspace(0, 2 * math.pi, 17)[:-1]]
        for k in range(16):
            bm.faces.new((c, rim_v[(k + 1) % 16], rim_v[k]))   # facing -y, the front
        ncap += 16
    bm.to_mesh(me)
    bm.free()
    me.update()
    me.calc_loop_triangles()
    tris = list(me.loop_triangles)
    sock = grey + 1                              # the socket faces: their own colour (MENUICON_SOCKET)
    tri_part = [tri_part[t.polygon_index] if t.polygon_index < first_new else sock for t in tris] \
        if len(tris) == len(tri_part) + ncap else tri_part + [sock] * ncap
    srgb_lin = lambda cs: np.array([x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4
                                    for x in (float(y) for y in cs.split(','))])
    pcol[grey] = srgb_lin(GREY)
    pcol[sock] = srgb_lin(os.environ.get('MENUICON_SOCKET', GREY))
    parts = parts + [grey] + ([sock] if ncap else [])
    # shaded as the slot is, facing the menu's light from above: the same grey to the eye,
    # though they face the front
    SOCK_LIT_UP = os.environ.get('MENUICON_SOCKET_LIT_UP', '1') != '0'
    log('grey parts:', nc, 'triangles, and', ncap, 'in socket faces')

# ---- 4. size ------------------------------------------------------------------------------------
vco = np.array([v.co[:] for v in me.vertices])
mn, mx = vco.min(0), vco.max(0)
key, val = SIZE.split('=')
scale = float(val) / ((mx[2] - mn[2]) if key == 'HEIGHT' else max(mx[0] - mn[0], mx[1] - mn[1]))
HOVER = float(os.environ.get('MENUICON_HOVER', '250'))   # the game's icons float this far over their shadow
off = np.array([-(mn[0] + mx[0]) / 2, -(mn[1] + mx[1]) / 2, -mn[2] + HOVER / scale])
log('scale', round(scale, 2), 'size', ((mx - mn) * scale).round(0).tolist())

# ---- the model ------------------------------------------------------------------------------------
cn = me.corner_normals


def srgb5(c):
    c = np.clip(c, 0, 1)
    s = np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - 0.055)
    return [int(round(x * 31)) for x in s]


def morton(p):
    q = [int(min(max((p[a] - mn[a]) / max(mx[a] - mn[a], 1e-9), 0), 1) * 1023) for a in range(3)]
    r = 0
    for bit in range(10):
        for a in range(3):
            r |= ((q[a] >> bit) & 1) << (bit * 3 + a)
    return r


LIT_UP = {sock} if GREY and ncap and SOCK_LIT_UP else set()   # parts shaded as though facing up (the socket faces)


def corner_keys(i):
    t = tris[i]
    out = []
    for k in range(3):
        n = cn[t.loops[k]].vector
        if tri_part[i] in LIT_UP:
            out.append((t.vertices[k], 0.0, 0.0, 1.0))
        else:
            out.append((t.vertices[k], round(n.x, 3), round(n.y, 3), round(n.z, 3)))
    return out


def make_loads(ts):
    # loads of up to 32 vertices, each grown as a patch: from a seed, the neighbouring
    # triangle that adds the fewest new vertices, until the next would not fit
    keys = {i: corner_keys(i) for i in ts}
    by_v = {}
    for i in ts:
        for kk in keys[i]:
            by_v.setdefault(kk, []).append(i)
    left = set(ts)
    order = sorted(ts, key=lambda i: morton(tris[i].center))
    loads = []
    while left:
        seed = next(i for i in order if i in left)
        cur, idx, tl = [], {}, []
        cand = {seed}
        while True:
            if not cand:                             # (a patch closed in: the next nearest)
                rest = [i for i in order if i in left]
                if not rest:
                    break
                cand = {rest[0]}
            best, bn = None, 99
            for i in cand:
                n = len({kk for kk in keys[i] if kk not in idx})
                if n < bn:
                    best, bn = i, n
            if len(cur) + bn > 32:
                break
            cand.discard(best)
            left.discard(best)
            for kk in keys[best]:
                if kk not in idx:
                    idx[kk] = len(cur)
                    cur.append(kk)
                    cand.update(j for j in by_v[kk] if j in left)
            tl.append(tuple(idx[kk] for kk in keys[best]))
        loads.append((cur, tl))
    return loads


blob = bytearray(8 + 20 * (len(parts) + 1))
struct.pack_into('>II', blob, 0, len(parts) + 1, 0)


def align8():
    while len(blob) % 8:
        blob.append(0)


ntri = nvtx = 0
for pi, j in enumerate(parts):
    ts = [i for i in range(len(tris)) if tri_part[i] == j]
    r5, g5, b5 = srgb5(pcol[j])
    texel = (r5 << 11) | (g5 << 6) | (b5 << 1) | 1
    align8()
    tex = len(blob)
    blob += struct.pack('>4H', *([texel] * 4))
    loads = make_loads(ts)
    vofs = []
    for vs, _tl in loads:
        align8()
        vofs.append(len(blob))
        for (vi, nx, ny, nz) in vs:
            p = (vco[vi] + off) * scale
            n = Vector((nx, ny, nz)).normalized()
            blob += struct.pack('>hhhHhhbbbB', *(int(round(x)) for x in p), 0, 0, 0,
                                *(int(round(x * 127)) for x in n), 255)
        nvtx += len(vs)
    align8()
    dl = len(blob)
    blob += struct.pack('>II', 0xFD100000, tex)          # the texture: as the game's one-colour parts
    blob += struct.pack('>II', 0xE6000000, 0)
    blob += struct.pack('>II', 0xF3000000, 0x07003000)
    blob += struct.pack('>II', 0xF5100200, 0x00F00020)
    blob += struct.pack('>II', 0xF2002002, 0x0000E002)
    blob += struct.pack('>II', 0xBB000001, 0xFFFFFFFF)
    for (vs, tl), vo in zip(loads, vofs):
        n = len(vs)
        blob += struct.pack('>II', 0x04000000 | (n << 10) | (n * 16 - 1), vo)
        for k in range(0, len(tl), 2):
            a = tl[k]
            w0 = 0xB1000000 | (a[0] * 2 << 16) | (a[1] * 2 << 8) | (a[2] * 2)
            if k + 1 < len(tl):
                b = tl[k + 1]
                blob += struct.pack('>II', w0, (b[0] * 2 << 16) | (b[1] * 2 << 8) | (b[2] * 2))
            else:
                blob += struct.pack('>II', 0xBF000000, (a[0] * 2 << 16) | (a[1] * 2 << 8) | (a[2] * 2))
        ntri += len(tl)
    blob += struct.pack('>II', 0xB8000000, 0)
    # flags 4: both sides drawn (BrModelDraw clears the culling for it): a generated model's
    # winding is not to be trusted everywhere, and the lighting reads the vertices' normals
    struct.pack_into('>IHHfff', blob, 8 + 20 * pi, dl, 4, 0, 0.0, 0.0, 0.0)
    log('part', pi, 'colour', (r5, g5, b5), len(ts), 'triangles', len(loads), 'loads')

# a look at it: the low model in its colours, smooth shaded, from the front and the side
if len(argv) > 5:
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_WORKBENCH'
    sc.display.shading.light = 'STUDIO'
    sc.display.shading.color_type = 'MATERIAL'
    sc.render.resolution_x = sc.render.resolution_y = 384
    bpy.data.objects.remove(src)
    if csrc is not src:
        bpy.data.objects.remove(csrc)
    r = float(np.linalg.norm(mx - mn))
    ctr = Vector(((mn + mx) / 2).tolist())
    for nm, ang in (('front', -90), ('side', 0), ('three', -45)):
        cd = bpy.data.cameras.new('cam')
        cd.type = 'ORTHO'
        cd.ortho_scale = r * 1.1
        cam = bpy.data.objects.new('cam', cd)
        sc.collection.objects.link(cam)
        a_ = math.radians(ang)
        cam.location = ctr + Vector((math.cos(a_) * r * 1.6, math.sin(a_) * r * 1.6, r * 0.45))
        cam.rotation_euler = (ctr - cam.location).to_track_quat('-Z', 'Y').to_euler()
        sc.camera = cam
        sc.render.filepath = f"{argv[5]}_{nm}.png"
        bpy.ops.render.render(write_still=True)

# the shadow, as the game's icons have it: the body's outline seen from above (the hull
# of all its vertices) at z 0, under the floating body, drawn blended in the second pass
# in their grey
def hull(pts):
    pts = sorted(set(pts))
    if len(pts) < 3:
        return pts
    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lo_, hi_ = [], []
    for p_ in pts:
        while len(lo_) >= 2 and cross(lo_[-2], lo_[-1], p_) <= 0:
            lo_.pop()
        lo_.append(p_)
    for p_ in reversed(pts):
        while len(hi_) >= 2 and cross(hi_[-2], hi_[-1], p_) <= 0:
            hi_.pop()
        hi_.append(p_)
    return lo_[:-1] + hi_[:-1]                       # counter-clockwise from above


foot = [tuple(int(round(x)) for x in ((vco[i] + off) * scale)[:2]) for i in range(len(vco))]
ring = hull(foot)
if len(ring) > 32:                                   # (one load: evenly spaced around it)
    ring = [ring[int(i * len(ring) / 32)] for i in range(32)]
align8()
stex = len(blob)
blob += bytes.fromhex('7777777777777777')
align8()
sv = len(blob)
for (x, y) in ring:
    blob += struct.pack('>hhhHhhbbbB', x, y, 0, 0, 0, 0, 0, 0, 127, 0)
align8()
sdl = len(blob)
for w0, w1 in ((0xFD100000, stex), (0xE6000000, 0), (0xF3000000, 0x07003000), (0xF5800200, 0x00F00040),
               (0xF2002002, 0x0003E002), (0xBB000001, 0xFFFFFFFF), (0xE7000000, 0),
               (0xB900031D, 0x0C1849D8), (0xFCFFA004, 0xFFFD93F8), (0xFA001700, 0),
               (0x04000000 | (len(ring) << 10) | (len(ring) * 16 - 1), sv)):
    blob += struct.pack('>II', w0, w1)
for k in range(1, len(ring) - 1):
    blob += struct.pack('>II', 0xBF000000, (0 << 16) | (k * 2 << 8) | ((k + 1) * 2))
for w0, w1 in ((0xE7000000, 0), (0xB900031D, 0x0C192038), (0xFC26A004, 0x1FFC93F8), (0xB8000000, 0)):
    blob += struct.pack('>II', w0, w1)
struct.pack_into('>IHHfff', blob, 8 + 20 * len(parts), sdl, 8, 0, 0.0, 0.0, 0.0)
log('shadow', len(ring), 'points')

open(OUT, 'wb').write(blob)
log('wrote', OUT, len(blob), 'bytes,', len(parts), 'parts,', ntri, 'triangles,', nvtx, 'vertices')
