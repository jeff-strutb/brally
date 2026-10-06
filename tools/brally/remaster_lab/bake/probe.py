import bpy, sys
dg = bpy.context.evaluated_depsgraph_get()
for o in bpy.context.scene.objects:
    mods = [(m.type, m.name) for m in getattr(o, 'modifiers', [])]
    tris = 0
    if o.type == 'MESH':
        tris = sum(len(p.vertices) - 2 for p in o.data.polygons)
    ev = o.evaluated_get(dg)
    etris = 0
    try:
        me = ev.to_mesh(); etris = sum(len(p.vertices)-2 for p in me.polygons); ev.to_mesh_clear()
    except Exception: pass
    mats = [m.name for m in getattr(o.data, 'materials', [])] if o.type == 'MESH' else []
    print('OBJ', o.name, o.type, 'hide', o.hide_render, o.hide_get(), 'coll', [c.name for c in o.users_collection], 'tris', tris, 'eval', etris, 'mods', mods, 'mats', mats, 'dims', tuple(round(d,2) for d in o.dimensions), 'inst', o.instance_type)
insts = sum(1 for i in dg.object_instances if i.is_instance)
print('INSTANCES', insts)
