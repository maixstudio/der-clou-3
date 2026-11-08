"""G10 - special door leaves for the villa: steel fire door, vault door, wrought-iron grate, slatted cellar door and
the bookshelf secret door.

Door leaf rule (Docs/Villa/Villa-Modelle.md): origin = hinge at the bottom. The leaf runs from y = 0 to y = -B,
its thickness lies along X centred on x = 0, z = 0 .. H. Hinges at y ~ 0, handle/lock at y ~ -(B - 8) on both
faces, show side (outside) = -X. No frames here (they are separate models).
"""

import math
import random

from mathutils import Euler, Vector

from dc_items import item
from dc_items_small import spline
from dc_mesh import Item, circle_profile, cube, cylinder, lathe, ngon_prism, rect_profile, sphere, sweep, torus

# ---------------------------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------------------------


def axis_rot(axis):
    """Rotation (degrees) that turns a part built along +Z (cylinder, lathe, ngon_prism) to point along `axis`."""
    return {"+x": (0, 90, 0), "-x": (0, -90, 0), "+y": (-90, 0, 0), "-y": (90, 0, 0), "+z": (0, 0, 0)}[axis]


def face_rot(side):
    return axis_rot("+x" if side > 0 else "-x")


def radial_rot(a_deg):
    """Rotation for a +Z-built part so it points radially in the YZ plane at angle a (0 = +Y, 90 = +Z)."""
    return (a_deg - 90.0, 0, 0)


def rounded_outline(half_u, half_v, r, steps=4):
    """Rounded rectangle outline (cm) centred on the origin."""
    r = min(r, half_u - 0.01, half_v - 0.01)
    pts = []
    for cu, cv, a0 in ((half_u - r, half_v - r, 0), (-half_u + r, half_v - r, 90), (-half_u + r, -half_v + r, 180),
                       (half_u - r, -half_v + r, 270)):
        for i in range(steps + 1):
            a = math.radians(a0 + 90.0 * i / steps)
            pts.append((cu + r * math.cos(a), cv + r * math.sin(a)))
    return pts


def plate(it, mat, face_x, side, y, z, w, h, t, r=1.0, bevel=0.15):
    """Flat plate with rounded corners lying on a door face (x = face_x), growing towards `side`."""
    it.add(ngon_prism(rounded_outline(h / 2, w / 2, r), t), mat, loc=(face_x, y, z), rot=face_rot(side), bevel=bevel, seg=2)


def dome(it, mat, face_x, side, y, z, r=0.6):
    """Screw / rivet head on a door face."""
    it.add(sphere(r, 12, 5, sz=0.55), mat, loc=(face_x, y, z), rot=face_rot(side), shade="smooth")


def knuckle(it, mat, x, y, z, h, r, seg=20):
    """Hinge barrel along Z (bottom at z): two halves with a joint line and a pin head on top."""
    prof = [(0, 0), (r * 0.82, 0), (r, r * 0.18), (r, h / 2 - 0.15), (r * 0.9, h / 2), (r, h / 2 + 0.15), (r, h - r * 0.18),
            (r * 0.82, h), (0, h)]
    it.add(lathe(prof, seg), mat, loc=(x, y, z), shade="auto", angle=40)
    it.add(lathe([(0, 0), (r * 0.55, 0), (r * 0.5, r * 0.22), (0, r * 0.32)], seg), mat, loc=(x, y, z + h), shade="smooth")


def lever(it, mat, face_x, side, y, z, length=12.5, reach=6.0, thick=1.0, rose_r=2.6, rose_mat=None):
    """Door lever on a face: round rose, neck out along X, grip towards the hinge (+Y)."""
    s = side
    if rose_r:
        it.add(cylinder(rose_r, 0.9, 32), rose_mat or mat, loc=(face_x, y, z), rot=face_rot(s), bevel=0.3, seg=2)
        face_x += s * 0.9
    pts = spline([(face_x, y, z), (face_x + s * reach * 0.62, y, z), (face_x + s * reach * 0.92, y + 0.7, z),
                  (face_x + s * reach, y + 2.4, z), (face_x + s * reach, y + length, z)], 5)
    it.add(sweep(pts, circle_profile(thick, 16, sx=0.8, sy=1.15), up=(0, 0, 1)), mat, shade="smooth")
    it.add(sphere(thick * 0.98, 16, 8, sz=1.15), mat, loc=(face_x + s * reach, y + length, z - thick * 1.12), shade="smooth")


def keyhole(it, face_x, side, y, z, size=1.0, ring="Brass"):
    """Round escutcheon with a dark keyhole (bit key)."""
    it.add(cylinder(1.25 * size, 0.3, 24), ring, loc=(face_x, y, z), rot=face_rot(side), bevel=0.1, seg=1)
    fx = face_x + side * 0.3
    it.add(cylinder(0.32 * size, 0.06, 12), "Plastic_Black", loc=(fx, y, z + 0.22 * size), rot=face_rot(side))
    it.add(cube(0.06, 0.32 * size, 0.85 * size), "Plastic_Black", loc=(fx + side * 0.03, y, z - 0.2 * size))


def profile_cylinder(it, face_x, side, y, z, mat="Chrome"):
    """End of a euro profile cylinder flush on a plate (round top, web below) with the key slot."""
    it.add(cylinder(0.85, 0.35, 20), mat, loc=(face_x, y, z + 0.45), rot=face_rot(side), shade="auto")
    it.add(cube(0.35, 0.95, 1.6), mat, loc=(face_x + side * 0.175, y, z - 0.35), bevel=0.08, seg=1)
    it.add(cube(0.06, 0.16, 1.5), "Plastic_Black", loc=(face_x + side * 0.37, y, z - 0.05))


