"""Item models. Each builder gets an Item (dc_mesh) and adds its parts.

Sizes follow the item catalog (DCItemTypes.cpp, ShapeSize X/Y/Z in cm) so footprints and labels
stay the same in the game. Front = -Y.
"""

import math

from dc_mesh import (Item, arc_points, circle_profile, cube, cylinder, gem, lathe, ngon_prism, puff,
                     rect_profile, sphere, sweep, tapered_box, torus, wobble)

BUILDERS = {}


def item(item_id):
    def register(fn):
        BUILDERS[item_id] = fn
        return fn

    return register


# ---------------------------------------------------------------------------------------------
# Shared details
# ---------------------------------------------------------------------------------------------


def knob(it, mat, loc, facing="-y", scale=1.0):
    """Turned knob pointing along `facing`."""
    s = scale
    bm = lathe([(0, 0), (0.9 * s, 0), (0.95 * s, 0.25 * s), (0.45 * s, 0.6 * s), (0.42 * s, 1.1 * s), (1.15 * s, 1.6 * s),
                (1.2 * s, 2.0 * s), (0.9 * s, 2.45 * s), (0, 2.6 * s)], 24)
    rot = {"-y": (90, 0, 0), "+y": (-90, 0, 0), "+z": (0, 0, 0), "-x": (0, -90, 0), "+x": (0, 90, 0)}[facing]
    it.add(bm, mat, loc=loc, rot=rot, shade="smooth")


def escutcheon(it, loc, facing="-y", plate="Brass", size=1.0):
    """Keyhole plate with a dark keyhole."""
    rot = (90, 0, 0) if facing == "-y" else (-90, 0, 0)
    it.add(cylinder(1.1 * size, 0.25, 24), plate, loc=loc, rot=rot, shade="smooth")
    d = -1 if facing == "-y" else 1
    x, y, z = loc
    it.add(cube(0.3 * size, 0.1, 0.9 * size), "Plastic_Black", loc=(x, y + d * 0.27, z - 0.15 * size))
    it.add(cylinder(0.28 * size, 0.1, 12), "Plastic_Black", loc=(x, y + d * 0.22, z + 0.2 * size), rot=rot)


def bar_handle(it, mat, loc, length, facing="-y", vertical=True):
    """Bar pull on two standoffs."""
    x, y, z = loc
    d = -1 if facing == "-y" else 1
    for k in (-1, 1):
        off = (0, 0, k * length * 0.4) if vertical else (k * length * 0.4, 0, 0)
        it.add(cylinder(0.45, 2.2, 16), mat, loc=(x + off[0], y, z + off[2]), rot=(90 * -d, 0, 0), shade="smooth")
    bar_rot = (0, 0, 0) if vertical else (0, 90, 0)
    bar_loc = (x, y + d * 2.2, z - length / 2) if vertical else (x - length / 2, y + d * 2.2, z)
    it.add(lathe([(0, 0), (0.45, 0), (0.55, 0.3), (0.55, length - 0.3), (0.45, length), (0, length)], 16), mat,
           loc=bar_loc, rot=bar_rot, shade="smooth")


def turned_leg(height, top_r=2.6, scale=1.0):
    """Classic turned table leg profile, z 0..height."""
    h = height
    s = scale
    prof = [(0, 0), (2.3 * s, 0), (2.7 * s, 0.6), (2.7 * s, 1.6), (2.1 * s, 2.6), (1.9 * s, 0.18 * h), (2.6 * s, 0.33 * h),
            (2.9 * s, 0.42 * h), (2.5 * s, 0.48 * h), (2.0 * s, 0.53 * h), (2.6 * s, 0.6 * h), (2.9 * s, 0.63 * h),
            (2.5 * s, 0.66 * h), (top_r, 0.68 * h), (top_r, h), (0, h)]
    return lathe(prof, 24)


def upholstery(it, mat, size, loc, round_cm, puff_cm=0.0, puff_axis=2, rot=(0, 0, 0), cuts=6):
    sx, sy, sz = size
    post = [puff(puff_cm, puff_axis)] if puff_cm else None
    it.add(cube(sx, sy, sz, cuts=cuts), mat, loc=loc, rot=rot, bevel=round_cm, seg=6, shade="smooth", post=post)


# ---------------------------------------------------------------------------------------------
# Furniture
# ---------------------------------------------------------------------------------------------


