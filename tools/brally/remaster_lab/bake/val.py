import bpy
for n in ('jacaranda_tree_LOD0','jacaranda_tree_LOD1','jacaranda_tree_trunk_LOD0'):
    me=bpy.data.objects[n].data
    print('ATTRS',n,[(a.name,a.domain,a.data_type,len(a.data)) for a in me.attributes], 'loops',len(me.loops),'polys',len(me.polygons),'verts',len(me.vertices),'edges',len(me.edges))
    print('VALIDATE',n,me.validate(verbose=False))