def pull_bar(it, mat, face_x, side, y, z, length, vertical=True, standoff=3.0, r=0.75):
    """Bar pull on two standoffs on a door face."""
    s = side
    for k in (-1, 1):
        oy, oz = (0, k * length * 0.42) if vertical else (k * length * 0.42, 0)
        it.add(cylinder(r * 0.75, standoff, 16), mat, loc=(face_x, y + oy, z + oz), rot=face_rot(s), shade="smooth")
        it.add(cylinder(r * 1.5, 0.4, 20), mat, loc=(face_x, y + oy, z + oz), rot=face_rot(s), bevel=0.12, seg=1)
    x = face_x + s * (standoff + r * 0.6)
    if vertical:
        it.add(lathe([(0, 0), (r * 0.8, 0), (r, 0.3), (r, length - 0.3), (r * 0.8, length), (0, length)], 16), mat,
               loc=(x, y, z - length / 2), shade="smooth")
    else:
        it.add(lathe([(0, 0), (r * 0.8, 0), (r, 0.3), (r, length - 0.3), (r * 0.8, length), (0, length)], 16), mat,
               loc=(x, y - length / 2, z), rot=axis_rot("+y"), shade="smooth")


def yz_ellipse(cy, cz, a, b, t0, t1, steps):
    """Points (x = 0) on an ellipse in the door plane, angles in degrees."""
    out = []
    for i in range(steps + 1):
        t = math.radians(t0 + (t1 - t0) * i / steps)
        out.append((0.0, cy + a * math.cos(t), cz + b * math.sin(t)))
    return out


def spiral_yz(cy, cz, r_out, r_in, turns, a0, cw, steps_per_turn=28, power=None):
    """Spiral in the door plane from the outer to the inner end (logarithmic, or r ~ (1 - t)^power)."""
    n = max(int(steps_per_turn * turns), 6)
    out = []
    for i in range(n + 1):
        t = i / n
        a = math.radians(a0) + (-1 if cw else 1) * 2 * math.pi * turns * t
        r = r_out * (r_in / r_out) ** t if power is None else r_in + (r_out - r_in) * (1 - t) ** power
        out.append((0.0, cy + r * math.cos(a), cz + r * math.sin(a)))
    return out


# ---------------------------------------------------------------------------------------------
# Steel fire door
# ---------------------------------------------------------------------------------------------


@item("Door_steel")
def door_steel(it: Item):
    """Grey fire door 88 x 5 x 208: rebated steel leaf, two hinges, overhead closer (+X), black lever sets."""
    B, H = 88.0, 208.0
    paint = "Paint_Grey"
    # Rebated leaf: the +X half overlaps the frame on hinge side, lock side and top, the -X half sits in the opening.
    lip = 1.4
    it.add(cube(2.4, B, H), paint, loc=(1.3, -B / 2, H / 2), bevel=0.45, seg=3, grain="z")                       # x 0.1 .. 2.5
    it.add(cube(2.9, B - 2 * lip, H - lip), paint, loc=(-1.05, -B / 2, (H - lip) / 2), bevel=0.5, seg=3, grain="z")  # x -2.5 .. 0.4
    # Black seal in the rebate (outlines the leaf on the show side).
    sl = [(0, -0.75, 0.0), (0, -0.75, H - 0.75), (0, -B + 0.75, H - 0.75), (0, -B + 0.75, 0.0)]
    it.add(sweep(sl, rect_profile(0.7, 0.5), up=(1, 0, 0)), "Rubber_Black", loc=(-0.15, 0, 0), shade="flat")
    # Generic sign plate at eye level on the show side (white with red border and dark text lines).
    sx = -2.5
    plate(it, "Paint_Red", sx, -1, -B / 2, 158.0, 22.0, 9.0, 0.15, r=0.6, bevel=0.0)
    plate(it, "Paint_White", sx - 0.15, -1, -B / 2, 158.0, 20.6, 7.6, 0.05, r=0.4, bevel=0.0)
    for k, (ww, zz) in enumerate(((15.0, 160.0), (17.0, 157.6), (11.0, 155.4))):
        it.add(cube(0.05, ww, 1.0 if k == 0 else 0.7), "Plastic_Black", loc=(sx - 0.22, -B / 2, zz))
    # Hinges on the opening side (+X corner of the hinge edge).
    for z in (24, 172):
        knuckle(it, "Steel", 2.3, 0.55, z, 13.0, 1.15)
        it.add(cube(0.5, 2.2, 6.0), "Steel", loc=(2.55, -0.6, z + 6.5), bevel=0.1, seg=1)  # welded tab
    # Overhead door closer (slide-rail type) on +X: body at the hinge side, arm along the top towards the lock side.
    fx = 2.5
    it.add(cube(5.0, 28.0, 6.2), "Aluminium", loc=(fx + 2.5, -20.5, 199.5), bevel=1.4, seg=4, grain="y")
    for yy in (-6.6, -34.4):
        it.add(cube(5.1, 0.8, 6.3), "Plastic_Grey", loc=(fx + 2.5, yy, 199.5), bevel=0.3, seg=2)  # end caps
    it.add(cube(0.6, 31.0, 2.4), "Plastic_Grey", loc=(fx + 0.3, -20.5, 199.5), bevel=0.15, seg=1)  # mounting plate edge
    it.add(cylinder(1.4, 1.2, 24), "Steel", loc=(fx + 5.0, -10.0, 201.0), rot=face_rot(1), shade="auto")  # spindle
    arm = [(fx + 6.6, -10.0, 201.0), (fx + 6.6, -30.0, 203.0), (fx + 6.6, -48.0, 204.6)]
    it.add(sweep(arm, rect_profile(2.0, 0.9, 0.2), up=(1, 0, 0)), "Aluminium", shade="auto")
    it.add(cube(2.4, 3.4, 2.4), "Plastic_Black", loc=(fx + 6.8, -48.5, 204.8), bevel=0.4, seg=2)  # slider
    # Lever sets: black long plates with lever and euro cylinder on both faces.
    ly, lz = -80.0, 105.0
    for side, face in ((-1, -2.5), (1, 2.5)):
        plate(it, "Plastic_Black", face, side, ly, lz - 3.4, 4.6, 26.0, 0.9, r=2.0)
        f = face + side * 0.9
        lever(it, "Plastic_Black", f, side, ly, lz, length=12.0, reach=5.8, thick=1.0, rose_r=1.6)
        profile_cylinder(it, f, side, ly, lz - 9.2)
    # Lock edge: faceplate of the mortise lock and the bolt.
    it.add(cube(0.25, 0.3, 24.0), "Steel", loc=(-0.6, -B - 0.1, lz - 4), bevel=0.05, seg=1)
    it.add(cube(1.2, 0.5, 1.8), "Steel", loc=(-0.6, -B - 0.2, lz), bevel=0.15, seg=1)
    # Fire door tag on the hinge edge.
    it.add(cube(0.2, 0.15, 4.0), "Aluminium", loc=(1.3, 0.07, 160.0))


