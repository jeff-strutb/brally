import bpy, sys, re, collections
asset = sys.argv[sys.argv.index('--') + 1]
c = collections.defaultdict(dict)
for o in bpy.data.objects:
    m = re.match(re.escape(asset) + r'_?(.*?)_?LOD(\d)$', o.name)
    if m and o.type == 'MESH':
        c[m.group(1) or 'a'][int(m.group(2))] = sum(len(p.vertices) - 2 for p in o.data.polygons)
print('LODS', asset, dict(c))
