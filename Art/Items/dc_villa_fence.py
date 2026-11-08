"""Villa fence and hedges (group G13): bar fence panel, fence post, garden gate, thuja and privet hedge.
Exported as SM_<id>.

Fence_panel, Hedge_thuja and Hedge_privet are modules that the level lines up along X with a pitch of 200 cm:
they span x = -100 .. +100, end flush there and nothing sticks out. The hedges are an extruded cross-section
whose leafy displacement is periodic in x (period 200 cm), so two modules meet without a seam or a waist; the
flat end caps are hidden inside the neighbour (and give a clean cut end at the end of a run).

Gate_garden follows the door leaf rule: origin = hinge edge at the bottom, leaf y = 0 .. -118, thickness
centred on x = 0, z = 0 .. 180, street / show side = -X. The level stretches it to 140 cm, so it is modelled at
140 cm with round hardware and squashed along Y by 118/140 (after stretching it matches the fence: same rails,
same 2 cm bars, same gaps).
"""

import math
import random

import bmesh
from mathutils import Euler, Vector, noise

from dc_items import item
from dc_mesh import CM, Item, circle_profile, cube, cylinder, lathe, sphere, sweep, tapered_box

PAINT = "Paint_Anthracite"

# Shared fence dimensions (cm). The gate uses the same rail heights and bar size.
RAIL_D, RAIL_H, RAIL_R = 5.0, 4.0, 0.6     # rail tube: depth across the fence, height, edge radius
RAIL_LOW_Z, RAIL_TOP_Z = 8.0, 176.0         # bottom edges of the lower and the upper rail
BAR = 2.0                                   # square bar
BAR_PITCH = 12.5                            # 16 bars per 200 cm module, 10.5 cm clear gap
BAR_FOOT_Z = 2.0                            # lower end of the bars (they reach 6 cm below the lower rail)


# ---------------------------------------------------------------------------------------------
# Small helpers
# ---------------------------------------------------------------------------------------------


def rrect(w, h, r, n=3):
    """Rounded rectangle profile centred on (0, 0), corner radius r, n segments per corner (cm)."""
    hw, hh = w / 2, h / 2
    pts = []
    for (cx, cy), a0 in (((hw - r, -hh + r), 270), ((hw - r, hh - r), 0), ((-hw + r, hh - r), 90), ((-hw + r, -hh + r), 180)):
        for i in range(n + 1):
            a = math.radians(a0 + 90.0 * i / n)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def tri_ngons(bm):
    """Triangulate faces with more than 4 corners (sweep caps), so the FBX export can write tangents."""
    big = [f for f in bm.faces if len(f.verts) > 4]
    if big:
        bmesh.ops.triangulate(bm, faces=big, quad_method="BEAUTY", ngon_method="BEAUTY")
    return bm


def rail_x(it, z_bottom, x0=-100.0, x1=100.0, mat=PAINT):
    """Horizontal rail tube along X with flat cut ends (a sweep, so the ends stay square for the module joint)."""
    zc = z_bottom + RAIL_H / 2
    # sweep along +X with up=Z: profile x -> -Y, profile y -> +Z
    it.add(tri_ngons(sweep([(x0, 0, zc), (x1, 0, zc)], rrect(RAIL_D, RAIL_H, RAIL_R))), mat, shade="auto", angle=40)


def squash_y(k):
    """Post deformer: scale local Y by k (the part is built around its own origin)."""

    def apply(me):
        for v in me.vertices:
            v.co.y *= k

    return apply


def rotated(bm, rot_deg):
    """Rotate a bmesh (degrees XYZ) in place and return it."""
    m = Euler([math.radians(a) for a in rot_deg]).to_matrix()
    for v in bm.verts:
        v.co = m @ v.co
    return bm


# ---------------------------------------------------------------------------------------------
# Fence
# ---------------------------------------------------------------------------------------------