# ---------------------------------------------------------------------------------------------
# Vault door
# ---------------------------------------------------------------------------------------------


def _dial(it, x, y, z, scale=1.0):
    """Combination dial facing -X: chrome ring with ticks, knurled knob, index mark."""
    s = scale
    it.add(lathe([(0, 0), (9.4 * s, 0), (9.4 * s, 0.9), (8.8 * s, 1.5), (0, 1.5)], 64), "Chrome", loc=(x, y, z), rot=face_rot(-1),
           shade="auto")
    for i in range(50):
        a = 360.0 * i / 50
        ln = 1.5 if i % 5 == 0 else 0.8
        r = 8.4 * s - ln / 2
        ar = math.radians(a)
        it.add(cube(0.12, 0.22, ln), "Plastic_Black", loc=(x - 1.55, y + r * math.cos(ar), z + r * math.sin(ar)), rot=radial_rot(a))
    it.add(lathe([(0, 0), (5.6 * s, 0), (5.6 * s, 2.8), (5.0 * s, 3.8), (1.8 * s, 4.3), (0, 4.3)], 60, ridges=(48, 0.3, 1, 2)),
           "Iron_Dark", loc=(x - 1.5, y, z), rot=face_rot(-1), shade="auto", angle=50)
    it.add(cylinder(1.2 * s, 0.4, 24), "Chrome", loc=(x - 5.8, y, z), rot=face_rot(-1), shade="auto")
    it.add(cube(0.5, 0.5, 1.8), "Brass", loc=(x - 1.3, y, z + 10.3 * s), bevel=0.1, seg=1)  # index mark


def _wheel(it, x, side, y, z, r_spoke, n_spokes, hub_r, rim_r=0.0, knob_len=7.5, bar=1.3, depth=6.5):
    """Spoked hand wheel on a face (x = face), spokes with turned grips, optional rim ring."""
    s = side
    it.add(lathe([(0, 0), (hub_r * 2.2, 0), (hub_r * 2.2, 0.8), (hub_r * 1.8, 1.8), (hub_r * 1.2, 2.3), (0, 2.3)], 48), "Chrome",
           loc=(x, y, z), rot=face_rot(s), shade="auto")
    it.add(lathe([(0, 0), (hub_r, 0), (hub_r, depth - 1.6), (hub_r * 1.25, depth - 1.0), (hub_r * 1.25, depth + 0.6),
                  (hub_r * 0.9, depth + 1.4), (0, depth + 1.6)], 32), "Chrome", loc=(x, y, z), rot=face_rot(s), shade="auto")
    px = x + s * depth
    for k in range(n_spokes):
        a = 90.0 + 360.0 * k / n_spokes
        ar = math.radians(a)
        p0 = (px, y + hub_r * 0.6 * math.cos(ar), z + hub_r * 0.6 * math.sin(ar))
        p1 = (px, y + r_spoke * math.cos(ar), z + r_spoke * math.sin(ar))
        it.add(sweep([p0, p1], circle_profile(bar, 16), up=(1, 0, 0)), "Chrome", shade="smooth")
        g = [(0, 0), (bar * 1.05, 0), (bar * 1.25, knob_len * 0.25), (bar * 1.75, knob_len * 0.62), (bar * 1.8, knob_len * 0.8),
             (bar * 1.45, knob_len * 0.96), (0, knob_len)]
        it.add(lathe(g, 20), "Chrome", loc=p1, rot=radial_rot(a), shade="smooth")
    if rim_r:
        it.add(torus(rim_r, bar * 0.85, 64, 12), "Chrome", loc=(px, y, z), rot=(0, 90, 0), shade="smooth")


