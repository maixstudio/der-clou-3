"""Villa outdoor lights (G15): post lantern, bollard, wall lantern, floodlight with motion sensor, in-ground uplight.

Exported as SM_<id>. Conventions: cm, front = -Y (Unreal +Y), width along X.

- Lamp_lantern, Lamp_bollard: origin bottom centre on the ground.
- Lamp_wall, Lamp_spot_sensor: wall parts – the back of the wall plate lies on the wall at y = 0, everything sticks
  out towards -Y, origin = centre of the wall plate (z = 0 in the middle of the plate).
- Lamp_ground: flush in the ground – the steel ring tops out at z = +0.8, the pot sits below z = 0.

The game puts the real light source at LIGHT_POINTS (cm, model space, before the actor's rotation).
Lamp_spot_sensor is modelled switched off (no glowing material); its beam points along SPOT_SENSOR_AIM.
"""

import math

from mathutils import Euler, Matrix, Vector

from dc_items import item
from dc_items_small import squash
from dc_mesh import Item, circle_profile, cube, cylinder, lathe, rect_profile, sphere, sweep

LIGHT_POINTS = {
    "Lamp_lantern": (0.0, 0.0, 196.9),      # middle of the glass head
    "Lamp_bollard": (0.0, 0.0, 68.0),       # middle of the glowing ring
    "Lamp_wall": (0.0, -12.5, 0.0),         # middle of the glass lantern
    "Lamp_spot_sensor": (0.0, -14.2, 3.1),  # on the front glass of the floodlight
    "Lamp_ground": (0.0, 0.0, 0.5),         # lens, shines straight up
}
SPOT_SENSOR_TILT = 35.0                     # floodlight pitched down by this many degrees
SPOT_SENSOR_AIM = (0.0, -math.cos(math.radians(SPOT_SENSOR_TILT)), -math.sin(math.radians(SPOT_SENSOR_TILT)))

ANTH = "Paint_Anthracite"
SQ2 = math.sqrt(2.0)
FRONT = (90, 0, 0)    # local +Z -> world -Y
ALONG_X = (0, 90, 0)  # local +Z -> world +X


# ---------------------------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------------------------


def square_lathe(profile):
    """Square 'turned' part: profile [(half_side, z), ...]. Add it with rot=(0, 0, 45) so the sides face X and Y."""
    return lathe([(h * SQ2, z) for h, z in profile], 4)


def frame_matrix(loc, rot):
    """Placement (cm, degrees XYZ) as a matrix, to build parts in a tilted local frame."""
    return Matrix.Translation(Vector(loc)) @ Euler([math.radians(a) for a in rot]).to_matrix().to_4x4()


def placed(frame, loc=(0, 0, 0), rot=(0, 0, 0)):
    """loc/rot keyword arguments for Item.add of a part placed at loc/rot inside `frame`."""
    m = frame @ frame_matrix(loc, rot)
    return dict(loc=tuple(m.to_translation()), rot=tuple(math.degrees(a) for a in m.to_euler("XYZ")))


def bezier(p0, p1, p2, p3, steps=10):
    """Cubic Bezier points (cm)."""
    out = []
    for i in range(steps + 1):
        t = i / steps
        a, b, c, d = (1 - t) ** 3, 3 * (1 - t) ** 2 * t, 3 * (1 - t) * t * t, t ** 3
        out.append(tuple(a * p0[k] + b * p1[k] + c * p2[k] + d * p3[k] for k in range(3)))
    return out


def finial(it, z, height, mat=ANTH, x=0.0, y=0.0, scale=1.0):
    """Turned ball-and-spike finial standing on z."""
    s = height / 4.0 * scale
    prof = [(0, 0), (1.1 * s, 0), (1.1 * s, 0.35 * s), (0.6 * s, 0.7 * s), (1.0 * s, 1.0 * s), (1.2 * s, 1.45 * s),
            (1.0 * s, 1.9 * s), (0.45 * s, 2.2 * s), (0.3 * s, 2.7 * s), (0.0, height)]
    it.add(lathe(prof, 16), mat, loc=(x, y, z), shade="smooth")


