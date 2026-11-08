"""Villa G11 – door frames (Zargen) and large doors/gates. Exported as SM_<id>.

Frames (Door_frame_*) follow the frame rule: like the old 100 cm cube, origin in the middle,
post 16 (X) x 8 (Y) x 100 (Z), head 16 x 100 x 8. The game stretches them along their length, so the
cross-section stays the same along it (no end details). Profiles are symmetric in X and Y: casing on
both wall sides, a centred stop (the 4 cm leaf, centred on x = 0, closes against it) with rubber seals.

Leaves (Door_garage, Door_sliding_*, Gate_sliding) follow the leaf rule: origin at the bottom of the
edge y = 0, the leaf runs to y = -B, thickness centred on x = 0, z = 0 .. H, show side (outside) = -X.
"""

import math

import bmesh
from mathutils import Vector

from dc_items import item
from dc_mesh import CM, Item, circle_profile, cube, cylinder, lathe, rect_profile, sweep

# ---------------------------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------------------------


def rrect(x0, x1, y0, y1, r=(0.0, 0.0, 0.0, 0.0), n=4):
    """Convex rectangle profile (cm) with one radius per corner: (x0,y0), (x1,y0), (x1,y1), (x0,y1)."""
    if not isinstance(r, (tuple, list)):
        r = (r, r, r, r)
    corners = (((x0, y0), (1, 1), 180), ((x1, y0), (-1, 1), 270), ((x1, y1), (-1, -1), 0), ((x0, y1), (1, -1), 90))
    pts = []
    for ((cx, cy), (sx, sy), a0), rad in zip(corners, r):
        if rad <= 1e-6:
            pts.append((cx, cy))
            continue
        ox, oy = cx + sx * rad, cy + sy * rad
        for i in range(n + 1):
            a = math.radians(a0 + 90.0 * i / n)
            pts.append((ox + rad * math.cos(a), oy + rad * math.sin(a)))
    return pts


