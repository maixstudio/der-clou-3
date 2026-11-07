"""Procedural, seamlessly tiling PBR textures for the item props.

Every texture is built from periodic (FFT) noise, so it tiles without seams. Each set writes
  T_<Name>_BC.png   base colour (sRGB)
  T_<Name>_N.png    normal map, DirectX convention (Unreal)
  T_<Name>_ORM.png  R = ambient occlusion, G = roughness, B = metallic mask
Arrays are stored with row 0 at the bottom (Blender's pixel order).
"""

import os

import numpy as np

# ---------------------------------------------------------------------------------------------
# Noise helpers (all periodic over the tile)
# ---------------------------------------------------------------------------------------------


def _freqs(n):
    f = np.fft.fftfreq(n) * n  # integer cycles per tile
    return f[None, :], f[:, None]  # fx (columns = u), fy (rows = v)


def pnoise(n, seed, beta=2.0, stretch_u=1.0, stretch_v=1.0, low=0.0, high=None):
    """Fractal periodic noise, normalised to mean 0 / std 1.

    beta: spectral falloff (higher = smoother). stretch_u > 1 elongates features along u.
    low/high: band limits in cycles per tile.
    """
    rng = np.random.default_rng(seed)
    spectrum = np.fft.fft2(rng.standard_normal((n, n)))
    fx, fy = _freqs(n)
    f = np.sqrt((fx * stretch_u) ** 2 + (fy * stretch_v) ** 2)
    f[0, 0] = 1.0
    amp = f ** (-beta / 2.0)
    if low > 0:
        amp *= 1.0 - np.exp(-(f / low) ** 2)
    if high is not None:
        amp *= np.exp(-(f / high) ** 2)
    amp[0, 0] = 0.0
    r = np.real(np.fft.ifft2(spectrum * amp))
    return (r - r.mean()) / (r.std() + 1e-9)


def grid(n):
    c = (np.arange(n) + 0.5) / n
    return np.meshgrid(c, c)  # u (columns), v (rows)


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def remap01(x):
    lo, hi = np.percentile(x, 1), np.percentile(x, 99)
    return np.clip((x - lo) / (hi - lo + 1e-9), 0.0, 1.0)


def mix(a, b, t):
    t = t[..., None] if np.ndim(t) == 2 else t
    return np.asarray(a) * (1.0 - t) + np.asarray(b) * t


def normal_from_height(h, strength):
    """DirectX normal map from a height field given in 'pixels of height' * strength."""
    dhdu = (np.roll(h, -1, axis=1) - np.roll(h, 1, axis=1)) * 0.5 * strength
    dhdv = (np.roll(h, -1, axis=0) - np.roll(h, 1, axis=0)) * 0.5 * strength
    nx, ny, nz = -dhdu, dhdv, np.ones_like(h)  # +dhdv = DirectX green
    length = np.sqrt(nx * nx + ny * ny + nz * nz)
    return np.stack([nx / length, ny / length, nz / length], axis=-1) * 0.5 + 0.5


def cavity(h, amount):
    """Cheap AO: darker where the height is below its local average."""
    blur = h.copy()
    for shift in (1, 2, 4):
        blur = (blur + np.roll(blur, shift, 0) + np.roll(blur, -shift, 0) + np.roll(blur, shift, 1) + np.roll(blur, -shift, 1)) / 5.0
    return np.clip(1.0 - np.maximum(blur - h, 0.0) * amount, 0.0, 1.0)


def scratches(n, seed, count, length=(0.02, 0.15), angle_spread=np.pi):
    """Sparse thin lines (periodic), value 0..1."""
    rng = np.random.default_rng(seed)
    out = np.zeros((n, n))
    for _ in range(count):
        x0, y0 = rng.random() * n, rng.random() * n
        ang = rng.random() * angle_spread
        ln = rng.uniform(*length) * n
        steps = int(ln * 1.5) + 2
        t = np.linspace(0, ln, steps)
        xs = (x0 + np.cos(ang) * t).astype(int) % n
        ys = (y0 + np.sin(ang) * t).astype(int) % n
        out[ys, xs] = np.maximum(out[ys, xs], rng.uniform(0.3, 1.0))
    return out


