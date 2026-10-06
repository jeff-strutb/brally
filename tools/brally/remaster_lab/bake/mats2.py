import bpy
for m in bpy.data.materials:
    if not m.use_nodes: continue
    b = next((n for n in m.node_tree.nodes if n.type=='BSDF_PRINCIPLED'), None)
    print('MAT', m.name, [ (n.type, n.image.name if n.type=='TEX_IMAGE' and n.image else '') for n in m.node_tree.nodes], 'base linked', bool(b and b.inputs['Base Color'].links), 'basecol', tuple(round(x,2) for x in b.inputs['Base Color'].default_value) if b else None)
for o in bpy.data.objects:
    if o.type=='MESH': print('OBJ', o.name, [s.material.name if s.material else None for s in o.material_slots], 'colattr', [a.name for a in o.data.color_attributes])
