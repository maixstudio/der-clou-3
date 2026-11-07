"""Small item models: clothing, tools, loot and the jewellery box (registered into dc_items.BUILDERS)."""

import math

from dc_items import escutcheon, item
from dc_mesh import (Item, arc_points, circle_profile, cube, cylinder, gem, lathe, ngon_prism, puff, rect_profile, sphere,
                     sweep, torus, wobble)

# ---------------------------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------------------------


def spline(points, steps=6):
    """Catmull-Rom curve through the points (cm)."""
    pts = [tuple(p) for p in points]
    out = []
    for i in range(len(pts) - 1):
        p0 = pts[max(i - 1, 0)]
        p1, p2 = pts[i], pts[i + 1]
        p3 = pts[min(i + 2, len(pts) - 1)]
        for s in range(steps):
            t = s / steps
            t2, t3 = t * t, t * t * t
            out.append(tuple(0.5 * ((2 * p1[k]) + (-p0[k] + p2[k]) * t + (2 * p0[k] - 5 * p1[k] + 4 * p2[k] - p3[k]) * t2
                                    + (-p0[k] + 3 * p1[k] - 3 * p2[k] + p3[k]) * t3) for k in range(3)))
    out.append(pts[-1])
    return out


def squash(axis, factor):
    """Scale the part along one axis around its centre (e.g. an oval cross-section)."""

    def apply(me):
        if not me.vertices:
            return
        lo = min(v.co[axis] for v in me.vertices)
        hi = max(v.co[axis] for v in me.vertices)
        mid = (lo + hi) / 2
        for v in me.vertices:
            v.co[axis] = mid + (v.co[axis] - mid) * factor

    return apply


def gem_on(it, r, loc, mat="Gem", setting="Silver", prongs=4):
    """Cut stone in a small cup setting with prongs; loc = bottom of the setting."""
    x, y, z = loc
    it.add(lathe([(0, 0), (r * 0.7, 0), (r * 1.05, r * 0.55), (r * 0.95, r * 0.65), (0, r * 0.4)], 16), setting, loc=(x, y, z), shade="smooth")
    it.add(gem(r, r * 0.38, r * 0.7, 16), mat, loc=(x, y, z + r * 0.75), shade="flat")
    for k in range(prongs):
        a = 2 * math.pi * k / prongs + math.pi / prongs
        it.add(cylinder(r * 0.12, r * 0.55, 8), setting, loc=(x + math.cos(a) * r * 0.98, y + math.sin(a) * r * 0.98, z + r * 0.55), shade="smooth")


# ---------------------------------------------------------------------------------------------
# Clothing and gear
# ---------------------------------------------------------------------------------------------


@item("work_clothes")
def work_clothes(it: Item):
    fab = "Fabric_Navy"
    it.add(cube(40, 30, 4.6, cuts=6), fab, loc=(0, 0, 2.3), bevel=1.6, seg=5, shade="smooth", post=[wobble(0.25, 14, 1), puff(0.4)])
    it.add(cube(37, 27, 4.2, cuts=6), fab, loc=(0.5, -0.5, 6.6), rot=(0, 0, 2), bevel=1.6, seg=5, shade="smooth", post=[wobble(0.3, 12, 2), puff(0.6)])
    # Breast pocket with flap, buttons and placket of the folded jacket.
    it.add(cube(9, 8, 0.5), fab, loc=(-8, -3, 8.85), rot=(0, 0, 2), bevel=0.2, seg=2, shade="smooth")
    it.add(cube(9.4, 3, 0.5), fab, loc=(-8, -6.2, 9.25), rot=(0, 0, 2), bevel=0.2, seg=2, shade="smooth")
    for k, x in enumerate((-12, 0, 12)):
        it.add(cylinder(0.75, 0.35, 16), "Plastic_Black", loc=(x + 6, 9.5 + k * 0.2, 8.6), shade="auto")
    it.add(cube(36, 0.5, 0.25), "Plastic_Black", loc=(0.8, 8.2, 8.75), rot=(0, 0, 2))