@item("desk")
def desk(it: Item):
    W, D, H = 120, 60, 75
    wood = "Wood_Walnut"
    it.add(cube(W, D, 3.5), wood, loc=(0, 0, H - 1.75), bevel=0.8, seg=3)
    # Leather writing surface with a thin gilt edge.
    it.add(cube(96, 40, 0.2), "Leather_Green", loc=(0, 2, H + 0.1), bevel=0.08, seg=1)
    lx, ly = W / 2 - 5, D / 2 - 5
    for sx in (-1, 1):
        for sy in (-1, 1):
            it.add(tapered_box(5.5, 5.5, 3.6, 3.6, H - 3.5), wood, loc=(sx * lx, sy * ly, 0), bevel=0.35, seg=2, grain="z")
            it.add(cylinder(2.0, 1.2, 16), "Brass", loc=(sx * lx, sy * ly, 0), shade="smooth")  # brass shoe
    apron_z = H - 3.5 - 5.5
    front_y = -ly + 0.9
    # Side and back aprons.
    for sx in (-1, 1):
        it.add(cube(2, 2 * ly - 5, 11), wood, loc=(sx * (lx - 0.9), 0, apron_z), bevel=0.25, seg=2, grain="y")
    it.add(cube(2 * lx - 5, 2, 11), wood, loc=(0, ly - 0.9, apron_z), bevel=0.25, seg=2)
    # Front apron in three pieces around the drawer.
    for sx in (-1, 1):
        it.add(cube(lx - 30 - 2.75, 2, 11), wood, loc=(sx * (30 + (lx - 30 - 2.75) / 2 - 1.4), front_y, apron_z), bevel=0.25, seg=2)
    it.add(cube(60, 2, 0.9), wood, loc=(0, front_y, apron_z - 5.05), bevel=0.2, seg=1)
    it.add(cube(60, 1.2, 10), "Plastic_Black", loc=(0, front_y + 0.6, apron_z + 0.4))  # dark drawer pocket
    # Drawer front with cock-bead moulding, pulls and lock.
    dfy = front_y - 1.0
    it.add(cube(58.4, 1.8, 9.4), wood, loc=(0, dfy + 0.1, apron_z + 0.5), bevel=0.45, seg=3)
    it.add(cube(55, 0.6, 6.6), wood, loc=(0, dfy - 1.0, apron_z + 0.5), bevel=0.5, seg=3, profile=0.3)
    for sx in (-1, 1):
        knob(it, "Brass", (sx * 17, dfy - 1.1, apron_z + 0.5), "-y", 0.9)
    escutcheon(it, (0, dfy - 1.35, apron_z + 1.2), "-y")


@item("chair")
def chair(it: Item):
    wood = "Wood_Oak"
    seat_z = 45
    it.add(cube(42, 41, 3.2), wood, loc=(0, -1, seat_z), bevel=0.7, seg=3)
    # Upholstered seat pad.
    upholstery(it, "Leather_Brown", (37, 35, 3.5), (0, -2, seat_z + 2.8), 1.4, puff_cm=0.8)
    for sx in (-1, 1):
        it.add(tapered_box(3.6, 3.6, 2.8, 2.8, seat_z - 1.6), wood, loc=(sx * 18.5, -18.5, 0), bevel=0.3, seg=2, grain="z")
        # Rear leg runs up into the backrest with a slight backward rake.
        path = [(sx * 18.5, 18.0, 0), (sx * 18.5, 17.0, seat_z - 2), (sx * 18.5, 19.5, 70), (sx * 18.5, 22.0, 90)]
        it.add(sweep(path, rect_profile(3.4, 3.4, 0.5), up=(1, 0, 0)), wood, grain="z")
        # Side stretchers.
        it.add(sweep([(sx * 18.5, -18.5, 14), (sx * 18.5, 17.6, 14)], circle_profile(1.0, 12), up=(0, 0, 1)), wood, grain="y")
    it.add(sweep([(-18.5, -18.5, 20), (18.5, -18.5, 20)], circle_profile(1.0, 12)), wood)
    # Curved top rail and slats.
    rail = [(x, 21.2 + 2.2 * (1 - (x / 18.5) ** 2), 84) for x in [-18.5 + 37 * i / 12 for i in range(13)]]
    it.add(sweep(rail, rect_profile(2.2, 8, 0.6), up=(0, 0, 1)), wood)
    for x in (-8, 0, 8):
        y_top = 21.2 + 2.2 * (1 - (x / 18.5) ** 2)
        it.add(sweep([(x, 17.8, seat_z + 1.6), (x, y_top, 81)], rect_profile(1.2, 4.2, 0.3), up=(1, 0, 0)), wood, grain="z")