# ---------------------------------------------------------------------------------------------
# Texture sets
# ---------------------------------------------------------------------------------------------


def wood(n, seed, dark, light, rings=14, pore_strength=0.35):
    """Grain runs along u. Tile = about 1 m of timber."""
    u, v = grid(n)
    warp = pnoise(n, seed, beta=3.2, stretch_u=6.0) * 0.06 + pnoise(n, seed + 1, beta=2.4, stretch_u=12.0) * 0.012
    phase = (v + warp) * rings
    ring = phase - np.floor(phase)
    # Late wood: sharp dark band at the end of each ring.
    late = smoothstep(0.55, 0.92, ring) * (1.0 - smoothstep(0.95, 1.0, ring))
    fibres = pnoise(n, seed + 2, beta=1.6, stretch_u=24.0, high=n * 0.45)
    pores = np.clip(pnoise(n, seed + 3, beta=0.6, stretch_u=40.0) - 1.4, 0.0, None)
    blotch = pnoise(n, seed + 4, beta=3.0, stretch_u=2.0) * 0.5 + 0.5

    t = np.clip(late * 0.75 + fibres * 0.06 + (blotch - 0.5) * 0.25, 0.0, 1.0)
    color = mix(light, dark, t)
    color *= (1.0 - pores * pore_strength)[..., None]
    height = -late * 0.6 + fibres * 0.15 - pores * 1.2
    rough = np.clip(0.42 + late * 0.12 + pores * 0.25 + fibres * 0.03, 0.0, 1.0)
    return color, height, rough, np.ones_like(u), 2.5


def fabric(n, seed, threads=48):
    """Plain weave, neutral grey (tinted per material). Tile is small (about 8 cm)."""
    u, v = grid(n)
    cu, cv = (u * threads) % 1.0, (v * threads) % 1.0
    iu, iv = np.floor(u * threads), np.floor(v * threads)
    over = ((iu + iv) % 2) == 0
    warp = np.sin(np.pi * cu) * (0.55 + 0.45 * np.sin(np.pi * cv))
    weft = np.sin(np.pi * cv) * (0.55 + 0.45 * np.sin(np.pi * cu))
    h = np.where(over, warp, weft)
    fuzz = pnoise(n, seed, beta=1.2, high=n * 0.4)
    slub = pnoise(n, seed + 1, beta=2.5, stretch_u=8.0)
    h = h + fuzz * 0.08 + slub * 0.015
    shade = 0.66 + 0.18 * remap01(h) + slub * 0.012
    color = np.repeat(np.clip(shade, 0, 1)[..., None], 3, axis=2)
    rough = np.clip(0.86 + fuzz * 0.04, 0.0, 1.0)
    return color, h, rough, np.ones_like(u), 3.0


def leather(n, seed):
    """Pebble grain, neutral (tinted per material)."""
    a = pnoise(n, seed, beta=1.0, low=n * 0.05, high=n * 0.12)
    b = pnoise(n, seed + 1, beta=1.0, low=n * 0.1, high=n * 0.25)
    pebbles = 1.0 - np.abs(a)  # ridges = creases between pebbles
    creases = smoothstep(0.75, 1.0, pebbles)
    h = -creases * 1.0 + b * 0.15
    blotch = pnoise(n, seed + 2, beta=3.0) * 0.5 + 0.5
    shade = 0.7 - creases * 0.18 + (blotch - 0.5) * 0.12
    color = np.repeat(np.clip(shade, 0, 1)[..., None], 3, axis=2)
    rough = np.clip(0.55 + creases * 0.2 - (blotch - 0.5) * 0.1, 0.0, 1.0)
    return color, h, rough, np.ones_like(a), 2.0


