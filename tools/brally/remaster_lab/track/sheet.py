import json, os
from PIL import Image, ImageDraw
js = json.load(open('texcat.json'))
C = 72; COLS = 14
for name, L in js.items():
    L = sorted(L, key=lambda t: -t['area'])
    rows = (len(L) + COLS - 1) // COLS
    S = Image.new('RGB', (COLS * C, rows * (C + 12)), (40, 40, 40))
    d = ImageDraw.Draw(S)
    for i, t in enumerate(L):
        t['idx'] = i
        x, y = (i % COLS) * C, (i // COLS) * (C + 12)
        if t['png']:
            im = Image.open('tex/' + t['png']).convert('RGBA')
            s = (C - 4) / max(im.size); im = im.resize((max(1, int(im.size[0] * s)), max(1, int(im.size[1] * s))), Image.NEAREST)
            bg = Image.new('RGBA', im.size, (255, 0, 255, 255)); bg.alpha_composite(im)
            S.paste(bg.convert('RGB'), (x + 2, y + 2))
        d.text((x + 2, y + C - 1), '%d %s' % (i, t['fmt']), fill=(255, 255, 0))
    S.save('sheet_%s.png' % name)
json.dump(js, open('texcat.json', 'w'), indent=1)