@item("dining_table")
def dining_table(it: Item):
    W, D, H = 160, 90, 75
    wood = "Wood_Oak"
    it.add(cube(W, D, 4), wood, loc=(0, 0, H - 2), bevel=1.3, seg=4)
    it.add(cube(W - 6, D - 6, 1.5), wood, loc=(0, 0, H - 4.6), bevel=0.4, seg=2)  # under-lip
    lx, ly = W / 2 - 9, D / 2 - 9
    leg_h = H - 5.3
    for sx in (-1, 1):
        for sy in (-1, 1):
            it.add(turned_leg(leg_h - 11, top_r=2.9, scale=1.1), wood, loc=(sx * lx, sy * ly, 0), grain="z")
            it.add(cube(6.4, 6.4, 11), wood, loc=(sx * lx, sy * ly, leg_h - 5.5), bevel=0.4, seg=2, grain="z")
    apron_z = leg_h - 5.5
    for sy in (-1, 1):
        it.add(cube(2 * lx - 6, 2.4, 10), wood, loc=(0, sy * (ly - 1.0), apron_z), bevel=0.3, seg=2)
    for sx in (-1, 1):
        it.add(cube(2.4, 2 * ly - 6, 10), wood, loc=(sx * (lx - 1.0), 0, apron_z), bevel=0.3, seg=2, grain="y")


@item("safe")
def safe(it: Item):
    paint, chrome = "Paint_Safe", "Chrome"
    it.add(cube(58, 56, 72), paint, loc=(0, 1, 40), bevel=3.0, seg=6)
    for sx in (-1, 1):
        for sy in (-1, 1):
            it.add(cube(9, 9, 4), "Iron_Dark", loc=(sx * 23, sy * 22 + 1, 2), bevel=0.6, seg=2)
    fy = -27.0
    it.add(cube(49.2, 0.6, 61.2), "Plastic_Black", loc=(0, fy - 0.2, 41))  # door gap
    it.add(cube(48, 2.2, 60), paint, loc=(0, fy - 1.2, 41), bevel=1.2, seg=4)
    dy = fy - 2.3
    # Raised inner panel.
    it.add(cube(40, 0.8, 52), paint, loc=(0, dy - 0.3, 41), bevel=0.8, seg=3, profile=0.3)
    dy -= 0.7
    # Combination dial: chrome ring with ticks, dark knurled knob.
    dial = (0, dy, 55)
    it.add(lathe([(0, 0), (5.8, 0), (5.8, 0.6), (5.4, 1.0), (0, 1.0)], 48), chrome, loc=dial, rot=(90, 0, 0), shade="auto")
    for i in range(50):
        a = 2 * math.pi * i / 50
        ln = 0.9 if i % 5 == 0 else 0.5
        r = 5.0 - ln / 2
        it.add(cube(0.14, 0.12, ln), "Plastic_Black", loc=(r * math.sin(a), dy - 1.05, 55 + r * math.cos(a)), rot=(0, math.degrees(a), 0))
    it.add(lathe([(0, 0), (3.6, 0), (3.6, 1.8), (3.2, 2.4), (1.2, 2.6), (0, 2.6)], 60, ridges=(40, 0.25, 1, 2)), "Iron_Dark",
           loc=(0, dy - 1.0, 55), rot=(90, 0, 0), shade="auto", angle=50)
    it.add(cube(0.3, 0.2, 1.2), "Brass", loc=(0, dy - 1.0, 61.2))  # index mark
    # Three-spoke handle.
    hub = (0, dy, 34)
    it.add(lathe([(0, 0), (2.6, 0), (2.6, 0.8), (1.8, 1.4), (1.6, 3.6), (2.0, 4.2), (0, 4.4)], 32), chrome, loc=hub, rot=(90, 0, 0), shade="auto")
    for k in range(3):
        a = math.radians(90 + 120 * k)
        end = (9.5 * math.cos(a), dy - 3.0, 34 + 9.5 * math.sin(a))
        it.add(sweep([(0, dy - 3.0, 34), end], circle_profile(0.75, 14)), chrome, shade="smooth")
        it.add(sphere(1.5, 20, 10), chrome, loc=(end[0], end[1], end[2] - 1.5), shade="smooth")
    # Hinges and rivets.
    for z in (20, 62):
        it.add(lathe([(0, 0), (1.5, 0), (1.5, 9), (0.8, 9.6), (0, 9.6)], 20), "Iron_Dark", loc=(-24.6, fy - 1.0, z - 4.8), shade="auto")
    for sx in (-1, 1):
        for z in (14, 68):
            it.add(sphere(0.8, 12, 6, sz=0.6), chrome, loc=(sx * 21.5, dy + 0.6, z), rot=(90, 0, 0), shade="smooth")
    # Blank brass plate.
    it.add(cube(16, 0.4, 3.4), "Brass", loc=(0, dy + 0.1, 66), bevel=0.3, seg=2)
    it.add(cube(14.5, 0.1, 2.2), "Iron_Dark", loc=(0, dy - 0.15, 66), bevel=0.05, seg=1)


