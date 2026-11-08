"""Villa windows (group G12): casement, floor-to-ceiling, tilted, frosted and cellar window plus the light well.
Exported as SM_Window_<name> and SM_Deco_lightwell.

Window rule (Docs/Villa/Villa-Modelle.md): origin = bottom centre of the wall opening, on the wall centre line.
Width along X (centred on 0), wall normal = Y, outside = -Y, outer wall 30 cm (y -15 .. +15). The model fills the
opening x -W/2 .. +W/2, z 0 .. H exactly (nothing sticks out sideways, so the level can scale X to the real width
and Z to the real height); only the sills reach below z = 0 and out of the wall (outer sill to y = -19, inner sill
to y = +18). The reveal (wall faces inside the opening) is built by the level.

Section through a closed window (y, cm):  -19 outer sill nose · -15 facade · -12 .. -4 outer frame (Blendrahmen)
· -11 .. -3 sash (Fluegel, its inner face 1 cm proud of the frame) · glass at -7.6 · +15 inner wall face ·
+18 inner sill nose.

Light well: origin = top centre on the outer wall face (y = 0 is the facade), the well reaches 60 cm out to -Y and
100 cm down, the grating's top is the ground at z = 0.
"""

import math
import random

import bmesh
from mathutils import Euler, Matrix, Vector

from dc_items import item
from dc_mesh import CM, Item, circle_profile, cube, cylinder, sphere, sweep, wobble

# Profiles (cm). y values are absolute (wall centre = 0, outside = -Y).
FY0, FY1 = -12.0, -4.0   # outer frame: outer face, inner face
SY0, SY1 = -11.0, -3.0   # sash: outer face, inner face
FW = 4.6                 # visible width of the outer frame
OV = 0.8                 # sash overlaps the frame opening by this much
SW = 6.2                 # sash width (incl. overlap)
MW = 7.0                 # mullion width
GT = 2.4                 # insulated glass thickness
GY = -7.6                # glass centre
BW = 1.6                 # glazing bead width
SILL_OUT = -19.0         # outer sill nose (4 cm in front of the facade)
SILL_IN = 18.0           # inner sill nose (3 cm in front of the inner wall face)
REVEAL_OUT = -15.0       # facade

FRAME = "Paint_White"


# ---------------------------------------------------------------------------------------------
# Geometry helpers
# ---------------------------------------------------------------------------------------------


def ring(x0, x1, z0, z1, prof):
    """Mitred rectangular frame in the XZ plane. The path is the rectangle (x0..x1, z0..z1);
    prof = [(d, y), ...]: d = distance inwards from the path (negative = outwards), y = absolute depth."""
    pts = [(x0, 0, z0), (x1, 0, z0), (x1, 0, z1), (x0, 0, z1)]
    return sweep(pts, prof, up=(0, 1, 0), closed=True)


def xsweep(prof_yz, x0, x1):
    """Extrude a closed (y, z) section along X from x0 to x1. Concave end caps are triangulated here so Unreal
    does not have to guess."""
    bm = sweep([(x0, 0, 0), (x1, 0, 0)], [(-y, z) for y, z in prof_yz], up=(0, 0, 1))
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 4])
    return bm


def box(x0, x1, y0, y1, z0, z1):
    """Axis-aligned box from corner ranges; returns (bmesh, centre)."""
    return cube(x1 - x0, y1 - y0, z1 - z0), ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)


def put(it: Item, bm, mat, xf=None, loc=(0, 0, 0), rot=(0, 0, 0), **kw):
    """it.add, optionally through a hinge transform xf = (pivot_cm, angle_deg): the part is first placed at
    loc/rot as if the sash were closed, then turned about the X axis through the pivot (top towards +Y)."""
    if xf is None:
        return it.add(bm, mat, loc=loc, rot=rot, **kw)
    pivot, ang = xf
    m = Matrix.Translation((Vector(loc) - Vector(pivot)) * CM) @ Euler([math.radians(a) for a in rot]).to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=m, verts=bm.verts)
    return it.add(bm, mat, loc=pivot, rot=(-ang, 0, 0), **kw)


def xf_point(xf, p):
    if xf is None:
        return tuple(p)
    pivot, ang = xf
    r = Euler((math.radians(-ang), 0, 0)).to_matrix()
    return tuple(Vector(pivot) + r @ (Vector(p) - Vector(pivot)))


