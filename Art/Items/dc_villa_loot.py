"""G7 – new loot for the villa: gold bar, wine bottle, silverware case, watch box, flour tin.

All items lie or stand the way they are found (origin bottom centre, front = -Y, width along X, real cm).
"""

import math

import bmesh

from dc_items import escutcheon, item
from dc_mesh import CM, Item, arc_points, circle_profile, cube, cylinder, lathe, ngon_prism, rect_profile, sweep, tapered_box, wobble

# ---------------------------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------------------------


def disc(r, n=24):
    """Single flat polygon (circle) at z=0 facing +Z – dials, sub-dials."""
    bm = bmesh.new()
    vs = [bm.verts.new((r * CM * math.cos(2 * math.pi * i / n), r * CM * math.sin(2 * math.pi * i / n), 0.0)) for i in range(n)]
    bm.faces.new(vs)
    return bm


def ring(r_in, r_out, h, n=24):
    """Flat ring (bezel, painted line) from z=0 to z=h."""
    return lathe([(r_in, 0), (r_out, 0), (r_out, h), (r_in, h), (r_in, 0)], n, caps=False)


def shell_patch(r, t, z0, z1, a0, a1, steps=8):
    """Curved slab around Z: radius r..r+t, angles a0..a1 (degrees), z0..z1 (cm) – labels, letters."""
    bm = bmesh.new()
    rings = []
    for i in range(steps + 1):
        a = math.radians(a0 + (a1 - a0) * i / steps)
        c, s = math.cos(a), math.sin(a)
        rings.append([bm.verts.new((rr * c * CM, rr * s * CM, z * CM)) for rr, z in ((r, z0), (r + t, z0), (r + t, z1), (r, z1))])
    for a, b in zip(rings, rings[1:]):
        for k in range(4):
            bm.faces.new((a[k], a[(k + 1) % 4], b[(k + 1) % 4], b[k]))
    bm.faces.new(rings[0])
    bm.faces.new(list(reversed(rings[-1])))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def tray(sx, sy, h, wall, floor):
    """Box (centred, z -h/2..h/2) with an open cavity: walls `wall` thick, cavity floor `floor` above the bottom."""
    bm = cube(sx, sy, h)
    top = max(bm.faces, key=lambda f: f.calc_center_median().z)
    bmesh.ops.inset_region(bm, faces=[top], thickness=wall * CM, depth=0.0, use_even_offset=True)
    bmesh.ops.inset_region(bm, faces=[top], thickness=0.0, depth=-(h - floor) * CM)
    return bm


def uv_scale(ob, su, sv):
    """Scale a part's UVs (finer wood grain on small boxes: the material tiling is made for furniture)."""
    for d in ob.data.uv_layers[0].data:
        d.uv = (d.uv[0] * su, d.uv[1] * sv)
    return ob


def lcg(seed):
    """Tiny deterministic random stream (0..1)."""
    k = seed
    while True:
        k = (k * 1103515245 + 12345) & 0x7FFFFFFF
        yield (k % 10007) / 10007.0


def text_blocks(u0, u1, seed, min_w=0.22, max_w=0.5, gap=0.1, word_gap=0.3):
    """Letter-sized spans filling u0..u1 – suggested lettering without real text."""
    rnd = lcg(seed)
    out, u = [], u0
    while u < u1 - 1e-6:
        w = min_w + (max_w - min_w) * next(rnd)
        if u + w > u1:
            w = u1 - u
            if w < min_w * 0.6:
                break
        out.append((u, u + w))
        u += w + (word_gap if next(rnd) < 0.2 else gap)
    return out


SEG7 = {"0": "abcdef", "2": "abged", "3": "abgcd", "4": "fgbc", "5": "afgcd", "6": "afgedc", "7": "abc", "8": "abcdefg", "9": "abcdfg",
        # a few letters for pseudo lettering (no real words)
        "A": "abcefg", "C": "adef", "E": "adefg", "F": "aefg", "H": "bcefg", "L": "def", "O": "abcdef", "P": "abefg", "U": "bcdef",
        "n": "ceg", "r": "eg", "o": "cdeg", "t": "defg", "d": "bcdeg", "b": "cdefg"}


