"""Villa garden and terrace (G16): pavilion, fountain, mailbox stele, sun lounger, pool ladder.

Exported as SM_<id>. Conventions: cm, origin bottom centre, front = -Y, width along X, height Z
(see Docs/Villa/Villa-Modelle.md, group G16).

- Deco_pavilion stands on a 12 cm deck that the level builds; the model starts at z = 0 on the deck.
- Deco_pool_ladder: origin = middle of the pool edge at deck height; the steps hang into the pool towards +Y.
"""

import math
import random

import bmesh
from mathutils import Vector

from dc_items import item
from dc_items_small import spline, squash
from dc_mesh import CM, Item, circle_profile, cube, cylinder, lathe, puff, rect_profile, sphere, sweep, torus


# ---------------------------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------------------------


def clip_poly(poly, a, b, c):
    """Part of a convex 2D polygon with a*x + b*y <= c (Sutherland-Hodgman)."""
    out = []
    n = len(poly)
    for i in range(n):
        p, q = poly[i], poly[(i + 1) % n]
        fp = a * p[0] + b * p[1] - c
        fq = a * q[0] + b * q[1] - c
        if fp <= 0:
            out.append(p)
        if (fp < 0 < fq) or (fq < 0 < fp):
            t = fp / (fp - fq)
            out.append((p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t))
    return out


def poly_area(poly):
    n = len(poly)
    return 0.5 * abs(sum(poly[i][0] * poly[(i + 1) % n][1] - poly[(i + 1) % n][0] * poly[i][1] for i in range(n)))


def add_slab(bm, poly, z_bottom, z_top):
    """Append a prism over an XY polygon (cm) to bm; z_bottom / z_top map (x, y) -> height (cm)."""
    bot = [bm.verts.new((x * CM, y * CM, z_bottom(x, y) * CM)) for x, y in poly]
    top = [bm.verts.new((x * CM, y * CM, z_top(x, y) * CM)) for x, y in poly]
    n = len(poly)
    bm.faces.new(list(reversed(bot)))
    bm.faces.new(top)
    for i in range(n):
        bm.faces.new((bot[i], bot[(i + 1) % n], top[(i + 1) % n], top[i]))


def finished(bm):
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def closed_loop(points):
    """Profile for lathe(..., caps=False): repeat the first point so the shell closes."""
    return list(points) + [points[0]]


# ---------------------------------------------------------------------------------------------
# Pavilion
# ---------------------------------------------------------------------------------------------

PAV_R = 225.0    # half width at the eaves (outer face of the fascia)
PAV_P = 205.0    # post centre line
PAV_TAN = 0.40   # roof pitch, about 22 degrees
PAV_Z0 = 214.0   # top of the roof boards at the outer edge
PAV_BOARD = 2.2  # roof boarding (seen from below)


def pav_zp(v):
    """Top of the roof boards at horizontal distance v from the eaves."""
    return PAV_Z0 + v * PAV_TAN


