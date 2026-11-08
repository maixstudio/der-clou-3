"""Villa basement leisure (G5): wine rack module, billiard table, treadmill, weight bench, sauna bench.

Exported as SM_<id>. Conventions: cm, origin bottom centre, front = -Y, width along X, height Z.

Deco_wine_rack is a 100-cm module that the level lines up into 5.8-m rows (one against the wall, one free in
the room): everything stays inside x = -50 .. +50, and the bottles lie head to tail in a checkerboard so their
necks show from both sides.
"""

import math
import random
from collections import defaultdict

import bmesh
import bpy
from mathutils import Euler, Matrix, Vector

from dc_items import item, turned_leg
from dc_mesh import CM, Item, circle_profile, cube, cylinder, lathe, ngon_prism, puff, rect_profile, sweep


# ---------------------------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------------------------


def place(bm, loc=(0, 0, 0), rot=(0, 0, 0)):
    """Move a bmesh (metres) by loc (cm) and rot (degrees, XYZ)."""
    m = Matrix.Translation(Vector(loc) * CM) @ Euler([math.radians(a) for a in rot]).to_matrix().to_4x4()
    bm.transform(m)
    return bm


def merged(parts):
    """Join [(bm, loc, rot)] into one bmesh, so a hundred bottles become one Item part. Frees the inputs."""
    out = bmesh.new()
    tmp = bpy.data.meshes.new("_g5_merge")
    for entry in parts:
        if isinstance(entry, bmesh.types.BMesh):
            entry = (entry,)
        bm, loc, rot = (tuple(entry) + ((0, 0, 0), (0, 0, 0)))[:3]
        place(bm, loc, rot)
        bm.to_mesh(tmp)
        out.from_mesh(tmp)
        bm.free()
    bpy.data.meshes.remove(tmp)
    return out


def arc2d(cx, cy, r, a0, a1, step=15.0):
    """Points on a circle arc (degrees, a0 -> a1, both included)."""
    n = max(1, int(math.ceil(abs(a1 - a0) / step - 1e-6)))
    out = []
    for i in range(n + 1):
        a = math.radians(a0 + (a1 - a0) * i / n)
        out.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return out


def poly_prism(points, z0, z1):
    """Concave 2D outline (cm) extruded from z0 to z1; the caps are triangulated so Unreal gets them right."""
    bm = ngon_prism(points, z1 - z0)
    big = [f for f in bm.faces if len(f.verts) > 4]
    if big:
        bmesh.ops.triangulate(bm, faces=big, quad_method="BEAUTY", ngon_method="BEAUTY")
    bmesh.ops.translate(bm, vec=Vector((0, 0, z0 * CM)), verts=bm.verts)
    return bm


def side_prism(points_xz, width, y_center=0.0):
    """Side outline in the XZ plane (cm) extruded along Y, centred on y_center (bmesh already placed)."""
    bm = ngon_prism(points_xz, width)
    big = [f for f in bm.faces if len(f.verts) > 4]
    if big:
        bmesh.ops.triangulate(bm, faces=big, quad_method="BEAUTY", ngon_method="BEAUTY")
    # ngon_prism: outline in XY, extruded along +Z. Rotate X +90: y -> z, z -> -y.
    return place(bm, (0, y_center + width / 2, 0), (90, 0, 0))


def ring_lathe(profile, segments=32):
    """Closed profile loop (r, z) turned around Z (plates, collars) without a seam."""
    bm = lathe(list(profile) + [profile[0]], segments, caps=False)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-6)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def ball(r, segments=16, rings=10, i0=0, i1=None):
    """Sphere centred on the origin, or the zone between rings i0 and i1 (0 = bottom pole) – stripes share the
    rings of the white caps, so nothing overlaps."""
    i1 = rings if i1 is None else i1
    prof = [(r * math.sin(math.pi * i / rings), -r * math.cos(math.pi * i / rings)) for i in range(i0, i1 + 1)]
    bm = lathe(prof, segments, caps=False)
    for f in bm.faces:  # open zones: make sure every face points outward
        if f.normal.dot(f.calc_center_median()) < 0:
            f.normal_flip()
    return bm


