import bpy
def walk(tree, pre=''):
    for n in tree.nodes:
        if n.type == 'MAPPING':
            v = {i.name: tuple(round(x, 3) for x in i.default_value) for i in n.inputs if i.name in ('Location', 'Rotation', 'Scale')}
            print('MAP', pre, n.name, n.vector_type, v, [l.from_node.type + ':' + l.from_socket.name for l in n.inputs['Vector'].links])
        if n.type == 'GROUP' and n.node_tree: walk(n.node_tree, pre + '/' + n.node_tree.name)
for m in bpy.data.materials:
    if m.use_nodes and ('leaves' in m.name or 'twig' in m.name):
        print('MAT', m.name); walk(m.node_tree)
