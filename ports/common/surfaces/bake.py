"""bake.py: each set of surfaces.txt (src/, fetch.py) as the two maps the lit renderer
reads (brr_metal_surf.h), 2048 in baked/ and 1024 in baked1k/ (the iPhone's):

  <id>_a.raw  RGB the albedo's detail: each texel's linear colour over its neighbourhood's
              (a wrapping Gaussian of a sixteenth of the tile), times 0.5, so 1.0 is 128 and
              the region's own colour (the game texture's) sets the mean; the scan's own large
              blotches, which would repeat every tile, are gone, and its outliers held to 2.5
              sigma (log luminance, soft).  A the height, its 1st to
              99th percentile over 0..1
  <id>_b.raw  RG the normal's x and y (OpenGL convention, +y up; z is rebuilt)

Each file: a line "BRSURF 1 <width> <height> <channels>", then the texels, rows top first,
linear values (light_surf_sets.c reads them on every platform; photographed detail barely
compresses, so PNG saved nothing)."""
import os, glob
import numpy as np
from PIL import Image
D = os.path.dirname(os.path.abspath(__file__))
def lin(c): return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)
def gauss_wrap(x, sig):
    """a Gaussian blur of a tiling image (H, W[, C]), wrapping, by FFT"""
    H, W = x.shape[:2]
    fy = np.fft.fftfreq(H)[:, None]; fx = np.fft.fftfreq(W)[None, :]
    g = np.exp(-2 * (np.pi * sig) ** 2 * (fx * fx + fy * fy))
    if x.ndim == 3: g = g[..., None]
    return np.real(np.fft.ifft2(np.fft.fft2(x, axes=(0, 1)) * g, axes=(0, 1)))
def load(p, mode):
    im = Image.open(p)
    if im.mode in ('I;16', 'I;16B', 'I'):
        a = np.asarray(im, np.float64) / 65535.0
        return a if mode == 'L' else np.repeat(a[..., None], 3, 2)
    return np.asarray(im.convert(mode), np.float64) / 255.0
os.makedirs(D + '/baked', exist_ok=True); os.makedirs(D + '/baked1k', exist_ok=True)
ids = [l.split()[0] for l in open(D + '/surfaces.txt') if l.strip() and not l.startswith('#')]
for i in ids:
    src = lambda k: glob.glob('%s/src/%s_%s.*' % (D, i, k))[0]
    c = lin(load(src('diff'), 'RGB'))
    n = c.shape[0]
    ratio = c / np.maximum(gauss_wrap(c, n / 16.0), 1e-4)
    # outliers (a footprint in snow, a black speck in sand) soft-limited in log space to 2.5
    # sigma (by the luminance's median absolute deviation), so nothing alone marks the repeat
    y = np.log(np.maximum(ratio @ [0.2126, 0.7152, 0.0722], 1e-4))
    med = np.median(y); sg = 1.4826 * np.median(np.abs(y - med)); k = 2.5 * sg
    y2 = med + k * np.tanh((y - med) / k)
    ratio = ratio * np.exp(y2 - y)[..., None]
    ratio /= ratio.mean((0, 1))                     # mean 1 again: the region's colour is the mean
    h = load(src('disp'), 'L')
    lo, hi = np.percentile(h, [1, 99]); h = np.clip((h - lo) / max(hi - lo, 1e-6), 0, 1)
    nr = np.asarray(Image.open(src('nor_gl')).convert('RGB'), np.float64) / 255.0
    r = load(src('rough'), 'L')
    a8 = np.dstack([np.clip(ratio * 0.5, 0, 1), h]); b8 = nr[..., :2]
    for size, out in ((2048, 'baked'), (1024, 'baked1k')):
        for name, arr in (('a', a8), ('b', b8)):
            if arr.shape[0] != size:   # a box filter: each texel the mean of its block
                f = arr.shape[0] // size
                arr = arr.reshape(size, f, size, f, arr.shape[2]).mean((1, 3))
            px = np.round(arr * 255).astype(np.uint8)
            with open('%s/%s/%s_%s.raw' % (D, out, i, name), 'wb') as fo:
                fo.write(b'BRSURF 1 %d %d %d\n' % (size, size, px.shape[2]))
                fo.write(px.tobytes())
    print('%-22s detail: mean %.3f  sd %.3f  clipped %.4f%%  height p50 %.2f  rough %.2f' % (
        i, ratio.mean(), ratio.std(), 100 * float((ratio > 2).mean()), np.median(h), r.mean()))