def lantern_head(it, x, y, z, w_bot, w_top, h_glass, roof_w, frame_t, scale=1.0):
    """Classic tapered lantern with four glowing panes, standing on z (bottom frame) – returns the roof top z.

    w_bot/w_top: glass width at the bottom/top, roof_w: eave width, frame_t: frame rail height.
    """
    s = scale
    bar = 1.2 * s
    # Bottom rail.
    it.add(cube(w_bot + 2.2 * s, w_bot + 2.2 * s, frame_t), ANTH, loc=(x, y, z + frame_t / 2), bevel=0.3 * s, seg=2)
    zg = z + frame_t
    # Glass body (four panes, tapering outwards to the top).
    it.add(square_lathe([(0, 0), (w_bot / 2, 0), (w_top / 2, h_glass), (0, h_glass)]), "Lamp_Glass",
           loc=(x, y, zg), rot=(0, 0, 45), shade="flat")
    # Corner posts along the glass edges.
    for sx in (-1, 1):
        for sy in (-1, 1):
            p0 = (x + sx * (w_bot / 2 + 0.15 * s), y + sy * (w_bot / 2 + 0.15 * s), zg - 0.2 * s)
            p1 = (x + sx * (w_top / 2 + 0.15 * s), y + sy * (w_top / 2 + 0.15 * s), zg + h_glass + 0.2 * s)
            it.add(sweep([p0, p1], rect_profile(bar, bar, 0.2 * s)), ANTH, shade="auto", angle=50)
    # Thin glazing bar across the middle of each pane (classic look, keeps the panes readable).
    zm = zg + h_glass * 0.62
    wm = w_bot + (w_top - w_bot) * 0.62
    for k in range(4):
        a = math.radians(90 * k)
        cx, cy = math.cos(a) * (wm / 2 + 0.1 * s), math.sin(a) * (wm / 2 + 0.1 * s)
        it.add(cube(0.5 * s, wm, 0.6 * s), ANTH, loc=(x + cx, y + cy, zm), rot=(0, 0, 90 * k), bevel=0.1 * s, seg=1)
    zt = zg + h_glass
    # Top rail.
    it.add(cube(w_top + 2.4 * s, w_top + 2.4 * s, frame_t), ANTH, loc=(x, y, zt + frame_t / 2), bevel=0.3 * s, seg=2)
    zr = zt + frame_t
    # Roof: overhanging pyramid with a small vent cap.
    hr = roof_w / 2
    # Slightly bell-shaped: flatter at the eave, steeper to the top (the bend stays smooth-shaded).
    roof = [(0, 0), (hr, 0), (hr, 0.8 * s), (hr - 0.6 * s, 1.3 * s), (hr * 0.58, hr * 0.18 + 1.3 * s),
            (hr * 0.27, hr * 0.47 + 1.3 * s), (hr * 0.27, hr * 0.47 + 1.9 * s), (0, hr * 0.47 + 1.9 * s)]
    it.add(square_lathe(roof), ANTH, loc=(x, y, zr), rot=(0, 0, 45), shade="auto", angle=30)
    zv = zr + hr * 0.47 + 1.9 * s
    vent = [(0, 0), (hr * 0.33, 0), (hr * 0.33, 0.5 * s), (hr * 0.12, 1.8 * s), (0, 1.8 * s)]
    it.add(square_lathe(vent), ANTH, loc=(x, y, zv - 0.2 * s), rot=(0, 0, 45), shade="auto", angle=30)
    return zv + 1.6 * s


# ---------------------------------------------------------------------------------------------
# Lamp_lantern – classic post lantern, 35 x 35 x 230
# ---------------------------------------------------------------------------------------------