def bar(profile, axis, a0, a1):
    """Prism of a convex profile (u, v in cm) from a0 to a1 along `axis`.

    axis 'z': (u, v) = (x, y) · axis 'y': (u, v) = (x, z) · axis 'x': (u, v) = (y, z).
    """

    def place(u, v, a):
        if axis == "z":
            return (u, v, a)
        if axis == "y":
            return (u, a, v)
        return (a, u, v)

    bm = bmesh.new()
    lo = [bm.verts.new(Vector(place(u, v, a0)) * CM) for u, v in profile]
    hi = [bm.verts.new(Vector(place(u, v, a1)) * CM) for u, v in profile]
    n = len(profile)
    bm.faces.new(lo)
    bm.faces.new(hi)
    for i in range(n):
        bm.faces.new((lo[i], lo[(i + 1) % n], hi[(i + 1) % n], hi[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def mirror_x(profile):
    return [(-u, v) for u, v in reversed(profile)]


# ---------------------------------------------------------------------------------------------
# Door frames (Zargen)
# ---------------------------------------------------------------------------------------------


def _wood_frame_profile():
    """Cross-section of the wooden frame: (profile, material) in (u = X, v = across) cm, |u| <= 8, |v| <= 4."""
    parts = []
    # Casing (Bekleidung) on both wall sides: 1.6 cm board, rounded front edges.
    casing = rrect(6.4, 8.0, -4.0, 4.0, r=(0.15, 0.8, 0.8, 0.15))
    parts += [(casing, "Paint_White"), (mirror_x(casing), "Paint_White")]
    # Jamb lining (Futter) set back 0.8 cm behind the casing edge.
    parts.append((rrect(-6.5, 6.5, -3.2, 3.2), "Paint_White"))
    # Centred stop (Anschlag/Falz) the leaf closes against.
    parts.append((rrect(-2.6, 2.6, -4.0, 4.0, r=0.3), "Paint_White"))
    # Rubber seals in the rebate corners.
    for su in (-1, 1):
        for sv in (-1, 1):
            parts.append(([(su * 2.92 + u, sv * 3.5 + v) for u, v in circle_profile(0.42, 10)], "Rubber_Black"))
    return parts


def _steel_frame_profile():
    """Cross-section of the pressed-steel frame (Umfassungszarge): thin face flanges, stepped rebate, seals."""
    parts = []
    face = rrect(7.0, 8.0, -4.0, 4.0, r=(0.0, 0.2, 0.2, 0.0), n=2)
    parts += [(face, "Paint_Grey"), (mirror_x(face), "Paint_Grey")]
    step = rrect(5.4, 7.1, -3.45, 3.45, r=(0.0, 0.0, 0.0, 0.0))
    parts += [(step, "Paint_Grey"), (mirror_x(step), "Paint_Grey")]
    parts.append((rrect(-5.5, 5.5, -2.8, 2.8), "Paint_Grey"))
    # Stop as a folded sheet: chamfered corners.
    parts.append((rrect(-2.4, 2.4, -4.0, 4.0, r=0.4, n=1), "Paint_Grey"))
    for su in (-1, 1):
        for sv in (-1, 1):
            parts.append(([(su * 2.85 + u, sv * 3.2 + v) for u, v in circle_profile(0.55, 10)], "Rubber_Black"))
    return parts


def _frame_post(it: Item, parts):
    for prof, mat in parts:
        it.add(bar(prof, "z", -50.0, 50.0), mat, shade="auto", angle=40, grain="z")


def _frame_head(it: Item, parts):
    for prof, mat in parts:
        it.add(bar(prof, "y", -50.0, 50.0), mat, shade="auto", angle=40, grain="y")


@item("Door_frame_post")
def door_frame_post(it: Item):
    """Wooden frame post, white, origin in the middle: 16 (X) x 8 (Y) x 100 (Z), stretched along Z."""
    _frame_post(it, _wood_frame_profile())


@item("Door_frame_top")
def door_frame_top(it: Item):
    """Wooden frame head, white, origin in the middle: 16 (X) x 100 (Y) x 8 (Z), stretched along Y."""
    _frame_head(it, _wood_frame_profile())


@item("Door_frame_metal_post")
def door_frame_metal_post(it: Item):
    """Steel frame post (steel and vault doors), grey, 16 x 8 x 100, origin in the middle."""
    _frame_post(it, _steel_frame_profile())


@item("Door_frame_metal_top")
def door_frame_metal_top(it: Item):
    """Steel frame head, grey, 16 x 100 x 8, origin in the middle."""
    _frame_head(it, _steel_frame_profile())


# ---------------------------------------------------------------------------------------------
# Sectional garage door
# ---------------------------------------------------------------------------------------------


def _garage_hinge(it, y, z, x0):
    """Section hinge on the inside face x0 across the joint at height z."""
    for dz in (-1, 1):
        it.add(cube(0.35, 4.4, 9.0), "Steel", loc=(x0 + 0.17, y, z + dz * 5.0), bevel=0.12, seg=1)
    it.add(cylinder(0.7, 4.8, 12), "Steel", loc=(x0 + 0.7, y + 2.4, z), rot=(90, 0, 0), shade="smooth")


def _garage_roller(it, y_edge, sgn, z, x0):
    """Roller holder at a side edge: plate on the inside face, tab and a nylon roller at the edge.

    sgn = +1 for the edge at y = 0 (holder reaches towards -Y), -1 for the edge at y = -B.
    """
    it.add(cube(0.45, 6.0, 12.0), "Steel", loc=(x0 + 0.22, y_edge - sgn * 4.0, z), bevel=0.15, seg=1)
    it.add(cube(2.6, 0.45, 5.0), "Steel", loc=(x0 + 1.3, y_edge - sgn * 2.4, z), bevel=0.1, seg=1)
    # Shaft from the tab to the roller, axis along Y.
    shaft_y0 = y_edge - sgn * 0.3
    it.add(cylinder(0.4, 2.0, 10), "Steel", loc=(x0 + 2.3, shaft_y0 if sgn > 0 else shaft_y0 + 2.0, z), rot=(90, 0, 0), shade="smooth")
    # Roller (axis along Y) right at the edge.
    roller_y0 = y_edge - sgn * 0.25
    it.add(lathe([(0, 0), (1.5, 0), (1.75, 0.25), (1.75, 1.05), (1.5, 1.3), (0, 1.3)], 20), "Plastic_Black",
           loc=(x0 + 2.3, roller_y0 if sgn > 0 else roller_y0 + 1.3, z), rot=(90, 0, 0), shade="auto", angle=40)


@item("Door_garage")
def door_garage(it: Item):
    """Sectional garage door 498 x 5 x 218: four white sections with grooves, handle at the bottom.

    Leaf rule: y = 0 .. -498, x = -2.5 .. 2.5 (outside -X), z = 0 .. 218.
    """
    B, H = 498.0, 218.0
    white = "Paint_White"
    seal_h = 1.2
    n_sec = 4
    sec_h = (H - seal_h) / n_sec
    cy = -B / 2
    x_in = 2.5  # inside face
    # Bottom rubber seal (bulb).
    it.add(bar(rrect(-2.1, 2.1, 0.0, seal_h + 0.8, r=(0.9, 0.9, 0.2, 0.2)), "y", -B + 0.4, -0.4), "Rubber_Black",
           shade="auto", angle=40)
    joints = []
    for k in range(n_sec):
        z0 = seal_h + k * sec_h
        z1 = z0 + sec_h
        top = k == n_sec - 1
        g0, g1 = 0.15, (0.0 if top else 0.15)
        # Section core (4 cm sandwich panel), rounded at the joints on both faces.
        core_h = sec_h - g0 - g1
        it.add(cube(4.1, B, core_h), white, loc=(0.45, cy, z0 + g0 + core_h / 2), bevel=0.7, seg=2, grain="y")
        # Outside skin: two raised fields per section, the groove between them is the bead (Sicke).
        half = sec_h / 2
        for j in range(2):
            s0 = z0 + j * half + (0.6 if j == 0 else 0.45)
            s1 = z0 + (j + 1) * half - (0.45 if j == 0 else (0.25 if top else 0.6))
            it.add(cube(1.25, B - 0.6, s1 - s0), white, loc=(-1.87, cy, (s0 + s1) / 2), bevel=0.5, seg=2, grain="y")
        if k:
            joints.append(z0)
    # Dark finger-protection joints between the sections.
    for zj in joints:
        it.add(cube(3.6, B - 1.0, 0.45), "Rubber_Black", loc=(0.4, cy, zj))
    # Top seal lip.
    it.add(bar(rrect(-2.4, -0.6, H - 0.9, H, r=(0.0, 0.0, 0.4, 0.4)), "y", -B + 0.6, -0.6), "Rubber_Black", shade="auto", angle=40)

    # Handle (bottom section, centred) – outside: grip shell with a lock cylinder.
    hz = 36.0  # on the upper field of the bottom section, clear of the hinges above
    it.add(cube(0.8, 26.0, 9.0), "Plastic_Black", loc=(-2.9, cy, hz), bevel=0.35, seg=2)
    grip = [(-3.2, cy - 10.0, hz - 0.5), (-5.0, cy - 8.0, hz - 0.5), (-5.0, cy + 2.0, hz - 0.5), (-3.2, cy + 4.0, hz - 0.5)]
    it.add(sweep(grip, rect_profile(2.2, 1.4, 0.4), up=(1, 0, 0)), "Plastic_Black", shade="auto", angle=40)
    it.add(cylinder(1.3, 0.6, 24), "Chrome", loc=(-3.2, cy + 8.5, hz), rot=(0, -90, 0), shade="auto", angle=40)
    it.add(cube(0.12, 0.25, 1.2), "Plastic_Black", loc=(-3.82, cy + 8.5, hz - 0.1))  # keyway
    # Inside: handle with locking bars running out to both side edges.
    it.add(cube(0.8, 24.0, 8.0), "Plastic_Black", loc=(x_in + 0.4, cy, hz), bevel=0.35, seg=2)
    grip_in = [(x_in + 0.7, cy - 8.0, hz), (x_in + 3.0, cy - 6.5, hz), (x_in + 3.0, cy + 6.5, hz), (x_in + 0.7, cy + 8.0, hz)]
    it.add(sweep(grip_in, circle_profile(0.7, 12), up=(1, 0, 0)), "Plastic_Black", shade="smooth")
    bar_len = B / 2 - 12.0 - 7.0
    for y_start in (-7.0, cy - 12.0):
        it.add(cylinder(0.45, bar_len, 10), "Steel", loc=(x_in + 1.3, y_start, hz), rot=(90, 0, 0), shade="smooth")
    for yg in (-40.0, -130.0, cy + 70.0, -B + 40.0, -B + 130.0, cy - 70.0):
        it.add(cube(1.6, 2.4, 3.2), "Steel", loc=(x_in + 0.8, yg, hz), bevel=0.2, seg=1)
    for y_end, sgn in ((-3.5, 1), (-B + 3.5, -1)):
        it.add(cube(1.8, 5.0, 4.2), "Steel", loc=(x_in + 0.9, y_end - sgn * 1.5, hz), bevel=0.25, seg=1)  # bolt housing

    # Inside hardware: hinges across every joint, roller holders at both edges.
    for zj in joints:
        for yh in (-B * 0.25, cy, -B * 0.75):
            _garage_hinge(it, yh, zj, x_in)
    for z in [seal_h + 9.0] + joints + [H - 9.0]:
        for y_edge, sgn in ((0.0, 1), (-B, -1)):
            _garage_roller(it, y_edge, sgn, z, x_in)


# ---------------------------------------------------------------------------------------------
# Lift-and-slide door (Hebeschiebetür)
# ---------------------------------------------------------------------------------------------

HST_W, HST_H = 300.0, 220.0
SASH_W, SASH_D, SASH_H = 150.0, 6.0, 216.0
SILL_TOP = 2.0
TRACK_OUT = -3.6  # fixed sash (outer track)
TRACK_IN = 3.6  # sliding sash (inner track, room side)
SLIDE_CLOSED_Y = -1.5  # sliding sash origin when closed (its edge sits in the left jamb)
FIXED_Y = -148.5  # fixed sash origin (spans -148.5 .. -298.5)


def _hst_sash(it: Item, x0=0.0, y0=0.0, z0=0.0, handle=True):
    """One sash 150 x 6 x 216: white profiles, double glazing with beads and gaskets.

    Local origin: left edge (y = 0) bottom, runs to y = -150, thickness centred on x = 0.
    """
    white = "Paint_White"
    W, H = SASH_W, SASH_H
    stile, rail_b, rail_t = 11.0, 12.0, 9.0
    for ys in (y0 - stile / 2, y0 - W + stile / 2):
        it.add(cube(SASH_D, stile, H), white, loc=(x0, ys, z0 + H / 2), bevel=0.4, seg=2, grain="z")
    inner_w = W - 2 * stile
    it.add(cube(SASH_D, inner_w, rail_b), white, loc=(x0, y0 - W / 2, z0 + rail_b / 2), bevel=0.4, seg=2, grain="y")
    it.add(cube(SASH_D, inner_w, rail_t), white, loc=(x0, y0 - W / 2, z0 + H - rail_t / 2), bevel=0.4, seg=2, grain="y")
    # Glass opening.
    gy0, gy1 = y0 - stile, y0 - W + stile
    gz0, gz1 = z0 + rail_b, z0 + H - rail_t
    gw, gh = gy0 - gy1, gz1 - gz0
    gcy, gcz = (gy0 + gy1) / 2, (gz0 + gz1) / 2
    it.add(cube(2.6, gw + 1.0, gh + 1.0), "Glass", loc=(x0, gcy, gcz), shade="flat")
    bead, bd = 1.7, 1.1
    for sx in (-1, 1):
        bx = x0 + sx * (1.3 + bd / 2)
        # Glazing beads (Glasleisten) on both faces, slightly behind the sash face.
        it.add(cube(bd, gw, bead), white, loc=(bx, gcy, gz0 + bead / 2), bevel=0.25, seg=1, grain="y")
        it.add(cube(bd, gw, bead), white, loc=(bx, gcy, gz1 - bead / 2), bevel=0.25, seg=1, grain="y")
        it.add(cube(bd, bead, gh - 2 * bead), white, loc=(bx, gy0 - bead / 2, gcz), bevel=0.25, seg=1, grain="z")
        it.add(cube(bd, bead, gh - 2 * bead), white, loc=(bx, gy1 + bead / 2, gcz), bevel=0.25, seg=1, grain="z")
        # Black gasket line between bead and glass.
        gx = x0 + sx * 1.45
        g = 0.45
        it.add(cube(0.3, gw - 2 * bead, g), "Rubber_Black", loc=(gx, gcy, gz0 + bead + g / 2))
        it.add(cube(0.3, gw - 2 * bead, g), "Rubber_Black", loc=(gx, gcy, gz1 - bead - g / 2))
        it.add(cube(0.3, g, gh - 2 * bead), "Rubber_Black", loc=(gx, gy0 - bead - g / 2, gcz))
        it.add(cube(0.3, g, gh - 2 * bead), "Rubber_Black", loc=(gx, gy1 + bead + g / 2, gcz))
    if not handle:
        return
    # Lift-and-slide lever on the room side (+X) of the leading stile (y = 0 edge), lever down = closed.
    hy = y0 - 7.5  # clear of the jamb flange (6 cm) when closed
    hz = z0 + 108.0
    xf = x0 + SASH_D / 2
    it.add(cube(1.0, 4.4, 24.0), "Steel", loc=(xf + 0.5, hy, hz), bevel=0.4, seg=2)
    it.add(cylinder(1.5, 2.4, 20), "Steel", loc=(xf + 1.0, hy, hz + 7.0), rot=(0, 90, 0), shade="auto", angle=40)
    lever = [(xf + 3.0, hy, hz + 7.0), (xf + 4.0, hy, hz - 3.0), (xf + 4.2, hy, hz - 22.0)]
    it.add(sweep(lever, rect_profile(1.5, 2.2, 0.5), up=(0, 1, 0)), "Steel", shade="auto", angle=40)
    # Outside (-X): flush pull shell and a lock cylinder.
    xo = x0 - SASH_D / 2
    it.add(cube(0.5, 4.2, 15.0), "Aluminium", loc=(xo - 0.25, hy, hz - 2.0), bevel=0.2, seg=2)
    it.add(cube(0.2, 2.6, 10.0), "Plastic_Black", loc=(xo - 0.55, hy, hz - 2.0), bevel=0.08, seg=1)
    it.add(cylinder(1.1, 0.6, 20), "Chrome", loc=(xo, hy, hz + 9.5), rot=(0, -90, 0), shade="auto", angle=40)
    it.add(cube(0.12, 0.22, 1.0), "Plastic_Black", loc=(xo - 0.62, hy, hz + 9.4))


@item("Door_sliding_panel")
def door_sliding_panel(it: Item):
    """Sliding sash of the lift-and-slide door: 150 x 6 x 216, origin at the left edge bottom (y 0 .. -150).

    In the frame it runs in the inner track: x = +3.6, z = 2.0; closed y = -1.5, open y = -148.5.
    """
    _hst_sash(it, handle=True)


@item("Door_sliding_frame")
def door_sliding_frame(it: Item):
    """Frame of the lift-and-slide door: 300 x 15 x 220 with the fixed sash in the outer track.

    Origin at the left edge bottom (y 0 .. -300), x -7.5 .. 7.5 (outside -X). The fixed sash covers
    y -148.5 .. -298.5 at x = -3.6; the sliding sash (separate model) runs at x = +3.6 in front of it.
    """
    white = "Paint_White"
    W, H, D = HST_W, HST_H, 15.0
    head_z = 210.0
    fl = 0.7  # flange thickness
    # Head: web on top, outer/inner flanges and the divider between the tracks.
    it.add(cube(D, W, 2.2), white, loc=(0, -W / 2, H - 1.1), bevel=0.3, seg=2, grain="y")
    for x in (-D / 2 + fl / 2, D / 2 - fl / 2):
        it.add(cube(fl, W, H - head_z), white, loc=(x, -W / 2, (head_z + H) / 2), bevel=0.25, seg=2, grain="y")
    it.add(cube(0.8, W, 218.0 - head_z + 0.2), white, loc=(0, -W / 2, (head_z + 218.2) / 2), bevel=0.2, seg=1, grain="y")
    # Jambs: U-channel with the divider, the sash edges slide into it.
    for y_out, sgn in ((0.0, 1), (-W, -1)):
        jc = y_out - sgn * 3.0
        for x in (-D / 2 + fl / 2, D / 2 - fl / 2):
            it.add(cube(fl, 6.0, head_z), white, loc=(x, jc, head_z / 2), bevel=0.25, seg=2, grain="z")
        it.add(cube(D, 1.5, H), white, loc=(0, y_out - sgn * 0.75, H / 2), bevel=0.3, seg=2, grain="z")
        it.add(cube(0.8, 6.0, head_z), white, loc=(0, jc, head_z / 2), bevel=0.2, seg=1, grain="z")
    # Sill: flat aluminium threshold with a rail per track.
    it.add(cube(D, W - 3.0, 0.8), "Aluminium", loc=(0, -W / 2, 0.4), bevel=0.2, seg=1, grain="y")
    it.add(bar(rrect(-7.5, -6.0, 0.0, 1.4, r=(0.0, 0.0, 0.3, 0.6)), "y", -W + 1.5, -1.5), "Aluminium", shade="auto", angle=40)
    it.add(bar(rrect(TRACK_OUT - 1.0, TRACK_OUT + 1.0, 0.6, SILL_TOP), "y", -W + 1.5, -1.5), "Aluminium", shade="auto", angle=40)
    it.add(bar(rrect(TRACK_IN - 0.9, TRACK_IN + 0.9, 0.6, SILL_TOP, r=(0.0, 0.0, 0.8, 0.8)), "y", -W + 1.5, -1.5), "Aluminium",
           shade="auto", angle=40)
    it.add(cube(1.2, W - 3.0, 0.3), "Rubber_Black", loc=(0, -W / 2, 0.95))  # seal strip between the tracks
    # Fixed sash in the outer track.
    _hst_sash(it, x0=TRACK_OUT, y0=FIXED_Y, z0=SILL_TOP, handle=False)


# ---------------------------------------------------------------------------------------------
# Sliding driveway gate
# ---------------------------------------------------------------------------------------------


@item("Gate_sliding")
def gate_sliding(it: Item):
    """Ground-guided sliding gate 400 x 6 x 180, anthracite with vertical bars, rollers on a ground rail.

    Leaf rule: y = 0 (tail end) .. -400 (closing edge), x -3 .. 3, z 0 .. 180. Gear rack on the inside (+X).
    """
    B, H = 400.0, 180.0
    paint = "Paint_Anthracite"
    beam_z0, beam_z1 = 13.0, 23.0
    rail_z0 = H - 6.0
    lead = -B + 2.0  # closing edge of the steel frame (rubber bumper in front)
    # Ground rail: flat bar with a round running bar on top.
    it.add(bar(rrect(-2.5, 2.5, 0.0, 0.8, r=(0.0, 0.0, 0.2, 0.2)), "y", -B, 0.0), "Steel", shade="auto", angle=40)
    it.add(bar([(u, v + 1.8) for u, v in circle_profile(1.0, 12)], "y", -B, 0.0), "Steel", shade="auto", angle=40)
    # Frame: bottom beam (10 x 6), top rail (6 x 6), end stiles (6 x 6).
    it.add(cube(6.0, -lead - 0.8, beam_z1 - beam_z0), paint, loc=(0, lead / 2, (beam_z0 + beam_z1) / 2), bevel=0.35, seg=2, grain="y")
    it.add(cube(6.0, -lead - 0.8, 6.0), paint, loc=(0, lead / 2, rail_z0 + 3.0), bevel=0.35, seg=2, grain="y")
    for yc in (-3.0, lead + 3.0):
        it.add(cube(6.0, 6.0, rail_z0 - beam_z1 + 0.4), paint, loc=(0, yc, (beam_z1 + rail_z0) / 2), bevel=0.35, seg=2, grain="z")
    # Tube end caps.
    for yc in (-0.2, lead + 0.2):
        for zc, h in (((beam_z0 + beam_z1) / 2, beam_z1 - beam_z0 - 0.6), (rail_z0 + 3.0, 5.4)):
            it.add(cube(5.4, 0.4, h), "Plastic_Black", loc=(0, yc, zc), bevel=0.12, seg=1)
    # Vertical bars, 2.4 cm square, about 9 cm clear gap.
    y_in0, y_in1 = -6.0, lead + 6.0
    n = 33
    gap = ((y_in0 - y_in1) - n * 2.4) / (n + 1)
    for i in range(n):
        yb = y_in0 - gap * (i + 1) - 2.4 * i - 1.2
        it.add(cube(2.4, 2.4, rail_z0 - beam_z1 + 0.4), paint, loc=(0, yb, (beam_z1 + rail_z0) / 2), bevel=0.2, seg=1, grain="z")
    # Rubber bumper on the closing edge.
    it.add(bar(rrect(-2.4, 2.4, -B, lead + 0.1, r=(0.9, 0.9, 0.0, 0.0)), "z", beam_z1, rail_z0), "Rubber_Black",
           shade="auto", angle=40)
    # Running gear: two V-groove wheels in housings under the bottom beam.
    wheel_r = 5.0
    wz = 2.8 + 3.9 - 0.3
    wheel = [(0, -1.6), (4.6, -1.6), (wheel_r, -1.25), (wheel_r, -0.85), (3.9, 0.0), (wheel_r, 0.85), (wheel_r, 1.25), (4.6, 1.6), (0, 1.6)]
    for yw in (-32.0, -B + 32.0):
        it.add(lathe(wheel, 32), "Steel", loc=(0, yw, wz), rot=(0, 90, 0), shade="auto", angle=40)
        it.add(cylinder(1.6, 3.8, 16), "Iron_Dark", loc=(-1.9, yw, wz), rot=(0, 90, 0), shade="auto", angle=40)  # hub
        for sx in (-1, 1):
            it.add(cube(0.6, 13.0, beam_z0 - 1.0 - (wz - 2.0)), paint, loc=(sx * 2.35, yw, (beam_z0 - 1.0 + wz - 2.0) / 2), bevel=0.2, seg=1)  # housing cheeks
            it.add(cylinder(0.95, 0.6, 6), "Iron_Dark", loc=(sx * 2.65 - 0.3, yw, wz), rot=(0, 90, 0), shade="flat")  # axle nut
        it.add(cube(6.0, 14.0, 1.1), paint, loc=(0, yw, beam_z0 - 0.5), bevel=0.2, seg=1)  # housing top plate
    # Gear rack along the inside of the bottom beam (teeth point down to the drive pinion).
    rack_x = 3.0 + 0.7
    rack_y0, rack_y1 = -8.0, lead + 8.0
    it.add(cube(1.4, rack_y0 - rack_y1, 2.6), "Plastic_Black", loc=(rack_x, (rack_y0 + rack_y1) / 2, beam_z0 + 1.6), bevel=0.15, seg=1)
    pitch = 2.0
    nt = int((rack_y0 - rack_y1) / pitch)
    for i in range(nt):
        it.add(cube(1.4, 0.9, 1.0), "Plastic_Black", loc=(rack_x, rack_y0 - pitch * (i + 0.5), beam_z0 - 0.2))
    # Lock box with emergency release on the inside of the closing stile.
    it.add(cube(1.2, 4.6, 14.0), "Plastic_Black", loc=(3.6, lead + 3.0, 105.0), bevel=0.3, seg=2)
    it.add(cylinder(0.9, 0.4, 16), "Chrome", loc=(4.2, lead + 3.0, 109.0), rot=(0, 90, 0), shade="auto", angle=40)
