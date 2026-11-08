"""Villa kitchen (G2): island, three 60-cm modules (base, sink, stove) and the fridge. Exported as SM_<id>.

Conventions: cm, origin bottom centre, front = -Y, width along X.
The 60-cm modules are lined up in the level, so they share worktop, plinth and front grid. Nothing sticks out
beyond x = -30 .. +30, and the worktop, carcass and plinth end flat (no bevel) at both sides, so neighbours join
without a groove. Only the fronts keep their small bevel – the 4 mm gap between modules is the same as between
two drawers.
"""

import math

from dc_items import item
from dc_mesh import Item, arc_points, circle_profile, cube, cylinder, lathe, ngon_prism, rect_profile, sweep

OAK = "Wood_Oak"
STEEL = "Steel"
MARBLE = "Marble"
SHADOW = "Plastic_Black"
PLINTH = "Paint_Anthracite"

# Module grid (shared by base, sink and stove)
MW = 60.0                      # module width (x -30 .. +30)
TOP_Z0, TOP_Z1 = 86.0, 90.0    # worktop
PLINTH_H = 10.0
BODY_FRONT, BODY_BACK = -25.4, 29.5
FRONT_T = 1.9
FRONT_FACE = BODY_FRONT - FRONT_T   # -27.3
GAP = 0.4
FRONT_X = MW / 2 - GAP / 2          # fronts end 2 mm inside the module edge
FRONT_Z0 = PLINTH_H + 0.3           # 10.3
FRONT_Z1 = TOP_Z0 - 0.4             # 85.6
BAND_Z0 = 70.0                      # top band: top drawer / blind panel / oven controls
LOWER_Z1 = BAND_Z0 - GAP            # 69.6


# ---------------------------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------------------------

# Face orientation: to_world(u, out, z, f) for a face plane at coordinate f, u running along the face (left to
# right when looking at it), out = distance in front of the face; rotation that points +Z outward; rotation that
# points +Z along +u.
FACES = {
    "-y": (lambda u, o, z, f: (u, f - o, z), (90, 0, 0), (0, 90, 0)),
    "+y": (lambda u, o, z, f: (-u, f + o, z), (-90, 0, 0), (0, -90, 0)),
    "+x": (lambda u, o, z, f: (f + o, u, z), (0, 90, 0), (-90, 0, 0)),
    "-x": (lambda u, o, z, f: (f - o, -u, z), (0, -90, 0), (90, 0, 0)),
}


def panel(it, face, f, u0, u1, z0, z1, t=FRONT_T, mat=OAK, grain="x", bevel=0.2):
    """Flat front panel whose back lies on the face plane f; it sticks out by t."""
    to_world = FACES[face][0]
    w, h = u1 - u0, z1 - z0
    loc = to_world((u0 + u1) / 2, t / 2, (z0 + z1) / 2, f)
    size = (w, t, h) if face in ("-y", "+y") else (t, w, h)
    it.add(cube(*size), mat, loc=loc, bevel=bevel, seg=2, grain=grain)


def bar_pull(it, face, f, cu, cz, length, vertical=False, mat=STEEL, r=0.6, standoff=1.6):
    """Stainless bar pull on two standoffs, mounted on the front face plane f."""
    to_world, out_rot, along_rot = FACES[face]
    span = length / 2 - 3.0
    for k in (-1, 1):
        u, z = (cu, cz + k * span) if vertical else (cu + k * span, cz)
        it.add(cylinder(r * 0.7, standoff + r, 16), mat, loc=to_world(u, -0.05, z, f), rot=out_rot, shade="smooth")
    prof = [(0, 0), (r * 0.6, 0), (r * 0.92, r * 0.25), (r, r * 0.6), (r, length - r * 0.6), (r * 0.92, length - r * 0.25),
            (r * 0.6, length), (0, length)]
    if vertical:
        loc, rot = to_world(cu, standoff + r, cz - length / 2, f), (0, 0, 0)
    else:
        loc, rot = to_world(cu - length / 2, standoff + r, cz, f), along_rot
    it.add(lathe(prof, 20), mat, loc=loc, rot=rot, shade="smooth", grain="z" if vertical else "x")


