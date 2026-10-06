import bpy, sys
from mathutils import Vector
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=sys.argv[sys.argv.index('--')+1])
for o in bpy.context.scene.objects:
    if o.type=='MESH' and 'Fern_B051_MI' in o.name:
        mw=o.matrix_world
        bb=[mw @ Vector(c) for c in o.bound_box]
        vs=[mw @ v.co for v in o.data.vertices]
        print('FERN',o.name,'bbox x',round(min(p.x for p in bb),2),round(max(p.x for p in bb),2),'verts x',round(min(p.x for p in vs),2),round(max(p.x for p in vs),2),'mods',[m.type for m in o.modifiers],'shape',bool(o.data.shape_keys), 'parent', o.parent.name if o.parent else None, 'scale', tuple(round(x,3) for x in o.matrix_world.to_scale()))
