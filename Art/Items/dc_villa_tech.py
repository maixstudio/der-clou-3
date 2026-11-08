"""Villa technology & security (G4): heating, fuse box, alarm central, alarm keypad, metal shelf module.

Exported as SM_<id>. Conventions: cm, origin bottom centre, front = -Y, width along X.

Wall parts (fuse box, alarm central, keypad): origin at the bottom centre of the part itself, the back lies on the
wall at y = +depth/2 and everything sticks out towards -Y. The level hangs them at the right height.
The heating stands on the floor with its back (pipes, flue) against the wall at y = +60.
The metal shelf is a 100-cm module that is lined up in the level: nothing sticks out beyond x = -50 .. +50, the
posts end flat on the module edge so two neighbours stand post to post.
"""

import math

import bmesh
import bpy
from mathutils import Vector

from dc_items import item
from dc_mesh import CM, Item, circle_profile, cube, cylinder, lathe, ngon_prism, rect_profile, sweep, torus, wobble

FRONT = (90, 0, 0)    # local +Z -> world -Y (a lathe/cylinder pointing out of a front face)
BACK = (-90, 0, 0)    # local +Z -> world +Y (into the wall)
SIDE_L = (0, -90, 0)  # local +Z -> world -X
SIDE_R = (0, 90, 0)   # local +Z -> world +X


# ---------------------------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------------------------


def rounded(points, radius, steps=4):
    """Polyline with filleted corners (quadratic arcs), cm."""
    pts = [Vector(p) for p in points]
    if len(pts) < 3 or radius <= 0:
        return [tuple(p) for p in pts]
    out = [pts[0]]
    for i in range(1, len(pts) - 1):
        p0, p1, p2 = pts[i - 1], pts[i], pts[i + 1]
        a, b = p1 - p0, p2 - p1
        if a.length < 1e-6 or b.length < 1e-6:
            continue
        an, bn = a.normalized(), b.normalized()
        if (an - bn).length < 1e-4:
            out.append(p1)
            continue
        d = min(radius, a.length / 2, b.length / 2)
        t0, t1 = p1 - an * d, p1 + bn * d
        for s in range(steps + 1):
            t = s / steps
            out.append(t0 * (1 - t) ** 2 + p1 * (2 * (1 - t) * t) + t1 * (t * t))
    out.append(pts[-1])
    return [tuple(p) for p in out]


def cut_polyline(points, cut0, cut1):
    """Same polyline, shortened by the arc lengths cut0 at the start and cut1 at the end."""

    def cut_start(p, c):
        while c > 0 and len(p) > 1:
            seg = (p[1] - p[0]).length
            if seg > c:
                p[0] = p[0] + (p[1] - p[0]).normalized() * c
                return p
            c -= seg
            p.pop(0)
        return p

    pts = cut_start([Vector(p) for p in points], cut0)
    pts = list(reversed(cut_start(list(reversed(pts)), cut1)))
    return [tuple(p) for p in pts]


def pipe(it, points, r, mat="Copper", bend=None, seg=12):
    """Round pipe along a polyline with bent corners."""
    bend = max(2.4 * r, 2.5) if bend is None else bend
    it.add(sweep(rounded(points, bend), circle_profile(r, seg)), mat, shade="auto", angle=50, grain="x")


def insulated(it, points, r, ins_r, bend=7.0, cut0=3.5, cut1=3.5, mat="Copper", ins="Rubber_Black"):
    """Copper pipe with a black foam sleeve; the copper shows at both ends."""
    path = rounded(points, bend)
    it.add(sweep(path, circle_profile(r, 12)), mat, shade="auto", angle=50)
    it.add(sweep(cut_polyline(path, cut0, cut1), circle_profile(ins_r, 14)), ins, shade="auto", angle=50)


def rosette(it, x, z, r, wall_y=60.0, mat="Plastic_White"):
    """Cover ring where a pipe enters the wall (wall plane y = wall_y, ring sticks out towards -Y)."""
    it.add(cylinder(r, 0.6, 20), mat, loc=(x, wall_y - 0.6, z), rot=BACK, shade="auto")


def dome(r, h, seg=16, steps=5):
    """Quarter-ellipse dome on z = 0 (LED lens, beacon)."""
    prof = [(0, 0)] + [(r * math.cos(math.pi / 2 * i / steps), h * math.sin(math.pi / 2 * i / steps)) for i in range(steps)]
    return lathe(prof + [(0, h)], seg)


def led(it, loc, mat, r=0.45):
    """Small LED in a black bezel on a -Y facing surface (loc = point on that surface)."""
    x, y, z = loc
    it.add(cylinder(r * 1.55, 0.15, 14), "Plastic_Black", loc=(x, y, z), rot=FRONT, shade="auto")
    it.add(dome(r, r * 0.9, 12, 3), mat, loc=(x, y - 0.15, z), rot=FRONT, shade="smooth")


def screw(it, loc, r=0.45, facing=FRONT, mat="Chrome"):
    it.add(dome(r, r * 0.45, 12, 3), mat, loc=loc, rot=facing, shade="smooth")


def text_bm(text, size, depth, res=2):
    """Raised letters (Blender's built-in font): centred on the origin in XY, from z = 0 to z = depth (cm)."""
    cu = bpy.data.curves.new("dc_text", "FONT")
    cu.body = text
    cu.size = size * CM
    cu.extrude = depth * CM / 2
    cu.align_x = "CENTER"
    cu.align_y = "CENTER"
    cu.resolution_u = res
    ob = bpy.data.objects.new("dc_text", cu)
    bpy.context.scene.collection.objects.link(ob)
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
    bm = bmesh.new()
    bm.from_mesh(me)
    bpy.data.objects.remove(ob, do_unlink=True)
    bpy.data.curves.remove(cu)
    bpy.data.meshes.remove(me)
    bmesh.ops.translate(bm, verts=bm.verts[:], vec=(0.0, 0.0, depth * CM / 2))
    return bm


def front_text(it, text, mat, loc, size, depth=0.2):
    """Letters standing on a -Y facing surface; loc = centre of the text on that surface."""
    it.add(text_bm(text, size, depth), mat, loc=loc, rot=FRONT, shade="flat")