def drawer(it, face, f, u0, u1, z0, z1, handle=30.0, mat=OAK):
    """Oak drawer front with a bar pull near its top edge."""
    panel(it, face, f, u0, u1, z0, z1, mat=mat)
    bar_pull(it, face, f + (-FRONT_T if face[0] == "-" else FRONT_T), (u0 + u1) / 2, z1 - 3.2, handle)


def rrect(cx, cy, w, h, r, steps=5, z=None):
    """Rounded rectangle, counter-clockwise seen from above. 2D points, or 3D when z is given."""
    hw, hh = w / 2, h / 2
    pts = []
    for ccx, ccy, a0 in ((cx + hw - r, cy - hh + r, -90), (cx + hw - r, cy + hh - r, 0), (cx - hw + r, cy + hh - r, 90),
                         (cx - hw + r, cy - hh + r, 180)):
        for i in range(steps + 1):
            a = math.radians(a0 + 90 * i / steps)
            p = (ccx + r * math.cos(a), ccy + r * math.sin(a))
            pts.append(p if z is None else (p[0], p[1], z))
    return pts


def ring(it, mat, x, y, z, r_out, width, h=0.04, seg=64):
    """Flat ring (annulus) lying on z."""
    r_in = r_out - width
    it.add(lathe([(r_in, 0), (r_out, 0), (r_out, h), (r_in, h), (r_in, 0)], seg, caps=False), mat, loc=(x, y, z), shade="flat")


SEGMENTS = {"0": "abcdef", "1": "bc", "2": "abged", "3": "abgcd", "4": "fgbc", "5": "afgcd", "6": "afgedc", "7": "abc",
            "8": "abcdefg", "9": "abcdfg", "-": "g"}


def seven_seg(it, text, face_y, x0, z0, h, mat="Display_Blue"):
    """Seven-segment digits on a -Y facing display (face at face_y), text starts at x0, digit bottom at z0."""
    w, t = h * 0.55, h * 0.13
    x = x0
    for ch in text:
        if ch == ":":
            for dz in (0.3, 0.7):
                it.add(cube(t, 0.06, t), mat, loc=(x + t / 2, face_y - 0.03, z0 + dz * h))
            x += t * 3
            continue
        segs = {"a": (x + w / 2, z0 + h, True), "b": (x + w, z0 + 0.75 * h, False), "c": (x + w, z0 + 0.25 * h, False),
                "d": (x + w / 2, z0, True), "e": (x, z0 + 0.25 * h, False), "f": (x, z0 + 0.75 * h, False),
                "g": (x + w / 2, z0 + h / 2, True)}
        for s in SEGMENTS.get(ch, ""):
            sx, sz, horizontal = segs[s]
            size = (w - t, 0.06, t) if horizontal else (t, 0.06, h / 2 - t)
            it.add(cube(*size), mat, loc=(sx, face_y - 0.03, sz))
        x += w + h * 0.3


# ---------------------------------------------------------------------------------------------
# 60-cm module parts
# ---------------------------------------------------------------------------------------------


def worktop_profile(back_y):
    """Worktop section for a sweep along +X (profile x = -y, profile y = z), rounded front edge at y = -30."""
    f, r = MW / 2, 0.6
    c = r * (1 - math.sqrt(0.5))
    return [(-back_y, TOP_Z0), (f - r, TOP_Z0), (f - c, TOP_Z0 + c), (f, TOP_Z0 + r), (f, TOP_Z1 - r), (f - c, TOP_Z1 - c),
            (f - r, TOP_Z1), (-back_y, TOP_Z1)]


