"""Villa props, group G14 (garden trees): deciduous tree, apple tree, conifer (trunk + canopy each) and a bed shrub.

Exported as SM_<id>. Real cm. Every tree is two models with the origin at the foot of the trunk (x = y = z = 0):
the trunk with its main branches and the canopy, so the game can hide the canopy alone when a figure stands
underneath. Branches reach into the leaf clumps, so the bare skeleton still reads as a tree. The level scales the
trees uniformly (0.8-1.2) and turns them at random, so nothing depends on a facing direction.
All shapes are deterministic (fixed seeds); trunk and canopy share one layout per tree.
"""

import math
import random

import bmesh
from mathutils import Vector, noise

from dc_items import item
from dc_items_small import spline
from dc_mesh import CM, Item

# ---------------------------------------------------------------------------------------------
# Noise
# ---------------------------------------------------------------------------------------------


def _offset(seed):
    s = float(seed % 997)
    return Vector(((s * 12.9898) % 113.0, (s * 78.233) % 127.0, (s * 37.719) % 131.0))


def _perlin(p, freq=1.0, seed=0, octaves=2):
    """Smooth noise, roughly -0.8..0.8."""
    o = _offset(seed)
    v, amp, norm, f = 0.0, 1.0, 0.0, freq
    for k in range(octaves):
        q = Vector((p[0] * f + o.x + k * 17.0, p[1] * f + o.y, p[2] * f + o.z))
        v += amp * noise.noise(q)
        norm += amp
        amp *= 0.5
        f *= 2.03
    return v / norm


def _cells(p, freq, seed):
    """Rounded bumps 0..1 on a cellular pattern - the small leaf clusters on one clump."""
    o = _offset(seed + 501)
    d = noise.voronoi(Vector((p[0] * freq + o.x, p[1] * freq + o.y, p[2] * freq + o.z)))[0][0]
    return max(0.0, 1.0 - (d / 0.78) ** 2)


# ---------------------------------------------------------------------------------------------
# Wood: tapered tubes with rotation-minimising frames
# ---------------------------------------------------------------------------------------------


def _tube(points, radii, sides=10, bark=0.0, knots=0.0, seed=0, flare=None, tip=True, cap=True):
    """Round, tapered branch along a polyline (cm). radii: one per point (cm).

    bark: furrows along the wood (fraction of the radius); knots: slow lumps (gnarled wood);
    flare: fn(z_cm, world_angle) -> extra radius (root buttresses). Nothing goes below the ground (z >= 0).
    tip: the last point becomes a pointed end, otherwise the end is capped.
    """
    pts, rad = [], []
    for p, r in zip(points, radii):
        v = Vector(p)
        if pts and (v - pts[-1]).length < 0.05:
            continue
        pts.append(v)
        rad.append(r)
    n = len(pts)
    tans = [(pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)]).normalized() for i in range(n)]
    ref = Vector((1, 0, 0)) if abs(tans[0].x) < 0.9 else Vector((0, 1, 0))
    nrm = tans[0].cross(ref).normalized()
    frames = []
    for i in range(n):
        if i:
            nrm = tans[i - 1].rotation_difference(tans[i]) @ nrm
            nrm = (nrm - tans[i] * nrm.dot(tans[i])).normalized()
        frames.append((nrm.copy(), tans[i].cross(nrm).normalized()))
    arc = [0.0]
    for i in range(1, n):
        arc.append(arc[-1] + (pts[i] - pts[i - 1]).length)
    bm = bmesh.new()
    rings = []
    for i in range(n - 1 if tip else n):
        nv, bv = frames[i]
        ring = []
        for k in range(sides):
            a = 2.0 * math.pi * k / sides
            d = nv * math.cos(a) + bv * math.sin(a)
            r = rad[i]
            if bark:
                r *= 1.0 + bark * _perlin((d.x * 1.9, d.y * 1.9, d.z * 1.9 + arc[i] / 60.0), 1.0, seed, 2)
            if knots:
                r *= 1.0 + knots * _perlin((d.x * 0.6, d.y * 0.6, d.z * 0.6 + arc[i] / 22.0), 1.0, seed + 7, 1)
            if flare:
                r += flare(pts[i].z, math.atan2(d.y, d.x))
            p = pts[i] + d * r
            p.z = max(p.z, 0.0)
            ring.append(bm.verts.new(p * CM))
        rings.append(ring)
    for a, b in zip(rings, rings[1:]):
        for k in range(sides):
            bm.faces.new((a[k], a[(k + 1) % sides], b[(k + 1) % sides], b[k]))
    if tip:
        tv = bm.verts.new(Vector((pts[-1].x, pts[-1].y, max(pts[-1].z, 0.0))) * CM)
        for k in range(sides):
            bm.faces.new((rings[-1][k], rings[-1][(k + 1) % sides], tv))
    elif cap:
        bm.faces.new(rings[-1])
    if cap:
        bm.faces.new(list(reversed(rings[0])))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def _taper(n, r0, r1, power=1.0):
    return [r0 + (r1 - r0) * (i / max(n - 1, 1)) ** power for i in range(n)]