# ---------------------------------------------------------------------------------------------
# Window parts
# ---------------------------------------------------------------------------------------------


FRAME_PROF = [(0, FY0), (FW - 0.7, FY0), (FW, FY0 + 0.7), (FW, FY1), (0, FY1)]
SASH_PROF = [(0, SY0), (SW - 0.9, SY0), (SW, SY0 + 0.9), (SW, SY1 - 0.5), (SW - 0.5, SY1), (0, SY1)]


def outer_frame(it: Item, W, H, mullions=(), drains=True):
    """Blendrahmen filling the opening, optional vertical mullions at the given x. Returns the clear bays."""
    it.add(ring(-W / 2, W / 2, 0, H, FRAME_PROF), FRAME, bevel=0.3, seg=2)
    edges = [-W / 2 + FW]
    for mx in sorted(mullions):
        bm, c = box(mx - MW / 2, mx + MW / 2, FY0, FY1, FW - 0.4, H - FW + 0.4)
        it.add(bm, FRAME, loc=c, bevel=0.3, seg=2, grain="z")
        edges += [mx - MW / 2, mx + MW / 2]
    edges.append(W / 2 - FW)
    bays = [(edges[i], edges[i + 1], FW, H - FW) for i in range(0, len(edges), 2)]
    for x0, x1, z0, z1 in bays:
        # Dark joint/gasket line on the frame's room side around each bay (shows when a sash is open).
        it.add(ring(x0, x1, z0, z1, [(-1.25, FY1 - 0.2), (-0.75, FY1 - 0.2), (-0.75, FY1 + 0.25), (-1.25, FY1 + 0.25)]),
               "Rubber_Black", shade="flat")
        if drains:  # drainage caps on the outside of the bottom frame member
            for k in (-1, 1):
                bm, c = box((x0 + x1) / 2 + k * (x1 - x0) * 0.3 - 1.4, (x0 + x1) / 2 + k * (x1 - x0) * 0.3 + 1.4,
                            FY0 - 0.35, FY0 + 0.1, 1.9, 2.7)
                it.add(bm, "Plastic_Grey", loc=c, bevel=0.15, seg=1)
    return bays


def glazing(it: Item, rect, y_in, glass, xf=None, embed=1.2):
    """Insulated pane in rect (clear opening of sash or frame) with outer gasket and inner glazing bead."""
    x0, x1, z0, z1 = rect
    bm, c = box(x0 - embed, x1 + embed, GY - GT / 2, GY + GT / 2, z0 - embed, z1 + embed)
    put(it, bm, glass, xf, loc=c, shade="flat")
    g_out, g_in = GY - GT / 2, GY + GT / 2
    put(it, ring(x0, x1, z0, z1, [(-0.3, g_out - 0.45), (0.45, g_out - 0.45), (0.45, g_out), (-0.3, g_out)]),
        "Rubber_Black", xf, shade="flat")
    put(it, ring(x0, x1, z0, z1, [(-0.3, g_in), (BW, g_in), (BW, g_in + 0.7), (0.45, y_in), (-0.3, y_in)]),
        FRAME, xf, bevel=0.12, seg=1)
    # Thin inner gasket where the bead meets the glass.
    put(it, ring(x0 + BW, x1 - BW, z0 + BW, z1 - BW, [(-0.05, g_in), (0.3, g_in), (0.3, g_in + 0.25), (-0.05, g_in + 0.25)]),
        "Rubber_Black", xf, shade="flat")


def olive(it: Item, x, z, direction="down", xf=None, y_face=SY1, mat="Aluminium"):
    """Window handle ('Olive') on the room side: rosette, neck and lever pointing down/up/left/right."""
    d = {"down": Vector((0, 0, -1)), "up": Vector((0, 0, 1)), "left": Vector((-1, 0, 0)), "right": Vector((1, 0, 0))}[direction]
    rose = (2.9, 1.1, 7.4) if d.z else (7.4, 1.1, 2.9)  # rosette long along the lever
    put(it, cube(*rose), mat, xf, loc=(x - d.x * 0.8, y_face + 0.5, z - d.z * 0.8), bevel=0.45, seg=3)
    put(it, cylinder(0.85, 1.4, 16), mat, xf, loc=(x, y_face + 0.9, z), rot=(-90, 0, 0), shade="smooth")
    c = Vector((x, y_face + 2.9, z))
    lift = Vector((0, 1, 0))
    pts = [c - d * 1.0, c + d * 2.0, c + d * 6.5 + lift * 0.35, c + d * 11.0 + lift * 0.15, c + d * 11.9, c + d * 12.4]
    put(it, sweep([tuple(p) for p in pts], circle_profile(1.0, 14, sx=1.08, sy=0.72), up=(0, 1, 0),
                  scales=[1.0, 0.82, 0.92, 1.08, 0.8, 0.35]), mat, xf, shade="smooth")