@item("Deco_pavilion")
def pavilion(it: Item):
    R, P, T = PAV_R, PAV_P, PAV_TAN
    cos_a = 1.0 / math.sqrt(1.0 + T * T)
    frame, metal, bench = "Wood_Light", "Paint_Anthracite", "Wood_Weathered"
    rafter_d = 11.0
    beam_h = 14.0
    beam_top = pav_zp(R - P) - PAV_BOARD - rafter_d / cos_a  # rafters rest on the ring beam
    post_z0 = 6.0
    post_top = beam_top - beam_h

    # Posts on steel anchors (U cup on a short stem, bolted).
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = sx * P, sy * P
            it.add(cube(14, 14, post_top - post_z0), frame, loc=(x, y, (post_z0 + post_top) / 2), bevel=1.0, seg=2, grain="z")
            it.add(cube(18, 18, 1.2), metal, loc=(x, y, 0.6), bevel=0.3, seg=1)
            it.add(cylinder(2.2, 4.0, 16), metal, loc=(x, y, 1.2), shade="smooth")
            it.add(cube(16, 16, 1.0), metal, loc=(x, y, post_z0 - 0.5), bevel=0.2, seg=1)
            for fx in (-1, 1):
                it.add(cube(1.0, 12, 16), metal, loc=(x + fx * 7.5, y, post_z0 + 8), bevel=0.2, seg=1)
                for bz in (post_z0 + 4.5, post_z0 + 11.5):
                    it.add(cylinder(1.1, 0.8, 12), "Steel_Satin", loc=(x + fx * 8.0, y, bz), rot=(0, 90 * fx, 0), shade="smooth")

    # Ring beam (ends run 12 cm past the posts, lap-jointed at the corners).
    zb = beam_top - beam_h / 2
    for s in (-1, 1):
        it.add(cube(2 * P + 24, 12, beam_h), frame, loc=(0, s * P, zb), bevel=0.6, seg=2, grain="x")
        it.add(cube(12, 2 * P + 24, beam_h), frame, loc=(s * P, 0, zb), bevel=0.6, seg=2, grain="y")
    # Knee braces from each post to both beams.
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = sx * P, sy * P
            for dx, dy in ((-sx, 0), (0, -sy)):
                a = (x + dx * 4, y + dy * 4, post_top - 44)
                b = (x + dx * 48, y + dy * 48, post_top + 3)
                it.add(sweep([a, b], rect_profile(7, 9, 0.5)), frame, grain="x" if dx else "y")

    # Rafters under the boarding: common/jack rafters per roof face, four hip rafters, hanging king post.
    zr = lambda v: pav_zp(v) - PAV_BOARD - rafter_d / 2 / cos_a  # noqa: E731
    hip_d = 14.0
    cos_h = 1.0 / math.sqrt(1.0 + (T / math.sqrt(2)) ** 2)
    zh = lambda v: pav_zp(v) - PAV_BOARD - hip_d / 2 / cos_h  # noqa: E731
    for k in range(4):
        rot = (0, 0, 90 * k)
        grain = "y" if k % 2 == 0 else "x"
        for u in (-140, -70, 0, 70, 140):
            v_end = R - abs(u) - (7 if u == 0 else 6)
            it.add(sweep([(u, -R + 2.5, zr(2.5)), (u, -R + v_end, zr(v_end))], rect_profile(6, rafter_d, 0.4)), frame, rot=rot,
                   grain=grain)
        it.add(sweep([(R - v, R - v, zh(v)) for v in (4.0, R - 8.0)], rect_profile(8, hip_d, 0.5)), frame, rot=rot)
    apex = pav_zp(R)
    kp_bottom = apex - PAV_BOARD - 46
    it.add(cube(13, 13, 46), frame, loc=(0, 0, kp_bottom + 23), bevel=0.8, seg=2, grain="z")
    it.add(lathe([(0, 0), (2.5, 0.4), (4.5, 2.5), (5.2, 5.0), (4.2, 7.0), (6.0, 8.0), (6.4, 9.6), (0, 9.6)], 24), frame,
           loc=(0, 0, kp_bottom - 9.4), shade="smooth", grain="z")

    # Fascia boards round the eaves (mitred corners).
    fz = pav_zp(2.5) - 0.3
    sq = [(-R, -R), (R, -R), (R, R), (-R, R)]
    it.add(sweep([(x, y, fz) for x, y in sq], [(-2.5, -16), (0, -16), (0, 0), (-2.5, 0)], closed=True), frame, grain="x")

    # Roof faces: boarding, black roofing felt, wooden shingles in staggered courses (gaps show the felt).
    rng = random.Random(16)
    tri = [(-R, -R), (R, -R), (0, 0)]  # front face; the others are rotated copies
    zp = lambda x, y: pav_zp(y + R)  # noqa: E731
    s_exp, s_ov = 17.0, 5.0   # course exposure and overlap (horizontal cm)
    t_butt, t_tail = 3.4, 0.9
    gap = 0.7                 # half joint between shingles
    for k in range(4):
        rot = (0, 0, 90 * k)
        grain = "y" if k % 2 == 0 else "x"
        boards = bmesh.new()
        add_slab(boards, tri, lambda x, y: zp(x, y) - PAV_BOARD, zp)
        it.add(finished(boards), frame, rot=rot, shade="flat", grain=grain)
        felt = bmesh.new()
        add_slab(felt, tri, zp, lambda x, y: zp(x, y) + 0.15)
        it.add(finished(felt), "Rubber_Black", rot=rot, shade="flat")
        dark, light = bmesh.new(), bmesh.new()
        v = 0.5
        while v < R - 4:
            vlo, vhi = v, min(v + s_exp + s_ov, R)
            hw = R - vlo
            u = -hw - rng.uniform(0, 20)
            while u < hw:
                w = rng.uniform(13, 25)
                u0, u1 = u + gap, u + w - gap
                u += w
                poly = [(u0, vlo), (u1, vlo), (u1, vhi), (u0, vhi)]
                poly = clip_poly(poly, 1, 1, R)
                poly = clip_poly(poly, -1, 1, R) if poly else poly
                if len(poly) < 3 or poly_area(poly) < 8:
                    continue
                xy = [(pu, pv - R) for pu, pv in poly]

                def z_top(x, y, lo=vlo, hi=vhi):
                    return pav_zp(y + R) + t_butt - (t_butt - t_tail) * (y + R - lo) / (hi - lo)

                add_slab(light if rng.random() < 0.07 else dark, xy, lambda x, y: pav_zp(y + R) + 0.15, z_top)
            v += s_exp
        it.add(finished(dark), "Wood_Dark", rot=rot, shade="flat", grain=grain)
        it.add(finished(light), "Wood_Walnut", rot=rot, shade="flat", grain=grain)

    # Hip caps (metal) and finial.
    cap = [(-6.5, -1.8), (6.5, -1.8), (6.5, -0.6), (5.0, 0.9), (2.5, 2.0), (0, 2.4), (-2.5, 2.0), (-5.0, 0.9), (-6.5, -0.6)]
    for k in range(4):
        path = [(R - v, R - v, pav_zp(v) + 2.0) for v in (5.0, R - 5.0)]
        it.add(sweep(path, cap), metal, rot=(0, 0, 90 * k), shade="auto", angle=50)
    fin = [(0, 0), (11, 0), (11, 1.6), (8.5, 3.2), (4, 4.6), (2.6, 6.2), (3.8, 7.6), (4.3, 9.0), (3.6, 10.6), (1.6, 11.8), (1.1, 13.0),
           (0, 14.2)]
    it.add(lathe([(r, z * 0.8) for r, z in fin], 32), metal, loc=(0, 0, apex + 4.6), shade="smooth")  # top at 320
    it.add(lathe([(0, 0), (11.2, 0), (11.2, 5.0), (0, 5.0)], 32), metal, loc=(0, 0, apex), shade="auto")  # collar over the hips

    # Bench along the back (+Y), between the rear posts.
    seat = 45.0
    for yc in (154.6, 165.2, 175.8, 186.4):
        it.add(cube(370, 9.2, 2.8), bench, loc=(0, yc, seat - 1.4), bevel=0.5, seg=2)
    for x in (-170, -57, 57, 170):
        it.add(cube(5, 44, 6), bench, loc=(x, 170.5, seat - 5.8), bevel=0.4, seg=2, grain="y")
        it.add(cube(5.5, 5.5, seat - 8.8), bench, loc=(x, 153.5, (seat - 8.8) / 2), bevel=0.4, seg=2, grain="z")
        it.add(sweep([(x, 188, 0), (x, 188, 40), (x, 197, 92)], rect_profile(5.5, 5.5, 0.4), up=(1, 0, 0)), bench, grain="z")
        it.add(cube(4, 34, 4), bench, loc=(x, 170.8, 12), bevel=0.3, seg=1, grain="y")
    for zs in (58.0, 70.0, 82.0):
        ys = 188 + 9 * (zs - 40) / 52 - 4.0
        it.add(cube(370, 2.4, 8.5), bench, loc=(0, ys, zs), rot=(-10, 0, 0), bevel=0.5, seg=2)