def module_worktop(it, hole=None):
    """Marble worktop 60 x 60 x 4, flat at x = +-30. hole = (x0, x1, y0, y1) leaves a rectangular cut-out."""
    run = [(-MW / 2, 0, 0), (MW / 2, 0, 0)]
    if hole is None:
        it.add(sweep(run, worktop_profile(MW / 2)), MARBLE, shade="auto", angle=50)
        return
    hx0, hx1, hy0, hy1 = hole
    th, zc = TOP_Z1 - TOP_Z0, (TOP_Z0 + TOP_Z1) / 2
    it.add(sweep(run, worktop_profile(hy0)), MARBLE, shade="auto", angle=50)
    it.add(cube(MW, MW / 2 - hy1, th), MARBLE, loc=(0, (hy1 + MW / 2) / 2, zc))
    it.add(cube(hx0 + MW / 2, hy1 - hy0, th), MARBLE, loc=((hx0 - MW / 2) / 2, (hy0 + hy1) / 2, zc))
    it.add(cube(MW / 2 - hx1, hy1 - hy0, th), MARBLE, loc=((hx1 + MW / 2) / 2, (hy0 + hy1) / 2, zc))


def module_body(it, open_above=None):
    """Plinth, oak carcass and the dark gap shadow behind the fronts. open_above: carcass top open from there."""
    it.add(cube(MW, 49, PLINTH_H), PLINTH, loc=(0, 3.5, PLINTH_H / 2), grain="x")  # recessed 6 cm behind the fronts
    d, cy = BODY_BACK - BODY_FRONT, (BODY_BACK + BODY_FRONT) / 2
    solid_top = TOP_Z0 if open_above is None else open_above
    it.add(cube(MW, d, solid_top - PLINTH_H), OAK, loc=(0, cy, (PLINTH_H + solid_top) / 2), grain="z")
    if open_above is not None:
        h = TOP_Z0 - open_above
        for sx in (-1, 1):
            it.add(cube(1.8, d, h), OAK, loc=(sx * (MW / 2 - 0.9), cy, open_above + h / 2), grain="z")
        for y in (BODY_FRONT + 0.9, BODY_BACK - 0.9):
            it.add(cube(MW - 3.6, 1.8, h), OAK, loc=(0, y, open_above + h / 2))
    it.add(cube(MW - 0.2, 0.1, TOP_Z0 - PLINTH_H), SHADOW, loc=(0, BODY_FRONT - 0.05, (PLINTH_H + TOP_Z0) / 2))


# ---------------------------------------------------------------------------------------------
# Modules
# ---------------------------------------------------------------------------------------------


@item("Deco_kitchen_base")
def kitchen_base(it: Item):
    """Base unit: top drawer and two deep pot drawers, oak fronts, marble worktop."""
    module_body(it)
    module_worktop(it)
    half = (LOWER_Z1 - FRONT_Z0 - GAP) / 2
    for z0, z1 in ((BAND_Z0, FRONT_Z1), (FRONT_Z0 + half + GAP, LOWER_Z1), (FRONT_Z0, FRONT_Z0 + half)):
        drawer(it, "-y", BODY_FRONT, -FRONT_X, FRONT_X, z0, z1)