def brushed_metal(n, seed):
    streaks = pnoise(n, seed, beta=1.0, stretch_u=60.0, high=n * 0.5)
    smudge = pnoise(n, seed + 1, beta=3.0) * 0.5 + 0.5
    sc = scratches(n, seed + 2, 120, angle_spread=0.15)
    shade = 0.78 + streaks * 0.03 - sc * 0.05
    color = np.repeat(np.clip(shade, 0, 1)[..., None], 3, axis=2)
    h = streaks * 0.2 - sc * 0.6
    rough = np.clip(0.28 + streaks * 0.03 + smudge * 0.12 + sc * 0.1, 0.0, 1.0)
    return color, h, rough, np.ones_like(h), 1.5


def fine(n, seed):
    """Neutral 'used surface': smudges and fine scratches. For polished metals, lacquer, plastic."""
    smudge = remap01(pnoise(n, seed, beta=3.0))
    micro = pnoise(n, seed + 1, beta=1.0, high=n * 0.5)
    sc = scratches(n, seed + 2, 220)
    color = np.repeat(np.clip(1.0 - sc * 0.08 - smudge * 0.04, 0, 1)[..., None], 3, axis=2)
    h = micro * 0.03 - sc * 0.4
    rough = np.clip(0.18 + smudge * 0.18 + sc * 0.15 + micro * 0.01, 0.0, 1.0)
    return color, h, rough, np.ones_like(h), 1.0


def paint(n, seed):
    """Painted steel: orange peel, a little wear."""
    peel = pnoise(n, seed, beta=2.0, low=n * 0.02, high=n * 0.08)
    smudge = remap01(pnoise(n, seed + 1, beta=3.0))
    sc = scratches(n, seed + 2, 90)
    color = np.repeat(np.clip(0.85 + smudge * 0.08 - sc * 0.25, 0, 1)[..., None], 3, axis=2)
    h = peel * 0.06 - sc * 0.5
    rough = np.clip(0.38 + smudge * 0.15 + sc * 0.2, 0.0, 1.0)
    metal = np.clip(sc * 1.5, 0.0, 1.0)  # bare metal in the scratches
    return color, h, rough, metal, 1.0


def cardboard(n, seed):
    u, v = grid(n)
    blotch = pnoise(n, seed, beta=2.6) * 0.5 + 0.5
    fibres = pnoise(n, seed + 1, beta=1.2, stretch_u=3.0, high=n * 0.5)
    flutes = np.sin(v * np.pi * 2 * 90) * 0.5 + 0.5  # corrugation shows through faintly
    base = np.array([0.62, 0.46, 0.29])
    color = base * (0.86 + blotch[..., None] * 0.2 + fibres[..., None] * 0.03)
    h = flutes * 0.3 + fibres * 0.1
    rough = np.clip(0.88 + fibres * 0.03, 0.0, 1.0)
    return np.clip(color, 0, 1), h, rough, np.zeros_like(u), 1.5


def paper(n, seed):
    fibres = pnoise(n, seed, beta=1.3, high=n * 0.5)
    blotch = pnoise(n, seed + 1, beta=3.0) * 0.5 + 0.5
    color = np.array([0.93, 0.92, 0.88]) * (0.97 + blotch[..., None] * 0.03 + fibres[..., None] * 0.01)
    return np.clip(color, 0, 1), fibres * 0.1, np.full_like(fibres, 0.8), np.zeros_like(fibres), 1.0


def marble(n, seed):
    u, v = grid(n)
    turb = pnoise(n, seed, beta=2.4) * 1.2 + pnoise(n, seed + 1, beta=1.6) * 0.3
    vein = np.abs(np.sin((u * 3 + v * 2) * np.pi * 2 + turb * 3.0))
    vein = 1.0 - smoothstep(0.0, 0.12, vein)
    cloud = pnoise(n, seed + 2, beta=3.0) * 0.5 + 0.5
    color = mix(np.array([0.92, 0.91, 0.88]) * (0.95 + cloud * 0.05)[..., None], np.array([0.45, 0.45, 0.47]), vein * 0.8)
    return np.clip(color, 0, 1), -vein * 0.1, np.full_like(u, 0.2) + vein * 0.05, np.zeros_like(u), 1.0