@item("Door_vault")
def door_vault(it: Item):
    """Vault door 98 x 18 x 208: stepped solid steel leaf, big spoked wheel, combination dial, heavy hinges (-X),
    locking bolts on the edges, exposed bolt work on the inside (+X)."""
    B, H = 98.0, 208.0
    steel, chrome, iron = "Steel", "Chrome", "Iron_Dark"
    cy = -B / 2
    fx = -9.0  # show face
    # Stepped leaf: front plate over the full size, body behind it inset 3 cm all round.
    it.add(cube(6.0, B, H), steel, loc=(-6.0, cy, H / 2), bevel=0.9, seg=3, grain="y")                 # x -9 .. -3
    it.add(cube(12.4, B - 6, H - 6), "Paint_Anthracite", loc=(2.8, cy, H / 2), bevel=0.6, seg=3)    # x -3.4 .. 9
    # Raised chrome trim frame on the front with a row of rivets just outside it.
    m = 7.0
    path = [(0, -m, m), (0, -B + m, m), (0, -B + m, H - m), (0, -m, H - m)]
    trim = [(-1.3, 0.05), (1.3, 0.05), (1.0, -0.7), (-1.0, -0.7)]
    it.add(sweep(path, trim, up=(1, 0, 0), closed=True), chrome, loc=(fx, 0, 0), shade="auto", angle=30)
    rv = 3.6
    for k in range(5):
        yy = -rv - (B - 2 * rv) * k / 4
        for zz in (rv, H - rv):
            it.add(sphere(0.9, 10, 4, sz=0.55), chrome, loc=(fx, yy, zz), rot=face_rot(-1), shade="smooth")
    for k in range(1, 9):
        zz = rv + (H - 2 * rv) * k / 9
        for yy in (-rv, -B + rv):
            it.add(sphere(0.9, 10, 4, sz=0.55), chrome, loc=(fx, yy, zz), rot=face_rot(-1), shade="smooth")
    # Big wheel with five spokes and a rim, dial above, key lock near the lock edge, blank brass plate on top.
    wz = 100.0
    _wheel(it, fx, -1, cy, wz, r_spoke=25.0, n_spokes=5, hub_r=4.8, rim_r=19.5, knob_len=9.0, bar=1.75, depth=8.0)
    _dial(it, fx, cy, 158.0, scale=1.3)
    plate(it, chrome, fx, -1, -86.0, 132.0, 6.5, 13.0, 0.6, r=1.5)
    keyhole(it, fx - 0.6, -1, -86.0, 135.0, size=1.2, ring=iron)
    keyhole(it, fx - 0.6, -1, -86.0, 128.5, size=1.2, ring=iron)
    plate(it, "Brass", fx, -1, cy, 186.0, 30.0, 6.0, 0.5, r=0.8)
    plate(it, iron, fx - 0.5, -1, cy, 186.0, 27.0, 3.8, 0.08, r=0.5, bevel=0.0)
    # Heavy hinges on the show side: barrel, leaf on the door, bolt heads.
    for z0 in (22.0, 150.0):
        hh = 34.0
        knuckle(it, iron, -12.0, -2.0, z0, hh, 5.2, seg=32)
        it.add(cube(2.4, 24.0, hh - 4), iron, loc=(fx - 1.2, -14.5, z0 + hh / 2), bevel=0.5, seg=2)
        for dy in (-9.0, -22.0):
            for dz in (-9.0, 9.0):
                dome(it, chrome, fx - 2.4, -1, dy, z0 + hh / 2 + dz, r=1.3)
    # Locking bolts: four on the lock edge (thrown), three fixed ones on the hinge edge.
    for z in (34.0, 80.0, 128.0, 174.0):
        it.add(cylinder(2.7, 5.4, 32), chrome, loc=(3.0, -94.4, z), rot=axis_rot("-y"), bevel=0.4, seg=2)
    for z in (50.0, 104.0, 158.0):
        it.add(cylinder(2.2, 3.6, 32), chrome, loc=(3.0, -3.3, z), rot=axis_rot("+y"), bevel=0.35, seg=2)
    # Inside (+X): mechanism cover with screws, inner hand wheel, bolt bar with links.
    bx = 9.0
    it.add(cube(0.8, 64.0, 168.0), "Paint_Grey", loc=(bx + 0.4, -45.0, 104.0), bevel=0.3, seg=2, grain="z")
    for yy in (-15.5, -45.0, -74.5):
        for zz in (23.0, 63.0, 104.0, 145.0, 185.0):
            if yy == -45.0 and 63.0 <= zz <= 145.0:
                continue
            dome(it, chrome, bx + 0.8, 1, yy, zz, r=0.7)
    _wheel(it, bx + 0.8, 1, cy, wz, r_spoke=10.5, n_spokes=3, hub_r=2.6, knob_len=5.5, bar=0.9, depth=4.5)
    it.add(cube(2.6, 2.6, 150.0), chrome, loc=(bx + 1.3, -87.5, 104.0), bevel=0.4, seg=2, grain="z")  # bolt bar
    for z in (34.0, 80.0, 128.0, 174.0):
        it.add(cube(2.0, 7.0, 4.0), steel, loc=(bx + 1.0, -90.8, z), bevel=0.4, seg=2)
        dome(it, chrome, bx + 2.6, 1, -87.5, z, r=0.8)
    it.add(cube(1.2, 34.0, 3.2), chrome, loc=(bx + 1.8, -70.0, wz), bevel=0.3, seg=2)  # link wheel -> bolt bar
    it.add(cylinder(4.2, 1.6, 40), steel, loc=(bx + 0.8, cy, wz), rot=face_rot(1), bevel=0.3, seg=2)  # cam disc


# ---------------------------------------------------------------------------------------------
# Wrought-iron grate door
# ---------------------------------------------------------------------------------------------


def _s_scroll(cy, cz, d, e, r_out, r_in=0.6, turns=1.3):
    """S-shaped scroll in the door plane, point symmetric around (cy, cz)."""
    top = spiral_yz(cy + d, cz + e, r_out, r_in, turns, 180.0, cw=True)
    bottom = [(0.0, 2 * cy - p[1], 2 * cz - p[2]) for p in top]
    return list(reversed(top)) + bottom


