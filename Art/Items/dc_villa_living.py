"""Villa props, group G3 (living room and bathroom): fireplace, bookshelf module, WC, washbasin, washing machine.

Exported as SM_Deco_<name>. Real cm, origin bottom centre, front = Blender -Y, back against the wall = +Y.
"""

import math
import random

import bmesh
from mathutils import Euler, Matrix, Vector

from dc_items import item
from dc_items_small import spline
from dc_mesh import CM, Item, circle_profile, cube, cylinder, lathe, rect_profile, sphere, sweep, torus, wobble

# ---------------------------------------------------------------------------------------------
# Helpers (own to this file)
# ---------------------------------------------------------------------------------------------


def _rot(rot):
    return Euler([math.radians(a) for a in rot]).to_matrix().to_4x4()


class BoxBatch:
    """Many boxes merged into one part per (material, bevel) - keeps the object count low (books)."""

    def __init__(self):
        self.groups = {}

    @staticmethod
    def frame(center, rot=(0, 0, 0)):
        return Matrix.Translation(Vector(center) * CM) @ _rot(rot)

    def add(self, mat, size, center, rot=(0, 0, 0), bevel=0.0, parent=None):
        m = self.frame(center, rot)
        if parent is not None:
            m = parent @ m
        m = m @ Matrix.Diagonal((size[0] * CM, size[1] * CM, size[2] * CM, 1.0))
        self.groups.setdefault((mat, bevel), []).append(m)

    def flush(self, it, grain="z"):
        for (mat, bevel), ms in self.groups.items():
            bm = bmesh.new()
            for m in ms:
                bmesh.ops.create_cube(bm, size=1.0, matrix=m)
            it.add(bm, mat, bevel=bevel, seg=1, grain=grain)
        self.groups = {}


