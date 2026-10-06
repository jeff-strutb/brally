import bpy, numpy as np
o = bpy.data.objects['island_tree_02_LOD1']; me = o.data
print('UVL', [(u.name, u.active, u.active_render) for u in me.uv_layers], [s.material.name for s in o.material_slots])
mi = np.empty(len(me.polygons), np.int32); me.polygons.foreach_get('material_index', mi)
for li, lay in enumerate(me.uv_layers):
    a = np.empty(len(me.loops) * 2, np.float32); lay.data.foreach_get('uv', a); a = a.reshape(-1, 2)
    print('RANGE', lay.name, a.min(0), a.max(0))
# leaves loops uv range
loops = np.empty(len(me.polygons), np.int32); me.polygons.foreach_get('loop_start', loops)
tot = np.empty(len(me.polygons), np.int32); me.polygons.foreach_get('loop_total', tot)
for m in set(mi.tolist()):
    sel = np.nonzero(mi == m)[0][:2000]
    idx = np.concatenate([np.arange(loops[i], loops[i] + tot[i]) for i in sel])
    a = np.empty(len(me.loops) * 2, np.float32); me.uv_layers.active.data.foreach_get('uv', a); a = a.reshape(-1, 2)[idx]
    print('MAT', m, o.material_slots[m].material.name, 'uv', a.min(0).round(3), a.max(0).round(3), 'polys', (mi == m).sum())