@item("Door_grate")
def door_grate(it: Item):
    """Wine cellar grate door 98 x 4 x 208: square frame, vertical bars under an arch, S-scroll band, spirals in the
    spandrels, oval band at the bottom, box lock (+X) and lever on both sides."""
    B, H = 98.0, 208.0
    iron = "Iron_Dark"
    st = 4.0
    # Frame.
    for yc in (-st / 2, -B + st / 2):
        it.add(cube(4.0, st, H), iron, loc=(0, yc, H / 2), bevel=0.35, seg=2, grain="z")
    for zc in (st / 2, H - st / 2):
        it.add(cube(3.6, B - 2 * st + 0.4, st), iron, loc=(0, -B / 2, zc), bevel=0.35, seg=2)
    # Bars: seven square bars, equal gaps, ending in the arch.
    n, bw = 7, 1.6
    gap = (B - 2 * st - n * bw) / (n + 1)
    bars = [-st - gap * (k + 1) - bw * k - bw / 2 for k in range(n)]
    gaps = [-st - gap / 2 - (gap + bw) * k for k in range(n + 1)]
    acy, a_half, a_z0 = -B / 2, B / 2 - st, 172.0
    a_h = H - st - 0.7 - a_z0

    def arch_z(y):
        u = (y - acy) / a_half
        return a_z0 + a_h * math.sqrt(max(0.0, 1 - u * u))

    for y in bars:
        top = arch_z(y)
        it.add(cube(bw, bw, top - st + 0.4), iron, loc=(0, y, (top + st) / 2), bevel=0.15, seg=1, grain="z")
    # Arch (flat bar on edge).
    it.add(sweep(yz_ellipse(acy, a_z0, a_half + 0.2, a_h, 180, 0, 40), rect_profile(1.4, 2.4), up=(1, 0, 0)), iron, shade="auto")
    # Double flat-bar rails with rivets: lower rail, ornament band, middle rail.
    rails = (24.0, 98.0, 146.0, 170.0)
    for zr in rails:
        for sx in (-1, 1):
            it.add(cube(0.8, B - 2 * st + 0.2, 3.6), iron, loc=(sx * 1.2, -B / 2, zr), bevel=0.2, seg=1)
            for y in bars:
                it.add(sphere(0.55, 8, 3, sz=0.6), iron, loc=(sx * 1.6, y, zr), rot=face_rot(sx), shade="smooth")
    # S-scroll band between 146 and 170, ovals at the bottom between 4 and 22.
    zc = (rails[2] + rails[3]) / 2
    for i, y in enumerate(gaps):
        sc = _s_scroll(y, zc, 0.75, 4.9, 4.1, r_in=0.7, turns=1.55)
        if i % 2:
            sc = [(0.0, 2 * y - p[1], p[2]) for p in sc]  # mirror every other one
        it.add(sweep(sc, rect_profile(0.9, 1.8), up=(1, 0, 0)), iron, shade="auto", angle=50)
        oval = yz_ellipse(y, 13.0, gap / 2 + 0.2, 7.6, 0, 360, 32)[:-1]
        it.add(sweep(oval, rect_profile(0.9, 1.8), up=(1, 0, 0), closed=True), iron, shade="auto", angle=50)
    # Spandrel spirals (between frame, top rail and arch), mirrored.
    # The bar leaves the arch, runs down along it and curls up into the corner (hinge side; mirrored for the lock side).
    sp = spiral_yz(acy + a_half - 6.0, H - st - 6.0, 7.0, 1.0, 1.6, -146.0, cw=False, power=0.7)
    tail = [(0.0, y, arch_z(y) + 1.2) for y in (acy + 23.0, acy + 27.0, acy + 30.5)]
    scroll = spline(tail + [sp[0], sp[2]], 5)[:-6] + sp  # drop the helper segment sp[0] -> sp[2]
    for sgn in (1, -1):
        pts = scroll if sgn > 0 else [(0.0, 2 * acy - p[1], p[2]) for p in scroll]
        it.add(sweep(pts, rect_profile(1.0, 1.8), up=(1, 0, 0)), iron, shade="auto", angle=50)
        it.add(sphere(0.95, 12, 6), iron, loc=(0, pts[0][1], pts[0][2] - 0.95), shade="smooth")
    # Lock plate in the grate plane, box lock on +X, levers on both sides.
    ly, lz = -89.5, 110.0
    it.add(cube(1.0, 10.8, 30.0), iron, loc=(0, -88.9, 106.0), bevel=0.2, seg=1)
    it.add(cube(3.6, 13.0, 22.0), iron, loc=(2.4, -89.8, 106.0), bevel=0.5, seg=2)
    for zz in (97.0, 115.0):
        dome(it, iron, 4.2, 1, -84.6, zz, r=0.6)
        dome(it, iron, 4.2, 1, -95.0, zz, r=0.6)
    lever(it, iron, 4.2, 1, ly, lz, length=11.0, reach=5.0, thick=0.9, rose_r=1.8)
    keyhole(it, 4.2, 1, ly, 101.0, size=1.1)
    plate(it, iron, -0.5, -1, ly, 105.5, 6.0, 20.0, 0.5, r=2.2)
    lever(it, iron, -1.0, -1, ly, lz, length=11.0, reach=5.0, thick=0.9, rose_r=1.8)
    keyhole(it, -1.0, -1, ly, 101.0, size=1.1)
    # Hinge knuckles on the hinge stile.
    for z in (28.0, 172.0):
        knuckle(it, iron, 0.0, 0.6, z, 9.0, 1.6)


# ---------------------------------------------------------------------------------------------
# Slatted cellar door
# ---------------------------------------------------------------------------------------------


def _strap(it, z0, length=40.0, mat="Iron_Dark"):
    """Strap hinge on the -X face (on the slats), knuckle at the hinge edge."""
    # Outline in (z, y) around the strap axis; ngon_prism rotated so local x -> z, local y -> y, extrusion -> -X.
    pts = [(2.1, 0.3)]
    for a in range(0, 181, 20):  # rounded tip
        ar = math.radians(a)
        pts.append((1.4 * math.cos(ar), -length - 1.4 * math.sin(ar)))
    pts.append((-2.1, 0.3))
    it.add(ngon_prism(pts, 0.4), mat, loc=(-2.0, 0, z0), rot=axis_rot("-x"), bevel=0.1, seg=1)
    for y in (-5.0, -15.0, -26.0, -length + 1.0):
        dome(it, mat, -2.4, -1, y, z0, r=0.5)
    knuckle(it, mat, -1.8, 0.6, z0 - 4.5, 9.0, 1.05)