@item("Fence_panel")
def fence_panel(it: Item):
    """Anthracite bar fence module 200 x 5 x 180: lower and upper rail, 16 square bars at 12.5 cm pitch.

    The first and last bar sit 6.25 cm from the module end, so a fence post (8 x 8) on the joint fits between them
    and the bar rhythm continues across the joint.
    """
    rail_x(it, RAIL_LOW_Z)
    rail_x(it, RAIL_TOP_Z)
    z0, z1 = BAR_FOOT_Z, RAIL_TOP_Z + 0.5
    n = int(round(200.0 / BAR_PITCH))
    for i in range(n):
        x = -100.0 + BAR_PITCH * (i + 0.5)
        it.add(cube(BAR, BAR, z1 - z0), PAINT, loc=(x, 0, (z0 + z1) / 2), bevel=0.25, seg=1, grain="z")


@item("Fence_post")
def fence_post(it: Item):
    """Square post 8 x 8 x 190 with a black pyramid cap and flat rail brackets on both panel sides (+-X)."""
    tube_h = 186.0
    it.add(cube(8, 8, tube_h), PAINT, loc=(0, 0, tube_h / 2), bevel=0.7, seg=3, grain="z")
    # Cap: collar slightly wider than the tube, low pyramid on top.
    it.add(cube(8.6, 8.6, 1.6), "Plastic_Black", loc=(0, 0, tube_h + 0.6), bevel=0.35, seg=2)
    it.add(tapered_box(2.6, 2.6, 8.6, 8.6, 2.6), "Plastic_Black", loc=(0, 0, tube_h + 1.4), bevel=0.3, seg=2)
    # Rail brackets where the panel rails meet the post (same heights as Fence_panel).
    for zb in (RAIL_LOW_Z, RAIL_TOP_Z):
        zc = zb + RAIL_H / 2
        for sx in (-1, 1):
            it.add(cube(0.5, 7.0, 7.5), "Iron_Dark", loc=(sx * 4.2, 0, zc), bevel=0.15, seg=1)
            for sy in (-1, 1):
                it.add(cylinder(0.55, 0.35, 6), "Steel", loc=(sx * 4.45, sy * 2.4, zc), rot=(0, 90 * sx, 0), shade="flat")


# ---------------------------------------------------------------------------------------------
# Garden gate (door leaf rule, modelled at 140 cm and squashed to 118 cm)
# ---------------------------------------------------------------------------------------------

GATE_B, GATE_T, GATE_H = 118.0, 5.0, 180.0
GATE_STRETCH = 140.0  # width the level gives the gate
GATE_K = GATE_B / GATE_STRETCH


class _Gate:
    """Adds parts designed in the 140 cm target space; Y positions and shapes are squashed by GATE_K."""

    def __init__(self, it):
        self.it = it

    def add(self, bm, mat, loc=(0, 0, 0), rot=None, **kw):
        if rot:
            rotated(bm, rot)
        post = [squash_y(GATE_K)]
        if kw.get("post"):
            extra = kw.pop("post")
            post += extra if isinstance(extra, (list, tuple)) else [extra]
        x, y, z = loc
        return self.it.add(bm, mat, loc=(x, y * GATE_K, z), post=post, **kw)

    def box(self, mat, y0, y1, z0, z1, tx, bevel=0.3, seg=2, grain="z"):
        """Box from y0..y1, z0..z1 (target cm), thickness tx centred on x = 0."""
        return self.add(cube(tx, abs(y1 - y0), z1 - z0), mat, loc=(0, (y0 + y1) / 2, (z0 + z1) / 2), bevel=bevel, seg=seg,
                        grain=grain)


def _lever(g, side, y_spindle, z, plate_x):
    """Lever handle on one face (side -1 = street side -X): neck out of the plate, bar towards the hinge (+Y)."""
    x0 = side * plate_x
    reach = 5.6
    path = [(x0, y_spindle, z), (x0 + side * (reach - 1.2), y_spindle, z), (x0 + side * reach, y_spindle + 1.2, z),
            (x0 + side * reach, y_spindle + 11.5, z - 0.6)]
    g.add(tri_ngons(sweep(path, circle_profile(0.85, 14), up=(0, 0, 1))), "Steel", shade="smooth")
    g.add(sphere(0.95, 16, 8), "Steel", loc=(x0 + side * reach, y_spindle + 11.5, z - 1.55), shade="smooth")
    # Rose collar where the neck leaves the plate.
    g.add(cylinder(1.5, 0.6, 20), "Steel", loc=(x0, y_spindle, z), rot=(0, 90 * side, 0), shade="smooth")