@item("beanie")
def beanie(it: Item):
    prof = [(0, 0.3), (9.6, 0.0), (10.4, 0.4), (10.5, 2.5), (10.4, 4.6), (10.0, 5.2), (9.9, 7.5), (9.0, 10.0), (7.2, 12.0),
            (4.6, 13.3), (2.0, 13.9), (0, 14.0)]
    it.add(lathe(prof, 64, ridges=(64, 0.35, 1, 4)), "Fabric_Black", shade="smooth", post=wobble(0.2, 10, 3), grain="z")
    it.add(lathe([(0, 13.0), (1.2, 13.2), (1.7, 14.2), (1.2, 15.0), (0, 15.2)], 20), "Fabric_Black", loc=(0, 0, -1.4), shade="smooth")


@item("tool_vest")
def tool_vest(it: Item):
    fab = "Fabric_Olive"
    it.add(cube(40, 35, 2.4, cuts=4), fab, loc=(0, 0, 1.2), bevel=0.9, seg=4, shade="smooth", post=wobble(0.2, 16, 4))
    for sx in (-1, 1):
        it.add(cube(18.6, 31, 1.8, cuts=4), fab, loc=(sx * 9.6, -1, 3.2), bevel=0.7, seg=4, shade="smooth", post=wobble(0.2, 12, 5 + sx))
        for py in (-6, 6):
            it.add(cube(7.5, 7, 2.0), fab, loc=(sx * 9.6, py - 2, 5.0), bevel=0.6, seg=3, shade="smooth", post=puff(0.4))
            it.add(cube(7.9, 2.6, 0.6), fab, loc=(sx * 9.6, py + 1.6, 6.2), bevel=0.25, seg=2, shade="smooth")
            it.add(cube(1.2, 0.6, 0.3), "Plastic_Black", loc=(sx * 9.6, py + 0.6, 6.55))  # snap
    it.add(cube(0.9, 31, 0.4), "Plastic_Black", loc=(0, -1, 4.1))  # zipper
    it.add(cube(1.4, 2.2, 0.7), "Steel", loc=(0, -10, 4.4), bevel=0.15, seg=2)  # zipper pull
    it.add(cube(26, 7, 0.4), "Plastic_Black", loc=(0, 15, 2.55), bevel=0.3, seg=2)  # neck opening


@item("backpack")
def backpack(it: Item):
    fab = "Fabric_Olive"
    it.add(cube(32, 20, 44, cuts=5), fab, loc=(0, 0, 22.5), bevel=6.0, seg=6, shade="smooth", post=[puff(1.5, 1), wobble(0.25, 16, 6)])
    it.add(cube(24, 6, 20, cuts=4), fab, loc=(0, -11, 14), bevel=2.6, seg=5, shade="smooth", post=puff(0.8, 1))
    it.add(cube(31, 22, 7, cuts=4), fab, loc=(0, 0.5, 45.5), rot=(-4, 0, 0), bevel=3.0, seg=5, shade="smooth", post=puff(0.8))
    for sx in (-1, 1):
        # Lid straps with buckles down the front.
        it.add(sweep(spline([(sx * 8, -6, 48.5), (sx * 8, -11.2, 44), (sx * 8, -10.7, 36), (sx * 8, -10.4, 30)], 5),
                     rect_profile(0.35, 2.6), up=(1, 0, 0)), "Fabric_Black", shade="smooth")
        it.add(cube(3.4, 1.2, 2.6), "Plastic_Black", loc=(sx * 8, -11.0, 33), bevel=0.3, seg=2)
        # Shoulder straps on the back.
        it.add(sweep(spline([(sx * 7, 9, 44), (sx * 7.5, 13.5, 38), (sx * 8, 14, 22), (sx * 9, 12, 8), (sx * 10, 9.5, 4)], 5),
                     rect_profile(1.4, 5.5, 0.5), up=(1, 0, 0)), "Fabric_Black", shade="smooth")
    it.add(sweep(spline([(-3, 6, 49), (0, 6.5, 52.5), (3, 6, 49)], 5), circle_profile(0.6, 8, sy=0.5)), "Fabric_Black", shade="smooth")
    it.add(cube(0.5, 0.5, 18), "Plastic_Black", loc=(0, -14.1, 14))  # zipper of the front pocket
    it.add(cube(1.2, 0.8, 2.2), "Steel", loc=(0, -14.4, 22.5), bevel=0.15, seg=2)