def loft(rings, cap_start=True, cap_end=True):
    """Skin a list of point rings (cm, equal point counts). A ring with a single point is a pole."""
    bm = bmesh.new()
    vr = [[bm.verts.new(Vector(p) * CM) for p in r] for r in rings]
    for a, b in zip(vr, vr[1:]):
        if len(a) == 1 and len(b) == 1:
            continue
        if len(a) == 1:
            for s in range(len(b)):
                bm.faces.new((a[0], b[s], b[(s + 1) % len(b)]))
        elif len(b) == 1:
            for s in range(len(a)):
                bm.faces.new((a[s], b[0], a[(s + 1) % len(a)]))
        else:
            n = len(a)
            for s in range(n):
                bm.faces.new((a[s], a[(s + 1) % n], b[(s + 1) % n], b[s]))
    if cap_start and len(vr[0]) > 2:
        bm.faces.new(list(reversed(vr[0])))
    if cap_end and len(vr[-1]) > 2:
        bm.faces.new(vr[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def ray_rrect(phi, x0, x1, y0, y1, r_lo, r_hi):
    """Where the ray from (0, 0) at angle phi leaves the rounded rectangle [x0, x1] x [y0, y1] (origin inside).

    r_lo: corner radius on the y0 side (front), r_hi on the y1 side (back).
    """
    dx, dy = math.cos(phi), math.sin(phi)
    ts = []
    if dx > 1e-9:
        ts.append(x1 / dx)
    if dx < -1e-9:
        ts.append(x0 / dx)
    if dy > 1e-9:
        ts.append(y1 / dy)
    if dy < -1e-9:
        ts.append(y0 / dy)
    t = min(ts)
    px, py = t * dx, t * dy
    r = r_hi if dy > 0 else r_lo
    if r > 0:
        cx = (x1 - r) if dx > 0 else (x0 + r)
        cy = (y1 - r) if dy > 0 else (y0 + r)
        if ((px > cx) if dx > 0 else (px < cx)) and ((py > cy) if dy > 0 else (py < cy)):
            b = dx * cx + dy * cy
            c = cx * cx + cy * cy - r * r
            t = b + math.sqrt(max(b * b - c, 0.0))
            px, py = t * dx, t * dy
    return px, py


def pan_outline(w, lf, yb, rb):
    """Toilet-like outline: half ellipse in front (-Y, length lf), rounded rectangle at the back up to y = yb."""

    def fn(phi):
        dx, dy = math.cos(phi), math.sin(phi)
        if dy < 0:
            t = 1.0 / math.sqrt((dx / (w / 2)) ** 2 + (dy / lf) ** 2)
            return t * dx, t * dy
        return ray_rrect(phi, -w / 2, w / 2, -lf, yb, 0.0, rb)

    return fn


def ellipse(a, b):
    return lambda phi: (a * math.cos(phi), b * math.sin(phi))


def ring(fn, n, z, cx=0.0, cy=0.0, inset=0.0, scale=1.0):
    """n points of the outline fn(phi) at height z; inset shrinks radially (cm)."""
    out = []
    for i in range(n):
        x, y = fn(2 * math.pi * i / n)
        x, y = x * scale, y * scale
        if inset:
            ln = math.hypot(x, y)
            k = max(ln - inset, 0.01) / max(ln, 1e-6)
            x, y = x * k, y * k
        out.append((cx + x, cy + y, z))
    return out


def facing_y(it, bm, mat, loc, **kw):
    """Add a lathe part (built around +Z) so that it points towards -Y."""
    it.add(bm, mat, loc=loc, rot=(90, 0, 0), **kw)


# ---------------------------------------------------------------------------------------------
# Fireplace
# ---------------------------------------------------------------------------------------------


def _log(it, length, r, center, yaw, tilt=0.0, seed=0, stub=None):
    """Firewood log lying roughly along X: furrowed bark mantle, light end grain, optional branch stub (cm along)."""
    L = length
    prof = [(0, -L / 2), (r * 0.86, -L / 2), (r * 0.96, -L / 2 + 0.7)]
    steps = max(int(L / 4), 6)
    for i in range(steps + 1):
        z = -L / 2 + 2.0 + (L - 4.0) * i / steps
        prof.append((r * (1.0 + 0.05 * math.sin(i * 1.7 + seed) + 0.03 * math.sin(i * 3.1 + seed * 2)), z))
    prof += [(r * 0.95, L / 2 - 0.7), (r * 0.84, L / 2), (0, L / 2)]
    rot = (0, 90 - tilt, yaw)
    rows = len(prof) - 2
    it.add(lathe(prof, 20, ridges=(10, r * 0.13, 2, rows - 2)), "Bark", loc=center, rot=rot, shade="smooth", grain="x",
           post=[wobble(0.55, 7, seed), wobble(0.3, 3.5, seed + 7)])
    mrot = Euler([math.radians(a) for a in rot]).to_matrix()
    d = mrot @ Vector((0, 0, 1))
    for s in (-1, 1):
        rr = r * 0.84
        p = Vector(center) + d * (s * (L / 2 + 0.05))
        it.add(lathe([(0, -0.25), (rr, -0.25), (rr, 0.25), (0, 0.25)], 20), "Wood_Light", loc=tuple(p), rot=rot, shade="auto",
               post=wobble(0.15, 3, seed))
    if stub:
        along, ang = stub
        side = mrot @ Vector((math.cos(math.radians(ang)), math.sin(math.radians(ang)), 0))
        base = Vector(center) + d * along + side * (r * 0.8)
        tip = base + side * 3.2 + d * 1.2
        it.add(sweep([tuple(base), tuple(tip)], circle_profile(1.3, 10), scales=[1.0, 0.8]), "Bark", shade="smooth")
        it.add(sweep([tuple(tip), tuple(tip + (tip - base).normalized() * 0.3)], circle_profile(1.0, 10)), "Wood_Light", shade="auto")


def _split_log(it, length, r, center, yaw, roll, seed=0):
    """Half-split log: bark on the round side, light split face (normal tilted by roll towards -Y)."""
    L = length
    arc = [(r * math.cos(math.radians(a)), r * math.sin(math.radians(a)) * 0.9) for a in range(180, 361, 15)]
    rot = (roll, 0, yaw)
    it.add(sweep([(-L / 2, 0, 0), (L / 2, 0, 0)], arc, up=(0, 0, 1)), "Bark", loc=center, rot=rot, shade="smooth", grain="x",
           post=[wobble(0.4, 7, seed), wobble(0.25, 3.5, seed + 3)])
    face = cube(L - 0.6, 2 * r - 0.5, 0.3, cuts=3)
    bmesh.ops.translate(face, vec=(0, 0, 0.1 * CM), verts=face.verts)
    it.add(face, "Wood_Light", loc=center, rot=rot, shade="smooth", grain="x", post=wobble(0.12, 5, seed))
    for x in (-L / 2 - 0.2, L / 2 - 0.05):
        it.add(sweep([(x, 0, 0), (x + 0.25, 0, 0)], [(px * 0.9, py * 0.9) for px, py in arc]), "Wood_Light", loc=center,
               rot=rot, shade="auto")


@item("Deco_fireplace")
def fireplace(it: Item):
    """Marble mantel with a dark cast-iron firebox, basket grate and logs. 180 x 45 x 110, back at y = +22.5."""
    m = "Marble"
    back = 22.5
    yf = -10.0  # front plane of the surround body
    # Black stone hearth slab.
    it.add(cube(172, 45, 4), "Lacquer_Black", loc=(0, 0, 2), bevel=0.6, seg=3)
    # Pilasters with plinth blocks, raised panels and capitals.
    for sx in (-1, 1):
        x = sx * 65
        it.add(cube(30, back - yf, 90), m, loc=(x, (back + yf) / 2, 49), bevel=0.4, seg=2, grain="z")
        it.add(cube(34, back + 12, 14), m, loc=(x, (back - 12) / 2, 11), bevel=0.8, seg=3)
        it.add(cube(33, 1.0, 1.6), m, loc=(x, -12.3, 18.6), bevel=0.5, seg=3)  # plinth top moulding
        it.add(cube(21, 1.2, 62), m, loc=(x, yf - 0.6, 52), bevel=0.7, seg=3, profile=0.3, grain="z")
        it.add(cube(16, 0.6, 56), m, loc=(x, yf - 1.3, 52), bevel=0.3, seg=2, grain="z")
        it.add(cube(34, back + 12, 7), m, loc=(x, (back - 12) / 2, 90.5), bevel=0.9, seg=3, profile=0.7)
        rosette = lathe([(0, 0), (3.4, 0), (3.5, 0.4), (3.1, 0.9), (2.2, 1.2), (1.3, 1.5), (0.9, 2.0), (0, 2.1)], 40,
                        ridges=(8, 0.55, 1, 4))
        facing_y(it, rosette, m, (x, -12.0, 90.5), shade="smooth")
    # Frieze with a raised centre tablet.
    it.add(cube(104, back - yf, 18), m, loc=(0, (back + yf) / 2, 85), bevel=0.3, seg=2)
    it.add(cube(42, 1.2, 11), m, loc=(0, yf - 0.6, 84.5), bevel=0.6, seg=3, profile=0.3)
    it.add(cube(36, 0.5, 6.5), m, loc=(0, yf - 1.35, 84.5), bevel=0.25, seg=2)
    # Cornice (cove) around front and sides, then the mantel shelf.
    cove = [(-2.0, 0.0), (0.0, 0.0), (0.0, 1.4)]
    for i in range(1, 9):
        a = math.radians(180 - 90 * i / 8)
        cove.append((6.0 + 6.0 * math.cos(a), 1.4 + 6.0 * math.sin(a)))
    cove += [(6.0, 8.0), (6.5, 8.0), (6.5, 10.0), (-2.0, 10.0)]
    path = [(-82, back, 94), (-82, -12, 94), (82, -12, 94), (82, back, 94)]
    it.add(sweep(path, cove, up=(0, 0, 1)), m, shade="auto", angle=40)
    it.add(cube(180, 45, 6), m, loc=(0, 0, 107), bevel=1.8, seg=4)
    # Opening: marble bead around it, cast-iron frame set back.
    bead = [(-2.0, 2.0), (-2.0, 0.0)]
    for i in range(1, 8):
        a = math.radians(180 + 180 * i / 8)
        bead.append((0.5 + 2.5 * math.cos(a), 2.2 * math.sin(a)))
    bead += [(3.0, 0.0), (3.0, 2.0)]
    it.add(sweep([(-50, yf, 4), (-50, yf, 76), (50, yf, 76), (50, yf, 4)], bead, up=(0, 1, 0)), m, shade="smooth")
    it.add(sweep([(-47.5, -9.6, 4), (-47.5, -9.6, 73.5), (47.5, -9.6, 73.5), (47.5, -9.6, 4)], rect_profile(5, 2.4, 0.4),
                 up=(0, 1, 0)), "Iron_Dark", shade="auto")
    # Firebox: back plate with fireback relief, splayed cheeks, top, sooty floor, ash.
    it.add(cube(82, 2, 68), "Iron_Dark", loc=(0, 19.5, 38))
    it.add(cube(56, 1.0, 44), "Iron_Dark", loc=(0, 18.2, 34), bevel=0.8, seg=3, profile=0.3)
    it.add(cube(40, 0.6, 26), "Iron_Dark", loc=(0, 17.5, 34), bevel=0.4, seg=2, profile=0.3)
    ang = math.degrees(math.atan2(5.0, 26.5))
    for sx in (-1, 1):
        it.add(cube(2, 27.5, 68), "Iron_Dark", loc=(sx * 43.5, 5.25, 38), rot=(0, 0, sx * ang))
    it.add(cube(94, 30, 2), "Iron_Dark", loc=(0, 6, 73))
    it.add(cube(90, 27, 0.4), "Plastic_Black", loc=(0, 5.5, 4.2))
    it.add(cube(62, 21, 1.4, cuts=5), "Concrete", loc=(0, 4, 4.6), bevel=0.5, seg=2, shade="smooth", post=wobble(0.35, 9, 3))
    # Basket grate.
    g = "Iron_Dark"
    for sx in (-1, 1):
        post = lathe([(0, 0), (1.7, 0), (1.7, 1.6), (1.1, 2.6), (1.0, 23), (1.45, 24), (1.0, 25.2), (0, 25.4)], 16)
        it.add(post, g, loc=(sx * 26, -7.5, 4), shade="auto", angle=40)
        it.add(sphere(1.9, 16, 8), g, loc=(sx * 26, -7.5, 29.0), shade="smooth")
        it.add(cylinder(1.0, 19, 12), g, loc=(sx * 22, 15, 4), shade="smooth")
        for z in (15.2, 21.5):
            it.add(sweep([(sx * 26, -7.5, z), (sx * 22, 15, z)], circle_profile(0.65, 8)), g, shade="smooth")
    for z in (15.2, 20.0, 24.8):
        pts = [(x, -7.5 - 1.6 * (1 - (x / 26) ** 2), z) for x in (-26, -19.5, -13, -6.5, 0, 6.5, 13, 19.5, 26)]
        it.add(sweep(pts, circle_profile(0.65, 8)), g, shade="smooth")
    for x in range(-20, 21, 5):
        it.add(sweep([(x, -8.5, 14.6), (x, 15, 14.6)], circle_profile(0.75, 8)), g, shade="smooth")
    it.add(sweep([(-22, 15, 21.5), (22, 15, 21.5)], circle_profile(0.65, 8)), g, shade="smooth")
    # Logs on the grate.
    _log(it, 50, 5.4, (-2, -1.8, 20.8), 4, seed=1, stub=(9, 200))
    _log(it, 47, 4.8, (3, 8.8, 20.2), -5, seed=2)
    _split_log(it, 44, 5.0, (-1, 3.2, 28.2), 14, 32, seed=3)


# ---------------------------------------------------------------------------------------------
# Bookshelf module
# ---------------------------------------------------------------------------------------------

BOOK_COLOURS = [("Leather_Brown", 2.8), ("Leather_Green", 2.0), ("Fabric_Navy", 1.4), ("Velvet_Red", 1.8),
                ("Leather_Black", 1.6), ("Fabric_Olive", 1.3), ("Fabric_Red", 1.0), ("Fabric_Blue", 0.5),
                ("Terracotta", 0.6), ("Fabric_Cream", 0.6), ("Paint_Yellow", 0.3)]
LEATHER = {"Leather_Brown", "Leather_Green", "Leather_Black", "Velvet_Red"}
TITLE_FIELD = {"Leather_Brown": "Velvet_Red", "Leather_Green": "Leather_Black", "Leather_Black": "Velvet_Red",
               "Velvet_Red": "Leather_Black"}


def _pick_colour(rng, avoid):
    total = sum(w for _, w in BOOK_COLOURS)
    while True:
        r = rng.uniform(0, total)
        for name, w in BOOK_COLOURS:
            r -= w
            if r <= 0:
                break
        if name != avoid:
            return name


def _spine_style(rng, colour):
    if colour in LEATHER:
        return "leather"
    r = rng.random()
    return "label" if r < 0.2 else ("gilt" if r < 0.75 else None)


def _book(batch, mat, t, d, h, center, rot=(0, 0, 0), style=None):
    """Upright book: cover box, page block visible on top, spine decoration on the front (-Y)."""
    f = batch.frame(center, rot)
    batch.add(mat, (t, d, h), (0, 0, 0), bevel=0.15, parent=f)
    batch.add("Paper", (max(t - 0.36, 0.3), d - 0.5, 0.12), (0, 0.22, h / 2 + 0.03), parent=f)
    front = -d / 2 - 0.04
    w = t - 0.3
    if style == "leather":
        # Gilt bands with a coloured title field between them, one band at the foot.
        top = h / 2
        batch.add("Gilt", (w, 0.1, 0.25), (0, front, top - 1.5), parent=f)
        batch.add(TITLE_FIELD[mat], (w - 0.1, 0.08, 2.6), (0, front, top - 3.3), parent=f)
        batch.add("Gilt", (w, 0.1, 0.25), (0, front - 0.01, top - 5.1), parent=f)
        batch.add("Gilt", (w, 0.1, 0.25), (0, front, -top + 1.8), parent=f)
    elif style == "gilt" and t > 1.6:
        top = h / 2
        batch.add("Gilt", (t * 0.55, 0.08, 0.3), (0, front, top - 3.0), parent=f)
        batch.add("Gilt", (t * 0.45, 0.08, 0.22), (0, front, top - 3.8), parent=f)
        batch.add("Gilt", (t * 0.35, 0.08, 0.22), (0, front, -top + 2.2), parent=f)
    elif style == "label" and t > 2.0:
        batch.add("Paper", (t * 0.6, 0.08, 1.8), (0, front, h / 2 - 4.0), parent=f)


def _fill_compartment(batch, rng, x0, x1, zb, hc, yfb):
    """Fill one shelf compartment left to right with book runs, a lying stack, books lying on top and a leaning book."""
    x = x0 + rng.uniform(0.0, 0.8)
    prev = None
    last_h = None
    stacks = 0
    while True:
        room = x1 - x
        if room < 1.8:
            break
        r = rng.random()
        if stacks < 1 and r < 0.09 and room > 28 and hc > 24:
            # Lying stack: book height along X, spines to the front.
            n = rng.randint(2, 5)
            lh = rng.uniform(21, min(29, room - 2))
            z = zb
            width = lh
            colour = prev
            for k in range(n):
                lt = rng.uniform(2.2, 4.6)
                if z + lt > zb + hc - 4:
                    break
                ld = rng.uniform(15, 22)
                colour = _pick_colour(rng, colour)
                cx = x + width / 2 + 0.4 + rng.uniform(-0.6, 0.6)
                cy = yfb + ld / 2 + rng.uniform(0, 0.8)
                batch.add(colour, (lh, ld, lt), (cx, cy, z + lt / 2), rot=(0, 0, rng.uniform(-3.5, 3.5)), bevel=0.15)
                if colour in LEATHER:
                    for gx in (-lh / 2 + 2.0, lh / 2 - 2.0):
                        batch.add("Gilt", (0.28, 0.1, lt - 0.3), (cx + gx, cy - ld / 2 - 0.04, z + lt / 2))
                z += lt
                lh *= rng.uniform(0.86, 0.98)
            stacks += 1
            prev = colour
            last_h = None
            x += width + 1.4
            continue
        if room < 16 and last_h and rng.random() < 0.75:
            # Leaning book at the end of the row, resting on the last upright one.
            t = rng.uniform(1.8, 3.4)
            h = min(last_h - 1.0, rng.uniform(18, hc - 3))
            d = min(h * 0.7 + rng.uniform(-1, 2), 24)
            a = math.radians(rng.uniform(10, 22))
            while h * math.sin(a) + t * math.cos(a) > room - 0.3 and a > math.radians(4):
                a -= math.radians(1)
            if h * math.sin(a) + t * math.cos(a) <= room - 0.3:
                px = x + h * math.sin(a)
                cx = px + t / 2 * math.cos(a) - h / 2 * math.sin(a)
                cz = zb + t / 2 * math.sin(a) + h / 2 * math.cos(a)
                colour = _pick_colour(rng, prev)
                _book(batch, colour, t, d, h, (cx, yfb + d / 2 + rng.uniform(0, 0.6), cz), rot=(0, -math.degrees(a), 0),
                      style=_spine_style(rng, colour))
            break
        # A run of one series (same colour, near-equal height), or a single book.
        n = 1 if rng.random() < 0.35 else rng.randint(2, 7)
        colour = _pick_colour(rng, prev)
        h_base = rng.uniform(max(16.0, hc * 0.6), hc - 2.2)
        t_base = rng.uniform(1.8, 5.0)
        d_base = min(max(h_base * 0.68 + rng.uniform(-1.5, 2.5), 12), 25)
        style = _spine_style(rng, colour)
        back = 2.2 if rng.random() < 0.08 else 0.0
        run_x0 = x
        run_h = []
        for _ in range(n):
            t = t_base * rng.uniform(0.85, 1.15)
            room = x1 - x
            if t > room - 0.1:
                break
            h = min(h_base + (rng.uniform(-0.4, 0.4) if n > 1 else 0.0), hc - 2.0)
            yfront = yfb + rng.uniform(-0.3, 0.7) + back
            _book(batch, colour, t, d_base, h, (x + t / 2, yfront + d_base / 2, zb + h / 2), style=style)
            x += t + rng.uniform(0.02, 0.15)
            last_h = h
            run_h.append(h)
        if not run_h:
            break
        # Now and then a book lies across the top of a run with enough headroom.
        run_w = x - run_x0
        head = hc - max(run_h)
        if run_w > 16 and head > 5.5 and rng.random() < 0.18:
            lh = min(run_w - 1, rng.uniform(17, 26))
            lt = rng.uniform(2.0, min(3.6, head - 2.5))
            ld = rng.uniform(14, 20)
            c2 = _pick_colour(rng, colour)
            batch.add(c2, (lh, ld, lt), (run_x0 + run_w / 2 + rng.uniform(-1, 1), yfb + ld / 2 + 0.4, zb + max(run_h) + lt / 2 + 0.05),
                      rot=(0, 0, rng.uniform(-4, 4)), bevel=0.15)
        prev = colour


@item("Deco_bookshelf")
def bookshelf(it: Item):
    """Walnut bookcase module 90 x 37 x 220, six shelves of books. Flush at x = +-45 so modules line up seamlessly."""
    wood = "Wood_Walnut"
    hx, side_t = 45.0, 1.6
    inner = hx - side_t
    yf, yb = -16.5, 18.5
    # Plinth and crown run the full width with flat ends (no bevel at x = +-45, so neighbours join without a groove).
    plinth = [(-yb, 0.0), (15.8, 0.0), (15.8, 8.6), (16.5, 9.1), (16.5, 10.0), (-yb, 10.0)]
    it.add(sweep([(-hx, 0, 0), (hx, 0, 0)], plinth, up=(0, 0, 1)), wood, shade="auto", angle=40)
    crown = [(-yb, 208.0), (16.8, 208.0), (16.8, 212.6), (17.2, 212.8), (17.2, 213.6)]
    for i in range(1, 7):
        a = math.radians(180 - 90 * i / 6)
        crown.append((18.5 + 1.3 * math.cos(a), 213.6 + 1.6 * math.sin(a)))
    crown += [(18.5, 217.0), (18.5, 219.0), (18.25, 219.7), (17.7, 220.0), (-yb, 220.0)]
    it.add(sweep([(-hx, 0, 0), (hx, 0, 0)], crown, up=(0, 0, 1)), wood, shade="auto", angle=40)
    # Carcass.
    for sx in (-1, 1):
        it.add(cube(side_t, yb - yf, 198), wood, loc=(sx * (hx - side_t / 2), (yf + yb) / 2, 109), bevel=0.2, seg=2, grain="z")
    it.add(cube(2 * inner, 34, 2.5), wood, loc=(0, 0.5, 11.25), bevel=0.25, seg=2)
    it.add(cube(2 * inner, 1.0, 195.5), "Wood_Dark", loc=(0, 18.0, 110.25), grain="z")
    zb0, shelf_t = 12.5, 2.2
    weights = [1.17, 1.07, 1.02, 0.99, 0.98, 0.97]
    avail = 208.0 - zb0 - 5 * shelf_t
    clear = [avail * w / sum(weights) for w in weights]
    batch = BoxBatch()
    rng = random.Random(1937)
    z = zb0
    for k, hc in enumerate(clear):
        _fill_compartment(batch, rng, -inner + 0.2, inner - 0.2, z, hc, yf + 1.2)
        z += hc
        if k < 5:
            it.add(cube(2 * inner, 34, shelf_t), wood, loc=(0, 0.5, z + shelf_t / 2), bevel=0.3, seg=2)
            z += shelf_t
    batch.flush(it, grain="z")


# ---------------------------------------------------------------------------------------------
# Bathroom
# ---------------------------------------------------------------------------------------------


@item("Deco_toilet")
def toilet(it: Item):
    """Back-to-wall floor WC with close-coupled cistern, lid closed. 38 x 65 x 80, wall at y = +32.5."""
    wall = 32.5
    n = 48
    rings = []
    for z, w, lf in ((0.0, 28.0, 16.0), (7.0, 27.4, 17.4), (15.0, 29.6, 21.6), (24.0, 33.2, 27.0), (31.0, 35.4, 30.6),
                     (36.2, 36.0, 32.0), (38.1, 35.6, 31.7), (38.5, 34.8, 31.0)):
        rings.append(ring(pan_outline(w, lf, wall, 3.0), n, z))
    it.add(loft(rings), "Porcelain", shade="smooth")
    # Seat and closed lid.
    seat = pan_outline(35.4, 32.0, 10.0, 7.0)
    it.add(loft([ring(seat, n, 38.4, inset=0.8), ring(seat, n, 38.9), ring(seat, n, 40.0), ring(seat, n, 40.4, inset=0.7)]),
           "Plastic_White", shade="smooth")
    lid = pan_outline(36.0, 32.5, 11.0, 8.0)
    it.add(loft([ring(lid, n, 40.5, inset=0.6), ring(lid, n, 41.0), ring(lid, n, 42.0, inset=0.3), ring(lid, n, 42.6, inset=2.0),
                 ring(lid, n, 43.0, inset=6.0)]), "Plastic_White", shade="smooth")
    for sx in (-1, 1):
        it.add(cube(4.2, 3.2, 1.0), "Chrome", loc=(sx * 11, 13.4, 38.9), bevel=0.4, seg=2)
        it.add(cylinder(1.25, 4.4, 16), "Chrome", loc=(sx * 11 - 2.2, 12.8, 41.0), rot=(0, 90, 0), shade="smooth")
    # Cistern with lid and dual flush button on top.
    it.add(cube(37.6, 16.6, 39.4), "Porcelain", loc=(0, wall - 8.3, 38.4 + 19.7), bevel=2.6, seg=5)
    it.add(cube(38, 17, 2.4), "Porcelain", loc=(0, wall - 8.5, 78.8), bevel=1.0, seg=4)
    it.add(lathe([(0, 0), (3.6, 0), (3.6, 0.15), (3.4, 0.25), (0, 0.25)], 40), "Chrome", loc=(0, wall - 8.5, 80.0), shade="auto")
    it.add(lathe([(0, 0), (3.1, 0), (3.1, 0.2), (2.9, 0.35), (0, 0.35)], 40), "Chrome", loc=(0, wall - 8.5, 80.05), shade="auto")
    it.add(cube(0.25, 6.2, 0.1), "Plastic_Grey", loc=(-0.9, wall - 8.5, 80.42))
    # Floor fixing caps on the flanks.
    for sx in (-1, 1):
        it.add(sphere(1.1, 14, 6, sz=0.6), "Plastic_White", loc=(sx * 13.9, 14, 2.4), rot=(0, sx * 90, 0), shade="smooth")


@item("Deco_washbasin")
def washbasin(it: Item):
    """Wall-hung ceramic washbasin on a floating oak vanity drawer. Basin top at 88, origin on the floor, back at y = +22.5."""
    n = 96
    cy = -4.0  # bowl centre; all rings are sampled from here

    def outer(i=0.0):
        return lambda phi: ray_rrect(phi, -30 + i, 30 - i, -22.5 - cy + i, 22.5 - cy - i, 5.0 - i, max(0.6 - i, 0.0))

    a, b = 20.0, 14.0
    rings = [ring(outer(0.6), n, 75.0, cy=cy), ring(outer(), n, 75.6, cy=cy), ring(outer(), n, 86.9, cy=cy),
             ring(outer(0.3), n, 87.7, cy=cy), ring(outer(1.0), n, 88.0, cy=cy),
             ring(ellipse(a + 1.4, b + 1.4), n, 88.0, cy=cy), ring(ellipse(a + 0.4, b + 0.4), n, 87.75, cy=cy),
             ring(ellipse(a, b), n, 87.0, cy=cy)]
    for s, z in ((0.97, 85.0), (0.9, 82.6), (0.76, 79.8), (0.55, 77.6), (0.32, 76.5), (0.12, 76.1)):
        rings.append(ring(ellipse(a * s, b * s), n, z, cy=cy))
    rings.append([(0.0, cy, 76.0)])
    it.add(loft(rings), "Porcelain", shade="auto", angle=40)
    it.add(lathe([(0, 0), (1.7, 0), (1.7, 0.15), (1.5, 0.25), (0, 0.25)], 24), "Chrome", loc=(0, cy, 76.02), shade="auto")
    it.add(cube(3.2, 0.3, 0.9), "Plastic_Black", loc=(0, -17.2, 84.4), rot=(-30, 0, 0), bevel=0.12, seg=2)  # overflow slot
    # Single-lever mixer.
    fx, fy, fz = 0.0, 15.5, 88.0
    it.add(lathe([(0, 0), (2.8, 0), (2.8, 0.5), (2.3, 1.0), (1.95, 1.6), (1.95, 8.6), (2.05, 9.0), (1.7, 9.6), (0, 9.8)], 32),
           "Chrome", loc=(fx, fy, fz), shade="auto", angle=40)
    spout = spline([(0, fy - 0.5, fz + 7.6), (0, fy - 5.5, fz + 8.2), (0, fy - 10.0, fz + 7.6), (0, fy - 11.6, fz + 6.0)], 5)
    sc = [1.0 - 0.18 * i / (len(spout) - 1) for i in range(len(spout))]
    it.add(sweep(spout, circle_profile(1.05, 16, sy=0.85), up=(1, 0, 0), scales=sc), "Chrome", shade="smooth")
    it.add(cylinder(0.82, 0.25, 16), "Plastic_Grey", loc=(0, fy - 11.6, fz + 5.8), shade="auto")  # aerator
    it.add(sweep([(0, fy + 0.2, fz + 9.4), (0, fy + 3.0, fz + 10.4), (0, fy + 5.5, fz + 11.0)], rect_profile(0.7, 1.7, 0.25),
                 up=(1, 0, 0)), "Chrome", shade="auto")
    # Floating vanity with one big drawer and a handle-less grip groove under the basin.
    oak = "Wood_Oak"
    it.add(cube(57, 41.5, 35), oak, loc=(0, 22.5 - 20.75, 57.5), bevel=0.4, seg=2)
    it.add(cube(56.6, 0.6, 2.4), "Plastic_Black", loc=(0, -19.2, 73.7))
    it.add(cube(56.4, 1.8, 32.4), oak, loc=(0, -19.9, 56.4), bevel=0.45, seg=2, grain="x")
    it.add(cube(56.4, 2.0, 0.5), "Aluminium", loc=(0, -19.8, 72.85), bevel=0.15, seg=1)


@item("Deco_washing_machine")
def washing_machine(it: Item):
    """White front-loader 60 x 60 x 85: round door with glass, control panel with drawer, knob and display."""
    white = "Lacquer_White"
    for sx in (-1, 1):
        for sy in (-1, 1):
            it.add(cylinder(2.0, 1.5, 16), "Rubber_Black", loc=(sx * 25, sy * 24 + 2, 0), shade="smooth")
    it.add(cube(59.6, 56.5, 81.5), white, loc=(0, 1.75, 42.25), bevel=1.6, seg=4)
    it.add(cube(60, 57.5, 2.0), "Plastic_White", loc=(0, 1.25, 84.0), bevel=0.7, seg=3)
    it.add(cube(59.0, 0.4, 0.4), "Plastic_Grey", loc=(0, -26.6, 82.9))  # seam under the top
    # Control panel.
    pf = -27.6  # panel front face
    it.add(cube(59.6, 1.6, 12.0), "Plastic_White", loc=(0, pf + 0.8, 76.6), bevel=0.6, seg=3)
    it.add(cube(16.6, 0.3, 9.6), "Plastic_Grey", loc=(-20.4, pf - 0.05, 76.6))  # drawer gap
    it.add(cube(16.0, 0.8, 9.0), "Plastic_White", loc=(-20.4, pf - 0.4, 76.6), bevel=0.35, seg=2)
    it.add(cube(9.0, 0.3, 1.4), "Plastic_Grey", loc=(-20.4, pf - 0.8, 73.0), bevel=0.1, seg=1)  # finger recess
    it.add(cube(12.0, 0.3, 4.6), "Screen_Black", loc=(4.5, pf - 0.12, 78.4), bevel=0.15, seg=1)
    for k in range(3):
        it.add(cube(1.1, 0.12, 2.0), "Display_Blue", loc=(1.0 + k * 1.6, pf - 0.3, 78.5))
    it.add(cube(0.6, 0.12, 0.6), "LED_Green", loc=(9.4, pf - 0.3, 79.6))
    for k in range(4):
        it.add(cube(2.2, 0.5, 1.2), "Plastic_Grey", loc=(0.3 + k * 2.9, pf - 0.2, 73.6), bevel=0.2, seg=2)
    kx, kz = 20.5, 76.6
    facing_y(it, lathe([(0, 0), (4.1, 0), (4.1, 0.3), (3.8, 0.45), (0, 0.45)], 40), "Chrome", (kx, pf, kz), shade="auto")
    facing_y(it, lathe([(0, 0), (3.2, 0), (3.3, 0.4), (3.2, 1.2), (2.9, 1.5), (0, 1.55)], 40), "Plastic_White", (kx, pf - 0.4, kz),
             shade="auto", angle=40)
    it.add(cube(0.8, 0.5, 4.6), "Plastic_Grey", loc=(kx, pf - 2.1, kz), bevel=0.2, seg=2)  # grip ridge
    for i in range(12):
        a = math.radians(-150 + 300 * i / 11)
        it.add(cube(0.3, 0.1, 0.7), "Plastic_Grey", loc=(kx + 5.0 * math.sin(a), pf - 0.05, kz + 5.0 * math.cos(a)), rot=(0, math.degrees(a), 0))
    # Door: white ring, chrome trim, tinted glass dome; drum and gasket behind.
    bf = -26.5  # body front face
    dz = 40.0
    facing_y(it, cylinder(16.8, 0.15, 48), "Plastic_Black", (0, bf, dz), shade="auto")
    facing_y(it, torus(15.6, 1.7, 48, 10), "Plastic_Grey", (0, bf - 0.4, dz), shade="smooth")
    facing_y(it, cylinder(14.2, 0.1, 48), "Steel", (0, bf - 0.2, dz), shade="auto")  # drum back wall
    facing_y(it, cylinder(2.2, 0.25, 24), "Steel_Satin", (0, bf - 0.25, dz), shade="auto")  # hub
    holes = BoxBatch()
    for rr, cnt in ((4.2, 8), (6.6, 14), (9.0, 18), (11.4, 24)):
        for i in range(cnt):
            a = 2 * math.pi * (i + 0.5 * (cnt % 4 == 2)) / cnt
            holes.add("Plastic_Black", (0.55, 0.06, 0.55), (rr * math.cos(a), bf - 0.32, dz + rr * math.sin(a)), rot=(0, 45, 0))
    holes.flush(it, grain="x")
    for k in range(3):
        a = math.radians(90 + 120 * k)
        it.add(cube(1.8, 1.2, 3.0), "Steel_Satin", loc=(12.6 * math.cos(a), bf - 0.8, dz + 12.6 * math.sin(a)),
               rot=(0, -math.degrees(a) + 90, 0), bevel=0.4, seg=2)  # paddles at the drum rim
    ring_prof = [(17.2, 0.0), (24.0, 0.0), (24.0, 1.1), (23.5, 2.3), (21.8, 3.1), (19.4, 3.1), (18.0, 2.4), (17.2, 1.4), (17.2, 0.0)]
    facing_y(it, lathe(ring_prof, 72, caps=False), "Plastic_White", (0, bf, dz), shade="auto", angle=40)
    facing_y(it, torus(18.6, 0.55, 72, 10), "Chrome", (0, bf - 2.7, dz), shade="smooth")
    facing_y(it, lathe([(0, 2.0), (6, 2.0), (12, 1.7), (15.5, 1.2), (17.6, 0.5), (17.8, 0.0), (0, 0.0)], 48), "Glass_Tinted",
             (0, bf - 1.5, dz), shade="smooth")
    it.add(cube(3.0, 1.6, 7.0), "Plastic_Grey", loc=(22.3, bf - 2.2, dz), bevel=0.5, seg=3)  # handle
    for z in (dz - 9, dz + 9):
        it.add(cube(2.4, 2.4, 4.0), "Plastic_Grey", loc=(-24.2, bf - 1.0, z), bevel=0.4, seg=2)  # hinges
    # Service flap at the bottom.
    it.add(cube(14.6, 0.2, 10.6), "Plastic_Grey", loc=(19.0, bf - 0.05, 8.5))
    it.add(cube(14.0, 0.4, 10.0), white, loc=(19.0, bf - 0.2, 8.5), bevel=0.3, seg=2)