# ---------------------------------------------------------------------------------------------
# Fountain
# ---------------------------------------------------------------------------------------------


@item("Deco_fountain")
def fountain(it: Item):
    stone, water = "Stone_Sand", "Water"
    # Round basin: plinth, slightly drawn-in wall, moulded coping; inner wall down to the floor.
    ring = [(112, 0), (130, 0), (130, 5), (128.6, 6.4), (126, 7.2), (125, 8.6), (125, 37.4), (126.4, 38.6), (129, 40), (130, 41.8),
            (130, 46.2), (129.3, 47.6), (127.8, 48), (115.2, 48), (113.9, 47.5), (113.4, 46.2), (113.4, 40.6), (112.6, 39.6),
            (112, 39)]
    it.add(lathe(closed_loop(ring), 72, caps=False), stone, shade="auto", angle=40, grain="z")
    # Sealed basin interior (dark coating) so the water reads from above.
    it.add(lathe([(0, 7), (111.9, 7), (111.9, 39), (111.4, 39), (111.4, 7.5), (0, 7.5)], 64), "Plastic_Grey", shade="auto")
    # Joints between the coping and wall stones.
    for k in range(8):
        a = math.radians(22.5 + 45 * k)
        ca, sa = math.cos(a), math.sin(a)
        rot = (0, 0, math.degrees(a))
        it.add(cube(14.6, 0.5, 0.2), "Concrete", loc=(121.5 * ca, 121.5 * sa, 48.0), rot=rot)
        it.add(cube(0.3, 0.5, 5.0), "Concrete", loc=(130.0 * ca, 130.0 * sa, 44.0), rot=rot)
        it.add(cube(0.3, 0.5, 28.0), "Concrete", loc=(125.0 * ca, 125.0 * sa, 23.0), rot=rot)

    # Water surface at 40 with a few rings where the falling water hits it.
    surf = [(0, 39.6), (112.2, 39.6), (112.2, 40.0), (104, 40.0)]
    for k in range(17):
        r = 96.0 - 3.25 * k
        d = r - 56.0
        surf.append((r, 40.0 + 0.45 * math.exp(-(d / 18.0) ** 2) * math.cos(d * 0.8)))
    surf += [(36, 40.0), (0, 40.0)]
    it.add(lathe(surf, 56), water, shade="smooth")
    it.add(torus(56.0, 1.0, 48, 6), water, loc=(0, 0, 40.1), shade="smooth", post=squash(2, 0.5))

    # Central column with a moulded base, standing in the water.
    col = [(0, 7.5), (30, 7.5), (30, 12.5), (28.6, 14), (25.5, 14.6), (24.5, 16.2), (22.6, 17.6), (19.8, 18.4), (16, 19.2),
           (14.6, 20.5), (14.6, 22.5), (13.4, 24), (13, 30), (12.2, 56), (11.6, 71), (12.6, 72.4), (14.8, 73.4), (15, 75.2),
           (13.2, 76.4), (10.4, 77.4), (9.2, 79), (8.6, 84), (9.6, 86.5), (0, 86.5)]
    it.add(lathe(col, 40), stone, shade="auto", angle=40, grain="z")
    # Bowl with a twelve-lobed rim; the water runs over the lobe tips.
    bowl = [(9.6, 86), (14, 87.6), (22, 90.6), (31, 95), (38.5, 99.6), (43.5, 103.4), (46.2, 105.8), (47, 107.4), (46.6, 108.6),
            (45.4, 109.2), (44.2, 108.8), (42.6, 107.4), (37, 105), (27, 103), (15, 101.8), (0, 101.4)]
    it.add(lathe(bowl, 72, ridges=(12, 2.5, 4, 11)), stone, shade="smooth", grain="z")
    it.add(lathe([(0, 102), (38.5, 102), (43.0, 106.9), (0, 106.9)], 48), water, shade="smooth")
    # Small urn spout in the bowl with a bubbling jet; water runs down it into the bowl.
    it.add(lathe([(0, 101), (7, 101), (7.4, 102.4), (5.2, 103.6), (4, 106), (5.4, 108.6), (5.0, 110.6), (3.0, 112), (1.6, 112.6),
                  (0, 112.8)], 24), stone, shade="smooth")
    it.add(lathe([(0, 112.2), (1.5, 112.2), (1.2, 114.5), (2.2, 116.6), (2.6, 118.2), (1.9, 119.5), (0, 120)], 16), water, shade="smooth")
    film = [(1.7, 112.9), (3.3, 112.3), (5.3, 110.8), (5.8, 108.6), (4.4, 106), (5.6, 103.8), (7.7, 102.5), (8.6, 106.9)]
    it.add(lathe(closed_loop(film + [(r - 0.4, z) for r, z in reversed(film)]), 24, caps=False), water, shade="smooth")
    # Water falling over the bowl rim into the basin: twelve flat streams that thin out as they fall.
    fall = spline([(r, z, 0) for r, z in ((46.0, 109.6), (48.0, 109.2), (49.8, 106.6), (51.0, 101.0), (52.0, 92.0), (53.0, 80.0),
                                         (54.0, 66.0), (55.0, 52.0), (55.6, 40.0))], 2)  # (radius, height) in the stream's plane
    widths = [1.0 - 0.5 * k / (len(fall) - 1) for k in range(len(fall))]
    for k in range(12):
        a = math.radians(15 + 30 * k)
        ca, sa = math.cos(a), math.sin(a)
        path = [(r * ca, r * sa, z) for r, z, _ in fall]
        it.add(sweep(path, circle_profile(1.0, 8, sx=0.8, sy=3.4), up=(-sa, ca, 0), scales=widths), water, shade="smooth")


