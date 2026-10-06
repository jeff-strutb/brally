import bpy
for o in bpy.context.scene.objects:
    for m in getattr(o,'modifiers',[]):
        if m.type=='NODES' and m.node_group and 'geometry_nodes' in o.name:
            print('OBJ',o.name,'group',m.node_group.name)
            for it in m.node_group.interface.items_tree:
                if getattr(it,'in_out',None)=='INPUT':
                    v = m.get(it.identifier)
                    print('  IN',it.name,it.socket_type,getattr(v,'name',v))