@item("coat")
def coat(it: Item):
    wool = "Wool_Brown"
    it.add(cube(48, 34, 6, cuts=6), wool, loc=(0, 0, 3), bevel=2.2, seg=5, shade="smooth", post=[wobble(0.35, 14, 7), puff(0.5)])
    it.add(cube(30, 7, 2.6, cuts=3), wool, loc=(0, 13, 7.0), bevel=1.0, seg=4, shade="smooth", post=wobble(0.2, 8, 8))  # collar
    for sx in (-1, 1):
        it.add(ngon_prism([(0, 0), (sx * 9, -1), (sx * 4, -16)], 0.9), wool, loc=(sx * 2, 11, 6.0), bevel=0.35, seg=3, shade="smooth")
    for y in (2, -6, -14):
        it.add(lathe([(0, 0), (1.1, 0), (1.2, 0.3), (0.9, 0.55), (0, 0.5)], 20), "Leather_Brown", loc=(5, y, 6.0), shade="smooth")
    it.add(cube(10, 0.6, 0.3), wool, loc=(-12, -8, 6.15), bevel=0.1, seg=1)  # pocket welt


# ---------------------------------------------------------------------------------------------
# Tools
# ---------------------------------------------------------------------------------------------


@item("lockpicks")
def lockpicks(it: Item):
    it.add(cube(18, 10, 0.8), "Leather_Black", loc=(0, 0, 0.4), bevel=0.3, seg=3, shade="smooth")
    it.add(cube(17, 9, 0.12), "Fabric_Black", loc=(0, 0, 0.84))  # lining
    it.add(cube(0.3, 9.6, 0.12), "Leather_Brown", loc=(0, 0, 0.86))  # fold stitch
    it.add(cube(16, 1.0, 0.35), "Fabric_Black", loc=(0, 2.2, 1.0), bevel=0.1, seg=1)  # elastic band
    tips = ["hook", "rake", "diamond", "ball", "hook", "rake", "wrench"]
    for k, y in enumerate((-3.6, -2.4, -1.2, 0.0, 1.2, 2.4, 3.6)):
        it.add(cube(4.2, 0.75, 0.35), "Plastic_Black", loc=(-5.0, y, 1.1), bevel=0.15, seg=2)
        it.add(cube(9.0, 0.16, 0.08), "Steel", loc=(1.6, y, 1.05))
        tip = tips[k]
        if tip == "hook":
            it.add(cube(0.16, 0.6, 0.08), "Steel", loc=(6.0, y + 0.25, 1.05))
        elif tip == "rake":
            for j in range(3):
                it.add(cube(0.25, 0.3, 0.08), "Steel", loc=(4.6 + j * 0.6, y + 0.18, 1.05), rot=(0, 0, 30))
        elif tip == "diamond":
            it.add(cube(0.45, 0.45, 0.08), "Steel", loc=(5.9, y, 1.05), rot=(0, 0, 45))
        elif tip == "ball":
            it.add(sphere(0.25, 10, 5), "Steel", loc=(6.0, y, 0.95), shade="smooth")
        else:
            it.add(cube(0.2, 1.1, 0.12), "Steel", loc=(6.0, y - 0.45, 1.05))
    it.add(cylinder(0.6, 0.35, 16), "Chrome", loc=(8.2, 0, 0.8), shade="auto")  # snap