@item("Deco_kitchen_sink")
def kitchen_sink(it: Item):
    """Sink unit: inset stainless bowl with drain, gooseneck mixer tap, blind panel and door below."""
    cx, cy, w, d, r = 0.0, -3.5, 46.0, 34.0, 3.5
    module_body(it, open_above=70.0)
    module_worktop(it, hole=(cx - w / 2, cx + w / 2, cy - d / 2, cy + d / 2))
    drawer(it, "-y", BODY_FRONT, -FRONT_X, FRONT_X, BAND_Z0, FRONT_Z1)        # blind panel (same look as a drawer)
    drawer(it, "-y", BODY_FRONT, -FRONT_X, FRONT_X, FRONT_Z0, LOWER_Z1)       # door

    # Bowl: one shell swept along the cut line (profile x = outward, y = up): flat rim on the worktop, walls, fillet.
    depth = 17.5
    shell = [(1.8, 0.0), (1.8, 0.15), (1.55, 0.4), (-0.2, 0.4), (-0.7, 0.2), (-0.95, -0.4), (-1.35, -depth + 0.9),
             (-1.8, -depth + 0.2), (-2.6, -depth), (-2.6, -depth - 0.6), (-1.2, -depth - 0.6), (0.0, -0.6), (0.0, 0.0)]
    it.add(sweep(rrect(cx, cy, w, d, r, steps=6, z=TOP_Z1), shell, up=(0, 0, 1), closed=True), STEEL, shade="auto",
           angle=40, grain="x")
    inset = 2.6
    floor = rrect(cx, cy, w - 2 * inset, d - 2 * inset, r - inset, steps=3)
    it.add(ngon_prism(floor, 0.6), STEEL, loc=(0, 0, TOP_Z1 - depth - 0.6), shade="auto", grain="x")
    # Drain: ring, dark gap and strainer cap; small overflow slot in the back wall.
    dz = TOP_Z1 - depth
    dx, dy = cx, cy + 6.0
    it.add(lathe([(0, 0), (4.2, 0), (4.2, 0.12), (3.7, 0.3), (0, 0.3)], 40), STEEL, loc=(dx, dy, dz), shade="auto")
    it.add(cylinder(3.3, 0.05, 40), SHADOW, loc=(dx, dy, dz + 0.3))
    it.add(lathe([(0, 0), (2.6, 0), (2.6, 0.25), (2.2, 0.45), (0, 0.5)], 32), "Chrome", loc=(dx, dy, dz + 0.3), shade="smooth")
    it.add(cube(5.0, 0.4, 1.2), SHADOW, loc=(cx, cy + d / 2 - 1.15, TOP_Z1 - 4.5), rot=(-1.5, 0, 0), bevel=0.3, seg=2)

    # Gooseneck mixer tap (chrome) behind the bowl, single lever on the right.
    tx, ty = 0.0, 21.5
    it.add(lathe([(0, 0), (2.9, 0), (2.9, 0.5), (2.4, 1.0), (1.9, 1.4), (1.9, 14), (1.6, 16.5), (1.15, 19), (1.15, 20), (0, 20)], 32),
           "Chrome", loc=(tx, ty, TOP_Z1), shade="smooth")
    arc_r, arc_z = 9.5, 116.0
    path = [(tx, ty, 109.5), (tx, ty, arc_z)] + arc_points((tx, ty - arc_r, arc_z), arc_r, 0, 180, 18, plane="yz")
    path.append((tx, ty - 2 * arc_r, 112.6))
    it.add(sweep(path, circle_profile(1.1, 18), up=(1, 0, 0)), "Chrome", shade="smooth")
    sy = ty - 2 * arc_r
    it.add(lathe([(0, 0), (1.3, 0), (1.35, 0.3), (1.35, 2.0), (1.1, 2.4), (0, 2.4)], 24), "Chrome", loc=(tx, sy, 110.4), shade="smooth")
    it.add(cylinder(0.9, 0.05, 20), SHADOW, loc=(tx, sy, 110.35))  # aerator
    it.add(lathe([(0, 0), (1.15, 0), (1.15, 1.4), (0.9, 1.7), (0, 1.7)], 20), "Chrome", loc=(tx + 1.7, ty, 103.0), rot=(0, 90, 0),
           shade="smooth")
    it.add(sweep([(tx + 2.6, ty, 103.0), (tx + 3.6, ty + 1.2, 105.5), (tx + 4.2, ty + 3.8, 109.0)], rect_profile(1.3, 0.6, 0.2),
                 up=(0, 1, 0)), "Chrome", shade="smooth")