def _limb(rng, start, end, bend=0.0, kink=0.0, ctrl=4, steps=4):
    """Curved branch path start -> end (cm). bend > 0 bows it upward, < 0 lets it sag; kink adds crooked offsets."""
    s, e = Vector(start), Vector(end)
    d = e - s
    side = d.cross(Vector((0, 0, 1)))
    side = side.normalized() if side.length > 1e-6 else Vector((1, 0, 0))
    up = side.cross(d).normalized()
    pts = []
    for i in range(ctrl):
        t = i / (ctrl - 1)
        p = s + d * t + up * bend * math.sin(math.pi * t)
        if 0 < i < ctrl - 1:
            p += side * rng.uniform(-kink, kink) + up * rng.uniform(-kink, kink) * 0.6
        pts.append(tuple(p))
    return [Vector(p) for p in spline(pts, steps)]


def _nearest_index(path, z, lo=0.3, hi=0.85):
    a, b = int(len(path) * lo), max(int(len(path) * hi), int(len(path) * lo) + 1)
    return min(range(a, min(b, len(path))), key=lambda i: abs(path[i].z - z))


def _azimuth(v):
    return math.atan2(v.y, v.x)


def _angle_diff(a, b):
    return abs((a - b + math.pi) % (2 * math.pi) - math.pi)


def _roots(it, rng, count, reach, r0, seed, mat="Bark", phase=None):
    """Surface roots running from the trunk foot into the ground (phase: line them up with the flare lobes)."""
    phase = rng.uniform(0, 2 * math.pi) if phase is None else phase
    for k in range(count):
        a = phase + 2 * math.pi * k / count + rng.uniform(-0.12, 0.12)
        d = Vector((math.cos(a), math.sin(a), 0))
        length = reach * rng.uniform(0.75, 1.1)
        side = Vector((-d.y, d.x, 0)) * rng.uniform(-0.15, 0.15) * length
        ctrl = [d * 4 + Vector((0, 0, r0 * 1.8)), d * length * 0.4 + side * 0.5 + Vector((0, 0, r0 * 0.55)),
                d * length * 0.75 + side + Vector((0, 0, r0 * 0.1)), d * length + side + Vector((0, 0, -r0 * 0.4))]
        path = spline([tuple(c) for c in ctrl], 3)
        it.add(_tube(path, _taper(len(path), r0, r0 * 0.5, 0.6), sides=8, bark=0.12, seed=seed + k, tip=False), mat,
               shade="smooth", grain="z")


def _flare(r_extra, height, lobes, phase):
    def fn(z, a):
        if z >= height:
            return 0.0
        w = (1.0 - z / height) ** 2
        return r_extra * w * (0.45 + 0.55 * max(0.0, math.cos(lobes * (a - phase))) ** 1.5)

    return fn


# ---------------------------------------------------------------------------------------------
# Leaves: clumps of deformed icospheres, one shared layout per tree
# ---------------------------------------------------------------------------------------------