def add_groups(it, groups, **kw):
    """groups: material -> [(bm, loc, rot)], one merged part per material."""
    for mat, parts in groups.items():
        if parts:
            it.add(merged(parts), mat, **kw)


# ---------------------------------------------------------------------------------------------
# Wine rack module (100 x 50 x 200)
# ---------------------------------------------------------------------------------------------

RACK_ROWS = 17
RACK_COLS = 10
RACK_PITCH_X = 9.0
RACK_PITCH_Z = 11.0

# Bottle profiles (r, z), base at z = 0, 30 cm long, lying along Y in the rack.
BORDEAUX = [(0, 0), (3.5, 0), (3.75, 0.6), (3.75, 20.5), (2.9, 22.8), (1.55, 24.6), (1.5, 29.6), (0, 30.0)]
BURGUNDY = [(0, 0), (3.7, 0), (3.95, 0.6), (3.95, 17.0), (3.2, 21.2), (1.65, 25.0), (1.5, 29.6), (0, 30.0)]
CAPSULES = ["Lacquer_Red", "Lacquer_Red", "Velvet_Red", "Gilt", "Gilt", "Lacquer_Black", "Lacquer_Cream", "Silver"]


@item("Deco_wine_rack")
def wine_rack(it: Item):
    """Oak slat rack: four posts, side rungs, rails under the bottles, a front and back slat per row, no top board
    (the top row of bottles shows from above). Ends flush at x = +-50 so modules line up."""
    oak = "Wood_Oak"
    rng = random.Random(5)
    W, D, H = 100.0, 50.0, 200.0
    pw = 4.0
    px, py = W / 2 - pw / 2, D / 2 - pw / 2          # post centres 48 / 23
    ix, iy = W / 2 - pw, D / 2 - pw                  # inner faces 46 / 21
    it.add(merged([(cube(pw, pw, H), (sx * px, sy * py, H / 2)) for sx in (-1, 1) for sy in (-1, 1)]), oak,
           bevel=0.5, seg=2, grain="z")

    rungs, rails, slats = [], [], []
    bottles = []  # (x, row axis z, neck to front?)
    for k in range(RACK_ROWS):
        z0 = 2.0 + RACK_PITCH_Z * k
        axis = z0 + 2.5 + 3.2 + 3.75 - 0.3          # bottle bodies sit slightly down in the rails
        for sx in (-1, 1):
            rungs.append((cube(3.0, 2 * iy, 2.5), (sx * px, 0, z0 + 1.25)))
        for sy in (-1, 1):
            rails.append((cube(2 * ix + 2.0, 2.2, 3.2), (0, sy * 5.0, z0 + 2.5 + 1.6)))
            slats.append((cube(2 * ix, 1.8, 3.0), (0, sy * py, axis - 1.8 - 1.5)))
        for c in range(RACK_COLS):
            if rng.random() < 0.1:
                continue  # a few empty cells
            bottles.append((-RACK_PITCH_X * (RACK_COLS - 1) / 2 + RACK_PITCH_X * c, axis, (c + k) % 2 == 0))
    # Top frame.
    for sy in (-1, 1):
        slats.append((cube(2 * ix, 2.4, 5.0), (0, sy * py, H - 2.5)))
    for sx in (-1, 1):
        rungs.append((cube(3.0, 2 * iy, 5.0), (sx * px, 0, H - 2.5)))
    it.add(merged(rungs), oak, bevel=0.3, seg=1, grain="y")
    it.add(merged(rails + slats), oak, bevel=0.3, seg=1, grain="x")

    glass, fill, caps = defaultdict(list), defaultdict(list), defaultdict(list)
    for x, z, front in bottles:
        burgundy = rng.random() < 0.25
        prof = BURGUNDY if burgundy else BORDEAUX
        d = -1.0 if front else 1.0                   # neck direction along Y
        base_y = -d * 6.0
        rot = (90, 0, 0) if front else (-90, 0, 0)
        glass["Glass_Tinted" if rng.random() < 0.2 else "Glass_Bottle"].append((lathe(prof, 8), (x, base_y, z), rot))
        r_fill, h_fill = (3.75, 17.0) if burgundy else (3.55, 20.3)
        wine = "Fabric_Olive" if rng.random() < 0.18 else "Lacquer_Black"   # white / red wine
        fill[wine].append((cylinder(r_fill - 0.2, h_fill, 8), (x, base_y + d * 0.5, z), rot))
        caps[rng.choice(CAPSULES)].append((cylinder(1.63, 4.95, 8), (x, base_y + d * 25.2, z), rot))
    add_groups(it, fill, shade="smooth")
    add_groups(it, caps, shade="smooth")
    add_groups(it, glass, shade="smooth")