# ---------------------------------------------------------------------------------------------
# Mailbox stele
# ---------------------------------------------------------------------------------------------


@item("Deco_mailbox")
def mailbox(it: Item):
    st, dark, num = "Steel_Satin", "Plastic_Black", "Paint_Anthracite"
    fy = -7.0  # front face of the body
    by = 7.0   # back face
    it.add(cube(37, 11.5, 3.0), num, loc=(0, 0, 1.5), bevel=0.3, seg=1)  # recessed plinth: shadow gap at the ground
    it.add(cube(40, 14, 134), st, loc=(0, 0, 70), bevel=0.8, seg=3, grain="z")
    it.add(cube(40.2, 14.2, 0.35), dark, loc=(0, 0, 137.1))  # joint under the cap
    it.add(cube(40, 15, 3.0), st, loc=(0, 0, 138.5), bevel=0.7, seg=3)

    # House number "12": raised digits.
    zd = 118.0
    yd = fy - 0.3

    def stroke(points, w=1.8):
        it.add(sweep(points, rect_profile(w, 0.7, 0.15), up=(0, -1, 0)), num, shade="auto", angle=40)

    stroke([(-4.6, yd, zd + 12.9), (-4.6, yd, zd + 0.3)])                     # 1: stem
    stroke([(-8.0, yd, zd + 10.0), (-4.4, yd, zd + 12.9)], w=1.6)            # 1: flag
    stroke([(-8.0, yd, zd + 0.9), (-1.4, yd, zd + 0.9)])                     # 1: foot
    arc = [(5.0 + 3.7 * math.cos(math.radians(a)), yd, zd + 9.2 + 3.7 * math.sin(math.radians(a))) for a in range(165, -40, -15)]
    stroke(spline(arc + [(4.4, yd, zd + 4.4), (1.9, yd, zd + 1.6)], 3))       # 2: bow and diagonal
    stroke([(1.4, yd, zd + 0.9), (9.0, yd, zd + 0.9)])                        # 2: foot

    # Intercom plate: camera eye, speaker, lit name plate and bell push.
    it.add(cube(30, 0.6, 25), st, loc=(0, fy - 0.2, 104), bevel=0.25, seg=2)
    pf = fy - 0.5
    it.add(cylinder(3.2, 0.25, 32), dark, loc=(0, pf, 111.5), rot=(90, 0, 0), shade="smooth")
    it.add(lathe([(1.5, 0), (2.4, 0), (2.5, 0.25), (2.2, 0.55), (1.5, 0.55)], 32), "Chrome", loc=(0, pf - 0.2, 111.5), rot=(90, 0, 0))
    it.add(cylinder(1.55, 0.3, 24), "Screen_Black", loc=(0, pf - 0.2, 111.5), rot=(90, 0, 0), shade="smooth")
    it.add(sphere(1.2, 20, 10, sz=0.3), "Glass", loc=(0, pf - 0.45, 111.5), rot=(90, 0, 0), shade="smooth")
    for sx in (-1, 1):
        it.add(cylinder(0.35, 0.1, 12), "Plastic_Grey", loc=(sx * 4.4, pf - 0.2, 111.5), rot=(90, 0, 0), shade="smooth")  # IR LEDs
    for k in range(5):
        it.add(cube(12, 0.12, 0.55), dark, loc=(0, pf - 0.04, 103.0 + k * 1.1), bevel=0.05, seg=1)
    it.add(cube(18.6, 0.2, 4.4), dark, loc=(-4.0, pf - 0.08, 97.0), bevel=0.1, seg=1)
    it.add(cube(17.4, 0.3, 3.2), "Lamp_Glass", loc=(-4.0, pf - 0.2, 97.0), bevel=0.08, seg=1)
    it.add(torus(1.85, 0.22, 32, 8), "Display_Blue", loc=(9.5, pf - 0.1, 97.0), rot=(90, 0, 0), shade="smooth")
    it.add(lathe([(0, 0), (1.5, 0), (1.5, 0.35), (1.1, 0.7), (0, 0.8)], 24), "Chrome", loc=(9.5, pf, 97.0), rot=(90, 0, 0), shade="smooth")

    # Letter slot with a hinged flap (bottom edge tilted out) and a rain lip.
    it.add(cube(34, 0.5, 9.0), st, loc=(0, fy - 0.15, 81.0), bevel=0.2, seg=2)
    it.add(cube(31, 0.3, 3.8), dark, loc=(0, fy - 0.45, 81.4))
    it.add(cube(32, 0.5, 4.6), st, loc=(0, fy - 0.85, 81.2), rot=(-7, 0, 0), bevel=0.15, seg=2)
    it.add(cylinder(0.45, 32.5, 12), st, loc=(-16.25, fy - 0.7, 83.6), rot=(0, 90, 0), shade="smooth")  # hinge
    it.add(cube(34, 1.1, 0.7), st, loc=(0, fy - 0.55, 86.0), bevel=0.25, seg=2)

    # Newspaper compartment (open front).
    it.add(cube(31, 0.3, 7.6), dark, loc=(0, fy - 0.05, 54.0))
    it.add(sweep([(-16, fy - 0.35, 49.9), (16, fy - 0.35, 49.9)], circle_profile(0.55, 10)), st, shade="smooth")
    it.add(cube(32.6, 0.7, 0.8), st, loc=(0, fy - 0.3, 58.2), bevel=0.2, seg=2)

    # Back: emptying door outline with a lock cylinder.
    for z in (60.0, 90.0):
        it.add(cube(32.5, 0.2, 0.3), dark, loc=(0, by + 0.05, z))
    for x in (-16.1, 16.1):
        it.add(cube(0.3, 0.2, 30.3), dark, loc=(x, by + 0.05, 75.0))
    it.add(lathe([(0, 0), (1.4, 0), (1.4, 0.35), (1.15, 0.6), (0, 0.6)], 24), "Chrome", loc=(12.5, by, 86.0), rot=(-90, 0, 0), shade="smooth")
    it.add(cube(0.25, 0.1, 1.2), dark, loc=(12.5, by + 0.62, 86.0))