def glyph(ch, w, h, t):
    """Strokes of a simple segment-style character as rectangles (u0, u1, v0, v1) in cm, baseline v=0."""
    if ch == ",":
        return [(w * 0.15, w * 0.15 + t, -t * 1.8, t)]
    if ch == "1":
        c = w * 0.62
        return [(c - t / 2, c + t / 2, 0, h), (c - t / 2 - w * 0.3, c - t / 2, h - t, h)]
    if ch == "g":  # small 9 hanging below the baseline
        return [(u0, u1, v0 - h * 0.3, v1 - h * 0.3) for u0, u1, v0, v1 in glyph("9", w * 0.92, h * 0.74, t)]
    m = h / 2
    seg = {"a": (0, w, h - t, h), "d": (0, w, 0, t), "g": (t, w - t, m - t / 2, m + t / 2), "f": (0, t, m, h - t),
           "b": (w - t, w, m, h - t), "e": (0, t, t, m), "c": (w - t, w, t, m)}
    return [seg[s] for s in SEG7.get(ch, "")]


def text_rects(text, w, h, t, pitch, space):
    """Rectangles for a whole string, centred on u=0."""
    rects, u = [], 0.0
    for ch in text:
        if ch == " ":
            u += space
            continue
        cw = w * 0.92 if ch == "g" else (w * 0.4 if ch == "," else w)
        rects += [(u + a, u + b, c, d) for a, b, c, d in glyph(ch, w, h, t)]
        u += (cw if ch != "," else w * 0.45) + (pitch - w)
    width = u - (pitch - w)
    return [(a - width / 2, b - width / 2, c, d) for a, b, c, d in rects]


# ---------------------------------------------------------------------------------------------
# Gold bar
# ---------------------------------------------------------------------------------------------


@item("gold_bar")
def gold_bar(it: Item):
    """Cast 1 kg bar: trapezoid section, soft edges, stamped field on top with weight and fineness."""
    L, W, H = 11.8, 5.4, 1.8  # bevel takes ~0.1 per side -> 11.6 x 5.2
    top_l, top_w = 10.5, 4.3
    border, slope, depth = 0.4, 0.21, 0.1  # slope < 30 deg: the bevel leaves the stamp edges alone
    bm = tapered_box(top_l, top_w, L, W, H)
    top = max(bm.faces, key=lambda f: f.calc_center_median().z)
    bmesh.ops.inset_region(bm, faces=[top], thickness=border * CM, depth=0.0, use_even_offset=True)
    bmesh.ops.inset_region(bm, faces=[top], thickness=slope * CM, depth=-depth * CM, use_even_offset=True)
    it.add(bm, "Gold", bevel=0.32, seg=3, grain="x")
    zf = H - depth
    relief = 0.08

    def raised(u0, u1, v0, v1, x0=0.0, y0=0.0):
        it.add(cube(u1 - u0, v1 - v0, relief), "Gold", loc=(x0 + (u0 + u1) / 2, y0 + (v0 + v1) / 2, zf + relief / 2), bevel=0.018, seg=1)

    for r in text_rects("1000 g", 0.6, 0.95, 0.15, 0.8, 0.32):
        raised(*r, y0=0.14)
    for r in text_rects("999,9", 0.46, 0.72, 0.13, 0.63, 0.3):
        raised(*r, y0=-1.2)
    raised(-3.9, 3.9, -0.03, 0.03, y0=-0.25)  # dividing line
    for sx in (-1, 1):  # little stars of the (fictional) assayer next to the weight
        for a in (0, 60, 120):
            it.add(cube(0.42, 0.08, relief), "Gold", loc=(sx * 3.55, 0.62, zf + relief / 2), rot=(0, 0, a))


# ---------------------------------------------------------------------------------------------
# Wine bottle (lying, neck towards +X)
# ---------------------------------------------------------------------------------------------

# The bottle is modelled standing (axis = Z, 0..32) and laid down: local Z -> world +X, local -X -> world up.
BOTTLE = dict(loc=(-16.0, 0.0, 4.0), rot=(0, 90, 0))
LABEL_R = 4.0


def bottle_patch(it, mat, u0, u1, z0, z1, r, t, shade="flat"):
    """Patch on the bottle around the label centre (local -X = world up); u = arc length in cm."""
    a0 = 180.0 + math.degrees(u0 / r)
    a1 = 180.0 + math.degrees(u1 / r)
    steps = max(1, int(abs(a1 - a0) / 9))
    it.add(shell_patch(r, t, z0, z1, a0, a1, steps), mat, shade=shade, **BOTTLE)