@item("stethoscope")
def stethoscope(it: Item):
    tube = spline([(3.5, -1.5, 0.9), (0, -4.0, 0.6), (-5.5, -3.5, 0.6), (-7.0, 0.5, 0.6), (-4.5, 4.5, 0.6), (0.5, 4.2, 0.6),
                   (3.0, 2.0, 0.6), (5.0, 3.5, 0.6)], 6)
    it.add(sweep(tube, circle_profile(0.45, 10)), "Rubber_Black", shade="smooth")
    # Chest piece: diaphragm and bell.
    it.add(lathe([(0, 0), (2.4, 0), (2.5, 0.3), (2.4, 0.9), (1.2, 1.2), (0.6, 1.25), (0, 1.25)], 32), "Chrome", loc=(4.5, -1.6, 0), shade="auto")
    it.add(cylinder(2.2, 0.08, 32), "Plastic_Black", loc=(4.5, -1.6, 1.22))
    it.add(sweep([(4.5, -1.6, 0.8), (3.5, -1.5, 0.9)], circle_profile(0.35, 10)), "Chrome", shade="smooth")
    # Binaurals with ear tips.
    for sx, end in ((1, (7.5, 5.2, 0.6)), (-1, (6.0, 6.0, 0.6))):
        mid = ((5.0 + end[0]) / 2 + sx * 0.4, (3.5 + end[1]) / 2, 0.6)
        it.add(sweep(spline([(5.0, 3.5, 0.6), mid, end], 5), circle_profile(0.22, 8)), "Chrome", shade="smooth")
        it.add(sphere(0.55, 12, 6), "Rubber_Black", loc=(end[0], end[1], 0.05), shade="smooth")


@item("flashlight")
def flashlight(it: Item):
    body = [(0, 0), (1.6, 0), (1.75, 0.3), (1.75, 2.8), (1.55, 3.1), (1.5, 3.4), (1.5, 15.5), (1.6, 16.2), (2.6, 19.0), (3.2, 20.5),
            (3.3, 22.0), (3.2, 22.4), (0, 22.4)]
    it.add(lathe(body, 48, ridges=(48, 0.12, 2, 3)), "Iron_Dark", shade="auto", angle=40, grain="z")
    it.add(lathe([(1.52, 0), (1.62, 0), (1.62, 9), (1.52, 9)], 64, ridges=(64, 0.09, 0, 3)), "Iron_Dark", loc=(0, 0, 5), shade="auto", angle=60)
    it.add(lathe([(0, 0), (3.35, 0), (3.4, 1.6), (3.0, 2.0), (2.7, 1.9), (0, 1.9)], 48, ridges=(16, 0.15, 1, 2)), "Chrome", loc=(0, 0, 22.2), shade="auto")
    it.add(cylinder(2.7, 0.15, 40), "Glass", loc=(0, 0, 23.9), shade="smooth")
    it.add(lathe([(0, 0), (2.2, 0), (0.6, 0.9), (0, 1.0)], 24), "Chrome", loc=(0, 0, 23.0), shade="smooth")  # reflector
    it.add(cube(1.0, 0.6, 1.6), "Rubber_Black", loc=(0, -1.6, 17.0), bevel=0.25, seg=3, shade="smooth")  # switch
    it.add(torus(1.4, 0.18, 24, 8), "Iron_Dark", loc=(0, 0, 0.9), rot=(90, 0, 0), shade="smooth")  # lanyard ring


@item("crowbar")
def crowbar(it: Item):
    hexp = circle_profile(1.15, 6, phase=math.pi / 6)
    r = 1.2  # centreline height: lies on its flats
    chisel = [(-35.5, 0, 0.6), (-33, 0, 0.9), (-30, 0, r)]
    shaft = [(-30, 0, r), (-10, 0, r), (10, 0, r), (25, 0, r)]
    hook = [(25, 0, r)] + arc_points((25, 0, r + 2.6), 2.6, -90, 150, 10, plane="xz")
    it.add(sweep(chisel, hexp, up=(0, 1, 0), scales=[0.55, 0.85, 1.0]), "Steel", grain="x")
    it.add(sweep(shaft, hexp, up=(0, 1, 0)), "Paint_Red", grain="x")
    it.add(sweep(hook[:-3], hexp, up=(0, 1, 0)), "Paint_Red", grain="x")
    it.add(sweep(hook[-4:], hexp, up=(0, 1, 0), scales=[1.0, 0.9, 0.75, 0.55]), "Steel", grain="x")
    end = hook[-1]
    it.add(cube(1.0, 0.35, 1.0), "Plastic_Black", loc=(end[0] - 0.3, 0, end[2] - 0.2), rot=(0, 30, 0))  # nail slot