@item("Lamp_lantern")
def lantern(it: Item):
    """Cast-iron style post lantern: fluted plinth, slim mast with ladder bar, tapered glass head, pyramid roof."""
    # Plinth with flutes (rows 9..12 get the ridges).
    base = [(0, 0), (13.0, 0), (13.0, 1.6), (12.4, 2.2), (12.4, 3.6), (11.2, 4.6), (9.8, 6.8), (8.8, 9.0),
            (8.6, 10.2), (8.6, 11.0), (8.3, 18.0), (8.0, 25.0), (7.7, 31.0), (7.7, 31.8), (8.6, 32.6), (8.6, 34.2),
            (7.2, 35.4), (6.0, 37.0), (5.0, 39.0), (4.4, 41.0), (0, 41.0)]
    it.add(lathe(base, 64, ridges=(16, 0.8, 9, 12)), ANTH, shade="auto", angle=40)
    # Mast, slightly tapered.
    it.add(lathe([(0, 0), (3.8, 0), (3.2, 130.0), (0, 130.0)], 24), ANTH, loc=(0, 0, 40.0), shade="auto", angle=40)
    # Ladder bar with hub and ball ends.
    it.add(lathe([(0, 0), (4.0, 0), (4.5, 0.8), (4.5, 4.4), (4.0, 5.2), (0, 5.2)], 24), ANTH, loc=(0, 0, 145.4),
           shade="auto", angle=40)
    it.add(cylinder(0.9, 29.0, 12), ANTH, loc=(-14.5, 0, 148.0), rot=ALONG_X, shade="auto", angle=40)
    for sx in (-1, 1):
        it.add(sphere(1.5, 14, 7), ANTH, loc=(sx * 14.8, 0, 146.5), shade="smooth")
    # Collar and bowl under the head.
    bowl = [(0, 0), (3.3, 0), (4.6, 0.8), (4.6, 2.6), (3.8, 3.4), (3.5, 5.0), (4.4, 6.2), (6.0, 7.6), (7.8, 9.0),
            (9.4, 10.4), (10.6, 11.4), (10.6, 12.0), (0, 12.0)]
    it.add(lathe(bowl, 32), ANTH, loc=(0, 0, 168.0), shade="auto", angle=40)
    # Head: glass 20 -> 27 wide, 29 high; roof 34 x 34.
    top = lantern_head(it, 0, 0, 180.0, 20.0, 27.0, 29.0, 34.0, 2.4)
    finial(it, top - 0.3, 230.0 - top + 0.3)


# ---------------------------------------------------------------------------------------------
# Lamp_bollard – modern bollard, D 16 x 80
# ---------------------------------------------------------------------------------------------


@item("Lamp_bollard")
def bollard(it: Item):
    """Round anthracite bollard with an all-round glowing slot under a flat cap."""
    col = [(0, 0), (7.4, 0), (7.4, 0.8), (8.0, 1.0), (8.0, 64.6), (7.7, 65.0), (0, 65.0)]
    it.add(lathe(col, 48), ANTH, shade="auto", angle=40)
    # Glowing ring, set back 0.6 cm behind column and cap.
    it.add(cylinder(7.4, 6.4, 48), "Lamp_Glass", loc=(0, 0, 64.8), shade="auto", angle=40)
    # Flat cap with a soft edge and a barely domed top.
    cap = [(0, 0), (7.7, 0), (8.0, 0.4), (8.0, 8.2), (7.8, 8.7), (7.2, 8.9), (4.0, 8.97), (0, 9.0)]
    it.add(lathe(cap, 48), ANTH, loc=(0, 0, 71.0), shade="auto", angle=40)
    # Thin satin-steel trim ring framing the light slot at the top of the column.
    it.add(lathe([(7.7, 0), (8.03, 0), (8.03, 0.5), (7.7, 0.5), (7.7, 0)], 48, caps=False), "Steel_Satin",
           loc=(0, 0, 64.4), shade="auto", angle=40)
    # Service hatch at the front with two screws.
    it.add(cube(4.8, 0.4, 11.0), ANTH, loc=(0, -7.95, 14.0), bevel=0.15, seg=1)
    for z in (9.6, 18.4):
        it.add(cylinder(0.35, 0.25, 10), "Steel", loc=(0, -8.1, z), rot=FRONT, shade="auto")


# ---------------------------------------------------------------------------------------------
# Lamp_wall – classic wall lantern, 15 x 20 x 30 (wall part)
# ---------------------------------------------------------------------------------------------


