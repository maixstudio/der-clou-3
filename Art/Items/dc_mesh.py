"""Modelling helpers for the item props. All sizes are in centimetres (converted to metres here).

Convention: Z up, origin at the bottom centre of the item, the front faces -Y.
An item is built from parts; each part gets bevels, smoothing, UVs and a material, then all
parts are joined into one mesh named SM_<id>.
"""

import math

import bmesh
import bpy
from mathutils import Euler, Matrix, Vector

CM = 0.01


# ---------------------------------------------------------------------------------------------
# Geometry (bmesh, metres)
# ---------------------------------------------------------------------------------------------


def cube(sx, sy, sz, cuts=0):
    """Box centred on the origin."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    if cuts:
        bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=cuts, use_grid_fill=True)
    for v in bm.verts:
        v.co = Vector((v.co.x * sx, v.co.y * sy, v.co.z * sz)) * CM
    return bm


def tapered_box(top_x, top_y, bottom_x, bottom_y, h):
    """Box from z=0 (bottom size) to z=h (top size), e.g. a tapered leg."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        top = v.co.z > 0
        sx, sy = (top_x, top_y) if top else (bottom_x, bottom_y)
        v.co = Vector((v.co.x * sx, v.co.y * sy, h if top else 0.0)) * CM
    return bm


def lathe(profile, segments=32, ridges=None, caps=True):
    """Surface of revolution around Z. profile: [(radius, z), ...] in cm, bottom to top.

    ridges: optional (count, depth_cm, first_index, last_index) – knurling on those profile rows.
    caps: close the first/last ring; False for shells whose profile is a closed loop.
    """
    bm = bmesh.new()
    rings = []
    for idx, (r, z) in enumerate(profile):
        if r <= 1e-6:
            rings.append([bm.verts.new((0.0, 0.0, z * CM))])
            continue
        ring = []
        for s in range(segments):
            a = 2.0 * math.pi * s / segments
            rr = r
            if ridges and ridges[2] <= idx <= ridges[3]:
                rr = r - ridges[1] * (0.5 + 0.5 * math.cos(a * ridges[0]))
            ring.append(bm.verts.new((rr * CM * math.cos(a), rr * CM * math.sin(a), z * CM)))
        rings.append(ring)
    for a, b in zip(rings, rings[1:]):
        if len(a) == 1 and len(b) == 1:
            continue
        if len(a) == 1:
            for s in range(segments):
                bm.faces.new((a[0], b[s], b[(s + 1) % segments]))
        elif len(b) == 1:
            for s in range(segments):
                bm.faces.new((a[s], b[0], a[(s + 1) % segments]))
        else:
            for s in range(segments):
                bm.faces.new((a[s], a[(s + 1) % segments], b[(s + 1) % segments], b[s]))
    if caps and len(rings[0]) > 1:
        bm.faces.new(list(reversed(rings[0])))
    if caps and len(rings[-1]) > 1:
        bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def cylinder(r, h, segments=32, r_top=None):
    return lathe([(0, 0), (r, 0), (r if r_top is None else r_top, h), (0, h)], segments)


def circle_profile(r, n=12, sx=1.0, sy=1.0, phase=0.0):
    return [(r * sx * math.cos(2 * math.pi * i / n + phase), r * sy * math.sin(2 * math.pi * i / n + phase)) for i in range(n)]


def rect_profile(w, h, chamfer=0.0):
    hw, hh, c = w / 2, h / 2, chamfer
    if c <= 0:
        return [(-hw, -hh), (hw, -hh), (hw, hh), (-hw, hh)]
    return [(-hw + c, -hh), (hw - c, -hh), (hw, -hh + c), (hw, hh - c), (hw - c, hh), (-hw + c, hh), (-hw, hh - c), (-hw, -hh + c)]


