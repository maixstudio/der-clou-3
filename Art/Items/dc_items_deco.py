"""Decoration for the room (not catalog items): skirting board and lamps. Exported as SM_Deco_<name>."""

import math

from mathutils import Euler, Matrix

from dc_items import item
from dc_items_small import squash
from dc_mesh import Item, circle_profile, cube, cylinder, lathe, ngon_prism, sphere, sweep


@item("Deco_skirting")
def skirting(it: Item):
    """1 m of painted skirting board from x=0 to x=100, back at y=0, front towards -Y (scaled along X in the level)."""
    prof = [(0, 0), (1.6, 0), (1.6, 7.4), (1.25, 8.0), (1.35, 8.8), (0.9, 9.6), (0.55, 10.0), (0.0, 10.2)]
    it.add(sweep([(0, 0, 0), (100, 0, 0)], prof, up=(0, 0, 1)), "Paint_White", shade="auto", angle=40)


@item("Deco_desk_lamp")
def desk_lamp(it: Item):
    """Banker's lamp: brass base and stem, green glass shade along X."""
    it.add(lathe([(0, 0), (8.0, 0), (8.2, 0.4), (7.6, 1.2), (5.0, 1.8), (2.0, 2.4), (1.2, 3.2), (0, 3.3)], 48), "Brass",
           shade="smooth", post=squash(1, 0.62))
    it.add(lathe([(0, 0), (0.75, 0), (0.75, 26), (1.1, 26.6), (1.1, 27.4), (0.75, 28), (0, 28)], 20), "Brass", loc=(0, 0, 3.0), shade="auto")
    it.add(cube(14, 1.2, 1.2), "Brass", loc=(0, 0, 31.4), bevel=0.4, seg=2)  # shade holder
    # Shade: crescent shell swept along X with end caps.
    outer, inner = 7.0, 6.6
    arc = [(math.cos(math.radians(a)) * outer, math.sin(math.radians(a)) * outer) for a in range(15, 166, 10)]
    arc += [(math.cos(math.radians(a)) * inner, math.sin(math.radians(a)) * inner) for a in range(165, 14, -10)]
    # sweep along X with up=Z: profile x -> -Y, profile y -> +Z
    it.add(sweep([(-12, 0, 0), (12, 0, 0)], arc, up=(0, 0, 1)), "Glass_Green", loc=(0, -1.5, 28.6), rot=(-12, 0, 0), shade="smooth")
    cap = [(math.cos(math.radians(a)) * outer, math.sin(math.radians(a)) * outer) for a in range(15, 166, 10)]
    # Cap polygon (XY, extruded +Z) into the shade's cross-section: x -> -Y, y -> +Z, extrusion -> -X; then the tilt.
    to_section = Matrix(((0, 0, -1), (-1, 0, 0), (0, 1, 0)))
    rot = (Euler((math.radians(-12), 0, 0)).to_matrix() @ to_section).to_euler()
    rot_deg = tuple(math.degrees(a) for a in rot)
    for x in (12.0, -11.6):
        it.add(ngon_prism(cap, 0.4), "Glass_Green", loc=(x, -1.5, 28.6), rot=rot_deg, shade="flat")
    it.add(sphere(2.0, 16, 8), "Bulb", loc=(0, -1.5, 28.8), shade="smooth")
    # Pull chain.
    it.add(cylinder(0.08, 6, 6), "Brass", loc=(4, -6, 25), shade="smooth")
    it.add(sphere(0.35, 10, 5), "Brass", loc=(4, -6, 24.6), shade="smooth")


@item("Deco_floor_lamp")
def floor_lamp(it: Item):
    """Floor lamp with a cream fabric shade (light source at about 150 cm)."""
    it.add(lathe([(0, 0), (14, 0), (14.2, 0.6), (13.4, 1.6), (6, 2.6), (2.0, 3.4), (0, 3.5)], 48), "Iron_Dark", shade="smooth")
    it.add(cylinder(1.1, 146, 20), "Brass", loc=(0, 0, 3.2), shade="smooth")
    for z in (40, 100):
        it.add(lathe([(0, 0), (1.5, 0), (1.6, 0.8), (1.5, 1.6), (0, 1.6)], 20), "Brass", loc=(0, 0, z), shade="smooth")
    it.add(lathe([(0, 0), (1.6, 0), (1.6, 4), (0, 4)], 20), "Iron_Dark", loc=(0, 0, 146), shade="auto")  # socket
    it.add(sphere(3.2, 20, 10), "Bulb", loc=(0, 0, 148.5), shade="smooth")
    shell = [(23.0, 0.0), (15.0, 27.0), (14.6, 27.0), (22.6, 0.0), (23.0, 0.0)]
    it.add(lathe(shell, 64, caps=False), "Lampshade_Cream", loc=(0, 0, 139), shade="auto", angle=60, grain="z")
    for k in range(4):  # harp wires
        a = math.radians(45 + 90 * k)
        it.add(sweep([(0, 0, 152), (math.cos(a) * 14.6, math.sin(a) * 14.6, 165.5)], circle_profile(0.2, 6)), "Brass", shade="smooth")
    it.add(lathe([(0, 0), (1.0, 0), (0.8, 1.5), (0, 2.6)], 16), "Brass", loc=(0, 0, 166), shade="smooth")  # finial