@item("Lamp_wall")
def wall_lantern(it: Item):
    """Wall lantern on a swan-neck arm, matching the post lantern. Plate centre = origin, back on y = 0."""
    cy = -12.5
    # Oval wall plate (8.4 x 18.5) with a raised inner oval.
    it.add(cylinder(4.2, 1.2, 32), ANTH, loc=(0, 0, 0), rot=FRONT, bevel=0.35, seg=2, post=squash(1, 2.2))
    it.add(cylinder(3.0, 0.6, 32), ANTH, loc=(0, -1.2, 0), rot=FRONT, bevel=0.2, seg=2, post=squash(1, 2.4))
    # Swan-neck arm from the plate to the socket under the lantern.
    arm = bezier((0, -1.4, -1.5), (0, -6.5, -1.5), (0, -5.5, -10.8), (0, -10.0, -10.8), 14) + [(0, cy + 0.5, -10.8)]
    it.add(sweep(arm, rect_profile(0.9, 1.3, 0.25)), ANTH, shade="auto", angle=50)
    # Scroll between plate and arm.
    scroll = []
    for i in range(28):
        a = math.radians(i * 18)
        r = 2.5 - 1.9 * i / 27
        scroll.append((0, -3.7 + r * math.cos(a), -6.2 - r * math.sin(a)))
    it.add(sweep(scroll, rect_profile(0.6, 0.9, 0.15)), ANTH, shade="auto", angle=50)
    # Socket under the lantern with a drop finial.
    socket = [(0, -7.0), (0.9, -7.0), (1.5, -6.2), (1.1, -5.3), (1.5, -4.7), (2.5, -3.6), (3.6, -2.0), (4.4, -0.8),
              (4.6, -0.3), (4.6, 0), (0, 0)]
    it.add(lathe(socket, 24), ANTH, loc=(0, cy, -8.0), shade="auto", angle=40)
    # Lantern: glass 8.8 -> 11.6 wide, 14 high; roof 15 x 15.
    top = lantern_head(it, 0, cy, -8.0, 8.8, 11.6, 14.0, 15.0, 1.0, scale=0.45)
    finial(it, top - 0.15, 15.0 - top + 0.15, y=cy)


# ---------------------------------------------------------------------------------------------
# Lamp_spot_sensor – LED floodlight with motion sensor, 25 x 20 x 25 (wall part, switched off)
# ---------------------------------------------------------------------------------------------