def hinge_cap(it: Item, x, z, xf=None, h=5.4):
    """Plastic cover over a corner hinge, spanning the frame/sash joint on the room side."""
    bm, c = box(x - 0.9, x + 0.9, FY1 - 0.1, SY1 + 0.45, z - h / 2, z + h / 2)
    put(it, bm, "Plastic_White", xf, loc=c, bevel=0.4, seg=2)


def sash(it: Item, bay, hinge="left", glass="Glass", handle="side", handle_dir="down", handle_z=None, tilt=0.0):
    """Turn/tilt sash in a frame bay. hinge: side of the hinges ('left', 'right', 'bottom').
    tilt: how far the top inner edge leans into the room (cm); 0 = closed. Returns the hinge transform."""
    x0, x1, z0, z1 = bay
    sx0, sx1, sz0, sz1 = x0 - OV, x1 + OV, z0 - OV, z1 + OV
    xf = None
    if tilt:
        ang = math.degrees(math.asin(tilt / (sz1 - sz0)))
        xf = ((0.0, SY1, sz0), ang)
    put(it, ring(sx0, sx1, sz0, sz1, SASH_PROF), FRAME, xf, bevel=0.35, seg=2)
    clear = (sx0 + SW, sx1 - SW, sz0 + SW, sz1 - SW)
    glazing(it, clear, SY1 - 0.6, glass, xf)
    hz = (sz0 + sz1) / 2 if handle_z is None else handle_z
    if handle == "side":
        hx = sx1 - SW / 2 if hinge in ("left", "bottom") else sx0 + SW / 2
        olive(it, hx, hz, handle_dir, xf)
    elif handle == "top":
        olive(it, (sx0 + sx1) / 2, sz1 - SW / 2, handle_dir, xf)
    if hinge in ("left", "right"):
        ex = sx0 if hinge == "left" else sx1
        hinge_cap(it, ex, sz0 + 4.0)
        hinge_cap(it, ex, sz1 - 4.0)
    else:  # bottom-hung: corner bearings at both bottom corners stay with the frame
        for ex in (sx0, sx1):
            bm, c = box(ex - 0.9 + (1.6 if ex == sx0 else -1.6), ex + 0.9 + (1.6 if ex == sx0 else -1.6),
                        FY1 - 0.1, SY1 + 0.45, sz0 - 0.4, sz0 + 3.6)
            it.add(bm, "Plastic_White", loc=c, bevel=0.4, seg=2)
    return xf, (sx0, sx1, sz0, sz1)


def fixed_glazing(it: Item, bay, glass="Glass"):
    """Pane set straight into the frame (no sash): only frame and bead are visible."""
    glazing(it, bay, FY1 - 0.4, glass)


def outer_sill(it: Item, W):
    """Anthracite aluminium sill: sloped top from the frame to the drip nose 4 cm in front of the facade,
    with raised end caps against the reveal."""
    n = SILL_OUT
    prof = [(-7.0, 0.0), (FY0, 0.0), (n + 0.9, -1.05), (n + 0.35, -1.2), (n, -1.6), (n, -4.3), (n + 0.35, -4.5),
            (n + 0.6, -4.3), (n + 0.6, -2.7), (n + 1.4, -2.5), (-7.0, -2.5)]
    it.add(xsweep(prof, -W / 2, W / 2), "Paint_Anthracite", bevel=0.12, seg=1, angle=50)
    for s in (-1, 1):
        cap = [(FY0 + 0.2, -0.4), (FY0 + 0.2, 1.6), (n - 0.1, 0.3), (n - 0.1, -1.9)]
        x_in = s * (W / 2 - 0.35)
        it.add(xsweep(cap, min(x_in, s * W / 2), max(x_in, s * W / 2)), "Paint_Anthracite", bevel=0.08, seg=1)