# ---------------------------------------------------------------------------------------------
# Loot
# ---------------------------------------------------------------------------------------------


@item("necklace")
def necklace(it: Item):
    n = 64
    for i in range(n):
        a = 2 * math.pi * i / n
        x, y = 6.3 * math.cos(a), 5.6 * math.sin(a) + 1.0
        yaw = math.degrees(math.atan2(5.6 * math.cos(a), -6.3 * math.sin(a)))
        upright = i % 2 == 1
        it.add(torus(0.34, 0.085, 12, 6), "Gold", loc=(x, y, 0.42 if upright else 0.1), rot=(90 if upright else 0, 0, yaw),
               shade="smooth", post=squash(0, 1.45))
    # Pendant at the front.
    it.add(lathe([(0, 0), (1.3, 0), (1.45, 0.25), (1.3, 0.45), (0, 0.4)], 32), "Gold", loc=(0, -5.9, 0), shade="smooth", post=squash(1, 1.3))
    it.add(gem(1.1, 0.45, 0.4, 16), "Gem", loc=(0, -5.9, 0.75), shade="flat", post=squash(1, 1.3))
    it.add(torus(0.35, 0.09, 12, 6), "Gold", loc=(0, -4.3, 0.42), rot=(90, 0, 90), shade="smooth")


@item("ring")
def ring(it: Item):
    it.add(torus(3.0, 0.45, 64, 14), "Gold", loc=(0, 0, 0.45), shade="smooth", post=squash(2, 0.9))
    gem_on(it, 1.4, (2.9, 0, 0.75), setting="Silver", prongs=6)


@item("watch")
def watch(it: Item):
    z0 = 0.35
    it.add(lathe([(0, 0), (2.0, 0), (2.25, 0.3), (2.3, 0.75), (2.05, 1.05), (0, 1.05)], 48), "Silver", loc=(0, 0, z0), shade="auto")
    it.add(cylinder(1.85, 0.1, 48), "Porcelain", loc=(0, 0, z0 + 1.0), shade="smooth")  # dial
    for k in range(12):
        a = 2 * math.pi * k / 12
        it.add(cube(0.1, 0.32 if k % 3 else 0.5, 0.05), "Iron_Dark", loc=(1.5 * math.sin(a), 1.5 * math.cos(a), z0 + 1.12), rot=(0, 0, -math.degrees(a)))
    it.add(cube(0.08, 1.05, 0.04), "Iron_Dark", loc=(0.3, 0.42, z0 + 1.16), rot=(0, 0, -35))
    it.add(cube(0.07, 1.4, 0.04), "Iron_Dark", loc=(-0.45, 0.5, z0 + 1.19), rot=(0, 0, 40))
    it.add(cylinder(1.95, 0.15, 48, r_top=1.8), "Glass", loc=(0, 0, z0 + 1.05), shade="smooth")
    it.add(lathe([(0, 0), (0.35, 0), (0.35, 0.5), (0, 0.5)], 16, ridges=(16, 0.05, 1, 2)), "Silver", loc=(2.25, 0, z0 + 0.5), rot=(0, 90, 0))
    for sy in (-1, 1):
        for lx in (-1, 1):
            it.add(cube(0.35, 0.8, 0.45), "Silver", loc=(lx * 0.85, sy * 2.2, z0 + 0.45), bevel=0.1, seg=2)
        path = [(0, sy * 2.4, z0 + 0.4), (0, sy * 3.4, 0.25), (0, sy * 4.6, 0.18)]
        it.add(sweep(spline(path, 4), rect_profile(0.3, 1.75, 0.08), up=(1, 0, 0)), "Leather_Black", shade="smooth")
    it.add(cube(2.0, 0.6, 0.25), "Silver", loc=(0, 4.55, 0.25), bevel=0.08, seg=1)  # buckle