@item("Deco_kitchen_stove")
def kitchen_stove(it: Item):
    """Cooker unit: glass-ceramic hob with four zones, built-in oven with glass door, drawer below."""
    module_body(it)
    module_worktop(it)
    f = BODY_FRONT

    # Hob: black glass with a polished bevel, sits on the worktop.
    hy = 1.0
    gz = TOP_Z1 + 0.6
    it.add(cube(56, 50, 0.6), "Screen_Black", loc=(0, hy, TOP_Z1 + 0.3), bevel=0.25, seg=3)
    # Cooking zones (x, y, radius, dual circuit): dark-grey field, light outline, inner ring for the big one.
    for x, y, r, dual in ((-13.5, -5.0, 10.0, True), (-13.5, 15.0, 8.0, False), (13.5, -5.0, 8.0, False), (13.5, 15.0, 9.0, False)):
        it.add(cylinder(r - 0.5, 0.02, 64), "Paint_Anthracite", loc=(x, y, gz), shade="flat")
        ring(it, "Plastic_Grey", x, y, gz, r, 0.55)
        if dual:
            ring(it, "Plastic_Grey", x, y, gz, r * 0.62, 0.35)
    # Touch controls at the front: slider, zone dots, power symbol, residual heat LED.
    cyc = hy - 21.0
    it.add(cube(16, 0.3, 0.03), "Plastic_Grey", loc=(0, cyc, gz + 0.015))
    for i in range(9):
        it.add(cube(0.12, 0.8, 0.03), "Plastic_Grey", loc=(-8 + i * 2, cyc + 0.9, gz + 0.015))
    for x in (-13.5, -11.0, 11.0, 13.5):
        ring(it, "Plastic_Grey", x, cyc, gz, 0.8, 0.18, seg=24)
    ring(it, "Plastic_Grey", 22.5, cyc, gz, 1.0, 0.2, seg=32)
    it.add(cube(0.2, 1.0, 0.03), "Plastic_Grey", loc=(22.5, cyc + 0.6, gz + 0.015))
    it.add(cylinder(0.35, 0.04, 12), "LED_Red", loc=(-20.0, cyc, gz))

    # Oven: steel control strip in the top band, display and two knobs.
    it.add(cube(2 * FRONT_X, 2.0, FRONT_Z1 - BAND_Z0), STEEL, loc=(0, f - 1.0, (BAND_Z0 + FRONT_Z1) / 2), bevel=0.25, seg=2)
    pz = (BAND_Z0 + FRONT_Z1) / 2
    it.add(cube(24, 0.3, 8.0), "Screen_Black", loc=(0, f - 2.1, pz), bevel=0.12, seg=2)
    seven_seg(it, "12:00", f - 2.25, -4.6, pz - 1.4, 2.8)
    for x in (-9.5, 9.5):
        it.add(cylinder(0.45, 0.05, 12), "Display_Blue" if x < 0 else "Plastic_Grey", loc=(x, f - 2.25, pz), rot=(90, 0, 0))
    for sx in (-1, 1):
        kx = sx * 21.0
        it.add(lathe([(0, 0), (2.5, 0), (2.5, 0.25), (2.25, 0.45), (2.15, 0.6), (2.15, 1.9), (1.95, 2.3), (0, 2.35)], 40), STEEL,
               loc=(kx, f - 2.0, pz), rot=(90, 0, 0), shade="auto", angle=40)
        it.add(cube(0.35, 0.15, 1.6), SHADOW, loc=(kx, f - 4.36, pz + 1.0))
    # Oven door: steel, black glass frame around a tinted window, cavity with racks behind, bar handle on top.
    dz0, dz1 = 26.3, LOWER_Z1
    door_face = f - 2.0
    it.add(cube(2 * FRONT_X, 2.0, dz1 - dz0), STEEL, loc=(0, f - 1.0, (dz0 + dz1) / 2), bevel=0.25, seg=2)
    gx, gz0, gz1 = 28.8, dz0 + 0.8, 62.4
    wx, wz0, wz1 = 21.0, 32.5, 57.5
    gy = door_face - 0.2
    for loc, size in (((0, gy, (gz0 + wz0) / 2), (2 * gx, 0.4, wz0 - gz0)), ((0, gy, (wz1 + gz1) / 2), (2 * gx, 0.4, gz1 - wz1)),
                      ((-(gx + wx) / 2, gy, (wz0 + wz1) / 2), (gx - wx, 0.4, wz1 - wz0)),
                      (((gx + wx) / 2, gy, (wz0 + wz1) / 2), (gx - wx, 0.4, wz1 - wz0))):
        it.add(cube(*size), "Screen_Black", loc=loc, bevel=0.1, seg=1)
    it.add(cube(2 * wx, 0.1, wz1 - wz0), "Iron_Dark", loc=(0, door_face - 0.05, (wz0 + wz1) / 2))   # cavity
    for z in (40.0, 49.5):  # oven racks
        it.add(cube(2 * wx - 2, 0.12, 0.35), STEEL, loc=(0, door_face - 0.16, z))
        for i in range(9):
            it.add(cube(0.2, 0.12, 0.25), STEEL, loc=(-16 + i * 4, door_face - 0.17, z - 0.3))
    it.add(cube(2 * wx, 0.1, wz1 - wz0), "Glass_Tinted", loc=(0, door_face - 0.34, (wz0 + wz1) / 2), shade="flat")
    bar_pull(it, "-y", door_face, 0, 65.9, 46, r=0.7, standoff=1.3)
    # Drawer below the oven.
    drawer(it, "-y", f, -FRONT_X, FRONT_X, FRONT_Z0, dz0 - GAP)