@item("Door_slatted")
def door_slatted(it: Item):
    """Cellar storage door 88 x 4 x 208: vertical slats with gaps (-X), battens and Z braces (+X), strap hinges,
    turn button and pull handles."""
    B, H = 88.0, 208.0
    wood = "Wood_Light"
    rng = random.Random(41)
    n, gap = 7, 3.0
    w = (B - (n - 1) * gap) / n
    slat_y = []
    for i in range(n):
        y_hi = -i * (w + gap)
        trim = 0.0 if i in (0, n - 1) else rng.uniform(0.0, 0.5)
        ww = w - trim
        yc = y_hi - w / 2 + (rng.uniform(-trim, trim) * 0.5 if trim else 0.0)
        if i == 0:
            yc = -ww / 2
        elif i == n - 1:
            yc = -B + ww / 2
        hh = H - (0.0 if i in (0, n - 1) else rng.uniform(0.0, 0.8))  # hand-cut tops
        it.add(cube(2.0, ww, hh), wood, loc=(-1.0, yc, hh / 2), bevel=0.35, seg=2, grain="z")
        slat_y.append(yc)
    # Battens and braces on the back.
    battens = (18.0, 104.0, 190.0)
    for zc in battens:
        it.add(cube(2.2, B - 2.0, 11.0), wood, loc=(1.1, -B / 2, zc), bevel=0.45, seg=2, grain="y")
    nails = [(yc, zc + dz) for zc in battens for yc in slat_y for dz in (-2.8, 2.8)]
    for z_lo, z_hi in ((battens[0], battens[1]), (battens[1], battens[2])):
        p0, p1 = Vector((0, -7.0, z_lo)), Vector((0, -B + 7.0, z_hi))
        d = p1 - p0
        ang = math.degrees(math.atan2(d.z, -d.y))
        mid = (p0 + p1) / 2
        it.add(cube(1.95, d.length, 9.0), wood, loc=(1.03, mid.y, mid.z), rot=(-ang, 0, 0), bevel=0.4, seg=2, grain="y")
        for yc in slat_y[1:-1]:
            zz = z_lo + (yc - p0.y) / d.y * d.z
            nails.append((yc, zz))
    # Nail heads on the show side where slats meet battens and braces.
    for yc, zz in nails:
        it.add(cylinder(0.32, 0.12, 8), "Iron_Dark", loc=(-2.0, yc + rng.uniform(-1.5, 1.5), zz), rot=face_rot(-1), shade="flat")
    # Strap hinges on the slats, at the outer battens.
    for zc in (battens[0], battens[2]):
        _strap(it, zc)
    # Turn button (Vorreiber) near the lock edge, turned up (open).
    it.add(cube(0.6, 2.2, 10.0), "Iron_Dark", loc=(-2.3, -84.0, 122.0), rot=(18, 0, 0), bevel=0.25, seg=2)
    it.add(cylinder(0.6, 0.5, 16), "Iron_Dark", loc=(-2.6, -84.0, 122.0), rot=face_rot(-1), bevel=0.1, seg=1)    # Pull handles: vertical on the slats (-X), horizontal on the middle batten (+X).
    pull_bar(it, "Iron_Dark", -2.0, -1, -80.0, 104.0, 15.0, vertical=True, standoff=2.6, r=0.7)
    pull_bar(it, "Iron_Dark", 2.2, 1, -77.0, battens[1], 14.0, vertical=False, standoff=2.6, r=0.7)


# ---------------------------------------------------------------------------------------------
# Bookshelf secret door
# ---------------------------------------------------------------------------------------------

COVERS = ["Leather_Brown", "Leather_Brown", "Leather_Brown", "Leather_Black", "Leather_Black", "Leather_Green", "Leather_Green",
          "Fabric_Red", "Fabric_Red", "Velvet_Red", "Fabric_Navy", "Fabric_Navy", "Fabric_Olive", "Fabric_Olive", "Fabric_Black",
          "Fabric_Grey", "Wool_Brown", "Wool_Brown", "Terracotta", "Paint_Green", "Fabric_Cream", "Linen_White", "Fabric_Blue"]
GILT = {"Leather_Brown", "Leather_Black", "Leather_Green", "Velvet_Red", "Fabric_Navy"}


def _book(it, centre, size, rot, cover, bands=0, label=None):
    """Book with the spine to local -X: cover box plus a page block that shows on top (head)."""
    R = Euler([math.radians(a) for a in rot]).to_matrix()
    c = Vector(centre)
    d, t, h = size

    def put(bm, mat, off, **kw):
        it.add(bm, mat, loc=tuple(c + R @ Vector(off)), rot=rot, **kw)

    put(cube(d, t, h), cover, (0, 0, 0), bevel=0.12, seg=1, grain="z")
    put(cube(d - 0.55, t - 0.34, h - 0.16), "Paper", (0.175, 0, 0.12))
    if bands:
        span = h * 0.78
        for k in range(bands):
            zk = -span / 2 + span * k / max(bands - 1, 1)
            put(cube(0.14, t * 0.9, 0.32), "Gilt", (-d / 2 - 0.04, 0, zk))
    if label:
        put(cube(0.1, t * 0.82, min(3.2, h * 0.14)), label, (-d / 2 - 0.03, 0, h * 0.22))