def inner_sill(it: Item, W, mat="Marble", thick=2.5):
    bm, c = box(-W / 2, W / 2, FY1 - 0.5, SILL_IN, -thick, 0.0)
    it.add(bm, mat, loc=c, bevel=0.6, seg=3, grain="x")


def frame_details(it: Item, W, H):
    """Fixing screws covered by caps on the room side of the frame (one row each side)."""
    for s in (-1, 1):
        for z in (H * 0.25, H * 0.75):
            it.add(cylinder(0.45, 0.2, 10), "Plastic_White", loc=(s * (W / 2 - FW * 0.45), FY1, z), rot=(-90, 0, 0),
                   shade="smooth")


# ---------------------------------------------------------------------------------------------
# Windows
# ---------------------------------------------------------------------------------------------


@item("Window_casement")
def window_casement(it: Item):
    """120 x 120 two-sash tilt/turn window with a fixed mullion, olive handles at the mullion, both sills."""
    W, H = 120.0, 120.0
    left, right = outer_frame(it, W, H, mullions=(0.0,))
    sash(it, left, hinge="left")
    sash(it, right, hinge="right")
    outer_sill(it, W)
    inner_sill(it, W)
    frame_details(it, W, H)


@item("Window_tall")
def window_tall(it: Item):
    """120 x 210 floor-to-ceiling window: fixed pane (-X) + tilt/turn sash (+X, handle at 105), thresholds
    instead of sills."""
    W, H = 120.0, 210.0
    fixed, door = outer_frame(it, W, H, mullions=(0.0,), drains=False)
    fixed_glazing(it, fixed)
    sash(it, door, hinge="right", handle_z=105.0)
    # Room side: stone threshold over the reveal floor up to the inner wall face.
    bm, c = box(-W / 2, W / 2, FY1 - 0.5, 15.0, -0.5, 1.2)
    it.add(bm, "Marble", loc=c, bevel=0.35, seg=2, grain="x")
    # Outside: low anthracite threshold with a drip edge, 4 cm in front of the facade.
    n = SILL_OUT
    prof = [(-8.0, 0.4), (FY0, 0.4), (n + 0.5, -0.25), (n, -0.6), (n, -2.2), (n + 0.5, -2.2), (n + 0.6, -1.5), (-8.0, -1.5)]
    it.add(xsweep(prof, -W / 2, W / 2), "Paint_Anthracite", bevel=0.1, seg=1, angle=50)
    # Drainage caps sit just above the threshold.
    for x in (-W / 4, W / 4):
        bm, c = box(x - 1.4, x + 1.4, FY0 - 0.35, FY0 + 0.1, 1.5, 2.3)
        it.add(bm, "Plastic_Grey", loc=c, bevel=0.15, seg=1)
    frame_details(it, W, H)


@item("Window_tilt")
def window_tilt(it: Item):
    """70 x 70 tilt/turn window, sash TILTED: its top leans 12.5 cm into the room (+Y), handle up, scissor
    stays bridging the gap on both sides near the top."""
    W, H = 70.0, 70.0
    (bay,) = outer_frame(it, W, H)
    xf, (sx0, sx1, sz0, sz1) = sash(it, bay, hinge="bottom", handle="side", handle_dir="up", tilt=12.5)
    outer_sill(it, W)
    inner_sill(it, W)
    frame_details(it, W, H)
    # Tilt stays (Kippscheren): an arm from the frame jamb's room face to the sash side, and a short link from
    # the sash top corner to the middle of that arm.
    for s in (-1, 1):
        x = sx1 + 0.3 if s > 0 else sx0 - 0.3
        a = (x, FY1 - 0.3, H - FW - 1.6)
        b = xf_point(xf, (x, SY1 - 2.0, sz1 - 15.0))
        c = xf_point(xf, (x, SY1 - 3.0, sz1 - 2.5))
        m = tuple((Vector(a) + Vector(b)) / 2)
        it.add(sweep([a, b], [(-0.8, -0.16), (0.8, -0.16), (0.8, 0.16), (-0.8, 0.16)], up=(1, 0, 0)), "Steel",
               bevel=0.1, seg=1)
        it.add(sweep([c, m], [(-0.65, -0.16), (0.65, -0.16), (0.65, 0.16), (-0.65, 0.16)], up=(1, 0, 0)), "Steel",
               loc=(s * 0.34, 0, 0), bevel=0.1, seg=1)
        for p in (a, b, c, m):
            it.add(cylinder(0.42, 0.9, 12), "Steel", loc=(p[0] - 0.45, p[1], p[2]), rot=(0, 90, 0), shade="smooth")
        # Bearing blocks the arms turn in.
        bm, cc = box(x - 0.5, x + 0.5, FY1 - 0.4, FY1 + 0.6, a[2] - 1.6, a[2] + 1.6)
        it.add(bm, "Plastic_White", loc=cc, bevel=0.25, seg=1)