# ---------------------------------------------------------------------------------------------
# Island
# ---------------------------------------------------------------------------------------------


@item("Deco_kitchen_island")
def kitchen_island(it: Item):
    """Kitchen island 240 x 100 x 92: 5-cm marble top, oak drawers on the working side (-Y), oak doors on both
    ends, fluted oak panelling and a stainless footrest under the 35-cm overhang for bar stools (+Y)."""
    W, D, H = 240.0, 100.0, 92.0
    top_t = 5.0
    tz0 = H - top_t                  # 87
    bx = 115.0                       # carcass half width (end doors + pulls stay inside the top)
    by0, by1 = -45.0, 15.0           # carcass front / back face
    fz0, fz1 = PLINTH_H + 0.3, tz0 - 0.4

    it.add(cube(W, D, top_t), MARBLE, loc=(0, 0, tz0 + top_t / 2), bevel=0.6, seg=3)
    # Carcass, recessed plinth, gap shadows.
    it.add(cube(2 * bx, by1 - by0, tz0 - PLINTH_H), OAK, loc=(0, (by0 + by1) / 2, (PLINTH_H + tz0) / 2), grain="z")
    it.add(cube(2 * bx - 12, by1 - by0 - 12, PLINTH_H), PLINTH, loc=(0, (by0 + by1) / 2, PLINTH_H / 2))
    zc, hz = (PLINTH_H + tz0) / 2, tz0 - PLINTH_H
    it.add(cube(2 * bx - 0.2, 0.1, hz), SHADOW, loc=(0, by0 - 0.05, zc))
    for sx in (-1, 1):
        it.add(cube(0.1, by1 - by0 - 0.2, hz), SHADOW, loc=(sx * (bx + 0.05), (by0 + by1) / 2, zc))

    # Working side: 4 columns x 3 drawers.
    col = 2 * bx / 4
    band = fz1 - 15.6
    half = (band - GAP - fz0 - GAP) / 2
    rows = ((band, fz1), (fz0 + half + GAP, band - GAP), (fz0, fz0 + half))
    for c in range(4):
        u0 = -bx + c * col + GAP / 2
        for z0, z1 in rows:
            drawer(it, "-y", by0, u0, u0 + col - GAP, z0, z1)
    # Ends: one door each, wrapping the corner of the drawer fronts.
    for face, f in (("+x", bx), ("-x", -bx)):
        lo, hi = by0 - FRONT_T, by1 - GAP / 2
        u0, u1 = (lo, hi) if face == "+x" else (-hi, -lo)
        drawer(it, face, f, u0, u1, fz0, fz1)

    # Seating side: fluted oak panelling (one prism with a scalloped outline) up to the end doors.
    x0, x1 = -(bx + FRONT_T), bx + FRONT_T
    n = 59
    pitch = (x1 - x0) / n
    base, depth, steps = 0.4, 1.1, 6
    pts = []
    for i in range(n):
        xa = x0 + i * pitch
        for s in range(steps + (1 if i == n - 1 else 0)):
            a = math.pi * s / steps
            pts.append((xa + pitch * (1 - math.cos(a)) / 2, by1 + base + depth * math.sin(a)))
    pts += [(x1, by1), (x0, by1)]
    it.add(ngon_prism(pts, fz1 - fz0), OAK, loc=(0, 0, fz0), shade="auto", angle=40, grain="z")
    # Footrest: U-shaped stainless tube, fixed to the panelling at both ends and in the middle.
    fy, fz, fr = 31.0, 24.0, 5.0
    wall = by1 + base
    xe = 104.0
    path = [(-xe, wall, fz), (-xe, fy - fr, fz)] + arc_points((-xe + fr, fy - fr, fz), fr, 180, 90, 6)[1:] \
        + arc_points((xe - fr, fy - fr, fz), fr, 90, 0, 6) + [(xe, wall, fz)]
    it.add(sweep(path, circle_profile(1.4, 18), up=(0, 0, 1)), STEEL, shade="smooth")
    it.add(sweep([(0, wall, fz), (0, fy, fz)], circle_profile(0.9, 14), up=(0, 0, 1)), STEEL, shade="smooth")
    for x in (-xe, 0.0, xe):
        it.add(cylinder(2.6, 0.5, 24), STEEL, loc=(x, wall + depth - 0.1, fz), rot=(-90, 0, 0), shade="auto")  # rosettes


