"""Villa stairs and attachments (group G17): hall stair, cellar stair, outside cellar stair, glass canopy and a
security camera. Exported as SM_Stair_<name> and SM_Deco_<name>.

Stair rule (Docs/Villa/Villa-Modelle.md): origin = middle of the lowest step's front edge on the lower floor
(x = 0, y = 0, z = 0). The stair climbs towards +Y, its width runs along X (-B/2 .. +B/2). The model ends exactly at
y = run and z = rise; the upper floor joins there. The game has no collision on these meshes (the level keeps
invisible step boxes), so they only have to look right and hit the sizes.

Step layout (same as the level's step boxes): n risers, n - 1 treads. The last riser lifts onto a short exit
nosing (Austrittskante) at floor height that ends at y = run:

    nosing i (i = 1 .. n)   y_i = (i - 1) * g,   top z_i = i * r,   r = rise / n,   g = (run - exit) / (n - 1)

Wall parts (canopy, camera): the back lies on the wall at y = 0, the part reaches out towards -Y.
"""

import math

import bmesh
from mathutils import Euler, Matrix, Vector

from dc_items import item
from dc_mesh import CM, Item, circle_profile, cube, cylinder, lathe, rect_profile, sphere, sweep

# ---------------------------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------------------------


def box(x0, x1, y0, y1, z0, z1):
    """Axis-aligned box from corner ranges; returns (bmesh, centre)."""
    return cube(x1 - x0, y1 - y0, z1 - z0), ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)


def add_box(it: Item, mat, x0, x1, y0, y1, z0, z1, **kw):
    bm, c = box(x0, x1, y0, y1, z0, z1)
    return it.add(bm, mat, loc=c, **kw)