@item("cash")
def cash(it: Item):
    it.add(cube(15.6, 7.6, 3.3), "Banknote", loc=(0, 0, 1.65), bevel=0.12, seg=1, fit="z")
    it.add(cube(3.2, 7.8, 3.5), "Paper", loc=(-2.5, 0, 1.65), bevel=0.06, seg=1)  # paper band
    it.add(cube(15.6, 7.6, 0.1), "Banknote", loc=(0.6, 0.2, 3.4), rot=(0, 0, 4), fit="z")
    it.add(cube(15.6, 7.6, 0.1), "Banknote", loc=(-0.4, -0.3, 3.55), rot=(0, 0, -6), fit="z")


@item("wallet")
def wallet(it: Item):
    it.add(cube(11.6, 9, 0.9, cuts=3), "Leather_Brown", loc=(0, 0, 0.45), bevel=0.4, seg=4, shade="smooth", post=wobble(0.08, 6, 9))
    it.add(cube(11.6, 9, 0.9, cuts=3), "Leather_Brown", loc=(0, 0, 1.45), bevel=0.4, seg=4, shade="smooth", post=[wobble(0.08, 6, 10), puff(0.2)])
    it.add(cube(10.6, 7.6, 0.12), "Banknote", loc=(1.2, 0.2, 0.95), rot=(0, 0, 3), fit="z")  # notes sticking out
    it.add(cube(0.4, 8.6, 1.9), "Leather_Brown", loc=(-5.9, 0, 0.95), bevel=0.2, seg=3, shade="smooth")  # fold
    it.add(cube(10.6, 0.08, 0.06), "Leather_Black", loc=(0, -4.15, 1.92))  # stitch line


@item("laptop")
def laptop(it: Item):
    al = "Aluminium"
    it.add(cube(34, 24, 1.5), al, loc=(0, 0, 0.95), bevel=0.7, seg=4)
    it.add(cube(33.6, 23.6, 0.2), "Plastic_Black", loc=(0, 0.1, 1.75))
    it.add(cube(34, 24, 0.75), al, loc=(0, 0, 2.2), bevel=0.35, seg=3)
    it.add(cylinder(0.5, 30, 20), "Iron_Dark", loc=(-15, 11.8, 1.6), rot=(0, 90, 0), shade="smooth")  # hinge
    for sx in (-1, 1):
        for sy in (-1, 1):
            it.add(cylinder(0.7, 0.25, 16), "Rubber_Black", loc=(sx * 14, sy * 9, 0), shade="smooth")
    for k in range(3):
        it.add(cube(0.1, 1.2, 0.45), "Plastic_Black", loc=(-17.02, -6 + k * 2, 0.95))  # ports
    it.add(cube(5.0, 3.0, 0.03), "Chrome", loc=(0, 0, 2.58), bevel=0.6, seg=4)  # badge (no brand)


@item("files")
def files(it: Item):
    cover = "Binder_Blue"
    for sy in (-1, 1):
        it.add(cube(29.5, 0.3, 32), cover, loc=(0.2, sy * 3.85, 16), bevel=0.12, seg=2, grain="z")
        it.add(cube(3, 0.35, 0.6), "Chrome", loc=(13, sy * 3.9, 0.35), bevel=0.08, seg=1)  # edge protector
    it.add(cube(0.5, 8, 32), cover, loc=(-14.6, 0, 16), bevel=0.2, seg=3, grain="z")
    it.add(cube(27.5, 7.0, 30.5), "Paper", loc=(0.8, 0, 15.8), bevel=0.15, seg=1)
    # Spine: label holder and finger hole.
    it.add(cube(0.2, 5.6, 9), "Chrome", loc=(-14.9, 0, 21), bevel=0.08, seg=1)
    it.add(cube(0.1, 5.0, 8.4), "Paper", loc=(-15.0, 0, 21), fit="x")
    it.add(torus(1.1, 0.22, 24, 8), "Chrome", loc=(-14.9, 0, 8), rot=(0, 90, 0), shade="smooth")
    it.add(cylinder(1.05, 0.12, 24), "Plastic_Black", loc=(-14.85, 0, 8), rot=(0, 90, 0))


