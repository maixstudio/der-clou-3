"""Villa doors I (group G9): door leaves for the inner door, front door, side entrance, glass door and one
leaf of the French double door. Exported as SM_Door_<name>.

Door leaf rule (Docs/Villa/Villa-Modelle.md): origin = bottom of the hinge edge. The leaf runs from y = 0 to
y = -B (B = leaf width), its thickness along X is centred on x = 0, z = 0 .. H. Hinges sit at y ~ 0, the
lever/lock at y ~ -(B - 8) on both faces. Outside / show side = -X, hinge barrels on +X. No frame (Zarge) parts.
"""

import math

import bmesh

from dc_items import item
from dc_mesh import CM, Item, circle_profile, cube, cylinder, lathe, sweep

HANDLE_Z = 105.0  # lever spindle height above the floor (DIN 1050 mm)
KEY_BB = 7.2      # spindle to keyhole, warded interior lock
KEY_PZ = 9.2      # spindle to profile cylinder, exterior lock
BACKSET = 8.0     # free edge to spindle


# ---------------------------------------------------------------------------------------------
# Leaf body
# ---------------------------------------------------------------------------------------------


def _face_rot(side):
    """Rotation that turns a lathe's +Z axis to point along side * X."""
    return (0, 90 * side, 0)


def slab(W, T, H, holes=()):
    """Leaf body from y=0 to y=-W, x -T/2..T/2, z 0..H with rectangular through-holes [(ya, yb, za, zb)] (cm).

    Built on a grid through all hole edges so it stays one closed mesh; the bevel modifier then rounds the
    outer and the hole edges but not the flat grid lines.
    """
    ys = sorted({0.0, -W} | {h[0] for h in holes} | {h[1] for h in holes})
    zs = sorted({0.0, H} | {h[2] for h in holes} | {h[3] for h in holes})
    ni, nj = len(ys) - 1, len(zs) - 1

    def in_hole(i, j):
        cy, cz = (ys[i] + ys[i + 1]) / 2, (zs[j] + zs[j + 1]) / 2
        return any(min(a, b) < cy < max(a, b) and min(c, d) < cz < max(c, d) for a, b, c, d in holes)

    solid = [[not in_hole(i, j) for j in range(nj)] for i in range(ni)]

    def sol(i, j):
        return 0 <= i < ni and 0 <= j < nj and solid[i][j]

    bm = bmesh.new()
    verts = {}

    def v(i, j, s):
        key = (i, j, s)
        if key not in verts:
            verts[key] = bm.verts.new((s * T / 2 * CM, ys[i] * CM, zs[j] * CM))
        return verts[key]

    for i in range(ni):
        for j in range(nj):
            if not solid[i][j]:
                continue
            for s in (-1, 1):
                bm.faces.new((v(i, j, s), v(i + 1, j, s), v(i + 1, j + 1, s), v(i, j + 1, s)))
            if not sol(i - 1, j):
                bm.faces.new((v(i, j, -1), v(i, j + 1, -1), v(i, j + 1, 1), v(i, j, 1)))
            if not sol(i + 1, j):
                bm.faces.new((v(i + 1, j, -1), v(i + 1, j + 1, -1), v(i + 1, j + 1, 1), v(i + 1, j, 1)))
            if not sol(i, j - 1):
                bm.faces.new((v(i, j, -1), v(i + 1, j, -1), v(i + 1, j, 1), v(i, j, 1)))
            if not sol(i, j + 1):
                bm.faces.new((v(i, j + 1, -1), v(i + 1, j + 1, -1), v(i + 1, j + 1, 1), v(i, j + 1, 1)))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def _hole_box(hole):
    y0, y1 = sorted(hole[:2])
    z0, z1 = sorted(hole[2:])
    return y0, y1, z0, z1


def bead_loop(it, mat, hole, x_base, side, prof, grain="z"):
    """Moulding swept around a hole on one face. prof: [(b, n)] with b > 0 into the frame, b < 0 into the
    opening, n = height above the base plane x_base (towards side * X)."""
    y0, y1, z0, z1 = _hole_box(hole)
    pts = [(x_base, y0, z0), (x_base, y1, z0), (x_base, y1, z1), (x_base, y0, z1)]
    it.add(sweep(pts, [(b, side * n) for b, n in prof], up=(1, 0, 0), closed=True), mat, shade="auto", angle=40,
           grain=grain)