@item("wine_bottle")
def wine_bottle(it: Item):
    """Old Bordeaux bottle lying on its side: dark glass, wine, yellowed label, red wax capsule with drips."""
    glass = [(0, 2.4), (1.0, 2.1), (1.9, 1.3), (2.6, 0.55), (3.1, 0.15), (3.55, 0.0), (3.85, 0.12), (3.98, 0.45), (4.0, 0.9),
             (4.0, 20.2), (3.96, 20.9), (3.82, 21.6), (3.55, 22.3), (3.1, 23.0), (2.55, 23.6), (2.0, 24.2), (1.68, 24.8),
             (1.58, 25.4), (1.5, 27.0), (1.45, 29.6), (1.6, 29.8), (1.66, 30.4), (1.62, 31.2), (1.45, 31.6), (1.1, 31.75),
             (0, 31.75)]
    it.add(lathe(glass, 32), "Glass_Bottle", shade="smooth", grain="z", **BOTTLE)
    wine = [(0, 2.65), (1.0, 2.35), (1.85, 1.6), (2.55, 0.8), (3.1, 0.4), (3.5, 0.28), (3.74, 0.5), (3.78, 0.95), (3.78, 20.1),
            (3.73, 20.8), (3.58, 21.5), (3.3, 22.2), (2.85, 22.9), (2.3, 23.5), (1.78, 24.1), (1.42, 24.7), (0, 24.7)]
    it.add(lathe(wine, 24), "Lacquer_Black", shade="smooth", **BOTTLE)
    # Wax capsule dipped over cork and lip (lumpy), with drips running down the neck.
    wax = [(1.4, 27.45), (1.52, 27.5), (1.6, 27.75), (1.62, 28.4), (1.63, 29.3), (1.76, 29.65), (1.86, 30.3), (1.86, 31.1),
           (1.72, 31.65), (1.42, 31.92), (0.8, 32.0), (0, 32.0)]
    it.add(lathe(wax, 32), "Lacquer_Red", shade="smooth", post=wobble(0.025, 1.2, 4), **BOTTLE)
    for a, length in ((15, 0.9), (80, 0.5), (150, 1.05), (205, 0.6), (262, 0.85), (320, 0.45)):
        c, s = math.cos(math.radians(a)), math.sin(math.radians(a))
        r_neck = 1.5
        path = [(r_neck * c, r_neck * s, 28.0 - length * f) for f in (0.0, 0.35, 0.7, 0.92, 1.0)]
        it.add(sweep(path, circle_profile(0.36, 10, sx=1.0, sy=0.42), up=(c, s, 0), scales=[1.25, 0.85, 0.72, 0.6, 0.3]),
               "Lacquer_Red", shade="smooth", **BOTTLE)
    paper = "Ivory"  # yellowed paper
    ink = "Plastic_Black"
    # Neck label with gilt edges.
    neck = lambda z: 1.58 + (1.5 - 1.58) * (z - 25.4) / 1.6  # noqa: E731 – neck radius between z 25.4 and 27.0
    z0, z1 = 25.45, 26.75
    it.add(lathe([(neck(z0) + 0.03, z0), (neck(z1) + 0.03, z1), (neck(z1) - 0.05, z1), (neck(z0) - 0.05, z0), (neck(z0) + 0.03, z0)],
                 32, caps=False), paper, shade="smooth", **BOTTLE)
    for z in (z0 + 0.05, z1 - 0.13):
        it.add(lathe([(neck(z) + 0.04, z), (neck(z + 0.08) + 0.04, z + 0.08), (neck(z + 0.08), z + 0.08), (neck(z), z), (neck(z) + 0.04, z)],
                     32, caps=False), "Gilt", shade="smooth", **BOTTLE)
    # Main label facing up: paper, gilt frame, small engraving of an estate, title, red band, vintage.
    r0, t0 = LABEL_R + 0.005, 0.03
    r1, t1 = LABEL_R + 0.035, 0.015
    hw = 4.6

    def mark(mat, u0, u1, v0, v1):
        bottle_patch(it, mat, u0, u1, v0, v1, r1, t1)

    bottle_patch(it, paper, -hw, hw, 5.0, 15.0, r0, t0, shade="smooth")
    mark("Gilt", -hw + 0.32, hw - 0.32, 14.5, 14.62)
    mark("Gilt", -hw + 0.32, hw - 0.32, 5.38, 5.5)
    for u in (-hw + 0.32, hw - 0.44):
        mark("Gilt", u, u + 0.12, 5.38, 14.62)
    for u0, u1 in text_blocks(-1.7, 1.7, 3, 0.13, 0.22, 0.06, 0.2):  # small top line
        mark(ink, u0, u1, 13.75, 13.95)
    # Estate engraving: ground, house with roof, two towers, windows, door.
    lw = 0.06
    mark(ink, -2.4, 2.4, 11.0, 11.0 + lw)
    for u in (-1.15, 1.15 - lw):
        mark(ink, u, u + lw, 11.0, 12.05)
    mark(ink, -1.3, 1.3, 12.05, 12.05 + lw)
    mark(ink, -0.9, 0.9, 12.4, 12.4 + lw)
    mark(ink, -0.1, 0.1, 12.45, 12.75)
    for side in (-1, 1):
        for u in (1.25, 1.75 - lw):
            mark(ink, side * u - (lw if side < 0 else 0), side * u + (0 if side < 0 else lw), 11.06, 12.55)
        mark(ink, min(side * 1.2, side * 1.8), max(side * 1.2, side * 1.8), 12.55, 12.55 + lw)
        mark(ink, min(side * 1.35, side * 1.65), max(side * 1.35, side * 1.65), 12.8, 12.8 + lw)
        for u in (0.45, 0.8):
            mark(ink, side * u - 0.07, side * u + 0.07, 11.55, 11.75)
        mark(ink, side * 1.5 - 0.06, side * 1.5 + 0.06, 11.8, 12.0)
    mark(ink, -0.14, 0.14, 11.06, 11.45)
    for u0, u1, v0, v1 in text_rects("CAHEL PUOrE", 0.36, 0.56, 0.075, 0.5, 0.3):  # title (pseudo lettering)
        mark(ink, u0, u1, 10.05 + v0, 10.05 + v1)
    mark("Lacquer_Red", -hw + 0.6, hw - 0.6, 9.55, 9.78)
    for u0, u1, v0, v1 in text_rects("1961", 0.58, 1.15, 0.14, 0.78, 0.3):  # vintage
        mark(ink, u0, u1, 7.95 + v0, 7.95 + v1)
    for z, w, seed in ((7.05, 2.9, 5), (6.35, 2.1, 7)):
        for u0, u1 in text_blocks(-w, w, seed, 0.12, 0.22, 0.05, 0.18):
            mark(ink, u0, u1, z, z + 0.2)