@item("painting")
def painting(it: Item):
    # Gilt frame swept around the canvas; the canvas faces -X (towards the room in the level).
    W, H = 100, 80
    fw = 9.0
    hw, hh = (W - fw) / 2, (H - fw) / 2
    path = [(0, -hw, H / 2 - hh), (0, hw, H / 2 - hh), (0, hw, H / 2 + hh), (0, -hw, H / 2 + hh)]
    # Ornate profile (across the moulding, depth): bead, cove, raised lip.
    prof = [(-4.5, 0), (-4.5, 2.2), (-3.6, 3.2), (-2.4, 3.6), (-1.6, 2.6), (-0.6, 2.4), (0.4, 3.8), (1.6, 4.6), (2.8, 4.2),
            (3.6, 3.0), (4.0, 3.4), (4.5, 2.6), (4.5, 0)]
    prof = [(x, -y) for x, y in prof]  # relief towards -X
    it.add(sweep(path, prof, up=(1, 0, 0), closed=True), "Gilt", loc=(1.5, 0, 0), shade="auto", angle=30, grain="y")
    it.add(cube(1.2, W - 2 * fw + 2, H - 2 * fw + 2), "Canvas", loc=(-0.3, 0, H / 2), fit="x")
    it.add(cube(1.0, W - 4, H - 4), "Wood_Oak", loc=(1.0, 0, H / 2), grain="y")  # back board


@item("statue")
def statue(it: Item):
    it.add(cube(26, 26, 17), "Marble", loc=(0, 0, 8.5), bevel=0.6, seg=3)
    it.add(cube(28, 28, 2.6), "Marble", loc=(0, 0, 18.3), bevel=0.7, seg=3)
    it.add(cube(16, 16, 1.6), "Bronze", loc=(0, 0, 20.4), bevel=0.5, seg=2)
    # Stylised standing figure (lathe with an oval cross-section).
    s = 0.93
    prof = [(0, 0), (3.0, 0), (3.1, 1.0), (2.3, 6), (3.4, 13), (2.8, 19), (5.4, 29), (7.0, 35), (6.4, 39), (4.7, 43.5),
            (5.8, 48), (7.2, 51.5), (6.6, 53.5), (4.0, 55.5), (1.9, 56.8), (2.1, 58), (3.3, 60.3), (3.2, 62.4), (2.0, 64), (0, 64.6)]
    prof = [(r * s, z * s) for r, z in prof]
    it.add(lathe(prof, 48), "Bronze", loc=(0, 0, 21.2), shade="smooth", post=squash(1, 0.62), grain="z")


@item("brooch")
def brooch(it: Item):
    it.add(lathe([(0, 0), (4.3, 0), (4.5, 0.25), (4.2, 0.6), (2.5, 1.0), (0, 1.1)], 48), "Silver", shade="smooth")
    for k in range(6):
        a = 2 * math.pi * k / 6
        it.add(sphere(1.15, 16, 8, sz=0.35), "Silver", loc=(2.4 * math.cos(a), 2.4 * math.sin(a), 0.7), rot=(0, 0, math.degrees(a)),
               shade="smooth", post=squash(1, 0.55))
    for k in range(12):
        a = 2 * math.pi * k / 12 + math.pi / 12
        gem_on(it, 0.42, (3.55 * math.cos(a), 3.55 * math.sin(a), 0.45), prongs=4)
    gem_on(it, 1.5, (0, 0, 1.0), prongs=6)