@item("Gate_garden")
def gate_garden(it: Item):
    """Garden gate 118 x 5 x 180 matching Fence_panel: tube frame, 8 bars, lock case with lever on both sides,
    profile cylinder, latch faceplate on the closing edge, two weld-on hinge eyes at the hinge edge."""
    g = _Gate(it)
    B = GATE_STRETCH
    stile = 4.5
    # Frame: stiles from just above the ground to the top, rails at the fence heights.
    g.box(PAINT, 0.0, -stile, 2.0, GATE_H, GATE_T, bevel=0.5, seg=2)
    g.box(PAINT, -B + 0.3, -B + 0.3 + stile, 2.0, GATE_H, GATE_T, bevel=0.5, seg=2)
    for zb in (RAIL_LOW_Z, RAIL_TOP_Z):
        g.box(PAINT, -stile + 0.4, -B + 0.3 + stile - 0.4, zb, zb + RAIL_H, GATE_T, bevel=0.5, seg=2, grain="y")
    # Lock case between the closing stile and a lock stile.
    lock_y0, lock_y1 = -B + 0.3 + stile, -B + 0.3 + stile + 14.5
    lock_z0, lock_z1 = 91.0, 121.0
    g.box(PAINT, lock_y0 - 0.3, lock_y1, lock_z0, lock_z1, 6.0, bevel=0.5, seg=2)
    hold_w = 3.5
    g.box(PAINT, lock_y1 - 0.2, lock_y1 + hold_w, RAIL_LOW_Z + RAIL_H - 0.4, RAIL_TOP_Z + 0.4, 4.0, bevel=0.35, seg=2)
    # Bars between hinge stile and lock stile (same size and nearly the same gap as the fence).
    ya, yb = -stile, lock_y1 + hold_w
    n = 8
    gap = ((ya - yb) - n * BAR) / (n + 1)
    for i in range(n):
        y = ya - gap * (i + 1) - BAR * i - BAR / 2
        g.box(PAINT, y + BAR / 2, y - BAR / 2, BAR_FOOT_Z, RAIL_TOP_Z + 0.5, BAR, bevel=0.22, seg=1)
    # Hardware: lever spindle 8 cm (in the 118 cm leaf) from the closing edge, DIN height 105.
    y_sp = -(GATE_B - 8.0) / GATE_K
    z_sp = 105.0
    face = 3.0  # lock case face
    for side in (-1, 1):
        # Long plate with lever and profile cylinder.
        g.add(cube(0.6, 3.8, 24.0), "Steel", loc=(side * (face + 0.3), y_sp, 101.5), bevel=0.25, seg=2)
        _lever(g, side, y_sp, z_sp, face + 0.6)
        zc = z_sp - 9.2
        g.add(cylinder(1.25, 0.45, 20), "Steel", loc=(side * (face + 0.6), y_sp, zc), rot=(0, 90 * side, 0), shade="smooth")
        g.add(cylinder(0.95, 0.55, 20), "Brass", loc=(side * (face + 0.6), y_sp, zc), rot=(0, 90 * side, 0), shade="smooth")
        g.add(cube(0.2, 0.32, 1.2), "Plastic_Black", loc=(side * (face + 1.15), y_sp, zc - 0.15))
    # Latch faceplate on the closing edge, flush, with dark openings for latch and bolt.
    g.add(cube(2.6, 0.3, 24.0), "Steel", loc=(0, -B + 0.15, 104.0), bevel=0.1, seg=1)
    for zc, h in ((z_sp, 2.2), (z_sp - 9.2, 1.4)):
        g.add(cube(1.0, 0.1, h), "Plastic_Black", loc=(0, -B + 0.02, zc))
    # Weld-on hinge eyes at the hinge edge with steel pins (the counterparts sit on the post).
    for z0 in (26.0, 150.0):
        g.add(cube(1.2, 2.4, 7.0), PAINT, loc=(0, -0.2, z0 + 4.5), bevel=0.2, seg=1)  # weld lug
        g.add(cylinder(1.5, 9.0, 20), PAINT, loc=(0, 0.35, z0), shade="auto", angle=40)
        g.add(cylinder(0.65, 12.0, 14), "Steel", loc=(0, 0.35, z0 - 1.5), shade="smooth")
        g.add(lathe([(0, 0), (1.5, 0), (1.5, 0.4), (1.1, 0.9), (0, 1.1)], 20), "Steel", loc=(0, 0.35, z0 + 9.0), shade="smooth")