# ---------------------------------------------------------------------------------------------
# Silverware case
# ---------------------------------------------------------------------------------------------


@item("silverware_case")
def silverware_case(it: Item):
    """Closed mahogany cutlery chest: walnut inlay with brass line, brass corners, clasp, hinges, folding handle."""
    wood = "Wood_Mahogany"
    D = 29.4  # depth of the wooden body; corner caps reach 29.7, hinges/clasp ~30.2
    uv_scale(it.add(cube(44.6, D - 0.1, 5.7), wood, loc=(0, 0, 2.85), bevel=0.35, seg=3, grain="x"), 1.6, 3.0)
    it.add(cube(44.2, D - 0.5, 0.3), "Plastic_Black", loc=(0, 0, 5.85))  # lid gap
    lid_top = 8.35
    uv_scale(it.add(cube(44.7, D, lid_top - 6.0), wood, loc=(0, 0, (lid_top + 6.0) / 2), bevel=0.7, seg=4, profile=0.7, grain="x"), 1.6, 3.0)
    # Inlay: walnut field framed by a brass line.
    hx, hy = 19.35, D / 2 - 3.0
    uv_scale(it.add(cube(2 * hx - 0.22, 2 * hy - 0.22, 0.03), "Wood_Walnut", loc=(0, 0, lid_top + 0.015), grain="x"), 1.6, 3.0)
    it.add(sweep([(-hx, -hy, lid_top), (hx, -hy, lid_top), (hx, hy, lid_top), (-hx, hy, lid_top)], rect_profile(0.22, 0.07),
                 closed=True), "Brass", shade="flat")
    # Brass corner caps (lid top corners and bottom corners).
    fy = D / 2  # front/back face of the lid
    for sx in (-1, 1):
        for sy in (-1, 1):
            for cx, cy, z0, h, leg in ((22.5, fy + 0.15, 5.85, lid_top + 0.12 - 5.85, 3.2), (22.45, fy + 0.1, 0.0, 2.2, 2.8)):
                pts = [(sx * cx, sy * cy), (sx * (cx - leg), sy * cy), (sx * cx, sy * (cy - leg))]
                it.add(ngon_prism(pts, h), "Brass", loc=(0, 0, z0), bevel=0.12, seg=2)
    # Folding bail handle lying flat on the lid, with its two mounts.
    zc = lid_top + 0.3
    path = ([(-6.5, 0.0, zc)] + arc_points((-5.5, -2.3, zc), 1.0, 180, 270, 5) + arc_points((5.5, -2.3, zc), 1.0, 270, 360, 5)
            + [(6.5, 0.0, zc)])
    it.add(sweep(path, circle_profile(0.27, 12)), "Brass", shade="smooth")
    for sx in (-1, 1):
        it.add(cube(1.5, 1.9, 0.12), "Brass", loc=(sx * 6.5, 0, lid_top + 0.06), bevel=0.05, seg=1)
        it.add(cylinder(0.34, 0.9, 16), "Brass", loc=(sx * 6.5 - 0.45, 0, zc), rot=(0, 90, 0), shade="smooth")
    # Name plate (engraved lettering suggested) behind the handle.
    it.add(cube(7.0, 2.1, 0.08), "Brass", loc=(0, 6.2, lid_top + 0.04), bevel=0.03, seg=1)
    for u0, u1 in text_blocks(-2.2, 2.2, 21, 0.26, 0.4, 0.12, 0.35):
        it.add(cube(u1 - u0, 0.5, 0.012), "Iron_Dark", loc=((u0 + u1) / 2, 6.2, lid_top + 0.085))
    # Clasp at the front: catch with keyhole on the box, hasp hanging from the lid.
    by = (D - 0.1) / 2  # front face of the box
    it.add(cube(2.8, 0.16, 2.3), "Brass", loc=(0, -by - 0.08, 4.25), bevel=0.06, seg=1)
    escutcheon(it, (0, -by - 0.16, 4.0), "-y", size=0.5)
    it.add(cube(2.1, 0.22, 2.3), "Brass", loc=(0, -fy - 0.11, 6.75), bevel=0.07, seg=1)
    it.add(cylinder(0.22, 2.1, 12), "Brass", loc=(-1.05, -fy - 0.2, 7.9), rot=(0, 90, 0), shade="smooth")
    it.add(cube(0.9, 0.12, 0.5), "Iron_Dark", loc=(0, -fy - 0.2, 6.0))  # slot of the hasp
    # Hinges at the back.
    for sx in (-1, 1):
        it.add(cube(4.0, 0.12, 2.6), "Brass", loc=(sx * 12, fy + 0.04, 5.95), bevel=0.04, seg=1)
        it.add(cylinder(0.24, 4.0, 12), "Brass", loc=(sx * 12 - 2.0, fy + 0.1, 5.95), rot=(0, 90, 0), shade="smooth")