@item("Window_frosted")
def window_frosted(it: Item):
    """100 x 70 single-sash tilt/turn window with frosted glass (bathroom), closed."""
    W, H = 100.0, 70.0
    (bay,) = outer_frame(it, W, H)
    sash(it, bay, hinge="left", glass="Glass_Frosted")
    outer_sill(it, W)
    inner_sill(it, W)
    frame_details(it, W, H)


@item("Window_cellar")
def window_cellar(it: Item):
    """100 x 70 bottom-hung cellar window (handle on the top rail), iron bar grille in the outer reveal in
    front of the glass, anthracite outer sill, concrete inner sill."""
    W, H = 100.0, 70.0
    (bay,) = outer_frame(it, W, H)
    sash(it, bay, hinge="bottom", handle="top", handle_dir="left")
    outer_sill(it, W)
    inner_sill(it, W, mat="Concrete", thick=3.0)
    frame_details(it, W, H)
    # Grille: flat-bar frame with vertical round bars and a middle cross bar, between facade (-15) and frame (-12).
    gy = -13.7
    gx0, gx1, gz0, gz1, band = -W / 2 + 0.4, W / 2 - 0.4, 0.7, H - 0.7, 3.4
    it.add(ring(gx0, gx1, gz0, gz1, [(0, gy - 0.4), (band, gy - 0.4), (band, gy + 0.4), (0, gy + 0.4)]), "Iron_Dark",
           bevel=0.15, seg=1)
    bm, c = box(gx0 + band - 0.2, gx1 - band + 0.2, gy - 0.4, gy + 0.4, H / 2 - band / 2, H / 2 + band / 2)
    it.add(bm, "Iron_Dark", loc=c, bevel=0.15, seg=1)
    inner = gx1 - gx0 - 2 * band
    n_bars = 7
    for i in range(n_bars):
        x = gx0 + band + inner * (i + 1) / (n_bars + 1)
        it.add(cylinder(0.8, gz1 - gz0 - 2 * band + 1.6, 12), "Iron_Dark", loc=(x, gy, gz0 + band - 0.8), shade="smooth")
    # Anchor bolts through the frame corners into the reveal.
    for x in (gx0 + band / 2, gx1 - band / 2):
        for z in (gz0 + band / 2, gz1 - band / 2):
            it.add(cylinder(0.75, 0.5, 6), "Steel", loc=(x, gy - 0.35, z), rot=(90, 0, 0), shade="flat")


# ---------------------------------------------------------------------------------------------
# Light well
# ---------------------------------------------------------------------------------------------


def _pebble(sy, sz):
    """Squash a sphere into a pebble (applied in metres, before placement)."""

    def apply(me):
        for v in me.vertices:
            v.co.y *= sy
            v.co.z *= sz

    return apply