# ---------------------------------------------------------------------------------------------
# Billiard table (280 x 160 x 80)
# ---------------------------------------------------------------------------------------------

BALL_D = 5.72
# Rows of the rack from the apex (foot spot) outward; 8 in the middle, a solid and a stripe in the back corners.
BALL_ROWS = [[1], [9, 2], [10, 8, 3], [11, 7, 14, 4], [5, 13, 15, 6, 12]]
# Nearest existing materials (no purple/orange in the table: 4 = navy, 5 = terracotta).
BALL_COLOURS = {1: "Plastic_Yellow", 2: "Porcelain_Blue", 3: "Lacquer_Red", 4: "Fabric_Navy", 5: "Terracotta",
                6: "Paint_Green", 7: "Velvet_Red", 8: "Lacquer_Black"}


@item("Deco_billiard_table")
def billiard_table(it: Item):
    """Pool table: mahogany rails with sights, green cloth and cushions, six leather pockets, balls in the
    wooden triangle at the foot spot, cue ball at the head spot, a cue and chalk."""
    wood, felt = "Wood_Mahogany", "Felt_Green"
    L2, W2, H = 140.0, 80.0, 80.0
    FX, FY = 125.0, 62.5             # playing field (cushion noses)
    BX, BY = 130.0, 67.5             # cushion backs = inner edge of the wooden rails
    PR = 6.5                         # pocket radius
    MY = BY + 1.5                    # side pockets sit a little back
    ma = math.degrees(math.asin((MY - BY) / PR))
    mx = PR * math.cos(math.radians(ma))
    RAIL_Z0, BED_Z0, CLOTH_Z, CUSH_Z1 = 71.0, 72.5, 75.5, 79.2

    # Rails: two C-shaped halves around the pockets, meeting under the side-pocket leathers.
    right = [(0, W2), (L2, W2), (L2, -W2), (0, -W2)]
    right += arc2d(0, -MY, PR, -90, ma)
    right += arc2d(BX, -BY, PR, 180, 450)
    right += arc2d(BX, BY, PR, -90, 180)
    right += arc2d(0, MY, PR, -ma, 90)
    left = [(-x, y) for x, y in reversed(right)]
    for pts in (right, left):
        it.add(poly_prism(pts, RAIL_Z0, H), wood, bevel=1.0, seg=3)

    # Slate bed with cloth, cut out at the pockets.
    bed = []
    bed += arc2d(BX, BY, PR, 270, 180)
    bed += arc2d(0, MY, PR, -ma, -180 + ma)
    bed += arc2d(-BX, BY, PR, 360, 270)
    bed += arc2d(-BX, -BY, PR, 90, 0)
    bed += arc2d(0, -MY, PR, 180 - ma, ma)
    bed += arc2d(BX, -BY, PR, 180, 90)
    it.add(poly_prism(bed, BED_Z0, CLOTH_Z), felt, shade="flat")

    # Cushions with angled pocket jaws.
    cushions = []
    for sx in (-1, 1):
        for sy in (-1, 1):
            long_c = [(7.5, FY), (116.0, FY), (BX - PR, BY), (mx, BY)]
            cushions.append(poly_prism([(sx * x, sy * y) for x, y in long_c], CLOTH_Z, CUSH_Z1))
        end_c = [(FX, -53.5), (FX, 53.5), (BX, BY - PR), (BX, -(BY - PR))]
        cushions.append(poly_prism([(sx * x, y) for x, y in end_c], CLOTH_Z, CUSH_Z1))
    it.add(merged(cushions), felt, bevel=0.8, seg=3)

    # Pockets: dark hole, leather cover over the rail, leather pouch hanging below.
    pockets = [(sx * BX, sy * BY) for sx in (-1, 1) for sy in (-1, 1)] + [(0, sy * MY) for sy in (-1, 1)]
    it.add(merged([(cylinder(PR - 0.05, 0.2, 24), (x, y, BED_Z0)) for x, y in pockets]), "Plastic_Black", shade="flat")
    pouch = [(0, 0), (3.0, 0.3), (5.0, 2.0), (6.0, 5.5), (6.2, 11.0), (0, 11.0)]
    it.add(merged([(lathe(pouch, 20), (x, y, 47.0)) for x, y in pockets]), "Leather_Black", shade="smooth")
    liners = []
    for sx in (-1, 1):
        for sy in (-1, 1):
            arc = arc2d(BX, BY, PR - 0.25, -90, 180, 15)
            liners.append(sweep([(sx * x, sy * y, (BED_Z0 + H - 0.2) / 2) for x, y in arc], rect_profile(0.5, H - 0.2 - BED_Z0)))
    for sy in (-1, 1):
        arc = arc2d(0, MY, PR - 0.25, -ma, 180 + ma, 15)
        liners.append(sweep([(x, sy * y, (BED_Z0 + H - 0.2) / 2) for x, y in arc], rect_profile(0.5, H - 0.2 - BED_Z0)))
    it.add(merged(liners), "Rubber_Black", shade="auto")
    covers = []
    for sx in (-1, 1):
        for sy in (-1, 1):
            arc = arc2d(BX, BY, 8.25, -80, 170, 10)
            covers.append((sweep([(sx * x, sy * y, H + 0.3) for x, y in arc], rect_profile(3.5, 0.6, 0.2)),))
    for sy in (-1, 1):
        arc = arc2d(0, MY, 8.75, 8, 172, 10)
        covers.append((sweep([(x, sy * y, H + 0.3) for x, y in arc], rect_profile(4.5, 0.6, 0.2)),))
        covers.append((cube(9.0, 0.5, 8.0), (0, sy * (W2 + 0.25), H - 4.0)))  # leather wraps over the rail edge
    it.add(merged(covers), "Leather_Black", bevel=0.15, seg=1)

    # Sights (diamonds) on the rails.
    sights = []
    for x in (-93.75, -62.5, -31.25, 31.25, 62.5, 93.75):
        for sy in (-1, 1):
            sights.append((cube(1.4, 1.4, 0.3), (x, sy * (BY + W2) / 2, H), (0, 0, 45)))
    for y in (-31.25, 0.0, 31.25):
        for sx in (-1, 1):
            sights.append((cube(1.4, 1.4, 0.3), (sx * (BX + L2) / 2, y, H), (0, 0, 45)))
    it.add(merged(sights), "Ivory", shade="flat")

    # Body, panels, plinth moulding, legs.
    it.add(cube(272, 152, BED_Z0 - 58.0), wood, loc=(0, 0, (BED_Z0 + 58.0) / 2), bevel=0.6, seg=2)
    panels = []
    for x in (-86.0, 0.0, 86.0):
        for sy in (-1, 1):
            panels.append((cube(72, 1.0, 8.5), (x, sy * 76.5, 65.2)))
    for sx in (-1, 1):
        panels.append((cube(1.0, 112, 8.5), (sx * 136.5, 0, 65.2)))
    it.add(merged(panels), wood, bevel=0.45, seg=2, profile=0.3)
    it.add(cube(275, 155, 3.0), wood, loc=(0, 0, 57.5), bevel=1.2, seg=3, profile=0.7)
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = sx * 118.0, sy * 58.0
            it.add(cube(15, 15, 10), wood, loc=(x, y, 51.0), bevel=0.6, seg=2, grain="z")
            it.add(turned_leg(44.0, top_r=5.5, scale=3.0), wood, loc=(x, y, 2.0), grain="z")
            it.add(cylinder(6.0, 2.0, 24), "Rubber_Black", loc=(x, y, 0), shade="smooth")

    # Balls in the triangle, apex on the foot spot pointing to the head end (-X).
    r = BALL_D / 2
    d = BALL_D + 0.02
    bz = CLOTH_Z + r
    rng = random.Random(3)
    solids, whites, bands = defaultdict(list), [], defaultdict(list)
    for i, row in enumerate(BALL_ROWS):
        for j, n in enumerate(row):
            pos = (FX / 2 + i * d * math.sin(math.radians(60)), (j - i / 2) * d, bz)
            rot = (rng.uniform(0, 360), rng.uniform(0, 360), 0)
            if n <= 8:
                solids[BALL_COLOURS[n]].append((ball(r), pos, rot))
            else:
                whites.append((ball(r, i1=3), pos, rot))
                whites.append((ball(r, i0=7), pos, rot))
                bands[BALL_COLOURS[n - 8]].append((ball(r, i0=3, i1=7), pos, rot))
    whites.append((ball(r), (-FX / 2, 0, bz)))  # cue ball on the head spot
    add_groups(it, solids, shade="smooth")
    add_groups(it, bands, shade="smooth")
    it.add(merged(whites), "Porcelain", shade="smooth")
    # Wooden triangle around the balls.
    cx = FX / 2 + (2.0 / 3.0) * 4 * d * math.sin(math.radians(60))
    inr = 4 * d / (2 * math.sqrt(3)) + r + 0.1 + 0.8
    tri = [(cx - 2 * inr, 0, CLOTH_Z + 1.7), (cx + inr, inr * math.sqrt(3), CLOTH_Z + 1.7),
           (cx + inr, -inr * math.sqrt(3), CLOTH_Z + 1.7)]
    it.add(sweep(tri, rect_profile(1.6, 3.4), closed=True), "Wood_Oak", bevel=0.4, seg=2)

    # Cue lying on the cloth, butt towards the head end.
    def cue_r(z):
        return 1.48 - 0.38 * z / 73.0 if z <= 75.0 else 1.07 - 0.42 * (z - 75.0) / 69.5

    sections = [(0.0, 1.2, "Rubber_Black"), (1.2, 20.0, "Lacquer_Black"), (20.0, 46.0, "Fabric_Black"),
                (46.0, 73.0, "Wood_Mahogany"), (73.0, 75.0, "Chrome"), (75.0, 144.5, "Wood_Light"),
                (144.5, 146.8, "Lacquer_White")]
    tilt = math.degrees(math.atan((1.48 - 0.64) / 147.4))
    cue_loc, cue_rot = (-100.0, -42.0, CLOTH_Z + 1.48), (0, 90 + tilt, 9)
    for z0, z1, mat in sections:
        r0, r1 = cue_r(z0), cue_r(min(z1, 144.5))
        if mat == "Lacquer_White":
            r0 = r1 = 0.66
        if mat == "Chrome":
            r0 = r1 = 1.13
        prof = [(0, z0), (r0, z0), (r1, z1), (0, z1)]
        it.add(lathe(prof, 16), mat, loc=cue_loc, rot=cue_rot, shade="smooth", grain="z")
    it.add(lathe([(0, 146.8), (0.64, 146.8), (0.62, 147.3), (0.4, 147.55), (0, 147.6)], 16), "Leather_Brown",
           loc=cue_loc, rot=cue_rot, shade="smooth")
    # Chalk on the rail.
    it.add(cube(2.2, 2.2, 1.9), "Fabric_Blue", loc=(108.0, -(BY + W2) / 2, H + 0.95), rot=(0, 0, 20), bevel=0.2, seg=1)