# ---------------------------------------------------------------------------------------------
# Hedges
# ---------------------------------------------------------------------------------------------
#
# A hedge module is a lumpy body (the cross-section extruded along X and displaced along its normal) in a darker
# green plus a few thousand small leaf scales (two flat-shaded triangles each) on its surface. All noise maps x
# onto a circle (period 200 cm), so both module ends are identical; leaves that cross an end are copied to the
# other end and clamped to x = +-100. The end caps are dented inwards only.

PERIOD = 200.0                       # module length and noise period along X (cm)
R_PERIOD = PERIOD / (2.0 * math.pi)  # x is mapped onto a circle of this radius in noise space (keeps the scale)


def _smoothstep(e0, e1, x):
    t = min(max((x - e0) / (e1 - e0), 0.0), 1.0)
    return t * t * (3 - 2 * t)


def _ring_point(x):
    th = 2.0 * math.pi * (x + PERIOD / 2) / PERIOD
    return R_PERIOD * math.cos(th), R_PERIOD * math.sin(th)


def hedge_profile(half_bottom, half_top, height, r_top, crown, belly, foot_h=0.0, foot_in=0.0, n=600):
    """Fine cross-section polyline (y, z) in cm: back foot (y > 0) - up - over the top - down - front foot (y < 0)."""
    zs = height - r_top
    back = []
    for i in range(n // 3 + 1):
        z = zs * i / (n // 3)
        t = z / zs
        y = half_bottom + (half_top - half_bottom) * t + belly * math.sin(math.pi * t)
        if foot_h > 0:
            y -= foot_in * (1.0 - _smoothstep(0.0, foot_h, z))
        back.append((y, z))
    cy, cz = half_top - r_top, zs
    arc = []
    for i in range(1, n // 6 + 1):
        a = math.radians(90.0 * i / (n // 6))
        arc.append((cy + r_top * math.cos(a), cz + r_top * math.sin(a)))
    top = []
    for i in range(1, n // 6):
        y = cy - 2 * cy * i / (n // 6)
        top.append((y, height + crown * (1.0 - (y / cy) ** 2)))
    return back + arc + top + [(-y, z) for y, z in reversed(back + arc)]


class Profile:
    """Arc-length access to the cross-section: point, outward normal and tangent at u (cm)."""

    def __init__(self, poly):
        self.pts = poly
        self.cum = [0.0]
        for a, b in zip(poly, poly[1:]):
            self.cum.append(self.cum[-1] + math.dist(a, b))
        self.total = self.cum[-1]
        self.zmax = max(z for _, z in poly)

    def pos(self, u):
        u = min(max(u, 0.0), self.total)
        lo, hi = 0, len(self.cum) - 1
        while hi - lo > 1:
            mid = (lo + hi) // 2
            if self.cum[mid] <= u:
                lo = mid
            else:
                hi = mid
        a, b = self.pts[lo], self.pts[hi]
        t = (u - self.cum[lo]) / max(self.cum[hi] - self.cum[lo], 1e-9)
        return a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t

    def at(self, u):
        """(y, z, ny, nz): point and outward normal; the feet get a horizontal normal."""
        y, z = self.pos(u)
        if u <= 1e-6:
            return y, z, 1.0, 0.0
        if u >= self.total - 1e-6:
            return y, z, -1.0, 0.0
        p0, p1 = self.pos(u - 1.5), self.pos(u + 1.5)  # wider window: smooth across polyline kinks
        dy, dz = p1[0] - p0[0], p1[1] - p0[1]
        ln = math.hypot(dy, dz) or 1.0
        return y, z, dz / ln, -dy / ln  # back side first, so outward = (dz, -dy)

    def samples(self, spacing):
        """Arc lengths from 0 to total with a step spacing(z) (cm)."""
        us = [0.0]
        while True:
            step = spacing(self.pos(us[-1])[1])
            if us[-1] + step >= self.total - 0.35 * step:
                break
            us.append(us[-1] + step)
        us.append(self.total)
        return us


class HedgeLook:
    """Leafy displacement of the body (cm): soft cut irregularity + two sizes of leaf clumps (cellular) + fine noise."""

    def __init__(self, seed, big_scale, big_amp, cell, cell_amp, cell_round, cell2, cell2_amp, fine_scale, fine_amp,
                 stretch_u=1.0):
        rng = random.Random(seed)
        self.off = [Vector((rng.uniform(-500, 500), rng.uniform(-500, 500), rng.uniform(-500, 500))) for _ in range(6)]
        self.big_scale, self.big_amp = big_scale, big_amp
        self.cell, self.cell_amp, self.cell_round = cell, cell_amp, cell_round
        self.cell2, self.cell2_amp = cell2, cell2_amp
        self.fine_scale, self.fine_amp = fine_scale, fine_amp
        self.stretch_u = stretch_u

    def displace(self, x, u):
        """Offset along the profile normal and the clump strength 0..1 (1 = middle of a leaf clump)."""
        cx, cy = _ring_point(x)
        o = self.off
        p = Vector((cx, cy, u))
        ps = Vector((cx, cy, u * self.stretch_u))
        big = noise.noise(p / self.big_scale + o[0]) + 0.5 * noise.noise(p / (self.big_scale * 0.45) + o[1])
        d, _ = noise.voronoi(ps / self.cell + o[2], distance_metric="DISTANCE")
        clump = min((d[1] - d[0]) / 0.55, 1.0) ** self.cell_round
        d2, _ = noise.voronoi(ps / self.cell2 + o[3], distance_metric="DISTANCE")
        clump2 = min((d2[1] - d2[0]) / 0.5, 1.0) ** 0.6
        fine = noise.noise(p / self.fine_scale + o[4])
        off = big * self.big_amp + (clump - 0.5) * self.cell_amp + (clump2 - 0.5) * self.cell2_amp + fine * self.fine_amp
        return off, clump * (0.7 + 0.3 * clump2)

    def dent(self, y, z):
        """0..1 noise for the inward dents of the end caps (the same at both ends)."""
        p = Vector((y, z, 0.0))
        return 0.5 + 0.35 * noise.noise(p / self.cell + self.off[5]) + 0.25 * noise.noise(p / (self.cell * 0.4) + self.off[4])


class LeafLook:
    """Leaf scales on the hedge surface (cm)."""

    def __init__(self, seed, spacing_side, spacing_top, spacing_cap, length, ratio, lift, upright, tilt, accent_side,
                 accent_top, min_z=2.0):
        self.seed = seed
        self.spacing_side, self.spacing_top, self.spacing_cap = spacing_side, spacing_top, spacing_cap
        self.length, self.ratio = length, ratio      # (min, max) length, (min, max) width / length
        self.lift = lift                             # (min, max) tip height above the surface
        self.upright = upright                       # 0..1: how strongly the tips point up (thuja sprays grow up)
        self.tilt = math.radians(tilt)               # random tilt of the leaf normal
        self.accent_side, self.accent_top = accent_side, accent_top
        self.min_z = min_z


def _leaf(c, n, t, length, width, lift, emb=0.25):
    """Kite leaf around surface point c: tail sunk in, wings just above the surface, raised midrib to the tip."""
    s = n.cross(t)
    base = c - n * emb
    tail = base - t * (length * 0.42) - n * 0.6
    tip = base + t * (length * 0.58) + n * lift
    mid = base + t * (length * 0.08) + n * (0.35 + lift * 0.3)
    left = mid + s * (width / 2) - n * (lift * 0.12)
    right = mid - s * (width / 2) - n * (lift * 0.12)
    return [tail, right, tip, left], [(0, 1, 2), (0, 2, 3)]


def _point_in_poly(y, z, poly):
    inside = False
    j = len(poly) - 1
    for i in range(len(poly)):
        yi, zi = poly[i]
        yj, zj = poly[j]
        if (zi > z) != (zj > z) and y < (yj - yi) * (z - zi) / (zj - zi + 1e-12) + yi:
            inside = not inside
        j = i
    return inside


def _dist_to_poly(y, z, poly):
    best = 1e9
    for (ay, az), (by, bz) in zip(poly, poly[1:] + poly[:1]):
        dy, dz = by - ay, bz - az
        t = max(0.0, min(1.0, ((y - ay) * dy + (z - az) * dz) / (dy * dy + dz * dz + 1e-12)))
        best = min(best, math.hypot(y - ay - t * dy, z - az - t * dz))
    return best


def hedge_module(it, W, D, H, prof, look, leaves, dx, ds_side, ds_top, top_band, mat_body, mat_leaf, mat_accent,
                 cap_depth=4.0, cap_ramp=9.0, cap_rings=(0.94, 0.84, 0.55), angle=50.0):
    """Builds the hedge module (body + leaves), scales it to exactly W x D x H and adds it to the item."""
    pr = Profile(hedge_profile(**prof))
    band_z = pr.zmax - top_band
    rng = random.Random(leaves.seed)
    up = Vector((0, 0, 1))
    lverts, lfaces, lmats = [], [], []

    def tip_direction(n):
        """Upwards on the sides (growing sprays), random where the surface faces up."""
        a = rng.uniform(0, 2 * math.pi)
        e1 = Vector((1, 0, 0)) if abs(n.x) < 0.9 else Vector((0, 1, 0))
        e1 = (e1 - n * e1.dot(n)).normalized()
        rnd = e1 * math.cos(a) + n.cross(e1) * math.sin(a)
        g = up - n * up.dot(n)
        t = g.normalized() * leaves.upright + rnd * (1.0 - leaves.upright) if g.length > 0.25 else rnd
        t = t - n * t.dot(n)
        return (t if t.length > 1e-4 else rnd).normalized()

    def tilted(n):
        n = n.copy()
        tl = leaves.tilt
        n.rotate(Euler((rng.uniform(-tl, tl), rng.uniform(-tl, tl), rng.uniform(-tl, tl))))
        return n.normalized()

    def emit(p, n, clump, top, lift_max=None, wrap=True):
        t = tip_direction(n)
        length = rng.uniform(*leaves.length)
        width = length * rng.uniform(*leaves.ratio)
        lift = rng.uniform(*leaves.lift) * (0.75 + 0.5 * clump)
        if lift_max is not None:
            lift = min(lift, lift_max)
        pts, tris = _leaf(p, n, t, length, width, lift)
        chance = (leaves.accent_top if top else leaves.accent_side) * (0.6 + 0.8 * clump)
        mat_i = 1 if rng.random() < chance else 0
        shifts = [0.0]  # a leaf crossing a module end is continued by its copy at the other end
        if wrap and max(q.x for q in pts) > W / 2 and p.x > 0:
            shifts.append(-W)
        if wrap and min(q.x for q in pts) < -W / 2 and p.x < 0:
            shifts.append(W)
        for shift in shifts:
            q = [[min(max(v.x + shift, -W / 2), W / 2), v.y, max(v.z, 0.0)] for v in pts]
            if max(v[0] for v in q) - min(v[0] for v in q) < 0.05:
                continue  # nothing of this copy is inside the module
            base = len(lverts)
            lverts.extend(q)
            for tri in tris:
                lfaces.append(tuple(base + k for k in tri))
                lmats.append(mat_i)

    # --- body grid -------------------------------------------------------------------------------------
    us = pr.samples(lambda z: ds_top if z > band_z else ds_side)
    nx = int(round(W / dx))
    xs = [-W / 2 + W * i / nx for i in range(nx + 1)]
    xs[0], xs[-1] = -W / 2, W / 2
    bverts, bfaces, grid = [], [], []
    for x in xs:
        row = []
        for u in us:
            y, z, ny, nz = pr.at(u)
            off, _ = look.displace(x, u)
            if z < 0.5:
                off *= 0.6  # calm foot line on the ground
            row.append(len(bverts))
            bverts.append([x, y + ny * off, max(z + nz * off, 0.0)])
        grid.append(row)
    nj = len(us)
    side_faces = set()  # indices of the leafy surface faces (for the periodic normals at the ends)
    for i in range(nx):
        a, b = grid[i], grid[i + 1]
        for j in range(nj - 1):
            side_faces.add(len(bfaces))
            bfaces.append((a[j], b[j], b[j + 1], a[j + 1]))
        bfaces.append((a[0], a[nj - 1], b[nj - 1], b[0]))  # bottom strip

    # --- end caps: dented inwards only (hidden in the neighbour, a leafy cut at the end of a run) ----------
    rim = [(bverts[vi][1], bverts[vi][2]) for vi in grid[0]]  # identical at both ends
    cy_c, cz_c = 0.0, pr.zmax * 0.45

    def cap_depth_at(y, z):
        ramp = _smoothstep(0.0, cap_ramp, _dist_to_poly(y, z, rim))
        return cap_depth * ramp * (0.3 + 0.7 * min(max(look.dent(y, z), 0.0), 1.0))

    for i, sign in ((0, -1.0), (nx, 1.0)):
        rings = [grid[i]]
        for f in cap_rings:  # scale of each inner ring towards the centre (dense near the rim, where the dent ramps)
            new = []
            for (y, z) in rim:
                yy, zz = cy_c + (y - cy_c) * f, cz_c + (z - cz_c) * f
                new.append(len(bverts))
                bverts.append([sign * (W / 2 - cap_depth_at(yy, zz)), yy, zz])
            rings.append(new)
        centre = len(bverts)
        bverts.append([sign * (W / 2 - cap_depth_at(cy_c, cz_c)), cy_c, cz_c])
        for r0, r1 in zip(rings, rings[1:]):
            for j in range(nj):
                k2 = (j + 1) % nj
                bfaces.append((r0[j], r0[k2], r1[k2], r1[j]))
        last = rings[-1]
        for j in range(nj):
            bfaces.append((last[j], last[(j + 1) % nj], centre))

    # --- leaves on the sides and the top -------------------------------------------------------------------
    u = 0.0
    while u < pr.total:
        du = leaves.spacing_top if pr.pos(u)[1] > band_z else leaves.spacing_side
        n_row = max(int(round(W / du)), 1)
        for i in range(n_row):
            lx = -W / 2 + (i + 0.5 + rng.uniform(-0.45, 0.45)) * W / n_row
            lu = min(max(u + rng.uniform(-0.45, 0.45) * du, 0.0), pr.total)
            y, z, ny, nz = pr.at(lu)
            off, clump = look.displace(lx, lu)
            p = Vector((lx, y + ny * off, z + nz * off))
            if p.z < leaves.min_z:
                continue
            emit(p, tilted(Vector((0.0, ny, nz))), clump, p.z > band_z)
        u += du

    # --- leaves on the end caps (stay inside x = +-W/2: lift limited by the local dent) ---------------------
    ys = [q[0] for q in rim]
    zs = [q[1] for q in rim]
    sc = leaves.spacing_cap
    for sign in (-1.0, 1.0):
        z = min(zs) + sc / 2
        while z < max(zs):
            y = min(ys) + sc / 2
            while y < max(ys):
                py, pz = y + rng.uniform(-0.45, 0.45) * sc, z + rng.uniform(-0.45, 0.45) * sc
                y += sc
                if pz < leaves.min_z or not _point_in_poly(py, pz, rim) or _dist_to_poly(py, pz, rim) < 2.5:
                    continue
                dep = cap_depth_at(py, pz)
                p = Vector((sign * (W / 2 - dep), py, pz))
                emit(p, tilted(Vector((sign, 0.0, 0.0))), 0.5, pz > band_z, lift_max=max(dep - 0.3, 0.2), wrap=False)
            z += sc

    # --- exact module size: one transform for everything (keeps both ends identical) --------------------
    ally = [v[1] for v in bverts + lverts]
    allz = [v[2] for v in bverts + lverts]
    ymid = (min(ally) + max(ally)) / 2
    sy = D / (max(ally) - min(ally))
    sz = H / max(allz)

    def to_bm(verts, faces, mats=None):
        bm = bmesh.new()
        bv = [bm.verts.new((v[0] * CM, (v[1] - ymid) * sy * CM, v[2] * sz * CM)) for v in verts]
        for k, f in enumerate(faces):
            face = bm.faces.new([bv[i] for i in f])
            if mats:
                face.material_index = mats[k]
        return bm

    body = to_bm(bverts, bfaces)
    bmesh.ops.recalc_face_normals(body, faces=body.faces[:])
    body_ob = it.add(body, mat_body, shade="auto", angle=angle, grain="z")
    periodic_end_normals(body_ob, W / 2 * CM, side_faces)
    leaf_ob = it.add(to_bm(lverts, lfaces, lmats), mat_leaf, shade="flat", grain="z")
    leaf_ob.data.materials.append(it.materials[mat_accent])


def periodic_end_normals(ob, half_w, side_faces):
    """Smooth normals at both module ends as if the neighbour were there: the rim vertices at x = -half_w and
    x = +half_w (same y, z) get the average of both one-sided normals, so a row of modules shades without a line."""
    me = ob.data
    normals = [Vector(c.vector) for c in me.corner_normals]
    groups = {}
    for poly in me.polygons:
        if poly.index not in side_faces:
            continue
        for li in poly.loop_indices:
            co = me.vertices[me.loops[li].vertex_index].co
            if abs(abs(co.x) - half_w) < 1e-6:
                groups.setdefault((round(co.y, 5), round(co.z, 5)), []).append(li)
    for lis in groups.values():
        n = Vector((0.0, 0.0, 0.0))
        for li in lis:
            n += normals[li]
        if n.length > 1e-9:
            n.normalize()
            for li in lis:
                normals[li] = n.copy()
    me.normals_split_custom_set([tuple(n) for n in normals])


@item("Hedge_thuja")
def hedge_thuja(it: Item):
    """Clipped thuja hedge module 200 x 100 x 220: slightly tapered, rounded top edges, dark and dense; broad
    scale-leaf sprays point upwards and overlap like shingles, some lighter fresh tips (Leaves_Green)."""
    prof = dict(half_bottom=45.0, half_top=41.5, height=210.0, r_top=17.0, crown=2.5, belly=1.5, foot_h=22.0, foot_in=3.0)
    look = HedgeLook(seed=13, big_scale=70.0, big_amp=2.4, cell=16.0, cell_amp=5.5, cell_round=0.6, cell2=7.0,
                     cell2_amp=1.8, fine_scale=5.0, fine_amp=0.6, stretch_u=0.62)
    leaves = LeafLook(seed=131, spacing_side=5.3, spacing_top=4.3, spacing_cap=7.0, length=(7.0, 11.0), ratio=(0.5, 0.68),
                      lift=(1.3, 2.4), upright=0.75, tilt=20.0, accent_side=0.09, accent_top=0.22)
    hedge_module(it, 200.0, 100.0, 220.0, prof, look, leaves, dx=6.0, ds_side=5.6, ds_top=4.4, top_band=40.0,
                 mat_body="Leaves_Dark", mat_leaf="Leaves_Dark", mat_accent="Leaves_Green", cap_depth=4.5, angle=50.0)


@item("Hedge_privet")
def hedge_privet(it: Item):
    """Clipped privet hedge module 200 x 80 x 120: boxy with a flat top, small light leaves in all directions over a
    darker body (Leaves_Green) that shows as depth between them."""
    prof = dict(half_bottom=36.5, half_top=36.0, height=113.0, r_top=9.0, crown=1.0, belly=1.0, foot_h=12.0, foot_in=2.0)
    look = HedgeLook(seed=7, big_scale=55.0, big_amp=2.2, cell=11.0, cell_amp=5.0, cell_round=0.5, cell2=4.8,
                     cell2_amp=1.6, fine_scale=3.5, fine_amp=0.4)
    leaves = LeafLook(seed=71, spacing_side=3.9, spacing_top=3.2, spacing_cap=5.5, length=(5.0, 7.5), ratio=(0.42, 0.55),
                      lift=(0.9, 1.7), upright=0.15, tilt=35.0, accent_side=0.10, accent_top=0.07)
    hedge_module(it, 200.0, 80.0, 120.0, prof, look, leaves, dx=5.0, ds_side=4.2, ds_top=3.4, top_band=24.0,
                 mat_body="Leaves_Green", mat_leaf="Leaves_Fresh", mat_accent="Leaves_Green", cap_depth=3.5, cap_ramp=7.0,
                 angle=50.0)