def bronze(n, seed):
    blotch = remap01(pnoise(n, seed, beta=2.4))
    micro = pnoise(n, seed + 1, beta=1.2, high=n * 0.5)
    patina = smoothstep(0.62, 0.85, blotch)
    color = mix(np.array([0.55, 0.36, 0.18]) * (0.9 + micro * 0.05)[..., None], np.array([0.30, 0.45, 0.38]), patina)
    rough = np.clip(0.32 + patina * 0.5 + micro * 0.03, 0, 1)
    metal = 1.0 - patina
    return np.clip(color, 0, 1), micro * 0.1 + patina * 0.3, rough, metal, 1.5


def banknote(n, seed):
    """Generic note (no real currency): guilloche pattern, border and an oval medallion."""
    u, v = grid(n)
    # The note is 2:1 – the UVs stretch the square texture over it.
    x, y = (u - 0.5) * 2.0, (v - 0.5)
    r = np.sqrt((x + 0.45) ** 2 + (y * 2) ** 2)
    ang = np.arctan2(y * 2, x + 0.45)
    guil = 0.5 + 0.5 * np.sin(r * 90 + np.sin(ang * 12) * 3)
    lines = 0.5 + 0.5 * np.sin((u * 7 + np.sin(v * np.pi * 2 * 3) * 0.08) * np.pi * 2 * 30)
    border = (np.minimum(np.minimum(u, 1 - u) * 2, np.minimum(v, 1 - v)) < 0.06).astype(float)
    medal = smoothstep(0.42, 0.38, np.sqrt(((u - 0.72) * 2) ** 2 + ((v - 0.5) * 1.3) ** 2))
    paper_c = np.array([0.80, 0.85, 0.76])
    ink = np.array([0.22, 0.38, 0.28])
    t = np.clip(guil * 0.25 * (1 - medal) + lines * 0.12 + border * 0.6 + medal * (0.3 + 0.3 * guil), 0, 1)
    color = mix(paper_c, ink, t)
    return np.clip(color, 0, 1), -t * 0.2, np.full_like(u, 0.75), np.zeros_like(u), 1.0


def painting(n, seed):
    """Original abstract landscape in oils: sky, rolling hills, a low sun; visible brush strokes."""
    u, v = grid(n)
    rng = np.random.default_rng(seed)
    sky = mix(np.array([0.85, 0.66, 0.42]), np.array([0.32, 0.45, 0.62]), smoothstep(0.55, 1.0, v))
    color = sky.copy()
    sun = smoothstep(0.09, 0.06, np.sqrt((u - 0.7) ** 2 + ((v - 0.62) * 1.0) ** 2))
    color = mix(color, np.array([0.98, 0.86, 0.55]), sun)
    hill_cols = [np.array([0.36, 0.42, 0.26]), np.array([0.27, 0.35, 0.2]), np.array([0.2, 0.27, 0.15]), np.array([0.14, 0.18, 0.1])]
    for i, col in enumerate(hill_cols):
        base = 0.55 - i * 0.12
        line = base + 0.05 * np.sin(u * np.pi * 2 * (1 + i) + rng.random() * 6) + 0.025 * np.sin(u * np.pi * 2 * (3 + i * 2) + rng.random() * 6)
        shade = 0.85 + 0.3 * smoothstep(line - 0.15, line, v)  # lighter towards the ridge
        color = mix(color, col * shade[..., None], smoothstep(line + 0.01, line - 0.01, v))
    strokes = pnoise(n, seed + 1, beta=1.4, stretch_u=10.0, high=n * 0.4)
    strokes2 = pnoise(n, seed + 2, beta=1.4, stretch_v=10.0, high=n * 0.4)
    stroke = np.where(v > 0.5, strokes, strokes2)
    color = color * (1.0 + stroke[..., None] * 0.05)
    weave = fabric(n, seed + 3, threads=120)[1]
    h = stroke * 0.6 + weave * 0.15
    rough = np.clip(0.45 + stroke * 0.05, 0, 1)
    return np.clip(color, 0, 1), h, rough, np.zeros_like(u), 2.0


