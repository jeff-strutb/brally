import bpy
for m in bpy.data.materials:
    if not m.use_nodes: continue
    at=[(n.attribute_name,n.attribute_type) for n in m.node_tree.nodes if n.type=='ATTRIBUTE']
    uvn=[n.uv_map for n in m.node_tree.nodes if n.type=='UVMAP']
    tc=[n.name for n in m.node_tree.nodes if n.type=='TEX_COORD' and any(o.links for o in n.outputs)]
    print('MAT',m.name,at,uvn,tc)
for o in bpy.data.objects:
    if o.type=='MESH' and 'LOD0' in o.name:
        print('UV',o.name,[u.name for u in o.data.uv_layers],'active',o.data.uv_layers.active.name if o.data.uv_layers.active else None, [a.name for a in o.data.attributes if a.domain=='CORNER'])
