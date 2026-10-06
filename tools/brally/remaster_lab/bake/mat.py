import bpy
for m in bpy.data.materials:
    if not m.use_nodes: continue
    print('MAT', m.name, 'blend', getattr(m,'blend_method',None), 'cull', m.use_backface_culling)
    for n in m.node_tree.nodes:
        extra = n.image.filepath if n.type=='TEX_IMAGE' and n.image else (n.node_tree.name if n.type=='GROUP' else '')
        print('  N', n.type, n.name, extra, [ (i.name, [l.from_node.name for l in i.links]) for i in n.inputs if i.links])
