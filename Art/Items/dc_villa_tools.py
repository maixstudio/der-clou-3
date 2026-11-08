"""Villa tools and small things (G8): bolt cutter, extension ladder, cordless drill, key ring, garage remote.

The crew finds them in the coach house (Remise). Exported as SM_<id>. Conventions: cm, origin bottom centre,
front = -Y, width along X.

- bolt_cutter and ladder lie flat, length along X (jaws / ladder feet at -X).
- cordless_drill stands on its battery, chuck towards +X, so its side view faces -Y.
- key_ring and garage_remote lie flat; the ring is at -X, the keys fan out towards +X.
Every builder ends with _recentre(), so the bounding box is centred on X/Y and starts at z = 0.
"""

import math

import bmesh
from mathutils import Euler, Matrix, Vector

from dc_items import item
from dc_items_small import squash
from dc_mesh import (CM, Item, arc_points, circle_profile, cube, cylinder, lathe, ngon_prism, puff, rect_profile, sweep,
                     tapered_box)

TO_X = (0, 90, 0)    # local +Z -> world +X (lathe/cylinder lying along X)
TO_MX = (0, -90, 0)  # local +Z -> world -X
TO_Y = (-90, 0, 0)   # local +Z -> world +Y
TO_MY = (90, 0, 0)   # local +Z -> world -Y


# ---------------------------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------------------------