# ---------------------------------------------------------------------------------------------
# Treadmill (180 x 80 x 140, console at +X)
# ---------------------------------------------------------------------------------------------


@item("Deco_treadmill")
def treadmill(it: Item):
    alu, black = "Aluminium", "Plastic_Black"
    # Frame, belt with rounded roller ends, step-on side rails with grip strips.
    it.add(cube(146, 52, 8), "Plastic_Grey", loc=(-11, 0, 8), bevel=0.8, seg=2)
    it.add(cube(142, 50, 5.4), "Rubber_Black", loc=(-11, 0, 14.3), bevel=2.6, seg=6)
    for sy in (-1, 1):
        it.add(cube(146, 11, 13.5), alu, loc=(-11, sy * 31.5, 10.75), bevel=1.2, seg=3)
        it.add(cube(138, 7.2, 0.4), "Rubber_Black", loc=(-12, sy * 31.6, 17.55), bevel=0.12, seg=1)
    # Rear end cap and feet.
    it.add(cube(6, 74, 12), black, loc=(-87, 0, 10), bevel=2.4, seg=4)
    feet = [(cube(6, 8, 4), (-87, sy * 30, 2)) for sy in (-1, 1)] + [(cube(8, 8, 4), (-20, sy * 31.5, 2)) for sy in (-1, 1)]
    it.add(merged(feet), "Rubber_Black", bevel=0.8, seg=2)
    # Motor hood at the front.
    hood = [(58, 4), (90, 4), (90, 15), (84, 26), (62, 24), (58, 18)]
    it.add(side_prism(hood, 74), black, bevel=3.0, seg=4)
    it.add(merged([(cube(0.4, 44, 0.9), (89.85, 0, z)) for z in (7.5, 9.5, 11.5)]), "Plastic_Grey", shade="flat")  # vents
    for sy in (-1, 1):
        it.add(cylinder(3.6, 3.0, 20), "Rubber_Black", loc=(86, sy * 33 + 1.5, 3.6), rot=(90, 0, 0), shade="auto")
    # Uprights leaning back towards the runner.
    for sy in (-1, 1):
        it.add(sweep([(84, sy * 37, 16), (68.6, sy * 37, 126)], circle_profile(1.0, 16, sx=2.0, sy=3.2), up=(1, 0, 0)),
               alu, shade="smooth", grain="z")
    # Console: tilted panel facing the runner.
    fa, fb = (61.0, 126.0), (81.0, 140.0)    # display face, lower and upper edge (x, z)
    ln = math.hypot(fb[0] - fa[0], fb[1] - fa[1])
    u = ((fb[0] - fa[0]) / ln, (fb[1] - fa[1]) / ln)
    n = (-u[1], u[0])                          # face normal (towards the runner and up)
    console = [fa, fb, (fb[0] - 6.5 * n[0], fb[1] - 6.5 * n[1]), (88.0, 126.0), (fa[0] - 8.0 * n[0], fa[1] - 8.0 * n[1])]
    it.add(side_prism(console, 78), black, bevel=1.5, seg=3)
    tilt = (0, -math.degrees(math.atan2(-n[0], n[1])), 0)

    def face(s, y, out):
        return (fa[0] + (fb[0] - fa[0]) * s + n[0] * out, y, fa[1] + (fb[1] - fa[1]) * s + n[1] * out)

    it.add(cube(13.5, 34, 0.5), "Screen_Black", loc=face(0.56, 0, 0.1), rot=tilt, bevel=0.15, seg=1)
    leds = [(cube(4.2, 8.0, 0.2), face(0.62, y, 0.4), tilt) for y in (-11, 11)]
    for i, h in enumerate((2, 3, 5, 6, 4, 5, 3, 2)):  # program profile as an LED bar graph
        for j in range(h):
            leds.append((cube(0.8, 0.75, 0.2), face(0.47 + 0.044 * j, -3.6 + 1.03 * i, 0.4), tilt))
    it.add(merged(leds), "Display_Blue", shade="flat")
    it.add(merged([(cube(2.4, 3.0, 0.6), face(s, y, 0.2), tilt) for s in (0.2,) for y in (-26, -22, 22, 26)]),
           "Plastic_Grey", bevel=0.2, seg=1)
    it.add(cylinder(2.0, 1.4, 24), "Plastic_Red", loc=face(0.2, -6, 0.0), rot=tilt, shade="auto")
    it.add(cylinder(1.1, 1.0, 16), "Plastic_Yellow", loc=face(0.2, 6, 0.0), rot=tilt, shade="auto")
    # Handrails with rubber grips and steel pulse sensors.
    for sy in (-1, 1):
        it.add(sweep([(71.5, sy * 38, 110.5), (40, sy * 38, 108.5), (26, sy * 38, 105.0)], circle_profile(1.8, 16)),
               "Rubber_Black", shade="smooth")
        it.add(sweep([(48, sy * 38, 109.0), (62, sy * 38, 109.9)], circle_profile(1.95, 16)), "Steel_Satin", shade="smooth")