def floor_planks(n, seed, rows=16):
    """Oak floor boards along u, staggered joints. Tile = 2 m (16 boards of 12.5 cm)."""
    rng = np.random.default_rng(seed)
    u, v = grid(n)
    wc, wh, wr, _, _ = wood(n, seed + 1, dark=[0.30, 0.19, 0.10], light=[0.56, 0.39, 0.23], rings=26, pore_strength=0.4)
    row = np.floor(v * rows).astype(int)
    fv = (v * rows) % 1.0
    cuts = np.sort(rng.random((rows, 2)), axis=1)
    c = cuts[row]
    seg = (u[..., None] >= c).sum(-1) % 2  # the last piece continues across the tile edge
    pid = row * 2 + seg
    count = rows * 2
    tint = 0.82 + 0.32 * rng.random(count)
    shift = rng.integers(0, n, count)
    rough_off = (rng.random(count) - 0.5) * 0.12
    rr = (np.arange(n)[:, None] + shift[pid]) % n
    cc = np.broadcast_to(np.arange(n)[None, :], (n, n))
    color = wc[rr, cc] * tint[pid][..., None]
    h = wh[rr, cc] * 0.6
    rough = wr[rr, cc] + rough_off[pid] + 0.05
    du = np.min(np.abs(((u[..., None] - c) + 0.5) % 1.0 - 0.5), axis=-1)
    gap =np.maximum(smoothstep(0.012, 0.0, np.minimum(fv, 1.0 - fv)), smoothstep(1.6 / n, 0.0, du))
    bevel = smoothstep(0.05, 0.0, np.minimum(fv, 1.0 - fv))
    color = color * (1.0 - gap * 0.8)[..., None] * (1.0 - bevel * 0.1)[..., None]
    h = h - gap * 2.0 - bevel * 0.5
    rough = np.clip(rough + gap * 0.25, 0.0, 1.0)
    return np.clip(color, 0, 1), h, rough, np.zeros_like(u), 2.0


def wallpaper(n, seed, stripes=20):
    """Old-town wallpaper: muted vertical stripes (u horizontal, v up). Tile = 1 m."""
    u, v = grid(n)
    phase = (u * stripes) % 1.0
    wide = smoothstep(0.02, 0.06, phase) * (1.0 - smoothstep(0.54, 0.58, phase))
    pin = smoothstep(0.70, 0.71, phase) * (1.0 - smoothstep(0.73, 0.74, phase))
    base = np.array([0.66, 0.68, 0.58])
    stripe = np.array([0.55, 0.60, 0.50])
    color = mix(base, stripe, wide)
    color = mix(color, np.array([0.78, 0.74, 0.60]), pin * 0.8)
    fibres = pnoise(n, seed, beta=1.3, high=n * 0.5)
    age = pnoise(n, seed + 1, beta=3.0) * 0.5 + 0.5
    color = color * (0.95 + fibres[..., None] * 0.015) * (0.92 + age[..., None] * 0.08)
    h = fibres * 0.08 + wide * 0.05
    rough = np.clip(0.82 + fibres * 0.02, 0, 1)
    return np.clip(color, 0, 1), h, rough, np.zeros_like(u), 1.0


