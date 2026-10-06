import bpy, sys
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=sys.argv[sys.argv.index('--')+1])
def show(o, d=0):
    if d < 6: print('TREE', '  '*d + o.name, o.type, o.data.name if o.type=='MESH' else '', len(o.data.polygons) if o.type=='MESH' else '')
    for c in o.children: show(c, d+1)
for o in bpy.context.scene.objects:
    if o.parent is None: show(o)