def ogee(w, h):
    """Small panel moulding: tall at the frame edge, curving down onto the panel."""
    return [(0.1, 0.0), (-w, 0.0), (-w, 0.1 * h), (-0.82 * w, 0.28 * h), (-0.6 * w, 0.58 * h), (-0.35 * w, 0.84 * h),
            (-0.12 * w, h), (0.1, h)]


def flat_panel(it, mat, hole, thick, overlap=0.8):
    y0, y1, z0, z1 = _hole_box(hole)
    it.add(cube(thick, y1 - y0 + 2 * overlap, z1 - z0 + 2 * overlap), mat, loc=(0, (y0 + y1) / 2, (z0 + z1) / 2),
           shade="flat", grain="z")


def fielded_panel(it, mat, hole, t_edge, t_field, inset, overlap=0.8):
    """Raised panel: thin edge (hidden in the frame groove) sloping up to a flat field on both faces."""
    y0, y1, z0, z1 = _hole_box(hole)
    w, h = y1 - y0 + 2 * overlap, z1 - z0 + 2 * overlap
    inset += overlap
    bm = bmesh.new()

    def rect(x, hw, hh):
        return [bm.verts.new((x * CM, y * CM, z * CM)) for y, z in ((-hw, -hh), (hw, -hh), (hw, hh), (-hw, hh))]

    of, ifr = rect(-t_edge / 2, w / 2, h / 2), rect(-t_field / 2, w / 2 - inset, h / 2 - inset)
    ob, ib = rect(t_edge / 2, w / 2, h / 2), rect(t_field / 2, w / 2 - inset, h / 2 - inset)
    bm.faces.new(ifr)
    bm.faces.new(ib)
    for k in range(4):
        n = (k + 1) % 4
        bm.faces.new((of[k], of[n], ifr[n], ifr[k]))
        bm.faces.new((ob[k], ob[n], ib[n], ib[k]))
        bm.faces.new((of[k], of[n], ob[n], ob[k]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    it.add(bm, mat, loc=(0, (y0 + y1) / 2, (z0 + z1) / 2), shade="auto", angle=4, grain="z")


def glazing(it, hole, T, glass="Glass", bead_mat="Paint_White", pane=0.8, bead_w=1.4, overlap=0.8):
    """Glass pane centred in the leaf plus a glazing bead on both faces."""
    y0, y1, z0, z1 = _hole_box(hole)
    it.add(cube(pane, y1 - y0 + 2 * overlap, z1 - z0 + 2 * overlap), glass, loc=(0, (y0 + y1) / 2, (z0 + z1) / 2),
           shade="flat")
    h = (T - pane) / 2 - 0.15
    prof = [(0.1, 0.0), (-bead_w, 0.0), (-bead_w, 0.22 * h), (-0.55 * bead_w, 0.8 * h), (-0.25 * bead_w, h), (0.1, h)]
    for side in (-1, 1):
        bead_loop(it, bead_mat, hole, side * pane / 2, side, prof)


# ---------------------------------------------------------------------------------------------
# Hardware
# ---------------------------------------------------------------------------------------------


def screw(it, mat, x_surf, y, z, side, r=0.3):
    it.add(lathe([(0, 0), (r, 0), (r * 0.85, 0.07), (r * 0.45, 0.13), (0, 0.14)], 12), mat, loc=(x_surf, y, z),
           rot=_face_rot(side), shade="smooth")


def rosette(it, mat, x_surf, ys, zs, side, r=2.6, h=0.9):
    it.add(lathe([(0, 0), (r, 0), (r, h * 0.4), (r - 0.12, h * 0.75), (r - 0.4, h), (0, h)], 40), mat,
           loc=(x_surf, ys, zs), rot=_face_rot(side), shade="auto", angle=28)


def lever(it, mat, x_surf, ys, zs, side, length=12.5, reach=6.0, r=0.95, bend=1.9):
    """L-shaped lever: neck out of the face, rounded bend, bar pointing towards the hinge (+Y)."""
    cx = x_surf + side * (reach - bend)
    cy = ys + bend
    xb = x_surf + side * reach
    pts = [(x_surf + side * 0.3, ys, zs), (cx, ys, zs)]
    for k in range(1, 8):
        a = math.radians(90 * k / 7)
        pts.append((cx + side * bend * math.sin(a), cy - bend * math.cos(a), zs))
    tip = ys + length
    pts += [(xb, tip - 1.4, zs), (xb, tip - 0.6, zs), (xb, tip - 0.15, zs), (xb, tip, zs)]
    scales = [1.0] * (len(pts) - 3) + [0.96, 0.8, 0.45]
    it.add(sweep(pts, circle_profile(r, 18, sx=0.82), up=(0, 0, 1), scales=scales), mat, shade="smooth")


def keyhole_rosette(it, mat, x_surf, ys, zs, side, r=2.3):
    rosette(it, mat, x_surf, ys, zs, side, r=r, h=0.7)
    x = x_surf + side * 0.7
    it.add(cylinder(0.33, 0.05, 14), "Plastic_Black", loc=(x, ys, zs + 0.35), rot=_face_rot(side), shade="flat")
    it.add(cube(0.1, 0.3, 0.95), "Plastic_Black", loc=(x, ys, zs - 0.12))


def long_plate(it, mat, x_surf, ys, z_bot, z_top, side, w=4.6, t=0.8):
    it.add(cube(t, w, z_top - z_bot), mat, loc=(x_surf + side * t / 2, ys, (z_bot + z_top) / 2), bevel=min(0.45 * t, 0.5),
           seg=3, grain="z")


def profile_cylinder(it, x_surf, ys, zc, side, mat="Steel", proud=0.3):
    """Euro profile cylinder face: round core with key way plus the cam tail below (zc = core centre)."""
    it.add(lathe([(0, 0), (0.86, 0), (0.86, proud - 0.07), (0.78, proud), (0, proud)], 24), mat, loc=(x_surf, ys, zc),
           rot=_face_rot(side), shade="auto", angle=28)
    it.add(cube(proud, 1.0, 1.7), mat, loc=(x_surf + side * proud / 2, ys, zc - 0.85), bevel=0.06, seg=1)
    it.add(lathe([(0, 0), (0.52, 0), (0.52, 0.04), (0, 0.04)], 20), "Brass", loc=(x_surf + side * proud, ys, zc),
           rot=_face_rot(side), shade="flat")
    it.add(cube(0.08, 0.16, 1.15), "Plastic_Black", loc=(x_surf + side * (proud + 0.04), ys, zc - 0.25))


def hinge(it, mat, T, z0, r=0.75, h=11.0, flap=False, side=1):
    """Two-part barrel hinge on the +X face at the hinge edge; flap=True adds a screwed leaf (heavy doors)."""
    x, y = side * (T / 2 - 0.05), -0.35
    half = h / 2
    low = [(0, -0.28), (0.3 * r, -0.26), (0.75 * r, -0.12), (r, 0.1), (r, half - 0.1), (0, half - 0.1)]
    up = [(0, 0), (r, 0), (r, half - 0.2), (0.75 * r, half - 0.02), (0.3 * r, half + 0.12), (0, half + 0.14)]
    it.add(lathe(low, 24), mat, loc=(x, y, z0), shade="auto", angle=28, grain="z")
    it.add(cylinder(0.55 * r, 0.2, 16), mat, loc=(x, y, z0 + half - 0.1), shade="smooth", grain="z")
    it.add(lathe(up, 24), mat, loc=(x, y, z0 + half + 0.1), shade="auto", angle=28, grain="z")
    if flap:
        fz = z0 + half + 0.1 + (half - 0.3) / 2
        fw = 4.6
        it.add(cube(0.3, fw, half - 0.4), mat, loc=(side * (T / 2 + 0.15), y - fw / 2, fz), bevel=0.1, seg=2)
        for dz in (-0.27, 0.27):
            screw(it, mat, side * (T / 2 + 0.3), y - fw * 0.62, fz + dz * (half - 0.4), side)


def lock_edge(it, W, T, zs, key_dz, mat="Steel", length=23.5, z_mid=None, pins=()):
    """Lock face plate on the free edge (y = -W): latch at the lever, bolt slot at the key, optional pins."""
    y = -W
    zc = zs - key_dz / 2 if z_mid is None else z_mid
    it.add(cube(min(2.4, T - 1.2), 0.3, length), mat, loc=(0, y - 0.1, zc), bevel=0.08, seg=1, grain="z")
    it.add(cube(1.0, 0.8, 2.0), "Chrome", loc=(0, y - 0.45, zs), bevel=0.12, seg=2)  # latch bolt
    it.add(cube(1.0, 0.05, 2.0), "Plastic_Black", loc=(0, y - 0.27, zs - key_dz))  # dead bolt slot
    for z in pins:  # roller pins of a multi-point lock
        it.add(cube(1.0, 0.05, 2.6), "Plastic_Black", loc=(0, y - 0.27, z))
        it.add(lathe([(0, 0), (0.55, 0), (0.55, 0.45), (0.45, 0.6), (0, 0.6)], 16), mat, loc=(0, y - 0.25, z + 2.2),
               rot=(90, 0, 0), shade="auto", angle=28)


def push_bar(it, mat, T, yb, z0, z1, side=-1, stand=6.5, r=1.5):
    """Vertical stainless push bar on two standoffs (front door outside)."""
    L = z1 - z0
    x = side * (T / 2 + stand)
    it.add(lathe([(0, 0), (0.55 * r, 0), (0.9 * r, 0.15 * r), (r, 0.5 * r), (r, L - 0.5 * r), (0.9 * r, L - 0.15 * r),
                  (0.55 * r, L), (0, L)], 28), mat, loc=(x, yb, z0), shade="auto", angle=26, grain="z")
    for z in (z0 + 0.16 * L, z1 - 0.16 * L):
        it.add(lathe([(0, 0), (1.7, 0), (1.7, 0.35), (1.15, 0.65), (0.95, 0.9), (0.95, stand), (0, stand)], 24), mat,
               loc=(side * T / 2, yb, z), rot=_face_rot(side), shade="auto", angle=40)


def weather_bar(it, mat, W, T, side=-1):
    """Weather drip along the bottom of the outside face (profile sticks out towards side * X)."""
    prof = [(-0.3, 0.3), (2.7, 0.3), (2.9, 0.8), (2.6, 1.4), (0.5, 4.6), (-0.3, 4.9)]
    # Path along -Y with up=Z: profile x points to -X, so mirror it for the +X side.
    prof = [(px * -side, pz) for px, pz in prof]
    it.add(sweep([(side * T / 2, -0.4, 0), (side * T / 2, -W + 0.4, 0)], prof, up=(0, 0, 1)), mat, shade="auto",
           angle=30, grain="y")


def kick_plate(it, mat, W, T, z0, z1, side, screws="Steel"):
    x = side * T / 2
    it.add(cube(0.15, W - 3.0, z1 - z0), mat, loc=(x + side * 0.075, -W / 2, (z0 + z1) / 2), bevel=0.05, seg=1, grain="y")
    for k in range(4):
        y = -2.5 - k * (W - 5.0) / 3
        for z in (z0 + 2.0, z1 - 2.0):
            screw(it, screws, x + side * 0.15, y, z, side, r=0.28)


def lever_set(it, T, W, side, mat, plate=None, key="bb", plate_mat=None, rose_r=2.6, lever_len=12.5):
    """Lever with rosettes or a long plate on one face."""
    ys = -(W - BACKSET)
    x_surf = side * T / 2
    pm = plate_mat or mat
    if plate == "long":
        kz = HANDLE_Z - (KEY_PZ if key == "pz" else KEY_BB)
        long_plate(it, pm, x_surf, ys, kz - 4.2, HANDLE_Z + 6.0, side)
        x_out = x_surf + side * 0.8
        rosette(it, pm, x_out, ys, HANDLE_Z, side, r=1.6, h=0.5)
        lever(it, mat, x_out + side * 0.3, ys, HANDLE_Z, side, length=lever_len, reach=5.5)
        if key == "pz":
            profile_cylinder(it, x_out, ys, kz, side)
        else:
            it.add(cylinder(0.33, 0.05, 14), "Plastic_Black", loc=(x_out, ys, kz + 0.35), rot=_face_rot(side), shade="flat")
            it.add(cube(0.1, 0.3, 0.95), "Plastic_Black", loc=(x_out, ys, kz - 0.12))
        return
    rosette(it, mat, x_surf, ys, HANDLE_Z, side, r=rose_r)
    lever(it, mat, x_surf + side * 0.9, ys, HANDLE_Z, side, length=lever_len, reach=5.4)
    if key == "bb":
        keyhole_rosette(it, mat, x_surf, ys, HANDLE_Z - KEY_BB, side, r=rose_r * 0.9)
    elif key == "pz":
        rosette(it, mat, x_surf, ys, HANDLE_Z - KEY_PZ, side, r=rose_r * 0.9, h=0.7)
        profile_cylinder(it, x_surf + side * 0.7, ys, HANDLE_Z - KEY_PZ + 0.4, side)


# ---------------------------------------------------------------------------------------------
# Doors
# ---------------------------------------------------------------------------------------------


@item("Door_inner")
def door_inner(it: Item):
    """White lacquered interior door, four recessed panels with a small moulding, chrome levers on rosettes."""
    W, T, H = 88.0, 4.0, 208.0
    paint = "Paint_White"
    st, mid, top, bot, lock0, lock1 = 12.0, 10.0, 13.0, 22.0, 90.0, 112.0
    cols = ((-st, -(W / 2 - mid / 2)), (-(W / 2 + mid / 2), -(W - st)))
    rows = ((bot, lock0), (lock1, H - top))
    holes = [(ya, yb, za, zb) for ya, yb in cols for za, zb in rows]
    it.add(slab(W, T, H, holes), paint, bevel=0.25, seg=2, grain="z")
    panel_t = 1.4
    for hole in holes:
        flat_panel(it, paint, hole, panel_t)
        for side in (-1, 1):
            bead_loop(it, paint, hole, side * panel_t / 2, side, ogee(1.3, 1.1))
    for side in (-1, 1):
        lever_set(it, T, W, side, "Chrome", key="bb")
    for z0 in (24.0, 172.0):
        hinge(it, "Steel", T, z0)
    lock_edge(it, W, T, HANDLE_Z, KEY_BB)


@item("Door_front")
def door_front(it: Item):
    """Solid dark wood front door with a narrow frosted glass strip; stainless push bar outside (-X),
    security plate with cylinder guard outside, lever on a long plate inside, multi-point lock."""
    W, T, H = 118.0, 7.0, 218.0
    wood = "Wood_Dark"
    strip = (-30.0, -46.0, 40.0, 190.0)
    it.add(slab(W, T, H, [strip]), wood, bevel=0.4, seg=3, grain="z")
    glazing(it, strip, T, glass="Glass_Frosted", bead_mat="Steel", pane=3.6, bead_w=1.2)
    # Milled grooves on the outside, between glass strip and lock side.
    for z in (62.0, 84.0, 126.0, 148.0):
        it.add(cube(0.12, W - 58.0, 0.6), "Plastic_Black", loc=(-T / 2 - 0.03, -(46.0 + (W - 58.0) / 2 + 6.0), z))
    ys = -(W - BACKSET)
    # Outside: push bar and security plate (no lever) with a cylinder guard.
    push_bar(it, "Steel", T, -(W - 20.0), 45.0, 165.0)
    kz = HANDLE_Z - KEY_PZ
    it.add(cube(1.2, 4.8, 32.0), "Steel", loc=(-T / 2 - 0.6, ys, 101.0), bevel=0.5, seg=3, grain="z")
    guard_x = -T / 2 - 1.2
    it.add(lathe([(0, 0), (1.95, 0), (1.95, 0.6), (1.75, 1.0), (1.2, 1.1), (0, 1.1)], 32), "Steel", loc=(guard_x, ys, kz),
           rot=_face_rot(-1), shade="auto", angle=28)
    profile_cylinder(it, guard_x - 1.1, ys, kz + 0.4, -1, proud=0.15)
    # Inside: lever on a long plate.
    lever_set(it, T, W, 1, "Steel", plate="long", key="pz")
    for z0 in (24.0, 108.0, 184.0):
        hinge(it, "Steel", T, z0, r=1.1, h=14.0, flap=True)
    lock_edge(it, W, T, HANDLE_Z, KEY_PZ, length=176.0, z_mid=110.0, pins=(40.0, 175.0))
    weather_bar(it, "Aluminium", W, T, side=-1)


@item("Door_exterior")
def door_exterior(it: Item):
    """Robust side entrance door (workshop/garage): green paint, two raised panels, small window at the top,
    aluminium kick plates, black lever on long plates, three surface hinges."""
    W, T, H = 98.0, 5.0, 208.0
    paint = "Paint_Green"
    st = 14.0
    win = (-st, -(W - st), 150.0, 192.0)
    upper = (-st, -(W - st), 100.0, 138.0)
    lower = (-st, -(W - st), 32.0, 88.0)
    it.add(slab(W, T, H, [win, upper, lower]), paint, bevel=0.35, seg=2, grain="z")
    for hole in (upper, lower):
        fielded_panel(it, paint, hole, 1.6, 3.2, 5.5)
    glazing(it, win, T, glass="Glass", bead_mat=paint, pane=2.0, bead_w=1.5)
    it.add(cube(T - 1.4, 3.0, win[3] - win[2] + 1.0), paint, loc=(0, -W / 2, (win[2] + win[3]) / 2), bevel=0.45, seg=2,
           grain="z")
    for side in (-1, 1):
        lever_set(it, T, W, side, "Iron_Dark", plate="long", key="pz")
    kick_plate(it, "Aluminium", W, T, 6.5, 28.0, -1)
    kick_plate(it, "Aluminium", W, T, 1.0, 25.0, 1)
    weather_bar(it, "Aluminium", W, T, side=-1)
    for z0 in (22.0, 100.0, 178.0):
        hinge(it, "Iron_Dark", T, z0, r=0.9, h=12.0, flap=True)
    lock_edge(it, W, T, HANDLE_Z, KEY_PZ)


@item("Door_glass")
def door_glass(it: Item):
    """White frame with one large frosted glass pane (kitchen, also used for the sauna)."""
    W, T, H = 98.0, 5.0, 208.0
    paint = "Paint_White"
    st, top, bot = 12.0, 12.0, 22.0
    hole = (-st, -(W - st), bot, H - top)
    it.add(slab(W, T, H, [hole]), paint, bevel=0.3, seg=2, grain="z")
    glazing(it, hole, T, glass="Glass_Frosted", bead_mat=paint, pane=0.8, bead_w=1.6)
    for side in (-1, 1):
        lever_set(it, T, W, side, "Chrome", key="bb")
    for z0 in (24.0, 172.0):
        hinge(it, "Steel", T, z0)
    lock_edge(it, W, T, HANDLE_Z, KEY_BB)


@item("Door_french_leaf")
def door_french_leaf(it: Item):
    """One leaf of a white French double door: glass in 2 x 5 lites with glazing bars, brass lever."""
    W, T, H = 69.0, 4.0, 208.0
    paint = "Paint_White"
    st, top, bot = 11.0, 11.0, 24.0
    hole = (-st, -(W - st), bot, H - top)
    y0, y1, z0, z1 = _hole_box(hole)
    it.add(slab(W, T, H, [hole]), paint, bevel=0.25, seg=2, grain="z")
    glazing(it, hole, T, glass="Glass", bead_mat=paint, pane=0.6, bead_w=1.3)
    bar_w, bar_t = 2.2, T - 0.6
    it.add(cube(bar_t, bar_w, z1 - z0 + 1.0), paint, loc=(0, (y0 + y1) / 2, (z0 + z1) / 2), bevel=0.5, seg=2, grain="z")
    for k in range(1, 5):
        z = z0 + (z1 - z0) * k / 5
        it.add(cube(bar_t, y1 - y0 + 1.0, bar_w), paint, loc=(0, (y0 + y1) / 2, z), bevel=0.5, seg=2, grain="y")
    for side in (-1, 1):
        lever_set(it, T, W, side, "Brass", key="bb", rose_r=2.3, lever_len=11.5)
    for zz in (24.0, 172.0):
        hinge(it, "Brass", T, zz)
    lock_edge(it, W, T, HANDLE_Z, KEY_BB, mat="Brass")