def concrete(n, seed):
    """Dark courtyard concrete. Tile = 3 m."""
    blotch = pnoise(n, seed, beta=2.6) * 0.5 + 0.5
    grain = pnoise(n, seed + 1, beta=0.8, high=n * 0.5)
    pits = np.clip(pnoise(n, seed + 2, beta=0.5) - 2.0, 0, None)
    cracks = scratches(n, seed + 3, 25, length=(0.05, 0.3))
    shade = 0.30 + blotch * 0.08 + grain * 0.012 - pits * 0.08 - cracks * 0.1
    color = np.array([1.0, 1.0, 0.98]) * shade[..., None]
    h = grain * 0.3 - pits * 1.5 - cracks * 1.0 + blotch * 0.3
    rough = np.clip(0.86 + grain * 0.03 - blotch * 0.08, 0, 1)
    return np.clip(color, 0, 1), h, rough, np.zeros_like(blotch), 1.5


def flat(n, value, rough):
    c = np.ones((n, n, 3)) * np.asarray(value)
    return c, np.zeros((n, n)), np.full((n, n), rough), np.ones((n, n)), 1.0


TEXTURES = {
    # name: (builder, size)
    "Wood_Walnut": (lambda n: wood(n, 11, dark=[0.16, 0.09, 0.05], light=[0.36, 0.22, 0.13], rings=16), 2048),
    "Wood_Oak": (lambda n: wood(n, 21, dark=[0.42, 0.29, 0.16], light=[0.70, 0.54, 0.34], rings=11, pore_strength=0.5), 2048),
    "Wood_Mahogany": (lambda n: wood(n, 31, dark=[0.22, 0.07, 0.04], light=[0.45, 0.17, 0.09], rings=20, pore_strength=0.25), 2048),
    "Fabric": (lambda n: fabric(n, 41), 1024),
    "Leather": (lambda n: leather(n, 51), 1024),
    "BrushedMetal": (lambda n: brushed_metal(n, 61), 1024),
    "Fine": (lambda n: fine(n, 71), 1024),
    "Paint": (lambda n: paint(n, 81), 1024),
    "Cardboard": (lambda n: cardboard(n, 91), 1024),
    "Paper": (lambda n: paper(n, 101), 1024),
    "Marble": (lambda n: marble(n, 111), 1024),
    "Bronze": (lambda n: bronze(n, 121), 1024),
    "Banknote": (lambda n: banknote(n, 131), 1024),
    "Painting": (lambda n: painting(n, 141), 2048),
    # Room surfaces
    "FloorPlanks": (lambda n: floor_planks(n, 151), 2048),
    "Wallpaper": (lambda n: wallpaper(n, 161), 1024),
    "Concrete": (lambda n: concrete(n, 171), 1024),
}


def save_png(path, rgb, colorspace_srgb):
    import bpy

    h, w = rgb.shape[:2]
    rgba = np.ones((h, w, 4), dtype=np.float32)
    rgba[..., :3] = np.clip(rgb, 0.0, 1.0)
    name = os.path.splitext(os.path.basename(path))[0]
    img = bpy.data.images.new(name, w, h, alpha=False, float_buffer=False)
    img.colorspace_settings.name = "sRGB" if colorspace_srgb else "Non-Color"
    img.pixels.foreach_set(rgba.ravel())
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    bpy.data.images.remove(img)


def build_all(out_dir, only=None, log=print):
    os.makedirs(out_dir, exist_ok=True)
    for name, (builder, size) in TEXTURES.items():
        if only and name not in only:
            continue
        color, height, rough, metal, nstrength = builder(size)
        ao = cavity(height, 0.6)
        normal = normal_from_height(height, nstrength * size / 1024.0)
        orm = np.stack([ao, rough, metal], axis=-1)
        save_png(os.path.join(out_dir, f"T_{name}_BC.png"), color * ao[..., None] ** 0.5, True)
        save_png(os.path.join(out_dir, f"T_{name}_N.png"), normal, False)
        save_png(os.path.join(out_dir, f"T_{name}_ORM.png"), orm, False)
        log(f"texture {name} {size}px")