# ---------------------------------------------------------------------------------------------
# Weight bench (140 x 40 x 110) with barbell
# ---------------------------------------------------------------------------------------------

BAR_LEN = 150.0   # the barbell lies across the rack (along Y) – the only part wider than the 40-cm bench


def plate(R, t):
    """Cast iron disc with raised rim and hub, hole for a 30-mm bar, lying on z = 0 .. t."""
    return ring_lathe([(1.6, 0), (3.6, 0), (4.0, 0.5), (R - 2.0, 0.5), (R - 1.4, 0), (R - 0.3, 0), (R, 0.3), (R, t - 0.3),
                       (R - 0.3, t), (R - 1.4, t), (R - 2.0, t - 0.5), (4.0, t - 0.5), (3.6, t), (1.6, t)], 32)


@item("Deco_weight_bench")
def weight_bench(it: Item):
    frame, tube = "Paint_Anthracite", 5.0
    ux, uy = 60.0, 17.5                       # uprights
    bars = []
    bars.append((cube(127.5, tube, tube), (-3.75, 0, 33.5)))              # spine x -67.5 .. 60
    bars.append((cube(tube, 37, tube), (-67.5, 0, 2.5)))                  # foot bar
    bars.append((cube(tube, tube, 26), (-67.5, 0, 18)))                   # foot post
    bars.append((cube(tube, 2 * uy - tube, tube), (ux, 0, 33.5)))         # cross bar under the pad
    bars.append((cube(tube, 2 * uy - tube, tube), (ux, 0, 2.5)))          # floor cross bar
    for sy in (-1, 1):
        bars.append((cube(21, tube, tube), (58.0, sy * uy, 2.5)))         # floor feet x 47.5 .. 68.5
        bars.append((cube(tube, tube, 103), (ux, sy * uy, 56.5)))          # uprights z 5 .. 108
    it.add(merged(bars), frame, bevel=0.6, seg=2, grain="z")
    caps = [(cube(5.6, 5.6, 2.0), (ux, sy * uy, 109.0)) for sy in (-1, 1)]
    caps += [(cube(5.4, 1.5, 5.4), (-67.5, sy * 19.25, 2.5)) for sy in (-1, 1)]
    caps += [(cube(1.5, 5.4, 5.4), (69.25, sy * uy, 2.5)) for sy in (-1, 1)]
    caps += [(cube(1.5, 5.4, 5.4), (46.75, sy * uy, 2.5)) for sy in (-1, 1)]
    it.add(merged(caps), "Rubber_Black", bevel=0.5, seg=2)
    # J-hooks facing the bench.
    hooks = []
    for sy in (-1, 1):
        hooks.append((sweep([(ux - 2.4, sy * uy, 92.0), (51.0, sy * uy, 92.0), (51.0, sy * uy, 97.0)], rect_profile(1.2, 4.0, 0.3),
                            up=(0, 1, 0)),))
        hooks.append((cube(1.0, 4.4, 9.0), (ux - 3.0, sy * uy, 93.5)))   # backing plate
    it.add(merged(hooks), frame, shade="auto")
    # Pad on a board; it runs between the uprights so the bar is above the lifter's eyes.
    it.add(cube(118, 24, 1.8), "Plastic_Black", loc=(2, 0, 36.9), bevel=0.3, seg=1)
    it.add(cube(120, 27, 7.2, cuts=6), "Leather_Black", loc=(2, 0, 41.4), bevel=2.4, seg=6, shade="smooth", post=puff(0.7))
    # Barbell (built along Z, then turned to lie along Y): bar, knurling, collars, plates, spring clips.
    half = BAR_LEN / 2
    parts = defaultdict(list)
    parts["Chrome"].append((cylinder(1.5, 2 * half - 2.0, 24), (0, 0, -half + 1.0)))
    for s in (-1, 1):
        z_in = 10.0 if s > 0 else -40.0
        parts["Steel"].append((cylinder(1.53, 30.0, 24), (0, 0, z_in)))
        z = 48.0 if s > 0 else -50.0
        parts["Chrome"].append((cylinder(2.5, 2.0, 24), (0, 0, z)))                         # inner collar
        parts["Chrome"].append((cylinder(1.7, 1.0, 24), (0, 0, half - 1.0 if s > 0 else -half)))  # end cap
        stack = [(15.0, 3.2), (15.0, 3.2), (11.0, 2.2)]
        pos = 50.4
        for R, t in stack:
            z0 = pos if s > 0 else -pos - t
            parts["Iron_Dark"].append((plate(R, t), (0, 0, z0)))
            pos += t + 0.2
        z0 = pos + 0.2 if s > 0 else -pos - 0.2 - 2.4
        parts["Chrome"].append((ring_lathe([(1.55, 0), (2.4, 0), (2.6, 0.3), (2.6, 2.1), (2.4, 2.4), (1.55, 2.4)], 24), (0, 0, z0)))
    bar_loc, bar_rot = (53.5, 0, 92.6 + 1.5), (-90, 0, 0)
    for mat, lst in parts.items():
        it.add(merged(lst), mat, loc=bar_loc, rot=bar_rot, shade="auto", angle=40, grain="z")