class _Shelf:
    """Fills one shelf compartment from y_start towards -Y."""

    def __init__(self, it, rng, z, clear, y_end, x_front=-26.4, max_depth=25.5):
        self.it, self.rng, self.z, self.clear, self.y_end = it, rng, z, clear, y_end
        self.x_front, self.max_depth = x_front, max_depth
        self.y = None
        self.last_h = 0.0

    def room(self):
        return self.y - self.y_end

    def stand(self, t, h, d, cover, bands=0, label=None, x_shift=0.0):
        if t + 0.15 > self.room():
            return False
        xf = self.x_front + self.rng.uniform(0.0, 0.9) + x_shift
        self.it_book((xf + d / 2, self.y - t / 2, self.z + h / 2), (d, t, h), (0, 0, self.rng.uniform(-0.5, 0.5)), cover, bands, label)
        self.y -= t + 0.15
        self.last_h = h
        return True

    def it_book(self, centre, size, rot, cover, bands, label):
        _book(self.it, centre, size, rot, cover, bands, label)

    def random_book(self, tall=False):
        r = self.rng
        cover = r.choice(COVERS)
        h = r.uniform(0.82, 0.96) * self.clear if tall else r.uniform(0.6, 0.93) * self.clear
        t = r.uniform(1.6, 4.8) if r.random() < 0.8 else r.uniform(4.8, 6.5)
        d = min(self.max_depth, h * r.uniform(0.66, 0.8))
        gilt = cover in GILT and r.random() < 0.75
        bands = r.choice((2, 3, 4, 5)) if gilt else 0
        label = r.choice(("Leather_Black", "Velvet_Red", "Leather_Green")) if gilt and r.random() < 0.5 else None
        return t, h, d, cover, bands, label

    def run(self, count, tall=False):
        for _ in range(count):
            if not self.stand(*self.random_book(tall)):
                while self.room() > 2.2:  # close the row with thin volumes
                    b = self.random_book(tall)
                    self.stand(min(self.room() - 0.4, self.rng.uniform(1.4, 2.8)), *b[1:])
                break

    def run_until(self, y_stop, tall=False):
        while True:
            b = self.random_book(tall)
            if self.y - b[0] - 0.15 < y_stop:
                break
            self.stand(*b)

    def series(self, count, cover, t, h, d, bands=4, label="Leather_Black"):
        for _ in range(count):
            if not self.stand(t + self.rng.uniform(-0.1, 0.1), h, d, cover, bands, label):
                break

    def gap(self, w):
        self.y -= min(w, max(self.room(), 0.0))

    def lean(self, angle=None):
        r = self.rng
        a = math.radians(angle if angle is not None else r.uniform(9, 16))
        t, h, d, cover, bands, label = self.random_book()
        h = min(h, self.last_h * 0.98 if self.last_h else h, (self.clear - t * math.sin(a)) / math.cos(a) - 0.3)
        d = min(d, h * 0.8)
        yc = self.y - h * math.sin(a)  # bottom corner on the +Y side
        need = h * math.sin(a) + t * math.cos(a) + 0.3
        if need > self.room():
            return
        xf = self.x_front + r.uniform(0.2, 0.8)
        centre = (xf + d / 2, yc - t / 2 * math.cos(a) + h / 2 * math.sin(a), self.z + t / 2 * math.sin(a) + h / 2 * math.cos(a))
        self.it_book(centre, (d, t, h), (-math.degrees(a), 0, 0), cover, bands, label)
        self.y = yc - t * math.cos(a) - 0.2
        self.last_h = 0.0

    def stack(self, count):
        """Books lying flat, spines to the front, largest at the bottom."""
        r = self.rng
        sizes = []
        h = r.uniform(24, 31)
        for _ in range(count):
            sizes.append((min(self.max_depth, h * r.uniform(0.68, 0.78)), r.uniform(2.0, 4.2), h, r.choice(COVERS)))
            h -= r.uniform(0.5, 3.5)
        width = sizes[0][2] + 1.0
        if width > self.room():
            return
        zc = self.z
        ymid = self.y - width / 2
        for d, t, h, cover in sizes:
            if zc + t > self.z + self.clear - 0.5:
                break
            xf = self.x_front + r.uniform(0.0, 1.2)
            gilt = cover in GILT and r.random() < 0.6
            self.it_book((xf + d / 2, ymid + r.uniform(-0.8, 0.8), zc + t / 2), (d, t, h), (90, 0, r.uniform(-4, 4)), cover,
                         3 if gilt else 0, None)
            zc += t
        self.y -= width + 0.2
        self.last_h = 0.0

    def vase(self):
        if self.room() < 13:
            return
        y = self.y - 6.5
        prof = [(0, 0), (3.6, 0), (4.2, 0.6), (5.2, 4), (5.4, 7), (4.4, 11), (2.4, 14.5), (2.0, 16.5), (2.6, 18.2), (2.7, 18.8),
                (2.2, 18.8), (1.7, 17.0), (1.6, 15.0), (0, 15.0)]
        self.it.add(lathe(prof, 32), "Porcelain_Blue", loc=(-17.0, y, self.z), shade="smooth", grain="z")
        self.y -= 13.0
        self.last_h = 0.0

    def box(self):
        if self.room() < 17:
            return
        y = self.y - 8.5
        self.it.add(cube(13.0, 16.0, 7.0), "Wood_Mahogany", loc=(-17.5, y, self.z + 3.5), bevel=0.3, seg=2)
        self.it.add(cube(13.4, 16.4, 1.6), "Wood_Mahogany", loc=(-17.5, y, self.z + 7.6), bevel=0.4, seg=2, profile=0.7)
        self.it.add(cube(0.3, 1.6, 1.4), "Brass", loc=(-24.1, y, self.z + 6.6), bevel=0.08, seg=1)
        self.y -= 17.0
        self.last_h = 0.0

    def bookend(self):
        if self.room() < 6:
            return
        y = self.y - 2.6
        self.it.add(cube(11.0, 5.0, 13.0), "Marble", loc=(-19.0, y, self.z + 6.5), bevel=0.6, seg=3)
        self.y -= 5.4
        self.last_h = 13.0