# ---------------------------------------------------------------------------------------------
# Fridge
# ---------------------------------------------------------------------------------------------


@item("Deco_fridge")
def fridge(it: Item):
    """Free-standing stainless fridge-freezer 60 x 65 x 200: fridge on top, freezer below, bar handles.

    The large surfaces use Paint_Grey (satin stainless look): the brushed Steel texture gets blotchy on a 2-m door.
    """
    W, H = 60.0, 200.0
    inox = "Paint_Grey"
    back = 31.8
    body_front = -24.6
    door_back, door_face = -25.4, -30.0
    door_t = door_back - door_face
    it.add(cube(W, back - body_front, H - 1.5), inox, loc=(0, (back + body_front) / 2, 1.5 + (H - 1.5) / 2), bevel=0.8, seg=3,
           grain="z")
    for sx in (-1, 1):
        for y in (body_front + 4, back - 4):
            it.add(cylinder(1.6, 1.5, 16), "Rubber_Black", loc=(sx * 25, y, 0))  # feet
    # Kick grille under the doors.
    it.add(cube(W - 1.6, 1.0, 7.2), SHADOW, loc=(0, body_front - 0.5, 5.4), bevel=0.2, seg=1)
    for i in range(4):
        it.add(cube(W - 4, 0.5, 0.6), "Plastic_Grey", loc=(0, body_front - 1.1, 2.9 + i * 1.6), bevel=0.15, seg=1)
    it.add(cube(W - 8, 4.0, 0.8), SHADOW, loc=(0, back - 6, H - 0.3))  # vent slot on top, at the back

    doors = ((9.6, 73.6), (74.4, H - 0.4))
    for z0, z1 in doors:
        it.add(cube(W - 0.4, door_t, z1 - z0), inox, loc=(0, (door_back + door_face) / 2, (z0 + z1) / 2), bevel=1.0, seg=4, grain="z")
        it.add(cube(W - 3.0, body_front - door_back + 0.2, z1 - z0 - 2.6), "Rubber_Black",  # door seal
               loc=(0, (door_back + body_front) / 2, (z0 + z1) / 2), bevel=0.3, seg=2)
    it.add(cube(W - 1.0, 0.1, 0.8), SHADOW, loc=(0, door_back + 0.3, 74.0))  # dark gap between the doors
    # Hinge covers on the left.
    it.add(cube(5.0, door_t + 1.0, 0.4), "Plastic_Grey", loc=(-26.5, (door_back + door_face) / 2 + 0.3, H - 0.2), bevel=0.2, seg=2)
    # Bar handles (right side, hinges left).
    hx = 23.5
    bar_pull(it, "-y", door_face, hx, 140.0, 72, vertical=True, r=1.1, standoff=1.0)
    bar_pull(it, "-y", door_face, hx, 52.0, 36, vertical=True, r=1.1, standoff=1.0)
    # Small display on the fridge door: temperatures, generic.
    dz = 150.0
    it.add(cube(13.0, 0.25, 4.6), "Screen_Black", loc=(-6.0, door_face - 0.12, dz), bevel=0.1, seg=2)
    seven_seg(it, "4", door_face - 0.25, -11.2, dz - 1.1, 2.2)
    seven_seg(it, "-18", door_face - 0.25, -6.0, dz - 1.1, 2.2)
    it.add(cylinder(0.25, 0.05, 10), "LED_Green", loc=(-0.3, door_face - 0.25, dz + 1.3), rot=(90, 0, 0))
