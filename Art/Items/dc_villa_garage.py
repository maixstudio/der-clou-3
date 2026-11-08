"""Villa G6 – Remise (garage, workshop, tool shed): workbench, two cars, lawn mower, garden tool rack.

Exported as SM_<id>. Conventions: cm, origin bottom centre, front = -Y, width along X.
Cars: length along Y, front at -Y. Generic shapes only – no real makes, badges or recognisable grilles.

Car bodies are lofted from cross-sections: for every station y a rounded rectangle (flat bottom, lower corner,
side curve, upper corner, crowned top) is built; consecutive sections are joined. Pillars, roof, trims and lights
are thin "patches" that follow the same sections slightly above the surface, so they always sit on the body.
"""

import bisect
import math

import bmesh
from mathutils import Vector

from dc_items import item
from dc_items_small import squash
from dc_mesh import CM, Item, circle_profile, cube, cylinder, lathe, puff, rect_profile, sphere, sweep, torus, wobble


# ---------------------------------------------------------------------------------------------
# Small maths helpers
# ---------------------------------------------------------------------------------------------


def smoothstep(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


def pchip(points):
    """Smooth monotone curve through (t, value) points (no overshoot), clamped outside."""
    pts = sorted(points)
    xs = [p[0] for p in pts]
    vs = [p[1] for p in pts]
    n = len(xs)
    d = [(vs[i + 1] - vs[i]) / (xs[i + 1] - xs[i]) for i in range(n - 1)]
    m = [0.0] * n
    m[0], m[-1] = d[0], d[-1]
    for i in range(1, n - 1):
        if d[i - 1] * d[i] <= 0:
            m[i] = 0.0
        else:
            h0, h1 = xs[i] - xs[i - 1], xs[i + 1] - xs[i]
            w1, w2 = 2 * h1 + h0, h1 + 2 * h0
            m[i] = (w1 + w2) / (w1 / d[i - 1] + w2 / d[i])

    def f(x):
        if x <= xs[0]:
            return vs[0]
        if x >= xs[-1]:
            return vs[-1]
        i = min(bisect.bisect_right(xs, x) - 1, n - 2)
        h = xs[i + 1] - xs[i]
        t = (x - xs[i]) / h
        return ((1 + 2 * t) * (1 - t) ** 2 * vs[i] + t * (1 - t) ** 2 * h * m[i] + t * t * (3 - 2 * t) * vs[i + 1]
                + t * t * (t - 1) * h * m[i + 1])

    return f


def plan_round(y, w, y0, y1, r0, r1):
    """Half-width w with rounded corners (radius r0 at the y0 end, r1 at the y1 end) in plan view."""
    if r0 > 0 and y < y0 + r0:
        d = min(1.0, (y0 + r0 - y) / r0)
        return w - r0 + r0 * math.sqrt(max(0.0, 1 - d * d))
    if r1 > 0 and y > y1 - r1:
        d = min(1.0, (y - (y1 - r1)) / r1)
        return w - r1 + r1 * math.sqrt(max(0.0, 1 - d * d))
    return w


def arch_z0(y, base, yw, ra, zc, ramp=1.0):
    """Underside height with a wheel arch (semicircle of radius ra around (yw, zc))."""
    d = abs(y - yw)
    if d <= ra:
        return max(base, zc + math.sqrt(ra * ra - d * d))
    if d < ra + ramp:
        return max(base, base + (zc - base) * (ra + ramp - d) / ramp)
    return base


# ---------------------------------------------------------------------------------------------
# Lofted bodies
# ---------------------------------------------------------------------------------------------


class Sec:
    """Cross-section in the XZ plane: rounded rectangle with a crowned top.

    w(z) gives the side's half-width at height z (bulge, tumblehome). Parameter u runs 0..5 on the right half
    (0 bottom centre, 1 lower corner, 2 side, 3 upper corner, 4 top, 5 top centre) and 5..10 back down the left.
    """

    def __init__(self, z0, z1, w, rb, rt, crown, xc=0.0):
        crown = min(crown, 0.25 * max(z1 - z0, 0.0))
        z1s = z1 - crown
        h = max(z1s - z0, 0.05)
        wmin = min(w(z0), w(z1s))
        rb, rt = min(rb, 0.98 * wmin), min(rt, 0.98 * wmin)  # narrow sections: corners never wider than the section
        k = min(1.0, 0.95 * h / max(rb + rt, 1e-6))
        self.rb, self.rt, self.crown, self.z0, self.z1s, self.w, self.xc = rb * k, rt * k, crown, z0, z1s, w, xc
        self.zlo = z0 + self.rb
        self.zhi = z1s - self.rt
        self.xb = max(w(self.zlo) - self.rb, 0.02)
        self.xt = max(w(self.zhi) - self.rt, 0.02)

    def half(self, u):
        k = min(int(u), 4)
        f = u - k
        if k == 0:
            return self.xb * f, self.z0
        if k == 1:
            a = math.radians(-90 + 90 * f)
            return self.xb + self.rb * math.cos(a), self.zlo + self.rb * math.sin(a)
        if k == 2:
            z = self.zlo + (self.zhi - self.zlo) * f
            return self.w(z), z
        if k == 3:
            a = math.radians(90 * f)
            return self.xt + self.rt * math.cos(a), self.zhi + self.rt * math.sin(a)
        x = self.xt * (1 - f)
        return x, self.z1s + self.crown * (1 - (x / self.xt) ** 2)

    def point(self, u):
        u = u % 10.0
        if u > 5:
            x, z = self.half(10 - u)
            return self.xc - x, z
        x, z = self.half(u)
        return self.xc + x, z

    def normal(self, u, e=1e-3):
        x0, z0 = self.point(u - e)
        x1, z1 = self.point(u + e)
        tx, tz = x1 - x0, z1 - z0
        ln = math.hypot(tx, tz) or 1.0
        return tz / ln, -tx / ln


def ring_us(counts):
    half = [k + i / c for k, c in enumerate(counts) for i in range(c)]
    return half + [5.0] + [10.0 - u for u in reversed(half[1:])]


def stations(y0, y1, step, extra=()):
    n = max(1, int(math.ceil((y1 - y0) / step)))
    ys = {y0 + (y1 - y0) * i / n for i in range(n + 1)}
    ys.update(y for y in extra if y0 <= y <= y1)
    out = []
    for y in sorted(ys):
        if not out or y - out[-1] > 0.05:
            out.append(y)
    return out


def end_stations(y_end, r, inward, n=8):
    """Stations crowded towards a rounded end (inward = +1 at the front end, -1 at the rear end)."""
    return [y_end + inward * r * (1 - math.cos(math.radians(90 * i / n))) for i in range(n + 1)]


def arch_stations(yw, ra, ramp=1.0, n=12):
    out = [yw - ra - ramp, yw + ra + ramp]
    out += [yw + ra * math.cos(math.radians(180 * i / n)) for i in range(n + 1)]
    return out


class Loft:
    def __init__(self, sec, ys, counts=(3, 4, 6, 6, 6)):
        self.sec, self.ys, self.us = sec, ys, ring_us(counts)

    def mesh(self):
        bm = bmesh.new()
        rings = []
        for y in self.ys:
            s = self.sec(y)
            rings.append([bm.verts.new(Vector((x, y, z)) * CM) for x, z in (s.point(u) for u in self.us)])
        n = len(self.us)
        for a, b in zip(rings, rings[1:]):
            for k in range(n):
                bm.faces.new((a[k], a[(k + 1) % n], b[(k + 1) % n], b[k]))
        bm.faces.new(list(reversed(rings[0])))
        bm.faces.new(rings[-1])
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
        return bm

    def x_at(self, y, z, outer=+1):
        """Outermost x of the surface at (y, z) on the +X (outer=+1) or -X side, None if not hit."""
        if y < self.ys[0] or y > self.ys[-1]:
            return None
        s = self.sec(y)
        best = None
        rng = range(0, 201) if outer > 0 else range(200, 401)
        prev = None
        for i in rng:
            cur = s.point(5 * i / 200)
            if prev is not None and (prev[1] - z) * (cur[1] - z) <= 0 and prev[1] != cur[1]:
                t = (z - prev[1]) / (cur[1] - prev[1])
                x = prev[0] + (cur[0] - prev[0]) * t
                if best is None or x * outer > best * outer:
                    best = x
            prev = cur
        return best


def thick_grid(outer, inner):
    bm = bmesh.new()
    O = [[bm.verts.new(p) for p in row] for row in outer]
    I = [[bm.verts.new(p) for p in row] for row in inner]
    R, C = len(O), len(O[0])
    for r in range(R - 1):
        for c in range(C - 1):
            bm.faces.new((O[r][c], O[r][c + 1], O[r + 1][c + 1], O[r + 1][c]))
            bm.faces.new((I[r][c], I[r + 1][c], I[r + 1][c + 1], I[r][c + 1]))
    for r in range(R - 1):
        for c in (0, C - 1):
            bm.faces.new((O[r][c], O[r + 1][c], I[r + 1][c], I[r][c]))
    for c in range(C - 1):
        for r in (0, R - 1):
            bm.faces.new((O[r][c], I[r][c], I[r][c + 1], O[r][c + 1]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def patch(loft, y0, y1, u0, u1, nu=4, out=0.3, inn=0.3):
    """Thin shell on a loft between stations y0..y1 and section parameters u0..u1 (numbers or f(sec, y))."""
    ys = [y0] + [y for y in loft.ys if y0 + 0.05 < y < y1 - 0.05] + [y1]
    outer, inner = [], []
    for y in ys:
        s = loft.sec(y)
        a = u0(s, y) if callable(u0) else u0
        b = u1(s, y) if callable(u1) else u1
        ro, ri = [], []
        for j in range(nu + 1):
            u = a + (b - a) * j / nu
            (x, z), (nx, nz) = s.point(u), s.normal(u)
            ro.append(Vector((x + nx * out, y, z + nz * out)) * CM)
            ri.append(Vector((x - nx * inn, y, z - nz * inn)) * CM)
        outer.append(ro)
        inner.append(ri)
    return thick_grid(outer, inner)


def mirror_u(u0, u1):
    """The same band on the left side of a symmetric section."""
    return 10.0 - u1, 10.0 - u0


def both(it, loft, y0, y1, u0, u1, mat, nu=4, out=0.3, inn=0.3, angle=40):
    it.add(patch(loft, y0, y1, u0, u1, nu, out, inn), mat, shade="auto", angle=angle)
    m0, m1 = mirror_u(u0, u1)
    it.add(patch(loft, y0, y1, m0, m1, nu, out, inn), mat, shade="auto", angle=angle)


def outer_x(lofts, y, z, side=+1):
    xs = [x for x in (lf.x_at(y, z, side) for lf in lofts) if x is not None]
    if not xs:
        return None
    return max(xs) if side > 0 else min(xs)


# ---------------------------------------------------------------------------------------------
# Generic mesh helpers
# ---------------------------------------------------------------------------------------------


def revolve(profile, segments=48, ridges=None):
    """Closed profile loop [(r, z), ...] revolved around Z (tyres, rims) – no seam, no caps.

    ridges: (count, depth, min_r) – radial tread blocks on profile points with r >= min_r.
    """
    bm = bmesh.new()
    rings = []
    for r, z in profile:
        ring = []
        for s in range(segments):
            a = 2 * math.pi * s / segments
            rr = r
            if ridges and r >= ridges[2]:
                rr = r - ridges[1] * (0.5 + 0.5 * math.copysign(1.0, math.cos(a * ridges[0])))
            ring.append(bm.verts.new(Vector((rr * math.cos(a), rr * math.sin(a), z)) * CM))
        rings.append(ring)
    m = len(rings)
    for i in range(m):
        a, b = rings[i], rings[(i + 1) % m]
        for s in range(segments):
            bm.faces.new((a[s], a[(s + 1) % segments], b[(s + 1) % segments], b[s]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def prism(poly, axis, a0, a1):
    """2D polygon extruded along an axis; poly is (y, z) for 'x', (x, z) for 'y', (x, y) for 'z'."""
    bm = bmesh.new()

    def P(p, a):
        u, v = p
        if axis == "x":
            return Vector((a, u, v)) * CM
        if axis == "y":
            return Vector((u, a, v)) * CM
        return Vector((u, v, a)) * CM

    bot = [bm.verts.new(P(p, a0)) for p in poly]
    top = [bm.verts.new(P(p, a1)) for p in poly]
    caps = [bm.faces.new(bot), bm.faces.new(list(reversed(top)))]
    n = len(poly)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((bot[i], bot[j], top[j], top[i]))
    bmesh.ops.triangulate(bm, faces=caps)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def quads(rects):
    """Many flat quads (x0, x1, y, z0, z1) facing -Y in one mesh (pegboard holes)."""
    bm = bmesh.new()
    for x0, x1, y, z0, z1 in rects:
        vs = [bm.verts.new(Vector(p) * CM) for p in ((x0, y, z0), (x1, y, z0), (x1, y, z1), (x0, y, z1))]
        bm.faces.new(vs)
    return bm


AXIS_ROT = {"+x": (0, 90, 0), "-x": (0, -90, 0), "+y": (-90, 0, 0), "-y": (90, 0, 0), "+z": (0, 0, 0)}


# ---------------------------------------------------------------------------------------------
# Wheels
# ---------------------------------------------------------------------------------------------


def wheel_modern(it, x, y, side):
    """19-inch alloy on a 70-cm tyre; local +Z = outward."""
    rot = (0, 90 * side, 0)
    loc = (x, y, 35.0)
    tyre = [(23.5, -10.6), (27.0, -11.7), (31.0, -11.9), (33.7, -11.1), (34.7, -9.6), (35.0, -8.4), (35.0, -5.4),
            (34.3, -5.1), (34.3, -3.6), (35.0, -3.3), (35.0, 3.3), (34.3, 3.6), (34.3, 5.1), (35.0, 5.4), (35.0, 8.4),
            (34.7, 9.6), (33.7, 11.1), (31.0, 11.9), (27.0, 11.7), (23.5, 10.6)]
    it.add(revolve(tyre, 48), "Rubber_Black", loc=loc, rot=rot, shade="auto", angle=40)
    rim = [(22.6, -10.4), (24.2, -10.0), (24.2, -8.8), (22.9, -8.3), (22.4, 0.0), (22.8, 8.0), (24.2, 9.2), (24.5, 10.2),
           (23.9, 10.8), (22.5, 10.3), (21.5, 8.8), (21.3, 0.0), (21.9, -9.4)]
    it.add(revolve(rim, 48), "Aluminium", loc=loc, rot=rot, shade="auto", angle=40)
    it.add(cylinder(21.6, 0.6, 40), "Plastic_Black", loc=loc, rot=rot, shade="smooth")  # dark barrel behind the spokes
    # Brake disc and calliper (seen through the spokes).
    it.add(lathe([(0, 0), (17, 0), (17, 2.4), (0, 2.4)], 40), "Steel", loc=_wl(loc, side, 1.2), rot=rot, shade="auto")
    cal = (x + side * 3.5, y + 11.5, 35 + 9.5)
    it.add(cube(5.5, 7, 10), "Paint_Red", loc=cal, rot=(35, 0, 0), bevel=1.2, seg=2)
    # Five twin spokes, slightly concave, plus hub with nuts.
    for k in range(5):
        for d in (-7.5, 7.5):
            a = math.radians(72 * k + d + 90)
            pts = [(r * math.cos(a), r * math.sin(a), z) for r, z in ((6.5, 7.6), (14.0, 8.6), (21.8, 9.6))]
            it.add(sweep(pts, rect_profile(2.2, 1.7, 0.45), up=(0, 0, 1)), "Aluminium", loc=loc, rot=rot, shade="auto", angle=40)
    it.add(lathe([(0, 0), (7.2, 0), (7.2, 7.4), (6.6, 8.4), (0, 8.6)], 32), "Aluminium", loc=loc, rot=rot, shade="auto", angle=40)
    it.add(lathe([(0, 0), (3.0, 0), (3.0, 9.0), (2.6, 9.3), (0, 9.3)], 24), "Iron_Dark", loc=loc, rot=rot, shade="auto")
    for k in range(5):
        a = math.radians(72 * k + 126)
        it.add(cylinder(0.75, 9.0, 6), "Chrome", loc=(x + side * 0.0, y + 4.8 * math.cos(a), 35 + 4.8 * math.sin(a)), rot=rot,
               shade="flat")


def _wl(loc, side, local_z):
    return (loc[0] + side * local_z, loc[1], loc[2])


def wheel_classic(it, x, y, side, body_mat):
    """15-inch steel wheel in body colour, big chrome hubcap, whitewall tyre (radius 36)."""
    rot = (0, 90 * side, 0)
    loc = (x, y, 36.0)
    tyre = [(22.0, -9.0), (27.5, -9.9), (32.0, -9.6), (34.7, -8.2), (35.8, -5.2), (36.0, 0.0), (35.8, 5.2), (34.7, 8.2),
            (32.0, 9.6), (27.5, 9.9), (22.0, 9.0)]
    it.add(revolve(tyre, 36, ridges=(36, 0.25, 35.7)), "Rubber_Black", loc=loc, rot=rot, shade="auto", angle=45)
    white = [(24.5, 9.72), (27.5, 10.04), (31.6, 9.78), (31.6, 9.95), (27.5, 10.2), (24.5, 9.9)]
    it.add(revolve(white, 36), "Paint_White", loc=loc, rot=rot, shade="smooth")
    steel = [(21.0, -8.6), (22.6, -8.8), (22.6, 8.6), (22.0, 9.5), (20.4, 9.2), (19.6, 6.0), (19.4, -6.0)]
    it.add(revolve(steel, 36), body_mat, loc=loc, rot=rot, shade="auto", angle=40)
    it.add(cylinder(20.0, 0.5, 40), "Plastic_Black", loc=_wl(loc, side, -4), rot=rot, shade="smooth")
    cap = [(0, 0), (18.8, 0), (19.0, 9.6), (18.4, 10.6), (16.8, 11.0), (12.5, 11.8), (8.0, 12.9), (4.2, 13.6), (3.6, 13.6),
           (3.4, 14.1), (0, 14.3)]
    it.add(lathe(cap, 36), "Chrome", loc=loc, rot=rot, shade="smooth")
    it.add(lathe([(0, 0), (3.3, 0), (3.3, 0.5), (0, 0.7)], 20), body_mat, loc=_wl(loc, side, 13.75), rot=rot, shade="smooth")
    it.add(torus(15.2, 0.35, 40, 4), "Iron_Dark", loc=_wl(loc, side, 11.45), rot=rot, shade="smooth")


def arch_liner(it, yw, ra, zc, z_floor, x_half):
    poly = [(yw + ra, z_floor)] + [(yw + ra * math.cos(math.radians(a)), zc + ra * math.sin(math.radians(a)))
                                   for a in range(0, 181, 12)] + [(yw - ra, z_floor)]
    it.add(prism(poly, "x", -x_half, x_half), "Plastic_Black", shade="flat")


# ---------------------------------------------------------------------------------------------
# Modern car: four-door crossover saloon, black
# ---------------------------------------------------------------------------------------------

MOD_AXLES = (-128.0, 147.0)
MOD_ARCH = 38.5
MOD_WHEEL_Z = 35.0
MOD_TRACK = 73.0

MOD_WS, MOD_RW = -88.0, 172.0   # windscreen base, rear window base
mod_z1 = pchip([(-215, 60), (-214, 67), (-212, 74), (-208, 80), (-202, 84.5), (-192, 87.5), (-170, 90.5), (-130, 94.5),
                (MOD_WS, 100), (0, 103), (100, 105), (MOD_RW, 107), (198, 107), (207, 106), (211.5, 103), (214, 98), (215, 92)])
mod_roof = pchip([(MOD_WS, 100.6), (-80, 105), (-62, 118), (-40, 134), (-20, 147), (-5, 154), (10, 157.5), (60, 158.5),
                  (95, 157), (110, 153), (125, 145), (145, 131), (163, 116), (MOD_RW, 107.6)])
mod_cab_w = pchip([(MOD_WS, 69), (-50, 75), (0, 77), (100, 77), (MOD_RW, 70)])


def mod_body_sec(y):
    w = plan_round(y, 87.0, -215, 215, 34, 28)
    z0 = 21 + 13 * smoothstep((-215 + 28 - y) / 28) + 15 * smoothstep((y - (215 - 26)) / 26)
    for yw in MOD_AXLES:
        z0 = arch_z0(y, z0, yw, MOD_ARCH, MOD_WHEEL_Z)
    return Sec(z0, mod_z1(y), lambda z: w * (1 - 0.04 * ((z - 62) / 45) ** 2), 7, 9, 3)


def mod_cabin_sec(y):
    wb = mod_cab_w(y)
    return Sec(mod_z1(y) - 3, mod_roof(y), lambda z: wb - 0.32 * (z - 104), 1, 14, 3.5)


@item("Deco_car_modern")
def car_modern(it: Item):
    paint, trim, glass = "Lacquer_Black", "Chrome", "Glass_Tinted"
    extra = end_stations(-215, 34, +1) + end_stations(215, 28, -1)
    for yw in MOD_AXLES:
        extra += arch_stations(yw, MOD_ARCH)
    body = Loft(mod_body_sec, stations(-215, 215, 8.5, extra))
    it.add(body.mesh(), paint, shade="auto", angle=42)
    cabin = Loft(mod_cabin_sec, stations(MOD_WS, MOD_RW, 7, end_stations(MOD_WS, 8, +1, 4) + end_stations(MOD_RW, 8, -1, 4)),
                 counts=(2, 2, 5, 6, 6))
    it.add(cabin.mesh(), glass, shade="auto", angle=50)

    # Greenhouse frame: A-pillars / roof edges, solid roof, B- and C-pillars, chrome window line.
    both(it, cabin, MOD_WS + 2, MOD_RW, 2.84, 3.78, paint, nu=5, out=0.35)
    it.add(patch(cabin, -6, 110, 3.7, 6.3, nu=12, out=0.4), paint, shade="auto", angle=40)      # roof
    both(it, cabin, 17, 26, 1.8, 2.9, paint, nu=3, out=0.4)                                       # B-pillar
    both(it, cabin, 108, MOD_RW, 1.8, 3.8, paint, nu=6, out=0.4)                                  # C-pillar
    both(it, cabin, -80, 112, 2.79, 2.85, trim, nu=1, out=0.5, inn=0.0)                           # chrome window top
    both(it, body, -84, 116, 3.72, 3.84, trim, nu=1, out=0.35)                                    # chrome belt line
    # Hood: two soft creases (catch the light) and a black band joining the headlights.
    both(it, body, -206, MOD_WS - 4, 4.32, 4.42, paint, nu=1, out=0.35)
    it.add(patch(body, -214.6, -209.5, 4.2, 5.8, nu=10, out=0.3), "Plastic_Black", shade="auto", angle=40)
    # Black rocker cladding and lower bumpers.
    both(it, body, -88, 107, 1.0, 1.75, "Plastic_Black", nu=3, out=0.3)
    # Roof rails and antenna fin.
    for sx in (-1, 1):
        pts = [(sx * 49.5, y, 159.0 if 12 <= y <= 92 else 158.0) for y in (2, 12, 60, 92, 104)]
        it.add(sweep(pts, rect_profile(2.4, 2.0, 0.6), up=(0, 0, 1)), "Aluminium", shade="auto")
        for y in (4, 102):
            it.add(cube(3.0, 5.0, 4.0), "Plastic_Black", loc=(sx * 49.5, y, 156.5), bevel=0.8, seg=2)
    it.add(lathe([(0, 0), (3.0, 0), (2.6, 2.5), (1.2, 5.0), (0, 5.4)], 16), paint, loc=(0, 104, 154.4), shade="smooth",
           post=[squash(0, 0.35), squash(1, 2.2)])

    # Lights: wrap-around headlights (chrome reflector under clear glass) and tail lights.
    for u0, u1 in ((3.2, 4.25), mirror_u(3.2, 4.25)):
        it.add(patch(body, -214.2, -197, u0, u1, nu=6, out=0.25, inn=0.2), "Chrome", shade="auto", angle=40)
        it.add(patch(body, -214.4, -196, u0 - 0.04, u1 + 0.04, nu=6, out=0.75, inn=-0.4), "Glass", shade="smooth")
        it.add(patch(body, -214.0, -205, u0 + 0.05, u0 + 0.25, nu=2, out=0.55, inn=0.0), "Lamp_Glass", shade="auto")
    for u0, u1 in ((3.05, 4.15), mirror_u(3.05, 4.15)):
        it.add(patch(body, 199, 214.6, u0, u1, nu=6, out=0.45), "LED_Red", shade="auto", angle=40)
    it.add(cube(76, 0.8, 2.2), "LED_Red", loc=(0, 215.1, 89.5), bevel=0.3, seg=1)
    # Front: lower air intake with slats, number-plate blank, fog-light pockets.
    it.add(cube(118, 1.6, 17), "Plastic_Black", loc=(0, -215.0, 46.5), bevel=1.2, seg=2)
    for z in (42.0, 47.0, 52.0):
        it.add(cube(112, 1.0, 0.8), "Plastic_Grey", loc=(0, -215.6, z), bevel=0.2, seg=1)
    it.add(cube(52, 0.6, 11.5), "Plastic_White", loc=(0, -215.7, 46.5), bevel=0.3, seg=1, fit="y")
    it.add(cube(53.5, 0.4, 13), "Plastic_Black", loc=(0, -215.6, 46.5), bevel=0.2, seg=1)
    # Rear: diffuser, plate, exhaust tips.
    it.add(cube(120, 1.6, 9), "Plastic_Black", loc=(0, 215.0, 41.5), bevel=0.8, seg=2)
    it.add(cube(52, 0.6, 11.5), "Plastic_White", loc=(0, 215.6, 62), bevel=0.3, seg=1, fit="y")
    it.add(cube(53.5, 0.4, 13), "Plastic_Black", loc=(0, 215.5, 62), bevel=0.2, seg=1)
    for sx in (-1, 1):
        it.add(lathe([(0, 0), (4.2, 0), (4.2, 8), (3.6, 8), (3.6, 1), (0, 1)], 24), "Chrome", loc=(sx * 46, 207.5, 37), rot=(-90, 0, 0),
               shade="auto")

    # Door handles (flush, chrome) and mirrors.
    for sx in (-1, 1):
        for y in (-24, 78):
            x = body.x_at(y, 93.5, sx)
            it.add(cube(1.6, 15, 2.4), trim, loc=(x + sx * 0.3, y, 93.5), bevel=0.6, seg=2)
        my = -72.0
        it.add(sweep([(sx * 75, my, 102), (sx * 80, my, 105.5), (sx * 82, my + 1, 107)], rect_profile(3.0, 2.0, 0.6), up=(0, 1, 0)),
               paint, shade="auto")
        it.add(cube(12.5, 8.5, 9.5, cuts=2), paint, loc=(sx * 84.0, my + 1, 109), bevel=2.6, seg=3, post=puff(0.6, 1))
        it.add(cube(10.5, 0.4, 7.4), "Chrome", loc=(sx * 84.2, my + 5.4, 109), bevel=0.4, seg=1)
        it.add(cube(5.0, 0.6, 1.0), "Lamp_Glass", loc=(sx * 87.0, my - 2.5, 105.2), bevel=0.2, seg=1)  # side indicator

    # Wheels and dark wheel wells.
    for yw in MOD_AXLES:
        arch_liner(it, yw, MOD_ARCH - 0.8, MOD_WHEEL_Z, 21.5, 62)
        for sx in (-1, 1):
            wheel_modern(it, sx * MOD_TRACK, yw, sx)

    # Interior (seen through the tinted glass): dash, wheel, seats in tan leather.
    seat = "Leather_Brown"
    it.add(cube(140, 22, 8), "Plastic_Black", loc=(0, -60, 105.5), rot=(-8, 0, 0), bevel=2.5, seg=3)
    it.add(cube(34, 9, 4), "Plastic_Black", loc=(36, -55, 110), bevel=1.5, seg=2)
    it.add(cube(26, 1.2, 15), "Screen_Black", loc=(0, -50, 116), rot=(-18, 0, 0), bevel=0.6, seg=2)
    it.add(cube(18, 62, 7), "Plastic_Black", loc=(0, -18, 105.5), bevel=2, seg=2)                       # console
    wc = Vector((36, -42, 116))
    it.add(torus(17.5, 1.6, 32, 6), "Leather_Black", loc=tuple(wc), rot=(-55, 0, 0), shade="smooth")
    it.add(cylinder(5.5, 3.5, 24), "Plastic_Black", loc=(wc.x, wc.y - 2, wc.z - 2.5), rot=(-55, 0, 0), shade="smooth")
    for a in (90, 210, 330):
        r = math.radians(a)
        it.add(sweep([(wc.x, wc.y - 0.6, wc.z - 1.0), (wc.x + 16 * math.cos(r), wc.y + 16 * math.sin(r) * 0.57, wc.z + 16 * math.sin(r) * 0.82)],
                     rect_profile(3.0, 1.0, 0.3), up=(0, 0.82, -0.57)), "Plastic_Black", shade="auto")
    for sx in (-1, 1):
        x = sx * 37
        it.add(cube(48, 46, 7, cuts=2), seat, loc=(x, -4, 104.0), bevel=2.5, seg=2, shade="smooth", post=puff(1.2))
        it.add(cube(47, 10, 36, cuts=2), seat, loc=(x, 23, 123.5), rot=(-12, 0, 0), bevel=3.0, seg=2, shade="smooth", post=puff(1.0, 1))
        it.add(cube(26, 9, 12), seat, loc=(x, 28, 147.0), rot=(-10, 0, 0), bevel=3.0, seg=2, shade="smooth")
    it.add(cube(128, 44, 7, cuts=2), seat, loc=(0, 70, 105.5), bevel=2.5, seg=2, shade="smooth", post=puff(1.0))
    for x in (-40, 0, 40):
        it.add(cube(40, 10, 28, cuts=2), seat, loc=(x, 98, 120), rot=(-16, 0, 0), bevel=3.0, seg=2, shade="smooth", post=puff(0.8, 1))
        it.add(cube(20, 7, 10), seat, loc=(x, 103, 138.5), rot=(-14, 0, 0), bevel=2.5, seg=2, shade="smooth")
    it.add(cube(120, 34, 2), "Plastic_Black", loc=(0, 128, 107.5), rot=(10, 0, 0), bevel=0.8, seg=2)   # parcel shelf


# ---------------------------------------------------------------------------------------------
# Classic car: generic 1950s four-door saloon, red with cream roof
# ---------------------------------------------------------------------------------------------

CL_AXLES = (-140.0, 125.0)
CL_WHEEL_Z = 36.0
CL_TRACK = 76.0

CL_WS, CL_RW = -78.0, 118.0   # windscreen base, rear window base
cl_z1 = pchip([(-209, 74), (-208, 80), (-205, 84.5), (-198, 87.5), (-170, 89.5), (-110, 91.5), (CL_WS, 93.5), (0, 95),
               (CL_RW, 95), (150, 94), (185, 92.5), (200, 91), (206, 88), (209, 80)])
cl_roof = pchip([(CL_WS, 94.1), (-72, 105), (-62, 122), (-52, 136), (-40, 145), (-26, 149), (0, 150), (60, 149.5),
                 (82, 146.5), (96, 138), (106, 126), (113, 112), (CL_RW, 95.6)])
cl_cab_w = pchip([(CL_WS, 66), (-55, 74), (0, 76), (90, 76), (CL_RW, 69)])
# Fender line (one loft per side): front fender, a slim crease along the doors, rear fender with a small fin.
cl_fender_top = pchip([(-211, 70), (-210, 76), (-207, 84), (-200, 91), (-188, 95), (-160, 97.5), (-110, 97.5), (-70, 96.5),
                       (10, 91), (90, 93.5), (140, 95), (185, 97.5), (200, 100), (206, 100.5), (209, 98), (211, 92)])
CL_FX = 77.0  # fender centre line (headlights, tail lights)


def cl_body_sec(y):
    w = plan_round(y, 86.0, -209, 209, 26, 22)
    z0 = 27 + 13 * smoothstep((-209 + 20 - y) / 20) + 13 * smoothstep((y - (209 - 20)) / 20)
    for yw in CL_AXLES:
        z0 = arch_z0(y, z0, yw, 40, CL_WHEEL_Z)
    return Sec(z0, cl_z1(y), lambda z: w * (1 - 0.025 * ((z - 60) / 40) ** 2), 10, 12, 3)


def cl_cabin_sec(y):
    wb = cl_cab_w(y)
    return Sec(cl_z1(y) - 3, cl_roof(y), lambda z: wb - 0.28 * (z - 95), 1, 18, 4)


def cl_door_blend(y):
    """0 on the fenders, 1 along the doors between the wheels."""
    if y < 10:
        return smoothstep((y + 75) / 60)
    return 1 - smoothstep((y - 30) / 55)


def cl_fender_sec(side):
    def sec(y):
        dd = cl_door_blend(y)
        xo = 94 - 6.5 * dd
        xi = 60 + 17 * dd
        hw = plan_round(y, (xo - xi) / 2, -211, 211, 17, 16)
        z0 = 32 + 16 * smoothstep((-191 - y) / 20) + 18 * smoothstep((y - 191) / 20) + 31 * dd
        z0 = arch_z0(y, z0, CL_AXLES[0], 41.5, CL_WHEEL_Z)
        d = abs(y - CL_AXLES[1])
        if d < 44:  # rear fender skirt: covers the wheel down to z = 40
            z0 = max(z0, 40.0 if d < 40 else 32 + 8 * (44 - d) / 4)
        z1 = cl_fender_top(y) * (1 - dd) + 87.5 * dd
        return Sec(z0, z1, lambda z: hw, 6, 14, 2, xc=side * (xo + xi) / 2)

    return sec


@item("Deco_car_classic")
def car_classic(it: Item):
    red, cream, chrome, glass = "Lacquer_Red", "Lacquer_Cream", "Chrome", "Glass"
    extra = end_stations(-209, 26, +1, 6) + end_stations(209, 22, -1, 6)
    for yw in CL_AXLES:
        extra += arch_stations(yw, 40, n=8)
    body = Loft(cl_body_sec, stations(-209, 209, 12, extra), counts=(2, 3, 5, 5, 5))
    it.add(body.mesh(), red, shade="auto", angle=42)
    fenders = []
    f_extra = (end_stations(-211, 17, +1, 6) + end_stations(211, 16, -1, 6) + arch_stations(CL_AXLES[0], 41.5, n=10)
               + [CL_AXLES[1] + d for d in (-44, -40, 40, 44)])
    for side in (-1, 1):
        fl = Loft(cl_fender_sec(side), stations(-211, 211, 9, f_extra), counts=(2, 2, 3, 5, 4))
        it.add(fl.mesh(), red, shade="auto", angle=42)
        fenders.append(fl)
    lofts = [body] + fenders
    cabin = Loft(cl_cabin_sec, stations(CL_WS, CL_RW, 6, end_stations(CL_WS, 8, +1, 4) + end_stations(CL_RW, 8, -1, 4)), counts=(2, 2, 5, 6, 6))
    it.add(cabin.mesh(), glass, shade="auto", angle=50)

    # Cream roof with slim pillars, chrome window frames.
    it.add(patch(cabin, -42, 88, 2.86, 7.14, nu=14, out=0.45), cream, shade="auto", angle=40)
    both(it, cabin, CL_WS, -40, 2.72, 3.3, cream, nu=3, out=0.4)                                  # A-pillars
    both(it, cabin, 16, 23, 1.8, 2.95, cream, nu=3, out=0.4)                                      # B-pillars
    both(it, cabin, 86, CL_RW, 1.8, 3.7, cream, nu=6, out=0.4)                                    # C-pillars
    both(it, cabin, CL_WS + 2, 90, 2.66, 2.74, chrome, nu=1, out=0.55, inn=0.0)
    both(it, body, CL_WS, CL_RW, 3.66, 3.80, chrome, nu=1, out=0.35)                              # belt chrome
    # Rocker chrome between the wheels.
    both(it, body, -96, 82, 1.15, 1.45, chrome, nu=2, out=0.35)

    # Side spear: chrome strip along the outermost surface at z = 70, from behind the front wheel to the tail.
    for sx in (-1, 1):
        pts = []
        for y in range(-94, 204, 8):
            x = outer_x(lofts, y, 70.0, sx)
            if x is not None:
                pts.append((x + sx * 0.25, y, 70.0))
        it.add(sweep(pts, circle_profile(0.9, 8, sx=1.0, sy=0.55), up=(0, 0, 1)), chrome, shade="smooth")
        # Door gaps and handles.
        for y in (-74, 19, 79):
            pts = []
            for z in range(34, 95, 4):
                x = outer_x(lofts, y, z, sx)
                if x is not None:
                    pts.append((x + sx * 0.05, y, z))
            if len(pts) > 2:
                it.add(sweep(pts, rect_profile(0.45, 0.35), up=(1, 0, 0)), "Plastic_Black", shade="flat")
        for y in (10, 72):
            x = outer_x(lofts, y, 86.0, sx)
            it.add(cube(1.6, 11, 1.8), chrome, loc=(x + sx * 0.6, y - 4, 86.0), bevel=0.7, seg=2)
            it.add(cylinder(1.0, 1.5, 12), chrome, loc=(x + sx * 0.6, y + 2.5, 86.0), rot=AXIS_ROT["+x" if sx > 0 else "-x"], shade="smooth")
        # Fender-mounted mirror.
        it.add(cylinder(0.6, 9, 10), chrome, loc=(sx * 80, -90, 96.0), shade="smooth")
        it.add(lathe([(0, 0), (4.6, 0), (4.6, 1.2), (3.8, 2.2), (0, 2.4)], 24), chrome, loc=(sx * 80, -89, 107), rot=(-90, 0, 0),
               shade="smooth")

    # Headlight pods on the fender noses.
    for sx in (-1, 1):
        cx = sx * CL_FX
        it.add(cylinder(10.2, 12, 32), red, loc=(cx, -201.0, 78.5), rot=AXIS_ROT["-y"], shade="smooth")
        it.add(torus(9.4, 1.3, 32, 6), chrome, loc=(cx, -213.2, 78.5), rot=(90, 0, 0), shade="smooth")
        it.add(lathe([(0, 0), (8.6, 0), (8.2, 1.2), (5.0, 2.2), (0, 2.5)], 32), "Glass", loc=(cx, -212.6, 78.5), rot=(90, 0, 0), shade="smooth")
        it.add(lathe([(0, 0), (8.4, 0), (5.0, -2.5), (1.5, -3.6), (0, -3.6)], 32), chrome, loc=(cx, -212.4, 78.5), rot=(90, 0, 0), shade="smooth")
        it.add(sphere(1.6, 12, 6), "Lamp_Glass", loc=(cx, -210.5, 76.9), shade="smooth")
        # Small parking lights under the headlights.
        it.add(lathe([(0, 0), (3.0, 0), (3.0, 1.6), (2.4, 2.4), (0, 2.6)], 20), "Lamp_Glass", loc=(cx, -205.5, 58), rot=(90, 0, 0), shade="smooth")
        # Tail lights in the fin ends.
        tx = sx * CL_FX
        it.add(lathe([(0, 0), (5.5, 0), (5.5, 2.0), (4.6, 3.0), (0, 3.0)], 32), chrome, loc=(tx, 207.2, 86), rot=(-90, 0, 0), shade="smooth",
               post=squash(2, 1.6))
        it.add(lathe([(0, 0), (4.6, 0), (4.4, 1.4), (2.6, 2.6), (0, 3.0)], 32), "LED_Red", loc=(tx, 210.0, 86), rot=(-90, 0, 0), shade="smooth",
               post=squash(2, 1.6))

    # Grille: dark oval opening with chrome surround and three bars (generic).
    it.add(cube(92, 1.6, 18), "Plastic_Black", loc=(0, -209.2, 59.5), bevel=6, seg=4)
    oval = [(46.5 * _sq(math.cos(math.radians(a)), 0.45), -210.1, 59.5 + 9.6 * _sq(math.sin(math.radians(a)), 0.45)) for a in range(0, 360, 6)]
    it.add(sweep(oval, circle_profile(1.1, 8), up=(0, 1, 0), closed=True), chrome, shade="smooth")
    for z in (54.5, 59.5, 64.5):
        it.add(lathe([(0, 0), (0.8, 0), (0.8, 86), (0, 86)], 10), chrome, loc=(-43, -210.4, z), rot=(0, 90, 0), shade="smooth")
    for x in range(-36, 37, 8):
        it.add(cube(0.7, 1.0, 17), chrome, loc=(x, -210.0, 59.5))
    # Hood centre strip with a small generic ornament.
    pts = [(0, y, cl_z1(y) + 0.2) for y in range(-203, -70, 6)]
    it.add(sweep(pts, circle_profile(0.9, 8, sy=0.5), up=(0, 0, 1)), chrome, shade="smooth")
    it.add(lathe([(0, 0), (1.6, 0), (1.4, 4), (0.6, 7.5), (0, 8)], 16), chrome, loc=(0, -197, cl_z1(-197) - 0.5), rot=(-70, 0, 0),
           shade="smooth", post=squash(0, 0.5))

    # Bumpers wrapping around the corners, with small guards.
    for sy in (-1, 1):
        path = [(-90, sy * 190), (-89.5, sy * 199), (-86, sy * 206.5), (-77, sy * 211.0), (-60, sy * 211.8), (0, sy * 211.8),
                (60, sy * 211.8), (77, sy * 211.0), (86, sy * 206.5), (89.5, sy * 199), (90, sy * 190)]
        it.add(sweep([(x, y, 42.0) for x, y in path], circle_profile(7, 18, sx=0.42, sy=1.0), up=(0, 0, 1)), chrome, shade="smooth")
        for gx in (-40, 40):
            it.add(lathe([(0, 0), (2.4, 0.6), (2.7, 5), (2.4, 12), (1.4, 15.5), (0, 16)], 20), chrome, loc=(gx, sy * 212.2, 33),
                   shade="smooth", post=squash(1, 0.9))
    # Rear: plate blank, trunk handle, exhaust.
    it.add(cube(42, 0.6, 12), "Plastic_White", loc=(0, 209.6, 61), bevel=0.4, seg=1, fit="y")
    it.add(cube(44, 0.4, 13.5), chrome, loc=(0, 209.3, 61), bevel=0.3, seg=1)
    it.add(cube(22, 2.4, 2.0), chrome, loc=(0, 207.8, 74.5), bevel=0.8, seg=2)
    it.add(lathe([(0, 0), (3.0, 0), (3.0, 10), (2.4, 10), (2.4, 1), (0, 1)], 20), chrome, loc=(-52, 204, 26), rot=(-90, 0, 0), shade="auto")

    # Wheels, dark wells.
    for yw, ra in zip(CL_AXLES, (40.5, 39.5)):
        arch_liner(it, yw, ra, CL_WHEEL_Z, 27.5, 64)
        for sx in (-1, 1):
            wheel_classic(it, sx * CL_TRACK, yw, sx, red)

    # Interior: bench seats in cream, painted dash, thin ivory steering wheel.
    seat = "Fabric_Cream"
    it.add(cube(126, 16, 9), red, loc=(0, -66, 98.0), bevel=3, seg=3)
    it.add(cube(110, 1.0, 1.4), chrome, loc=(0, -74.2, 101.0), bevel=0.3, seg=1)
    for gx in (26, 42):
        it.add(lathe([(0, 0), (5.0, 0), (5.0, 1.2), (0, 1.2)], 24), chrome, loc=(gx, -58.2, 100.5), rot=(-70, 0, 0), shade="auto")
    wy = -48.0
    it.add(cylinder(1.6, 22, 12), "Plastic_Black", loc=(34, wy - 13, 100), rot=(-50, 0, 0), shade="smooth")
    it.add(torus(20.5, 0.9, 36, 6), "Ivory", loc=(34, wy, 112), rot=(-42, 0, 0), shade="smooth")
    it.add(torus(13, 0.4, 28, 4), chrome, loc=(34, wy - 0.6, 112.6), rot=(-42, 0, 0), shade="smooth")
    it.add(cylinder(3.0, 2.4, 20), "Ivory", loc=(34, wy - 1, 111.5), rot=(-42, 0, 0), shade="smooth")
    for a in (200, 340):
        r = math.radians(a)
        c = math.cos(math.radians(42))
        s = math.sin(math.radians(42))
        end = (34 + 20 * math.cos(r), wy + 20 * math.sin(r) * c, 112 + 20 * math.sin(r) * s)
        it.add(sweep([(34, wy, 112), end], circle_profile(0.7, 8), up=(0, -s, c)), "Ivory", shade="smooth")
    it.add(cube(128, 44, 7, cuts=2), seat, loc=(0, -8, 96), bevel=3, seg=2, shade="smooth", post=puff(1.4))
    it.add(cube(128, 9, 30, cuts=2), seat, loc=(0, 18, 110), rot=(-14, 0, 0), bevel=3.5, seg=2, shade="smooth", post=puff(1.0, 1))
    it.add(cube(124, 36, 7, cuts=2), seat, loc=(0, 54, 96), bevel=3, seg=2, shade="smooth", post=puff(1.2))
    it.add(cube(120, 9, 22, cuts=2), seat, loc=(0, 77, 105), rot=(-16, 0, 0), bevel=3, seg=2, shade="smooth", post=puff(0.8, 1))
    for x in (-40, 0, 40):  # piping on the backrests
        it.add(cube(0.8, 9.4, 29), red, loc=(x, 18, 110), rot=(-14, 0, 0), bevel=0.3, seg=1)
        it.add(cube(0.8, 9.4, 21), red, loc=(x, 77, 105), rot=(-16, 0, 0), bevel=0.3, seg=1)
    it.add(cube(118, 28, 1.5), red, loc=(0, 100, 96.5), rot=(12, 0, 0), bevel=0.5, seg=1)     # parcel shelf


def _sq(c, e):
    """Superellipse helper: sign(c) * |c|^e."""
    return math.copysign(abs(c) ** e, c)


# ---------------------------------------------------------------------------------------------
# Workbench with pegboard
# ---------------------------------------------------------------------------------------------


@item("Deco_workbench")
def workbench(it: Item):
    W, D, H = 310.0, 60.0, 90.0
    oak, frame = "Wood_Oak", "Paint_Anthracite"
    top_t = 5.0
    it.add(cube(W, D, top_t), oak, loc=(0, 0, H - top_t / 2), bevel=0.5, seg=2)
    leg_z = H - top_t
    for x in (-150.0, 0.0, 150.0):
        for y in (-26.0, 26.0):
            it.add(cube(5, 5, leg_z - 2), frame, loc=(x, y, (leg_z - 2) / 2 + 2), bevel=0.4, seg=2, grain="z")
            it.add(cylinder(2.2, 2.0, 16), "Rubber_Black", loc=(x, y, 0), shade="auto")  # levelling feet
        it.add(cube(5, 47, 6), frame, loc=(x, 0, leg_z - 3), bevel=0.3, seg=2, grain="y")
        it.add(cube(5, 47, 4), frame, loc=(x, 0, 12), bevel=0.3, seg=2, grain="y")
    for y in (-26.0, 26.0):
        it.add(cube(W - 10, 4, 6), frame, loc=(0, y, leg_z - 3), bevel=0.3, seg=2)
        it.add(cube(W - 10, 4, 4), frame, loc=(0, y, 12), bevel=0.3, seg=2)
    it.add(cube(W - 12, 56, 2.2), "Wood_Light", loc=(0, 0, 15.1), bevel=0.3, seg=1)  # lower shelf

    # Drawer cabinet on the right, standing on the shelf.
    cab = "Paint_Red"
    cx, cw, cz0, cz1 = 119.0, 56.0, 16.2, 78.8
    it.add(cube(cw, 52, cz1 - cz0), cab, loc=(cx, 0, (cz0 + cz1) / 2), bevel=0.6, seg=2)
    n = 5
    dh = (cz1 - cz0 - 2.0) / n
    for i in range(n):
        zc = cz0 + 1.0 + dh * (i + 0.5)
        it.add(cube(cw - 3, 1.2, dh - 0.8), cab, loc=(cx, -26.4, zc), bevel=0.35, seg=2)
        it.add(cube(cw - 3.6, 0.4, 0.5), "Plastic_Black", loc=(cx, -26.3, zc - dh / 2 + 0.05))
        it.add(cube(30, 1.4, 1.4), "Steel", loc=(cx, -27.6, zc + dh * 0.22), bevel=0.5, seg=2)
        it.add(cube(9, 0.3, 2.4), "Paper", loc=(cx - 18, -27.1, zc + dh * 0.15), bevel=0.1, seg=1)

    # Side vice at the left end (jaws along X, opening outwards) – keeps the front of the top free.
    vy = -12.0
    it.add(cube(18, 16, 1.6), "Paint_Grey", loc=(-143, vy, H + 0.8), bevel=0.4, seg=2)
    it.add(cube(12, 9, 7), "Paint_Grey", loc=(-141, vy, H + 5), bevel=1.0, seg=2)
    it.add(cube(5, 15, 8), "Paint_Grey", loc=(-137.5, vy, H + 9.6), bevel=0.8, seg=2)      # fixed jaw
    it.add(cube(5, 15, 8), "Paint_Grey", loc=(-148.0, vy, H + 9.6), bevel=0.8, seg=2)      # moving jaw
    for x in (-140.2, -145.3):
        it.add(cube(0.6, 14, 4.6), "Steel", loc=(x, vy, H + 11.4), bevel=0.15, seg=1)
    it.add(cube(9, 6, 4), "Paint_Grey", loc=(-145, vy, H + 4.5), bevel=0.6, seg=2)
    it.add(cylinder(1.3, 8, 16), "Steel", loc=(-158 + 3, vy, H + 8), rot=AXIS_ROT["+x"], shade="smooth")
    it.add(cylinder(0.7, 18, 12), "Steel", loc=(-153.6, vy - 9, H + 8), rot=AXIS_ROT["+y"], shade="smooth")
    for y in (vy - 9.5, vy + 9.5):
        it.add(sphere(1.1, 12, 6), "Steel", loc=(-153.6, y, H + 6.9), shade="smooth")

    # Pegboard with frame, holes and hanging tools.
    py = 29.2  # board centre (back at y = 30)
    it.add(cube(300, 1.2, 100), "Paint_White", loc=(0, py, H + 50), bevel=0.2, seg=1)
    for x in (-151.5, 151.5):
        it.add(cube(3, 2.4, 100), oak, loc=(x, 28.6, H + 50), bevel=0.3, seg=1, grain="z")
    it.add(cube(306, 2.4, 3), oak, loc=(0, 28.6, H + 98.5), bevel=0.3, seg=1)
    holes = []
    for i in range(50):
        x = -147 + 6 * i
        for j in range(16):
            z = H + 5 + 6 * j
            holes.append((x - 0.35, x + 0.35, py - 0.62, z - 0.35, z + 0.35))
    it.add(quads(holes), "Plastic_Black", shade="flat")
    _pegboard_tools(it, py - 0.6)

    # Back strip of the top: parts organiser, oil can, jar of screws (front 40 cm stays free).
    it.add(cube(30, 15, 22), "Plastic_Grey", loc=(128, 20, H + 11), bevel=0.6, seg=2)
    for r in range(4):
        for c in range(3):
            it.add(cube(8.6, 0.6, 4.4), "Glass_Frosted", loc=(118.5 + c * 9.5, 12.3, H + 3.2 + r * 5.2), bevel=0.2, seg=1)
            it.add(cube(3.0, 0.8, 0.8), "Plastic_Black", loc=(118.5 + c * 9.5, 11.8, H + 4.4 + r * 5.2), bevel=0.2, seg=1)
    it.add(lathe([(0, 0), (5, 0), (5, 9), (4.6, 10), (1.6, 12.5), (1.0, 13)], 24), "Steel", loc=(100, 20, H), shade="auto")
    it.add(sweep([(100, 21, H + 12.5), (100, 17, H + 16), (100, 10, H + 18)], circle_profile(0.35, 6), up=(1, 0, 0)), "Steel", shade="smooth")
    it.add(lathe([(0, 0), (4.0, 0), (4.0, 10.5), (3.6, 11), (0, 11)], 24), "Glass", loc=(84, 21, H), shade="smooth")
    it.add(cylinder(3.7, 5.0, 20), "Steel_Satin", loc=(84, 21, H + 0.3), shade="smooth")
    it.add(cylinder(4.3, 1.6, 24), "Plastic_Red", loc=(84, 21, H + 11), shade="auto")

    # Lower shelf: tool box, paint tins, cardboard box, extension cable reel.
    it.add(cube(48, 22, 20), "Paint_Green", loc=(-112, -4, 16.2 + 10), bevel=0.8, seg=2)
    it.add(cube(48.4, 22.4, 1.2), "Paint_Green", loc=(-112, -4, 16.2 + 17), bevel=0.4, seg=1)
    it.add(sweep([(-124, -4, 36.2), (-124, -4, 39.5), (-100, -4, 39.5), (-100, -4, 36.2)], circle_profile(0.6, 8), up=(0, 1, 0)), "Steel",
           shade="smooth")
    for x in (-130, -94):
        it.add(cube(3, 1.0, 2.2), "Steel", loc=(x, -15.4, 16.2 + 17), bevel=0.3, seg=1)
    for k, x in enumerate((-66, -50)):
        it.add(lathe([(0, 0), (8.5, 0), (8.5, 18), (8.0, 18.6), (0, 18.6)], 32), "Steel", loc=(x, 6 - 6 * k, 16.2), shade="auto")
        it.add(cylinder(8.56, 10, 32), "Paper", loc=(x, 6 - 6 * k, 19.2), shade="smooth")
    it.add(cube(46, 38, 30), "Cardboard", loc=(10, 2, 16.2 + 15), bevel=0.3, seg=2, post=wobble(0.2, 30, 3))
    it.add(cylinder(12, 4, 32), "Plastic_Red", loc=(55, -8, 16.2), shade="auto")
    it.add(cylinder(9, 16, 32), "Plastic_Yellow", loc=(55, -8, 20.2), shade="smooth")
    it.add(cylinder(12, 3, 32), "Plastic_Red", loc=(55, -8, 36.2), shade="auto")


def _pegboard_tools(it, y):
    """Tools on the pegboard; y = front face of the board. Each tool hangs on small steel pegs."""
    H = 90.0
    fy = y - 1.6  # tool centre plane

    def peg(x, z, length=4.0):
        it.add(cylinder(0.3, length, 8), "Steel", loc=(x, y, z), rot=AXIS_ROT["-y"], shade="smooth")

    # Hand saw (blade horizontal).
    blade = [(-148, H + 66), (-108, H + 70.5), (-108, H + 82), (-146, H + 76)]
    it.add(prism(blade, "y", fy - 0.1, fy + 0.1), "Steel", shade="flat")
    handle = [(-110, H + 68.5), (-94, H + 66), (-92, H + 74), (-95, H + 86), (-110, H + 84)]
    it.add(prism(handle, "y", fy - 1.2, fy + 1.2), "Wood_Light", shade="flat")
    it.add(prism([(-105, H + 72), (-97, H + 71.5), (-98, H + 80), (-105, H + 80)], "y", fy - 1.3, fy + 1.3), "Plastic_Black", shade="flat")
    peg(-128, H + 79)
    peg(-112, H + 79)
    # Hammer.
    it.add(lathe([(0, 0), (1.5, 0), (1.3, 26), (1.2, 30), (0, 30)], 12), "Wood_Light", loc=(-80, fy, H + 40), shade="auto", grain="z")
    it.add(cube(12, 2.8, 2.8), "Iron_Dark", loc=(-80.5, fy, H + 71), bevel=0.4, seg=2)
    it.add(cylinder(1.6, 1.0, 12), "Iron_Dark", loc=(-86.8, fy, H + 71), rot=AXIS_ROT["-x"], shade="auto")
    peg(-83, H + 68)
    peg(-77, H + 68)
    # Screwdrivers in a steel holder strip.
    it.add(cube(26, 3.0, 1.0), "Steel", loc=(-60, y - 1.5, H + 74), bevel=0.2, seg=1)
    for k, mat in enumerate(("Plastic_Red", "Plastic_Yellow", "Plastic_Red", "Plastic_Black", "Plastic_Yellow")):
        x = -70 + 5 * k
        ln = 14 + 2 * (k % 3)
        it.add(lathe([(0, 0), (0.35, 0), (0.35, ln), (0, ln)], 8), "Steel", loc=(x, fy, H + 74.5 - ln), shade="smooth")
        it.add(lathe([(0, 0), (1.0, 0), (1.3, 1.5), (1.3, 9), (1.0, 10), (0, 10.2)], 12), mat, loc=(x, fy, H + 75), shade="smooth",
               grain="z")
    # Open-end wrenches, growing in size.
    for k in range(6):
        x = -40 + 6.5 * k
        ln = 13 + 2.6 * k
        r = 1.5 + 0.25 * k
        zt = H + 88
        it.add(sweep([(x, fy, zt - r), (x, fy, zt - ln + r)], rect_profile(1.6 + 0.15 * k, 0.5), up=(0, 1, 0)), "Chrome", shade="auto")
        for z in (zt - r, zt - ln + r):
            it.add(cylinder(r, 0.6, 20), "Chrome", loc=(x, fy + 0.3, z), rot=AXIS_ROT["-y"], shade="auto")
        it.add(cube(r * 0.9, 0.7, r * 1.1), "Plastic_Black", loc=(x, fy, zt - r * 0.4))
        peg(x, zt - r)
    # Pliers (two), V-shaped.
    for k, x in enumerate((2, 12)):
        for d in (-1, 1):
            it.add(sweep([(x + d * 0.4, fy, H + 76), (x + d * 2.6, fy, H + 58)], rect_profile(1.4, 0.8, 0.3), up=(0, 1, 0)),
                   "Plastic_Red" if k == 0 else "Plastic_Yellow", shade="auto")
            it.add(sweep([(x + d * 0.4, fy, H + 76), (x + d * 0.6, fy, H + 83)], rect_profile(1.1, 0.7), up=(0, 1, 0)), "Steel",
                   shade="auto")
        it.add(cylinder(0.8, 1.6, 10), "Steel", loc=(x, fy + 0.8, H + 76), rot=AXIS_ROT["-y"], shade="auto")
        peg(x, H + 83.5)
    # Spirit level.
    it.add(cube(60, 2.2, 5.5), "Aluminium", loc=(55, fy, H + 86), bevel=0.3, seg=1)
    for x in (32, 55, 78):
        it.add(cube(5.0, 0.4, 2.0), "Glass_Green", loc=(x, fy - 1.15, H + 86), bevel=0.2, seg=1)
    peg(40, H + 82.5)
    peg(70, H + 82.5)
    # F-clamps.
    for k, x in enumerate((32, 44)):
        it.add(cube(1.4, 0.7, 34), "Steel", loc=(x, fy, H + 58), bevel=0.15, seg=1)
        it.add(cube(9, 1.8, 2.2), "Iron_Dark", loc=(x + 4, fy, H + 74), bevel=0.4, seg=2)
        it.add(cube(9, 1.8, 2.2), "Iron_Dark", loc=(x + 4, fy, H + 52 - 4 * k), bevel=0.4, seg=2)
        it.add(lathe([(0, 0), (1.3, 0), (1.5, 1), (1.5, 9), (1.2, 10), (0, 10)], 12), "Wood_Light", loc=(x + 7.5, fy, H + 40 - 4 * k),
               shade="smooth", grain="z")
        peg(x + 4, H + 76)
    # Extension cable coil and tape measure.
    for k in range(4):
        it.add(torus(10.0 - 0.7 * k, 0.75, 24, 5), "Plastic_Yellow", loc=(78, fy - 0.5 - 0.3 * k, H + 52 + 0.4 * k), rot=(90, 0, 0),
               shade="smooth")
    peg(78, H + 61)
    it.add(cube(7, 4, 7), "Plastic_Yellow", loc=(105, fy - 0.6, H + 58), bevel=1.5, seg=3)
    it.add(cylinder(2, 4.2, 16), "Plastic_Black", loc=(105, fy + 1.5, H + 58), rot=AXIS_ROT["-y"], shade="auto")
    peg(105, H + 62)
    # Hacksaw at the right.
    it.add(prism([(118, H + 60), (146, H + 60), (146, H + 61.4), (118, H + 61.4)], "y", fy - 0.2, fy + 0.2), "Steel", shade="flat")
    it.add(sweep([(118, fy, H + 61), (118, fy, H + 72), (146, fy, H + 72), (146, fy, H + 61)], rect_profile(1.0, 0.8), up=(0, 1, 0)),
           "Paint_Red", shade="auto")
    it.add(cube(3, 2.2, 9), "Plastic_Black", loc=(148.5, fy, H + 63), bevel=0.8, seg=2)
    peg(132, H + 73.5)

    # Lower row: small shelf with spray cans and jars, rope coil, ear defenders, hand brush.
    sy = y - 5.6
    it.add(cube(50, 11, 1.8), "Wood_Oak", loc=(-120, sy, H + 32), bevel=0.3, seg=1)
    for x in (-141, -99):
        it.add(prism([(y, H + 31.1), (y - 9, H + 31.1), (y, H + 22)], "x", x - 0.6, x + 0.6), "Steel", shade="flat")
    for k, (x, cap) in enumerate(((-139, "Plastic_Red"), (-131.5, "Plastic_Yellow"), (-124, "Plastic_Red"))):
        it.add(lathe([(0, 0), (3.0, 0), (3.0, 14), (2.6, 15.4), (1.2, 16.4), (0, 16.4)], 20), "Steel", loc=(x, sy - 1, H + 32.9),
               shade="auto")
        it.add(cylinder(3.05, 7, 20), cap, loc=(x, sy - 1, H + 36), shade="smooth")
        it.add(cylinder(1.4, 2.4, 12), cap, loc=(x, sy - 1, H + 49.2), shade="auto")
    for x in (-114, -105):
        it.add(lathe([(0, 0), (3.6, 0), (3.6, 9.5), (3.0, 10), (0, 10)], 20), "Glass", loc=(x, sy - 1, H + 32.9), shade="smooth")
        it.add(cylinder(3.4, 4.5, 16), "Steel_Satin", loc=(x, sy - 1, H + 33.2), shade="smooth")
        it.add(cylinder(3.8, 1.6, 20), "Steel", loc=(x, sy - 1, H + 42.9), shade="auto")
    for k in range(3):
        it.add(torus(8.5 - 0.6 * k, 0.8, 24, 5), "Wool_Brown", loc=(-25 + 0.4 * k, fy - 0.4 - 0.4 * k, H + 44 + 0.5 * k),
               rot=(90, 0, 0), shade="smooth")
    peg(-25, H + 52)
    it.add(sweep([(0, fy, H + 38), (2, fy, H + 45), (8, fy, H + 47.5), (14, fy, H + 45), (16, fy, H + 38)], rect_profile(1.6, 1.0, 0.3),
                 up=(0, 1, 0)), "Plastic_Black", shade="auto")
    for x in (0, 16):
        it.add(sphere(4.0, 16, 8), "Plastic_Red", loc=(x, fy - 1.2, H + 33.5), shade="smooth", post=squash(1, 0.55))
    peg(8, H + 48.5)
    it.add(lathe([(0, 0), (1.2, 0), (1.4, 2), (1.4, 14), (1.0, 16), (0, 16)], 12), "Wood_Light", loc=(130, fy, H + 40), shade="smooth",
           grain="z")
    it.add(cube(20, 3.4, 3.0), "Wood_Light", loc=(130, fy, H + 38.5), bevel=0.6, seg=2)
    it.add(cube(19, 3.0, 4.5, cuts=2), "Plastic_Black", loc=(130, fy, H + 34.8), bevel=0.4, seg=1, post=wobble(0.12, 3, 1))
    peg(130, H + 55)


# ---------------------------------------------------------------------------------------------
# Lawn mower
# ---------------------------------------------------------------------------------------------


def mower_wheel(it, x, y, r, side):
    rot = (0, 90 * side, 0)
    loc = (x, y, r)
    tyre = [(r - 3.6, -2.0), (r - 1.0, -2.25), (r - 0.2, -1.8), (r, 0.0), (r - 0.2, 1.8), (r - 1.0, 2.25), (r - 3.6, 2.0)]
    it.add(revolve(tyre, 32, ridges=(16, 0.35, r - 0.3)), "Rubber_Black", loc=loc, rot=rot, shade="auto", angle=45)
    it.add(lathe([(0, 0), (r - 3.4, 0), (r - 3.4, 3.6), (r - 4.4, 4.2), (2.4, 4.2), (2.0, 4.8), (0, 4.8)], 28), "Plastic_Grey",
           loc=_wl(loc, side, -2.2), rot=rot, shade="auto", angle=40)
    for k in range(6):
        a = math.radians(60 * k)
        it.add(cube(0.9, 0.5, r - 6.5), "Plastic_Grey", loc=(x + side * 1.9, y + (r - 3.5) / 2 * math.cos(a), r + (r - 3.5) / 2 * math.sin(a)),
               rot=(math.degrees(a) - 90, 0, 0), bevel=0.15, seg=1)
    it.add(cylinder(1.4, 0.4, 12), "Plastic_Black", loc=_wl(loc, side, 2.4), rot=rot, shade="auto")


@item("Deco_lawn_mower")
def lawn_mower(it: Item):
    deck = "Paint_Red"
    # Deck: steel shell with a domed top, front bumper lip.
    it.add(cube(44, 50, 10, cuts=2), deck, loc=(0, -20, 13), bevel=3.5, seg=4)
    it.add(cube(39, 44, 5, cuts=6), deck, loc=(0, -21, 19.5), bevel=2.0, seg=3, shade="smooth", post=puff(1.6))
    it.add(cube(44.5, 50.5, 1.2), "Plastic_Black", loc=(0, -20, 8.6), bevel=0.5, seg=1)
    it.add(sweep([(-20, -44.6, 13), (-11, -45.4, 13), (11, -45.4, 13), (20, -44.6, 13)], rect_profile(1.4, 3.0, 0.5), up=(0, 0, 1)),
           "Plastic_Black", shade="auto")
    for sx in (-1, 1):
        it.add(cube(1.2, 44, 1.6), deck, loc=(sx * 20.8, -20, 18.6), bevel=0.4, seg=1)  # stiffening ribs

    # Engine on the deck.
    ex, ey = 0.0, -20.0
    it.add(cylinder(13.5, 1.5, 32), "Iron_Dark", loc=(ex, ey, 21.5), shade="auto")
    it.add(lathe([(0, 0), (12, 0), (12.5, 1.5), (12.3, 9.5), (11, 12.5), (7, 14.2), (0, 14.6)], 36), "Plastic_Black", loc=(ex, ey, 23),
           shade="auto", angle=40)
    for k in range(10):  # cooling fins showing below the shroud
        a = math.radians(36 * k)
        it.add(cube(0.6, 3.2, 3.0), "Iron_Dark", loc=(ex + 12.6 * math.cos(a), ey + 12.6 * math.sin(a), 24.8), rot=(0, 0, math.degrees(a)))
    it.add(cube(10, 7, 6.5), "Plastic_Grey", loc=(ex - 6.5, ey - 9.5, 32), bevel=1.6, seg=3)          # air filter
    it.add(cylinder(2.4, 2.4, 20), "Plastic_Yellow", loc=(ex + 5, ey - 4, 37.0), shade="auto")         # fuel cap
    it.add(lathe([(0, 0), (2.4, 0), (2.4, 1.2), (0, 1.2)], 16, ridges=(12, 0.25, 0, 1)), "Plastic_Yellow", loc=(ex + 5, ey - 4, 39.4),
           shade="auto")
    it.add(cube(9, 5, 5.5), "Iron_Dark", loc=(ex + 10, ey - 11, 26), bevel=1.2, seg=2)                 # muffler
    it.add(cube(10, 1.0, 6.5), "Steel", loc=(ex + 10, ey - 14, 26.5), bevel=0.4, seg=1)
    it.add(cube(2.4, 6, 2.4), "Plastic_Grey", loc=(ex - 8, ey + 8, 34), bevel=0.6, seg=2)               # spark plug boot
    it.add(cube(6.5, 2.4, 2.2), "Plastic_Red", loc=(ex, ey + 10.5, 36.5), bevel=0.8, seg=2)             # pull-start grip
    it.add(sweep([(ex, ey + 10.5, 36.5), (ex - 1, ey + 12, 32)], circle_profile(0.25, 6), up=(1, 0, 0)), "Plastic_Black", shade="smooth")

    # Wheels with height levers.
    for sx in (-1, 1):
        mower_wheel(it, sx * 24.4, -36.0, 9.0, sx)
        mower_wheel(it, sx * 24.4, 0.0, 10.0, sx)
        for y, r in ((-36.0, 9.0), (0.0, 10.0)):
            it.add(sweep([(sx * 22.4, y, r), (sx * 22.6, y - 3, r + 7), (sx * 22.6, y - 5, r + 9)], rect_profile(0.9, 1.4, 0.3),
                         up=(1, 0, 0)), "Steel", shade="auto")
            it.add(cube(1.6, 2.4, 1.4), "Plastic_Grey", loc=(sx * 22.6, y - 5.4, r + 9.4), bevel=0.4, seg=2)

    # Rear chute and grass catcher.
    it.add(cube(24, 6, 12), "Plastic_Black", loc=(0, 5, 15), bevel=1.2, seg=2)
    it.add(cube(37, 3, 27), "Plastic_Black", loc=(0, 8, 21.5), bevel=1.0, seg=2)
    it.add(cube(35, 30, 24, cuts=4), "Fabric_Grey", loc=(0, 24, 21), bevel=3, seg=3, shade="smooth",
           post=[puff(1.6, 0), wobble(0.35, 12, 2)])
    it.add(cube(37, 32, 3), "Plastic_Grey", loc=(0, 23.5, 34.6), bevel=1.2, seg=2)
    for x in (-10, -5, 0, 5, 10):  # vent slots in the catcher lid
        it.add(cube(2.2, 18, 0.4), "Plastic_Black", loc=(x, 26, 36.05), bevel=0.15, seg=1)
    it.add(sweep([(-9, 14, 35.6), (-8, 14, 39.4), (8, 14, 39.4), (9, 14, 35.6)], rect_profile(1.8, 1.4, 0.4), up=(0, 1, 0)),
           "Plastic_Black", shade="auto")
    for sx in (-1, 1):
        it.add(sweep([(sx * 17.6, 8.5, 33), (sx * 17.6, 38.5, 33), (sx * 17.6, 38.5, 10)], circle_profile(0.35, 6), up=(1, 0, 0)),
               "Plastic_Black", shade="smooth")

    # Handle: one bent tube, quick-release knobs, bail lever, foam grip.
    r = 1.1
    pts = [(-22, 2, 18), (-22, 18, 46), (-22, 36, 82), (-21.6, 41.0, 92.4), (-20.0, 43.2, 97.0), (-16.5, 43.9, 98.9),
           (16.5, 43.9, 98.9), (20.0, 43.2, 97.0), (21.6, 41.0, 92.4), (22, 36, 82), (22, 18, 46), (22, 2, 18)]
    it.add(sweep(pts, circle_profile(r, 12), up=(0, -0.9, 0.45)), "Steel_Satin", shade="smooth")
    it.add(cylinder(1.65, 30, 16), "Rubber_Black", loc=(-15, 43.9, 98.9), rot=AXIS_ROT["+x"], shade="smooth")
    for sx in (-1, 1):
        it.add(cube(3.6, 5.0, 3.6), "Plastic_Black", loc=(sx * 22, 2.5, 18.5), bevel=0.8, seg=2)  # bracket
        it.add(cylinder(2.4, 1.8, 16), "Plastic_Black", loc=(sx * 23.0, 20, 50), rot=AXIS_ROT["+x" if sx > 0 else "-x"], shade="auto")
        it.add(cube(1.4, 6.5, 1.6), "Plastic_Black", loc=(sx * 24.4, 21.5, 48), rot=(-30, 0, 0), bevel=0.4, seg=2)
    it.add(sweep([(-22.5, 38.8, 86.5), (-21.2, 35.6, 93.5), (-18.0, 35.0, 95.0), (18.0, 35.0, 95.0), (21.2, 35.6, 93.5), (22.5, 38.8, 86.5)],
                 circle_profile(0.55, 8), up=(0, -0.9, 0.45)), "Steel", shade="smooth")
    it.add(sweep([(-22, 18.4, 46.6), (22, 18.4, 46.6)], circle_profile(0.8, 10), up=(0, 0, 1)), "Steel_Satin", shade="smooth")  # cross bar
    # Throttle cable down the right tube.
    it.add(sweep([(21.0, 37.5, 88), (19.8, 30, 72), (19.8, 18, 48), (16, 8, 34), (10, -10, 33)], circle_profile(0.3, 6), up=(1, 0, 0)),
           "Plastic_Black", shade="smooth")


# ---------------------------------------------------------------------------------------------
# Garden tool rack (wall part, back at y = +7.5)
# ---------------------------------------------------------------------------------------------


@item("Deco_tool_rack")
def tool_rack(it: Item):
    back = 7.5
    rail_z = 150.0
    hy = -0.5  # handle centre plane
    it.add(cube(100, 3.0, 7.0), "Aluminium", loc=(0, back - 1.5, rail_z), bevel=0.4, seg=2)
    it.add(cube(100, 0.4, 3.0), "Plastic_Grey", loc=(0, back - 3.1, rail_z), bevel=0.1, seg=1)
    for x in (-49.4, 49.4):
        it.add(cube(1.6, 3.4, 7.4), "Plastic_Grey", loc=(x, back - 1.6, rail_z), bevel=0.5, seg=2)
    for x in (-30, 0, 30):
        it.add(cylinder(0.7, 0.5, 12), "Steel", loc=(x, back - 3.0, rail_z), rot=AXIS_ROT["-y"], shade="auto")

    def clip(x):
        it.add(cube(6, 3.4, 4.6), "Plastic_Black", loc=(x, back - 4.6, rail_z), bevel=0.6, seg=2)
        for d in (-1, 1):
            it.add(cube(1.2, 3.6, 4.0), "Plastic_Black", loc=(x + d * 2.4, hy + 0.3, rail_z), bevel=0.4, seg=2)

    # Spade, D-grip on top, blade down.
    sx = -40.0
    it.add(lathe([(0, 0), (1.7, 0), (1.8, 60), (1.7, 80), (0, 80)], 14), "Wood_Light", loc=(sx, hy, 76), shade="auto", grain="z")
    it.add(lathe([(0, 0), (2.4, 0), (1.9, 13), (0, 13)], 16), "Steel", loc=(sx, hy, 64), shade="auto")  # socket
    d_grip = [(sx, hy, 155), (sx - 6.0, hy, 160.5), (sx - 5.8, hy, 166.2), (sx + 5.8, hy, 166.2), (sx + 6.0, hy, 160.5), (sx, hy, 155)]
    it.add(sweep(d_grip, circle_profile(1.0, 10), up=(0, 1, 0)), "Wood_Light", shade="smooth")
    it.add(cylinder(1.5, 11, 14), "Plastic_Black", loc=(sx - 5.5, hy, 166.2), rot=AXIS_ROT["+x"], shade="smooth")
    blade = [(sx - 9, 66), (sx + 9, 66), (sx + 9, 46), (sx + 7, 41.5), (sx - 7, 41.5), (sx - 9, 46)]
    it.add(prism(blade, "y", hy - 0.25, hy + 0.25), "Steel", shade="flat")
    it.add(cube(18.4, 2.2, 1.2), "Steel", loc=(sx, hy - 0.8, 66.2), bevel=0.3, seg=1)  # tread
    clip(sx)

    # Rake, head on top, tines towards the room (-Y).
    rx = -16.0
    it.add(lathe([(0, 0), (1.4, 0), (1.5, 1.0), (1.5, 160), (0, 160)], 12), "Wood_Light", loc=(rx, hy, 0.4), shade="auto", grain="z")
    it.add(lathe([(0, 0), (1.9, 0), (1.6, 7), (0, 7)], 12), "Iron_Dark", loc=(rx, hy, 160), shade="auto")
    it.add(cube(30, 2.4, 2.4), "Iron_Dark", loc=(rx, hy, 167.6), bevel=0.5, seg=2)
    for k in range(14):
        x = rx - 14.0 + 28.0 * k / 13
        it.add(sweep([(x, hy - 0.8, 167.6), (x, hy - 4.4, 168.0), (x, hy - 6.6, 169.5)], circle_profile(0.42, 6), up=(1, 0, 0)), "Iron_Dark",
               shade="smooth")
    clip(rx)

    # Broom, head on top, bristles towards the room.
    bx = 13.0
    it.add(lathe([(0, 0), (1.4, 0), (1.5, 1.0), (1.5, 146), (0, 146)], 12), "Wood_Light", loc=(bx, hy, 14), shade="auto", grain="z")
    it.add(cube(26, 4.8, 4.4), "Wood_Light", loc=(bx, hy + 0.6, 164.0), bevel=0.8, seg=2)
    it.add(cube(25, 4.8, 3.6, cuts=3), "Plastic_Black", loc=(bx, hy - 4.2, 164.0), bevel=0.6, seg=2, post=wobble(0.15, 3, 4))
    for k in range(9):
        it.add(cube(1.0, 0.3, 3.0), "Fabric_Black", loc=(bx - 11 + 2.75 * k, hy - 6.7, 164.0))
    clip(bx)

    # Wall hose reel with green hose and hanging nozzle.
    cx, cz = 33.0, 70.0
    it.add(cube(14, 1.6, 36), "Plastic_Grey", loc=(cx, back - 0.8, cz - 2), bevel=0.6, seg=2)
    it.add(cube(5, 3.2, 5), "Plastic_Grey", loc=(cx, back - 3.0, cz), bevel=0.8, seg=2)
    it.add(cylinder(7.0, 9.0, 32), "Plastic_Grey", loc=(cx, 4.2, cz), rot=AXIS_ROT["-y"], shade="smooth")
    for y in (4.6, -5.0):
        it.add(lathe([(0, 0), (16.8, 0), (16.8, 1.0), (15.6, 1.2), (0, 1.2)], 40), "Plastic_Grey", loc=(cx, y, cz), rot=AXIS_ROT["-y"],
               shade="auto")
    for k in range(5):  # spokes on the front flange
        a = math.radians(90 + 72 * k)
        it.add(cube(1.6, 0.6, 9.0), "Plastic_Grey", loc=(cx + 11.5 * math.cos(a), -6.4, cz + 11.5 * math.sin(a)),
               rot=(0, 90 - math.degrees(a), 0), bevel=0.2, seg=1)
    for layer, rr in enumerate((8.1, 10.1, 12.1, 14.1)):
        for k in range(4):
            yy = 3.0 - 2.25 * k + (0.6 if layer % 2 else 0.0)
            it.add(torus(rr, 1.0, 36, 6), "Paint_Green", loc=(cx, yy, cz), rot=(90, 0, 0), shade="smooth")
    it.add(sweep([(cx, -6.5, cz), (cx + 11.0, -6.5, cz + 7)], rect_profile(1.8, 0.8), up=(0, 1, 0)), "Plastic_Grey", shade="auto")
    it.add(cylinder(1.0, 1.0, 12), "Plastic_Black", loc=(cx + 11.0, -6.5, cz + 7), rot=AXIS_ROT["-y"], shade="smooth")
    hose = [(cx + 14.5, -1.0, cz - 2), (cx + 15.5, -2.0, cz - 14), (cx + 14.0, -2.6, cz - 26), (cx + 12.0, -2.6, cz - 35)]
    it.add(sweep(hose, circle_profile(1.0, 8), up=(1, 0, 0)), "Paint_Green", shade="smooth")
    it.add(lathe([(0, 0), (1.3, 0), (1.5, 3), (1.1, 7), (0.6, 8), (0, 8)], 14), "Plastic_Yellow", loc=(cx + 12.0, -2.6, cz - 35),
           rot=(180, 0, 0), shade="smooth")