@item("wardrobe")
def wardrobe(it: Item):
    W, D, H = 100, 55, 190
    wood = "Wood_Mahogany"
    it.add(cube(96, 51, 8), wood, loc=(0, 0.5, 4), bevel=0.6, seg=2)
    it.add(cube(98, 53, 170), wood, loc=(0, 0.5, 93), bevel=0.5, seg=2, grain="z")
    it.add(cube(100, 55, 3), wood, loc=(0, 0, 179.5), bevel=0.8, seg=3)
    it.add(cube(102, 57, 4), wood, loc=(0, 0, 183), bevel=1.6, seg=5, profile=0.7)
    it.add(cube(101, 56, 5), wood, loc=(0, 0, 187.5), bevel=0.7, seg=3)
    # Plinth moulding.
    it.add(cube(99, 54, 2), wood, loc=(0, 0.5, 9), bevel=0.8, seg=3)
    front = -26.0
    it.add(cube(97, 0.5, 161), "Plastic_Black", loc=(0, front - 0.2, 93))
    dw = 48.3
    for sx in (-1, 1):
        cx = sx * (dw / 2 + 0.2)
        it.add(cube(dw, 2, 160), wood, loc=(cx, front - 1.2, 93), bevel=0.4, seg=2, grain="z")
        # Raised panels (upper and lower) with a moulded frame.
        for pz, ph in ((128, 72), (52, 58)):
            it.add(cube(dw - 13, 0.6, ph + 3), "Plastic_Black", loc=(cx, front - 2.3, pz))
            it.add(cube(dw - 14, 1.6, ph), wood, loc=(cx, front - 2.6, pz), bevel=1.6, seg=3, profile=0.25, grain="z")
            it.add(cube(dw - 20, 0.6, ph - 6), wood, loc=(cx, front - 3.5, pz), bevel=0.4, seg=2, grain="z")
        bar_handle(it, "Brass", (sx * 4.5, front - 2.2, 100), 16, "-y")
    escutcheon(it, (4.5, front - 2.3, 88), "-y")


@item("armchair")
def armchair(it: Item):
    fab = "Fabric_Red"
    for sx in (-1, 1):
        for sy in (-1, 1):
            it.add(lathe([(0, 0), (1.5, 0), (1.6, 0.4), (2.4, 9.5), (2.6, 10), (0, 10)], 20), "Wood_Walnut",
                   loc=(sx * 33, sy * 31, 0), shade="auto", grain="z")
            it.add(cylinder(1.6, 0.6, 16), "Brass", loc=(sx * 33, sy * 31, 0), shade="smooth")
    upholstery(it, fab, (78, 76, 28), (0, 0, 24), 4.0, puff_cm=0.6, puff_axis=1)
    upholstery(it, fab, (55, 58, 13), (0, -7, 44.5), 5.0, puff_cm=1.8)
    for sx in (-1, 1):
        upholstery(it, fab, (12.5, 74, 32), (sx * 32.7, 0, 52), 5.5, puff_cm=0.8, puff_axis=0)
    upholstery(it, fab, (78, 15, 52), (0, 30.5, 63), 6.0, rot=(-6, 0, 0))
    upholstery(it, fab, (53, 12, 36), (0, 19.5, 63), 5.0, puff_cm=2.2, puff_axis=1, rot=(-10, 0, 0))


