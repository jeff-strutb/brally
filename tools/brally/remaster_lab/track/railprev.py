import bpy, math, sys
from mathutils import Vector
o=bpy.data.objects["guardrail_a_LOD0"]
for k in (1,2):
    c=o.copy(); c.location.x=2.0*k; bpy.context.scene.collection.objects.link(c)
# ground
bpy.ops.mesh.primitive_plane_add(size=20, location=(2,0,-0.55))
w=bpy.data.worlds.new("w"); w.use_nodes=True; w.node_tree.nodes["Background"].inputs[0].default_value=(0.6,0.7,0.9,1); w.node_tree.nodes["Background"].inputs[1].default_value=0.8
bpy.context.scene.world=w
bpy.ops.object.light_add(type="SUN", rotation=(math.radians(50),0,math.radians(30))); bpy.context.object.data.energy=4
bpy.ops.object.camera_add(location=(4.5,3.2,0.6)); cam=bpy.context.object
d=Vector((1.5,0,0))-cam.location; cam.rotation_euler=d.to_track_quat('-Z','Y').to_euler(); cam.data.lens=35
s=bpy.context.scene; s.camera=cam; s.render.engine="CYCLES"; s.cycles.samples=48; s.render.resolution_x=900; s.render.resolution_y=500
s.render.filepath=sys.argv[-1]; bpy.ops.render.render(write_still=True)