@item("Lamp_spot_sensor")
def spot_sensor(it: Item):
    """LED floodlight on a yoke, pitched down, with a PIR motion sensor dome below. Modelled OFF (no emissive)."""
    # Wall plate (centre = origin).
    it.add(cube(7.0, 1.6, 12.0), ANTH, loc=(0, -0.8, 0), bevel=0.5, seg=2)
    # Upper neck to the yoke knuckle.
    it.add(sweep([(0, -1.4, 2.6), (0, -4.2, 4.2), (0, -6.0, 5.0)], rect_profile(2.4, 2.0, 0.4)), ANTH,
           shade="auto", angle=50)
    it.add(cylinder(1.5, 4.0, 20), ANTH, loc=(-2.0, -6.2, 5.0), rot=ALONG_X, bevel=0.3, seg=2)
    # Yoke: flat strap behind the housing, arms to the side pivots.
    yoke = [(-11.45, -12.4, 4.4), (-11.45, -6.6, 5.0), (11.45, -6.6, 5.0), (11.45, -12.4, 4.4)]
    it.add(sweep(yoke, rect_profile(0.7, 2.4, 0.15)), ANTH, shade="auto", angle=50)
    for sx in (-1, 1):
        it.add(cylinder(1.4, 0.5, 20), ANTH, loc=(sx * 11.8 - (0.5 if sx < 0 else 0), -12.4, 4.4), rot=ALONG_X,
               bevel=0.12, seg=1)
        it.add(cylinder(0.9, 0.8, 6), "Plastic_Black", loc=(sx * 12.3 - (0.8 if sx < 0 else 0), -12.4, 4.4),
               rot=ALONG_X, bevel=0.15, seg=1)  # clamp knob
    # Housing in its own frame, pitched down: local -Y = light direction.
    F = frame_matrix((0, -12.4, 4.4), (SPOT_SENSOR_TILT, 0, 0))
    it.add(cube(22.0, 3.4, 16.0), ANTH, **placed(F, (0, 0.6, 0)), bevel=0.7, seg=3)  # body
    for zl in (-7.3, 7.3):  # front bezel rails
        it.add(cube(22.0, 1.3, 1.4), ANTH, **placed(F, (0, -1.5, zl)), bevel=0.3, seg=2)
    for xl in (-10.3, 10.3):
        it.add(cube(1.4, 1.3, 13.4), ANTH, **placed(F, (xl, -1.5, 0)), bevel=0.3, seg=2)
    it.add(cube(19.4, 0.2, 13.4), "Aluminium", **placed(F, (0, -1.1, 0)), shade="flat")  # reflector
    it.add(cube(11.0, 0.3, 5.0), "Plastic_White", **placed(F, (0, -1.3, 0)), bevel=0.1, seg=1)  # LED board (off)
    for i in range(4):
        for j in range(2):
            it.add(cube(1.4, 0.2, 1.4), "Plastic_Yellow", **placed(F, (-3.9 + 2.6 * i, -1.5, -1.2 + 2.4 * j)),
                   shade="flat")  # LED chips (unlit)
    it.add(cube(19.6, 0.3, 13.6), "Glass", **placed(F, (0, -1.85, 0)), shade="flat")  # front glass
    for i in range(9):  # cooling fins on the back
        it.add(cube(0.5, 1.6, 14.0), ANTH, **placed(F, (-9.6 + 2.4 * i, 3.1, 0)), bevel=0.15, seg=1)
    it.add(cylinder(0.8, 1.2, 12), "Plastic_Black", **placed(F, (0, 1.0, -8.0), (180, 0, 0)), shade="auto")  # gland
    # Lower arm and ball joint to the sensor.
    it.add(sweep([(0, -1.4, -3.4), (0, -3.8, -5.6), (0, -5.0, -7.2)], rect_profile(2.0, 1.8, 0.35)), ANTH,
           shade="auto", angle=50)
    it.add(sphere(1.3, 14, 7), ANTH, loc=(0, -5.4, -9.0), shade="smooth")
    # Sensor: short drum with a faceted white PIR dome, looking forward-down.
    S = frame_matrix((0, -6.4, -8.6), (30, 0, 0))
    it.add(cylinder(3.0, 3.0, 24), ANTH, **placed(S, (0, 1.2, 0), FRONT), bevel=0.4, seg=2)
    dome = [(0, 0)] + [(2.7 * math.cos(math.radians(a)), 2.7 * math.sin(math.radians(a))) for a in range(0, 90, 18)]
    it.add(lathe(dome + [(0, 2.7)], 12), "Plastic_White", **placed(S, (0, -1.8, 0), FRONT), shade="flat")


# ---------------------------------------------------------------------------------------------
# Lamp_ground – in-ground uplight, D 15 x 3
# ---------------------------------------------------------------------------------------------


@item("Lamp_ground")
def ground_light(it: Item):
    """Stainless ring with a glowing lens, flush with the ground (ring top z = +0.8, pot below z = 0)."""
    it.add(cylinder(6.4, 2.2, 32), "Plastic_Black", loc=(0, 0, -2.2), shade="auto", angle=40)  # pot (in the ground)
    ring = [(5.3, 0.0), (7.5, 0.0), (7.5, 0.25), (7.25, 0.7), (6.9, 0.8), (5.7, 0.8), (5.3, 0.55), (5.3, 0.0)]
    it.add(lathe(ring, 48, caps=False), "Steel_Satin", shade="auto", angle=40)
    # Lens with a few Fresnel steps.
    lens = [(0, 0), (5.35, 0), (5.35, 0.45), (4.0, 0.45), (4.0, 0.5), (2.6, 0.5), (2.6, 0.55), (0, 0.55)]
    it.add(lathe(lens, 40), "Lamp_Glass", shade="auto", angle=40)
    for k in range(3):
        a = math.radians(90 + 120 * k)
        it.add(cylinder(0.32, 0.12, 10), "Steel", loc=(6.35 * math.cos(a), 6.35 * math.sin(a), 0.78), shade="auto")