@item("sofa")
def sofa(it: Item):
    fab = "Fabric_Blue"
    for sx in (-1, 0.0, 1):
        for sy in (-1, 1):
            it.add(lathe([(0, 0), (1.5, 0), (1.6, 0.4), (2.4, 9.5), (2.6, 10), (0, 10)], 20), "Wood_Walnut",
                   loc=(sx * 92, sy * 35, 0), shade="auto", grain="z")
    upholstery(it, fab, (196, 82, 26), (0, 0, 23), 4.0, puff_cm=0.5, puff_axis=1)
    for i in (-1, 0, 1):
        upholstery(it, fab, (52, 58, 13), (i * 53, -7, 42.5), 5.0, puff_cm=1.6)
        upholstery(it, fab, (51, 13, 34), (i * 53, 19.5, 59), 5.0, puff_cm=2.2, puff_axis=1, rot=(-9, 0, 0))
    for sx in (-1, 1):
        upholstery(it, fab, (18, 82, 30), (sx * 89, 0, 51), 7.0, puff_cm=1.0, puff_axis=0)
    upholstery(it, fab, (196, 16, 44), (0, 33, 57), 6.0, rot=(-5, 0, 0))


@item("display_case")
def display_case(it: Item):
    wood = "Wood_Mahogany"
    it.add(cube(86, 46, 5), wood, loc=(0, 0, 2.5), bevel=0.6, seg=2)
    it.add(cube(90, 50, 34), wood, loc=(0, 0, 22), bevel=0.6, seg=2)
    it.add(cube(91, 51, 3), wood, loc=(0, 0, 40.5), bevel=1.0, seg=4, profile=0.7)
    # Cabinet doors.
    for sx in (-1, 1):
        it.add(cube(41, 0.5, 29), "Plastic_Black", loc=(sx * 21.5, -25.1, 22))
        it.add(cube(40, 1.4, 28), wood, loc=(sx * 21.5, -25.6, 22), bevel=1.0, seg=3, profile=0.3, grain="z")
        knob(it, "Brass", (sx * 4.5, -26.3, 24), "-y", 0.7)
    # Glass cabinet.
    top = 108
    for sx in (-1, 1):
        for sy in (-1, 1):
            it.add(cube(3, 3, top - 42), wood, loc=(sx * 43.5, sy * 23.5, (top + 42) / 2), bevel=0.3, seg=2, grain="z")
    it.add(cube(84, 2.2, 2.6), wood, loc=(0, -23.5, 43.3), bevel=0.3, seg=2)  # front bottom rail
    it.add(cube(84, 44, 0.5), "Velvet_Red", loc=(0, 0, 42.25), bevel=0.1, seg=1)
    for sy in (-1, 1):
        it.add(cube(84, 0.6, top - 44.6), "Glass", loc=(0, sy * 23.5, (top + 44.6) / 2), shade="flat")
    for sx in (-1, 1):
        it.add(cube(0.6, 44, top - 42), "Glass", loc=(sx * 43.5, 0, (top + 42) / 2), shade="flat")
    it.add(cube(83, 43, 0.8), "Glass", loc=(0, 0, 76), shade="flat")
    for sx in (-1, 1):
        for sy in (-1, 1):
            it.add(cylinder(0.5, 1.2, 10), "Brass", loc=(sx * 40, sy * 20, 75.0), shade="smooth")  # shelf pins
    escutcheon(it, (0, -24.65, 43.3), "-y", size=0.8)
    it.add(cube(91, 51, 4), wood, loc=(0, 0, top + 2), bevel=1.2, seg=4, profile=0.7)
    it.add(cube(88, 48, 3), wood, loc=(0, 0, top + 5.5), bevel=0.8, seg=3)


