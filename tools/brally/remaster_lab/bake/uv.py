import bpy
o=bpy.data.objects['fir_tree_01_a_LOD0']; me=o.data
a=me.attributes['UVMap']; print('ATTR',a.data_type,a.domain,len(a.data),len(me.loops))