# ---------------------------------------------------------------------------------------------
# Sun lounger
# ---------------------------------------------------------------------------------------------


@item("Deco_sun_lounger")
def sun_lounger(it: Item):
    teak, fab = "Wood_Weathered", "Fabric_Cream"
    half = 97.5
    rx = 29.0  # side rail centre
    # Side rails and end rails.
    for sx in (-1, 1):
        it.add(cube(4, 2 * half, 9), teak, loc=(sx * rx, 0, 25.5), bevel=0.8, seg=2, grain="y")
    for sy in (-1, 1):
        it.add(cube(2 * rx - 4, 4, 8), teak, loc=(0, sy * (half - 2), 25.5), bevel=0.6, seg=2)
    # Legs: on the ground at the foot end, wheels at the head end.
    for sx in (-1, 1):
        it.add(cube(5, 5, 21.5), teak, loc=(sx * rx, -88, 10.75), bevel=0.5, seg=2, grain="z")
        it.add(cube(4.6, 4.6, 0.6), "Rubber_Black", loc=(sx * rx, -88, 0.3), bevel=0.15, seg=1)
        it.add(cube(5, 5, 13), teak, loc=(sx * rx, 86, 15.0), bevel=0.5, seg=2, grain="z")
        x0 = 31.6 if sx > 0 else -34.4  # wheel: 2.8 wide, just outside the leg
        tyre = [(6.2, 0), (8.3, 0), (8.8, 0.4), (9.0, 1.4), (8.8, 2.4), (8.3, 2.8), (6.2, 2.8)]
        it.add(lathe(closed_loop(tyre), 40, caps=False), "Rubber_Black", loc=(x0, 86, 9), rot=(0, 90, 0), shade="smooth")
        it.add(lathe([(0, -0.1), (6.3, -0.1), (6.3, 2.9), (0, 2.9)], 32), teak, loc=(x0, 86, 9), rot=(0, 90, 0), shade="auto")
        cap_x = x0 + 2.9 if sx > 0 else x0 - 0.1
        it.add(lathe([(0, 0), (1.6, 0), (1.6, 0.2), (1.2, 0.4), (0, 0.45)], 20), "Steel_Satin", loc=(cap_x, 86, 9),
               rot=(0, 90 * sx, 0), shade="smooth")
    it.add(cube(2 * rx, 4, 5), teak, loc=(0, -88, 8), bevel=0.4, seg=2)
    it.add(cylinder(1.0, 66, 12), "Steel_Satin", loc=(-33, 86, 9), rot=(0, 90, 0), shade="smooth")  # axle
    it.add(cube(2 * rx, 4, 5), teak, loc=(0, 86, 12), bevel=0.4, seg=2)
    # Slat bearers and slats of the flat part.
    for y in (-60, -20, 18):
        it.add(cube(2 * rx - 4, 4, 4), teak, loc=(0, y, 24.7), bevel=0.3, seg=1)
    for k in range(14):
        it.add(cube(2 * rx - 4, 6.2, 1.8), teak, loc=(0, -91 + 8.2 * k, 27.6), bevel=0.4, seg=2)

    # Backrest panel hinged at the end of the flat part, propped up 40 degrees.
    th = math.radians(40)
    yh, zh = 27.0, 28.5

    def hp(yl, zl):
        return yh + yl * math.cos(th) - zl * math.sin(th), zh + yl * math.sin(th) + zl * math.cos(th)

    rot = (40, 0, 0)
    for sx in (-1, 1):
        y, z = hp(32, -1.5)
        it.add(cube(3.5, 64, 3), teak, loc=(sx * 24.5, y, z), rot=rot, bevel=0.4, seg=2, grain="y")
    y, z = hp(62.5, -1.5)
    it.add(cube(52.5, 3.5, 3), teak, loc=(0, y, z), rot=rot, bevel=0.4, seg=2)
    for k in range(8):
        y, z = hp(4 + 7.6 * k, 0.9)
        it.add(cube(52.5, 5.4, 1.8), teak, loc=(0, y, z), rot=rot, bevel=0.4, seg=2)
    # Prop: two struts from under the panel to a dowel resting in the ratchet bars.
    py, pz = hp(44, -3)
    for sx in (-1, 1):
        it.add(sweep([(sx * 20.5, py, pz), (sx * 20.5, 74, 27.6)], rect_profile(3, 3.5, 0.3)), teak, grain="z")
        it.add(cube(2.5, 26, 3), teak, loc=(sx * 25.75, 76, 27.0), bevel=0.3, seg=1, grain="y")
        for k in range(3):
            it.add(cube(2.6, 1.2, 0.8), "Wood_Dark", loc=(sx * 25.75, 68 + 7 * k, 28.2))  # ratchet notches
    it.add(cylinder(1.2, 48, 12), teak, loc=(-24, 74, 27.6), rot=(0, 90, 0), shade="smooth")

    # Cushions: two on the flat part, one on the backrest, a head pillow.
    it.add(cube(53.5, 54.4, 6, cuts=5), fab, loc=(0, -65.6, 31.5), bevel=2.6, seg=5, shade="smooth", post=puff(0.8))
    it.add(cube(53.5, 63.4, 6, cuts=5), fab, loc=(0, -6.2, 31.5), bevel=2.6, seg=5, shade="smooth", post=puff(0.8))
    y, z = hp(32.2, 4.8)
    it.add(cube(53.5, 62.6, 6, cuts=5), fab, loc=(0, y, z), rot=rot, bevel=2.6, seg=5, shade="smooth", post=puff(0.8))
    y, z = hp(55.5, 11.0)
    it.add(cube(42, 13, 7, cuts=4), fab, loc=(0, y, z), rot=rot, bevel=3.0, seg=5, shade="smooth", post=puff(1.2))


