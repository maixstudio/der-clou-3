"""Plan marks on the floor (not catalog items): a footprint and a ring. Exported as SM_Plan_<name>.

The game draws them with its own unlit material (M_PlanMark, colour per instance), so the material
slot here is only for the Blender preview. Both are flat (3 mm) and lie on z = 0.
"""

import math

import bmesh

from dc_items import item
from dc_mesh import CM, Item, ngon_prism


def _ellipse(cx, cy, rx, ry, tilt_deg=0.0, n=28):
    """Points (cm, counter-clockwise) of an ellipse turned by tilt_deg."""
    t = math.radians(tilt_deg)
    pts = []
    for i in range(n):
        a = 2 * math.pi * i / n
        x, y = rx * math.cos(a), ry * math.sin(a)
        pts.append((cx + x * math.cos(t) - y * math.sin(t), cy + x * math.sin(t) + y * math.cos(t)))
    return pts


@item("Plan_footprint")
def plan_footprint(it: Item):
    """Right shoe print, 26 cm long, toes towards -Y (Unreal +Y). The big toe side (+X) reaches further forward.

    Two islands like a sole print: ball of the foot and heel. The game mirrors it in X for the left foot.
    """
    ball = _ellipse(0.4, -5.6, 4.7, 7.4, tilt_deg=-9.0)
    heel = _ellipse(-0.4, 8.6, 3.7, 4.4)
    it.add(ngon_prism(ball, 0.3), "Paint_White", shade="flat")
    it.add(ngon_prism(heel, 0.3), "Paint_White", shade="flat")


@item("Plan_ring")
def plan_ring(it: Item):
    """Flat ring, outer radius 50 cm, inner 43 cm (the game scales it to the stand spot)."""
    n, r_out, r_in, h = 64, 50.0, 43.0, 0.3
    bm = bmesh.new()
    rings = []
    for z in (0.0, h):
        outer = [bm.verts.new((r_out * math.cos(2 * math.pi * i / n) * CM, r_out * math.sin(2 * math.pi * i / n) * CM, z * CM)) for i in range(n)]
        inner = [bm.verts.new((r_in * math.cos(2 * math.pi * i / n) * CM, r_in * math.sin(2 * math.pi * i / n) * CM, z * CM)) for i in range(n)]
        rings.append((outer, inner))
    (ob, ib), (ot, it_) = rings
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((ot[i], ot[j], it_[j], it_[i]))  # top
        bm.faces.new((ob[i], ib[i], ib[j], ob[j]))  # bottom
        bm.faces.new((ob[i], ob[j], ot[j], ot[i]))  # outer wall
        bm.faces.new((ib[i], it_[i], it_[j], ib[j]))  # inner wall
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    it.add(bm, "Paint_White", shade="flat")