def _crown_layout(seed, n_shell, n_inner, rxy, rz, cz, r_clump, bottom=-0.5, gap=0.07, lump=0.13):
    """Clump centres on a lumpy ellipsoid shell (cm) plus a few inner ones that fill the gaps with shadow."""
    rng = random.Random(seed)
    golden = math.pi * (3.0 - math.sqrt(5.0))
    clumps = []
    for i in range(n_shell):
        zz = 1.0 - (1.0 - bottom) * (i + 0.5) / n_shell
        rad = math.sqrt(max(0.0, 1.0 - zz * zz))
        th = golden * i + rng.uniform(-0.3, 0.3)
        d = Vector((math.cos(th) * rad, math.sin(th) * rad, zz))
        if rng.random() < gap and -0.3 < zz < 0.8:
            continue  # a hole: look into the crown
        f = rng.uniform(0.9, 1.05) * (1.0 + lump * _perlin(d, 0.9, seed, 2) / 0.8)
        if abs(zz) < 0.45 and rng.random() < 0.18:
            f *= 1.14  # a branch end sticking out of the outline
        c = Vector((d.x * rxy * f, d.y * rxy * f, cz + d.z * rz * f))
        r = r_clump * rng.uniform(0.72, 1.22) * (1.08 if zz > 0.35 else 1.0)
        clumps.append(dict(c=c, r=r, d=d, seed=seed * 7 + i, outer=True))
    for i in range(n_inner):
        th = 2.0 * math.pi * i / max(n_inner, 1) + rng.uniform(-0.4, 0.4)
        zz = rng.uniform(-0.35, 0.55)
        rad = rng.uniform(0.25, 0.5)
        c = Vector((math.cos(th) * rxy * rad, math.sin(th) * rxy * rad, cz + zz * rz * 0.6))
        clumps.append(dict(c=c, r=r_clump * rng.uniform(1.05, 1.3), d=Vector((0, 0, 1)), seed=seed * 7 + 500 + i, outer=False))
    return clumps


def _normalise_layout(clumps, diameter, z0, z1, pad=1.15, sq_top=0.95, sq_bot=0.72):
    """Move/scale the clump centres so the crown roughly spans diameter and z0..z1 (fine fit comes after meshing)."""
    lo_x = min(c["c"].x - c["r"] * pad for c in clumps)
    hi_x = max(c["c"].x + c["r"] * pad for c in clumps)
    lo_y = min(c["c"].y - c["r"] * pad for c in clumps)
    hi_y = max(c["c"].y + c["r"] * pad for c in clumps)
    lo_z = min(c["c"].z - c["r"] * pad * sq_bot for c in clumps)
    hi_z = max(c["c"].z + c["r"] * pad * sq_top for c in clumps)
    s = diameter / max(hi_x - lo_x, hi_y - lo_y)
    sz = (z1 - z0) / (hi_z - lo_z)
    cx, cy = (lo_x + hi_x) / 2, (lo_y + hi_y) / 2
    for c in clumps:
        p = c["c"]
        c["c"] = Vector(((p.x - cx) * s, (p.y - cy) * s, z0 + (p.z - lo_z) * sz))
        c["r"] *= (s + sz) / 2
    return clumps


def _clump_point(cl, n):
    """Surface point of a clump in direction n (unit vector)."""
    r = cl["r"] * (1.0 + cl.get("lump", 0.28) * _perlin(n, 1.2, cl["seed"], 2) / 0.8
                   + cl.get("fine", 0.0) * _perlin(n, 3.6, cl["seed"] + 3, 2) / 0.8
                   + cl.get("bump", 0.26) * (_cells(n, cl.get("freq", 2.0), cl["seed"]) - 0.45))
    sq = cl.get("sq_top", 0.95) if n.z > 0 else cl.get("sq_bot", 0.72)
    c = cl["c"]
    return Vector((c.x + n.x * r, c.y + n.y * r, c.z + n.z * r * sq))


def _clump(bm, cl, sub=3):
    res = bmesh.ops.create_icosphere(bm, subdivisions=sub, radius=1.0)
    for v in res["verts"]:
        v.co = _clump_point(cl, v.co.normalized()) * CM


