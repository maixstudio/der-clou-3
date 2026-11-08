"""Villa G1 – catalog furniture: dresser, bed, nightstand, grand piano and the key board (SM_<id>).

Sizes follow Docs/Villa/Villa-Modelle.md (catalog sizes for dresser, bed, nightstand must stay exact).
Convention as everywhere: cm, origin bottom centre, front = -Y, width along X.
Exceptions: the bed runs along X (headboard at +X); the key board is a wall piece (back at y = +2.5).
"""

import math

import bmesh
from mathutils import Euler, Matrix, Vector

from dc_items import item, knob
from dc_items_small import spline
from dc_mesh import (CM, Item, circle_profile, cube, cylinder, lathe, ngon_prism, puff, rect_profile, sphere, sweep,
                     tapered_box, torus, wobble)

WALNUT = "Wood_Walnut"


# ---------------------------------------------------------------------------------------------
# Helpers (geometry in cm, bmesh in metres like dc_mesh)
# ---------------------------------------------------------------------------------------------


def rounded_rect(w, h, r, n=4):
    """Counter-clockwise rounded rectangle centred on the origin (XY, cm)."""
    pts = []
    for cx, cy, a0 in ((w / 2 - r, -h / 2 + r, -90), (w / 2 - r, h / 2 - r, 0), (-w / 2 + r, h / 2 - r, 90),
                       (-w / 2 + r, -h / 2 + r, 180)):
        for i in range(n + 1):
            a = math.radians(a0 + 90 * i / n)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def dedupe(poly, eps=1e-3):
    out = []
    for p in poly:
        if not out or math.hypot(p[0] - out[-1][0], p[1] - out[-1][1]) > eps:
            out.append(p)
    if len(out) > 2 and math.hypot(out[0][0] - out[-1][0], out[0][1] - out[-1][1]) <= eps:
        out.pop()
    return out


def slab(poly, z0, z1):
    """Polygon (XY, cm, may be concave) extruded from z0 to z1; caps triangulated so concave outlines export cleanly."""
    poly = dedupe(poly)
    bm = bmesh.new()
    bot = [bm.verts.new((x * CM, y * CM, z0 * CM)) for x, y in poly]
    top = [bm.verts.new((x * CM, y * CM, z1 * CM)) for x, y in poly]
    caps = [bm.faces.new(list(reversed(bot))), bm.faces.new(top)]
    k = len(poly)
    for i in range(k):
        bm.faces.new((bot[i], bot[(i + 1) % k], top[(i + 1) % k], top[i]))
    bmesh.ops.triangulate(bm, faces=caps)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def prism_yz(points, x0, x1):
    """Profile in the YZ plane (cm) extruded along X from x0 to x1 (e.g. an arched headboard)."""
    bm = bmesh.new()
    a = [bm.verts.new((x0 * CM, y * CM, z * CM)) for y, z in points]
    b = [bm.verts.new((x1 * CM, y * CM, z * CM)) for y, z in points]
    caps = [bm.faces.new(a), bm.faces.new(list(reversed(b)))]
    k = len(points)
    for i in range(k):
        bm.faces.new((a[i], a[(i + 1) % k], b[(i + 1) % k], b[i]))
    bmesh.ops.triangulate(bm, faces=caps)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def xf(bm, loc=(0, 0, 0), rot=(0, 0, 0), matrix=None):
    """Transform a bmesh in place (loc cm, rot degrees XYZ) – for parts that share one pivot."""
    m = matrix if matrix is not None else trs(loc, rot)
    bmesh.ops.transform(bm, matrix=m, verts=bm.verts[:])
    return bm


def trs(loc, rot):
    return Matrix.Translation(Vector(loc) * CM) @ Euler([math.radians(a) for a in rot]).to_matrix().to_4x4()


def scale_bm(bm, sx, sy, sz):
    bmesh.ops.scale(bm, vec=(sx, sy, sz), verts=bm.verts[:])
    return bm


def ticks(start, *legs):
    """1D coordinates: from start, each leg (end, step) adds evenly spaced points up to end."""
    out = [start]
    for end, step in legs:
        s = out[-1]
        n = max(1, round(abs(end - s) / step))
        out += [s + (end - s) * k / n for k in range(1, n + 1)]
    return out