# ---------------------------------------------------------------------------------------------
# Pool ladder
# ---------------------------------------------------------------------------------------------


@item("Deco_pool_ladder")
def pool_ladder(it: Item):
    chrome, steel = "Chrome", "Steel_Satin"
    rx, rt = 22.0, 2.15             # rail centre (x) and tube radius
    y_deck, y_pool = -33.0, 20.0    # anchor on the deck, descending part in the pool
    z_bend = -89.85
    # Swan-neck handrail (y, z): up from the deck anchor, over the edge (y = 0) at about 74, down into the pool.
    neck = [(y_deck, 22.0), (y_deck + 1.0, 40.0), (-28.5, 53.0), (-22.0, 64.5), (-12.0, 70.6), (-2.0, 71.2), (7.0, 67.4),
            (14.5, 57.6), (18.8, 44.0), (20.0, 30.0)]
    for sx in (-1, 1):
        x = sx * rx
        pts = [(x, y_deck, -4.0), (x, y_deck, 10.0)]
        pts += spline([(x, y, z) for y, z in [(y_deck, 10.0)] + neck + [(y_pool, 15.0)]], 4)[1:]
        pts += [(x, y_pool, z_bend)]
        pts += [(x, 6 + 14 * math.cos(math.radians(a)), z_bend + 14 * math.sin(math.radians(a))) for a in range(-10, -91, -10)]
        pts += [(x, 2.5, z_bend - 14)]
        it.add(sweep(pts, circle_profile(rt, 16), up=(1, 0, 0)), chrome, shade="smooth", grain="z")
        # Deck rosette and wall bumper.
        it.add(lathe([(0, 0), (3.8, 0), (3.8, 0.25), (3.3, 0.7), (2.6, 1.0), (0, 1.0)], 32), chrome, loc=(x, y_deck, 0), shade="smooth")
        it.add(lathe([(0, 0), (2.3, 0), (2.7, 0.5), (2.7, 3.0), (0, 3.0)], 20), "Rubber_Black", loc=(x, 0.3, z_bend - 14),
               rot=(-90, 0, 0), shade="smooth")
    # Three treads with anti-slip strips, clamped to the rails.
    for zt in (-25.0, -52.0, -79.0):
        it.add(cube(40.8, 13, 2.6), steel, loc=(0, 16, zt - 1.3), bevel=0.6, seg=2)
        for ys in (12.0, 16.0, 20.0):
            it.add(cube(36, 1.5, 0.4), "Rubber_Black", loc=(0, ys, zt + 0.1), bevel=0.15, seg=1)
        for sx in (-1, 1):
            it.add(cylinder(2.9, 8, 24), steel, loc=(sx * rx, y_pool, zt - 6.5), shade="auto")
            it.add(cube(1.2, 13, 6), steel, loc=(sx * 20.4, 16, zt - 3.4), bevel=0.3, seg=1)
            it.add(cylinder(0.7, 0.4, 12), chrome, loc=(sx * (rx + 2.9), y_pool, zt - 2.5), rot=(0, 90 * sx, 0), shade="smooth")