# ---------------------------------------------------------------------------------------------
# Sauna bench (220 x 55 x 45)
# ---------------------------------------------------------------------------------------------


@item("Deco_sauna_bench")
def sauna_bench(it: Item):
    """Light wood: six seat slats along X, four support frames, vertical back slats closing the side against
    the wall (+Y), front apron, a folded towel."""
    wood = "Wood_Light"
    L, D = 220.0, 55.0
    seat_top, slat_t = 43.5, 2.5
    n, gap = 6, 1.5
    w = (D - (n - 1) * gap) / n
    slats = [(cube(L, w, slat_t), (0, -D / 2 + w / 2 + i * (w + gap), seat_top - slat_t / 2)) for i in range(n)]
    it.add(merged(slats), wood, bevel=0.7, seg=2, grain="x")
    leg_h = seat_top - slat_t - 4.4
    across, legs = [], []
    for x in (-100.0, -33.3, 33.3, 100.0):
        across.append((cube(4.4, 50.0, 4.4), (x, 0, leg_h + 2.2)))       # bearer under the slats
        across.append((cube(3.4, 41.0, 3.4), (x, 0, 9.0)))               # low stretcher
        for y in (-22.8, 22.8):
            legs.append((cube(4.4, 4.4, leg_h), (x, y, leg_h / 2)))
    it.add(merged(across), wood, bevel=0.4, seg=2, grain="y")
    # Back slats against the wall and front apron.
    ns, gs = 23, 1.4
    ws = (L - (ns - 1) * gs) / ns
    for i in range(ns):
        legs.append((cube(ws, 2.0, leg_h - 1.0), (-L / 2 + ws / 2 + i * (ws + gs), 26.0, 1.0 + (leg_h - 1.0) / 2)))
    it.add(merged(legs), wood, bevel=0.4, seg=2, grain="z")
    rails = [(cube(L, 2.0, 4.4), (0, 26.0, leg_h + 2.2)),                  # top rail holding the back slats
             (cube(L, 2.0, 6.4), (0, -26.0, leg_h + 4.4 - 3.2))]           # front apron under the seat edge
    it.add(merged(rails), wood, bevel=0.5, seg=2, grain="x")
    # Folded towel.
    it.add(cube(46, 32, 2.0, cuts=4), "Linen_White", loc=(58, -3, seat_top + 1.0), rot=(0, 0, 7), bevel=0.8, seg=3,
           shade="smooth", post=puff(0.3))