def grid_slab(xs, ys, z0, z1):
    """Closed sheet with a top/bottom grid on the given X/Y coordinates (cm) – for cloth that gets bent later."""
    bm = bmesh.new()
    top = [[bm.verts.new((x * CM, y * CM, z1 * CM)) for y in ys] for x in xs]
    bot = [[bm.verts.new((x * CM, y * CM, z0 * CM)) for y in ys] for x in xs]
    nx, ny = len(xs), len(ys)
    for i in range(nx - 1):
        for j in range(ny - 1):
            bm.faces.new((top[i][j], top[i + 1][j], top[i + 1][j + 1], top[i][j + 1]))
            bm.faces.new((bot[i][j], bot[i][j + 1], bot[i + 1][j + 1], bot[i + 1][j]))
    for i in range(nx - 1):
        for j in (0, ny - 1):
            bm.faces.new((bot[i][j], bot[i + 1][j], top[i + 1][j], top[i][j]))
    for j in range(ny - 1):
        for i in (0, nx - 1):
            bm.faces.new((bot[i][j], bot[i][j + 1], top[i][j + 1], top[i][j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def drape(foot_x, half_y, top, radius):
    """Bend a flat sheet (bottom at z = top) over the mattress edges: the foot edge at x = foot_x (hangs towards -X)
    and the sides at y = +-half_y. The head end (+X) stays flat. Corners droop like real bedding.
    The part must be built in place (loc 0) – works on local coordinates in cm."""

    def apply(me):
        r0 = radius
        x_in = foot_x + r0          # inner rectangle: x >= x_in, |y| <= y_in
        y_in = half_y - r0
        z_c = top - r0              # height of the bend axis
        for v in me.vertices:
            x, y, z = v.co.x / CM, v.co.y / CM, v.co.z / CM
            nx = max(x, x_in)
            ny = max(-y_in, min(y_in, y))
            ox, oy = x - nx, y - ny
            s = math.hypot(ox, oy)
            if s < 1e-6:
                continue
            dx, dy = ox / s, oy / s
            r = r0 + (z - top)
            th = s / r0
            if th <= math.pi / 2:
                h, zz = r * math.sin(th), z_c + r * math.cos(th)
            else:
                h, zz = r, z_c - (s - r0 * math.pi / 2)
            v.co = Vector((nx + dx * h, ny + dy * h, zz)) * CM
        me.update()

    return apply


def dome(x0, x1, y0, y1, amount, z_split):
    """Raise vertices above z_split by up to `amount` (cm), most in the middle of the rectangle – a soft duvet top.
    Applied before drape(); the same rectangle on two layers keeps them parallel."""

    def apply(me):
        cx, cy, hx, hy = (x0 + x1) / 2, (y0 + y1) / 2, (x1 - x0) / 2, (y1 - y0) / 2
        for v in me.vertices:
            x, y, z = v.co.x / CM, v.co.y / CM, v.co.z / CM
            if z <= z_split:
                continue
            w = max(0.0, 1 - ((x - cx) / hx) ** 2) * max(0.0, 1 - ((y - cy) / hy) ** 2)
            v.co.z += amount * w * CM

    return apply


def inset(poly, d):
    """Offset a counter-clockwise polygon inwards by d (cm)."""
    n = len(poly)
    out = []
    for i in range(n):
        p0, p1, p2 = Vector(poly[i - 1]), Vector(poly[i]), Vector(poly[(i + 1) % n])
        e0, e1 = (p1 - p0).normalized(), (p2 - p1).normalized()
        n0, n1 = Vector((-e0.y, e0.x)), Vector((-e1.y, e1.x))
        m = (n0 + n1).normalized()
        out.append(tuple(p1 + m * (d / max(m.dot(n1), 0.3))))
    return out


def clip_y(poly, y0):
    """Part of the polygon with y >= y0."""
    out = []
    n = len(poly)
    for i in range(n):
        a, b = poly[i], poly[(i + 1) % n]
        ina, inb = a[1] >= y0, b[1] >= y0
        if ina:
            out.append(a)
        if ina != inb:
            t = (y0 - a[1]) / (b[1] - a[1])
            out.append((a[0] + (b[0] - a[0]) * t, y0))
    return dedupe(out)


def bail_pull(it, x, y, z, w=8.0, mat="Brass"):
    """Brass bail pull on a drawer face at y (front = -Y); z = height of the two posts."""
    it.add(ngon_prism(rounded_rect(w + 3.0, 4.2, 1.4), 0.25), mat, loc=(x, y, z - 0.8), rot=(90, 0, 0), bevel=0.08, seg=1)
    for sx in (-1, 1):
        it.add(cylinder(0.42, 1.0, 12), mat, loc=(x + sx * w / 2, y - 0.25, z), rot=(90, 0, 0), shade="smooth")
        it.add(sphere(0.5, 12, 6), mat, loc=(x + sx * w / 2, y - 0.85, z), rot=(90, 0, 0), shade="smooth")
    yb = y - 1.3
    pts = [(x + (w / 2) * math.cos(a), yb, z + 2.4 * math.sin(a)) for a in [math.pi + math.pi * i / 14 for i in range(15)]]
    it.add(sweep(pts, circle_profile(0.3, 10), up=(0, -1, 0)), mat, shade="smooth")


def top_inlay(it, w, d, z, inset_cm=4.0, width=0.9, mat="Wood_Light"):
    """Light string inlay around a table top (reads well from the steep game camera)."""
    hx, hy = w / 2 - inset_cm, d / 2 - inset_cm
    for sy in (-1, 1):
        it.add(cube(2 * hx + width, width, 0.12), mat, loc=(0, sy * hy, z))
    for sx in (-1, 1):
        it.add(cube(width, 2 * hy - width, 0.12), mat, loc=(sx * hx, 0, z), grain="y")


# ---------------------------------------------------------------------------------------------
# Dresser – 100 x 45 x 85, walnut, 2 x 3 drawers with brass bail pulls
# ---------------------------------------------------------------------------------------------


@item("dresser")
def dresser(it: Item):
    wood = WALNUT
    front = -19.5
    for sx in (-1, 1):
        for sy in (-1, 1):
            it.add(tapered_box(5.5, 5.5, 4.2, 4.2, 7.2), wood, loc=(sx * 44.5, sy * 17.5 + 1, 0), bevel=0.4, seg=2, grain="z")
    it.add(cube(98.6, 42.6, 3), wood, loc=(0, 1, 8.5), bevel=1.0, seg=3, profile=0.7)        # plinth moulding
    it.add(cube(97, 41, 72), wood, loc=(0, 1, 46), bevel=0.4, seg=2, grain="z")              # carcass z 10..82
    it.add(cube(98.6, 42.6, 1.6), wood, loc=(0, 1, 81.6), bevel=0.7, seg=3, profile=0.7)     # under-top moulding
    it.add(cube(100, 45, 2.6), wood, loc=(0, 0, 83.7), bevel=1.0, seg=4, profile=0.6)        # top z 82.4..85
    top_inlay(it, 100, 45, 85.0)
    # Raised panels on the sides.
    for sx in (-1, 1):
        it.add(cube(0.6, 33, 58), wood, loc=(sx * 48.75, 1, 46), bevel=0.5, seg=3, profile=0.3, grain="y")
    # Two columns of three graduated drawers.
    rows = [(13, 24), (39, 21), (62, 18)]  # (bottom, height)
    for cx in (-23.5, 23.5):
        for zb, h in rows:
            zc = zb + h / 2
            it.add(cube(44, 0.2, h), "Plastic_Black", loc=(cx, front - 0.05, zc))        # shadow gap
            it.add(cube(43.4, 1.6, h - 0.6), wood, loc=(cx, front - 0.2, zc), bevel=0.35, seg=2)
            it.add(cube(39.4, 0.5, h - 4.6), wood, loc=(cx, front - 1.1, zc), bevel=0.45, seg=3, profile=0.3)
            bail_pull(it, cx, front - 1.35, zc + 1.0)


# ---------------------------------------------------------------------------------------------
# Nightstand – 45 x 40 x 55, one drawer, open shelf below
# ---------------------------------------------------------------------------------------------


@item("nightstand")
def nightstand(it: Item):
    wood = WALNUT
    fy = -17.5
    for sx in (-1, 1):
        for sy in (-1, 1):
            it.add(tapered_box(3.6, 3.6, 2.6, 2.6, 12.2), wood, loc=(sx * 19.5, sy * 16 + 1, 0), bevel=0.3, seg=2, grain="z")
    # Aprons between the legs.
    it.add(cube(35.4, 1.6, 3.5), wood, loc=(0, -14.7, 10.25), bevel=0.25, seg=2)
    it.add(cube(35.4, 1.6, 3.5), wood, loc=(0, 16.7, 10.25), bevel=0.25, seg=2)
    for sx in (-1, 1):
        it.add(cube(1.6, 28.4, 3.5), wood, loc=(sx * 19.3, 1, 10.25), bevel=0.25, seg=2, grain="y")
    # Carcass (open at the front: sides, back, bottom, divider).
    for sx in (-1, 1):
        it.add(cube(2, 37, 40.5), wood, loc=(sx * 20.5, 1, 32.25), bevel=0.3, seg=2, grain="z")
    it.add(cube(39, 1.2, 40.5), wood, loc=(0, 18.9, 32.25), grain="z")
    it.add(cube(39, 35.8, 2), wood, loc=(0, 0.4, 13), bevel=0.2, seg=1)
    it.add(cube(39, 35.8, 1.8), wood, loc=(0, 0.4, 37.1), bevel=0.2, seg=1)
    # Drawer.
    it.add(cube(39, 0.2, 14.5), "Plastic_Black", loc=(0, fy - 0.05, 45.25))
    it.add(cube(38.4, 1.6, 13.8), wood, loc=(0, fy - 0.2, 45.25), bevel=0.35, seg=2)
    it.add(cube(34.4, 0.45, 9.8), wood, loc=(0, fy - 1.1, 45.25), bevel=0.45, seg=3, profile=0.3)
    knob(it, "Brass", (0, fy - 1.3, 45.25), "-y", 0.62)
    # Top.
    it.add(cube(45, 40, 2.5), wood, loc=(0, 0, 53.75), bevel=0.8, seg=3, profile=0.6)
    top_inlay(it, 45, 40, 55.0, inset_cm=3.0, width=0.7)


# ---------------------------------------------------------------------------------------------
# Double bed – 200 x 150, mattress top at 55, headboard at +X up to 105
# ---------------------------------------------------------------------------------------------


@item("bed")
def bed(it: Item):
    wood = WALNUT
    hx = 96.6  # post centres along X (posts 6 x 6, caps 6.8 x 6.8 end flush at x = +-100)
    # Posts with finials.
    for sy in (-1, 1):
        y = sy * 71.6
        it.add(cube(6, 6, 100), wood, loc=(hx, y, 50), bevel=0.6, seg=2, grain="z")
        it.add(cube(6.8, 6.8, 1.5), wood, loc=(hx, y, 100.25), bevel=0.5, seg=2)
        it.add(lathe([(0, 0), (2.4, 0), (2.4, 0.6), (1.4, 1.2), (2.6, 2.6), (2.2, 3.8), (0.6, 4.4), (0, 4.5)], 20), wood,
               loc=(hx, y, 100.5), shade="smooth", grain="z")
        it.add(cube(6, 6, 50), wood, loc=(-hx, y, 25), bevel=0.6, seg=2, grain="z")
        it.add(cube(6.8, 6.8, 1.5), wood, loc=(-hx, y, 50.25), bevel=0.5, seg=2)
        it.add(lathe([(0, 0), (2.2, 0), (2.2, 0.5), (1.3, 1.1), (2.4, 2.4), (2.0, 3.5), (0.5, 4.0), (0, 4.1)], 20), wood,
               loc=(-hx, y, 50.5), shade="smooth", grain="z")
        # Side rail with a moulded top edge.
        it.add(cube(188, 5, 20), wood, loc=(0, sy * 72.5, 32), bevel=0.5, seg=2)
        it.add(cube(188, 5.0, 1.6), wood, loc=(0, sy * 72.5, 42.6), bevel=0.7, seg=3, profile=0.7)
    # Headboard: lower board, two raised panels, arched crest rail.
    it.add(cube(3, 138, 28), wood, loc=(hx, 0, 36), bevel=0.4, seg=2, grain="y")
    it.add(cube(2, 138, 38), wood, loc=(hx, 0, 69), grain="y")
    it.add(cube(3.8, 138, 4), wood, loc=(hx, 0, 51), bevel=0.6, seg=3, profile=0.6, grain="y")
    it.add(cube(3.8, 138, 4), wood, loc=(hx, 0, 87), bevel=0.6, seg=3, profile=0.6, grain="y")
    it.add(cube(3.6, 5, 34), wood, loc=(hx, 0, 69), bevel=0.4, seg=2, grain="z")
    for sy in (-1, 1):
        it.add(cube(3.4, 58, 28), wood, loc=(hx, sy * 34, 69), bevel=1.4, seg=3, profile=0.3, grain="y")
    arch = lambda y: 95.5 + 6.5 * (1 - (y / 69.0) ** 2)  # noqa: E731
    ys = [-69 + 138 * i / 16 for i in range(17)]
    crest = [(-69, 88.5), (69, 88.5)] + [(y, arch(y)) for y in reversed(ys)]
    it.add(prism_yz(crest, hx - 1.6, hx + 1.6), wood, bevel=0.5, seg=2, grain="y")
    it.add(sweep([(hx, y, arch(y)) for y in ys], circle_profile(1.0, 12, sx=2.4, sy=1.5), up=(0, 0, 1)), wood,
           shade="smooth", grain="y")
    # Footboard: low board with one raised panel and a moulded cap.
    it.add(cube(3, 138, 26), wood, loc=(-hx, 0, 35), bevel=0.4, seg=2, grain="y")
    it.add(cube(3.8, 122, 16), wood, loc=(-hx, 0, 35), bevel=1.3, seg=3, profile=0.3, grain="y")
    it.add(cube(5, 140, 2.5), wood, loc=(-hx, 0, 49.25), bevel=0.9, seg=3, profile=0.6, grain="y")
    # Support board, mattress with fitted sheet.
    it.add(cube(186, 138, 2), wood, loc=(0, 0, 29))
    it.add(cube(181, 128, 25), "Fabric_Grey", loc=(1.5, 0, 42.5), bevel=5.0, seg=4, shade="smooth")
    # Duvet: flat sheet bent over the mattress edges (tucked in between mattress and frame).
    foot, half, mtop, rad = -89.0, 64.0, 55.3, 5.0
    xs = ticks(-104.85, (-80.0, 2.0), (36.0, 7.0))
    ys_d = ticks(-77.0, (-50.0, 2.0), (50.0, 7.0), (77.0, 2.0))
    soft = dome(-104.85, 36.0, -77.0, 77.0, 2.4, mtop + 2.0)
    it.add(grid_slab(xs, ys_d, mtop, mtop + 4.5), "Fabric_Blue", bevel=1.6, seg=3, shade="smooth",
           post=[soft, drape(foot, half, mtop, rad), wobble(0.7, 45, 2)])
    # White sheet folded back over the top edge of the duvet.
    xs_b = ticks(19.0, (37.0, 3.0))
    ys_b = ticks(-77.5, (-50.0, 2.0), (50.0, 8.0), (77.5, 2.0))
    soft_b = dome(-104.85, 36.0, -77.0, 77.0, 2.4, 0.0)
    it.add(grid_slab(xs_b, ys_b, mtop + 4.8, mtop + 6.3), "Linen_White", bevel=0.7, seg=2, shade="smooth",
           post=[soft_b, drape(foot, half, mtop, rad), wobble(0.7, 45, 2)])
    # Two pillows leaning on the headboard (thin seams, full middle), one velvet cushion in front.
    for sy in (-1, 1):
        it.add(cube(46, 61, 4, cuts=8), "Linen_White", loc=(70.0, sy * 33.0, 63.3), rot=(0, -13, sy * 4), bevel=1.8, seg=3,
               shade="smooth", post=[puff(7.0, 2), wobble(0.5, 22, 3 + sy)])
    it.add(cube(38, 40, 3, cuts=8), "Velvet_Red", loc=(57.0, 0, 65.0), rot=(0, -40, 0), bevel=1.3, seg=3,
           shade="smooth", post=[puff(5.0, 2), wobble(0.3, 16, 7)])


# ---------------------------------------------------------------------------------------------
# Grand piano – ca. 150 x 215, keyboard at -Y, tail at +Y, lid open on the stick
# ---------------------------------------------------------------------------------------------

PIANO_LID_ANGLE = 50.0
PIANO_TOP = 100.0
PIANO_BOTTOM = 62.0


def _piano_shape():
    """Rim outline seen from above: straight bass side at x = -75, bentside and tail on the right/back."""
    ctrl = [(75, -50), (73.5, -36), (68, -24), (59, -12), (48, 2), (37, 18), (27, 35), (18.5, 52), (11, 68), (4, 82),
            (-3.5, 94), (-13, 102.5), (-26, 107), (-41, 107.5), (-55, 105.5), (-66, 100), (-72.5, 92), (-75, 82)]
    curve = [(max(p[0], -75.0), p[1]) for p in spline([(x, y, 0.0) for x, y in ctrl], steps=3)]
    outline = dedupe([(75.0, -82.0)] + curve + [(-75.0, -82.0)])
    return curve, outline


def _rim_y(curve, x):
    """Inner end of the case along +Y at a given x (bentside or tail)."""
    best = None
    for (x1, y1), (x2, y2) in zip(curve, curve[1:]):
        lo, hi = min(x1, x2), max(x1, x2)
        if hi > lo and lo <= x <= hi:
            y = y1 + (y2 - y1) * (x - x1) / (x2 - x1)
            best = y if best is None else max(best, y)
    return best


def _rim_x(curve, y):
    """Right side of the case at a given y (front part of the bentside)."""
    for (x1, y1), (x2, y2) in zip(curve, curve[1:]):
        if min(y1, y2) <= y <= max(y1, y2) and y2 != y1 and x1 > 20:
            return x1 + (x2 - x1) * (y - y1) / (y2 - y1)
    return 75.0


def _grand_leg(it, x, y, lac):
    it.add(cube(11, 11, 5), lac, loc=(x, y, PIANO_BOTTOM - 2.5), bevel=0.8, seg=3)
    it.add(cube(9.6, 9.6, 1.6), lac, loc=(x, y, PIANO_BOTTOM - 5.8), bevel=0.5, seg=2)
    it.add(tapered_box(8.4, 8.4, 5.6, 5.6, PIANO_BOTTOM - 6.6 - 8), lac, loc=(x, y, 8), bevel=0.5, seg=2, grain="z")
    it.add(cube(6.4, 6.4, 3.4), "Brass", loc=(x, y, 6.3), bevel=0.4, seg=2)
    it.add(cube(2.8, 3.4, 2.2), "Brass", loc=(x, y, 3.7), bevel=0.2, seg=1)
    it.add(cylinder(2.3, 1.8, 16), "Brass", loc=(x - 0.9, y, 2.3), rot=(0, 90, 0), shade="auto")


@item("grand_piano")
def grand_piano(it: Item):
    lac = "Lacquer_Black"
    top, bottom = PIANO_TOP, PIANO_BOTTOM
    curve, outline = _piano_shape()

    # --- Case: rim wall, bottom board, front board with desk rail ---
    it.add(sweep([(x, y, bottom) for x, y in outline], [(-4, 0), (0, 0), (0, top - bottom), (-4, top - bottom)],
                 up=(0, 0, 1)), lac, bevel=0.6, seg=2)
    it.add(slab(outline, bottom, bottom + 2.0), lac)
    it.add(cube(142, 4, 28), lac, loc=(0, -80, bottom + 14), bevel=0.4, seg=2)
    it.add(cube(142, 12, 1.5), lac, loc=(0, -74, 90.75), bevel=0.4, seg=2)
    # Cheek blocks beside the keys.
    for sx in (-1, 1):
        it.add(cube(13.7, 25.5, 16), lac, loc=(sx * 68.15, -94.75, bottom + 8), bevel=2.0, seg=4)

    # --- Inside: soundboard, gilt plate, bridges, strings, dampers, tuning pins ---
    it.add(slab(inset(outline, 3.5), 84.5, 86.0), "Wood_Light", grain="y")
    it.add(sweep([(x, y, 86.0) for x, y in outline], [(-11.5, 0), (-3.8, 0), (-3.8, 3.0), (-11.5, 3.0)], up=(0, 0, 1),
                 closed=True), "Gilt", bevel=0.5, seg=2)
    it.add(cube(142, 12, 3.2), "Gilt", loc=(0, -64, 87.6), bevel=0.5, seg=2)            # pin field
    for row, py in enumerate((-66.5, -61.8)):
        for k in range(30):
            it.add(cylinder(0.38, 2.4, 6), "Steel", loc=(-64 + k * 4.4 + row * 2.2, py, 89.0), shade="smooth")
    # Long bridge (treble) and bass bridge under the strings.
    bridge = [(x, _rim_y(curve, x) - 25, 86.0) for x in range(-28, 67, 6)]
    it.add(sweep(bridge, [(-1.3, 0), (1.3, 0), (1.1, 3.65), (-1.1, 3.65)], up=(0, 0, 1)), "Wood_Dark", shade="auto")
    bass = []
    for i in range(18):
        sx, ex = -70 + i * 1.75, -62 + i * 3.0
        bass.append(((sx, -56.0), (ex, _rim_y(curve, ex) - 10)))
    b0 = [bass[0][0][k] + 0.9 * (bass[0][1][k] - bass[0][0][k]) for k in (0, 1)]
    b1 = [bass[-1][0][k] + 0.9 * (bass[-1][1][k] - bass[-1][0][k]) for k in (0, 1)]
    it.add(sweep([(b0[0] - 2, b0[1] - 0.3, 86.0), (b1[0] + 2, b1[1] + 0.3, 86.0)],
                 [(-1.3, 0), (1.3, 0), (1.1, 3.65), (-1.1, 3.65)], up=(0, 0, 1)), "Wood_Dark", shade="auto")
    # Treble strings (parallel), bass strings (copper, overstrung at an angle).
    for i in range(61):
        x = -30 + i * 1.6
        y1 = _rim_y(curve, x) - 10
        it.add(cube(0.22, y1 + 63, 0.22), "Steel", loc=(x, (y1 - 63) / 2, 89.81), shade="flat")
    for (sx, sy), (ex, ey) in bass:
        it.add(sweep([(sx, sy, 90.6), (ex, ey, 90.6)], rect_profile(0.5, 0.5)), "Copper", shade="flat")
    # Capo bar, struts of the plate (above the strings).
    it.add(cube(98, 2.6, 2.0), "Gilt", loc=(18, -58.5, 91.0), bevel=0.5, seg=2)
    for x0, x1 in ((-34, -44), (-5, -14), (25, 20), (50, 49)):
        y1 = _rim_y(curve, x1) - 8
        it.add(sweep([(x0, -57.5, 92.2), (x1, y1, 92.2)], rect_profile(3.6, 2.6, 0.5)), "Gilt", bevel=0.4, seg=2)
        for t in (0.35, 0.7):
            cx, cy = x0 + (x1 - x0) * t, -57.5 + (y1 + 57.5) * t
            it.add(cylinder(2.6, 2.8, 16), "Gilt", loc=(cx, cy, 90.9), shade="auto")
    # Damper heads.
    for k in range(48):
        it.add(cube(1.6, 5.0, 3.4), "Wood_Light", loc=(-66 + k * 2.4, -49.5, 91.7))

    # --- Keyboard ---
    it.add(cube(122.6, 26, 5), lac, loc=(0, -93, bottom + 2.5), bevel=0.4, seg=2)          # key bed
    it.add(cube(122.6, 15, 2.5), "Plastic_Black", loc=(0, -97.5, 68.25))
    it.add(cube(122.6, 2.4, 4.2), lac, loc=(0, -106.3, 67.6), bevel=0.5, seg=2)            # key slip
    kw = 122.6 / 52
    x0 = -61.3 + kw / 2
    notes = "ABCDEFG"
    for i in range(52):
        x = x0 + i * kw
        it.add(cube(kw - 0.18, 15, 2.0), "Ivory", loc=(x, -97.5, 70.5), bevel=0.12, seg=1)
        if notes[i % 7] in "ACDFG" and i < 51:
            it.add(cube(kw * 0.55, 9, 1.3), "Plastic_Black", loc=(x + kw / 2, -94.0, 72.15), bevel=0.15, seg=1)
    it.add(cube(122.6, 1.2, 1.4), "Felt_Red", loc=(0, -89.4, 72.2))
    it.add(cube(122.6, 1.4, 8), lac, loc=(0, -84.2, 76.2), rot=(-15, 0, 0), bevel=0.4, seg=2)   # fallboard (open)
    # Music desk on the desk rail.
    it.add(cube(76, 5, 1.2), lac, loc=(0, -75.5, 92.1), bevel=0.3, seg=2)
    it.add(cube(76, 0.8, 2.2), lac, loc=(0, -77.6, 93.3), bevel=0.3, seg=2)
    it.add(cube(76, 1.4, 30), lac, loc=(0, -69.0, 106.4), rot=(-16, 0, 0), bevel=0.5, seg=2)
    it.add(cube(68, 0.5, 22), lac, loc=(0, -70.0, 106.6), rot=(-16, 0, 0), bevel=0.4, seg=3, profile=0.3)

    # --- Legs, pedal lyre ---
    for x, y in ((-67, -92), (67, -92), (-40, 88)):
        _grand_leg(it, x, y, lac)
    ly = -84.0
    it.add(cube(30, 9, 8.5), lac, loc=(0, ly, 10.25), bevel=1.2, seg=3)
    for sx in (-1, 1):
        path = spline([(sx * 6.5, ly, 14.0), (sx * 9.5, ly, 25), (sx * 10.0, ly, 38), (sx * 8.0, ly, 51), (sx * 6.0, ly, bottom + 0.2)], 4)
        it.add(sweep(path, rect_profile(2.2, 2.6, 0.6), up=(0, 1, 0)), lac, shade="auto")
        it.add(sweep([(sx * 10, ly + 4.4, 12.0), (sx * 12, -52, bottom + 0.2)], circle_profile(0.9, 10)), lac, shade="smooth")
    for px in (-7.0, 0.0, 7.0):
        it.add(sweep([(px, ly - 4.4, 9.6), (px, ly - 9.5, 8.6), (px, ly - 13.5, 9.2)], rect_profile(2.0, 0.8, 0.25),
                     scales=[1.0, 1.1, 1.35]), "Brass", shade="auto")

    # --- Lid (hinged on the straight left side), folded front flap, stick ---
    a = PIANO_LID_ANGLE
    lid_m = trs((-75.0, 0.0, top + 2.2), (0, -a, 0))  # pivot on the top edge: nothing swings outwards
    lid_poly = [(x + 75.0, y) for x, y in clip_y(outline, -62.0)]
    it.add(xf(slab(lid_poly, -2.2, 0.0), matrix=lid_m), lac, bevel=0.6, seg=2)
    flap = xf(slab(rounded_rect(143, 21, 0.8, 2), 0.0, 2.0), (75.5, -51.5, 0))  # folded back onto the lid
    it.add(xf(flap, matrix=lid_m), lac, bevel=0.5, seg=2)
    it.add(xf(xf(cylinder(0.45, 143, 12), (4.0, -62.0, 0.4), (0, 90, 0)), matrix=lid_m), "Brass", shade="smooth")
    for y in (-40.0, 20.0, 80.0):
        it.add(cylinder(0.7, 6.0, 14), "Brass", loc=(-74.4, y - 3.0, top + 1.5), rot=(-90, 0, 0), shade="smooth")
    ys = -28.0
    base = Vector((_rim_x(curve, ys) - 2.0, ys, top))
    tip = lid_m @ (Vector((138.0, ys, -2.2)) * CM) / CM
    it.add(sweep([tuple(base), tuple(tip)], circle_profile(1.1, 12)), lac, shade="smooth")
    it.add(cylinder(1.6, 1.2, 16), "Brass", loc=(base.x, base.y, top - 0.1), shade="auto")


# ---------------------------------------------------------------------------------------------
# Key board – wall piece 40 x 5 x 25, back at y = +2.5, origin bottom centre of the board
# ---------------------------------------------------------------------------------------------


def _key(it, hang, phi, kind, mat, dy):
    """One key hanging from its bow at `hang` (cm), swung by phi degrees in the wall plane."""
    m = trs(hang, (0, phi, 0))

    def put(bm, material, loc=(0, 0, 0), rot=(0, 0, 0), **kw):
        it.add(xf(xf(bm, loc, rot), matrix=m), material, **kw)

    if kind == "bit":  # old cellar key: big round bow, round shank, bit at the end
        put(scale_bm(torus(0.95, 0.28, 18, 6), 1, 1, 0.8), mat, (0, dy, -0.95), (90, 0, 0), shade="smooth")
        put(cylinder(0.24, 5.2, 10), mat, (0, dy, -1.9), (180, 0, 0), shade="smooth")
        put(cube(0.9, 0.22, 1.1), mat, (0.55, dy, -6.4), bevel=0.05, seg=1)
        put(cube(0.3, 0.24, 0.3), "Iron_Dark", (0.85, dy, -6.4))
        return
    if kind == "car":  # plastic head + steel blade
        put(ngon_prism(rounded_rect(2.0, 2.4, 0.7), 0.6), "Plastic_Black", (0, dy + 0.3, -1.9), (90, 0, 0), bevel=0.15, seg=2)
        put(scale_bm(torus(0.45, 0.14, 12, 5), 1, 1, 1), "Steel", (0, dy, -0.45), (90, 0, 0), shade="smooth")
        put(cube(0.7, 0.2, 3.4), "Steel", (0, dy, -4.6), bevel=0.05, seg=1)
        return
    # cylinder-lock key: ring bow, shoulder, toothed blade
    put(scale_bm(torus(0.75, 0.3, 16, 6), 1, 1, 0.38), mat, (0, dy, -0.75), (90, 0, 0), shade="smooth")
    put(cube(1.1, 0.22, 0.5), mat, (0, dy, -1.75), bevel=0.06, seg=1)
    put(cube(0.75, 0.2, 3.6), mat, (0, dy, -3.8), bevel=0.05, seg=1)
    for k, h in enumerate((0.25, 0.4, 0.2, 0.35)):
        put(cube(h, 0.2, 0.45), mat, (0.375 + h / 2, dy, -2.6 - k * 0.7))


@item("Deco_key_board")
def key_board(it: Item):
    wood = WALNUT
    it.add(cube(40, 2.0, 25), wood, loc=(0, 1.5, 12.5), bevel=0.6, seg=3, profile=0.6)
    # Blank brass plate and four screws.
    it.add(ngon_prism(rounded_rect(13, 3.4, 0.8), 0.2), "Brass", loc=(0, 0.5, 20.5), rot=(90, 0, 0), bevel=0.06, seg=1)
    for sx in (-1, 1):
        for z in (3.0, 22.0):
            it.add(sphere(0.5, 10, 4, sz=0.6), "Brass", loc=(sx * 17.5, 0.55, z), rot=(90, 0, 0), shade="smooth")
            it.add(cube(0.7, 0.1, 0.12), "Iron_Dark", loc=(sx * 17.5, -0.05, z))
    # Five J-hooks.
    zh = 13.0
    for x in (-16.0, -8.0, 0.0, 8.0, 16.0):
        it.add(cylinder(0.7, 0.3, 12), "Brass", loc=(x, 0.5, zh), rot=(90, 0, 0), shade="smooth")
        path = [(x, 0.3, zh), (x, -1.2, zh), (x, -1.8, zh + 0.2), (x, -2.1, zh + 0.8), (x, -2.05, zh + 1.4)]
        it.add(sweep(path, circle_profile(0.2, 8), up=(1, 0, 0)), "Brass", shade="smooth")
        it.add(sphere(0.25, 8, 4), "Brass", loc=(x, -2.05, zh + 1.3), shade="smooth")
    # Three key bunches: ring on the hook, keys on the ring, a tag.
    ring_r = 1.1
    bunches = [(-16.0, [("cyl", "Steel", -14), ("cyl", "Brass", 10)], "red"),
               (-8.0, [("bit", "Iron_Dark", -6), ("cyl", "Silver", 14)], "leather"),
               (8.0, [("car", "Steel", -12), ("cyl", "Brass", 8)], "disc")]
    for x, keys, tag in bunches:
        cz = zh + 0.2 - (ring_r - 0.11)
        it.add(torus(ring_r, 0.11, 24, 5), "Steel", loc=(x, -1.45, cz), rot=(90, 0, 0), shade="smooth")
        low = cz - ring_r
        for n, (kind, mat, phi) in enumerate(keys):
            _key(it, (x, 0, low + 0.3), phi, kind, mat, -1.15 - 0.45 * n)
        # Tag hanging from the ring, a little to the side.
        tx = x + 0.8
        if tag == "red":
            it.add(ngon_prism(rounded_rect(2.4, 4.2, 0.8), 0.3), "Plastic_Red", loc=(tx + 0.6, -2.05, low - 2.5), rot=(90, 8, 0),
                   bevel=0.08, seg=1)
            it.add(cube(1.8, 0.05, 2.4), "Paper", loc=(tx + 0.5, -2.37, low - 2.8), rot=(0, 8, 0), fit="y")
        elif tag == "leather":
            it.add(ngon_prism(rounded_rect(2.2, 5.0, 1.0), 0.25), "Leather_Brown", loc=(tx + 0.4, -2.1, low - 2.9),
                   rot=(90, 6, 0), bevel=0.08, seg=1)
            it.add(torus(0.35, 0.1, 12, 6), "Brass", loc=(tx + 0.1, -2.38, low - 0.75), rot=(90, 0, 0), shade="smooth")
        else:  # round brass tag behind the keys (closer to the board), offset to the side
            it.add(cylinder(1.2, 0.2, 20), "Brass", loc=(x + 1.9, -0.6, low - 1.3), rot=(90, 0, 0), bevel=0.06, seg=1)
            it.add(torus(0.3, 0.08, 10, 5), "Steel", loc=(x + 1.2, -0.7, low - 0.05), rot=(90, 0, 0), shade="smooth")