def _fit(bms, diameter, z0, z1):
    """Scale/move the canopy meshes so the footprint is `diameter` x `diameter` (centred on the trunk) and spans z0..z1.

    X and Y are scaled separately (only a few per cent): the level sizes trees by one crown radius.
    """
    vs = [v for bm in bms for v in bm.verts]
    lo = [min(v.co[i] for v in vs) for i in range(3)]
    hi = [max(v.co[i] for v in vs) for i in range(3)]
    cx, cy = (lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2
    sx = diameter * CM / (hi[0] - lo[0])
    sy = diameter * CM / (hi[1] - lo[1])
    sz = (z1 - z0) * CM / (hi[2] - lo[2])
    for v in vs:
        v.co = Vector(((v.co.x - cx) * sx, (v.co.y - cy) * sy, z0 * CM + (v.co.z - lo[2]) * sz))


def _add_meshes(it, groups, grain="x"):
    for mat, bm in groups.items():
        if not bm.verts:
            bm.free()
            continue
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
        it.add(bm, mat, shade="smooth", grain=grain)


def _param_blob(bm, nu, nv, fn):
    """Closed surface from fn(u, v) -> Vector (cm); u in [0, 1] runs pole to pole, v in [0, 2 pi)."""
    p0 = bm.verts.new(fn(0.0, 0.0) * CM)
    rings = []
    for i in range(1, nu):
        u = i / nu
        rings.append([bm.verts.new(fn(u, 2.0 * math.pi * k / nv) * CM) for k in range(nv)])
    p1 = bm.verts.new(fn(1.0, 0.0) * CM)
    for k in range(nv):
        bm.faces.new((p0, rings[0][(k + 1) % nv], rings[0][k]))
        bm.faces.new((p1, rings[-1][k], rings[-1][(k + 1) % nv]))
    for a, b in zip(rings, rings[1:]):
        for k in range(nv):
            bm.faces.new((a[k], a[(k + 1) % nv], b[(k + 1) % nv], b[k]))


# ---------------------------------------------------------------------------------------------
# Deciduous tree (lime/oak type): crown 600 wide, 800 high, crown from about 275
# ---------------------------------------------------------------------------------------------

DECID_FORK = Vector((9.0, -7.0, 272.0))


def _deciduous_layout():
    clumps = _crown_layout(seed=1401, n_shell=62, n_inner=10, rxy=245, rz=185, cz=545, r_clump=54, bottom=-0.55, lump=0.26,
                           gap=0.12)
    return _normalise_layout(clumps, 600, 275, 800)


@item("Tree_deciduous_trunk")
def tree_deciduous_trunk(it: Item):
    """Slightly curved trunk with root flare, five leaders from the fork and one branch into every outer leaf clump."""
    rng = random.Random(1402)
    clumps = _deciduous_layout()
    outer = [c for c in clumps if c["outer"]]
    cz = sum(c["c"].z for c in clumps) / len(clumps)
    # Trunk: runs on above the fork as the central leader into the highest clump (no cut end at the fork).
    top = max(outer, key=lambda c: c["c"].z)
    tip = top["c"] - Vector((0, 0, 25))
    mid = DECID_FORK.lerp(tip, 0.45) + Vector((rng.uniform(-12, 12), rng.uniform(-12, 12), 0))
    steps = 4
    ctrl = [(0, 0, 0), (2, 2, 70), (6, -1, 150), (9, -5, 215), tuple(DECID_FORK), tuple(mid), tuple(tip)]
    trunk = [Vector(p) for p in spline(ctrl, steps)]
    fork_i = 4 * steps
    above = [0.0]
    for i in range(fork_i + 1, len(trunk)):
        above.append(above[-1] + (trunk[i] - trunk[i - 1]).length)
    radii = [27.0 - 8.0 * (p.z / DECID_FORK.z) ** 0.8 for p in trunk[:fork_i]]
    for d in above:
        t = d / above[-1]
        base = 1.6 + (11.0 - 1.6) * (1.0 - t) ** 1.5
        radii.append(base + (19.0 - base) * max(0.0, 1.0 - d / 50.0) ** 2)
    it.add(_tube(trunk, radii, sides=16, bark=0.09, knots=0.05, seed=1, flare=_flare(22, 85, 5, 0.4), tip=True),
           "Bark", shade="smooth", grain="z")
    _roots(it, rng, 5, 62, 13, seed=20, phase=0.4)
    # Leaders: four into the upper crown around the compass.
    leaders = [(trunk[fork_i:], radii[fork_i:], None)]
    used = {id(top)}
    phase = rng.uniform(0, 2 * math.pi)
    upper = [c for c in outer if c["c"].z > cz + 40]
    for k in range(4):
        az = phase + k * math.pi / 2
        best = min((c for c in upper if id(c) not in used), key=lambda c: _angle_diff(_azimuth(c["c"]), az))
        used.add(id(best))
        target = Vector((best["c"].x * 0.72, best["c"].y * 0.72, best["c"].z - 10))
        start = DECID_FORK + Vector((math.cos(az) * 3, math.sin(az) * 3, -28))
        path = _limb(rng, start, target, bend=18, kink=10, ctrl=5, steps=4)
        rr = _taper(len(path), 13.5, 2.0, 0.85)
        it.add(_tube(path, rr, sides=11, bark=0.08, seed=40 + k), "Bark", shade="smooth", grain="z")
        leaders.append((path, rr, az))
    # Secondary branches: from the leader nearest in direction into each remaining outer clump.
    targets = []
    for c in sorted(outer, key=lambda c: -abs(c["d"].z)):
        if id(c) in used or c["c"].z < 300 or any((c["c"] - t["c"]).length < 115 for t in targets):
            continue
        targets.append(c)
    for j, c in enumerate(targets):
        az = _azimuth(c["c"])
        path0, rr0, _ = min((l for l in leaders if l[2] is not None), key=lambda l: _angle_diff(l[2], az))
        i0 = _nearest_index(path0, c["c"].z - 90, 0.25, 0.8)
        start = path0[i0]
        target = c["c"] - (c["c"] - start) * 0.15
        path = _limb(rng, start, target, bend=-0.10 * (target - start).length, kink=7, ctrl=4, steps=3)
        r0 = min(rr0[i0] * 0.62, 7.0)
        it.add(_tube(path, _taper(len(path), r0, 1.0, 0.9), sides=7, bark=0.06, seed=60 + j), "Bark", shade="smooth", grain="z")


@item("Tree_deciduous_canopy")
def tree_deciduous_canopy(it: Item):
    """Crown of overlapping leaf clumps: lumpy, open outline from above, dark inner clumps give depth in the gaps."""
    clumps = _deciduous_layout()
    groups = {"Leaves_Green": bmesh.new(), "Leaves_Dark": bmesh.new()}
    for c in clumps:
        c.update(lump=0.30, fine=0.06, bump=0.2, freq=1.8)
        _clump(groups["Leaves_Green" if c["outer"] else "Leaves_Dark"], c, sub=3 if c["outer"] else 2)
    _fit(list(groups.values()), 600, 275, 800)
    _add_meshes(it, groups)


# ---------------------------------------------------------------------------------------------
# Apple tree: gnarled short trunk, open crown 400 wide, 450 high, crown from about 130, red apples
# ---------------------------------------------------------------------------------------------

FRUIT_FORK = Vector((10.0, -6.0, 112.0))


def _fruit_layout():
    clumps = _crown_layout(seed=1411, n_shell=36, n_inner=6, rxy=155, rz=88, cz=300, r_clump=43, bottom=-0.45, gap=0.16,
                           lump=0.22)
    return _normalise_layout(clumps, 400, 135, 450)


@item("Tree_fruit_trunk")
def tree_fruit_trunk(it: Item):
    """Short, leaning, gnarled trunk; four crooked scaffold limbs and side branches into the clumps."""
    rng = random.Random(1412)
    clumps = _fruit_layout()
    trunk = [Vector(p) for p in spline([(0, 0, 0), (3, 2, 30), (9, -2, 62), (6, -7, 90), tuple(FRUIT_FORK)], 4)]
    radii = [13.0 - 2.5 * (p.z / FRUIT_FORK.z) for p in trunk]
    it.add(_tube(trunk, radii, sides=14, bark=0.12, knots=0.14, seed=2, flare=_flare(10, 45, 4, 1.1), tip=False),
           "Bark", shade="smooth", grain="z")
    _roots(it, rng, 4, 36, 9, seed=120, phase=1.1)
    # Knobbly crotch where the limbs leave the trunk.
    knuckle = bmesh.new()
    res = bmesh.ops.create_icosphere(knuckle, subdivisions=3, radius=1.0)
    for v in res["verts"]:
        n = v.co.normalized()
        r = 12.5 * (1.0 + 0.18 * _perlin(n, 1.5, 9, 2) / 0.8)
        v.co = (FRUIT_FORK + Vector((n.x * r, n.y * r, n.z * r * 1.25 - 3))) * CM
    it.add(knuckle, "Bark", shade="smooth", grain="z")
    outer = [c for c in clumps if c["outer"]]
    cz = sum(c["c"].z for c in clumps) / len(clumps)
    limbs, used = [], set()
    phase = rng.uniform(0, 2 * math.pi)
    mid = [c for c in outer if c["c"].z > cz - 30]
    for k in range(4):
        az = phase + k * math.pi / 2 + rng.uniform(-0.3, 0.3)
        best = min((c for c in mid if id(c) not in used), key=lambda c: _angle_diff(_azimuth(c["c"]), az))
        used.add(id(best))
        target = Vector((best["c"].x * 0.78, best["c"].y * 0.78, best["c"].z - 15))
        start = FRUIT_FORK + Vector((math.cos(az) * 2.5, math.sin(az) * 2.5, -14))
        path = _limb(rng, start, target, bend=-12, kink=16, ctrl=5, steps=4)
        rr = _taper(len(path), 7.5, 1.3, 0.8)
        it.add(_tube(path, rr, sides=9, bark=0.10, knots=0.12, seed=140 + k), "Bark", shade="smooth", grain="z")
        limbs.append((path, rr, az))
    for j, c in enumerate(outer):
        if id(c) in used or c["c"].z < 170:
            continue
        az = _azimuth(c["c"])
        path0, rr0, _ = min(limbs, key=lambda l: _angle_diff(l[2], az))
        i0 = _nearest_index(path0, c["c"].z - 50, 0.3, 0.85)
        start = path0[i0]
        target = c["c"] - (c["c"] - start) * 0.15
        path = _limb(rng, start, target, bend=-0.08 * (target - start).length, kink=9, ctrl=4, steps=3)
        r0 = min(rr0[i0] * 0.65, 3.8)
        it.add(_tube(path, _taper(len(path), r0, 0.7, 0.9), sides=6, bark=0.06, knots=0.08, seed=160 + j), "Bark",
               shade="smooth", grain="z")


@item("Tree_fruit_canopy")
def tree_fruit_canopy(it: Item):
    """Looser crown with more gaps; red apples sit on the outside of the clumps (most on top and at the sides)."""
    rng = random.Random(1413)
    clumps = _fruit_layout()
    groups = {"Leaves_Green": bmesh.new(), "Leaves_Dark": bmesh.new(), "Fruit_Red": bmesh.new()}
    for c in clumps:
        c.update(lump=0.3, fine=0.05, bump=0.24, freq=2.0)
        _clump(groups["Leaves_Green" if c["outer"] else "Leaves_Dark"], c, sub=3)
    outer = [c for c in clumps if c["outer"]]
    placed = []
    tries = 0
    while len(placed) < 60 and tries < 2000:
        tries += 1
        c = rng.choice(outer)
        out = Vector((c["c"].x, c["c"].y, (c["c"].z - 290) * 1.5)).normalized()
        n = Vector((rng.gauss(0, 1), rng.gauss(0, 1), rng.gauss(0, 1))).normalized()
        if n.dot(out) < 0.25 or n.z < -0.55:
            continue
        p = _clump_point(c, n)
        if any((p - q).length < 22 for q in placed):
            continue
        # Hidden inside a neighbouring clump? Then skip it.
        if any(o is not c and (p - o["c"]).length < o["r"] * 0.95 for o in clumps):
            continue
        placed.append(p)
        r = rng.uniform(5.4, 6.4)
        centre = p - n * r * 0.35
        res = bmesh.ops.create_uvsphere(groups["Fruit_Red"], u_segments=8, v_segments=6, radius=r * CM)
        for v in res["verts"]:
            v.co = Vector((v.co.x + centre.x * CM, v.co.y + centre.y * CM, v.co.z * 0.88 + centre.z * CM))
    _fit(list(groups.values()), 400, 130, 450)
    _add_meshes(it, groups)


# ---------------------------------------------------------------------------------------------
# Conifer (spruce/fir): 500 wide at the bottom, 900 high, branch tiers almost down to the ground
# ---------------------------------------------------------------------------------------------

CONIFER_TOP = 900.0


def _conifer_tiers():
    rng = random.Random(1421)
    tiers = []
    count = 11
    for i in range(count):
        f = i / (count - 1)
        h = 96 + 712 * f ** 0.82
        r = 20 + 230 * (1 - f) ** 1.05
        n = max(4, round(4 + 8 * (1 - f)))
        rot = i * 2.39996 + rng.uniform(-0.25, 0.25)
        tiers.append((h, r, n, rot, f))
    return tiers


def _conifer_radius(z, tiers):
    """Crown radius at height z (cm), from the tier table."""
    if z <= tiers[0][0]:
        return tiers[0][1]
    for (h0, r0, *_), (h1, r1, *_) in zip(tiers, tiers[1:]):
        if h0 <= z <= h1:
            return r0 + (r1 - r0) * (z - h0) / (h1 - h0)
    return max(0.0, tiers[-1][1] * (CONIFER_TOP - z) / (CONIFER_TOP - tiers[-1][0]))


@item("Tree_conifer_trunk")
def tree_conifer_trunk(it: Item):
    """Straight tapered stem with a slight root flare and whorls of thin drooping branches (one per tier)."""
    rng = random.Random(1422)
    tiers = _conifer_tiers()
    stem = [Vector((1.5 * math.sin(z / 140.0), 1.2 * math.cos(z / 170.0) - 1.2, z)) for z in range(0, 841, 40)]
    stem.append(Vector((0, 0, CONIFER_TOP - 45)))  # the tip stays inside the canopy
    radii = [max(1.2, 17.0 * (1 - p.z / CONIFER_TOP) ** 0.9) for p in stem]
    it.add(_tube(stem, radii, sides=12, bark=0.07, seed=3, flare=_flare(10, 50, 4, 0.2)), "Bark", shade="smooth", grain="z")
    _roots(it, rng, 4, 45, 8, seed=220, phase=0.2)
    for i, (h, r, n, rot, f) in enumerate(tiers):
        if r < 35:
            continue
        for k in range(0, n, 2):
            a = rot + 2 * math.pi * k / n + rng.uniform(-0.15, 0.15)
            d = Vector((math.cos(a), math.sin(a), 0))
            length = r * rng.uniform(0.7, 0.82)
            base = Vector((0, 0, h - 4))
            ctrl = [base, base + d * length * 0.35 + Vector((0, 0, -0.04 * r)), base + d * length * 0.7 + Vector((0, 0, -0.13 * r)),
                    base + d * length + Vector((0, 0, -0.14 * r))]
            path = [Vector(p) for p in spline([tuple(c) for c in ctrl], 3)]
            r0 = min(1.5 + 0.03 * r, 6.0) * (1.0 - 0.3 * f)
            it.add(_tube(path, _taper(len(path), r0, 0.6), sides=6, seed=240 + i * 13 + k), "Bark", shade="smooth", grain="z")


def _spray(bm, h, theta, length, width, thick, droop, hang, seed, r_start=6.0, lift=0.0, tilt=0.0, roll=0.0, nu=12, nv=8):
    """One branch spray of a tier: flat and ragged from above, tip slightly up, needle curtains hanging below.

    tilt: rise per cm outwards (branch angle), roll: twist of the spray around its own axis (radians).
    """
    ca, sa = math.cos(theta), math.sin(theta)
    cr, sr = math.cos(roll), math.sin(roll)

    def fn(u, v):
        t = u
        rad = r_start + (length - r_start) * t
        w = math.sin(math.pi * min(1.0, t ** 1.36)) ** 0.55
        w *= 1.0 + 0.35 * _perlin((t * 3.2, 0.0, 0.0), 1.0, seed + 3, 1) / 0.8  # uneven side branchlets
        zc = h + lift * t + tilt * (rad - r_start) - droop * t ** 1.7 + 0.05 * length * max(0.0, t - 0.8) / 0.2
        lat = width / 2 * w * math.cos(v)
        s = math.sin(v)
        if s >= 0:
            vert = thick / 2 * w * s
        else:
            vert = (thick / 2 * w + hang * t ** 1.4 * math.sin(math.pi * t) ** 0.5) * s
        lat, vert = lat * cr - vert * sr, lat * sr + vert * cr
        p = Vector((rad * ca - lat * sa, rad * sa + lat * ca, zc + vert))
        amp = 0.2 * width * w
        q = p / 21.0
        p += Vector((_perlin(q, 1.0, seed, 2), _perlin(q, 1.0, seed + 1, 2), 0.7 * _perlin(q, 1.0, seed + 2, 2))) * amp / 0.8
        return p

    _param_blob(bm, nu, nv, fn)


@item("Tree_conifer_canopy")
def tree_conifer_canopy(it: Item):
    """Dark tiers of branch sprays around a dark core: a ragged star per tier from above, curtains hanging below."""
    rng = random.Random(1423)
    tiers = _conifer_tiers()
    bm = bmesh.new()
    for i, (h, r, n, rot, f) in enumerate(tiers):
        n_main = max(5, round(5 + 9 * (1 - f)))
        top = max(0.0, (f - 0.75) / 0.25)  # the last tiers point upwards
        for k in range(n_main):
            a = rot + 2 * math.pi * k / n_main + rng.uniform(-0.14, 0.14)
            length = r * rng.uniform(0.84, 1.06)
            width = min(max(2 * math.pi * 0.62 * r / n_main * 1.2, 24), 80)
            thick = min(max(0.09 * r + 9, 12), 30)
            _spray(bm, h, a, length, width, thick, droop=0.21 * r * (1 - top), hang=0.26 * r * (1 - 0.6 * f),
                   seed=1500 + i * 31 + k, lift=4.0, tilt=rng.uniform(-0.08, 0.1) + 0.55 * top, roll=rng.uniform(-0.25, 0.25))
        # Second, shorter layer between the main sprays fills the tier.
        n_sub = max(3, n_main // 2)
        for k in range(n_sub):
            a = rot + 2 * math.pi * (k + 0.5) / n_sub + rng.uniform(-0.2, 0.2)
            length = r * rng.uniform(0.66, 0.8)
            width = min(max(2 * math.pi * 0.55 * r / n_sub * 0.85, 22), 70)
            thick = min(max(0.09 * r + 8, 12), 28)
            _spray(bm, h + 12, a, length, width, thick, droop=0.17 * r * (1 - top), hang=0.16 * r, seed=2500 + i * 31 + k,
                   lift=6.0, tilt=rng.uniform(-0.05, 0.12) + 0.6 * top, roll=rng.uniform(-0.3, 0.3), nu=10, nv=8)
    # Core: fills the inside so gaps show dark needles, not the sky; ends in the leader at the top.
    z_lo, z_hi = 70.0, CONIFER_TOP

    def core(u, v):
        z = z_lo + (z_hi - z_lo) * u
        rc = 0.5 * _conifer_radius(z, tiers) + 8 * (1 - u)
        if u < 0.04:
            rc *= u / 0.04
        p = Vector((rc * math.cos(v), rc * math.sin(v), z))
        p += Vector((math.cos(v), math.sin(v), 0)) * rc * 0.3 * _perlin(p / 18.0, 1.0, 1601, 2) / 0.8
        return p

    _param_blob(bm, 30, 14, core)
    groups = {"Leaves_Dark": bm}
    _fit(list(groups.values()), 500, 40, CONIFER_TOP)
    _add_meshes(it, groups)


# ---------------------------------------------------------------------------------------------
# Bed shrub: about 160 x 160 x 110
# ---------------------------------------------------------------------------------------------


@item("Deco_bush")
def deco_bush(it: Item):
    """Round shrub of small leaf clumps (dark inner clumps show in the gaps) on a few short stems."""
    rng = random.Random(1431)
    clumps = _crown_layout(seed=1432, n_shell=19, n_inner=2, rxy=56, rz=34, cz=55, r_clump=27, bottom=-0.3, gap=0.05, lump=0.2)
    clumps = _normalise_layout(clumps, 160, 0, 110)
    groups = {"Leaves_Green": bmesh.new(), "Leaves_Dark": bmesh.new()}
    for c in clumps:
        c.update(lump=0.3, fine=0.06, bump=0.2, freq=1.8, sq_bot=0.8)
        mat = "Leaves_Green" if c["outer"] else "Leaves_Dark"
        _clump(groups[mat], c, sub=3 if c["outer"] and c["d"].z > -0.1 else 2)
    _fit(list(groups.values()), 160, 0, 110)
    for bm in groups.values():  # flat where it sits on the soil
        for v in bm.verts:
            v.co.z = max(v.co.z, 0.0)
    _add_meshes(it, groups)
    # Short stems visible under the lowest clumps.
    for k in range(6):
        a = 2 * math.pi * k / 6 + rng.uniform(-0.3, 0.3)
        d = Vector((math.cos(a), math.sin(a), 0))
        path = [Vector(p) for p in spline([(0, 0, 0), tuple(d * 10 + Vector((0, 0, 18))), tuple(d * 26 + Vector((0, 0, 42)))], 3)]
        it.add(_tube(path, _taper(len(path), 2.6, 1.0), sides=6, bark=0.1, seed=1440 + k), "Wood_Dark", shade="smooth", grain="z")