def _recentre(it: Item):
    """Move all parts so the bounding box is centred on X/Y and its bottom lies on z = 0."""
    lo = [1e9, 1e9, 1e9]
    hi = [-1e9, -1e9, -1e9]
    for ob in it.parts:
        mw = ob.matrix_world
        for v in ob.data.vertices:
            w = mw @ v.co
            for i in range(3):
                lo[i] = min(lo[i], w[i])
                hi[i] = max(hi[i], w[i])
    off = Vector(((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, lo[2]))
    for ob in it.parts:
        ob.matrix_world = Matrix.Translation(-off) @ ob.matrix_world


def frame(pos=(0, 0, 0), yaw=0.0, pitch=0.0, roll=0.0):
    """Placement of a sub-assembly (cm, degrees): roll about X, then pitch about Y (+ = +X tips down), then yaw."""
    rot = Euler((math.radians(roll), math.radians(pitch), math.radians(yaw)), "XYZ").to_matrix().to_4x4()
    return Matrix.Translation(Vector(pos)) @ rot


def placed(it: Item, f, bm, mat, loc=(0, 0, 0), rot=(0, 0, 0), **kw):
    """Item.add with the part's loc/rot given in the local space of frame f."""
    m = f @ Matrix.Translation(Vector(loc)) @ Euler([math.radians(a) for a in rot], "XYZ").to_matrix().to_4x4()
    e = m.to_euler("XYZ")
    return it.add(bm, mat, loc=tuple(m.to_translation()), rot=tuple(math.degrees(a) for a in e), **kw)


def tri_caps(bm):
    """Triangulate n-gons (toothed key blades, jaws) so concave outlines survive the FBX import."""
    faces = [f for f in bm.faces if len(f.verts) > 4]
    if faces:
        bmesh.ops.triangulate(bm, faces=faces)
    return bm


def round_poly(pts, r, steps=3):
    """Round the corners of a 2D polygon (cm) with radius r."""
    out = []
    n = len(pts)
    for i in range(n):
        p = Vector(pts[i])
        a = Vector(pts[i - 1])
        b = Vector(pts[(i + 1) % n])
        u1, u2 = a - p, b - p
        l1, l2 = u1.length, u2.length
        u1.normalize()
        u2.normalize()
        ang = math.acos(max(-1.0, min(1.0, u1.dot(u2))))
        if r <= 0 or ang > math.radians(178):
            out.append((p.x, p.y))
            continue
        half = ang / 2
        d = min(r / math.tan(half), 0.45 * l1, 0.45 * l2)
        rr = d * math.tan(half)
        c = p + (u1 + u2).normalized() * (rr / math.sin(half))
        t1, t2 = p + u1 * d, p + u2 * d
        a1 = math.atan2(t1.y - c.y, t1.x - c.x)
        a2 = math.atan2(t2.y - c.y, t2.x - c.x)
        da = a2 - a1
        while da > math.pi:
            da -= 2 * math.pi
        while da < -math.pi:
            da += 2 * math.pi
        for k in range(steps + 1):
            aa = a1 + da * k / steps
            out.append((c.x + rr * math.cos(aa), c.y + rr * math.sin(aa)))
    return out


def _ray_poly(poly, c, ang):
    dx, dy = math.cos(ang), math.sin(ang)
    best = None
    for i in range(len(poly)):
        ax, ay = poly[i]
        bx, by = poly[(i + 1) % len(poly)]
        ex, ey = bx - ax, by - ay
        den = dx * ey - dy * ex
        if abs(den) < 1e-12:
            continue
        wx, wy = ax - c[0], ay - c[1]
        t = (wx * ey - wy * ex) / den
        s = (wx * dy - wy * dx) / den
        if t > 1e-9 and -1e-9 <= s <= 1 + 1e-9 and (best is None or t < best):
            best = t
    return c[0] + dx * best, c[1] + dy * best


def holed_plate(poly, hole_r, t, hole=(0.0, 0.0), max_gap=15.0):
    """Flat plate (outline poly, cm, star-shaped around the hole) with a round through-hole, z = 0..t."""
    cx, cy = hole
    angs = sorted({round(math.atan2(y - cy, x - cx) % (2 * math.pi), 6) for x, y in poly})
    filled = []
    for i, a in enumerate(angs):
        b = angs[(i + 1) % len(angs)] + (2 * math.pi if i == len(angs) - 1 else 0)
        filled.append(a)
        k = int((b - a) / math.radians(max_gap))
        for j in range(1, k + 1):
            filled.append(a + (b - a) * j / (k + 1))
    bm = bmesh.new()
    ib, itop, ob, otop = [], [], [], []
    for a in filled:
        ox, oy = _ray_poly(poly, (cx, cy), a)
        ix, iy = cx + hole_r * math.cos(a), cy + hole_r * math.sin(a)
        ib.append(bm.verts.new((ix * CM, iy * CM, 0.0)))
        itop.append(bm.verts.new((ix * CM, iy * CM, t * CM)))
        ob.append(bm.verts.new((ox * CM, oy * CM, 0.0)))
        otop.append(bm.verts.new((ox * CM, oy * CM, t * CM)))
    m = len(filled)
    for i in range(m):
        j = (i + 1) % m
        bm.faces.new((itop[i], otop[i], otop[j], itop[j]))
        bm.faces.new((ib[j], ob[j], ob[i], ib[i]))
        bm.faces.new((ob[i], ob[j], otop[j], otop[i]))
        bm.faces.new((ib[j], ib[i], itop[i], itop[j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def annulus(r_in, r_out, h, segments=20):
    """Tube ring (eyelet, grommet), z = 0..h."""
    return lathe([(r_in, 0), (r_out, 0), (r_out, h), (r_in, h), (r_in, 0)], segments, caps=False)


def dome(r, h, segments=16):
    """Rounded bolt head / rivet, z = 0..h."""
    return lathe([(0, 0), (r, 0), (r, h * 0.25), (r * 0.85, h * 0.65), (r * 0.5, h * 0.92), (0, h)], segments)


def hex_nut(r, h):
    return lathe([(0, 0), (r, 0), (r, h * 0.85), (r * 0.82, h), (0, h)], 6)


# ---------------------------------------------------------------------------------------------
# Bolt cutter – 75 x 15 x 4, lying, jaws at -X, closed
# ---------------------------------------------------------------------------------------------


@item("bolt_cutter")
def bolt_cutter(it: Item):
    zc = 2.0
    dark = "Iron_Dark"
    # Jaws: closed, the cutting edges meet on the centre line; the front is ground to a wedge.
    for s in (1, -1):
        pts = [(-37.5, 0.03), (-31.0, 0.03), (-28.0, 0.5), (-25.0, 0.95), (-22.8, 1.1), (-22.8, 3.15), (-26.0, 3.55),
               (-29.5, 3.45), (-32.5, 2.95), (-34.8, 2.2), (-36.5, 1.3), (-37.3, 0.55)]
        bm = ngon_prism([(x, y * s) for x, y in pts], 1.6)
        for v in bm.verts:
            x, y = v.co.x / CM, abs(v.co.y / CM)
            if x < -30.0:
                f = 0.3 + 0.7 * min(1.0, y / 2.0)
                v.co.z = 0.8 * CM + (v.co.z - 0.8 * CM) * f
        it.add(tri_caps(bm), "Steel", loc=(0, 0, zc - 0.8), bevel=0.12, seg=2)
    # Link plates above and below the jaws with the two jaw pivots.
    plate = round_poly([(-31.0, -4.1), (-25.0, -4.1), (-25.0, 4.1), (-31.0, 4.1)], 1.6, 4)
    for z0 in (zc + 0.8, zc - 0.8 - 0.55):
        it.add(ngon_prism(plate, 0.55), dark, loc=(0, 0, z0), bevel=0.15, seg=2)
    top, bottom = zc + 1.35, zc - 1.35
    for s in (1, -1):
        it.add(dome(0.75, 0.5), "Steel", loc=(-28.0, s * 2.35, top), shade="smooth")
        it.add(hex_nut(0.75, 0.5), "Steel", loc=(-28.0, s * 2.35, bottom), rot=(180, 0, 0))
    # Forged handle heads (the jaw tails sit in their slots), meeting at the centre pivot.
    for s in (1, -1):
        pts = [(-24.8, 0.55), (-24.8, 3.35), (-23.5, 3.9), (-20.5, 3.95), (-17.5, 3.3), (-14.5, 2.6), (-12.3, 2.15),
               (-12.3, 0.1), (-16.5, 0.1), (-19.0, 0.3), (-22.5, 0.5)]
        head = round_poly([(x, y * s) for x, y in pts], 0.6, 3)
        it.add(ngon_prism(head, 2.8), dark, loc=(0, 0, zc - 1.4), bevel=0.3, seg=2)
        # Adjusting screws at the jaw/handle joints: lock nut + slotted eccentric bolt end.
        it.add(hex_nut(0.75, 0.45), "Steel", loc=(-21.0, s * 2.3, zc + 1.4))
        it.add(dome(0.42, 0.22), "Steel", loc=(-21.0, s * 2.3, zc + 1.85), shade="smooth")
        it.add(cube(0.8, 0.12, 0.08), dark, loc=(-21.0, s * 2.3, zc + 2.03), rot=(0, 0, 35 * s))
        it.add(dome(0.7, 0.45), dark, loc=(-21.0, s * 2.3, zc - 1.4), rot=(180, 0, 0), shade="smooth")
    # Centre pivot of the two handles.
    it.add(dome(1.0, 0.5), "Steel_Satin", loc=(-15.0, 0, zc + 1.4), shade="smooth")
    it.add(hex_nut(0.9, 0.5), "Steel", loc=(-15.0, 0, zc - 1.4), rot=(180, 0, 0))
    # Handles: tubes with red grip sleeves (grooved, flared end).
    p0 = Vector((-12.6, 1.15))
    p1 = Vector((37.3, 5.5))
    d = (p1 - p0).normalized()
    grip_len = 25.0
    g0 = p1 - d * grip_len
    st = [(0.0, 1.32), (0.5, 1.62), (1.4, 1.72)]
    t = 2.2
    while t < 20.5:
        st += [(t, 1.72), (t + 0.12, 1.6), (t + 0.68, 1.6), (t + 0.8, 1.72)]
        t += 1.8
    st += [(22.0, 1.75), (23.0, 1.95), (23.6, 2.0), (24.7, 2.0), (24.95, 1.82), (25.0, 1.3)]
    for s in (1, -1):
        def at(v, z=zc):
            return (v.x, v.y * s, z)
        tube_end = g0 + d * 2.0
        it.add(sweep([at(p0 - d * 0.5), at(tube_end)], circle_profile(1.05, 16)), dark, shade="smooth")
        it.add(sweep([at(p0 + d * 0.2), at(p0 + d * 1.2)], circle_profile(1.3, 16)), dark, shade="auto", angle=40)  # ferrule
        pts = [at(g0 + d * tt) for tt, _ in st]
        it.add(sweep(pts, circle_profile(1.0, 16), scales=[sc for _, sc in st]), "Plastic_Red", shade="auto", angle=30)
    _recentre(it)


# ---------------------------------------------------------------------------------------------
# Extension ladder – 300 x 45 x 9, two sections pushed together, lying flat, feet at -X
# ---------------------------------------------------------------------------------------------

RAIL_W, RAIL_H = 2.6, 4.2


def _rail_profile(s):
    """Rail cross-section (sweep along +X, up = Z: profile x -> -Y, y -> +Z); groove on the outer side (s * Y)."""
    hw, hh, c, g, gh = RAIL_W / 2, RAIL_H / 2, 0.25, 0.3, 0.9
    pts = [(-hw + c, -hh), (hw - c, -hh), (hw, -hh + c), (hw, -gh), (hw - g, -gh + 0.15), (hw - g, gh - 0.15), (hw, gh),
           (hw, hh - c), (hw - c, hh), (-hw + c, hh), (-hw, hh - c), (-hw, -hh + c)]
    return [(-y * s, z) for y, z in pts]


def _rung_profile():
    """D-rung (sweep along +Y, up = Z: profile x -> +X, y -> +Z); ribbed tread towards the top end (+X)."""
    pts = [(1.4, -1.1)]
    for k in range(5):
        z0 = -1.1 + k * 0.44
        pts += [(1.4, z0 + 0.2), (1.25, z0 + 0.24), (1.25, z0 + 0.40), (1.4, z0 + 0.44)]
    pts += [(1.15, 1.3), (0.6, 1.33), (-0.2, 1.25), (-0.85, 1.0), (-1.3, 0.55), (-1.45, 0.0), (-1.3, -0.55),
            (-0.85, -1.0), (-0.2, -1.25), (0.6, -1.33), (1.15, -1.3)]
    return pts


@item("ladder")
def ladder(it: Item):
    al = "Aluminium"
    rung = _rung_profile()
    # (x0, x1, rail centre |y|, bottom z): base section below (wider), fly section on top, shifted 14 cm.
    sections = [(-150.0, 136.0, 20.9, 0.0), (-136.0, 150.0, 19.0, 4.5)]
    for k, (x0, x1, yr, z0) in enumerate(sections):
        zc = z0 + RAIL_H / 2
        xc = (x0 + x1) / 2
        r0 = x0 + (2.5 if k == 0 else 1.0)  # rail ends hide in the feet / end caps
        r1 = x1 - (1.0 if k == 0 else 2.0)
        for s in (1, -1):
            it.add(sweep([(r0, s * yr, zc), (r1, s * yr, zc)], _rail_profile(s)), al, shade="auto", angle=40)
        for j in range(10):
            x = xc + 28.0 * (j - 4.5)
            it.add(sweep([(x, -yr, zc), (x, yr, zc)], rung), al, shade="auto", angle=40)
            for s in (1, -1):
                # Flared collar on the inside of the rail, swaged washer on the outside.
                it.add(cube(3.5, 0.24, 3.1), al, loc=(x - 0.05, s * (yr - RAIL_W / 2 - 0.12), zc), bevel=0.1, seg=1)
                it.add(cylinder(1.15, 0.12, 14), al, loc=(x, s * (yr + RAIL_W / 2 - 0.04), zc), rot=TO_Y if s > 0 else TO_MY)
    for s in (1, -1):
        # Rubber feet on the base section (the sole is the -X face).
        it.add(cube(7.0, 3.1, 4.8), "Rubber_Black", loc=(-146.3, s * 20.9, 2.4), bevel=0.6, seg=3)
        for j in range(4):
            it.add(cube(0.4, 2.8, 0.5), "Rubber_Black", loc=(-149.85, s * 20.9, 0.9 + j * 1.0), bevel=0.1, seg=1)
        it.add(dome(0.5, 0.25), "Steel", loc=(-145.5, s * 22.45, 2.4), rot=TO_Y if s > 0 else TO_MY, shade="smooth")
        # End caps.
        it.add(cube(3.0, 2.9, 4.4), "Plastic_Black", loc=(134.6, s * 20.9, 2.2), bevel=0.4, seg=2)
        it.add(cube(3.0, 2.9, 4.4), "Plastic_Black", loc=(-134.6, s * 19.0, 6.6), bevel=0.4, seg=2)
        it.add(cube(4.0, 2.9, 4.4), "Plastic_Black", loc=(148.1, s * 19.0, 6.6), bevel=0.5, seg=2)
        # Guide brackets: at the top of the base section around the fly rails ...
        strap = rect_profile(0.3, 3.5)
        xb = 129.25
        path = [(xb, s * 22.35, 0.6), (xb, s * 22.35, 8.85), (xb, s * 17.25, 8.85), (xb, s * 17.25, 7.2)]
        it.add(sweep(path, strap, up=(1, 0, 0)), "Steel", shade="auto", angle=40)
        for z in (1.4, 3.0):
            it.add(dome(0.35, 0.18), "Steel", loc=(xb, s * 22.5, z), rot=TO_Y if s > 0 else TO_MY, shade="smooth")
        it.add(cube(3.5, 2.2, 0.28), "Plastic_Black", loc=(xb, s * 20.0, 4.35))  # slide pad
        # ... and at the bottom of the fly section, hooked into the groove of the base rails.
        xf = -129.0
        path = [(xf, s * 20.45, 7.3), (xf, s * 20.45, 4.35), (xf, s * 22.35, 4.35), (xf, s * 22.35, 1.5), (xf, s * 21.95, 1.5)]
        it.add(sweep(path, strap, up=(1, 0, 0)), "Steel", shade="auto", angle=40)
        it.add(dome(0.35, 0.18), "Steel", loc=(xf, s * 20.6, 6.3), rot=TO_Y if s > 0 else TO_MY, shade="smooth")
        it.add(cube(3.5, 1.8, 0.28), "Plastic_Black", loc=(xf, s * 19.3, 4.35))
        # Fall hooks: pivot on the inside of the fly rails, resting on the first base rung.
        y = s * 17.45
        hook = [(-128.4, y, 6.9), (-129.6, y, 5.7), (-130.55, y, 4.3), (-130.7, y, 3.0)]
        hook += arc_points((-133.0, y, 2.1), 2.3, 18, -50, 6, plane="xz")
        it.add(sweep(hook, rect_profile(1.0, 0.35), up=(0, 1, 0)), "Steel", shade="auto", angle=40)
        it.add(cylinder(0.4, 2.2, 12), "Steel", loc=(-128.4, s * 16.8, 6.9), rot=TO_Y if s > 0 else TO_MY, shade="smooth")
        it.add(hex_nut(0.6, 0.3), "Steel", loc=(-128.4, s * 17.27, 6.9), rot=TO_MY if s > 0 else TO_Y)
    # Warning label on the base rail (generic, no text).
    it.add(cube(16.0, 1.5, 0.05), "Plastic_Yellow", loc=(-90.0, 21.35, 4.22))
    it.add(cube(1.2, 1.0, 0.03), "Plastic_Black", loc=(-96.0, 21.35, 4.25), rot=(0, 0, 45))
    it.add(cube(8.0, 0.22, 0.03), "Plastic_Black", loc=(-88.0, 21.35, 4.25))
    _recentre(it)


# ---------------------------------------------------------------------------------------------
# Cordless drill – 25 x 8 x 22, standing on the battery, chuck towards +X
# ---------------------------------------------------------------------------------------------


@item("cordless_drill")
def cordless_drill(it: Item):
    yel, blk, rub = "Plastic_Yellow", "Plastic_Black", "Rubber_Black"
    ax = 17.85  # motor axis height
    x0 = -12.5  # rear end of the housing
    bx = -5.5   # battery centre
    # Battery pack with a yellow stripe, release button and charge gauge.
    it.add(cube(12.0, 8.0, 5.1), blk, loc=(bx, 0, 2.75), bevel=0.7, seg=3)
    ring = round_poly([(bx - 6.03, -4.03), (bx + 6.03, -4.03), (bx + 6.03, 4.03), (bx - 6.03, 4.03)], 0.72, 4)
    it.add(sweep([(x, y, 4.1) for x, y in ring], rect_profile(0.06, 0.45), closed=True), yel, shade="auto", angle=50)
    it.add(cube(11.2, 6.8, 0.7), blk, loc=(bx, 0, 5.45), bevel=0.25, seg=2)  # slide rails
    for px in (-4.7, 4.7):
        for py in (-2.8, 2.8):
            it.add(cube(1.6, 1.2, 0.22), rub, loc=(bx + px, py, 0.11), bevel=0.08, seg=1)
    it.add(cube(0.5, 3.2, 1.0), yel, loc=(bx + 6.05, 0, 4.85), bevel=0.2, seg=2)  # release button
    it.add(cube(0.12, 2.8, 0.9), "Plastic_Grey", loc=(bx - 6.04, 0, 2.6), bevel=0.04, seg=1)
    for py in (-0.75, 0.0, 0.75):
        it.add(cylinder(0.16, 0.08, 12), "LED_Green", loc=(bx - 6.08, py - 0.3, 2.6), rot=TO_MX)
    it.add(cylinder(0.3, 0.1, 12), rub, loc=(bx - 6.08, 1.0, 2.6), rot=TO_MX)
    # Foot of the handle with the work light.
    it.add(cube(11.2, 7.0, 2.2), yel, loc=(bx, 0, 6.9), bevel=0.8, seg=3)
    it.add(cylinder(0.42, 0.15, 16), "Glass", loc=(bx + 5.55, 0, 7.0), rot=TO_X, shade="smooth")
    it.add(annulus(0.42, 0.56, 0.12, 20), "Chrome", loc=(bx + 5.55, 0, 7.0), rot=TO_X, shade="smooth")
    # Grip: rubber overmould with a yellow front strip, flaring into a fairing under the motor housing.
    grip = [(-6.7, 0, 7.3), (-6.35, 0, 9.0), (-5.9, 0, 11.0), (-5.5, 0, 13.0), (-5.1, 0, 15.2)]
    gs = [1.0, 1.04, 1.05, 1.05, 1.12]
    it.add(sweep(grip, circle_profile(1.0, 20, sx=2.1, sy=2.6), up=(1, 0, 0), scales=gs), rub, shade="smooth")
    strip = [(x + 2.6 * sc - 0.45, 0, z) for (x, _, z), sc in zip(grip[:3], gs[:3])] + [(-3.4, 0, 12.4), (-3.3, 0, 13.3)]
    it.add(sweep(strip, circle_profile(1.0, 12, sx=1.15, sy=0.6), up=(1, 0, 0)), yel, shade="smooth")
    it.add(tapered_box(6.6, 5.6, 6.0, 4.9, 3.6), yel, loc=(-4.8, 0, 12.7), bevel=1.0, seg=3)  # fairing
    it.add(cube(1.3, 1.5, 2.2), blk, loc=(-1.95, 0, 12.15), rot=(0, -10, 0), bevel=0.45, seg=3)  # trigger
    it.add(cylinder(0.42, 6.0, 16), blk, loc=(-2.75, 3.0, 14.75), rot=TO_MY, shade="auto")  # forward/reverse
    # Motor housing, gearbox, torque ring, keyless chuck and a screwdriver bit (lathes along X).
    hx = (x0, 0, ax)
    it.add(lathe([(0, 0), (1.7, 0), (2.5, 0.15), (3.0, 0.5), (3.25, 0.95), (3.33, 1.4), (0, 1.4)], 40), rub, loc=hx, rot=TO_X,
           shade="smooth", post=squash(1, 0.96))
    it.add(lathe([(0, 1.3), (3.34, 1.3), (3.42, 3.0), (3.42, 7.8), (3.3, 9.4), (3.05, 10.5), (0, 10.5)], 40), yel, loc=hx,
           rot=TO_X, shade="smooth", post=squash(1, 0.96))
    it.add(lathe([(0, 10.4), (3.0, 10.4), (2.95, 12.6), (2.85, 14.6), (0, 14.6)], 40), yel, loc=hx, rot=TO_X, shade="smooth")
    it.add(lathe([(0, 14.5), (2.9, 14.5), (3.12, 14.9), (3.12, 17.0), (2.98, 17.35), (2.7, 17.6), (0, 17.6)], 48,
                 ridges=(24, 0.13, 2, 3)), blk, loc=hx, rot=TO_X, shade="auto", angle=50)
    it.add(lathe([(0, 17.6), (1.95, 17.6), (2.15, 17.9), (2.15, 20.2), (2.0, 20.7), (1.6, 21.25), (0, 21.25)], 40,
                 ridges=(20, 0.14, 2, 3)), blk, loc=hx, rot=TO_X, shade="auto", angle=50)
    it.add(lathe([(0, 21.2), (1.5, 21.2), (1.3, 22.0), (1.0, 22.45), (0.55, 22.6), (0, 22.6)], 32), "Steel", loc=hx, rot=TO_X,
           shade="smooth")
    it.add(lathe([(0, 0), (0.34, 0), (0.34, 1.55), (0.2, 1.8), (0.2, 2.0), (0, 2.42)], 6), "Steel", loc=(x0 + 22.55, 0, ax),
           rot=TO_X, shade="flat")  # hex bit with a pointed tip
    it.add(cube(0.5, 0.3, 0.12), "Plastic_White", loc=(x0 + 14.1, 0, ax + 2.92), bevel=0.04, seg=1)  # torque mark
    # Gear selector on top, vents at the back, seam of the housing halves.
    it.add(cube(2.6, 1.7, 0.5), blk, loc=(-3.4, 0, 21.18), bevel=0.2, seg=2)
    it.add(cube(0.9, 1.1, 0.7), yel, loc=(-2.8, 0, 21.65), bevel=0.22, seg=2)
    for k in range(4):
        for s in (1, -1):
            it.add(cube(0.28, 0.3, 2.6), blk, loc=(-10.4 + k * 0.7, s * 3.2, ax), bevel=0.08, seg=1)
    _recentre(it)


# ---------------------------------------------------------------------------------------------
# Key ring – 10 x 5 x 1.5, lying flat: split ring, security key, warded key, car key, leather tag
# ---------------------------------------------------------------------------------------------

RING_R = 1.6


def _ring_point(theta):
    a = math.radians(theta)
    return RING_R * math.cos(a), RING_R * math.sin(a)


def _security_key(it, f):
    """Profile-cylinder key with a red plastic cap. Local: hole at the origin, blade along +X."""
    cap = round_poly([(-0.55, -0.55), (-0.3, -0.82), (1.5, -0.8), (1.8, -0.55), (1.8, 0.55), (1.5, 0.8), (-0.3, 0.82),
                      (-0.55, 0.55)], 0.18, 3)
    placed(it, f, holed_plate(cap, 0.28, 0.42), "Plastic_Red", bevel=0.1, seg=2)
    placed(it, f, annulus(0.26, 0.36, 0.44, 20), "Silver", loc=(0, 0, -0.01), shade="smooth")
    pts = [(1.7, -0.4), (4.4, -0.4), (4.6, -0.2), (4.6, 0.1), (4.42, 0.32)]
    for x, dep in ((4.1, 0.2), (3.65, 0.08), (3.2, 0.24), (2.75, 0.12), (2.3, 0.2)):
        pts += [(x + 0.17, 0.4), (x, 0.4 - dep), (x - 0.17, 0.4)]
    pts += [(1.7, 0.4)]
    placed(it, f, tri_caps(ngon_prism(pts, 0.22)), "Silver", loc=(0, 0, 0.1), bevel=0.03, seg=1)
    placed(it, f, cube(0.25, 1.15, 0.22), "Silver", loc=(1.92, 0, 0.21), bevel=0.04, seg=1)  # stop shoulder
    placed(it, f, cube(2.5, 0.06, 0.02), "Iron_Dark", loc=(3.25, -0.12, 0.325))  # profile grooves
    placed(it, f, cube(2.2, 0.06, 0.02), "Iron_Dark", loc=(3.1, 0.12, 0.325))


def _warded_key(it, f):
    """Old warded (Buntbart) key in brass: oval bow, collar, round shank, bit towards local -Y."""
    z = 0.2
    bow = [(0.5 + 0.72 * math.cos(2 * math.pi * i / 28), 0.55 * math.sin(2 * math.pi * i / 28), z) for i in range(28)]
    placed(it, f, sweep(bow, circle_profile(0.15, 8), closed=True), "Brass", shade="smooth")
    collar = [(0, 0), (0.26, 0), (0.29, 0.05), (0.29, 0.13), (0.23, 0.17), (0.23, 0.25), (0.29, 0.29), (0.29, 0.37),
              (0.25, 0.42), (0, 0.42)]
    placed(it, f, lathe(collar, 16), "Brass", loc=(1.15, 0, z), rot=TO_X, shade="auto", angle=50)
    placed(it, f, cylinder(0.19, 3.25, 12), "Brass", loc=(1.55, 0, z), rot=TO_X, shade="smooth")
    placed(it, f, lathe([(0, 0), (0.19, 0), (0.15, 0.22), (0, 0.28)], 12), "Brass", loc=(4.8, 0, z), rot=TO_X, shade="smooth")
    bit = [(3.95, 0.0), (4.8, 0.0), (4.8, -0.9), (4.62, -0.9), (4.62, -0.6), (4.48, -0.6), (4.48, -0.9), (4.25, -0.9),
           (4.25, -0.72), (4.12, -0.72), (4.12, -0.9), (3.95, -0.9)]
    placed(it, f, tri_caps(ngon_prism(bit, 0.2)), "Brass", loc=(0, 0, z - 0.1), bevel=0.03, seg=1)


def _car_key(it, f):
    """Remote fob with an opened flip blade. Local: ring hole at the origin, blade along +X."""
    t = 1.35
    fob = round_poly([(-0.5, -0.55), (-0.2, -0.92), (0.8, -0.98), (2.6, -0.9), (3.05, -0.55), (3.15, 0.0), (3.05, 0.55),
                      (2.6, 0.9), (0.8, 0.98), (-0.2, 0.92), (-0.5, 0.55)], 0.3, 3)
    placed(it, f, holed_plate(fob, 0.24, t), "Plastic_Black", bevel=0.3, seg=3, post=puff(0.06))
    placed(it, f, annulus(0.23, 0.32, t + 0.02, 20), "Chrome", loc=(0, 0, -0.01), shade="smooth")
    for x in (1.25, 2.2):
        placed(it, f, cylinder(0.36, 0.14, 20), "Rubber_Black", loc=(x, 0, t - 0.06), shade="auto", angle=50, post=squash(1, 1.3))
    placed(it, f, cube(0.18, 1.3, 0.55), "Chrome", loc=(3.08, 0, t / 2), bevel=0.06, seg=1)  # blade pivot bezel
    blade = [(3.0, -0.36), (5.05, -0.36), (5.3, -0.14), (5.3, 0.14), (5.05, 0.36), (3.0, 0.36)]
    placed(it, f, ngon_prism(blade, 0.28), "Steel", loc=(0, 0, t / 2 - 0.14), bevel=0.03, seg=1)
    wave = [(3.3 + i * 0.1, 0.13 * math.sin(i * 0.9), t / 2 + 0.15) for i in range(19)]
    placed(it, f, sweep(wave, rect_profile(0.12, 0.025)), "Iron_Dark", shade="smooth")  # laser-cut track


def _leather_tag(it, f):
    """Small leather tag with a brass grommet and stitching. Local: hole at the origin, tag along +X."""
    tag = round_poly([(-0.4, -0.62), (1.6, -0.62), (2.0, -0.3), (2.0, 0.3), (1.6, 0.62), (-0.4, 0.62)], 0.3, 3)
    placed(it, f, holed_plate(tag, 0.17, 0.26), "Leather_Brown", bevel=0.07, seg=2)
    placed(it, f, annulus(0.16, 0.29, 0.3, 20), "Brass", loc=(0, 0, -0.02), shade="smooth")
    stitch = round_poly([(0.45, -0.44), (1.5, -0.44), (1.8, -0.2), (1.8, 0.2), (1.5, 0.44), (0.45, 0.44)], 0.15, 2)
    placed(it, f, sweep([(x, y, 0.27) for x, y in stitch], rect_profile(0.045, 0.02), closed=True), "Fabric_Cream", shade="smooth")


@item("key_ring")
def key_ring(it: Item):
    # Split ring: two touching coils.
    coil = [(RING_R * math.cos(math.radians(i * 10)), RING_R * math.sin(math.radians(i * 10)), 0.09 + 0.18 * i / 36)
            for i in range(73)]
    it.add(sweep(coil, circle_profile(0.09, 8)), "Steel", shade="smooth")
    # Keys hang on the ring with their holes and fan out towards +X; the tag points to -X.
    hx, hy = _ring_point(-65)
    _car_key(it, frame((hx, hy, 0), yaw=-6))
    hx, hy = _ring_point(8)
    _security_key(it, frame((hx, hy, 0), yaw=3))
    hx, hy = _ring_point(70)
    _warded_key(it, frame((hx, hy, 0), yaw=10))
    _leather_tag(it, frame(_ring_point(180) + (0,), yaw=180))
    _recentre(it)


# ---------------------------------------------------------------------------------------------
# Garage remote – 6 x 3 x 1.5, lying flat, eyelet at -X
# ---------------------------------------------------------------------------------------------


@item("garage_remote")
def garage_remote(it: Item):
    blk = "Plastic_Black"
    # Pebble-shaped body; a thin parting line marks the two shell halves.
    it.add(cube(5.2, 3.0, 1.32, cuts=2), blk, loc=(0.4, 0, 0.66), bevel=0.5, seg=4, post=puff(0.06))
    seam = round_poly([(-2.205, -1.505), (3.005, -1.505), (3.005, 1.505), (-2.205, 1.505)], 0.44, 6)
    it.add(sweep([(x, y, 0.62) for x, y in seam], rect_profile(0.025, 0.035), closed=True), "Plastic_Grey", shade="smooth")
    it.add(cube(2.2, 1.6, 0.03), "Plastic_Grey", loc=(0.9, 0, 0.005), bevel=0.01, seg=1)  # battery lid line (bottom)
    # Lug with a metal eyelet for the key ring.
    lug = round_poly([(-3.0, -0.8), (-1.9, -0.8), (-1.9, 0.8), (-3.0, 0.8)], 0.55, 4)
    it.add(holed_plate(lug, 0.3, 0.55, hole=(-2.55, 0.0)), blk, loc=(0, 0, 0.4), bevel=0.12, seg=2)
    it.add(annulus(0.28, 0.36, 0.59, 20), "Chrome", loc=(-2.55, 0, 0.38), shade="smooth")
    # Two buttons in rubber rims, marked with one and two dots.
    for k, x in enumerate((0.35, 1.85)):
        it.add(cube(1.38, 1.78, 0.1), "Rubber_Black", loc=(x, 0, 1.33), bevel=0.12, seg=2)
        it.add(cube(1.2, 1.6, 0.3), "Plastic_Grey", loc=(x, 0, 1.3), bevel=0.12, seg=3, post=puff(0.02))
        for j in range(k + 1):
            it.add(cylinder(0.11, 0.03, 12), "Plastic_White", loc=(x, (j - k / 2) * 0.4, 1.45))
    # Red transmit LED in a small bezel.
    it.add(lathe([(0, 0), (0.17, 0), (0.17, 0.04), (0.12, 0.1), (0, 0.12)], 16), "LED_Red", loc=(-1.4, 0, 1.33), shade="smooth")
    it.add(annulus(0.17, 0.24, 0.06, 16), "Chrome", loc=(-1.4, 0, 1.32), shade="smooth")
    _recentre(it)