@item("Deco_lightwell")
def lightwell(it: Item):
    """Concrete light well 110 x 60 x 100 in front of a cellar window, galvanised grating flush with the ground."""
    W, D, H, t = 110.0, 60.0, 100.0, 8.0
    conc = "Concrete"
    ix = W / 2 - t           # inner half width (47)
    iy = -D + t              # inner front face (-52)
    floor_z = -H + t         # top of the bottom slab (-92)
    bm, c = box(-W / 2, W / 2, -D, iy, -H, 0.0)
    it.add(bm, conc, loc=c, bevel=0.5, seg=2)
    for s in (-1, 1):
        bm, c = box(min(s * ix, s * W / 2), max(s * ix, s * W / 2), iy - 0.5, 0.0, -H, 0.0)
        it.add(bm, conc, loc=c, bevel=0.5, seg=2, grain="y")
    bm, c = box(-ix - 0.5, ix + 0.5, iy - 0.5, 0.0, -H, floor_z)
    it.add(bm, conc, loc=c, bevel=0.3, seg=1)
    # Gravel bed and drain.
    it.add(cube(2 * ix - 1.6, -iy - 1.6, 3.0, cuts=16), "Concrete", loc=(0, iy / 2 - 0.6, floor_z + 1.2), bevel=0.0,
           shade="smooth", post=wobble(0.5, 5.0, 3))
    rng = random.Random(12)
    for _ in range(60):
        r = rng.uniform(0.8, 1.8)
        px, py = rng.uniform(-ix + 3, ix - 3), rng.uniform(iy + 3, -3)
        if math.hypot(px, py - iy / 2) < 9:
            continue
        it.add(sphere(r, 10, 5), rng.choice(["Stone_Sand", "Stone_Sand", "Marble"]), loc=(px, py, floor_z + 2.2),
               rot=(0, 0, rng.uniform(0, 180)), shade="smooth", post=_pebble(rng.uniform(0.7, 0.9), rng.uniform(0.4, 0.55)))
    it.add(cylinder(6.5, 1.0, 32), "Iron_Dark", loc=(0, iy / 2, floor_z + 2.6), shade="auto")
    for k in range(-2, 3):
        bm, c = box(-4.6 + abs(k) * 0.9, 4.6 - abs(k) * 0.9, iy / 2 + k * 2.1 - 0.5, iy / 2 + k * 2.1 + 0.5,
                    floor_z + 3.5, floor_z + 3.7)
        it.add(bm, "Plastic_Black", loc=c, shade="flat")
    # Galvanised steel rim angles the grating rests on (front, sides, wall side), with a flange over the concrete.
    gz = -3.0
    for (x0, x1, y0, y1) in ((-ix, ix, iy, iy + 0.5), (-ix, -ix + 0.5, iy, 0.0), (ix - 0.5, ix, iy, 0.0), (-ix, ix, -0.5, 0.0)):
        bm, c = box(x0, x1, y0, y1, gz - 0.5, 0.0)
        it.add(bm, "Steel", loc=c)
    for (x0, x1, y0, y1) in ((-ix, ix, iy, iy + 3.0), (-ix, -ix + 3.0, iy, 0.0), (ix - 3.0, ix, iy, 0.0), (-ix, ix, -3.0, -0.5)):
        bm, c = box(x0, x1, y0, y1, gz - 0.5, gz)
        it.add(bm, "Steel", loc=c)
    for (x0, x1, y0, y1) in ((-ix - 2.5, ix + 2.5, iy - 2.5, iy), (-ix - 2.5, -ix, iy, 0.0), (ix, ix + 2.5, iy, 0.0)):
        bm, c = box(x0, x1, y0, y1, -0.3, 0.05)
        it.add(bm, "Steel", loc=c)
    # Grating: bearing bars along Y every 3.3 cm, twisted cross bars every 10 cm, edge band.
    gx0, gx1, gy0, gy1 = -ix + 0.6, ix - 0.6, iy + 0.6, -0.6
    for (x0, x1, y0, y1) in ((gx0, gx1, gy0, gy0 + 0.3), (gx0, gx1, gy1 - 0.3, gy1), (gx0, gx0 + 0.3, gy0, gy1), (gx1 - 0.3, gx1, gy0, gy1)):
        bm, c = box(x0, x1, y0, y1, gz, 0.0)
        it.add(bm, "Steel", loc=c)
    n = int((gx1 - gx0) / 3.3)
    for i in range(1, n):
        x = gx0 + (gx1 - gx0) * i / n
        bm, c = box(x - 0.15, x + 0.15, gy0 + 0.3, gy1 - 0.3, gz, 0.0)
        it.add(bm, "Steel", loc=c)
    m = int((gy1 - gy0) / 10.0)
    for j in range(1, m + 1):
        y = gy0 + (gy1 - gy0) * j / (m + 1)
        bm, c = box(gx0 + 0.3, gx1 - 0.3, y - 0.25, y + 0.25, -0.5, -0.02)
        it.add(bm, "Steel", loc=c)