@item("Door_bookshelf")
def door_bookshelf(it: Item):
    """Secret door 88 x 30 x 208: walnut bookcase as the door leaf (x -27 .. +3), books to -X, plain back with
    hinges and pull, one book tilted out near the lock side."""
    B, H = 88.0, 208.0
    wood = "Wood_Walnut"
    xf, xb = -27.0, 3.0
    side = 2.0
    inner_y0, inner_y1 = -side, -B + side
    iw = B - 2 * side
    # Carcass: sides, back panel, plinth, bottom, top with frieze and cap, pilasters.
    for yc in (-side / 2, -B + side / 2):
        it.add(cube(28.0, side, H - 2.5), wood, loc=(-13.0, yc, (H - 2.5) / 2), bevel=0.3, seg=2, grain="z")
    it.add(cube(2.0, B, H - 2.5), wood, loc=(2.0, -B / 2, (H - 2.5) / 2), bevel=0.3, seg=2, grain="z")
    it.add(cube(1.8, iw, 8.0), wood, loc=(-25.5, -B / 2, 4.0), bevel=0.3, seg=2)
    bottom_top = 10.0
    it.add(cube(27.8, iw, 2.2), wood, loc=(-12.9, -B / 2, bottom_top - 1.1), bevel=0.3, seg=2)
    top_bottom = 199.0
    it.add(cube(27.0, iw, 2.2), wood, loc=(-12.5, -B / 2, top_bottom + 1.1), bevel=0.3, seg=2)
    it.add(cube(1.6, B, 6.6), wood, loc=(-26.8, -B / 2, top_bottom + 3.3), bevel=0.3, seg=2)  # frieze
    cove = [(0, 0), (0.0, -0.1), (0.9, 0.15), (1.4, 0.6), (1.6, 1.3), (0.0, 1.3)]
    it.add(sweep([(xf - 0.6, 0.0, top_bottom + 4.2), (xf - 0.6, -B, top_bottom + 4.2)], cove, up=(0, 0, 1)), wood, shade="auto",
           angle=40, grain="y")
    it.add(cube(31.2, B, 2.5), wood, loc=(-12.6, -B / 2, H - 1.25), bevel=0.7, seg=3, profile=0.7)  # cap, x -28.2 .. 3
    for yc in (-1.9, -B + 1.9):
        it.add(cube(1.1, 3.8, top_bottom - 8.0), wood, loc=(xf - 0.25, yc, (top_bottom + 8.0) / 2), bevel=0.4, seg=3, profile=0.3,
               grain="z")
    # Shelves: six compartments.
    n_comp, board = 6, 2.4
    clear = (top_bottom - bottom_top - (n_comp - 1) * board) / n_comp
    bases = [bottom_top + k * (clear + board) for k in range(n_comp)]
    for zb in bases[1:]:
        it.add(cube(27.6, iw, board), wood, loc=(-12.9, -B / 2, zb - board / 2), bevel=0.3, seg=2)
    # Books.
    rng = random.Random(1873)

    def shelf(k):
        s = _Shelf(it, rng, bases[k], clear, inner_y1 + 0.3)
        s.y = inner_y0 - 0.2
        return s

    s = shelf(0)
    s.run(5, tall=True)
    s.stack(3)
    s.run(20, tall=True)
    s = shelf(1)
    s.series(10, "Leather_Brown", 4.0, clear * 0.88, 19.0, bands=4, label="Leather_Black")
    s.gap(1.0)
    s.run(6)
    s.lean()
    s = shelf(2)
    s.run(9)
    s.vase()
    s.run(20)
    s = shelf(3)  # handle height: the tilted book is the secret latch
    s.run(5)
    s.stack(2)
    s.run_until(-74.5)
    lever_t, lever_h, lever_d, b_ang = 3.6, clear * 0.84, 19.0, 9.0
    s.y = -75.0
    # Tilted forward around its bottom spine edge (pivot at x = xp): top comes out to about the front plane.
    a = math.radians(b_ang)
    xp = xf + 4.6
    cx = xp - (-lever_d / 2 * math.cos(a) + lever_h / 2 * math.sin(a))
    cz = s.z + lever_d / 2 * math.sin(a) + lever_h / 2 * math.cos(a)
    _book(it, (cx, s.y - lever_t / 2, cz), (lever_d, lever_t, lever_h), (0, -b_ang, 0), "Leather_Green", 4, "Leather_Black")
    s.y -= lever_t + 0.3
    s.run(6)
    s = shelf(4)
    s.run(6)
    s.box()
    s.run(9)
    s.lean()
    s = shelf(5)
    s.stack(4)
    s.run(11)
    s.bookend()
    # Back (+X): plain panel, three hinges at the hinge edge, pull handle and a slide bolt near the lock edge.
    for z in (18.0, 100.0, 182.0):
        knuckle(it, "Iron_Dark", xb + 1.3, 0.3, z, 12.0, 1.3)
        plate(it, "Iron_Dark", xb, 1, -4.6, z + 6.0, 8.0, 10.0, 0.3, r=0.8)
        for dz in (-3.0, 3.0):
            dome(it, "Iron_Dark", xb + 0.3, 1, -6.0, z + 6.0 + dz, r=0.45)
    pull_bar(it, "Brass", xb, 1, -80.0, 105.0, 28.0, vertical=True, standoff=3.2, r=0.8)
    plate(it, "Iron_Dark", xb, 1, -82.0, 125.0, 9.0, 3.0, 0.4, r=0.6)
    it.add(cylinder(0.55, 9.0, 16), "Steel", loc=(xb + 0.95, -77.0, 125.0), rot=axis_rot("-y"), shade="smooth")
    it.add(cylinder(0.35, 2.2, 12), "Steel", loc=(xb + 0.95, -80.0, 125.0), rot=face_rot(1), shade="smooth")
