import bpy
for o in bpy.data.objects:
    ms=[(m.type,m.name) for m in getattr(o,'modifiers',[])]
    t=sum(len(p.vertices)-2 for p in o.data.polygons) if o.type=='MESH' else 0
    print('OBJ',o.name,o.type,t,ms,[c.name for c in o.users_collection])