@item("figurine")
def figurine(it: Item):
    it.add(lathe([(0, 0), (4.8, 0), (4.9, 0.4), (4.7, 0.9), (4.3, 1.1), (0, 1.1)], 48), "Porcelain", shade="smooth")
    it.add(torus(4.75, 0.09, 64, 6), "Gilt", loc=(0, 0, 0.65), shade="smooth")
    dress = [(0, 1.1), (4.2, 1.1), (4.3, 1.5), (3.9, 3.0), (3.3, 5.5), (2.6, 8.0), (2.0, 10.0), (1.5, 11.3), (1.35, 12.0),
             (1.7, 13.4), (1.9, 14.3), (1.6, 15.0), (0.55, 15.4), (0, 15.5)]
    it.add(lathe(dress, 48, ridges=(18, 0.22, 2, 6)), "Porcelain", shade="smooth", grain="z", post=squash(1, 0.85))
    it.add(torus(1.42, 0.22, 32, 8), "Porcelain_Blue", loc=(0, 0, 12.0), shade="smooth", post=squash(1, 0.85))  # sash
    it.add(torus(3.95, 0.12, 48, 6), "Porcelain_Blue", loc=(0, 0, 2.6), shade="smooth", post=squash(1, 0.85))  # hem band
    it.add(lathe([(0, 0), (0.55, 0), (0.6, 0.7), (1.25, 1.4), (1.35, 2.3), (1.0, 3.1), (0.4, 3.45), (0, 3.5)], 32), "Porcelain",
           loc=(0, 0, 15.3), shade="smooth")  # neck and head
    it.add(lathe([(0, 0), (0.75, 0), (0.95, 0.5), (0.6, 1.0), (0, 1.05)], 20), "Iron_Dark", loc=(0, 0.6, 18.2), shade="smooth")  # hair bun
    it.add(cylinder(2.3, 0.18, 40), "Porcelain_Blue", loc=(0, -0.2, 18.5), rot=(-12, 0, 0), shade="smooth")  # hat brim
    it.add(lathe([(0, 0), (1.05, 0), (1.0, 0.9), (0, 1.0)], 24), "Porcelain_Blue", loc=(0, -0.3, 18.6), rot=(-12, 0, 0), shade="smooth")
    for sx in (-1, 1):
        it.add(sweep(spline([(sx * 1.8, 0, 14.2), (sx * 2.2, -0.4, 12.8), (sx * 1.2, -1.6, 11.6), (sx * 0.2, -1.5, 11.4)], 5),
                     circle_profile(0.38, 10)), "Porcelain", shade="smooth")


@item("jewelry_box")
def jewelry_box(it: Item):
    wood = "Wood_Mahogany"
    it.add(cube(19.6, 14.6, 6.4), wood, loc=(0, 0, 4.0), bevel=0.35, seg=3)
    it.add(cube(19.0, 14.0, 0.3), "Plastic_Black", loc=(0, 0, 7.3))
    it.add(cube(20.2, 15.2, 2.6), wood, loc=(0, 0, 8.7), bevel=0.8, seg=4, profile=0.7)
    it.add(cube(13.5, 8.5, 0.15), "Wood_Walnut", loc=(0, 0, 10.05), bevel=0.05, seg=1)  # lid inlay
    it.add(cube(14.3, 9.3, 0.08), "Brass", loc=(0, 0, 10.0))
    for sx in (-1, 1):
        for sy in (-1, 1):
            it.add(sphere(0.55, 12, 6), "Brass", loc=(sx * 8.6, sy * 6.1, 0), shade="smooth")
            it.add(cube(1.4, 1.4, 2.7), "Brass", loc=(sx * 9.55, sy * 7.05, 8.7), bevel=0.3, seg=2)  # lid corners
    escutcheon(it, (0, -7.35, 5.6), "-y", size=0.7)
    it.add(cube(2.2, 0.5, 1.4), "Brass", loc=(0, -7.65, 8.0), bevel=0.15, seg=2)  # hasp