def frame_panel(w, h, t, iw, ih, ix=0.0, iz=0.0):
    """Plate in the XZ plane (thickness t along Y, centred) with a rectangular hole iw x ih at (ix, iz)."""
    bm = bmesh.new()

    def ring(y):
        o = [(-w / 2, -h / 2), (w / 2, -h / 2), (w / 2, h / 2), (-w / 2, h / 2)]
        i = [(ix - iw / 2, iz - ih / 2), (ix + iw / 2, iz - ih / 2), (ix + iw / 2, iz + ih / 2), (ix - iw / 2, iz + ih / 2)]
        return ([bm.verts.new((x * CM, y * CM, z * CM)) for x, z in o], [bm.verts.new((x * CM, y * CM, z * CM)) for x, z in i])

    fo, fi = ring(-t / 2)
    bo, bi = ring(t / 2)
    for k in range(4):
        n = (k + 1) % 4
        bm.faces.new((fo[k], fo[n], fi[n], fi[k]))
        bm.faces.new((bo[n], bo[k], bi[k], bi[n]))
        bm.faces.new((fo[n], fo[k], bo[k], bo[n]))
        bm.faces.new((fi[k], fi[n], bi[n], bi[k]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def flat_poly(points, t):
    """Flat (possibly concave) polygon, triangulated, extruded from z = 0 to z = t."""
    bm = ngon_prism(points, t)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    return bm


def warning_sign(it, loc, size):
    """Yellow triangle with a lightning bolt on a -Y facing surface; loc = centroid on that surface."""
    s = size
    h = s * math.sqrt(3) / 2

    def tri(k):
        return [(-s / 2 * k, -h / 3 * k), (s / 2 * k, -h / 3 * k), (0, 2 * h / 3 * k)]

    x, y, z = loc
    it.add(ngon_prism(tri(1.0), 0.06), "Plastic_Black", loc=(x, y, z), rot=FRONT, shade="flat")
    it.add(ngon_prism(tri(0.8), 0.05), "Paint_Yellow", loc=(x, y - 0.06, z), rot=FRONT, shade="flat")
    bolt = [(-0.02, 0.40), (0.10, 0.40), (0.03, 0.13), (0.11, 0.13), (-0.09, -0.25), (-0.02, 0.05), (-0.10, 0.05)]
    bolt = [(bx * 0.75 * s, (by * 0.75 - 0.02) * s) for bx, by in bolt]
    it.add(flat_poly(list(reversed(bolt)), 0.04), "Plastic_Black", loc=(x, y - 0.11, z), rot=FRONT, shade="flat")


def gauge(it, loc, r, depth=2.0, rim="Chrome", red_zone=True, needle=-25.0, dial="Plastic_White", case="Steel"):
    """Round dial gauge (manometer/thermometer) on a -Y facing surface; loc = centre of its back."""
    x, y, z = loc
    it.add(lathe([(0, 0), (r * 0.97, 0), (r * 0.97, depth * 0.72), (0, depth * 0.72)], 24), case, loc=(x, y, z), rot=FRONT,
           shade="auto")
    bez = [(r * 0.84, depth * 0.66), (r, depth * 0.66), (r, depth * 0.93), (r * 0.95, depth), (r * 0.84, depth),
           (r * 0.84, depth * 0.66)]
    it.add(lathe(bez, 28, caps=False), rim, loc=(x, y, z), rot=FRONT, shade="auto", angle=40)
    fy = y - depth * 0.72
    it.add(cylinder(r * 0.86, 0.04, 28), dial, loc=(x, fy, z), rot=FRONT, shade="auto")
    for i in range(9):
        a = -135 + 270 * i / 8
        rr = r * 0.66
        mat = "Paint_Red" if red_zone and i >= 7 else "Plastic_Black"
        it.add(cube(max(0.06, r * 0.04), 0.03, r * 0.17), mat,
               loc=(x + rr * math.sin(math.radians(a)), fy - 0.05, z + rr * math.cos(math.radians(a))), rot=(0, a, 0))
    nl = r * 0.68
    bm = cube(max(0.06, r * 0.045), 0.03, nl)
    bmesh.ops.translate(bm, verts=bm.verts[:], vec=(0, 0, (nl / 2 - nl * 0.18) * CM))
    it.add(bm, "Paint_Red", loc=(x, fy - 0.1, z), rot=(0, needle, 0))
    it.add(cylinder(r * 0.1, 0.12, 12), "Plastic_Black", loc=(x, fy - 0.06, z), rot=FRONT, shade="auto")
    it.add(cylinder(r * 0.85, 0.06, 28), "Glass", loc=(x, y - depth * 0.9, z), rot=FRONT, shade="flat")


# ---------------------------------------------------------------------------------------------
# Heating: gas boiler + hot-water tank + copper pipes (120 x 120 x 180, back = wall at y = +60)
# ---------------------------------------------------------------------------------------------


@item("Deco_heating")
def heating(it: Item):
    wall = 60.0

    # --- Hot-water tank (left front): white cylinder with grey cap, on three feet --------------------
    cx, cy, R = -30.0, -28.0, 30.0
    for k in range(3):
        a = math.radians(90 + 120 * k)
        it.add(cylinder(2.4, 3.0, 16), "Iron_Dark", loc=(cx + 21 * math.cos(a), cy + 21 * math.sin(a), 0), shade="auto")
    it.add(lathe([(0, 2.8), (28.4, 2.8), (29.4, 3.8), (29.4, 9.0), (0, 9.0)], 48), "Plastic_Grey", loc=(cx, cy, 0),
           shade="auto", angle=40)
    it.add(lathe([(0, 8.5), (R, 8.5), (R, 162.0), (0, 162.0)], 48), "Plastic_White", loc=(cx, cy, 0), shade="auto", angle=40,
           grain="z")
    it.add(lathe([(0, 161.5), (R + 0.3, 161.5), (R + 0.3, 165.5), (R - 0.7, 167.6), (R - 6, 169.5), (12, 170.4), (0, 170.6)],
                 48), "Plastic_Grey", loc=(cx, cy, 0), shade="auto", angle=40)
    it.add(cylinder(3.0, 1.8, 16), "Plastic_Black", loc=(cx + 10, cy - 8, 169.4), shade="auto")  # anode cap
    fy = cy - R  # front of the tank (-58)
    # Inspection flange with bolts.
    it.add(lathe([(0, -1.6), (9, -1.6), (9, 1.4), (8.3, 2.1), (0, 2.1)], 32), "Plastic_Grey", loc=(cx, fy, 30), rot=FRONT,
           shade="auto", angle=40)
    for k in range(8):
        a = 2 * math.pi * k / 8
        it.add(cylinder(0.6, 0.7, 6), "Chrome", loc=(cx + 7.2 * math.cos(a), fy - 1.6, 30 + 7.2 * math.sin(a)), rot=FRONT,
               shade="flat")
    gauge(it, (cx, fy + 0.3, 116), 3.6, depth=2.0, red_zone=False, needle=35)  # thermometer
    it.add(cube(6, 0.25, 8), "Paper", loc=(cx, fy - 0.05, 137), bevel=0.1, seg=1, fit="y")  # blank rating plate
    for k in range(4):
        it.add(cube(4.2 if k else 3.0, 0.05, 0.35), "Plastic_Black", loc=(cx - (0.6 if k else 0), fy - 0.2, 139.6 - 1.4 * k))

    # --- Gas condensing boiler (right): white floor-standing unit ----------------------------------------
    bx, by0, by1, top = 32.0, -58.8, 14.0, 142.0
    byc, bd = (by0 + by1) / 2, by1 - by0
    for sx in (-1, 1):
        for sy in (-1, 1):
            it.add(cylinder(1.8, 2.0, 12), "Rubber_Black", loc=(bx + sx * 23, byc + sy * 31, 0), shade="auto")
    it.add(cube(50, bd - 6, 3), "Plastic_Black", loc=(bx, byc, 2.5))
    it.add(cube(56, bd, top - 4), "Plastic_White", loc=(bx, byc, 4 + (top - 4) / 2), bevel=1.2, seg=3, grain="z")
    it.add(cube(50, bd - 8, 0.3), "Plastic_Grey", loc=(bx, byc + 1, top + 0.1), bevel=0.1, seg=1)  # top service lid
    # Front door and control fascia with dark gaps around them.
    door_z0, door_z1 = 7.0, 107.0
    it.add(cube(54.4, 0.3, door_z1 - door_z0 + 0.8), "Plastic_Black", loc=(bx, by0 - 0.12, (door_z0 + door_z1) / 2))
    it.add(cube(53.6, 1.2, door_z1 - door_z0), "Plastic_White", loc=(bx, by0 - 0.6, (door_z0 + door_z1) / 2), bevel=0.6, seg=2,
           grain="z")
    fz0, fz1 = 108.6, 139.0
    it.add(cube(54.4, 0.3, fz1 - fz0 + 0.8), "Plastic_Black", loc=(bx, by0 - 0.12, (fz0 + fz1) / 2))
    it.add(cube(53.6, 1.2, fz1 - fz0), "Plastic_Grey", loc=(bx, by0 - 0.6, (fz0 + fz1) / 2), bevel=0.6, seg=2)
    face = by0 - 1.2  # -60
    for k in range(5):  # air slots at the bottom of the door
        it.add(cube(32, 0.15, 0.8), "Plastic_Black", loc=(bx, face - 0.05, 12 + 2.2 * k))
    it.add(cube(16, 0.15, 1.0), "Plastic_Black", loc=(bx, face - 0.05, door_z1 - 2.6))  # grip recess
    # Fascia: manometer, display, buttons, knob, power switch.
    gauge(it, (bx - 18.5, face, 124.5), 3.8, depth=1.6, red_zone=True, needle=-40)
    it.add(cube(15, 0.3, 7), "Screen_Black", loc=(bx - 3, face - 0.15, 127), bevel=0.1, seg=1)
    it.add(cube(13, 0.12, 5), "Display_Blue", loc=(bx - 3, face - 0.33, 127))
    front_text(it, "1,8 bar", "Screen_Black", (bx - 3, face - 0.39, 126.8), 2.2, depth=0.05)
    for k in range(3):
        it.add(lathe([(0, 0), (1.0, 0), (1.0, 0.35), (0.85, 0.5), (0, 0.5)], 16), "Plastic_White",
               loc=(bx + 8.5 + 3.3 * k, face, 127), rot=FRONT, shade="auto")
    it.add(lathe([(0, 0), (2.5, 0), (2.5, 1.1), (2.1, 1.5), (0, 1.6)], 32, ridges=(24, 0.12, 1, 2)), "Plastic_Black",
           loc=(bx + 20, face, 124.5), rot=FRONT, shade="auto", angle=50)
    it.add(cube(0.35, 0.1, 1.4), "Plastic_White", loc=(bx + 20, face - 1.6, 125.6))
    it.add(cube(2.4, 0.8, 1.5), "Plastic_Black", loc=(bx + 8.5, face - 0.4, 117.5), bevel=0.2, seg=2)
    led(it, (bx + 12.5, face, 117.5), "LED_Green", 0.4)
    it.add(cube(50, 0.08, 0.25), "Plastic_Black", loc=(bx, face - 0.04, 113.5))  # service flap seam
    # Flue: concentric white pipe up from the boiler, back into the chimney.
    flue = [(bx, 0, top), (bx, 0, 164), (bx, wall, 164)]
    pipe(it, flue, 6.3, "Plastic_White", bend=10, seg=24)
    it.add(lathe([(0, 0), (7.4, 0), (7.4, 3.2), (6.6, 4.0), (0, 4.0)], 24), "Steel", loc=(bx, 0, top), shade="auto")
    it.add(cylinder(6.65, 1.4, 24), "Steel", loc=(bx, 0, 151), shade="auto")
    it.add(cylinder(6.65, 1.4, 24), "Steel", loc=(bx, 36, 164), rot=BACK, shade="auto")
    rosette(it, bx, 164, 10.5)

    # --- Heating circuit: insulated flow/return up to a pump group on the wall ----------------------------
    pg_x, pg_y, pg_z0, pg_z1 = 50.0, 50.0, 152.0, 174.0
    for px in (45.0, 55.0):
        it.add(cylinder(1.9, 1.6, 6), "Brass", loc=(px, 7, top + 0.6), shade="flat")  # union nut
        insulated(it, [(px, 7, top - 1), (px, 7, 149), (px, pg_y, 149), (px, pg_y, pg_z0 + 8)], 1.4, 2.7, bend=6, cut0=4.0,
                  cut1=0.5)
        pipe(it, [(px, pg_y, pg_z1 - 1), (px, pg_y, 177.5), (px, wall, 177.5)], 1.4, bend=4)
        rosette(it, px, 177.5, 2.4)
    it.add(cube(20, 2, 26), "Steel", loc=(pg_x, wall - 1, (pg_z0 + pg_z1) / 2), bevel=0.2, seg=1)  # wall bracket
    it.add(cube(20, 14, pg_z1 - pg_z0), "Plastic_Black", loc=(pg_x, pg_y - 1, (pg_z0 + pg_z1) / 2), bevel=1.2, seg=3)
    pf = pg_y - 8  # front of the insulation shell (42)
    gauge(it, (45, pf, 169), 2.4, depth=1.2, rim="Paint_Red", red_zone=False, needle=20)
    gauge(it, (55, pf, 169), 2.4, depth=1.2, rim="Porcelain_Blue", red_zone=False, needle=-30)
    it.add(cube(9.5, 1.5, 9.5), "Paint_Grey", loc=(45, pf - 0.6, 157.5), bevel=0.6, seg=2)  # pump body
    it.add(lathe([(0, 0), (4.0, 0), (4.0, 3.0), (3.4, 3.8), (0, 3.9)], 28), "Plastic_Grey", loc=(45, pf - 1.3, 157.5),
           rot=FRONT, shade="auto", angle=40)
    led(it, (45, pf - 5.2, 158.8), "LED_Green", 0.35)
    it.add(cube(3.0, 0.1, 1.0), "Screen_Black", loc=(45, pf - 5.25, 156.6))
    for k, z in enumerate((156.0, 163.0)):  # ball valve levers
        it.add(cube(1.0, 1.0, 5.0), "Paint_Red" if k == 0 else "Porcelain_Blue", loc=(55, pf - 0.6, z), rot=(0, 90, 0),
               bevel=0.25, seg=2)

    # --- Coil flow/return between tank and boiler (insulated, behind the units) ---------------------------
    insulated(it, [(-36, 0.5, 100), (-36, 24, 100), (12, 24, 100), (12, by1 - 1, 100)], 1.4, 2.6, bend=6)
    insulated(it, [(-24, 0.5, 60), (-24, 24, 60), (18, 24, 60), (18, by1 - 1, 60)], 1.4, 2.6, bend=6)

    # --- Hot water out (top of the tank) and circulation back into the wall --------------------------------
    pipe(it, [(cx, -16, 169), (cx, -16, 176), (cx, wall, 176)], 1.1, bend=4)
    it.add(cylinder(1.7, 1.6, 6), "Brass", loc=(cx, -16, 172), shade="flat")
    rosette(it, cx, 176, 2.6)
    pipe(it, [(-14, -2.0, 132), (-14, 56, 132), (-14, 56, 172), (-14, wall, 172)], 0.8, bend=3)
    it.add(cylinder(1.6, 5.0, 16), "Brass", loc=(-14, 56, 146), shade="auto")  # circulation pump
    it.add(lathe([(0, 0), (2.6, 0), (2.6, 3.4), (2.2, 4.2), (0, 4.2)], 20), "Paint_Grey", loc=(-14, 54.6, 148.5), rot=FRONT,
           shade="auto", angle=40)
    rosette(it, -14, 172, 2.2)

    # --- Cold water from the floor into the tank, with valve and safety group ------------------------------
    pipe(it, [(-26, 56, 0), (-26, 56, 20), (-26, 1.0, 20)], 1.1, bend=4)
    it.add(cylinder(3.0, 0.4, 20), "Plastic_White", loc=(-26, 56, 0), shade="auto")
    it.add(cylinder(1.8, 4.0, 6), "Brass", loc=(-26, 40, 20), rot=BACK, shade="flat")  # ball valve
    it.add(cube(1.0, 1.2, 1.4), "Brass", loc=(-26, 42, 22.4))
    it.add(cube(7.0, 1.4, 0.9), "Porcelain_Blue", loc=(-23.5, 42, 23.4), bevel=0.25, seg=2)
    it.add(cylinder(2.0, 5.0, 16), "Brass", loc=(-26, 12, 20), rot=BACK, shade="auto")  # safety group
    it.add(cylinder(1.3, 5.0, 16), "Brass", loc=(-26, 14.5, 20), shade="auto")
    it.add(cylinder(1.6, 1.8, 16), "Paint_Red", loc=(-26, 14.5, 25), shade="auto")

    # --- Expansion vessel (red, hung on the wall above the floor clutter) with its line to the boiler -------
    ex, ey, ez0, ez1, er = -45.0, 43.5, 108.0, 152.0, 15.0
    it.add(lathe([(0, ez0), (8, ez0 + 0.3), (12, ez0 + 1.8), (14.5, ez0 + 4.5), (er, ez0 + 7), (er, ez1 - 7), (14.5, ez1 - 4.5),
                  (12, ez1 - 1.8), (8, ez1 - 0.3), (0, ez1)], 36), "Paint_Red", loc=(ex, ey, 0), shade="auto", angle=40, grain="z")
    it.add(torus(er + 0.35, 0.45, 40, 8), "Steel", loc=(ex, ey, ez1 - 12), shade="smooth")  # strap
    it.add(cube(8, 1.6, 12), "Steel", loc=(ex, wall - 0.8, ez1 - 12), bevel=0.3, seg=1)  # wall bracket
    it.add(cube(3, wall - 1.6 - (ey + er), 3), "Steel", loc=(ex, (wall - 1.6 + ey + er) / 2, ez1 - 12))
    it.add(cube(5, 0.1, 6), "Paper", loc=(ex, ey - er - 0.12, ez0 + 24), fit="y")  # sticker
    it.add(cylinder(1.5, 3.0, 6), "Brass", loc=(ex, ey, ez0 - 2.6), shade="flat")
    pipe(it, [(ex, ey, ez0 - 1), (ex, ey, 68), (ex, 30, 68), (6, 30, 68), (6, by1 - 1, 68)], 0.9, bend=3.5)
    gauge(it, (ex, ey - 1.0, 84), 2.6, depth=1.4, red_zone=True, needle=-60)  # manometer on the vessel line

    # --- Gas line (yellow) from the wall into the boiler, with a yellow ball valve -------------------------
    pipe(it, [(24, wall, 26), (24, by1 - 1, 26)], 1.3, "Paint_Yellow")
    it.add(cylinder(2.1, 5.0, 6), "Brass", loc=(24, 32, 26), rot=BACK, shade="flat")
    it.add(cube(1.0, 1.0, 1.6), "Brass", loc=(24, 34.5, 28.6))
    it.add(cube(8.0, 1.6, 1.0), "Paint_Yellow", loc=(27.4, 34.5, 29.8), bevel=0.3, seg=2)
    rosette(it, 24, 26, 3.0)

    # --- Power cable from the boiler to a junction box on the wall ------------------------------------------
    it.add(cube(8, 4, 8), "Plastic_White", loc=(56, wall - 2, 133), bevel=0.6, seg=2)
    pipe(it, [(56, by1 - 1, 133), (56, wall - 4, 133)], 0.45, "Plastic_Grey")


# ---------------------------------------------------------------------------------------------
# Fuse box: grey wall cabinet with a window door (80 x 18 x 80, back on the wall at y = +9)
# ---------------------------------------------------------------------------------------------


@item("Deco_fuse_box")
def fuse_box(it: Item):
    W, H = 80.0, 80.0
    grey = "Paint_Grey"
    back, front = 9.0, -7.4
    it.add(cube(W, 1.6, H), grey, loc=(0, back - 0.8, H / 2), bevel=0.6, seg=2)  # back plate
    it.add(frame_panel(W, H, back - 1.6 - front, W - 2.4, H - 2.4), grey, loc=(0, (back - 1.6 + front) / 2, H / 2), bevel=1.0, seg=3)
    # Interior: mounting plate, DIN rows behind a white cover with four slots.
    it.add(cube(W - 2.4, 0.6, H - 2.4), "Steel", loc=(0, back - 1.9, H / 2))
    cov_y = -0.8  # front face of the cover
    rows = (65.0, 54.0, 43.0, 32.0, 21.5)  # four device rows, terminals at the bottom
    slot_h, slot_x, nose_h = 5.6, 27.5, 4.5
    it.add(cube(76, 1.0, 76), "Plastic_Black", loc=(0, 1.0, H / 2))  # dark rail zone behind the slots
    edges = [77.6 / 2 + H / 2]
    for z in rows:
        edges += [z + slot_h / 2, z - slot_h / 2]
    edges.append(H / 2 - 77.6 / 2)
    for k in range(0, len(edges), 2):  # horizontal cover strips between the slots
        z_hi, z_lo = edges[k], edges[k + 1]
        it.add(cube(77.6, 0.6, z_hi - z_lo), "Plastic_White", loc=(0, cov_y + 0.3, (z_hi + z_lo) / 2))
    for i, z in enumerate(rows):  # cover pieces left and right of each slot, label holders above the device rows
        for sx in (-1, 1):
            it.add(cube(38.8 - slot_x, 0.6, slot_h), "Plastic_White", loc=(sx * (slot_x + (38.8 - slot_x) / 2), cov_y + 0.3, z))
        if i < 4:
            lz = z + slot_h / 2 + 1.25
            it.add(cube(2 * slot_x - 4, 0.15, 1.6), "Plastic_Grey", loc=(0, cov_y - 0.07, lz), bevel=0.05, seg=1)
            it.add(cube(2 * slot_x - 4.6, 0.08, 1.1), "Paper", loc=(0, cov_y - 0.18, lz), fit="y")
            for k in range(7):  # handwritten circuit names (dashes)
                it.add(cube(3.2 + (k * 1.7) % 2.4, 0.03, 0.22), "Plastic_Black", loc=(-21 + 7.0 * k, cov_y - 0.23, lz + 0.05))
    # Devices: modules 1.8 wide. Top row: main switch, two RCDs, surge protector. Other rows: breakers.
    mod = 1.8

    def device(x0, n, z, body="Plastic_White", lever="Plastic_Black", down=False, test=False):
        w = n * mod - 0.1
        cxm = x0 + n * mod / 2
        it.add(cube(w, 3.2, nose_h), body, loc=(cxm, cov_y + 0.6, z))
        lw = min(w - 0.4, 0.8 + 0.6 * (n - 1))
        it.add(cube(lw, 1.1, 1.3), lever, loc=(cxm, cov_y - 1.5, z + (-0.7 if down else 0.7)), bevel=0.15, seg=1)
        if test:
            it.add(cube(0.7, 0.4, 0.7), "Plastic_Yellow", loc=(cxm + w / 2 - 0.8, cov_y - 1.1, z - 1.4))

    def blank(x0, x1, z):
        it.add(cube(x1 - x0, 0.9, nose_h), "Plastic_White", loc=((x0 + x1) / 2, cov_y - 0.15, z))

    x = -slot_x + 0.4
    device(x, 3, rows[0], "Plastic_Grey", "Paint_Red")
    x += 3 * mod + 0.6
    for _ in range(2):
        device(x, 4, rows[0], test=True)
        x += 4 * mod
    x += 0.6
    device(x, 4, rows[0], "Plastic_Grey")
    x += 4 * mod
    blank(x + 0.2, slot_x - 0.4, rows[0])
    fill = {1: 28, 2: 24, 3: 17}
    tripped = {(2, 9)}
    for r_i in (1, 2, 3):
        n = fill[r_i]
        x0 = -slot_x + 0.4
        for k in range(n):
            device(x0 + k * mod, 1, rows[r_i], down=(r_i, k) in tripped)
        if n < 29:
            blank(x0 + n * mod + 0.2, slot_x - 0.4, rows[r_i])
    for k in range(40):  # terminal blocks: neutral blue, earth green, phases grey
        mat = "Porcelain_Blue" if 14 <= k < 22 else "Paint_Green" if 22 <= k < 28 else "Plastic_Grey"
        it.add(cube(1.0, 2.6, 4.0), mat, loc=(-slot_x + 1.2 + k * 1.3, cov_y + 0.4, rows[4]), bevel=0.1, seg=1)
    # Door with window.
    dw, dh, dt = 74.8, 74.8, 1.6
    wz0, wz1, wx = 15.0, 71.0, 30.0
    it.add(frame_panel(dw + 0.8, dh + 0.8, 0.3, 2 * wx, wz1 - wz0, 0, (wz0 + wz1) / 2 - H / 2), "Plastic_Black",
           loc=(0, front - 0.12, H / 2))  # dark gap around the door
    it.add(frame_panel(dw, dh, dt, 2 * wx, wz1 - wz0, 0, (wz0 + wz1) / 2 - H / 2), grey, loc=(0, front - dt / 2, H / 2),
           bevel=0.5, seg=2)
    dfront = front - dt  # -9
    it.add(cube(2 * wx + 1.0, 0.3, wz1 - wz0 + 1.0), "Glass_Tinted", loc=(0, front - 0.6, (wz0 + wz1) / 2), shade="flat")
    it.add(frame_panel(2 * wx + 1.6, wz1 - wz0 + 1.6, 0.3, 2 * wx - 0.4, wz1 - wz0 - 0.4), "Rubber_Black",
           loc=(0, dfront - 0.1, (wz0 + wz1) / 2))
    # Lock with lever, hinges, warning sign.
    it.add(cylinder(1.3, 0.4, 20), "Chrome", loc=(33.6, dfront, 40), rot=FRONT, shade="auto")
    it.add(cube(1.5, 1.0, 6.0), "Plastic_Black", loc=(33.6, dfront - 0.9, 38.4), bevel=0.4, seg=2)
    it.add(cube(0.25, 0.1, 0.9), "Chrome", loc=(33.6, dfront - 1.45, 40.6))
    for z in (14.0, 66.0):
        it.add(cylinder(0.75, 7.0, 12), grey, loc=(-37.7, front - 0.6, z - 3.5), shade="auto")
    warning_sign(it, (0, dfront, 7.6), 7.5)
    # Cable glands on top.
    for gx in (-24.0, -8.0, 8.0, 24.0):
        it.add(lathe([(0, 0), (1.9, 0), (1.9, 0.5), (1.3, 0.9), (0.9, 0.9), (0, 0.9)], 6), "Plastic_Grey", loc=(gx, 0.5, H),
               shade="flat")


# ---------------------------------------------------------------------------------------------
# Alarm central: wall panel with control unit, battery box, power supply, siren + red beacon, cable duct
# (120 x 18 x 60, back on the wall at y = +9)
# ---------------------------------------------------------------------------------------------


@item("Deco_alarm_central")
def alarm_central(it: Item):
    W, H = 120.0, 60.0
    back = 9.0
    board_front = back - 1.6  # 7.4
    it.add(cube(W, 1.6, H), "Paint_Anthracite", loc=(0, back - 0.8, H / 2), bevel=0.4, seg=2)
    for sx in (-1, 1):
        for z in (3.0, 57.0):
            screw(it, (sx * 57.5, board_front, z), 0.6)
    # Cable ducts (white): along the bottom and up the left side.
    it.add(cube(116, 5.2, 5.6), "Plastic_White", loc=(0, board_front - 2.6, 3.6))
    it.add(cube(116.4, 0.6, 6.0), "Plastic_White", loc=(0, board_front - 5.5, 3.6), bevel=0.2, seg=1)
    it.add(cube(8.0, 5.2, 50.6), "Plastic_White", loc=(-54, board_front - 2.6, 6.6 + 25.3))
    it.add(cube(8.4, 0.6, 50.6), "Plastic_White", loc=(-54, board_front - 5.5, 6.6 + 25.3), bevel=0.2, seg=1)

    # --- Control unit ---------------------------------------------------------------------------------------
    ux, uz0, uz1 = -21.0, 10.0, 52.0
    uw, uh = 50.0, uz1 - uz0
    u_front = -7.6
    it.add(cube(uw, board_front - u_front, uh), "Paint_White", loc=(ux, (board_front + u_front) / 2, (uz0 + uz1) / 2),
           bevel=1.0, seg=3)
    it.add(cube(uw - 1.2, 0.3, uh - 1.2), "Plastic_Black", loc=(ux, u_front - 0.1, (uz0 + uz1) / 2))  # lid gap
    it.add(cube(uw - 2.0, 1.2, uh - 2.0), "Plastic_White", loc=(ux, u_front - 0.6, (uz0 + uz1) / 2), bevel=0.5, seg=2)
    lf = u_front - 1.2  # lid face (-8.8)
    front_text(it, "ALARM", "Paint_Red", (ux, lf, 46.2), 4.4, depth=0.25)
    it.add(cube(40, 0.12, 0.5), "Paint_Red", loc=(ux, lf - 0.06, 42.6))
    it.add(cube(20, 0.3, 7.5), "Screen_Black", loc=(ux, lf - 0.15, 36.4), bevel=0.1, seg=1)
    it.add(cube(18, 0.12, 5.5), "Display_Blue", loc=(ux, lf - 0.36, 36.4))
    front_text(it, "SCHARF", "Screen_Black", (ux, lf - 0.42, 36.2), 2.6, depth=0.05)
    for k, mat in enumerate(("LED_Green", "LED_Red", "LED_Red", "Plastic_Yellow", "LED_Green", "Plastic_Yellow")):
        led(it, (-41.0 + 4.0 * k, lf, 29.5), mat, 0.5)
    # Key switch.
    it.add(cylinder(2.2, 0.3, 24), "Chrome", loc=(-38, lf, 18), rot=FRONT, shade="auto")
    it.add(lathe([(0, 0), (1.4, 0), (1.4, 0.8), (1.2, 1.0), (0, 1.0)], 24), "Chrome", loc=(-38, lf - 0.3, 18), rot=FRONT,
           shade="auto")
    it.add(cube(0.3, 0.1, 1.4), "Plastic_Black", loc=(-38, lf - 1.32, 18), rot=(0, 30, 0))
    # Keypad 3 x 4.
    for row in range(4):
        for col in range(3):
            it.add(cube(3.0, 0.5, 2.4), "Plastic_Grey", loc=(-10.5 + 3.8 * col, lf - 0.25, 27.0 - 3.2 * row), bevel=0.3, seg=2)
    for sx in (-1, 1):  # lid screws
        for z in (uz0 + 2.2, uz1 - 2.2):
            screw(it, (ux + sx * (uw / 2 - 2.2), lf, z), 0.45)
    for k in range(6):  # side vents
        it.add(cube(0.15, 8.0, 0.6), "Plastic_Black", loc=(ux - uw / 2 - 0.05, 0, 22 + 2.2 * k))
    it.add(lathe([(0, 0), (0.85, 0), (0.85, 1.0), (0.6, 1.4), (0.45, 7.0), (0, 7.2)], 12), "Rubber_Black",
           loc=(ux + 20, 1.5, uz1), shade="auto", angle=50)  # dialer antenna

    # --- Battery box and power supply ------------------------------------------------------------------------
    bxc, b_front = 21.0, -5.0
    it.add(cube(26, board_front - b_front, 22), "Paint_Grey", loc=(bxc, (board_front + b_front) / 2, 19), bevel=0.8, seg=2)
    it.add(cube(13, 0.08, 5.4), "Paper", loc=(bxc, b_front - 0.04, 23.5), fit="y")
    front_text(it, "AKKU", "Plastic_Black", (bxc, b_front - 0.08, 23.5), 2.8, depth=0.06)
    for k in range(4):
        it.add(cube(16, 0.15, 0.6), "Plastic_Black", loc=(bxc, b_front - 0.05, 11.5 + 1.6 * k))
    for sx in (-1, 1):
        screw(it, (bxc + sx * 11, b_front, 27.5), 0.4)
    p_front = -3.0
    it.add(cube(26, board_front - p_front, 16), "Plastic_Grey", loc=(bxc, (board_front + p_front) / 2, 43), bevel=0.6, seg=2)
    led(it, (bxc + 9, p_front, 47.5), "LED_Green", 0.4)
    it.add(cylinder(0.9, 1.4, 16), "Plastic_Black", loc=(bxc - 8, p_front, 46), rot=FRONT, shade="auto")  # fuse holder
    warning_sign(it, (bxc + 1, p_front, 40.5), 5.0)

    # --- Siren with red beacon on top --------------------------------------------------------------------------
    sxc, s_front = 48.0, -5.6
    syc = (board_front + s_front) / 2
    it.add(cube(20, board_front - s_front, 24), "Plastic_Red", loc=(sxc, syc, 36), bevel=1.2, seg=3)
    for k in range(5):
        it.add(cube(12, 0.3, 0.8), "Plastic_Black", loc=(sxc, s_front - 0.05, 28 + 2.1 * k), bevel=0.1, seg=1)
    it.add(cylinder(7.4, 1.8, 36), "Plastic_Black", loc=(sxc, syc, 48), shade="auto")
    it.add(lathe([(0, 0), (6.9, 0), (6.9, 3.6), (6.4, 6.2), (5.0, 8.1), (2.6, 9.1), (0, 9.3)], 36), "LED_Red",
           loc=(sxc, syc, 49.8), shade="auto", angle=40)
    for k in range(10):  # lens ribs
        a = 2 * math.pi * k / 10
        it.add(cube(0.45, 0.45, 3.2), "LED_Red", loc=(sxc + 6.95 * math.cos(a), syc + 6.95 * math.sin(a), 51.7),
               rot=(0, 0, math.degrees(a)))

    # --- Cables ---------------------------------------------------------------------------------------------
    cy_ = 4.0
    for z in (22.0, 40.0):
        pipe(it, [(-50.0, cy_, z), (ux - uw / 2 + 0.5, cy_, z)], 0.4, "Plastic_Grey")
    for x in (-34.0, -24.0, -12.0):
        pipe(it, [(x, cy_, 6.6), (x, cy_, uz0 + 0.5)], 0.4, "Plastic_Grey")
    for x, mat in ((15.0, "Plastic_Red"), (17.0, "Plastic_Black")):
        pipe(it, [(x, 2.0, 7.5), (x, 2.0, 6.0)], 0.3, mat)
    pipe(it, [(ux + uw / 2 - 0.5, 2.0, 20.0), (bxc - 13 + 0.5, 2.0, 20.0)], 0.35, "Plastic_Red")
    pipe(it, [(ux + uw / 2 - 0.5, 2.0, 17.0), (bxc - 13 + 0.5, 2.0, 17.0)], 0.35, "Plastic_Black")
    pipe(it, [(ux + uw / 2 - 0.5, 3.0, 43.0), (bxc - 13 + 0.5, 3.0, 43.0)], 0.4, "Plastic_Grey")
    pipe(it, [(sxc, 5.5, 24.5), (sxc, 5.5, 6.6)], 0.4, "Plastic_Grey")


# ---------------------------------------------------------------------------------------------
# Alarm keypad next to the front door (14 x 4 x 18, back on the wall at y = +2)
# ---------------------------------------------------------------------------------------------


@item("Deco_alarm_keypad")
def alarm_keypad(it: Item):
    W, H = 14.0, 18.0
    it.add(cube(W, 0.8, H), "Plastic_Grey", loc=(0, 1.6, H / 2), bevel=0.3, seg=2)  # wall plate
    f = -1.8  # front of the housing
    it.add(cube(W - 0.4, 1.2 - f, H - 0.4), "Plastic_White", loc=(0, (1.2 + f) / 2, H / 2), bevel=0.9, seg=4)
    # Display with LEDs.
    it.add(cube(10.4, 0.25, 3.8), "Screen_Black", loc=(0, f - 0.1, 13.6), bevel=0.12, seg=2)
    it.add(cube(9.4, 0.1, 2.8), "Display_Blue", loc=(0, f - 0.25, 13.6))
    front_text(it, "SCHARF", "Screen_Black", (-0.6, f - 0.3, 13.6), 1.25, depth=0.03)
    led(it, (-4.0, f, 16.5), "LED_Green", 0.32)
    led(it, (-2.6, f, 16.5), "LED_Red", 0.32)
    # Keys 3 x 4 with digits.
    labels = ["1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#"]
    for i, lab in enumerate(labels):
        row, col = divmod(i, 3)
        x, z = -3.4 + 3.4 * col, 10.0 - 2.2 * row
        it.add(cube(2.7, 0.45, 1.7), "Plastic_Grey", loc=(x, f - 0.2, z), bevel=0.25, seg=2)
        front_text(it, lab, "Plastic_White", (x, f - 0.42, z - 0.05), 1.15, depth=0.04)
    # Speaker slots at the bottom.
    for k in range(4):
        it.add(cube(0.35, 0.1, 0.9), "Plastic_Black", loc=(-1.6 + 1.05 * k, f - 0.03, 1.25), bevel=0.1, seg=1)


# ---------------------------------------------------------------------------------------------
# Metal shelf module (100 x 40 x 180): galvanised posts and five shelves with boxes, paint and crates
# ---------------------------------------------------------------------------------------------

SHELF_Z = (9.0, 48.0, 87.0, 126.0, 166.0)  # top surface of each shelf
SHELF_T = 3.0


def carton(it, w, d, h, loc, rz=0.0, label=True, seed=0):
    """Closed cardboard box standing on loc (bottom centre); rz = rotation about Z."""
    x, y, z = loc
    c = "Cardboard"
    it.add(cube(w, d, h - 0.4), c, loc=(x, y, z + (h - 0.4) / 2), rot=(0, 0, rz), bevel=0.3, seg=2, post=wobble(0.15, 25, seed))
    ca, sa = math.cos(math.radians(rz)), math.sin(math.radians(rz))
    for sy in (-1, 1):  # top flaps, slightly sprung
        oy = sy * d / 4
        it.add(cube(w - 0.3, d / 2 - 0.15, 0.4), c, loc=(x - sa * oy, y + ca * oy, z + h - 0.2), rot=(sy * -1.5, 0, rz),
               bevel=0.1, seg=1)
    if label:
        lx, ly = -w * 0.18, -d / 2 - 0.05
        it.add(cube(min(w * 0.4, 10), 0.08, min(h * 0.35, 7)), "Paper", loc=(x + ca * lx - sa * ly, y + sa * lx + ca * ly, z + h * 0.5),
               rot=(0, 0, rz), fit="y")


def paint_bucket(it, loc, r, h, lid="Plastic_White", band="Paint_Green"):
    """Plastic paint bucket with lid, label band and wire handle."""
    x, y, z = loc
    it.add(lathe([(0, 0), (r - 1.2, 0), (r - 1.0, 0.4), (r - 0.2, h - 3.0), (r + 0.3, h - 2.6), (r + 0.3, h - 1.6),
                  (r, h - 1.4), (0, h - 1.4)], 40), "Plastic_White", loc=(x, y, z), shade="auto", angle=40, grain="z")
    it.add(lathe([(0, 0), (r + 0.5, 0), (r + 0.5, 1.2), (r - 0.4, 1.6), (r - 1.5, 1.6), (r - 2.0, 1.2), (0, 1.2)], 40), lid,
           loc=(x, y, z + h - 1.6), shade="auto", angle=40)

    def wall_r(zz):
        return r - 1.0 + 0.8 * (zz - 0.4) / (h - 3.4)

    z0b, bh = h * 0.3, min(9.0, h * 0.4)
    ra, rb = wall_r(z0b), wall_r(z0b + bh)
    it.add(lathe([(ra - 0.03, 0), (ra + 0.1, 0), (rb + 0.1, bh), (rb - 0.03, bh), (ra - 0.03, 0)], 40, caps=False), band,
           loc=(x, y, z + z0b), shade="auto")
    zl = z + h - 4.0
    for sx in (-1, 1):  # handle lugs
        it.add(cube(1.2, 2.0, 2.4), "Plastic_White", loc=(x + sx * (r + 0.1), y, zl), bevel=0.3, seg=2)
    R = r + 0.7
    phi = math.radians(100)  # handle folded down over the front
    arc = [(x + R * math.cos(math.radians(a)), y - R * math.sin(math.radians(a)) * math.sin(phi),
            zl + R * math.sin(math.radians(a)) * math.cos(phi)) for a in range(0, 181, 12)]
    it.add(sweep(arc, circle_profile(0.18, 6)), "Steel", shade="smooth")
    it.add(cylinder(0.75, 8.0, 12), "Plastic_Black", loc=(x - 4.0, y - R * math.sin(phi), zl + R * math.cos(phi)), rot=(0, 90, 0),
           shade="auto")


def paint_tin(it, loc, r, h, label="Paint_Red"):
    """Metal paint tin with rims and a coloured label."""
    x, y, z = loc
    it.add(lathe([(0, 0), (r - 0.3, 0), (r, 0.3), (r, 0.7), (r - 0.1, 0.9), (r - 0.1, h - 0.9), (r, h - 0.7), (r, h - 0.2),
                  (r - 0.6, h - 0.1), (r - 0.7, h - 0.4), (0, h - 0.4)], 32), "Steel", loc=(x, y, z), shade="auto", angle=40)
    it.add(lathe([(r - 0.05, 0), (r + 0.02, 0), (r + 0.02, h - 2.6), (r - 0.05, h - 2.6), (r - 0.05, 0)], 32, caps=False), label,
           loc=(x, y, z + 1.3), shade="auto")


def spray_can(it, loc, cap="Plastic_Red"):
    x, y, z = loc
    it.add(lathe([(0, 0), (3.0, 0), (3.2, 0.4), (3.2, 15.5), (2.6, 17.0), (1.4, 17.8), (0, 17.9)], 24), "Aluminium",
           loc=(x, y, z), shade="auto", angle=40, grain="z")
    it.add(lathe([(0, 0), (3.25, 0), (3.25, 3.6), (2.8, 4.4), (0, 4.4)], 24), cap, loc=(x, y, z + 15.2), shade="auto", angle=40)


def crate(it, loc, w, d, h, mat, lid=False):
    """Plastic storage crate (walls, rim, grip holes); open unless lid."""
    x, y, z = loc
    t = 0.8
    it.add(cube(w - 2, d - 2, 1.0), mat, loc=(x, y, z + 0.5), bevel=0.4, seg=2)
    for sy in (-1, 1):
        it.add(cube(w - 1.2, t, h - 1.5), mat, loc=(x, y + sy * (d / 2 - 0.9), z + (h - 1.5) / 2 + 0.3), bevel=0.25, seg=2)
    for sx in (-1, 1):
        it.add(cube(t, d - 1.2, h - 1.5), mat, loc=(x + sx * (w / 2 - 0.9), y, z + (h - 1.5) / 2 + 0.3), bevel=0.25, seg=2)
        it.add(cube(0.3, 9.0, 2.6), "Plastic_Black", loc=(x + sx * (w / 2 - 0.4), y, z + h - 4.2), bevel=0.1, seg=1)  # grip hole
    it.add(frame_panel(w, d, 1.2, w - 3.0, d - 3.0), mat, loc=(x, y, z + h - 0.6), rot=(90, 0, 0), bevel=0.3, seg=2)
    for k in range(5):  # ribs on the front
        it.add(cube(0.6, 0.5, h - 4.5), mat, loc=(x - w * 0.36 + k * w * 0.18, y - d / 2 - 0.1, z + (h - 4.5) / 2 + 1.0))
    if lid:
        it.add(cube(w + 0.4, d + 0.4, 1.4), mat, loc=(x, y, z + h + 0.7), bevel=0.4, seg=2)
        it.add(cube(w - 6, d - 6, 0.5), mat, loc=(x, y, z + h + 1.6), bevel=0.2, seg=1)


@item("Deco_metal_shelf")
def metal_shelf(it: Item):
    steel = "Steel"
    W, D, H = 100.0, 40.0, 180.0
    hw, hd = 49.92, D / 2  # post faces 0.8 mm inside the module edge: the slots stay within x = +-50
    leg, t = 3.5, 0.25
    # Four L-section posts, the corner on the module edge, flanges pointing inwards.
    for sx in (-1, 1):
        for sy in (-1, 1):
            pts = [(0, 0), (leg, 0), (leg, t), (t, t), (t, leg), (0, leg)]
            pts = [(sx * (hw - px), sy * (hd - py)) for px, py in pts]
            if sx * sy < 0:
                pts.reverse()
            it.add(ngon_prism(pts, H), steel, shade="flat", grain="z")
            # Keyhole slots on both flanges.
            for k in range(34):
                z = 6.0 + 5.0 * k
                it.add(cube(0.7, 0.06, 1.6), "Plastic_Black", loc=(sx * (hw - leg / 2 - 0.2), sy * (hd + 0.02), z))
                it.add(cube(0.06, 0.7, 1.6), "Plastic_Black", loc=(sx * (hw + 0.02), sy * (hd - leg / 2 - 0.2), z))
            it.add(cube(leg + 0.2, leg + 0.2, 1.6), "Plastic_Black", loc=(sx * (hw + 0.07 - (leg + 0.2) / 2), sy * (hd - (leg + 0.2) / 2), 0.8),
                   bevel=0.25, seg=2)  # foot cap
    # Shelves: folded sheet trays, bolted to the posts (bolt heads on the front and back flanges only).
    for zt in SHELF_Z:
        it.add(cube(W - 0.8, D - 0.8, SHELF_T), steel, loc=(0, 0, zt - SHELF_T / 2), bevel=0.15, seg=1)
        for sx in (-1, 1):
            for sy in (-1, 1):
                it.add(cylinder(0.55, 0.45, 6), steel, loc=(sx * (hw - 2.0), sy * (hd + 0.05), zt - 1.5),
                       rot=FRONT if sy < 0 else BACK, shade="flat")
    # X-bracing on the back (flat straps) in the two lower bays and the upper bays.
    for z0, z1 in ((SHELF_Z[0] - 1.5, SHELF_Z[2] - 1.5), (SHELF_Z[2] - 1.5, SHELF_Z[4] - 1.5)):
        for a, b in (((-hw + 4.0, z0), (hw - 4.0, z1)), ((-hw + 4.0, z1), (hw - 4.0, z0))):
            it.add(sweep([(a[0], hd - 0.15, a[1]), (b[0], hd - 0.15, b[1])], rect_profile(2.4, 0.12), up=(0, 1, 0)), steel,
                   shade="flat")
    it.add(cylinder(0.8, 0.3, 6), steel, loc=(0, hd - 0.4, (SHELF_Z[0] + SHELF_Z[2]) / 2 - 1.5), rot=BACK, shade="flat")
    it.add(cylinder(0.8, 0.3, 6), steel, loc=(0, hd - 0.4, (SHELF_Z[2] + SHELF_Z[4]) / 2 - 1.5), rot=BACK, shade="flat")

    # --- Contents ----------------------------------------------------------------------------------------
    z = SHELF_Z[0]  # bottom: two paint buckets and a big box
    paint_bucket(it, (-35, -2, z), 13.0, 30.0, lid="Plastic_White", band="Paint_Green")
    paint_bucket(it, (-7, 3, z), 12.0, 26.0, lid="Plastic_Yellow", band="Paint_Yellow")
    carton(it, 32, 34, 30, (29, 0, z), rz=2.0, seed=1)
    z = SHELF_Z[1]  # crates
    crate(it, (-23, 0, z), 40, 30, 22, "Plastic_Grey")
    it.add(torus(7.5, 1.0, 28, 8), "Plastic_Black", loc=(-27, -1, z + 3.2), shade="smooth")  # coiled cable
    it.add(torus(5.5, 1.0, 24, 8), "Plastic_Black", loc=(-27, -1, z + 5.0), shade="smooth")
    it.add(cube(10, 6, 9), "Plastic_Red", loc=(-12, 4, z + 5.5), bevel=0.8, seg=2)  # plug block in the crate
    crate(it, (23, 1, z), 40, 30, 22, "Plastic_Yellow", lid=True)
    z = SHELF_Z[2]  # cardboard boxes
    carton(it, 30, 30, 26, (-30, 1, z), rz=-3.0, seed=2)
    carton(it, 24, 28, 18, (0, 2, z), rz=4.0, seed=3)
    carton(it, 20, 24, 12, (0, 1, z + 18), rz=-6.0, label=False, seed=4)
    carton(it, 18, 32, 32, (31, -1, z), rz=0.0, seed=5)
    z = SHELF_Z[3]  # tins, spray cans, toolbox
    paint_tin(it, (-38, -6, z), 8.5, 19.0, "Paint_Red")
    paint_tin(it, (-38, 9, z), 8.5, 19.0, "Paint_Green")
    paint_tin(it, (-22, -4, z), 6.5, 13.0, "Paint_Yellow")
    for k, cap in enumerate(("Plastic_Red", "Plastic_Black", "Plastic_Yellow")):
        spray_can(it, (-11 + 6.8 * k, 8 - 3 * k, z), cap)
    tbx, tby = 27.0, -2.0
    it.add(cube(38, 19, 15), "Paint_Red", loc=(tbx, tby, z + 7.5), bevel=0.6, seg=2)
    it.add(cube(38.4, 19.4, 0.5), "Iron_Dark", loc=(tbx, tby, z + 11.0), bevel=0.15, seg=1)  # lid seam
    it.add(cube(38.6, 19.6, 3.4), "Paint_Red", loc=(tbx, tby, z + 16.2), bevel=0.8, seg=3)
    for sx in (-1, 1):
        it.add(cube(1.2, 1.4, 3.0), "Iron_Dark", loc=(tbx + sx * 7, tby, z + 18.8), bevel=0.2, seg=1)
        it.add(cube(2.0, 0.6, 2.4), "Chrome", loc=(tbx + sx * 13, tby - 9.8, z + 11.6), bevel=0.15, seg=1)  # latches
    it.add(lathe([(0, 0), (0.9, 0), (0.9, 15.0), (0, 15.0)], 12), "Plastic_Black", loc=(tbx - 7.5, tby, z + 20.6), rot=(0, 90, 0),
           shade="auto")
    z = SHELF_Z[4]  # top shelf: low things only (posts end at 180)
    carton(it, 40, 30, 10, (-24, 1, z), rz=-2.0, seed=6)
    paint_tin(it, (8, -5, z), 8.0, 11.5, "Paint_Grey")
    paint_tin(it, (8, 10, z), 8.0, 11.5, "Paint_Green")
    carton(it, 20, 22, 12, (34, 0, z), rz=5.0, label=True, seed=7)