def sweep(points, profile, up=(0, 0, 1), closed=False, caps=True, scales=None):
    """Extrude a closed 2D profile (cm, in the path's binormal/normal plane) along a polyline (cm).

    Corners are mitred so sharp bends keep the profile's thickness.
    """
    pts, keep = [], []
    for i, p in enumerate(points):
        v = Vector(p) * CM
        if pts and (v - pts[-1]).length < 1e-7:
            continue  # duplicate point: no direction
        pts.append(v)
        keep.append(i)
    if scales:
        scales = [scales[i] for i in keep]
    prof = [(x * CM, y * CM) for x, y in profile]
    n = len(pts)
    upv = Vector(up).normalized()
    bm = bmesh.new()
    rings = []
    for i in range(n):
        prev = pts[i - 1] if (closed or i > 0) else None
        nxt = pts[(i + 1) % n] if (closed or i < n - 1) else None
        t_in = (pts[i] - prev).normalized() if prev is not None and (pts[i] - prev).length > 1e-9 else None
        t_out = (nxt - pts[i]).normalized() if nxt is not None and (nxt - pts[i]).length > 1e-9 else None
        t = ((t_in or t_out) + (t_out or t_in)).normalized()
        b = t.cross(upv)
        if b.length < 1e-6:
            b = t.cross(Vector((1, 0, 0)))
        b.normalize()
        nrm = b.cross(t).normalized()
        bend = None
        miter = 1.0
        if t_in is not None and t_out is not None and (t_out - t_in).length > 1e-6:
            bend = (t_out - t_in).normalized()
            miter = 1.0 / max(t.dot(t_out), 0.35)
        s = scales[i] if scales else 1.0
        ring = []
        for x, y in prof:
            off = (b * x + nrm * y) * s
            if bend is not None:
                off += bend * off.dot(bend) * (miter - 1.0)
            ring.append(bm.verts.new(pts[i] + off))
        rings.append(ring)
    m = len(prof)
    pairs = list(zip(rings, rings[1:])) + ([(rings[-1], rings[0])] if closed else [])
    for a, c in pairs:
        for k in range(m):
            bm.faces.new((a[k], a[(k + 1) % m], c[(k + 1) % m], c[k]))
    if caps and not closed:
        bm.faces.new(list(reversed(rings[0])))
        bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def torus(major, minor, n_major=48, n_minor=12):
    path = [(major * math.cos(2 * math.pi * i / n_major), major * math.sin(2 * math.pi * i / n_major), 0) for i in range(n_major)]
    return sweep(path, circle_profile(minor, n_minor), up=(0, 0, 1), closed=True)


def sphere(r, segments=24, rings=12, sz=1.0):
    prof = [(r * math.sin(math.pi * i / rings), r * sz * (1 - math.cos(math.pi * i / rings))) for i in range(rings + 1)]
    return lathe(prof, segments)  # sits on z=0


def arc_points(center, radius, a0, a1, steps, plane="xy", z=0.0):
    """Points on a circular arc (degrees), plane 'xy', 'xz' or 'yz'."""
    out = []
    for i in range(steps + 1):
        a = math.radians(a0 + (a1 - a0) * i / steps)
        c, s = math.cos(a) * radius, math.sin(a) * radius
        if plane == "xy":
            out.append((center[0] + c, center[1] + s, center[2]))
        elif plane == "xz":
            out.append((center[0] + c, center[1], center[2] + s))
        else:
            out.append((center[0], center[1] + c, center[2] + s))
    return out