# ---------------------------------------------------------------------------------------------
# Watch box
# ---------------------------------------------------------------------------------------------

PILLOW_A, PILLOW_B = 3.0, 2.15  # half axes of the pillow section (Y, Z)


def bracelet_profile(sign):
    """Metal bracelet section (radial x, width y) with a raised centre link."""
    pts = [(-0.1, -0.78), (0.08, -0.78), (0.1, -0.7), (0.1, -0.32), (0.16, -0.27), (0.16, 0.27), (0.1, 0.32), (0.1, 0.7), (0.08, 0.78),
           (-0.1, 0.78)]
    return [(x * sign, y) for x, y in pts]


def watch_on_pillow(it, x, y, z_floor, case, dial, strap, ink="Iron_Dark", style="dress", pillow="Velvet_Red"):
    """Pillow (axis X) with a small wrist watch strapped around it, dial up, 12 o'clock towards +Y."""
    A, B = PILLOW_A, PILLOW_B
    zc = z_floor + B
    xs = [-3.75, -3.6, -3.3, 3.3, 3.6, 3.75]
    it.add(sweep([(x + v, y, zc) for v in xs], circle_profile(1.0, 12, sx=A, sy=B), scales=[0.62, 0.88, 1.0, 1.0, 0.88, 0.62]),
           pillow, shade="auto", angle=50)
    # Strap / bracelet around the pillow (section in the YZ plane).
    z0 = zc + B - 0.12  # case bottom, sunk a little into the pillow
    a_, b_ = A + 0.1, B + 0.1
    lug_y = 2.05
    pts = [(x, y + lug_y, z0 + 0.32)]
    for k in range(19):
        t = math.radians(50 - 280 * k / 18)
        pts.append((x, y + a_ * math.cos(t), zc + b_ * math.sin(t)))
    pts.append((x, y - lug_y, z0 + 0.32))
    if strap in ("Silver", "Gold"):
        prof = bracelet_profile(-1)
    else:
        prof = rect_profile(0.2, 1.6)
    it.add(sweep(pts, prof, up=(1, 0, 0)), strap, shade="smooth")
    # Case, lugs and crown.
    R = 1.55
    it.add(lathe([(0, 0), (R, 0), (R + 0.22, 0.18), (R + 0.27, 0.48), (R + 0.14, 0.74), (R + 0.02, 0.78), (0, 0.78)], 24),
           case, loc=(x, y, z0), shade="auto", angle=40)
    for sy in (-1, 1):
        for lx in (-0.94, 0.94):
            it.add(cube(0.26, 0.66, 0.34), case, loc=(x + lx, y + sy * 1.85, z0 + 0.3))
    it.add(cylinder(0.2, 0.32, 10), case, loc=(x + R + 0.18, y, z0 + 0.38), rot=(0, 90, 0), shade="smooth")
    zd = z0 + 0.79
    it.add(disc(R, 24), dial, loc=(x, y, zd))
    if style == "diver":  # rotating bezel with a marker dot
        it.add(ring(R - 0.05, R + 0.24, 0.14, 24), "Lacquer_Black", loc=(x, y, z0 + 0.7), shade="auto", angle=40)
        it.add(cube(0.16, 0.16, 0.03), "Porcelain", loc=(x, y + R + 0.08, z0 + 0.855))
    if style == "chrono":
        for dx, dy in ((-0.6, 0.0), (0.6, 0.0), (0.0, -0.62)):
            it.add(disc(0.38, 12), "Lacquer_Black", loc=(x + dx * 1.08, y + dy * 1.08, zd + 0.005))
        for sy in (-1, 1):  # pushers
            it.add(cylinder(0.12, 0.3, 8), case, loc=(x + R + 0.05, y + sy * 0.75, z0 + 0.38), rot=(0, 90, 0), shade="smooth")
    n = 12 if style == "dress" else 4
    for k in range(n):
        a = 2 * math.pi * k / n
        major = (k * 12 // n) % 3 == 0
        length = 0.34 if major else 0.2
        it.add(cube(0.08 if not major else 0.11, length, 0.03), ink, loc=(x + 1.22 * math.sin(a), y + 1.22 * math.cos(a), zd + 0.015),
               rot=(0, 0, -math.degrees(a)))
    for angle, length, width in ((305, 0.8, 0.14), (62, 1.2, 0.1)):
        a = math.radians(angle)
        it.add(cube(width, length, 0.03), ink, loc=(x + 0.4 * length * math.sin(a), y + 0.4 * length * math.cos(a), zd + 0.05),
               rot=(0, 0, -angle))


@item("watch_box")
def watch_box(it: Item):
    """Leather watch box with glass lid; six watches on red velvet pillows, visible from above."""
    leather = "Leather_Black"
    lining = "Velvet_Red"
    base_h = 6.4
    it.add(tray(30, 20, base_h, 1.0, 1.0), leather, loc=(0, 0, base_h / 2), bevel=0.3, seg=3)
    # Lining and dividers.
    it.add(cube(27.9, 17.9, 0.25), lining, loc=(0, 0, 1.125))
    for sy in (-1, 1):
        it.add(cube(27.8, 0.15, 5.0), lining, loc=(0, sy * 8.92, 3.6))
    for sx in (-1, 1):
        it.add(cube(0.15, 17.7, 5.0), lining, loc=(sx * 13.92, 0, 3.6))
    zf = 1.25
    for sx in (-1, 1):
        it.add(cube(0.6, 17.6, 3.3), lining, loc=(sx * 4.67, 0, zf + 1.65), bevel=0.22, seg=2)
    it.add(cube(27.6, 0.6, 3.3), lining, loc=(0, 0, zf + 1.65), bevel=0.22, seg=2)
    # Six watches: (case, dial, strap, ink, style)
    watches = [
        ("Gold", "Enamel_Cream", "Leather_Black", "Iron_Dark", "dress"),
        ("Silver", "Lacquer_Black", "Silver", "Porcelain", "diver"),
        ("Gold", "Gilt", "Gold", "Iron_Dark", "plain"),
        ("Silver", "Porcelain", "Leather_Brown", "Iron_Dark", "chrono"),
        ("Silver", "Porcelain_Blue", "Silver", "Silver", "plain"),
        ("Copper", "Paint_Green", "Leather_Brown", "Gold", "dress"),
    ]
    for k, (case, dial, strap, ink, style) in enumerate(watches):
        col, row = k % 3, k // 3
        watch_on_pillow(it, (col - 1) * 9.33, 4.5 if row == 0 else -4.5, zf, case, dial, strap, ink, style, pillow=lining)
    # Lid: leather frame (walls + top border) swept around, glass pane, cream stitching.
    lid_h = 10.0 - base_h
    frame_prof = [(0.5, 0), (0.5, lid_h), (-1.5, lid_h), (-1.5, lid_h - 0.55), (-0.5, lid_h - 0.55), (-0.5, 0)]
    hx, hy = 14.5, 9.5
    it.add(sweep([(-hx, -hy, base_h), (hx, -hy, base_h), (hx, hy, base_h), (-hx, hy, base_h)], frame_prof, closed=True), leather,
           bevel=0.22, seg=2)
    it.add(cube(27.2, 17.2, 0.2), "Glass_Tinted", loc=(0, 0, 10.0 - 0.55 - 0.1))
    sx_, sy_ = 14.42, 9.42
    it.add(sweep([(-sx_, -sy_, 10.0), (sx_, -sy_, 10.0), (sx_, sy_, 10.0), (-sx_, sy_, 10.0)], rect_profile(0.07, 0.04), closed=True),
           "Fabric_Cream", shade="flat")
    # Clasp at the front, hinges at the back.
    it.add(cube(2.6, 0.18, 0.9), "Chrome", loc=(0, -10.06, 5.45), bevel=0.06, seg=1)
    it.add(cube(1.9, 0.22, 1.5), "Chrome", loc=(0, -10.08, 6.75), bevel=0.08, seg=2)
    it.add(cube(0.8, 0.1, 0.4), "Plastic_Black", loc=(0, -10.2, 6.5))
    for sx in (-1, 1):
        it.add(cylinder(0.28, 3.0, 12), "Chrome", loc=(sx * 9 - 1.5, 9.92, base_h), rot=(0, 90, 0), shade="smooth")


# ---------------------------------------------------------------------------------------------
# Flour tin
# ---------------------------------------------------------------------------------------------


def tangent_box(it, mat, r, theta_deg, z, size, tilt=0.0, bevel=0.0):
    """Box lying on a cylinder (axis Z) at angle theta: size = (along the tangent, radial, height)."""
    a = math.radians(theta_deg)
    it.add(cube(*size), mat, loc=(r * math.cos(a), r * math.sin(a), z), rot=(0, tilt, theta_deg + 90), bevel=bevel, seg=1)


@item("flour_tin")
def flour_tin(it: Item):
    """Cream enamel storage tin with blue rims, lid with wooden knob, raised lettering 'MEHL' at the front."""
    enamel, rim = "Enamel_Cream", "Porcelain_Blue"
    R = 7.25
    it.add(lathe([(0, 0), (6.8, 0), (7.15, 0.08), (7.33, 0.3), (7.36, 0.6), (7.3, 0.85), (7.22, 0.95), (0, 0.95)], 48), rim,
           shade="auto", angle=40)
    it.add(lathe([(0, 0.9), (7.2, 0.9), (R, 1.05), (R, 17.6), (7.1, 18.0), (6.9, 18.3), (0, 18.3)], 48), enamel, shade="auto", angle=40,
           grain="z")
    for z in (2.0, 15.9):  # painted lines
        it.add(ring(R - 0.05, R + 0.03, 0.22, 48), rim, loc=(0, 0, z), shade="smooth")
    # Lid: blue rolled edge, cream dome, turned wooden knob.
    it.add(lathe([(7.29, 17.0), (7.42, 17.0), (7.52, 17.2), (7.52, 17.6), (7.47, 17.8), (7.29, 17.8), (7.29, 17.0)], 48, caps=False),
           rim, shade="smooth")
    it.add(lathe([(0, 18.6), (7.29, 18.6), (7.29, 17.8), (7.47, 17.8), (7.5, 19.5), (7.4, 19.85), (7.1, 20.05), (6.2, 20.25),
                  (3.5, 20.42), (0, 20.48)], 48), enamel, shade="auto", angle=40)
    it.add(lathe([(7.36, 19.35), (7.53, 19.5), (7.44, 19.87), (7.13, 20.08), (6.75, 20.17), (6.75, 19.95), (7.2, 19.7), (7.36, 19.35)], 48,
                 caps=False), rim, shade="smooth")  # blue edge round the top of the lid
    it.add(lathe([(0, 0), (1.1, 0), (1.15, 0.15), (0.65, 0.45), (0.6, 0.7), (1.2, 1.0), (1.3, 1.25), (1.05, 1.5), (0, 1.6)], 32),
           "Wood_Walnut", loc=(0, 0, 20.4), shade="smooth", grain="z")
    # Raised letters MEHL at the front (-Y): stems/bars as rectangles, diagonals as tilted boxes on the cylinder.
    H, w = 3.2, 0.42
    letters = {
        "M": (2.6, [(0, w, 0, H), (2.6 - w, 2.6, 0, H)], [((0.3, H - 0.22), (1.3, 0.95)), ((1.3, 0.95), (2.3, H - 0.22))]),
        "E": (1.8, [(0, w, 0, H), (w, 1.8, H - w, H), (w, 1.5, H / 2 - w / 2, H / 2 + w / 2), (w, 1.8, 0, w)], []),
        "H": (2.1, [(0, w, 0, H), (2.1 - w, 2.1, 0, H), (w, 2.1 - w, H / 2 - w / 2, H / 2 + w / 2)], []),
        "L": (1.7, [(0, w, 0, H), (w, 1.7, 0, w)], []),
    }
    gap, base_z, proud = 0.55, 8.0, 0.26
    total = sum(letters[c][0] for c in "MEHL") + gap * 3
    u = -total / 2
    for c in "MEHL":
        width, rects, diagonals = letters[c]
        for u0, u1, v0, v1 in rects:
            theta = -90.0 + math.degrees((u + (u0 + u1) / 2) / R)
            tangent_box(it, rim, R + 0.07, theta, base_z + (v0 + v1) / 2, (u1 - u0, proud, v1 - v0), bevel=0.05)
        for (u0, v0), (u1, v1) in diagonals:
            du, dv = u1 - u0, v1 - v0
            theta = -90.0 + math.degrees((u + (u0 + u1) / 2) / R)
            tangent_box(it, rim, R + 0.07, theta, base_z + (v0 + v1) / 2, (math.hypot(du, dv) + 0.1, proud, w),
                        tilt=-math.degrees(math.atan2(dv, du)), bevel=0.05)
        u += width + gap
    # Worn enamel: chips showing dark steel (on the rims, one on the body).
    for theta, z, r, sx, sz in ((-48, 0.55, 7.37, 0.7, 0.32), (-128, 17.4, 7.53, 0.6, 0.3), (-20, 4.8, R, 0.5, 0.36),
                                (-75, 19.6, 7.52, 0.45, 0.22)):
        rnd = lcg(int(-theta * 7 + z * 13))
        pts = [(sx * (0.55 + 0.45 * next(rnd)) * math.cos(2 * math.pi * k / 9), sz * (0.55 + 0.45 * next(rnd)) * math.sin(2 * math.pi * k / 9))
               for k in range(9)]
        a = math.radians(theta)
        it.add(ngon_prism(pts, 0.05), "Iron_Dark", loc=((r - 0.03) * math.cos(a), (r - 0.03) * math.sin(a), z), rot=(90, 0, theta + 90),
               shade="flat")