def yz_prism(poly, x0, x1):
    """Side profile (list of (y, z), cm) extruded along X from x0 to x1. Concave n-gons are triangulated here so
    Unreal does not have to guess."""
    bm = bmesh.new()
    a = [bm.verts.new((x0 * CM, y * CM, z * CM)) for y, z in poly]
    b = [bm.verts.new((x1 * CM, y * CM, z * CM)) for y, z in poly]
    bm.faces.new(a)
    bm.faces.new(list(reversed(b)))
    n = len(poly)
    for i in range(n):
        bm.faces.new((a[i], a[(i + 1) % n], b[(i + 1) % n], b[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    ngons = [f for f in bm.faces if len(f.verts) > 4]
    if ngons:
        bmesh.ops.triangulate(bm, faces=ngons, quad_method="BEAUTY", ngon_method="EAR_CLIP")
    return bm


def slab(x0, x1, y0, y1, z_at, t):
    """Plate over x0..x1, y0..y1 whose underside follows the linear function z_at(y); vertical thickness t."""
    bm = bmesh.new()
    corners = ((x0, y0), (x1, y0), (x1, y1), (x0, y1))
    lo = [bm.verts.new((x * CM, y * CM, z_at(y) * CM)) for x, y in corners]
    hi = [bm.verts.new((x * CM, y * CM, (z_at(y) + t) * CM)) for x, y in corners]
    bm.faces.new(list(reversed(lo)))
    bm.faces.new(hi)
    for i in range(4):
        bm.faces.new((lo[i], lo[(i + 1) % 4], hi[(i + 1) % 4], hi[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def placed(bm, loc=(0, 0, 0), rot=(0, 0, 0)):
    """Moves/turns the bmesh itself (loc cm, rot degrees XYZ) – for parts built in a local frame."""
    m = Matrix.Translation(Vector(loc) * CM) @ Euler([math.radians(a) for a in rot]).to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=m, verts=bm.verts)
    return bm


def round_corners(points, radius, steps=4):
    """Polyline with every inner corner replaced by a small quadratic arc (radius = distance cut off each leg)."""
    pts = [Vector(p) for p in points]
    out = [pts[0]]
    for i in range(1, len(pts) - 1):
        a, p, b = pts[i - 1], pts[i], pts[i + 1]
        ra = min(radius, (a - p).length * 0.45)
        rb = min(radius, (b - p).length * 0.45)
        q0 = p + (a - p).normalized() * ra
        q1 = p + (b - p).normalized() * rb
        for s in range(steps + 1):
            t = s / steps
            out.append(q0 * (1 - t) ** 2 + p * 2 * (1 - t) * t + q1 * t * t)
    out.append(pts[-1])
    return [tuple(v) for v in out]


def rod(it: Item, mat, a, b, r, seg=10, shade="smooth"):
    """Straight round bar between two points (cm)."""
    return it.add(sweep([a, b], circle_profile(r, seg)), mat, shade=shade)


class Flight:
    """Step geometry of one straight flight (see module doc)."""

    def __init__(self, run, rise, risers, exit_depth):
        self.L, self.H, self.n = run, rise, risers
        self.r = rise / risers
        self.g = (run - exit_depth) / (risers - 1)
        self.k = self.r / self.g  # pitch

    def y(self, i):
        """Nosing (front edge) of step i, i = 1 .. n (n = exit nosing)."""
        return (i - 1) * self.g

    def z(self, i):
        """Walking surface of step i (z(0) = lower floor)."""
        return i * self.r

    def nos(self, y):
        """Pitch line through all nosings."""
        return self.r + self.k * y


def wall_handrail(it: Item, f: Flight, x_wall, y0, y1, rail_r, rail_mat, bracket_mat, brackets, height=90.0,
                  standoff=7.5):
    """Round handrail on wall brackets along a wall at x = x_wall (wall on the +X side). Top of rail = pitch line +
    height. Both ends turn back to the wall (with a rosette), the brackets are L-shaped rods."""
    xr = x_wall - standoff
    zc = lambda y: f.nos(y) + height - rail_r  # noqa: E731 – rail centre line
    path = [(x_wall - 0.4, y0, zc(y0)), (xr, y0, zc(y0)), (xr, y1, zc(y1)), (x_wall - 0.4, y1, zc(y1))]
    it.add(sweep(round_corners(path, 3.0, 5), circle_profile(rail_r, 16)), rail_mat, shade="smooth", grain="y")
    for y in (y0, y1):
        it.add(cylinder(rail_r + 1.2, 0.8, 24), bracket_mat, loc=(x_wall, y, zc(y)), rot=(0, -90, 0), shade="auto")
    for y in brackets:
        zb = zc(y) - 7.0
        it.add(cylinder(3.0, 0.8, 24), bracket_mat, loc=(x_wall, y, zb), rot=(0, -90, 0), shade="auto")
        arm = round_corners([(x_wall - 0.5, y, zb), (xr, y, zb), (xr, y, zc(y))], 2.2, 5)
        it.add(sweep(arm, circle_profile(0.65, 10)), bracket_mat, shade="smooth")
        it.add(cylinder(1.3, 0.9, 16), bracket_mat, loc=(xr, y, zc(y) - rail_r - 0.6), shade="auto")  # saddle


# ---------------------------------------------------------------------------------------------
# Hall stair: oak treads housed in white strings, white risers, balustrade on the open (-X) side
# ---------------------------------------------------------------------------------------------

HALL = Flight(500.0, 310.0, 17, 4.5)


@item("Stair_hall")
def stair_hall(it: Item):
    f = HALL
    W2 = 80.0                      # half width
    T = 4.5                        # tread thickness (oak)
    OV = 4.5                       # nosing to the riser's back line
    RT = 1.8                       # riser board
    SX = 5.5                       # open string thickness (x -80 .. -74.5)
    WX = 5.0                       # wall string thickness (x 75 .. 80)
    xl, xr = -W2 + SX, W2 - WX     # clear width between the strings
    oak, white = "Wood_Oak", "Paint_White"
    n, L, H = f.n, f.L, f.H

    # Treads 1 .. n (n = exit nosing board at floor height) and risers 1 .. n.
    for i in range(1, n + 1):
        y_front = f.y(i)
        y_back = min(f.y(i + 1) + OV, L) if i < n else L
        add_box(it, oak, xl, xr, y_front, y_back, f.z(i) - T, f.z(i), bevel=0.6, seg=2)
        rb = f.y(i) + OV
        add_box(it, white, xl, xr, rb - RT, rb, f.z(i - 1), f.z(i) - T, bevel=0.15, seg=1)

    # Strings: top edge 5 cm above the pitch line, 32 cm below it; cut level with the floor at the bottom. The wall
    # string ends square at y = L with its top edge turning level at the exit; the open string runs from newel to
    # newel.
    top_off, bot_off = 5.0, 32.0
    y_floor = (bot_off - f.r) / f.k
    y_exit = f.y(n)
    NP, CAP = 8.5, 1.3             # newel post, cap overhang
    yb, yt = CAP + NP / 2, L - CAP - NP / 2
    wall = [(0.0, 0.0), (y_floor, 0.0), (L, f.nos(L) - bot_off), (L, H + top_off), (y_exit, H + top_off),
            (0.0, f.r + top_off)]
    open_ = [(yb, 0.0), (y_floor, 0.0), (yt, f.nos(yt) - bot_off), (yt, f.nos(yt) + top_off), (yb, f.nos(yb) + top_off)]
    it.add(yz_prism(open_, -W2, xl), white, bevel=0.3, seg=2, grain="y")
    it.add(yz_prism(wall, xr, W2), white, bevel=0.3, seg=2, grain="y")

    # Smooth white soffit under the steps and a board closing the top end.
    soff = lambda y: f.nos(y) - 28.2  # noqa: E731
    ys0 = (28.2 - f.r) / f.k
    it.add(slab(xl, xr, ys0, L, soff, 1.2), white, shade="flat")
    add_box(it, white, xl, xr, L - 1.5, L, soff(L) - 0.2, f.z(n - 1) - T + 0.2)

    # Balustrade on the open side: newel posts, square balusters, moulded oak handrail.
    nx0, nx1 = -W2 + CAP, -W2 + CAP + NP   # newels: caps flush with the outer face (x = -80)
    bx = nx0 + 2.4                 # baluster / rail centre line (rail flush with the newel's outer face)
    rail_top = lambda y: f.nos(y) + 90.0  # noqa: E731

    def newel(yc, z0, z1, drop):
        add_box(it, white, nx0, nx1, yc - NP / 2, yc + NP / 2, z0, z1, bevel=0.5, seg=2, grain="z")
        add_box(it, white, nx0 - 0.8, nx1 + 0.8, yc - NP / 2 - 0.8, yc + NP / 2 + 0.8, z1 - 9.0, z1 - 6.5, bevel=0.5, seg=2)
        add_box(it, oak, nx0 - 1.3, nx1 + 1.3, yc - NP / 2 - 1.3, yc + NP / 2 + 1.3, z1, z1 + 2.8, bevel=0.8, seg=3)
        cx = (nx0 + nx1) / 2
        it.add(lathe([(0, 0), (2.6, 0), (2.6, 0.8), (1.4, 1.6), (1.2, 2.4), (3.0, 4.6), (3.1, 6.0), (2.2, 7.8), (0, 8.4)], 24),
               oak, loc=(cx, yc, z1 + 2.8), shade="smooth", grain="z")
        if drop:  # turned drop below a newel that hangs in the air
            it.add(lathe([(0, 0), (1.0, 0.4), (2.6, 2.6), (3.0, 4.8), (2.4, 6.6), (3.6, 7.4), (3.6, 8.2), (0, 8.2)], 24),
                   white, loc=(cx, yc, z0 - 8.2), shade="smooth")
        else:  # base block on the floor
            add_box(it, white, nx0 - 0.9, nx1 + 0.9, yc - NP / 2 - 0.9, yc + NP / 2 + 0.9, z0, z0 + 9.0, bevel=0.6, seg=2)

    newel(yb, 0.0, rail_top(yb) + 14.0, drop=False)
    newel(yt, f.nos(yt) - bot_off - 8.0, rail_top(yt) + 14.0, drop=True)

    RAIL_H = 5.0
    rail_prof = [(-2.4, -2.5), (2.4, -2.5), (2.4, 0.9), (1.7, 2.1), (0.8, 2.5), (-0.8, 2.5), (-1.7, 2.1), (-2.4, 0.9)]
    it.add(sweep([(bx, yb, rail_top(yb) - RAIL_H / 2), (bx, yt, rail_top(yt) - RAIL_H / 2)], rail_prof), oak,
           shade="auto", angle=40, grain="y")
    # String capping under the balusters.
    sx_c = -W2 + SX / 2
    it.add(sweep([(sx_c, yb, f.nos(yb) + top_off + 0.6), (sx_c, yt, f.nos(yt) + top_off + 0.6)],
                 rect_profile(SX, 1.2, 0.3)), oak, shade="auto", grain="y")
    BW = 3.0
    for i in range(1, n):
        for frac in (0.25, 0.75):
            y = f.y(i) + frac * f.g
            if y < yb + NP / 2 + 2.5 or y > yt - NP / 2 - 2.5:
                continue
            z0 = f.nos(y) + top_off + 0.5
            z1 = rail_top(y) - RAIL_H + 0.8
            add_box(it, white, bx - BW / 2, bx + BW / 2, y - BW / 2, y + BW / 2, z0, z1, bevel=0.25, seg=1, grain="z")

    # Wall handrail on brackets (wall at x = +80).
    wall_handrail(it, f, W2, 12.0, L - 8.0, 2.25, oak, "Steel_Satin", brackets=(70.0, 195.0, 320.0, 440.0))


# ---------------------------------------------------------------------------------------------
# Cellar stair: solid concrete with sandstone treads/risers, dark nosing angles, wall handrail on +X
# ---------------------------------------------------------------------------------------------

CELLAR = Flight(445.0, 290.0, 16, 8.0)


def nosing_angle(it: Item, y, z, x0, x1):
    """Dark steel nosing angle on a stone step edge (front flange 3 cm, top flange 4.5 cm) with a rubber
    anti-slip insert."""
    sec = [(y, z - 3.0), (y + 0.6, z - 3.0), (y + 0.6, z - 0.4), (y + 4.5, z - 0.4), (y + 4.5, z + 0.1), (y, z + 0.1)]
    it.add(yz_prism(sec, x0, x1), "Iron_Dark", shade="flat")
    add_box(it, "Rubber_Black", x0, x1, y + 1.4, y + 3.8, z + 0.1, z + 0.28)


@item("Stair_cellar")
def stair_cellar(it: Item):
    f = CELLAR
    W2 = 75.0
    T = 3.0      # stone tread plate
    OV = 3.0     # nosing to the riser plate's back line
    RT = 2.0     # stone riser plate (front face 1 cm behind the nosing)
    SK = 1.2     # stepped skirting on both walls
    n, L, H = f.n, f.L, f.H
    stone, conc = "Stone_Sand", "Concrete"

    for i in range(1, n + 1):
        y0 = f.y(i)
        rb = y0 + OV
        y_back = f.y(i + 1) + OV if i < n else L
        # Concrete core under this step (from the cellar floor up).
        core_back = min(f.y(i + 1) + OV, L) if i < n else L
        add_box(it, conc, -W2, W2, rb, core_back, 0.0, f.z(i) - T)
        add_box(it, stone, -W2, W2, y0 + 0.6, y_back, f.z(i) - T, f.z(i), bevel=0.35, seg=2)
        add_box(it, stone, -W2, W2, rb - RT, rb, f.z(i - 1), f.z(i) - T, bevel=0.2, seg=1)
        nosing_angle(it, y0, f.z(i), -W2 + SK, W2 - SK)

    # Stepped skirting along both walls: bottom follows the steps, top 8 cm above the pitch line (level at the
    # top so it runs on as a floor skirting).
    top = lambda y: min(f.nos(y) + 8.0, H + 8.0)  # noqa: E731
    bottom = [(0.0, 0.0)]
    for i in range(1, n + 1):
        face = f.y(i) + 1.0  # riser face
        bottom += [(face, f.z(i - 1)), (face, f.z(i))]
    bottom.append((L, H))
    tops = [(L, top(L)), (f.y(n), top(f.y(n))), (0.0, top(0.0))]
    poly = bottom + tops
    for x0, x1 in ((-W2, -W2 + SK), (W2 - SK, W2)):
        it.add(yz_prism(poly, x0, x1), stone, shade="flat", grain="y")

    wall_handrail(it, f, W2, 10.0, L - 10.0, 2.1, "Steel", "Steel", brackets=(55.0, 185.0, 315.0, 425.0))


# ---------------------------------------------------------------------------------------------
# Outside cellar stair: concrete steps between retaining walls, coping, steel railings along both sides
# ---------------------------------------------------------------------------------------------

OUTSIDE = Flight(360.0, 290.0, 16, 12.0)
# Sides with a retaining wall + railing (+1 = +X). Empty in the villa: there the house wall stands on +X and the
# level's own pit walls (longer than the flight, up to the cellar door) double as parapets.
OUTSIDE_WALLS = ()


@item("Stair_outside")
def stair_outside(it: Item):
    f = OUTSIDE
    W2 = 70.0
    WT = 20.0                  # retaining wall thickness (outside the 140 cm)
    n, L, H = f.n, f.L, f.H
    conc = "Concrete"
    steel = "Paint_Anthracite"

    # Solid concrete steps (1 cm into the walls), the last block is the exit at ground level.
    for i in range(1, n + 1):
        y1 = f.y(i + 1) if i < n else L
        add_box(it, conc, -W2 - 1.0, W2 + 1.0, f.y(i), y1, 0.0, f.z(i), bevel=0.8, seg=2)

    COP_T, COP_OV = 4.0, 1.5
    rail_top = H + 100.0
    posts = [6.0 + k * (L - 12.0) / 4 for k in range(5)]
    for side in OUTSIDE_WALLS:
        xi, xo = side * W2, side * (W2 + WT)
        x0, x1 = min(xi, xo), max(xi, xo)
        xc = (x0 + x1) / 2
        add_box(it, conc, x0, x1, 0.0, L, 0.0, H, bevel=0.4, seg=1, grain="y")
        # Coping a little above the garden (no z-fighting with the ground, edge reads from above).
        add_box(it, conc, x0 - COP_OV, x1 + COP_OV, 0.0, L, H, H + COP_T, bevel=0.6, seg=2, grain="y")
        zc = H + COP_T
        # Railing: square posts on base plates, top rail, bottom bar, vertical bars (gaps < 12 cm).
        for y in posts:
            add_box(it, steel, xc - 5.0, xc + 5.0, y - 5.0, y + 5.0, zc, zc + 0.8, bevel=0.2, seg=1)
            for bxo in (-3.4, 3.4):
                for byo in (-3.4, 3.4):
                    it.add(cylinder(0.6, 0.6, 8), "Steel", loc=(xc + bxo, y + byo, zc + 0.8), shade="auto")
            add_box(it, steel, xc - 2.25, xc + 2.25, y - 2.25, y + 2.25, zc + 0.8, rail_top - 4.0, bevel=0.3, seg=1, grain="z")
        add_box(it, steel, xc - 2.5, xc + 2.5, posts[0] - 3.0, posts[-1] + 3.0, rail_top - 4.0, rail_top, bevel=0.4, seg=2,
                grain="y")
        z_bar0 = zc + 9.0
        add_box(it, steel, xc - 2.0, xc + 2.0, posts[0], posts[-1], z_bar0, z_bar0 + 1.5, bevel=0.2, seg=1, grain="y")
        for a, b in zip(posts, posts[1:]):
            gaps = 8
            for k in range(1, gaps):
                y = a + (b - a) * k / gaps
                add_box(it, steel, xc - 0.8, xc + 0.8, y - 0.8, y + 0.8, z_bar0 + 1.5, rail_top - 4.0, grain="z")


# ---------------------------------------------------------------------------------------------
# Glass canopy over the entrance
# ---------------------------------------------------------------------------------------------


@item("Deco_canopy")
def canopy(it: Item):
    """380 x 200 glass canopy, back on the wall at y = 0, lowest edge at z = 250, glass falls 8 % towards the front
    edge. Three laminated panes in a wall profile and a front drip profile, held by four tension rods."""
    W2, D = 190.0, 200.0
    slope = 0.08
    GT = 2.0
    satin = "Steel_Satin"
    under = lambda y: 250.4 + (y + D) * slope  # noqa: E731 – glass underside
    ang = math.degrees(math.atan(slope))

    # Panes with 8 mm joints.
    joint = 0.8
    pw = (2 * W2 - 2 * joint) / 3
    edges = [-W2, -W2 + pw, -W2 + pw + joint, W2 - pw - joint, W2 - pw, W2]
    for x0, x1 in zip(edges[::2], edges[1::2]):
        it.add(slab(x0, x1, -D + 0.4, -3.0, under, GT), "Glass", shade="flat")
    # Wall profile (glass slides in) and front drip profile.
    add_box(it, satin, -W2, W2, -6.0, 0.0, 262.0, 271.0, bevel=0.3, seg=2)
    add_box(it, satin, -W2, W2, -D, -D + 4.5, 250.0, 253.4, bevel=0.4, seg=2)
    for sx in (-1, 1):  # end caps of the wall profile
        add_box(it, satin, min(sx * W2, sx * (W2 - 0.8)), max(sx * W2, sx * (W2 - 0.8)), -6.4, 0.0, 261.6, 271.4, bevel=0.15, seg=1)

    # Tension rods from point holders near the front edge up to wall consoles.
    z_anchor = 347.0
    y_hold = -D + 12.0
    xj = W2 - pw - joint / 2  # pane joints
    for x in (-168.0, -xj, xj, 168.0):
        z_top = under(y_hold) + GT
        # Clamp discs above and below the glass, tilted with it.
        it.add(cylinder(3.2, 1.0, 24), satin, loc=(x, y_hold, z_top), rot=(ang, 0, 0), shade="auto")
        it.add(cylinder(3.2, 0.9, 24), satin, loc=(x, y_hold, under(y_hold) - 0.9), rot=(ang, 0, 0), shade="auto")
        it.add(cylinder(1.0, GT + 1.9, 12), satin, loc=(x, y_hold, under(y_hold) - 0.9), shade="smooth")  # bolt
        # Fork lug on the disc and the rod.
        add_box(it, satin, x - 1.2, x + 1.2, y_hold - 1.5, y_hold + 1.5, z_top + 0.8, z_top + 4.0, bevel=0.3, seg=1)
        p0 = Vector((x, y_hold + 0.5, z_top + 3.2))
        p1 = Vector((x, -4.0, z_anchor))
        rod(it, satin, tuple(p0), tuple(p1), 0.8, 12)
        mid = p0.lerp(p1, 0.55)
        dirv = (p1 - p0).normalized()
        rod(it, satin, tuple(mid - dirv * 6.0), tuple(mid + dirv * 6.0), 1.25, 12)  # turnbuckle
        for e in (p0 + dirv * 2.5, p1 - dirv * 2.5):  # fork heads
            rod(it, satin, tuple(e - dirv * 2.5), tuple(e + dirv * 2.5), 1.1, 12)
        # Wall console: plate, lug, pin.
        add_box(it, satin, x - 3.5, x + 3.5, -1.2, 0.0, z_anchor - 6.0, z_anchor + 6.0, bevel=0.4, seg=2)
        add_box(it, satin, x - 0.6, x + 0.6, -5.0, -1.2, z_anchor - 1.6, z_anchor + 1.6, bevel=0.2, seg=1)
        it.add(cylinder(0.55, 3.2, 12), satin, loc=(x - 1.6, -4.0, z_anchor), rot=(0, 90, 0), shade="smooth")
        for sz in (-4.2, 4.2):  # anchor bolts
            it.add(cylinder(0.7, 0.4, 12), satin, loc=(x, -1.2, z_anchor + sz), rot=(90, 0, 0), shade="auto")


# ---------------------------------------------------------------------------------------------
# Security camera
# ---------------------------------------------------------------------------------------------


def rrect_profile(w, h, r, n=4):
    """Rounded rectangle (w x h, corner radius r) as a closed profile."""
    pts = []
    for cx, cy, a0 in ((w / 2 - r, -h / 2 + r, -90), (w / 2 - r, h / 2 - r, 0), (-w / 2 + r, h / 2 - r, 90),
                       (-w / 2 + r, -h / 2 + r, 180)):
        for s in range(n + 1):
            a = math.radians(a0 + 90 * s / n)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


@item("Deco_security_camera")
def security_camera(it: Item):
    """Bullet camera on a short wall arm. Origin = centre of the mounting plate, plate back at y = 0; the camera
    looks towards -Y and 15 degrees down."""
    white, satin = "Plastic_White", "Steel"
    tilt = 18.0
    pivot = (0.0, -10.5, 2.6)  # top of the swivel joint

    # Wall plate with screws.
    add_box(it, white, -5.0, 5.0, -1.4, 0.0, -6.5, 6.5, bevel=0.6, seg=3)
    for sx in (-3.4, 3.4):
        for sz in (-4.9, 4.9):
            it.add(cylinder(0.45, 0.25, 12), satin, loc=(sx, -1.4, sz), rot=(90, 0, 0), shade="auto")
    # Arm: boss on the plate, tube out to an upright knuckle with a ball joint.
    it.add(cylinder(2.1, 1.4, 24), white, loc=(0, -1.4, 0), rot=(90, 0, 0), shade="auto")
    it.add(sweep([(0, -2.6, 0), (0, -10.5, 0)], circle_profile(1.35, 16)), white, shade="smooth")
    it.add(cylinder(1.5, 2.2, 20), white, loc=(0, -10.5, -1.1), shade="auto")
    it.add(sphere(1.25, 16, 8), white, loc=(0, -10.5, 0.4), shade="smooth")
    it.add(cylinder(1.6, 0.5, 20), "Plastic_Black", loc=(0, -10.5, 0.9), shade="auto")  # locking ring

    def cam(bm, mat, **kw):
        """Part built in the camera frame (pivot at 0, looking along -Y), tilted down and moved to the joint."""
        return it.add(bm, mat, loc=pivot, rot=(tilt, 0, 0), **kw)

    cam(cylinder(1.2, 1.2, 16), white, shade="auto")  # foot
    # Housing.
    bm, c = box(-4.2, 4.2, -14.5, 4.5, 0.9, 8.3)
    cam(placed(bm, c), white, bevel=2.0, seg=4)
    # Black front glass, lens with a ring, red status LED.
    cam(placed(sweep([(0, -13.9, 0), (0, -14.75, 0)], rrect_profile(7.2, 6.0, 1.9, 4)), (0, 0, 4.6)), "Screen_Black",
        bevel=0.15, seg=1)
    cam(placed(lathe([(0, 0), (2.0, 0), (2.0, 0.35), (1.6, 0.45), (0, 0.45)], 28), (0, -14.7, 5.0), (90, 0, 0)), satin,
        shade="auto")
    cam(placed(lathe([(0, 0), (1.45, 0), (1.45, 0.25), (1.1, 0.7), (0.5, 0.9), (0, 0.92)], 28), (0, -15.0, 5.0),
               (90, 0, 0)), "Glass_Tinted", shade="smooth")
    cam(placed(lathe([(0, 0), (0.95, 0), (0.95, 0.3), (0, 0.3)], 20), (0, -15.05, 5.0), (90, 0, 0)), "Screen_Black",
        shade="auto")
    for k in range(6):  # IR emitters around the lens
        a = math.radians(30 + 60 * k)
        cam(placed(cylinder(0.32, 0.15, 10), (2.65 * math.cos(a), -14.78, 5.0 + 2.25 * math.sin(a) * 0.82), (90, 0, 0)),
            "Plastic_Grey", shade="auto")
    cam(placed(sphere(0.32, 12, 6, sz=0.7), (2.3, -14.7, 2.6), (90, 0, 0)), "LED_Red", shade="smooth")
    # Sun shield: curved hood over the housing (the housing's rounded back stays visible behind it), standing off
    # on two blocks.
    hz, r_out, r_in = 3.4, 6.15, 5.75
    arc = [(r_out * math.cos(math.radians(a)), hz + r_out * math.sin(math.radians(a))) for a in range(20, 161, 10)]
    arc += [(r_in * math.cos(math.radians(a)), hz + r_in * math.sin(math.radians(a))) for a in range(160, 19, -10)]
    bm = sweep([(0, 1.5, 0), (0, -16.8, 0)], arc)
    bmesh.ops.triangulate(bm, faces=[fc for fc in bm.faces if len(fc.verts) > 4], quad_method="BEAUTY", ngon_method="EAR_CLIP")
    cam(bm, white, shade="auto", angle=40)
    for y in (-0.5, -10.0):
        bm, c = box(-1.5, 1.5, y - 1.2, y + 1.2, 8.0, hz + r_in + 0.15)
        cam(placed(bm, c), white)