@item("piano")
def piano(it: Item):
    lac = "Lacquer_Black"
    # Main case.
    it.add(cube(150, 30, 127), lac, loc=(0, 15, 63.5), bevel=0.8, seg=3)
    it.add(cube(151, 34, 2.6), lac, loc=(0, 13, 128.7), bevel=1.0, seg=3)
    # Upper front: raised panel and music ledge.
    it.add(cube(124, 1.0, 34), lac, loc=(0, -0.4, 103), bevel=0.9, seg=3, profile=0.3)
    it.add(cube(116, 0.4, 28), "Wood_Mahogany", loc=(0, -0.95, 103), bevel=0.1, seg=1)  # inlay
    it.add(cube(126, 5, 2.2), lac, loc=(0, -2, 84.5), bevel=0.6, seg=2)
    # Fallboard (open, leaning back).
    it.add(cube(124, 1.4, 11), lac, loc=(0, -4.2, 79), rot=(-14, 0, 0), bevel=0.4, seg=2)
    # Key bed and cheeks.
    it.add(cube(150, 28, 6), lac, loc=(0, -14, 64), bevel=0.6, seg=2)
    for sx in (-1, 1):
        it.add(cube(13.4, 30, 12), lac, loc=(sx * 68.3, -15, 73), bevel=1.6, seg=4)
    it.add(cube(122.6, 2.0, 4.5), lac, loc=(0, -29, 68.2), bevel=0.5, seg=2)  # key slip
    it.add(cube(122.6, 1.2, 1.4), "Felt_Red", loc=(0, -11.6, 71.3))
    # Keys: 52 white, 36 black, A0..C8.
    n_white = 52
    kw = 122.6 / n_white
    x0 = -61.3 + kw / 2
    notes = "ABCDEFG"
    for i in range(n_white):
        x = x0 + i * kw
        it.add(cube(kw - 0.18, 15, 2.0), "Ivory", loc=(x, -20.3, 69.6), bevel=0.12, seg=1)
        note = notes[i % 7]
        if note in "ACDFG" and i < n_white - 1:
            it.add(cube(kw * 0.55, 9, 1.3), "Plastic_Black", loc=(x + kw / 2, -17.3, 71.2), bevel=0.15, seg=2)
    # Lower front panel.
    it.add(cube(124, 1.0, 40), lac, loc=(0, -0.4, 36), bevel=0.9, seg=3, profile=0.3)
    # Front legs on toe blocks.
    for sx in (-1, 1):
        it.add(cube(9, 30, 4), lac, loc=(sx * 66, -14, 2), bevel=0.8, seg=2)
        it.add(turned_leg(57, top_r=2.6, scale=0.95), lac, loc=(sx * 66, -24, 4), grain="z")
        it.add(cylinder(2.4, 1.2, 16), "Brass", loc=(sx * 66, -26, 0), shade="smooth")
    # Pedals.
    it.add(cube(26, 1.2, 9), lac, loc=(0, -0.6, 8), bevel=0.4, seg=2)
    for px in (-6.5, 0, 6.5):
        it.add(sweep([(px, -1, 7), (px, -7, 6.2), (px, -12, 6.6)], rect_profile(0.8, 2.4, 0.25), up=(1, 0, 0)), "Brass", shade="auto")
    # Lid hinge line.
    it.add(cube(140, 0.6, 0.6), "Brass", loc=(0, 5, 130.05))


@item("cardboard_box")
def cardboard_box(it: Item):
    c = "Cardboard"
    it.add(cube(55, 40, 36), c, loc=(0, 0, 18), bevel=0.35, seg=2, post=wobble(0.25, 30, 1))
    # Closed (not taped) top flaps, slightly sprung up at the seam.
    for sy in (-1, 1):
        it.add(cube(54.6, 19.8, 0.45), c, loc=(0, sy * 9.95, 36.5), rot=(sy * -2.2, 0, 0), bevel=0.15, seg=1)
    # Hand holes on the short sides.
    for sx in (-1, 1):
        it.add(cube(0.2, 10, 3), "Plastic_Black", loc=(sx * 27.55, 0, 29), bevel=0.09, seg=1)
    # Shipping label.
    it.add(cube(11, 0.12, 7.5), "Paper", loc=(8, -20.06, 21), bevel=0.04, seg=1, fit="y")


# Small items register themselves into BUILDERS.
import dc_items_small  # noqa: E402,F401
import dc_items_deco  # noqa: E402,F401


def _import_villa_modules():
    """Villa props live in dc_villa_*.py (one file per batch). A broken file must not stop the others."""
    import glob
    import importlib
    import os
    import traceback

    here = os.path.dirname(os.path.abspath(__file__))
    for path in sorted(glob.glob(os.path.join(here, "dc_villa_*.py"))):
        name = os.path.splitext(os.path.basename(path))[0]
        try:
            importlib.import_module(name)
        except Exception:  # noqa: BLE001 – report and keep going
            print(f"[items] could not import {name}:\n{traceback.format_exc()}", flush=True)


_import_villa_modules()