def ngon_prism(points2d, h):
    """Flat polygon (cm, XY) extruded from z=0 to z=h."""
    bm = bmesh.new()
    bottom = [bm.verts.new((x * CM, y * CM, 0.0)) for x, y in points2d]
    top = [bm.verts.new((x * CM, y * CM, h * CM)) for x, y in points2d]
    bm.faces.new(list(reversed(bottom)))
    bm.faces.new(top)
    k = len(points2d)
    for i in range(k):
        bm.faces.new((bottom[i], bottom[(i + 1) % k], top[(i + 1) % k], top[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return bm


def gem(r, h_crown, h_pavilion, facets=8, table=0.55):
    """Brilliant-ish cut stone, girdle at z=0, table on top."""
    prof = [(0, -h_pavilion), (r, 0), (r, 0.05 * r), (r * table, h_crown), (0, h_crown)]
    return lathe(prof, facets)


# ---------------------------------------------------------------------------------------------
# Deformers (applied after modifiers, on metres)
# ---------------------------------------------------------------------------------------------


def puff(amount_cm, axis=2):
    """Cushion bulge: pushes the faces along +-axis outward, most in the middle."""

    def apply(me):
        coords = [v.co.copy() for v in me.vertices]
        if not coords:
            return
        lo = Vector([min(c[i] for c in coords) for i in range(3)])
        hi = Vector([max(c[i] for c in coords) for i in range(3)])
        centre = (lo + hi) / 2
        half = (hi - lo) / 2
        others = [i for i in range(3) if i != axis]
        for v in me.vertices:
            p = v.co - centre
            w = 1.0
            for i in others:
                w *= max(0.0, 1.0 - (p[i] / max(half[i], 1e-6)) ** 2)
            side = p[axis] / max(half[axis], 1e-6)
            v.co[axis] += math.copysign(amount_cm * CM * w * side * side, side)

    return apply


def wobble(amount_cm, scale_cm=20.0, seed=0):
    """Small organic irregularity (folded cloth, hand-made look)."""

    def apply(me):
        k = 2 * math.pi / (scale_cm * CM)
        for v in me.vertices:
            x, y, z = v.co
            d = math.sin(x * k + seed) * math.sin(y * k * 1.3 + seed * 2) * math.cos(z * k * 0.7 + seed * 3)
            d += 0.5 * math.sin(x * k * 2.1 + seed * 5) * math.cos(y * k * 1.7)
            v.co += v.normal * d * amount_cm * CM

    return apply


# ---------------------------------------------------------------------------------------------
# Item assembly
# ---------------------------------------------------------------------------------------------


def _bm_to_object(name, bm):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def _box_uv(me, matrix, grain, fit_axis):
    """World-space box projection (1 UV unit = 1 m), grain axis runs along U.

    fit_axis: planar projection along that axis, normalised to the part's extent (0..1).
    """
    uv_layer = me.uv_layers.new(name="UVMap") if not me.uv_layers else me.uv_layers[0]
    rot = matrix.to_3x3()
    world = [matrix @ v.co for v in me.vertices]
    axis_index = {"x": 0, "y": 1, "z": 2}
    g = axis_index[grain]
    if fit_axis:
        a = axis_index[fit_axis]
        u_ax, v_ax = [i for i in range(3) if i != a]
        if a == 0:
            u_ax, v_ax = 1, 2
        elif a == 1:
            u_ax, v_ax = 0, 2
        lo = [min(w[i] for w in world) for i in range(3)]
        hi = [max(w[i] for w in world) for i in range(3)]
    for poly in me.polygons:
        n = rot @ poly.normal
        if fit_axis:
            for li in poly.loop_indices:
                w = world[me.loops[li].vertex_index]
                u = (w[u_ax] - lo[u_ax]) / max(hi[u_ax] - lo[u_ax], 1e-9)
                v = (w[v_ax] - lo[v_ax]) / max(hi[v_ax] - lo[v_ax], 1e-9)
                if a == 1 and n[1] > 0:
                    u = 1.0 - u
                uv_layer.data[li].uv = (u, v)
            continue
        dominant = max(range(3), key=lambda i: abs(n[i]))
        plane = [i for i in range(3) if i != dominant]
        if g in plane:
            u_ax = g
            v_ax = plane[0] if plane[1] == g else plane[1]
        else:
            u_ax, v_ax = plane
        flip = -1.0 if n[dominant] < 0 and dominant != 2 else 1.0
        for li in poly.loop_indices:
            w = world[me.loops[li].vertex_index]
            uv_layer.data[li].uv = (w[u_ax] * (flip if u_ax != g else 1.0), w[v_ax])


class Item:
    def __init__(self, item_id, materials):
        self.id = item_id
        self.parts = []
        self.materials = materials  # name -> bpy material

    def add(self, bm, mat, loc=(0, 0, 0), rot=(0, 0, 0), bevel=0.0, seg=3, shade="auto", angle=35.0,
            grain="x", fit=None, post=None, profile=0.5):
        """Add one part. loc in cm, rot in degrees (XYZ).

        shade: 'auto' (smooth with sharp edges above `angle`), 'smooth', 'flat'.
        bevel: rounded edges (cm) with hardened normals – the main trick for the 'real object' look.
        """
        ob = _bm_to_object(f"{self.id}_part{len(self.parts)}", bm)
        me = ob.data
        me.shade_smooth()
        if shade == "auto" and bevel <= 0:
            me.set_sharp_from_angle(angle=math.radians(angle))
        elif shade == "flat":
            me.shade_flat()
        if bevel > 0:
            mod = ob.modifiers.new("Bevel", "BEVEL")
            mod.width = bevel * CM
            mod.segments = seg
            mod.limit_method = "ANGLE"
            mod.angle_limit = math.radians(30)
            mod.profile = profile
            mod.use_clamp_overlap = True
            mod.harden_normals = shade != "smooth"
            if shade == "smooth":
                pass
        if ob.modifiers:
            dg = bpy.context.evaluated_depsgraph_get()
            new_me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg), preserve_all_data_layers=True, depsgraph=dg)
            ob.modifiers.clear()
            ob.data = new_me
            bpy.data.meshes.remove(me)
            me = new_me
        if post:
            for fn in (post if isinstance(post, (list, tuple)) else [post]):
                fn(me)
            me.update()
        ob.matrix_world = Matrix.Translation(Vector(loc) * CM) @ Euler([math.radians(a) for a in rot]).to_matrix().to_4x4()
        _box_uv(me, ob.matrix_world, grain, fit)
        me.materials.append(self.materials[mat])
        self.parts.append(ob)
        return ob

    def join(self):
        target_me = bpy.data.meshes.new(f"SM_{self.id}")
        target = bpy.data.objects.new(f"SM_{self.id}", target_me)
        bpy.context.scene.collection.objects.link(target)
        objs = [target] + self.parts
        for o in bpy.data.objects:
            try:
                o.select_set(False)
            except RuntimeError:
                pass
        for o in objs:
            o.select_set(True)
        bpy.context.view_layer.objects.active = target
        with bpy.context.temp_override(active_object=target, object=target, selected_objects=objs, selected_editable_objects=objs):
            bpy.ops.object.join()
        self.parts = []
        # The empty join target leaves an unused first slot; drop it.
        me = target.data
        if me.uv_layers:
            me.uv_layers.active_index = 0
            me.uv_layers[0].active_render = True
        if len(me.materials) and me.materials[0] is None:
            before = [p.material_index for p in me.polygons]
            me.materials.pop(index=0)
            if [p.material_index for p in me.polygons] == before:
                for p, idx in zip(me.polygons, before):
                    p.material_index = max(idx - 1, 0)
        return target
