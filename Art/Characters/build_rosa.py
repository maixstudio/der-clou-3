"""Rosa: 3D figure from the reference sheet (front, back, both sides), rigged and animated.

Run headless (Blender 5.2):
    blender -b --factory-startup -P Art/Characters/build_rosa.py -- [--stage mesh|rig|anim|all] [--preview] [--export]

Stages (each saves a .blend, the next one starts from it):
    mesh  silhouettes -> voxel hull (superellipse cross sections), head from the close-up with a face shaped after the
          profile, a bun knot on a rounded back of the head, hair relief -> smooth mesh; modelled hands, ears and loose
          curls joined in -> UVs -> projected colour texture, hand details and strands on the back and top painted on top
    rig   UE-style skeleton (root, pelvis, spine_01..03, neck_01, head, clavicle/upperarm/lowerarm/hand, five digits
          with three bones each, thigh/calf/foot/ball) from landmarks of the silhouettes; bone heat on the body, distance
          weights on the hands, ears and curls on the head
    anim  actions Rosa_Idle, Rosa_Walk, Rosa_Jog, Rosa_Work, Rosa_Pickup (in place, 30 fps)
--preview renders stills into Saved/Characters/Rosa/, --export writes Art/Characters/Rosa/Export/SK_Rosa.fbx + texture,
--stage showcase renders an MP4 of all actions.

Conventions as for the items: metres, origin between the feet on the floor, front = Blender -Y (Unreal +Y).
"""

import math
import os
import sys
import time

import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
REF = os.path.join(HERE, "Rosa", "ref")
EXPORT = os.path.join(HERE, "Rosa", "Export")
WORK = os.path.join(ROOT, "Saved", "Characters", "Rosa")
os.makedirs(WORK, exist_ok=True)
os.makedirs(EXPORT, exist_ok=True)

HEIGHT = 1.70          # hair top to sole, metres
VOXEL = 0.003          # hull resolution
BG_THRESHOLD = 0.09    # colour distance from the grey backdrop that counts as figure
HAND_HALF_DEPTH = 0.043
TEX_SIZE = 4096
TARGET_FACES = 24000   # triangles after decimation below the head
HEAD_FACES = 20000     # triangles for the head (close-up detail, hair relief)
HEAD_UV_SCALE = 1.7    # head islands get this much more texture space
HAND_UV_SCALE = 1.6
HEAD_SMOOTH_FROM = 1.40  # metres; above this the silhouettes are opened by HEAD_OPEN_PX
HEAD_OPEN_PX = 10
JAW_SMOOTH_TO = 1.555    # between HEAD_SMOOTH_FROM and this (neck, jaw) curls are removed harder
JAW_OPEN_PX = 22

VIEWS = {
    # name: file, horizontal world axis (+1 = image right is +axis), which axis
    "front": ("rosa_front.webp", "x", +1),
    "back": ("rosa_back.webp", "x", -1),
    "left": ("rosa_side_left.webp", "y", +1),    # seen from +X, face to the image left
    "right": ("rosa_side_right.webp", "y", -1),  # seen from -X
}


def log(*args):
    print("[rosa]", *args, flush=True)


def args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out = {"stage": "all", "preview": False, "export": False}
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "--stage":
            out["stage"] = argv[i + 1]
            i += 1
        elif a == "--preview":
            out["preview"] = True
        elif a == "--export":
            out["export"] = True
        i += 1
    return out


def node_of_type(nodes, type_name):
    return next(n for n in nodes if n.type == type_name)


# ---------------------------------------------------------------------------------------------
# Reference images
# ---------------------------------------------------------------------------------------------


def load_rgb(path):
    img = bpy.data.images.load(path, check_existing=True)
    w, h = img.size
    px = np.empty(w * h * 4, np.float32)
    img.pixels.foreach_get(px)
    return px.reshape(h, w, 4)[::-1, :, :3].copy()  # row 0 = top


def save_rgb(arr, path):
    h, w = arr.shape[:2]
    rgba = np.ones((h, w, 4), np.float32)
    rgba[:, :, :3] = arr if arr.ndim == 3 else arr[:, :, None]
    name = os.path.splitext(os.path.basename(path))[0]
    img = bpy.data.images.new(name, w, h, alpha=False)
    img.pixels.foreach_set(rgba[::-1].ravel())
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    return img


def dilate(m):
    out = m.copy()
    out[1:] |= m[:-1]
    out[:-1] |= m[1:]
    out[:, 1:] |= m[:, :-1]
    out[:, :-1] |= m[:, 1:]
    return out


def opened(m, radius):
    out = m.copy()
    for _ in range(radius):
        out = ~dilate(~out)
    for _ in range(radius):
        out = dilate(out)
    return out & m


def raw_figure(px):
    """Pixels clearly different from the grey backdrop (holes between curls stay backdrop)."""
    corners = np.concatenate([px[:40, :40].reshape(-1, 3), px[:40, -40:].reshape(-1, 3),
                              px[-40:, :40].reshape(-1, 3), px[-40:, -40:].reshape(-1, 3)])
    bg = np.median(corners, axis=0)
    return np.abs(px - bg).max(axis=2) > BG_THRESHOLD


def figure_mask(px):
    fg = raw_figure(px)
    # Background = everything reachable from the border; holes inside the figure become figure.
    free = ~fg
    reach = np.zeros_like(fg)
    reach[0], reach[-1], reach[:, 0], reach[:, -1] = free[0], free[-1], free[:, 0], free[:, -1]
    for _ in range(4000):
        grown = dilate(reach) & free
        if grown.sum() == reach.sum():
            break
        reach = grown
    return ~reach


def bleed(px, mask, iterations=48):
    """Push figure colours outwards so projections never pick up the grey backdrop at silhouette edges."""
    known = mask.copy()
    # Drop the anti-aliased rim first.
    for _ in range(2):
        known = ~dilate(~known)
    col = np.where(known[:, :, None], px, 0.0)
    for _ in range(iterations):
        acc = np.zeros_like(col)
        cnt = np.zeros(known.shape, np.float32)
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, -1), (1, -1), (-1, 1)):
            sh_c = np.roll(np.roll(col, dy, 0), dx, 1)
            sh_k = np.roll(np.roll(known, dy, 0), dx, 1)
            acc += sh_c * sh_k[:, :, None]
            cnt += sh_k
        new = (~known) & (cnt > 0)
        col[new] = acc[new] / cnt[new][:, None]
        known = known | new
    col[~known] = px[~known]
    return col


class View:
    def __init__(self, name):
        file, axis, sign = VIEWS[name]
        self.name, self.axis, self.sign = name, axis, sign
        self.path = os.path.join(REF, file)
        self.px = load_rgb(self.path)
        self.h, self.w = self.px.shape[:2]
        self.mask = figure_mask(self.px)
        rows = np.nonzero(self.mask.any(axis=1))[0]
        self.top, self.bottom = int(rows[0]), int(rows[-1])
        self.scale = HEIGHT / (self.bottom - self.top + 1)  # metres per pixel
        self.shape_mask(HEAD_OPEN_PX, JAW_OPEN_PX)
        # Horizontal centre: middle of the figure at hip height.
        cols = []
        for frac in np.linspace(0.50, 0.58, 9):
            r = int(round(self.bottom - frac * HEIGHT / self.scale))
            c = np.nonzero(self.mask[r])[0]
            cols.append((c[0] + c[-1]) / 2)
        self.centre = float(np.mean(cols))
        log(f"view {name}: rows {self.top}-{self.bottom}, {self.scale * 1000:.3f} mm/px, centre col {self.centre:.1f}")

    def row_of(self, z):
        return self.bottom + 0.5 - z / self.scale

    def shape_mask(self, head_open, jaw_open):
        """Shape mask: loose curls would become thin blades in the hull, so open (erode, then dilate) the head
        region strongly, neck and jaw even more, and the rest a little."""
        self.geo_mask = self.mask.copy()
        head_row = int(min(self.h - 1, max(1, self.row_of(HEAD_SMOOTH_FROM))))
        jaw_row = int(min(head_row, max(0, self.row_of(JAW_SMOOTH_TO))))
        pad = min(40, self.h - head_row)
        top = self.mask[:head_row + pad]
        self.geo_mask[:jaw_row] = opened(top, head_open)[:jaw_row]
        self.geo_mask[jaw_row:head_row] = opened(top, jaw_open)[jaw_row:head_row]
        if head_row < self.h:
            self.geo_mask[head_row:] = opened(self.mask[max(0, head_row - 40):], 2)[min(40, head_row):]
        # Curls hanging beside the face are separate runs in a row; keep only the widest run per row above the
        # shoulders.
        for r in range(0, head_row):
            row = self.geo_mask[r]
            if not row.any():
                continue
            edges = np.nonzero(np.diff(np.concatenate([[0], row.astype(np.int8), [0]])))[0]
            spans = list(zip(edges[::2], edges[1::2]))
            if len(spans) > 1:
                a, b = max(spans, key=lambda s: s[1] - s[0])
                row[:] = False
                row[a:b] = True

    def sample_mask(self, hs, zs):
        """Shape mask on a (len(zs), len(hs)) grid, nearest pixel."""
        rows = np.clip(np.round(self.bottom + 0.5 - zs / self.scale - 0.5).astype(int), 0, self.h - 1)
        cols = np.clip(np.round(self.centre + self.sign * hs / self.scale).astype(int), 0, self.w - 1)
        return self.geo_mask[rows[:, None], cols[None, :]]

    def sample_rgb(self, hs, zs):
        rows = np.clip(np.round(self.bottom + 0.5 - zs / self.scale - 0.5).astype(int), 0, self.h - 1)
        cols = np.clip(np.round(self.centre + self.sign * hs / self.scale).astype(int), 0, self.w - 1)
        return self.px[rows[:, None], cols[None, :]]


# ---------------------------------------------------------------------------------------------
# Head close-up (front and right profile side by side, ~0.38 mm per pixel). It replaces the full-body photos
# from Z_HEAD up. Placed on the body by matching eye and chin height of the full-body front photo.
# ---------------------------------------------------------------------------------------------

HEAD_FILE = "rosa_head.png"
HEAD_SPLIT_COL = 800          # front view left of it, profile right of it
Z_HEAD = 1.45                 # below: full-body photos, above: close-up (turtleneck)
BODY_EYE_ROW, BODY_EYE_COLS, BODY_CHIN_ROW = 93.6, (750.0, 786.0), 153.0   # in rosa_front.webp
HEAD_FRONT = dict(eye_row=389.0, eye_cols=(387.5, 544.0), chin_row=656.0, hair_row=210.0, mouth_row=561.0,
                  mouth_half=65.0, alae_half=42.5)
HEAD_SIDE = dict(eye_row=380.0, chin_row=658.0, sellion_row=387.5, subnasale_row=500.0, crease_row=600.0)


class HeadView(View):
    def __init__(self, kind, body_front, body_left):
        img = load_rgb(os.path.join(REF, HEAD_FILE))
        self.name = f"head_{kind}"
        z_eye = (body_front.bottom + 0.5 - BODY_EYE_ROW) * body_front.scale
        z_chin = (body_front.bottom + 0.5 - BODY_CHIN_ROW) * body_front.scale
        if kind == "front":
            self.px = img[:, :HEAD_SPLIT_COL].copy()
            self.axis, self.sign = "x", +1
            L = HEAD_FRONT
        else:
            self.px = img[:, HEAD_SPLIT_COL:].copy()
            self.axis, self.sign = "y", -1          # seen from -X: face to the image right
            L = HEAD_SIDE
        self.h, self.w = self.px.shape[:2]
        self.mask = figure_mask(self.px)
        self.scale = (z_eye - z_chin) / (L["chin_row"] - L["eye_row"])
        self.bottom = L["eye_row"] + z_eye / self.scale - 0.5
        if kind == "front":
            face_x = (0.5 * sum(BODY_EYE_COLS) - body_front.centre) * body_front.scale
            self.centre = 0.5 * sum(L["eye_cols"]) - face_x / self.scale
        else:
            # Line up the face profile (chin to forehead) with the full-body profile: median offset of the front edge.
            self.centre = 0.0
            offsets = []
            for z in np.arange(z_chin + 0.012, z_eye + 0.04, 0.002):
                cb = np.nonzero(body_left.mask[int(round(body_left.row_of(z)))])[0]
                ch = np.nonzero(self.mask[int(round(self.row_of(z)))])[0]
                if len(cb) and len(ch):
                    y_body = (cb[0] - body_left.centre) * body_left.scale * body_left.sign
                    offsets.append(ch[-1] + y_body / self.scale)
            self.centre = float(np.median(offsets))
        # Curls are finer here; open with the same physical radii as the full-body photos.
        self.shape_mask(int(round(HEAD_OPEN_PX * body_front.scale / self.scale)),
                        int(round(JAW_OPEN_PX * body_front.scale / self.scale)))
        self.top = int(np.nonzero(self.mask.any(axis=1))[0][0])
        log(f"view {self.name}: {self.scale * 1000:.3f} mm/px, hair top z {(self.bottom + 0.5 - self.top) * self.scale:.3f}")


# Right ear traced in the close-up profile (full-image pixels, clockwise from the top of the helix), the bowl
# (concha) centre, how far the ear sits from the face centre where it attaches and how much it tilts outwards.
EAR_OUTLINE = [(1265, 371.7), (1250, 378), (1235, 390), (1224, 410), (1221.7, 425), (1223, 440), (1227, 455),
               (1233, 467), (1241, 481), (1251.7, 493), (1268, 507), (1290, 517), (1303, 515), (1311.7, 510),
               (1314, 495), (1313, 480), (1310, 465), (1306, 450), (1300, 432), (1293, 413), (1288, 395),
               (1286.7, 380), (1278, 372)]
EAR_CONCHA = (1296, 447)
EAR_ATTACH_U = 0.064
EAR_TILT_DEG = 30.0


def catmull_rom(points, samples_per_segment=4):
    pts = [Vector(p) for p in points]
    n = len(pts)
    out = []
    for i in range(n):
        p0, p1, p2, p3 = pts[i - 1], pts[i], pts[(i + 1) % n], pts[(i + 2) % n]
        for k in range(samples_per_segment):
            t = k / samples_per_segment
            out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t
                              + (-p0 + 3 * p1 - 3 * p2 + p3) * t * t * t))
    return out


class EarModel:
    """Outline of the ear in body coordinates (y, z) and the region around it where the hair has to give way."""

    def __init__(self, side_view, face_cx):
        def yz(col, row):
            c = col - HEAD_SPLIT_COL
            return Vector((side_view.sign * (c - side_view.centre) * side_view.scale,
                           (side_view.bottom + 0.5 - row) * side_view.scale))

        self.outline = catmull_rom([yz(*p) for p in EAR_OUTLINE], 2)
        self.concha = yz(*EAR_CONCHA)
        ys = [p.x for p in self.outline]
        zs = [p.y for p in self.outline]
        self.y_front, self.y_back = min(ys), max(ys)
        self.centre = Vector(((min(ys) + max(ys)) / 2, (min(zs) + max(zs)) / 2))
        self.half = Vector(((max(ys) - min(ys)) / 2, (max(zs) - min(zs)) / 2))
        self.cx = face_cx
        log(f"ear: y {self.y_front:.3f}..{self.y_back:.3f}, z {min(zs):.3f}..{max(zs):.3f}")

    def clearance(self, ys, zs):
        """(len(zs), len(ys)): 1 where the head must stay at skull width (behind the ear), 0 far away."""
        dy = (ys[None, :] - self.centre.x) / (self.half.x + 0.006)
        dy = np.where(dy > 0, dy / 1.8, dy)       # behind the ear the hair widens gradually towards the bun
        dz = (zs[:, None] - self.centre.y) / (self.half.y + 0.006)
        r = np.sqrt(dy ** 2 + dz ** 2)
        return np.clip((2.2 - r) / 1.2, 0.0, 1.0)


def ear_object(side, ear):
    """Closed ear: a cupped sheet over the traced outline (rim, ridge, bowl), tilted away from the head,
    made solid with a solidify modifier and smoothed."""
    import bmesh
    s = SIDES[side]
    rings = 6
    bm = bmesh.new()
    tilt = math.tan(math.radians(EAR_TILT_DEG))
    grid = []
    for k in range(rings + 1):
        f = k / rings
        row = []
        for p in ear.outline:
            q = ear.concha + (p - ear.concha) * f
            out = max(0.0, q.x - ear.y_front) * tilt
            out += 0.0025 * math.exp(-((f - 0.92) / 0.09) ** 2)      # helix rim
            out += 0.0014 * math.exp(-((f - 0.62) / 0.10) ** 2)      # antihelix
            out -= 0.0055 * math.exp(-(f / 0.42) ** 2)               # bowl
            x = ear.cx + s * (EAR_ATTACH_U + max(out, -0.004))
            row.append(bm.verts.new((x, q.x, q.y)))
            if k == 0:
                break
        grid.append(row)
    n = len(ear.outline)
    centre = grid[0][0]
    for i in range(n):
        a, b = grid[1][i], grid[1][(i + 1) % n]
        bm.faces.new((centre, a, b) if s < 0 else (centre, b, a))
    for k in range(1, rings):
        for i in range(n):
            a, b = grid[k][i], grid[k][(i + 1) % n]
            c, d = grid[k + 1][(i + 1) % n], grid[k + 1][i]
            bm.faces.new((a, d, c, b) if s < 0 else (a, b, c, d))
    me = bpy.data.meshes.new(f"Ear_{side}")
    bm.to_mesh(me)
    bm.free()
    ob = link(bpy.data.objects.new(f"Ear_{side}", me))
    me.update()
    # Normals should point away from the head; flip if they don't.
    nx = sum(p.normal.x for p in me.polygons) / max(1, len(me.polygons))
    if nx * s < 0:
        me.flip_normals()
    sol = ob.modifiers.new("Solidify", "SOLIDIFY")
    sol.thickness = 0.0042
    sol.offset = -1.0
    sol.use_rim = True
    sub = ob.modifiers.new("Subsurf", "SUBSURF")
    sub.levels = 1
    sub.render_levels = 1
    apply_modifiers(ob)
    attr = ob.data.attributes.new("skin", "FLOAT", "POINT")
    attr.data.foreach_set("value", np.zeros(len(ob.data.vertices), np.float32))
    vg = ob.vertex_groups.new(name="rigid_head")
    vg.add(list(range(len(ob.data.vertices))), 1.0, "REPLACE")
    log(f"ear {side}: {len(ob.data.polygons)} faces")
    return ob


class FaceModel:
    """Front surface of the face, y = f(x, z): midline from the close-up profile (brow, nose, lips, chin), a
    superellipse falling off to the sides, eye sockets. Used to carve and fill the hull in front of the ears."""

    def __init__(self, front, side):
        self.front, self.side = front, side
        zf = lambda row: (front.bottom + 0.5 - row) * front.scale
        zs_ = lambda row: (side.bottom + 0.5 - row) * side.scale
        F, S = HEAD_FRONT, HEAD_SIDE
        self.z_eye, self.z_chin, self.z_hair = zf(F["eye_row"]), zf(F["chin_row"]), zf(F["hair_row"])
        self.z_mouth = zf(F["mouth_row"])
        self.z_root, self.z_base, self.z_crease = zs_(S["sellion_row"]), zs_(S["subnasale_row"]), zs_(S["crease_row"])
        self.cx = (0.5 * sum(F["eye_cols"]) - front.centre) * front.scale
        self.eye_u = 0.5 * (F["eye_cols"][1] - F["eye_cols"][0]) * front.scale
        self.mouth_half = F["mouth_half"] * front.scale
        self.alae_half = F["alae_half"] * front.scale
        # Midline profile from the unopened profile mask (front edge = last figure pixel, face to the right).
        self.zt = np.arange(self.z_chin - 0.004, self.z_hair + 0.004, 0.0005)
        prof = []
        for z in self.zt:
            r = int(round(side.row_of(z)))
            cols = np.nonzero(side.mask[r])[0]
            prof.append((cols[-1] + 0.5 - side.centre) * side.scale * side.sign if len(cols) else 0.0)
        p = np.convolve(np.array(prof), np.ones(5) / 5, mode="same")
        p[:2], p[-2:] = prof[:2], prof[-2:]
        self.p = p

        def at(z):
            return float(np.interp(z, self.zt, p))

        b = p.copy()
        nose = (self.zt >= self.z_base) & (self.zt <= self.z_root)
        b[nose] = np.interp(self.zt[nose], [self.z_base, self.z_root], [at(self.z_base), at(self.z_root)])
        lips = (self.zt >= self.z_crease) & (self.zt < self.z_base)
        b[lips] = np.interp(self.zt[lips], [self.z_crease, self.z_base], [at(self.z_crease), at(self.z_base)])
        self.b = b
        self.d_nose = np.where(nose, np.maximum(b - p, 0.0), 0.0)
        self.d_lips = np.where(lips, np.maximum(b - p, 0.0), 0.0)
        # Half width and depth of the face (front edge to the side plane in front of the ears).
        zk = [self.z_chin - 0.004, self.z_chin, self.z_chin + 0.01, self.z_crease, self.z_mouth, self.z_base,
              self.z_eye, self.z_eye + 0.02, self.z_hair + 0.004]
        self.W = np.interp(self.zt, zk, [0.026, 0.030, 0.045, 0.055, 0.061, 0.067, 0.069, 0.068, 0.065])
        self.D = np.interp(self.zt, zk, [0.060, 0.065, 0.075, 0.085, 0.090, 0.098, 0.100, 0.100, 0.095])
        log(f"face: eyes z {self.z_eye:.3f} u {self.eye_u:.4f}, nose root/base z {self.z_root:.3f}/{self.z_base:.3f}, "
            f"nose tip y {p[nose].min() if nose.any() else 0:.4f}, chin z {self.z_chin:.3f}, hairline z {self.z_hair:.3f}")

    def in_zone(self, z):
        return (z >= self.zt[0]) & (z <= self.zt[-1])

    def surface(self, xs, zs):
        """(len(zs), len(xs)) arrays: front surface y, side plane y, half width W."""
        u = (xs - self.cx)[None, :]
        z = zs[:, None]
        b = np.interp(zs, self.zt, self.b)[:, None]
        W = np.interp(zs, self.zt, self.W)[:, None]
        D = np.interp(zs, self.zt, self.D)[:, None]
        q = 2.4
        t = np.clip(np.abs(u) / W, 0.0, 1.0)
        y = b + D * (1.0 - (1.0 - t ** q) ** (1.0 / q))
        # Nose: narrow at the root, alae at the base.
        dn = np.interp(zs, self.zt, self.d_nose)[:, None]
        wn = 0.009 + (self.alae_half - 0.009) * np.clip((self.z_root - z) / max(self.z_root - self.z_base, 1e-3), 0, 1)
        y -= dn * np.clip(1.0 - (u / wn) ** 2, 0.0, 1.0) ** 1.3
        dl = np.interp(zs, self.zt, self.d_lips)[:, None]
        y -= dl * np.clip(1.0 - (u / self.mouth_half) ** 2, 0.0, 1.0) ** 0.6
        for e in (-1.0, 1.0):
            y += 0.013 * np.exp(-((u - e * self.eye_u) / 0.013) ** 2 - ((z - self.z_eye) / 0.009) ** 2)
        return y, b + D, W


def runs(m, coords):
    """For a boolean grid, per cell the [lo, hi] world bounds of the run along the last axis it lies in."""
    n = m.shape[-1]
    idx = np.arange(n)
    prev = np.zeros_like(m)
    prev[..., 1:] = m[..., :-1]
    nxt = np.zeros_like(m)
    nxt[..., :-1] = m[..., 1:]
    start = np.where(m & ~prev, idx, -1)
    start = np.maximum.accumulate(start, axis=-1)
    end = np.where(m & ~nxt, idx, n)
    end = np.minimum.accumulate(end[..., ::-1], axis=-1)[..., ::-1]
    start = np.clip(start, 0, n - 1)
    end = np.clip(end, 0, n - 1)
    step = coords[1] - coords[0]
    lo = coords[start] - step / 2
    hi = coords[end] + step / 2
    return lo, hi


def is_skin(rgb):
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    return (r > 0.32) & (r > g * 1.12) & (g > b * 1.02) & (r - b > 0.12)


def row_runs(row, coords):
    """List of (lo, hi) runs of a 1-D boolean row."""
    out = []
    step = coords[1] - coords[0]
    idx = np.nonzero(np.diff(np.concatenate([[0], row.astype(np.int8), [0]])))[0]
    for a, b in zip(idx[::2], idx[1::2]):
        out.append((coords[a] - step / 2, coords[b - 1] + step / 2))
    return out


# ---------------------------------------------------------------------------------------------
# Hull: front silhouette gives x-z, side silhouette gives y-z, each cross section is a superellipse
# inscribed in the two runs. Arms in T-pose are sliced along x instead (round, hands flat).
# ---------------------------------------------------------------------------------------------


def round_back_outline(S, ys, zs, centre=(0.02, 1.58), sigma_deg=9.0):
    """Profile mask (Z, Y): smooth the outline between crown and bun. Where the wisps end, the opened close-up
    leaves a shelf (4 cm deeper within 1 cm of height); the outline's distance from the centre of the head is
    smoothed over the angle and the quarter behind and above the centre rebuilt from it."""
    yc, zc = centre
    angles = np.arange(0.0, 90.01, 1.0)
    rs = np.arange(0.0, 0.25, 0.001)
    rho = np.zeros(len(angles))
    for i, a in enumerate(np.radians(angles)):
        iy = np.clip(np.round((yc + rs * math.sin(a) - ys[0]) / VOXEL).astype(int), 0, len(ys) - 1)
        iz = np.clip(np.round((zc + rs * math.cos(a) - zs[0]) / VOXEL).astype(int), 0, len(zs) - 1)
        out = np.nonzero(~S[iz, iy])[0]
        rho[i] = rs[out[0]] if len(out) else rs[-1]
    pad = int(3 * sigma_deg)
    k = np.exp(-0.5 * (np.arange(-pad, pad + 1) / sigma_deg) ** 2)
    rho_s = np.convolve(np.pad(rho, pad, mode="reflect"), k / k.sum(), mode="valid")
    dy, dz = ys[None, :] - yc, zs[:, None] - zc
    quad = (dy >= 0) & (dz >= 0)
    lim = np.interp(np.degrees(np.arctan2(dy, dz)), angles, rho_s)
    out = S.copy()
    out[quad] = (np.hypot(dy, dz) <= lim)[quad]
    log(f"back of the head: outline {rho.min():.3f}..{rho.max():.3f} m from the centre, largest change "
        f"{np.abs(rho_s - rho).max() * 100:.1f} cm")
    return out


class Hull:
    def __init__(self, front, side, head_front=None, head_side=None):
        self.front, self.side = front, side
        self.xs = np.arange(-0.90, 0.90, VOXEL) + VOXEL / 2
        self.zs = np.arange(0.0, HEIGHT + 2 * VOXEL, VOXEL) + VOXEL / 2
        y_ext = []
        for v in (side, head_side):
            if v is None:
                continue
            cols = np.nonzero(v.mask.any(axis=0))[0]
            y_ext += [(cols[0] - v.centre) * v.scale * v.sign, (cols[-1] - v.centre) * v.scale * v.sign]
        y_lo, y_hi = min(y_ext) - 0.03, max(y_ext) + 0.03
        self.ys = np.arange(y_lo, y_hi, VOXEL) + VOXEL / 2
        xs, ys, zs = self.xs, self.ys, self.zs
        self.F = front.sample_mask(xs, zs)            # (Z, X)
        self.S = side.sample_mask(ys, zs)             # (Z, Y)
        self.skin = is_skin(front.sample_rgb(xs, zs)) & self.F
        self.face = None
        if head_front is not None:
            top = zs >= Z_HEAD
            self.F[top] = head_front.sample_mask(xs, zs[top])
            self.S[top] = head_side.sample_mask(ys, zs[top])
            self.S = round_back_outline(self.S, ys, zs)
            self.face = FaceModel(head_front, head_side)
            self.ear = EarModel(head_side, self.face.cx)
        self._arms()

    def _arms(self):
        xs, zs = self.xs, self.zs
        x0 = int(np.argmin(np.abs(xs)))
        lo, hi = runs(self.F, xs)
        has = self.F[:, x0]
        L = np.where(has, lo[:, x0], 0.0)
        R = np.where(has, hi[:, x0], 0.0)
        self.centre_L, self.centre_R, self.centre_has = L, R, has
        arm_rows = has & ((R > 0.45) | (L < -0.45))
        z_arm_lo, z_arm_hi = zs[arm_rows].min(), zs[arm_rows].max()
        below = has & (zs > z_arm_lo - 0.14) & (zs < z_arm_lo - 0.05)
        torso_r, torso_l = float(np.median(R[below])), float(np.median(L[below]))
        self.xs_r, self.xs_l = torso_r + 0.012, torso_l - 0.012

        # Armpit: from the arm band downwards, first row whose centre run fits inside the torso.
        def pit(vals, limit, sign):
            for iz in range(len(zs) - 1, -1, -1):
                if zs[iz] > z_arm_lo:
                    continue
                if has[iz] and sign * vals[iz] <= sign * limit:
                    return zs[iz]
            return z_arm_lo

        z_pit_r, z_pit_l = pit(R, self.xs_r, +1), pit(L, self.xs_l, -1)
        self.z_cut = min(z_pit_r, z_pit_l) + 0.004
        log(f"arms: band z {z_arm_lo:.3f}-{z_arm_hi:.3f}, torso x {torso_l:.3f}/{torso_r:.3f}, "
            f"armpit z {z_pit_l:.3f}/{z_pit_r:.3f}")
        overlap = 0.045
        upper = zs[:, None] > self.z_cut
        side_ok = (xs[None, :] > self.xs_r - overlap) | (xs[None, :] < self.xs_l + overlap)
        self.A = self.F & upper & side_ok
        alo, ahi = runs(self.A.T, zs)
        self.arm_mid = ((alo + ahi) / 2).T
        self.arm_half = ((ahi - alo) / 2).T
        skin_cnt = (self.skin & self.A).sum(axis=0)
        cnt = self.A.sum(axis=0)
        self.hand_col = (cnt > 0) & (skin_cnt > 0.5 * np.maximum(cnt, 1)) & (np.abs(xs) > 0.45)
        # The sleeve ends at the wrist; the hands are modelled separately (hand_object).
        self.A_full = self.A.copy()
        self.A[:, self.hand_col] = False
        z_arm = 0.5 * (z_arm_lo + z_arm_hi)
        iz = int(np.argmin(np.abs(zs - z_arm)))
        best = max(row_runs(self.S[iz], self.ys), key=lambda r: r[1] - r[0])
        self.arm_y = 0.5 * (best[0] + best[1]) + 0.005
        self.z_arm_lo, self.z_arm_hi = z_arm_lo, z_arm_hi
        hx = xs[self.hand_col]
        log(f"hands: left x {hx[hx < 0].min():.3f}..{hx[hx < 0].max():.3f}, right x {hx[hx > 0].min():.3f}.."
            f"{hx[hx > 0].max():.3f}, arm depth centre y {self.arm_y:.3f}")

    @staticmethod
    def exponent(z):
        # Rounder head and limbs, a little boxier jacket.
        p = np.full_like(z, 2.2)
        p[(z > 0.80) & (z < 1.42)] = 2.5
        p[z >= 1.42] = 2.0
        return p

    def voxels(self):
        xs, ys, zs = self.xs, self.ys, self.zs
        Fb = self.F.copy()
        upper = zs > self.z_cut
        Fb[upper] &= (xs >= self.xs_l) & (xs <= self.xs_r)
        flo, fhi = runs(Fb, xs)
        slo, shi = runs(self.S, ys)
        fm, fh = (flo + fhi) / 2, np.maximum((fhi - flo) / 2, 1e-4)
        sm, sh = (slo + shi) / 2, np.maximum((shi - slo) / 2, 1e-4)
        hy_arm = np.where(self.hand_col[None, :], np.maximum(HAND_HALF_DEPTH, self.arm_half), self.arm_half)
        if self.face is not None:
            # Back of the head: per height the profile run, the skull taken in beside the bun, the bun's centre.
            s_lo, s_hi = np.zeros(len(zs)), np.zeros(len(zs))
            for iz in np.nonzero(zs >= 1.45)[0]:
                rr = row_runs(self.S[iz], ys)
                if rr:
                    s_lo[iz], s_hi[iz] = max(rr, key=lambda r: r[1] - r[0])
            s_mid = 0.5 * (s_lo + s_hi)
            t = (zs - BUN["z"]) / (1.5 * BUN["rz"])
            skull_back = s_hi - BUN["depth"] * np.clip(1 - t * t, 0, None) ** 2     # no ledge above or below
            bun_y = s_hi[int(np.argmin(np.abs(zs - BUN["z"])))] - BUN["ry"]
            bun_x = self.face.cx + BUN["u"]
        V = np.zeros((len(zs), len(xs), len(ys)), bool)
        chunk = 24
        for a in range(0, len(zs), chunk):
            b = min(a + chunk, len(zs))
            p = self.exponent(zs[a:b])[:, None, None]
            ex = np.abs((xs[None, :, None] - fm[a:b, :, None]) / fh[a:b, :, None]) ** p
            ey = np.abs((ys[None, None, :] - sm[a:b, None, :]) / sh[a:b, None, :]) ** p
            body = Fb[a:b, :, None] & self.S[a:b, None, :] & (ex + ey <= 1.0)
            az = ((zs[a:b, None, None] - self.arm_mid[a:b, :, None]) / np.maximum(self.arm_half[a:b, :, None], 1e-4)) ** 2
            ay = ((ys[None, None, :] - self.arm_y) / np.maximum(hy_arm[a:b, :, None], 1e-4)) ** 2
            arm = self.A[a:b, :, None] & (az + ay <= 1.0)
            V[a:b] = body | arm
            if self.face is not None:
                zz = zs[a:b]
                k = np.nonzero((zz >= 1.47) & (s_hi[a:b] > s_mid[a:b] + 0.01))[0]
                if len(k):
                    y = ys[None, None, :]
                    mid = s_mid[a:b][k][:, None, None]
                    depth = np.maximum(skull_back[a:b][k] - s_mid[a:b][k], 1e-3)[:, None, None]
                    skull = (y <= mid) | (ex[k] + ((y - mid) / depth) ** 2 <= 1.0)
                    bun = (((xs[None, :, None] - bun_x) / BUN["rx"]) ** 2 + ((y - bun_y) / BUN["ry"]) ** 2
                           + ((zz[k][:, None, None] - BUN["z"]) / BUN["rz"]) ** 2 <= 1.0)
                    V[a:b][k] &= skull | bun
                zone = self.face.in_zone(zs[a:b])
                if zone.any():
                    zz = zs[a:b][zone]
                    yf, y_side, W = self.face.surface(xs, zz)
                    inside_w = np.abs(xs[None, :] - self.face.cx) <= W           # (z, x)
                    y = ys[None, None, :]
                    # In front of the ears the face surface decides; beside the face nothing reaches past the
                    # side plane (hair would otherwise bulge in front of the cheeks). Towards the temples hair
                    # beside the face may come forward again, gradually, so there is no ledge.
                    f = self.face
                    temple = np.clip((zz[:, None] - f.z_mouth) / (f.z_eye + 0.02 - f.z_mouth), 0.0, 1.0)
                    limit = np.where(inside_w, yf, y_side - 0.08 * temple)[:, :, None]
                    solid = inside_w[:, :, None] & (y >= yf[:, :, None]) & (y <= y_side[:, :, None] + 0.01)
                    V[a:b][zone] = (V[a:b][zone] & (y >= limit)) | solid
                # Behind the ears the head stays at skull width so the modelled ears stand free of the hair.
                clear = self.ear.clearance(ys, zs[a:b])                      # (z, y)
                if clear.any():
                    # Hair tucked behind the ear: slopes down to skull width instead of a steep wall.
                    limit_u = EAR_ATTACH_U + (1.0 - clear[:, None, :]) * 0.04 + (clear[:, None, :] == 0) * 0.25
                    V[a:b] &= np.abs(xs[None, :, None] - self.face.cx) <= limit_u
        log(f"voxels: {int(V.sum())} filled of {V.size} {V.shape}")
        return V

    def lattice_origin(self):
        return (self.xs[0] - VOXEL / 2, self.ys[0] - VOXEL / 2, self.zs[0] - VOXEL / 2)


def voxel_surface(V, origin):
    """Quads on every face between a filled and an empty voxel. V is (Z, X, Y)."""
    Z, X, Y = V.shape
    nx_, ny_ = X + 1, Y + 1

    def vid(iz, ix, iy):
        return (iz * nx_ + ix) * ny_ + iy

    quads = []
    P = np.zeros((Z + 2, X, Y), bool)
    P[1:-1] = V
    d = P[1:] != P[:-1]
    pos = P[:-1] & ~P[1:]
    k, i, j = np.nonzero(d)
    q = np.stack([vid(k, i, j), vid(k, i + 1, j), vid(k, i + 1, j + 1), vid(k, i, j + 1)], 1)
    flip = ~pos[k, i, j]
    q[flip] = q[flip][:, ::-1]
    quads.append(q)
    P = np.zeros((Z, X + 2, Y), bool)
    P[:, 1:-1] = V
    d = P[:, 1:] != P[:, :-1]
    pos = P[:, :-1] & ~P[:, 1:]
    k, i, j = np.nonzero(d)
    q = np.stack([vid(k, i, j), vid(k, i, j + 1), vid(k + 1, i, j + 1), vid(k + 1, i, j)], 1)
    flip = ~pos[k, i, j]
    q[flip] = q[flip][:, ::-1]
    quads.append(q)
    P = np.zeros((Z, X, Y + 2), bool)
    P[:, :, 1:-1] = V
    d = P[:, :, 1:] != P[:, :, :-1]
    pos = P[:, :, :-1] & ~P[:, :, 1:]
    k, i, j = np.nonzero(d)
    q = np.stack([vid(k, i, j), vid(k + 1, i, j), vid(k + 1, i + 1, j), vid(k, i + 1, j)], 1)
    flip = ~pos[k, i, j]
    q[flip] = q[flip][:, ::-1]
    quads.append(q)
    quads = np.concatenate(quads)
    used, inv = np.unique(quads, return_inverse=True)
    quads = inv.reshape(-1, 4)
    iz = used // (nx_ * ny_)
    rem = used % (nx_ * ny_)
    ix, iy = rem // ny_, rem % ny_
    ox, oy, oz = origin
    co = np.stack([ox + ix * VOXEL, oy + iy * VOXEL, oz + iz * VOXEL], 1).astype(np.float32)
    me = bpy.data.meshes.new("RosaVoxels")
    me.vertices.add(len(co))
    me.vertices.foreach_set("co", co.ravel())
    me.loops.add(len(quads) * 4)
    me.loops.foreach_set("vertex_index", quads.ravel().astype(np.int32))
    me.polygons.add(len(quads))
    me.polygons.foreach_set("loop_start", np.arange(0, len(quads) * 4, 4, dtype=np.int32))
    me.update(calc_edges=True)
    log(f"voxel surface: {len(co)} verts, {len(quads)} quads")
    return me


# ---------------------------------------------------------------------------------------------
# Blender helpers
# ---------------------------------------------------------------------------------------------


def clean_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.preferences.filepaths.save_version = 0   # no .blend1 backups next to rosa.blend
    scene = bpy.context.scene
    scene.render.fps = 30
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0


def link(ob):
    bpy.context.scene.collection.objects.link(ob)
    return ob


def activate(ob):
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)


def apply_modifiers(ob):
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
    old = ob.data
    ob.modifiers.clear()
    ob.data = me
    me.name = old.name
    bpy.data.meshes.remove(old)


def blend_path(stage):
    return os.path.join(WORK, f"rosa_{stage}.blend")


def save_blend(stage):
    bpy.ops.wm.save_as_mainfile(filepath=blend_path(stage), compress=True)
    log("saved", blend_path(stage))


def open_blend(stage):
    bpy.ops.wm.open_mainfile(filepath=blend_path(stage))


class Nodes:
    """Tiny helper to wire shader math without drowning in boilerplate."""

    def __init__(self, nt):
        self.nt = nt

    def _set(self, sock, v):
        if v is None:
            return
        if isinstance(v, (int, float)):
            sock.default_value = v
        elif isinstance(v, tuple):
            sock.default_value = v
        else:
            self.nt.links.new(v, sock)

    def math(self, op, a, b=None, c=None, clamp=False):
        n = self.nt.nodes.new("ShaderNodeMath")
        n.operation = op
        n.use_clamp = clamp
        for i, v in enumerate((a, b, c)):
            self._set(n.inputs[i], v)
        return n.outputs[0]

    def vmath(self, op, a, b=None, scale=None):
        n = self.nt.nodes.new("ShaderNodeVectorMath")
        n.operation = op
        self._set(n.inputs[0], a)
        self._set(n.inputs[1], b)
        if scale is not None:
            self._set(n.inputs["Scale"], scale)
        return n.outputs[0]

    def separate(self, v):
        n = self.nt.nodes.new("ShaderNodeSeparateXYZ")
        self.nt.links.new(v, n.inputs[0])
        return n.outputs[0], n.outputs[1], n.outputs[2]

    def combine(self, x, y, z):
        n = self.nt.nodes.new("ShaderNodeCombineXYZ")
        for i, v in enumerate((x, y, z)):
            self._set(n.inputs[i], v)
        return n.outputs[0]

    def image(self, img, vector):
        n = self.nt.nodes.new("ShaderNodeTexImage")
        n.image = img
        n.interpolation = "Linear"
        n.extension = "EXTEND"
        self.nt.links.new(vector, n.inputs["Vector"])
        return n.outputs["Color"]


# ---------------------------------------------------------------------------------------------
# Stage: mesh
# ---------------------------------------------------------------------------------------------


def projection_material(views, heads, hull, skin_rgb):
    """Emission material that paints every surface point with the reference photo looking at it most directly.
    From Z_HEAD up the close-up replaces the full-body front and side photos. Vertices with the attribute
    "skin" (the modelled hands) get a plain skin colour instead."""
    mat = bpy.data.materials.new("RosaProjection")
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    N = Nodes(nt)
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    px_, py_, pz_ = N.separate(geo.outputs["Position"])
    nx, ny, nz = N.separate(geo.outputs["Normal"])

    def lerp_v(a, b, t):
        return N.vmath("ADD", N.vmath("SCALE", a, scale=N.math("SUBTRACT", 1.0, t)), N.vmath("SCALE", b, scale=t))

    def project(name, v):
        img = save_rgb(bled_px(v), os.path.join(WORK, f"proj_{name}.png"))
        h_coord = px_ if v.axis == "x" else py_
        u = N.math("MULTIPLY_ADD", h_coord, v.sign / (v.scale * v.w), v.centre / v.w)
        vv = N.math("MULTIPLY_ADD", pz_, 1.0 / (v.scale * v.h), 1.0 - (v.bottom + 0.5) / v.h)
        return N.image(img, N.combine(u, vv, 0.0))

    c = {name: project(name, v) for name, v in views.items()}
    hc = {name: project(f"head_{name}", v) for name, v in heads.items()}
    head = N.math("DIVIDE", N.math("SUBTRACT", pz_, Z_HEAD), 0.015, clamp=True)
    c["front"] = lerp_v(c["front"], hc["front"], head)
    c["left"] = lerp_v(c["left"], hc["side"], head)
    c["right"] = lerp_v(c["right"], hc["side"], head)
    x_s = 0.5 * (hull.xs_r - hull.xs_l)
    arm_gate = N.math("MULTIPLY",
                      N.math("DIVIDE", N.math("SUBTRACT", N.math("ABSOLUTE", px_), x_s), 0.03, clamp=True),
                      N.math("DIVIDE", N.math("SUBTRACT", pz_, hull.z_cut), 0.02, clamp=True))
    side_gate = N.math("SUBTRACT", 1.0, arm_gate)
    # The face itself (normals pointing forward) only from the front photo; cheeks, ears and hair also from the
    # profile, whose midline the face shape is built from.
    side_ok = N.math("DIVIDE", N.math("ADD", ny, 0.65), 0.3, clamp=True)
    side_gate = N.math("MULTIPLY", side_gate,
                       N.math("ADD", N.math("SUBTRACT", 1.0, head), N.math("MULTIPLY", head, side_ok)))
    # Surfaces facing up or down (shoulders, hair top, soles) get front and back photo half and half.
    flat = N.math("ADD", N.math("MULTIPLY", N.math("MULTIPLY", nz, nz), 0.03), 0.0001)
    weights = {
        "front": N.math("ADD", N.math("POWER", N.math("MAXIMUM", N.math("MULTIPLY", ny, -1.0), 0.0), 3.0), flat),
        "back": N.math("ADD", N.math("POWER", N.math("MAXIMUM", ny, 0.0), 3.0), flat),
        "left": N.math("MULTIPLY", N.math("POWER", N.math("MAXIMUM", nx, 0.0), 3.0), side_gate),
        "right": N.math("MULTIPLY", N.math("POWER", N.math("MAXIMUM", N.math("MULTIPLY", nx, -1.0), 0.0), 3.0),
                        side_gate),
    }
    total, acc = None, None
    for name, w in weights.items():
        total = w if total is None else N.math("ADD", total, w)
        term = N.vmath("SCALE", c[name], scale=w)
        acc = term if acc is None else N.vmath("ADD", acc, term)
    col = N.vmath("SCALE", acc, scale=N.math("DIVIDE", 1.0, total))
    # Hands: skin colour with a little mottling.
    attr = nt.nodes.new("ShaderNodeAttribute")
    attr.attribute_name = "skin"
    noise = nt.nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 260.0
    nt.links.new(geo.outputs["Position"], noise.inputs["Vector"])
    lin = tuple(((v + 0.055) / 1.055) ** 2.4 for v in skin_rgb)
    mott = N.math("MULTIPLY_ADD", noise.outputs["Fac"], 0.22, 0.89)
    skin = N.vmath("SCALE", N.combine(*lin), scale=mott)
    col = lerp_v(col, skin, attr.outputs["Fac"])
    # Loose curls: dark brown with copper streaks.
    curl = nt.nodes.new("ShaderNodeAttribute")
    curl.attribute_name = "curl"
    streak = nt.nodes.new("ShaderNodeTexNoise")
    streak.inputs["Scale"].default_value = 90.0
    streak.inputs["Detail"].default_value = 4.0
    nt.links.new(geo.outputs["Position"], streak.inputs["Vector"])
    copper = N.math("POWER", N.math("SUBTRACT", streak.outputs["Fac"], 0.25, clamp=True), 1.6)
    hair_col = lerp_v(N.combine(0.030, 0.014, 0.008), N.combine(0.20, 0.075, 0.025), N.math("MULTIPLY", copper, 1.6))
    col = lerp_v(col, hair_col, curl.outputs["Fac"])
    emit = nt.nodes.new("ShaderNodeEmission")
    nt.links.new(col, emit.inputs["Color"])
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(emit.outputs[0], out.inputs["Surface"])
    return mat


def final_material(tex):
    mat = bpy.data.materials.new("M_Rosa")
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = node_of_type(nt.nodes, "BSDF_PRINCIPLED")
    t = nt.nodes.new("ShaderNodeTexImage")
    t.image = tex
    nt.links.new(t.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.78
    for name in ("Specular IOR Level", "Specular"):
        if name in bsdf.inputs:
            bsdf.inputs[name].default_value = 0.3
            break
    return mat


def decimate_zones(ob, zones):
    """Decimate parts of the mesh to their own triangle budgets: zones = [(predicate(centre), triangles)]."""
    import bmesh
    activate(ob)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.context.tool_settings.mesh_select_mode = (False, False, True)
    for pred, target in zones:
        bpy.ops.mesh.select_all(action="DESELECT")
        bm = bmesh.from_edit_mesh(ob.data)
        tris = 0
        for f in bm.faces:
            if pred(f.calc_center_median()):
                f.select_set(True)
                tris += len(f.verts) - 2
        bm.select_flush_mode()
        bmesh.update_edit_mesh(ob.data)
        if tris:
            bpy.ops.mesh.decimate(ratio=min(1.0, target / tris))
    bpy.ops.object.mode_set(mode="OBJECT")


def skin_colour(head_front):
    """Average sRGB skin colour of the cheeks and chin in the close-up (the hands in the full-body photo are
    lit brighter than the face)."""
    F = HEAD_FRONT
    r0, r1 = int(F["eye_row"] + 40), int(F["chin_row"] - 15)
    c0, c1 = int(F["eye_cols"][0] - 40), int(F["eye_cols"][1] + 40)
    box = head_front.px[r0:r1, c0:c1]
    m = is_skin(box)
    return tuple(float(v) for v in box[m].mean(axis=0) * 0.97)


def stage_mesh():
    import bmesh
    t0 = time.time()
    clean_scene()
    views = {n: View(n) for n in VIEWS}
    heads = {"front": HeadView("front", views["front"], views["left"]),
             "side": HeadView("side", views["front"], views["left"])}
    hull = Hull(views["front"], views["left"], heads["front"], heads["side"])
    V = hull.voxels()
    me = voxel_surface(V, hull.lattice_origin())
    ob = link(bpy.data.objects.new("Rosa", me))
    activate(ob)
    rem = ob.modifiers.new("Remesh", "REMESH")
    rem.mode = "VOXEL"
    rem.voxel_size = VOXEL * 1.15
    rem.adaptivity = 0.0
    apply_modifiers(ob)
    # Smooth everything, the face only lightly so nose, lips and eye sockets survive.
    face = hull.face
    vg = ob.vertex_groups.new(name="smooth")
    co = np.empty(len(ob.data.vertices) * 3, np.float32)
    ob.data.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)

    def ramp(v, a, b):
        return np.clip((v - a) / (b - a), 0.0, 1.0)

    core = (ramp(co[:, 2], face.z_chin, face.z_chin + 0.01) * (1 - ramp(co[:, 2], face.z_hair - 0.025, face.z_hair - 0.005))
            * (1 - ramp(np.abs(co[:, 0] - face.cx), 0.045, 0.06)) * (1 - ramp(co[:, 1], -0.03, 0.0)))
    weight = 1.0 - 0.8 * core
    for w in np.unique(np.round(weight, 2)):
        vg.add(np.nonzero(np.round(weight, 2) == w)[0].tolist(), float(w), "REPLACE")
    sm = ob.modifiers.new("Smooth", "SMOOTH")
    sm.factor = 0.6
    sm.iterations = 12
    sm.vertex_group = "smooth"
    apply_modifiers(ob)
    ob.vertex_groups.remove(ob.vertex_groups["smooth"])
    hd = HairDetail(heads["side"], hull.face.cx)
    hair_relief(ob, views, heads, hull, hd)
    n_faces = len(ob.data.polygons)
    decimate_zones(ob, [(lambda c: c.z > Z_HEAD - 0.01, HEAD_FACES), (lambda c: c.z <= Z_HEAD - 0.01, TARGET_FACES)])
    log(f"mesh: {n_faces} -> {len(ob.data.polygons)} faces ({time.time() - t0:.1f} s)")
    # Hands with fingers, ears and loose curls, joined into the body as separate islands.
    for name in ("skin", "curl"):
        a = ob.data.attributes.new(name, "FLOAT", "POINT")
        a.data.foreach_set("value", np.zeros(len(ob.data.vertices), np.float32))
    # Scalp: head part of the hull, where paint_hair works later.
    zv = np.empty(len(ob.data.vertices) * 3, np.float32)
    ob.data.vertices.foreach_get("co", zv)
    a = ob.data.attributes.new("scalp", "FLOAT", "POINT")
    a.data.foreach_set("value", (zv.reshape(-1, 3)[:, 2] > 1.44).astype(np.float32))
    J = measure(hull)
    parts = [hand_object(side, J[f"hand_layout_{side}"]) for side in SIDES]
    parts += [ear_object(side, hull.ear) for side in SIDES]
    parts.append(curl_object(ob, hull.face.cx))
    for p in parts:
        for name in ("curl", "scalp"):
            if name not in p.data.attributes:
                a = p.data.attributes.new(name, "FLOAT", "POINT")
                a.data.foreach_set("value", np.zeros(len(p.data.vertices), np.float32))
    for o in bpy.context.view_layer.objects:
        o.select_set(o in parts or o == ob)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.join()
    ob.data.shade_smooth()
    log(f"with hands, ears and curls: {len(ob.data.polygons)} faces")
    # UVs; head and hands get more texture space than their surface would earn.
    activate(ob)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=0.003)
    bm = bmesh.from_edit_mesh(ob.data)
    uv = bm.loops.layers.uv.active
    skin_l = bm.verts.layers.float.get("skin")
    for f in bm.faces:
        scale = 1.0
        if skin_l is not None and min(v[skin_l] for v in f.verts) > 0.5:
            scale = HAND_UV_SCALE
        elif f.calc_center_median().z > 1.40:
            scale = HEAD_UV_SCALE
        if scale != 1.0:
            for loop in f.loops:
                loop[uv].uv *= scale
    bmesh.update_edit_mesh(ob.data)
    bpy.context.tool_settings.use_uv_select_sync = True
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.pack_islands(rotate=True, margin=0.002)
    bpy.ops.object.mode_set(mode="OBJECT")
    # Bake the projected colours.
    proj = projection_material(views, heads, hull, skin_colour(heads["front"]))
    ob.data.materials.clear()
    ob.data.materials.append(proj)
    tex = bpy.data.images.new("T_Rosa_BC", TEX_SIZE, TEX_SIZE, alpha=False)
    tn = proj.node_tree.nodes.new("ShaderNodeTexImage")
    tn.image = tex
    proj.node_tree.nodes.active = tn
    tn.select = True
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 4
    scene.render.bake.margin = 8
    activate(ob)
    t1 = time.time()
    bpy.ops.object.bake(type="EMIT")
    log(f"bake {time.time() - t1:.1f} s")
    paint_hand_details(tex, ob, J)
    paint_hair(tex, ob, hd)
    tex.filepath_raw = os.path.join(EXPORT, "T_Rosa_BC.png")
    tex.file_format = "PNG"
    tex.save()
    ob.data.materials.clear()
    ob.data.materials.append(final_material(tex))
    bpy.data.materials.remove(proj)
    # Remember what the rig needs.
    ob["arm_y"] = float(hull.arm_y)
    ob["x_s"] = [float(hull.xs_l), float(hull.xs_r)]
    ob["z_cut"] = float(hull.z_cut)
    ob["z_arm"] = [float(hull.z_arm_lo), float(hull.z_arm_hi)]
    save_blend("mesh")
    log(f"stage mesh done in {time.time() - t0:.1f} s")


# ---------------------------------------------------------------------------------------------
# Stage: rig
# ---------------------------------------------------------------------------------------------

SIDES = {"l": +1, "r": -1}   # character's left is Blender +X (she faces -Y)

# Hands are modelled (the photos show them edge-on): T-pose, palm down, thumb forward (-Y) on both hands.
# Per finger: offset across the palm (+ towards the little finger), knuckle distance from the wrist, length,
# radius at the base and at the tip, spread in degrees (+ towards the little finger). Metres for a 17.5 cm hand.
FINGERS = (
    ("index", -0.026, 0.093, 0.070, 0.0094, 0.0076, -4.0),
    ("middle", -0.008, 0.097, 0.077, 0.0098, 0.0078, -0.5),
    ("ring", 0.010, 0.094, 0.072, 0.0092, 0.0074, 3.5),
    ("pinky", 0.026, 0.086, 0.057, 0.0082, 0.0066, 8.0),
)
PHALANX = (0.46, 0.30, 0.24)
THUMB = dict(base=(0.022, -0.026, -0.005), dir=(0.80, -0.48, -0.30), lengths=(0.034, 0.029, 0.024),
             radii=(0.0128, 0.0112, 0.0100, 0.0090))
HAND_REF_LENGTH = 0.175     # wrist to middle fingertip in the table above
HAND_FACES = 5200           # triangles per hand
DIGITS = [f[0] for f in FINGERS] + ["thumb"]


def hand_layout(side, wrist, length):
    """Bones and shapes of one hand, scaled to `length` (wrist to middle fingertip)."""
    s = SIDES[side]
    k = length / HAND_REF_LENGTH
    A, B, C = Vector((s, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))

    def at(a, b, c=0.0):
        return wrist + (A * a + B * b + C * c) * k

    lay = {"joints": {}, "capsules": [], "blobs": [], "axes": (A, B, C), "wrist": wrist.copy(), "k": k,
           "dorsal": {}, "radii": {}}
    axes = (A, B, C)

    def digit(name, p, radii, d, dorsal):
        side_v = dorsal.cross(d).normalized()
        for i in range(3):
            lay["joints"][f"{name}_0{i + 1}_{side}"] = (p[i], p[i + 1])
            lay["capsules"].append((p[i], p[i + 1], radii[i] * k, radii[i + 1] * k))
        # Slightly thicker joints, a nail plate on the last segment.
        for j in (1, 2):
            r = radii[j] * k
            lay["blobs"].append((p[j] + dorsal * r * 0.15, Vector((r * 0.8, r * 1.04, r * 0.98)), 2.0, (d, side_v, dorsal)))
        r_n = (0.4 * radii[2] + 0.6 * radii[3]) * k
        lay["blobs"].append((p[2] + (p[3] - p[2]) * 0.60 + dorsal * r_n * 0.80,
                             Vector((0.0050 * k, 0.0043 * k, 0.0010 * k)), 2.6, (d, side_v, dorsal)))
        lay["dorsal"][name] = dorsal
        lay["radii"][name] = [r * k for r in radii]

    knuckles = []
    for name, b, ka, L, r0, r1, spread in FINGERS:
        sp = math.radians(spread)
        d = (A * math.cos(sp) + B * math.sin(sp) + C * -0.04).normalized()
        p = [at(ka, b)]
        for frac in PHALANX:
            p.append(p[-1] + d * L * frac * k)
        radii = [r0 + (r1 - r0) * t for t in (0.0, 0.46, 0.76, 1.0)]
        digit(name, p, radii, d, (C - C.dot(d) * d).normalized())
        knuckles.append((p[0], r0 * k))
        if name == "middle":
            lay["joints"][f"hand_{side}"] = (wrist.copy(), p[0])
    t = THUMB
    d = (A * t["dir"][0] + B * t["dir"][1] + C * t["dir"][2]).normalized()
    p = [at(*t["base"])]
    for L in t["lengths"]:
        p.append(p[-1] + d * L * k)
    up = (C * 0.75 - B * 0.65).normalized()             # the thumb nail faces up and forwards
    digit("thumb", p, t["radii"], d, (up - up.dot(d) * d).normalized())
    # Knuckle ridge, palm, ball of the thumb, pad on the little-finger side, wrist going into the sleeve.
    for (pa, ra), (pb, rb) in zip(knuckles, knuckles[1:]):
        lay["capsules"].append((pa, pb, ra * 1.2, rb * 1.2))
    lay["blobs"].append((at(0.050, -0.001, -0.001), Vector((0.050, 0.040, 0.0125)) * k, 3.0, axes))
    lay["blobs"].append((at(0.036, -0.022, -0.006), Vector((0.030, 0.017, 0.012)) * k, 2.0, axes))
    lay["blobs"].append((at(0.047, 0.027, -0.006), Vector((0.034, 0.012, 0.010)) * k, 2.0, axes))
    lay["blobs"].append((at(-0.016, 0.0, 0.0), Vector((0.030, 0.026, 0.019)) * k, 2.2, axes))
    return lay


def hand_object(side, lay):
    """Closed, smooth hand mesh: primitives fused by a fine voxel remesh."""
    import bmesh
    A, B, C = lay["axes"]
    bm = bmesh.new()
    for centre, half, p, (e1, e2, e3) in lay["blobs"]:
        res = bmesh.ops.create_icosphere(bm, subdivisions=4, radius=1.0)
        for v in res["verts"]:
            d = v.co.copy()
            t = (abs(d.x) ** p + abs(d.y) ** p + abs(d.z) ** p) ** (-1.0 / p)
            v.co = centre + e1 * (d.x * t * half.x) + e2 * (d.y * t * half.y) + e3 * (d.z * t * half.z)
    for p0, p1, r0, r1 in lay["capsules"]:
        axis = p1 - p0
        a = axis.normalized()
        q = Vector((0, 0, 1)).rotation_difference(a)
        M = Matrix.Translation((p0 + p1) / 2) @ q.to_matrix().to_4x4()
        made = bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=16, radius1=r0, radius2=r1,
                                     depth=axis.length, matrix=M)["verts"]
        for c, r in ((p0, r0), (p1, r1)):
            made += bmesh.ops.create_icosphere(bm, subdivisions=2, radius=r, matrix=Matrix.Translation(c))["verts"]
        # Fingers are a little flatter than round.
        dor = (C - C.dot(a) * a)
        if dor.length > 0.3:
            dor.normalize()
            for v in made:
                if v.is_valid:            # create_cone reports helper verts it has already merged away
                    rel = v.co - p0
                    v.co = v.co - dor * (rel.dot(dor) * 0.10)
    me = bpy.data.meshes.new(f"Hand_{side}")
    bm.to_mesh(me)
    bm.free()
    ob = link(bpy.data.objects.new(f"Hand_{side}", me))
    rem = ob.modifiers.new("Remesh", "REMESH")
    rem.mode = "VOXEL"
    rem.voxel_size = 0.0011
    sm = ob.modifiers.new("Smooth", "SMOOTH")
    sm.factor = 0.5
    sm.iterations = 4
    apply_modifiers(ob)
    dec = ob.modifiers.new("Decimate", "DECIMATE")
    dec.decimate_type = "COLLAPSE"
    dec.ratio = min(1.0, HAND_FACES / max(2 * len(ob.data.polygons), 1))
    apply_modifiers(ob)
    attr = ob.data.attributes.new("skin", "FLOAT", "POINT")
    attr.data.foreach_set("value", np.ones(len(ob.data.vertices), np.float32))
    vg = ob.vertex_groups.new(name=f"handmesh_{side}")
    vg.add(list(range(len(ob.data.vertices))), 1.0, "REPLACE")
    log(f"hand {side}: {len(ob.data.polygons)} faces")
    return ob


def bled_px(v):
    if not hasattr(v, "_bled"):
        v._bled = bleed(v.px, v.geo_mask & raw_figure(v.px))
    return v._bled


def sample_photos(views, heads, co, no):
    """The projection shader in numpy, for points with normals (sRGB colours)."""
    def look(v, h, z):
        cols = np.clip(np.round(v.centre + v.sign * h / v.scale).astype(int), 0, v.w - 1)
        rows = np.clip(np.round(v.bottom + 0.5 - z / v.scale - 0.5).astype(int), 0, v.h - 1)
        return bled_px(v)[rows, cols]

    x, y, z = co[:, 0], co[:, 1], co[:, 2]
    nx, ny, nz = no[:, 0], no[:, 1], no[:, 2]
    head = np.clip((z - Z_HEAD) / 0.015, 0, 1)[:, None]
    c_front = look(views["front"], x, z) * (1 - head) + look(heads["front"], x, z) * head
    c_back = look(views["back"], x, z)
    side_head = look(heads["side"], y, z)
    c_left = look(views["left"], y, z) * (1 - head) + side_head * head
    c_right = look(views["right"], y, z) * (1 - head) + side_head * head
    h = head[:, 0]
    gate = (1 - h) + h * np.clip((ny + 0.65) / 0.3, 0, 1)
    flat = nz * nz * 0.03 + 1e-4
    w = np.stack([np.maximum(-ny, 0) ** 3 + flat, np.maximum(ny, 0) ** 3 + flat,
                  np.maximum(nx, 0) ** 3 * gate, np.maximum(-nx, 0) ** 3 * gate], 1)
    w /= w.sum(axis=1, keepdims=True)
    return (c_front * w[:, :1] + c_back * w[:, 1:2] + c_left * w[:, 2:3] + c_right * w[:, 3:4])


# ---------------------------------------------------------------------------------------------
# Back and top of the head: the back photo is small and soft there, and no photo looks at the top. Strands are
# taken from a hair-only patch of the close-up profile and splatted over the scalp: overlapping round patches
# centred on a jittered 3-D grid, each turned so its strands run towards the bun and wind around it there. Only the
# detail comes from the patch (log ratio to its local average); the colour at the scale of a centimetre stays the
# photos'. The bun gets spiral coils in relief, the same field drives the strands' relief and texture.
# ---------------------------------------------------------------------------------------------

HAIR_SRC = (75, 300, 290, 640)    # rows, cols of a hair-only patch in the profile half of the close-up
HAIR_CELL = 0.016                 # splat spacing and radius on the scalp
HAIR_BASE_CELL = 0.008            # the photos' colour is kept down to this scale
HAIR_GAIN = 1.0                   # contrast of the strands relative to the patch
# Bun (back photo + profile): centre (u from the face centre, z) on the back of the head, radius, spiral coils
# (radial pitch per turn, number of coils, relief height).
# The knot itself is an ellipsoid (half widths rx, ry, rz) whose back reaches the profile; beside it the back of
# the head is taken in by up to `depth`, so the bun stands out instead of the whole back bulging.
BUN = dict(u=0.0, z=1.57, radius=0.05, pitch=0.036, arms=2, ridge=0.0065,
           rx=0.055, ry=0.060, rz=0.065, depth=0.035)


def blur(img, sigma):
    """Separable Gaussian blur of an (H, W[, C]) array, edges extended."""
    r = max(1, int(3 * sigma))
    k = np.exp(-0.5 * (np.arange(-r, r + 1) / sigma) ** 2)
    k /= k.sum()
    out = img.astype(np.float64)
    for axis in (0, 1):
        pad = [(0, 0)] * img.ndim
        pad[axis] = (r, r)
        p = np.pad(out, pad, mode="edge")
        n = out.shape[axis]
        acc = np.zeros_like(out)
        for i, kv in enumerate(k):
            acc += kv * np.take(p, np.arange(i, i + n), axis=axis)
        out = acc
    return out


def hash01(i, j, k, salt):
    """Deterministic pseudo-random number in [0, 1) per integer cell (numpy arrays of int64)."""
    h = (i * 73856093) ^ (j * 19349663) ^ (k * 83492791) ^ (salt * 2654435761)
    h &= 0xFFFFFFFF
    for _ in range(2):
        h = ((h ^ (h >> 16)) * 0x45D9F3B) & 0xFFFFFFFF
    h ^= h >> 16
    return (h & 0xFFFFFF).astype(np.float64) / 16777216.0


def bilinear(img, rows, cols):
    H, W = img.shape[:2]
    r = np.clip(rows, 0, H - 1.001)
    c = np.clip(cols, 0, W - 1.001)
    r0, c0 = r.astype(np.int64), c.astype(np.int64)
    fr, fc = (r - r0)[:, None], (c - c0)[:, None]
    return (img[r0, c0] * (1 - fr) * (1 - fc) + img[r0 + 1, c0] * fr * (1 - fc)
            + img[r0, c0 + 1] * (1 - fr) * fc + img[r0 + 1, c0 + 1] * fr * fc)


def unit(v):
    return v / np.maximum(np.linalg.norm(v, axis=1, keepdims=True), 1e-9)


OFFSETS_27 = [(a, b, c) for a in (-1, 0, 1) for b in (-1, 0, 1) for c in (-1, 0, 1)]


class HairDetail:
    def __init__(self, head_side, face_cx):
        r0, r1, c0, c1 = HAIR_SRC
        src = np.clip(head_side.px[r0:r1, c0:c1].astype(np.float64), 0.01, 1.0)
        self.scale = head_side.scale
        self.H, self.W = src.shape[:2]
        # Strand detail: log ratio to the local average (about 6 mm), zero mean.
        ratio = np.log(src / blur(src, 0.006 / self.scale))
        self.ratio = ratio - ratio.reshape(-1, 3).mean(axis=0)
        # Coarser strands for the relief (the mesh has a vertex every ~3.5 mm): band 3.5-10 mm, unit variance.
        lum = src @ np.array([0.3, 0.59, 0.11])
        band = np.log(blur(lum, 0.0035 / self.scale) / blur(lum, 0.010 / self.scale))
        self.band = ((band - band.mean()) / band.std())[:, :, None]
        # Main strand direction of the patch (structure tensor: strands run across the brightness gradient).
        gy, gx = np.gradient(blur(lum, 2.0))
        jxx, jyy, jxy = (gx * gx).mean(), (gy * gy).mean(), (gx * gy).mean()
        self.theta = 0.5 * math.atan2(2 * jxy, jxx - jyy) + 0.5 * math.pi
        self.cx = face_cx
        self.bun = None
        log(f"hair patch {self.W}x{self.H} px ({self.W * self.scale * 100:.1f} x {self.H * self.scale * 100:.1f} cm), "
            f"strands at {math.degrees(self.theta):.0f} deg, detail sd {self.ratio.std():.2f}")

    def set_bun(self, co, no):
        """Bun centre on the hull: the rearmost surface point at the bun's height and lateral position."""
        near = ((np.abs(co[:, 0] - (self.cx + BUN["u"])) < 0.012) & (np.abs(co[:, 2] - BUN["z"]) < 0.012)
                & (co[:, 1] > 0.0))
        idx = np.nonzero(near)[0]
        i = idx[np.argmax(co[idx, 1])]
        self.bun = co[i].astype(np.float64)
        cap = idx[co[idx, 1] > co[i, 1] - 0.012]
        self.bn = unit(no[cap].astype(np.float64).mean(axis=0)[None])[0]
        ex = np.array([1.0, 0.0, 0.0])
        self.ex = ex - ex.dot(self.bn) * self.bn
        self.ex /= np.linalg.norm(self.ex)
        self.ez = np.cross(self.bn, self.ex)
        log(f"bun centre ({self.bun[0]:.3f}, {self.bun[1]:.3f}, {self.bun[2]:.3f}), normal "
            f"({self.bn[0]:.2f}, {self.bn[1]:.2f}, {self.bn[2]:.2f})")

    @staticmethod
    def coverage(n):
        """How badly the photos cover a surface direction: 1 facing back or up, 0 where the close-up looks."""
        return np.maximum(smoothstep(0.40, 0.75, n[:, 1]), smoothstep(0.50, 0.85, n[:, 2]))

    def _bun_polar(self, p):
        q = p - self.bun
        dn = q @ self.bn
        qb = q - dn[:, None] * self.bn
        r = np.linalg.norm(qb, axis=1)
        phi = np.arctan2(qb @ self.ez, qb @ self.ex)
        return q, dn, qb, r, phi

    def _coil_phase(self, p, r, phi):
        wobble = (0.55 * np.sin(p @ np.array([41.0, 17.0, 29.0])) + 0.35 * np.sin(p @ np.array([-23.0, 37.0, 53.0])))
        return BUN["arms"] * phi - 2 * math.pi * r / BUN["pitch"] + wobble

    def coils(self, p):
        """Spiral coils on the bun in [-1, 1] (ridge +1, gap -1), zero away from it."""
        _, dn, _, r, phi = self._bun_polar(p)
        R = BUN["radius"]
        mask = (1 - smoothstep(0.75 * R, 1.15 * R, r)) * smoothstep(0.003, 0.012, r) * (dn > -0.05)
        c = np.cos(self._coil_phase(p, r, phi))
        return (2.0 * (0.5 + 0.5 * c) ** 0.7 - 1.0) * mask

    def frame(self, p, n):
        """Strand direction per point: towards the bun, along the coils on the bun."""
        q, _, qb, r, phi = self._bun_polar(p)
        inflow = -q - (-q * n).sum(axis=1, keepdims=True) * n
        er = qb / np.maximum(r, 1e-6)[:, None]
        ephi = np.cross(self.bn, er)
        along = er + (2 * math.pi * r / (BUN["pitch"] * BUN["arms"]))[:, None] * ephi
        k = (1 - smoothstep(0.7 * BUN["radius"], 1.2 * BUN["radius"], r))[:, None]
        t = unit(inflow) * (1 - k) + unit(along) * k
        t = t - (t * n).sum(axis=1, keepdims=True) * n
        bad = np.linalg.norm(t, axis=1) < 1e-4
        t[bad] = np.cross(n[bad], [1.0, 0.0, 0.0])
        t = unit(t)
        return t, np.cross(n, t)

    def detail(self, p, n, img):
        """Splatted patch values at points p (normals n): variance-preserving blend of the overlapping splats."""
        p = p.astype(np.float64)
        n = unit(n.astype(np.float64))
        h = HAIR_CELL
        t1, t2 = self.frame(p, n)
        cell = np.floor(p / h).astype(np.int64)
        acc = np.zeros((len(p), img.shape[2]))
        w2 = np.zeros(len(p))
        margin = h / self.scale + 1
        jit = math.radians(25.0)
        for di, dj, dk in OFFSETS_27:
            ci, cj, ck = cell[:, 0] + di, cell[:, 1] + dj, cell[:, 2] + dk
            s = (np.stack([ci, cj, ck], 1) + np.stack([hash01(ci, cj, ck, k) for k in (1, 2, 3)], 1)) * h
            d = p - s
            w = np.clip(1.0 - (d * d).sum(axis=1) / (h * h), 0.0, None) ** 2
            m = w > 0
            if not m.any():
                continue
            ci, cj, ck, d, w = ci[m], cj[m], ck[m], d[m], w[m]
            a = (d * t1[m]).sum(axis=1) * np.where(hash01(ci, cj, ck, 4) < 0.5, -1.0, 1.0)
            b = (d * t2[m]).sum(axis=1)
            ang = self.theta + (hash01(ci, cj, ck, 5) - 0.5) * 2 * jit
            ca, sa = np.cos(ang), np.sin(ang)
            oc = margin + hash01(ci, cj, ck, 6) * (self.W - 2 * margin)
            orow = margin + hash01(ci, cj, ck, 7) * (self.H - 2 * margin)
            v = bilinear(img, orow + (a * sa + b * ca) / self.scale, oc + (a * ca - b * sa) / self.scale)
            acc[m] += w[:, None] * v
            w2[m] += w * w
        return acc / np.sqrt(np.maximum(w2, 1e-12))[:, None]

    @staticmethod
    def smooth(p, vals, h):
        """Partition-of-unity average of vals over a jittered grid of cell h (a low-pass of about h)."""
        p = p.astype(np.float64)
        cell = np.floor(p / h).astype(np.int64)
        lo = cell.min(axis=0) - 1
        dims = cell.max(axis=0) - lo + 2
        n_cells = int(np.prod(dims))
        S = np.zeros((n_cells, vals.shape[1]))
        Wc = np.zeros(n_cells)
        parts = []
        for di, dj, dk in OFFSETS_27:
            c = cell + np.array([di, dj, dk])
            s = (c + np.stack([hash01(c[:, 0], c[:, 1], c[:, 2], k) for k in (11, 12, 13)], 1)) * h
            w = np.clip(1.0 - ((p - s) ** 2).sum(axis=1) / (h * h), 0.0, None) ** 2
            idx = ((c[:, 0] - lo[0]) * dims[1] + (c[:, 1] - lo[1])) * dims[2] + (c[:, 2] - lo[2])
            Wc += np.bincount(idx, w, n_cells)
            for ch in range(vals.shape[1]):
                S[:, ch] += np.bincount(idx, w * vals[:, ch], n_cells)
            parts.append((idx, w))
        mean = S / np.maximum(Wc, 1e-12)[:, None]
        out = np.zeros_like(vals, dtype=np.float64)
        wsum = np.zeros(len(p))
        for idx, w in parts:
            out += w[:, None] * mean[idx]
            wsum += w
        return out / np.maximum(wsum, 1e-12)[:, None]


def paint_hair(tex, ob, hd):
    """Strand detail and coil shading on the back and top of the head, painted into the baked texture."""
    size = tex.size[0]
    px = np.empty(size * size * 4, np.float32)
    tex.pixels.foreach_get(px)
    px = px.reshape(size, size, 4)
    X, Y, P, Nn = texels_of(ob, "scalp", size)
    w = hd.coverage(Nn) * smoothstep(1.47, 1.50, P[:, 2])
    sel = w > 0.003
    X, Y, P, Nn, w = X[sel], Y[sel], P[sel].astype(np.float64), Nn[sel].astype(np.float64), w[sel]
    col = px[Y, X, :3].astype(np.float64)
    # Keep clear of skin (nape, hairline): fraction of skin-coloured texels around each point.
    skin = (is_skin(col) & (col[:, 1] > 0.62 * col[:, 0]) & (col[:, 0] > 0.45)).astype(np.float64)
    skin_frac = hd.smooth(P, skin[:, None], 0.006)[:, 0]
    w *= 1.0 - smoothstep(0.2, 0.5, skin_frac)
    base = hd.smooth(P, col, HAIR_BASE_CELL)
    L = hd.detail(P, Nn, hd.ratio)
    shade = 1.0 + 0.25 * hd.coils(P)
    new = np.clip(base * np.exp(HAIR_GAIN * L) * shade[:, None], 0.0, 1.0)
    col = col * (1 - w[:, None]) + new * w[:, None]
    px[Y, X, :3] = col
    tex.pixels.foreach_set(px.ravel())
    log(f"hair detail painted on {int((w > 0.5).sum())} of {len(X)} scalp texels")


def hair_relief(ob, views, heads, hull, hd):
    """Strands in relief: push hair vertices in where the photos show dark gaps between strands and out on the
    light strands (high-pass of the projected brightness), plus curly lumps. Where the photos are poor (back,
    top) the splatted strands of the close-up decide instead, and the bun gets spiral coils."""
    from mathutils import noise
    me = ob.data
    n = len(me.vertices)
    co = np.empty(n * 3, np.float32)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    no = np.empty(n * 3, np.float32)
    me.vertex_normals.foreach_get("vector", no)
    no = no.reshape(-1, 3)
    face = hull.face
    y_side = np.interp(co[:, 2], face.zt, face.b + face.D)
    Wz = np.interp(co[:, 2], face.zt, face.W)
    in_face = (face.in_zone(co[:, 2]) & (co[:, 1] < y_side + 0.004) & (np.abs(co[:, 0] - face.cx) < Wz + 0.004))
    rgb = sample_photos(views, heads, co, no)
    # Copper strands pass the general skin test; real skin is lighter and less saturated.
    skin = is_skin(rgb) & (rgb[:, 1] > 0.62 * rgb[:, 0]) & (rgb[:, 0] > 0.45)
    hair = (co[:, 2] > 1.475) & ~in_face & ~skin
    weight = smoothstep(1.475, 1.495, co[:, 2]) * hair
    lum = rgb @ np.array([0.3, 0.59, 0.11])
    # Neighbour average via the edges.
    ev = np.empty(len(me.edges) * 2, np.int32)
    me.edges.foreach_get("vertices", ev)
    ev = ev.reshape(-1, 2)
    deg = np.bincount(ev.ravel(), minlength=n).astype(np.float32)
    low = lum.copy()
    for _ in range(10):
        acc = np.zeros(n, np.float32)
        np.add.at(acc, ev[:, 0], low[ev[:, 1]])
        np.add.at(acc, ev[:, 1], low[ev[:, 0]])
        low = 0.5 * low + 0.5 * acc / np.maximum(deg, 1)
    d = np.clip((lum - low) * 0.035, -0.0028, 0.0028)
    lumps = np.array([noise.noise(Vector(p) * 55.0) for p in co[hair]]) if hair.any() else np.zeros(0)
    d_l = np.zeros(n, np.float32)
    d_l[hair] = lumps
    d = d + d_l * 0.0018
    hd.set_bun(co, no)
    if hair.any():
        hp, hn = co[hair], no[hair]
        strands = hd.detail(hp, hn, hd.band)[:, 0]
        d_new = np.clip(strands * 0.0009, -0.0022, 0.0022) + hd.coils(hp) * BUN["ridge"]
        cov = hd.coverage(hn)
        d[hair] = d[hair] * (1 - cov) + d_new * cov
    co += no * (d * weight)[:, None]
    me.vertices.foreach_set("co", co.ravel())
    me.update()
    log(f"hair relief on {int(hair.sum())} of {n} vertices")


# Loose curls: (u from the face centre, y, z) near the scalp, direction, length, tube radius, helix radius, turns,
# droop. Framing the face at the temples, two wisps on the forehead, behind the ears and at the nape, loops on the bun.
CURLS = (
    (-0.069, -0.022, 1.600, (-0.15, -0.10, -1.0), 0.100, 0.0042, 0.0085, 2.2, 0.1),
    (-0.073, -0.004, 1.578, (-0.25, 0.05, -1.0), 0.112, 0.0038, 0.0090, 2.5, 0.1),
    (0.069, -0.024, 1.602, (0.15, -0.10, -1.0), 0.092, 0.0042, 0.0085, 2.1, 0.1),
    (0.074, -0.006, 1.580, (0.25, 0.05, -1.0), 0.105, 0.0038, 0.0090, 2.4, 0.1),
    (-0.030, -0.080, 1.655, (-0.6, -0.3, -0.5), 0.042, 0.0030, 0.0060, 1.1, 0.3),
    (0.028, -0.082, 1.660, (0.55, -0.35, -0.45), 0.038, 0.0030, 0.0060, 1.0, 0.3),
    (-0.060, 0.065, 1.530, (-0.15, 0.2, -1.0), 0.080, 0.0040, 0.0080, 1.9, 0.0),
    (0.060, 0.065, 1.530, (0.15, 0.2, -1.0), 0.076, 0.0040, 0.0080, 1.8, 0.0),
    (-0.025, 0.095, 1.515, (-0.05, 0.25, -1.0), 0.062, 0.0038, 0.0075, 1.5, 0.0),
    (0.030, 0.095, 1.512, (0.05, 0.25, -1.0), 0.066, 0.0038, 0.0075, 1.6, 0.0),
    (-0.055, 0.145, 1.625, (-0.8, 0.3, -0.5), 0.040, 0.0048, 0.0095, 0.9, 0.5),
    (0.050, 0.150, 1.615, (0.8, 0.4, -0.5), 0.040, 0.0048, 0.0095, 0.9, 0.5),
    (-0.020, 0.170, 1.590, (-0.3, 0.7, -0.7), 0.045, 0.0045, 0.0090, 1.1, 0.4),
    (0.025, 0.160, 1.570, (0.4, 0.5, -0.8), 0.050, 0.0042, 0.0085, 1.3, 0.3),
)


def curl_object(body, face_cx):
    """Corkscrew tubes rooted in the scalp (snapped to the head surface)."""
    import bmesh
    from mathutils.bvhtree import BVHTree
    dg = bpy.context.evaluated_depsgraph_get()
    tree = BVHTree.FromObject(body, dg)
    bm = bmesh.new()
    ring = 7
    for u, y, z, dvec, length, radius, helix_r, turns, droop in CURLS:
        hit, normal, _, _ = tree.find_nearest(Vector((face_cx + u, y, z)))
        if hit is None:
            continue
        root = hit - normal * 0.004
        d = (Vector(dvec).normalized() * 0.8 + normal * 0.2).normalized()
        e1 = d.orthogonal().normalized()
        e2 = d.cross(e1)
        steps = max(24, int(turns * 16))
        centres = []
        for i in range(steps + 1):
            t = i / steps
            base = root + d * length * t + Vector((0, 0, -1)) * droop * length * t * t
            ang = 2 * math.pi * turns * t
            centres.append(base + (e1 * math.cos(ang) + e2 * math.sin(ang)) * helix_r * (0.35 + 0.65 * t))
        rings = []
        ref = e1
        for i, c in enumerate(centres):
            tan = (centres[min(i + 1, steps)] - centres[max(i - 1, 0)]).normalized()
            uvec = (ref - ref.dot(tan) * tan).normalized()
            vvec = tan.cross(uvec)
            ref = uvec
            r = radius * (1.0 - 0.72 * i / steps)
            rings.append([bm.verts.new(c + (uvec * math.cos(2 * math.pi * j / ring) + vvec * math.sin(2 * math.pi * j / ring)) * r)
                          for j in range(ring)])
        for a_, b_ in zip(rings, rings[1:]):
            for j in range(ring):
                bm.faces.new((a_[j], a_[(j + 1) % ring], b_[(j + 1) % ring], b_[j]))
        tip = bm.verts.new(centres[-1] + (centres[-1] - centres[-2]).normalized() * radius * 0.5)
        for j in range(ring):
            bm.faces.new((rings[-1][j], rings[-1][(j + 1) % ring], tip))
        start = bm.verts.new(centres[0])
        for j in range(ring):
            bm.faces.new((rings[0][(j + 1) % ring], rings[0][j], start))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new("Curls")
    bm.to_mesh(me)
    bm.free()
    ob = link(bpy.data.objects.new("Curls", me))
    for name, value in (("skin", 0.0), ("curl", 1.0)):
        a = me.attributes.new(name, "FLOAT", "POINT")
        a.data.foreach_set("value", np.full(len(me.vertices), value, np.float32))
    vg = ob.vertex_groups.new(name="rigid_head")
    vg.add(list(range(len(me.vertices))), 1.0, "REPLACE")
    log(f"curls: {len(CURLS)} strands, {len(me.polygons)} faces")
    return ob


def texels_of(ob, attr_name, size):
    """Every texel covered by triangles whose vertices carry attr_name > 0.5: pixel (x, y) from the bottom, rest
    position and normal (interpolated)."""
    me = ob.data
    me.calc_loop_triangles()
    tris = me.loop_triangles
    n = len(tris)
    vidx = np.empty(n * 3, np.int32)
    tris.foreach_get("vertices", vidx)
    lidx = np.empty(n * 3, np.int32)
    tris.foreach_get("loops", lidx)
    vidx, lidx = vidx.reshape(-1, 3), lidx.reshape(-1, 3)
    uv = np.empty(len(me.loops) * 2, np.float32)
    me.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2) * size
    co = np.empty(len(me.vertices) * 3, np.float32)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    no = np.empty(len(me.vertices) * 3, np.float32)
    me.vertex_normals.foreach_get("vector", no)
    no = no.reshape(-1, 3)
    val = np.empty(len(me.vertices), np.float32)
    me.attributes[attr_name].data.foreach_get("value", val)
    sel = np.nonzero(val[vidx].min(axis=1) > 0.5)[0]
    xs, ys, ps, ns = [], [], [], []
    for t in sel:
        q = uv[lidx[t]]
        x0, y0 = np.floor(q.min(axis=0)).astype(int)
        x1, y1 = np.ceil(q.max(axis=0)).astype(int)
        gx, gy = np.meshgrid(np.arange(x0, x1) + 0.5, np.arange(y0, y1) + 0.5)
        gx, gy = gx.ravel(), gy.ravel()
        (ax, ay), (bx, by), (cx, cy) = q
        den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
        if abs(den) < 1e-12:
            continue
        w0 = ((by - cy) * (gx - cx) + (cx - bx) * (gy - cy)) / den
        w1 = ((cy - ay) * (gx - cx) + (ax - cx) * (gy - cy)) / den
        w2 = 1.0 - w0 - w1
        inside = (w0 >= -1e-4) & (w1 >= -1e-4) & (w2 >= -1e-4)
        if not inside.any():
            continue
        W = np.stack([w0[inside], w1[inside], w2[inside]], 1)
        xs.append(np.floor(gx[inside]).astype(int))
        ys.append(np.floor(gy[inside]).astype(int))
        ps.append(W @ co[vidx[t]])
        ns.append(W @ no[vidx[t]])
    n_ = np.concatenate(ns)
    return (np.concatenate(xs), np.concatenate(ys), np.concatenate(ps),
            n_ / np.maximum(np.linalg.norm(n_, axis=1, keepdims=True), 1e-9))


def smoothstep(e0, e1, v):
    t = np.clip((v - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def seg_dist2d(px, py, pts):
    """Distance of points (px, py) to a polyline [(x, y), ...]."""
    d = np.full(px.shape, 1e9)
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        vx, vy = x1 - x0, y1 - y0
        t = np.clip(((px - x0) * vx + (py - y0) * vy) / (vx * vx + vy * vy), 0, 1)
        d = np.minimum(d, np.hypot(px - (x0 + t * vx), py - (y0 + t * vy)))
    return d


# Palm creases in hand coordinates (along the hand, across towards the little finger), metres for a 17.5 cm hand.
PALM_LINES = (
    [(0.074, 0.036), (0.078, 0.020), (0.080, 0.004), (0.083, -0.012)],                   # heart line
    [(0.072, -0.031), (0.066, -0.012), (0.061, 0.008), (0.057, 0.024)],                  # head line
    [(0.074, -0.030), (0.058, -0.024), (0.040, -0.020), (0.022, -0.016), (0.010, -0.010)],  # around the thumb ball
    [(0.004, -0.022), (0.003, 0.0), (0.004, 0.022)],                                     # wrist
)


def paint_hand_details(tex, ob, J):
    """Nails (with half-moon, cuticle and pale tip), wrinkles over the finger joints, creases on the palm side,
    redder knuckles and finger pads. Painted straight into the baked texture."""
    size = tex.size[0]
    px = np.empty(size * size * 4, np.float32)
    tex.pixels.foreach_get(px)
    px = px.reshape(size, size, 4)
    X, Y, P, Nn = texels_of(ob, "skin", size)
    col = px[Y, X, :3].copy()
    for side, s in SIDES.items():
        lay = J[f"hand_layout_{side}"]
        A, B, C = lay["axes"]
        k = lay["k"]
        m = (P[:, 0] - lay["wrist"].x) * s > -0.06
        if not m.any():
            continue
        p, n = P[m], Nn[m]
        c = col[m]
        shade = np.ones(len(p))
        tint = np.zeros((len(p), 3))
        for d in DIGITS:
            dor = np.array(lay["dorsal"][d])
            joints = [lay["joints"][f"{d}_0{i}_{side}"] for i in (1, 2, 3)]
            for i, (h, t_) in enumerate(joints):
                h, t_ = np.array(h), np.array(t_)
                a = t_ - h
                L = np.linalg.norm(a)
                a = a / L
                rel = p - h
                along = rel @ a
                tt = along / L
                radial = rel - along[:, None] * a
                rlen = np.linalg.norm(radial, axis=1)
                cosang = (radial @ dor) / np.maximum(rlen, 1e-9)
                near = (tt > -0.25) & (tt < 1.25) & (rlen < lay["radii"][d][i] * 1.6)
                if i == 2:
                    # Nail plate: dorsal part of the last segment.
                    nail = near * smoothstep(0.26, 0.32, tt) * (tt < 1.04) * smoothstep(0.50, 0.68, cosang)
                    lunula = nail * (1 - smoothstep(0.32, 0.42, tt))
                    tip = nail * smoothstep(0.86, 0.95, tt)
                    cuticle = near * smoothstep(0.18, 0.24, tt) * (1 - smoothstep(0.26, 0.30, tt)) * smoothstep(0.5, 0.7, cosang)
                    tint += nail[:, None] * np.array([0.07, 0.02, 0.03])
                    shade *= 1 + 0.08 * nail + 0.08 * lunula + 0.12 * tip - 0.18 * cuticle
                    pad = near * smoothstep(0.45, 0.9, tt) * smoothstep(-0.2, -0.6, cosang)
                    tint += pad[:, None] * np.array([0.05, -0.005, -0.01])
                # Creases at the start of the segment: thin lines over the joint on the back, one deep line in front.
                if i > 0 or d == "thumb":
                    dist = np.abs(along)
                    back = near * smoothstep(0.1, 0.4, cosang) * np.exp(-(dist / 0.0028) ** 2)
                    lines = 0.5 + 0.5 * np.cos(dist / 0.0009 * math.pi)
                    front = near * smoothstep(-0.2, -0.5, cosang) * np.exp(-(dist / 0.0009) ** 2)
                    shade *= 1 - 0.16 * back * lines - 0.28 * front
                    tint += (back * 0.6)[:, None] * np.array([0.03, 0.0, -0.005])
                if i == 0:
                    knuckle = np.exp(-(np.linalg.norm(rel - dor * lay["radii"][d][0], axis=1) / 0.007) ** 2)
                    tint += knuckle[:, None] * np.array([0.045, 0.0, -0.01])
                    front = near * smoothstep(-0.2, -0.5, cosang) * np.exp(-((along + 0.002) / 0.0010) ** 2)
                    shade *= 1 - 0.22 * front
        # Palm creases (palm faces down in the rest pose).
        wr = np.array(lay["wrist"])
        la = ((p - wr) @ np.array(A)) / k
        lb = ((p - wr) @ np.array(B)) / k
        palm = smoothstep(0.3, 0.6, -(n @ np.array(C)))
        for line in PALM_LINES:
            dist = seg_dist2d(la, lb, line)
            shade *= 1 - 0.26 * palm * np.exp(-(dist / 0.0010) ** 2)
        tint += palm[:, None] * np.array([0.025, 0.0, -0.005])
        c = np.clip(c * shade[:, None] + tint, 0.0, 1.0)
        col[m] = c
    px[Y, X, :3] = col
    tex.pixels.foreach_set(px.ravel())
    log(f"hand details painted on {len(X)} texels")


def body_heat_weights(body, arm):
    """Bone heat on a copy that holds only the body hull (hands, ears and curls make the solver fail for the
    whole mesh), copied back by vertex index. Parents the body to the armature."""
    import bmesh
    dup = link(bpy.data.objects.new("BodyOnly", body.data.copy()))
    orig = dup.data.attributes.new("orig", "INT", "POINT")
    orig.data.foreach_set("value", np.arange(len(dup.data.vertices), dtype=np.int32))
    skip = {body.vertex_groups[n].index for n in body.vertex_groups.keys()
            if n.startswith("handmesh_") or n.startswith("rigid_")}
    bm = bmesh.new()
    bm.from_mesh(dup.data)
    deform = bm.verts.layers.deform.active
    gone = [v for v in bm.verts if deform is not None and any(g in skip and w > 0.5 for g, w in v[deform].items())]
    bmesh.ops.delete(bm, geom=gone, context="VERTS")
    if deform is not None:
        for v in bm.verts:
            v[deform].clear()
    bm.to_mesh(dup.data)
    bm.free()
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    dup.select_set(True)
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.parent_set(type="ARMATURE_AUTO")
    # Parent the real body (empty bone groups), then copy the weights over.
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    body.select_set(True)
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.parent_set(type="ARMATURE_NAME")
    ids = np.empty(len(dup.data.vertices), np.int32)
    dup.data.attributes["orig"].data.foreach_get("value", ids)
    names = {g.index: g.name for g in dup.vertex_groups}
    per_group = {}
    for v in dup.data.vertices:
        for e in v.groups:
            if e.weight > 0.0:
                per_group.setdefault((names[e.group], round(e.weight, 3)), []).append(int(ids[v.index]))
    for (name, w), idx in per_group.items():
        g = body.vertex_groups.get(name) or body.vertex_groups.new(name=name)
        g.add(idx, w, "REPLACE")
    missing = sum(1 for v in dup.data.vertices if not v.groups)
    log(f"bone heat on {len(dup.data.vertices)} body vertices ({missing} without weight)")
    me = dup.data
    bpy.data.objects.remove(dup)
    bpy.data.meshes.remove(me)


def rigid_weights(body, marker, bone):
    """Vertices in the marker group (ears, curls) follow one bone only."""
    g = body.vertex_groups.get(marker)
    if g is None:
        return
    idx = [v.index for v in body.data.vertices if any(e.group == g.index and e.weight > 0.5 for e in v.groups)]
    for other in body.vertex_groups:
        if other.name not in (marker, bone):
            other.remove(idx)
    target = body.vertex_groups.get(bone) or body.vertex_groups.new(name=bone)
    target.add(idx, 1.0, "REPLACE")
    body.vertex_groups.remove(g)
    log(f"{marker}: {len(idx)} vertices on {bone}")


def hand_weights(body, arm):
    """Skin the modelled hands by distance to the hand, finger and forearm bones (bone heat does badly on the
    separate hand islands). A bone counts less for points behind its start, so the palm follows the hand."""
    bones = arm.data.bones
    for side in SIDES:
        marker = body.vertex_groups.get(f"handmesh_{side}")
        if marker is None:
            continue
        mi = marker.index
        idx = [v.index for v in body.data.vertices if any(g.group == mi and g.weight > 0.5 for g in v.groups)]
        names = [f"lowerarm_{side}", f"hand_{side}"] + [f"{d}_0{i}_{side}" for d in DIGITS for i in (1, 2, 3)]
        heads = np.array([bones[n].head_local for n in names])
        tails = np.array([bones[n].tail_local for n in names])
        pts = np.array([body.data.vertices[i].co for i in idx])
        seg = tails - heads
        L2 = (seg ** 2).sum(axis=1)
        t_raw = ((pts[:, None, :] - heads[None]) * seg[None]).sum(axis=2) / L2[None]
        t = np.clip(t_raw, 0.0, 1.0)
        closest = heads[None] + t[:, :, None] * seg[None]
        d = np.linalg.norm(pts[:, None, :] - closest, axis=2)
        behind = np.maximum(0.0, -t_raw) * np.sqrt(L2)[None]
        behind[:, :2] = 0.0                       # forearm and hand: no penalty
        d = d + 1.5 * behind
        w = 1.0 / (d + 0.002) ** 5
        top = np.argsort(-w, axis=1)[:, :2]
        for gi, g in enumerate(body.vertex_groups):
            if g.name in names or g.name.startswith("handmesh_"):
                continue
            g.remove(idx)
        groups = {n: body.vertex_groups.get(n) or body.vertex_groups.new(name=n) for n in names}
        for row, vi in enumerate(idx):
            ws = w[row, top[row]]
            ws = ws / ws.sum()
            for n in names:
                groups[n].remove([vi])
            for j, wt in zip(top[row], ws):
                groups[names[j]].add([vi], float(wt), "REPLACE")
        body.vertex_groups.remove(marker)
        log(f"hand weights {side}: {len(idx)} vertices")


def measure(hull):
    """Joint positions from the silhouettes (metres, Blender axes)."""
    xs, ys, zs, F, S = hull.xs, hull.ys, hull.zs, hull.F, hull.S

    def iz(z):
        return int(np.argmin(np.abs(zs - z)))

    def y_centre(z):
        lo, hi = max(row_runs(S[iz(z)], ys), key=lambda r: r[1] - r[0])
        return 0.5 * (lo + hi)

    def side_run(z):
        return max(row_runs(S[iz(z)], ys), key=lambda r: r[1] - r[0])

    def leg_centre(z, s):
        runs_ = [r for r in row_runs(F[iz(z)], xs) if not (r[0] < 0 < r[1]) and s * (r[0] + r[1]) > 0]
        if not runs_:
            return s * 0.09
        lo, hi = max(runs_, key=lambda r: r[1] - r[0])
        return 0.5 * (lo + hi)

    x0 = int(np.argmin(np.abs(xs)))
    col = F[:, x0]
    k = iz(1.0)
    while k > 0 and col[k]:
        k -= 1
    z_crotch = zs[k + 1]
    z_hip = z_crotch + 0.075
    z_ankle = 0.085
    z_neck = hull.z_arm_hi + 0.035
    z_head = 1.505
    J = {"root": Vector((0, 0, 0))}
    J["pelvis"] = Vector((0, y_centre(z_hip), z_hip))
    spine_lo = z_hip + 0.09
    for i, name in enumerate(("spine_01", "spine_02", "spine_03")):
        z = spine_lo + i * (z_neck - spine_lo) / 3
        J[name] = Vector((0, y_centre(z) + 0.01, z))
    J["neck_01"] = Vector((0, y_centre(z_neck) + 0.015, z_neck))
    J["head"] = Vector((0, y_centre(z_head) + 0.01, z_head))
    J["head_end"] = Vector((0, y_centre(z_head) + 0.01, HEIGHT - 0.02))
    toe, heel = side_run(0.03)
    for side, s in SIDES.items():
        x_s = hull.xs_r if s > 0 else hull.xs_l
        hand_x = xs[hull.hand_col & (s * xs > 0)]
        wrist_x = float(hand_x.min() if s > 0 else hand_x.max())
        end_x = float(hand_x.max() if s > 0 else hand_x.min())
        sh_x = x_s - s * 0.032

        def arm_z(x):
            ix = int(np.argmin(np.abs(xs - x)))
            rows = hull.A_full[:, ix]
            if not rows.any():
                return (hull.z_arm_lo + hull.z_arm_hi) / 2
            zr = zs[rows]
            return float(0.5 * (zr.min() + zr.max()))

        sh_z = arm_z(x_s + s * 0.04) - 0.01
        el_x = sh_x + 0.56 * (wrist_x - sh_x)
        J[f"clavicle_{side}"] = Vector((s * 0.03, J["spine_03"].y - 0.01, z_neck - 0.035))
        J[f"upperarm_{side}"] = Vector((sh_x, hull.arm_y, sh_z))
        J[f"lowerarm_{side}"] = Vector((el_x, hull.arm_y + 0.008, arm_z(el_x)))
        J[f"hand_{side}"] = Vector((wrist_x, hull.arm_y, arm_z(wrist_x)))
        lay = hand_layout(side, J[f"hand_{side}"], abs(end_x - wrist_x))
        J[f"hand_layout_{side}"] = lay
        J[f"hand_end_{side}"] = lay["joints"][f"hand_{side}"][1]
        hip_x = leg_centre(z_crotch - 0.08, s) * 0.9
        ankle_x = leg_centre(0.16, s)
        z_knee = z_ankle + 0.485 * (z_hip - z_ankle)
        J[f"thigh_{side}"] = Vector((hip_x, J["pelvis"].y, z_hip))
        J[f"calf_{side}"] = Vector((leg_centre(z_knee, s), y_centre(z_knee) - 0.012, z_knee))
        J[f"foot_{side}"] = Vector((ankle_x, heel - 0.075, z_ankle))
        J[f"ball_{side}"] = Vector((ankle_x * 1.04, toe + 0.075, 0.03))
        J[f"toe_{side}"] = Vector((ankle_x * 1.05, toe + 0.015, 0.03))
    for name, p in J.items():
        if isinstance(p, Vector):
            log(f"joint {name:12s} {p.x:+.3f} {p.y:+.3f} {p.z:+.3f}")
    return J


def bone_table(J):
    """(name, parent, head, tail, roll towards, connected)."""
    fwd, up = Vector((0, -1, 0)), Vector((0, 0, 1))
    t = [
        ("root", None, J["root"], J["root"] + Vector((0, 0.0, 0.12)), fwd, False),
        ("pelvis", "root", J["pelvis"], J["spine_01"], fwd, False),
        ("spine_01", "pelvis", J["spine_01"], J["spine_02"], fwd, True),
        ("spine_02", "spine_01", J["spine_02"], J["spine_03"], fwd, True),
        ("spine_03", "spine_02", J["spine_03"], J["neck_01"], fwd, True),
        ("neck_01", "spine_03", J["neck_01"], J["head"], fwd, True),
        ("head", "neck_01", J["head"], J["head_end"], fwd, True),
    ]
    for side in SIDES:
        t += [
            (f"clavicle_{side}", "spine_03", J[f"clavicle_{side}"], J[f"upperarm_{side}"], up, False),
            (f"upperarm_{side}", f"clavicle_{side}", J[f"upperarm_{side}"], J[f"lowerarm_{side}"], up, True),
            (f"lowerarm_{side}", f"upperarm_{side}", J[f"lowerarm_{side}"], J[f"hand_{side}"], up, True),
            (f"hand_{side}", f"lowerarm_{side}", J[f"hand_{side}"], J[f"hand_end_{side}"], up, True),
        ]
        joints = J[f"hand_layout_{side}"]["joints"]
        for d in DIGITS:
            for i in (1, 2, 3):
                name = f"{d}_0{i}_{side}"
                parent = f"hand_{side}" if i == 1 else f"{d}_0{i - 1}_{side}"
                head, tail = joints[name]
                t.append((name, parent, head, tail, up, i > 1))
        t += [
            (f"thigh_{side}", "pelvis", J[f"thigh_{side}"], J[f"calf_{side}"], fwd, False),
            (f"calf_{side}", f"thigh_{side}", J[f"calf_{side}"], J[f"foot_{side}"], fwd, True),
            (f"foot_{side}", f"calf_{side}", J[f"foot_{side}"], J[f"ball_{side}"], up, True),
            (f"ball_{side}", f"foot_{side}", J[f"ball_{side}"], J[f"toe_{side}"], up, True),
        ]
    return t


def stage_rig():
    t0 = time.time()
    open_blend("mesh")
    views = {n: View(n) for n in ("front", "left")}
    hull = Hull(views["front"], views["left"])
    J = measure(hull)
    body = bpy.data.objects["Rosa"]
    arm_data = bpy.data.armatures.new("Armature")
    arm = link(bpy.data.objects.new("Armature", arm_data))
    activate(arm)
    bpy.ops.object.mode_set(mode="EDIT")
    for name, parent, head, tail, roll_to, connect in bone_table(J):
        eb = arm_data.edit_bones.new(name)
        eb.head, eb.tail = head, tail
        eb.align_roll(roll_to)
        if parent:
            eb.parent = arm_data.edit_bones[parent]
            eb.use_connect = connect
        eb.use_deform = name != "root"
    bpy.ops.object.mode_set(mode="OBJECT")
    arm_data.display_type = "STICK"
    body_heat_weights(body, arm)
    hand_weights(body, arm)
    rigid_weights(body, "rigid_head", "head")
    groups = {g.index: g.name for g in body.vertex_groups}
    unweighted = sum(1 for v in body.data.vertices if not any(g.weight > 0.001 for g in v.groups))
    log(f"weights: {len(groups)} groups, {unweighted} of {len(body.data.vertices)} vertices without weight")
    save_blend("rig")
    log(f"stage rig done in {time.time() - t0:.1f} s")


# ---------------------------------------------------------------------------------------------
# Posing: rotations are given as deltas in armature space ("turn the thigh about X by 30 degrees"),
# legs are solved with a two-bone IK so feet can stay planted.
# ---------------------------------------------------------------------------------------------


def rot(axis, deg):
    return Quaternion(Vector(axis), math.radians(deg))


def frame_quat(x, d):
    d = d.normalized()
    x = (x - x.dot(d) * d).normalized()
    z = x.cross(d)
    return Matrix((x, d, z)).transposed().to_quaternion()


class Poser:
    def __init__(self, arm):
        self.arm = arm
        bones = arm.data.bones
        self.rest = {b.name: b.matrix_local.to_quaternion() for b in bones}
        self.head = {b.name: b.head_local.copy() for b in bones}
        self.tail = {b.name: b.tail_local.copy() for b in bones}
        self.parent = {b.name: (b.parent.name if b.parent else None) for b in bones}

        def depth(n):
            return 0 if self.parent[n] is None else 1 + depth(self.parent[n])

        self.order = sorted(self.parent, key=depth)
        self.last = {}
        # Finger joints bend towards the palm (palm down in the rest pose); the thumb across it.
        self.flex = {}
        palm = Vector((0, 0, -1))
        across = Vector((0, 0.7, -0.7)).normalized()
        for n in self.parent:
            if any(n.startswith(d + "_0") for d in DIGITS):
                d = (self.tail[n] - self.head[n]).normalized()
                self.flex[n] = d.cross(across if n.startswith("thumb") else palm).normalized()
        for pb in arm.pose.bones:
            pb.rotation_mode = "QUATERNION"
        ank = self.head["foot_l"]
        self.heel_back = 0.075          # heel contact behind the ankle
        self.ankle_z = ank.z

    def two_bone(self, a, b, c, root, target, pole, rest_pole):
        """IK for bones a (upper) and b (lower) so the head of c lands on `target`; the middle joint bends
        towards `pole`. rest_pole is where it points in the rest pose (keeps the twist sensible)."""
        L1 = (self.head[b] - self.head[a]).length
        L2 = (self.head[c] - self.head[b]).length
        to = target - root
        dist = max(min(to.length, (L1 + L2) * 0.9995), abs(L1 - L2) + 1e-3)
        u = to.normalized()
        Pp = (pole - pole.dot(u) * u).normalized()
        cos_a = max(-1.0, min(1.0, (L1 * L1 + dist * dist - L2 * L2) / (2 * L1 * dist)))
        ang = math.acos(cos_a)
        mid = root + L1 * (math.cos(ang) * u + math.sin(ang) * Pp)
        end = root + u * dist
        x1 = pole.cross(u).normalized()
        u0 = (self.head[c] - self.head[a]).normalized()
        x0 = rest_pole.cross(u0).normalized()
        Da = frame_quat(x1, mid - root) @ frame_quat(x0, self.head[b] - self.head[a]).inverted()
        Db = frame_quat(x1, end - mid) @ frame_quat(x0, self.head[c] - self.head[b]).inverted()
        return Da, Db

    def leg(self, side, D_pelvis, pelvis_offset, ankle, foot_rot, ball_rot=None, pole=None):
        """Leg IK: deltas for thigh, calf, foot, ball so the ankle reaches `ankle`."""
        th, ca, ft, bl = (f"{n}_{side}" for n in ("thigh", "calf", "foot", "ball"))
        hip = self.head["pelvis"] + pelvis_offset + D_pelvis @ (self.head[th] - self.head["pelvis"])
        P = pole if pole is not None else D_pelvis @ Vector((0, -1, 0))
        D = {}
        D[th], D[ca] = self.two_bone(th, ca, ft, hip, ankle, P, Vector((0, -1, 0)))
        D[ft] = foot_rot
        D[bl] = ball_rot if ball_rot is not None else foot_rot
        return D

    def resolve(self, D):
        full = {}
        for name in self.order:
            p = self.parent[name]
            full[name] = D.get(name, full[p] if p else Quaternion())
        return full

    def pos(self, full, offset, name):
        """Posed head position of a bone."""
        chain_ = []
        n = name
        while n:
            chain_.append(n)
            n = self.parent[n]
        chain_.reverse()
        P, prev = None, None
        for n in chain_:
            if prev is None:
                P = self.head[n].copy()
            else:
                P = P + full[prev] @ (self.head[n] - self.head[prev])
                if n == "pelvis":
                    P = P + offset
            prev = n
        return P

    def arm_ik(self, D, offset, side, target, pole, wrist_bend=0.0):
        """Replace the arm deltas in D so the wrist reaches `target`."""
        full = self.resolve(D)
        ua, la, ha = f"upperarm_{side}", f"lowerarm_{side}", f"hand_{side}"
        shoulder = self.pos(full, offset, ua)
        D[ua], D[la] = self.two_bone(ua, la, ha, shoulder, target, pole, Vector((0, 1, 0)))
        s = SIDES[side]
        D[ha] = D[la] @ rot((0, 0, 1), -s * wrist_bend)

    def apply(self, D, pelvis_offset=Vector()):
        full = {}
        for name in self.order:
            p = self.parent[name]
            Dp = full[p] if p else Quaternion()
            Db = D.get(name, Dp)
            full[name] = Db
            R = self.rest[name]
            q = R.inverted() @ Dp.inverted() @ Db @ R
            if name in self.last:
                q.make_compatible(self.last[name])   # no sign flips between keys
            self.last[name] = q.copy()
            self.arm.pose.bones[name].rotation_quaternion = q
        pb = self.arm.pose.bones["pelvis"]
        pb.location = self.rest["pelvis"].inverted() @ pelvis_offset
        return full

    def fingers(self, D, side, curl):
        """curl: digit -> (base, middle, tip) degrees, or one tuple for all four fingers (thumb via 'thumb')."""
        hand = self.resolve(D)[f"hand_{side}"]
        for d in DIGITS:
            angles = curl.get(d, curl.get("all", (0, 0, 0))) if isinstance(curl, dict) else curl
            acc = hand
            for i in (1, 2, 3):
                n = f"{d}_0{i}_{side}"
                acc = acc @ Quaternion(self.flex[n], math.radians(angles[i - 1]))
                D[n] = acc

    def foot(self, side, shift, lift, yaw, pitch, toe_bend=0.0):
        """Ankle target and foot/ball rotation for a foot moved by `shift` (xy), raised by `lift` and rolled by
        `pitch` degrees: > 0 heel up (rolls over the ball, toes stay flat), < 0 toes up (rolls over the heel)."""
        ank0 = self.head[f"foot_{side}"]
        ball0 = self.head[f"ball_{side}"]
        heel = Vector((ank0.x, ank0.y + self.heel_back, 0.0))
        ball = Vector((ball0.x, ball0.y, 0.0))
        R = rot((1, 0, 0), pitch)
        pivot = ball if pitch >= 0 else heel
        ank = pivot + R @ (ank0 - pivot)
        centre = Vector((ank0.x, ank0.y, 0.0))
        Ry = rot((0, 0, 1), yaw)
        ank = centre + Ry @ (ank - centre) + Vector((shift[0], shift[1], lift))
        D_foot = Ry @ R
        D_ball = Ry @ rot((1, 0, 0), toe_bend) if pitch >= 0 else D_foot
        return ank, D_foot, D_ball

    def key(self, frame):
        for pb in self.arm.pose.bones:
            pb.keyframe_insert("rotation_quaternion", frame=frame)
        self.arm.pose.bones["pelvis"].keyframe_insert("location", frame=frame)


def chain(D, names, parent_delta, local_rots):
    """FK down a chain: each entry is a delta relative to the previous bone (armature axes)."""
    acc = parent_delta
    for n, q in zip(names, local_rots):
        acc = acc @ q
        D[n] = acc
    return acc


def preview_rig():
    open_blend("rig")
    arm = bpy.data.objects["Armature"]
    P = Poser(arm)
    D = {}
    off = Vector((0, 0.02, -0.06))
    Dp = rot((0, 0, 1), 8)
    D["pelvis"] = Dp
    sp = chain(D, ["spine_01", "spine_02", "spine_03"], Dp, [rot((1, 0, 0), 6)] * 3)
    chain(D, ["neck_01", "head"], sp, [rot((1, 0, 0), -5), rot((0, 0, 1), -25)])
    for side, s in SIDES.items():
        cl = chain(D, [f"clavicle_{side}"], sp, [Quaternion()])
        chain(D, [f"upperarm_{side}", f"lowerarm_{side}", f"hand_{side}"], cl,
              [rot((1, 0, 0), -25 if s > 0 else 20) @ rot((0, 1, 0), s * 72), rot((0, 0, 1), -s * 35), Quaternion()])
        ank = P.head[f"foot_{side}"] + (Vector((0, -0.25, 0.25)) if s > 0 else Vector())
        D.update(P.leg(side, Dp, off, ank, rot((1, 0, 0), -20) if s > 0 else Quaternion()))
    P.apply(D, off)
    paths = render_views("rig", {k: PREVIEW_VIEWS[k] for k in ("front", "three_quarter", "side", "back")})
    contact_sheet(paths, os.path.join(WORK, "sheet_rig.png"), len(paths))


# ---------------------------------------------------------------------------------------------
# Stage: anim. Everything in place (the game moves the capsule), 30 fps, loops end on their first pose.
# ---------------------------------------------------------------------------------------------

FPS = 30
ARM_DOWN = 79.0   # degrees from the T-pose so the hands clear the jacket


def smooth(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


def lerp(a, b, t):
    return a + (b - a) * t


def new_action(arm, name, frames):
    act = bpy.data.actions.new(name)
    act.use_fake_user = True
    if arm.animation_data is None:
        arm.animation_data_create()
    arm.animation_data.action = act
    act.use_frame_range = True
    act.frame_start, act.frame_end = 0, frames
    return act


def upper_body(P, D, Dp, spine_pitch, spine_yaw, head_pitch, head_yaw, arms):
    """Spine, head and arms. spine_* are totals spread over three bones (relative to the pelvis),
    head_* absolute; arms: side -> (down, swing, elbow, twist) degrees."""
    per = rot((0, 0, 1), spine_yaw / 3) @ rot((1, 0, 0), spine_pitch / 3)
    sp = chain(D, ["spine_01", "spine_02", "spine_03"], Dp, [per] * 3)
    # Head: absolute target, split between neck and head.
    target = rot((0, 0, 1), head_yaw) @ rot((1, 0, 0), head_pitch)
    rel = sp.inverted() @ target
    half = Quaternion().slerp(rel, 0.4)
    chain(D, ["neck_01", "head"], sp, [half, half.inverted() @ rel])
    for side, s in SIDES.items():
        down, swing, elbow, twist = arms[side]
        cl = chain(D, [f"clavicle_{side}"], sp, [Quaternion()])
        chain(D, [f"upperarm_{side}", f"lowerarm_{side}", f"hand_{side}"], cl,
              [rot((1, 0, 0), swing) @ rot((0, 1, 0), s * down),
               rot((0, 0, 1), -s * elbow) @ rot((1, 0, 0), s * twist),
               rot((0, 0, 1), -s * 6)])


def act_locomotion(P, arm, name, frames, speed, duty, lift, bob, lean, swing, elbow, jog):
    new_action(arm, name, frames)
    T = frames / FPS
    stance = speed * T * duty
    for f in range(frames + 1):
        ph = f / frames
        Dp_yaw = -(5.0 if not jog else 7.0) * math.cos(2 * math.pi * ph)
        Dp_roll = -(3.0 if not jog else 2.0) * math.sin(2 * math.pi * ph)
        Dp = rot((0, 0, 1), Dp_yaw) @ rot((0, 1, 0), Dp_roll) @ rot((1, 0, 0), lean * 0.4)
        if jog:
            peak = (duty + 0.5) / 2
            dz = -0.07 + bob * math.cos(4 * math.pi * (ph - peak))
        else:
            dz = -0.035 + bob * math.cos(4 * math.pi * (ph - duty / 2))
        off = Vector((0.012 * math.sin(2 * math.pi * ph), 0.0, dz))
        D = {"pelvis": Dp}
        for side, s in SIDES.items():
            lp = (ph + (0.0 if s > 0 else 0.5)) % 1.0
            narrow = -s * 0.025
            if lp < duty:
                t = lp / duty
                y = lerp(-stance * 0.46, stance * 0.54, t)
                strike = 14.0 if not jog else 6.0
                pitch = -strike * (1 - smooth(t / 0.18)) + (32.0 if not jog else 28.0) * smooth((t - 0.6) / 0.4)
                z = 0.0
            else:
                w = (lp - duty) / (1 - duty)
                y = lerp(stance * 0.54, -stance * 0.46, smooth(w))
                z = lift * math.sin(math.pi * min(1.0, w * 1.08)) ** 1.3
                toeoff = 32.0 if not jog else 28.0
                strike = 14.0 if not jog else 6.0
                pitch = lerp(toeoff, -strike, smooth(w * 1.15))
            ank, Df, Db = P.foot(side, (narrow, y), z, 0.0, pitch, toe_bend=0.0)
            D.update(P.leg(side, Dp, off, ank, Df, Db))
        arms = {}
        for side, s in SIDES.items():
            sw = s * swing * math.cos(2 * math.pi * ph)
            fwd = max(0.0, -sw) / max(swing, 1e-3)
            arms[side] = (ARM_DOWN - (10 if jog else 0), sw - (4 if jog else 3), elbow + (12 if not jog else 8) * fwd,
                          -12.0 if jog else 0.0)
        upper_body(P, D, Dp, spine_pitch=lean * 0.6, spine_yaw=-1.6 * Dp_yaw, head_pitch=3.0 if jog else 1.0,
                   head_yaw=0.0, arms=arms)
        for side in SIDES:
            P.fingers(D, side, LOOSE_FIST if jog else RELAXED)
        P.apply(D, off)
        P.key(f)


def act_idle(P, arm, frames=90):
    new_action(arm, "Rosa_Idle", frames)
    for f in range(frames + 1):
        ph = f / frames
        breath = math.sin(2 * math.pi * ph)
        sway = math.sin(2 * math.pi * ph + 0.6)
        Dp = rot((0, 0, 1), 2.0 * sway) @ rot((0, 1, 0), 1.5 * sway)
        off = Vector((0.012 * sway, 0.0, -0.014 + 0.003 * breath))
        D = {"pelvis": Dp}
        for side in SIDES:
            ank, Df, Db = P.foot(side, (0.0, 0.0), 0.0, 0.0, 0.0)
            D.update(P.leg(side, Dp, off, ank, Df, Db))
        arms = {side: (ARM_DOWN + 0.8 * breath, -3.0 + 1.0 * breath, 12.0 + 2.0 * breath, 0.0) for side in SIDES}
        upper_body(P, D, Dp, spine_pitch=-1.2 * breath, spine_yaw=-2.5 * sway, head_pitch=1.5 * math.sin(4 * math.pi * ph),
                   head_yaw=5.0 * math.sin(2 * math.pi * ph + 1.0), arms=arms)
        for side, s in SIDES.items():
            D[f"clavicle_{side}"] = D["spine_03"] @ rot((0, 1, 0), -s * 1.2 * breath)
            P.fingers(D, side, mix_hand(RELAXED, LOOSE_FIST, 0.08 + 0.06 * math.sin(2 * math.pi * ph + s)))
        P.apply(D, off)
        P.key(f)


REST_ARMS = {side: (ARM_DOWN, -5.0, 12.0, 0.0) for side in SIDES}

# Finger poses: digit -> (base, middle, tip) joint angles in degrees.
RELAXED = {"index": (8, 14, 8), "middle": (11, 17, 9), "ring": (14, 20, 10), "pinky": (17, 22, 12), "thumb": (6, 10, 8)}
LOOSE_FIST = {"index": (45, 65, 35), "middle": (50, 70, 38), "ring": (55, 72, 38), "pinky": (58, 72, 36),
              "thumb": (18, 30, 25)}
GRIP = {"index": (22, 28, 16), "middle": (38, 52, 28), "ring": (50, 64, 34), "pinky": (55, 68, 36),
        "thumb": (16, 26, 20)}
OPEN = {"index": (2, 3, 2), "middle": (2, 3, 2), "ring": (3, 4, 2), "pinky": (4, 5, 3), "thumb": (0, 3, 2)}
GRAB = {"index": (40, 55, 35), "middle": (45, 60, 38), "ring": (48, 62, 38), "pinky": (50, 62, 36),
        "thumb": (22, 30, 28)}


def mix_hand(a, b, t):
    return {d: tuple(x + (y - x) * t for x, y in zip(a[d], b[d])) for d in a}


def act_work(P, arm, frames=60):
    """Crouched in front of a lock or a drawer, both hands busy at about 0.95 m (loop)."""
    new_action(arm, "Rosa_Work", frames)
    for f in range(frames + 1):
        ph = f / frames
        Dp = rot((0, 0, 1), -6) @ rot((1, 0, 0), 6)
        off = Vector((0.0, 0.05, -0.24 + 0.004 * math.sin(2 * math.pi * ph)))
        D = {"pelvis": Dp}
        for side, s in SIDES.items():
            shift = (s * 0.03, -0.09 if s > 0 else 0.11)
            ank, Df, Db = P.foot(side, shift, 0.0, -10 * s, 0.0 if s > 0 else 14.0)
            D.update(P.leg(side, Dp, off, ank, Df, Db))
        upper_body(P, D, Dp, spine_pitch=22, spine_yaw=4, head_pitch=24 + 2 * math.sin(2 * math.pi * ph),
                   head_yaw=-4, arms=REST_ARMS)
        for side, s in SIDES.items():
            k = 1.0 if s > 0 else 1.7
            jig = Vector((0.006 * math.sin(2 * math.pi * 3 * ph + k), 0.0, 0.008 * math.sin(2 * math.pi * 4 * ph + 2 * k)))
            target = Vector((s * 0.10 - 0.005, -0.40 if s > 0 else -0.42, 0.98 if s > 0 else 0.93)) + jig
            P.arm_ik(D, off, side, target, Vector((s * 0.8, 0.35, -0.6)).normalized(),
                     wrist_bend=10 + 6 * math.sin(2 * math.pi * 4 * ph + k))
            # Fingers work the pick: index and thumb twitch, the rest holds.
            tw = math.sin(2 * math.pi * 5 * ph + 2 * k)
            pose = dict(GRIP)
            pose["index"] = tuple(a + 8 * tw for a in GRIP["index"])
            pose["thumb"] = tuple(a - 5 * tw for a in GRIP["thumb"])
            P.fingers(D, side, pose)
        P.apply(D, off)
        P.key(f)


def act_pickup(P, arm, frames=48):
    """Crouch, pick something up from the floor in front with the right hand, stand up (one shot)."""
    new_action(arm, "Rosa_Pickup", frames)
    for f in range(frames + 1):
        t = f / FPS
        c = smooth(t / 0.55) if t < 0.85 else 1.0 - smooth((t - 0.85) / 0.75)
        Dp = rot((1, 0, 0), 14 * c)
        off = Vector((0.0, 0.10 * c, -0.36 * c))
        D = {"pelvis": Dp}
        for side, s in SIDES.items():
            shift = (0.0, (-0.10 if s > 0 else 0.12) * c)
            ank, Df, Db = P.foot(side, shift, 0.0, 0.0, (0.0 if s > 0 else 24.0) * c)
            D.update(P.leg(side, Dp, off, ank, Df, Db))
        upper_body(P, D, Dp, spine_pitch=34 * c, spine_yaw=-8 * c, head_pitch=18 * c, head_yaw=-6 * c,
                   arms=REST_ARMS)
        full = P.resolve(D)
        # Blend each wrist from where the hanging arm would be to its target.
        for side, target, pole, amount in (
                ("r", Vector((-0.10, -0.40, 0.12)), Vector((-0.6, 0.6, 0.1)), smooth((t - 0.05) / 0.5)),
                ("l", P.pos(full, off, "calf_l") + Vector((0.02, -0.05, 0.07)), Vector((0.8, 0.4, -0.2)), c)):
            amount = min(amount, c) if side == "r" else amount
            fk_wrist = P.pos(full, off, f"hand_{side}")
            fk_elbow = P.pos(full, off, f"lowerarm_{side}")
            shoulder = P.pos(full, off, f"upperarm_{side}")
            fk_pole = (fk_elbow - 0.5 * (shoulder + fk_wrist)).normalized()
            P.arm_ik(D, off, side, fk_wrist.lerp(target, amount), fk_pole.lerp(pole.normalized(), amount).normalized(),
                     wrist_bend=20 * amount)
        # Right hand opens on the way down, closes on the object at the bottom and keeps it.
        grab = smooth((t - 0.5) / 0.25)
        P.fingers(D, "r", mix_hand(mix_hand(RELAXED, OPEN, smooth(t / 0.4)), GRAB, grab))
        P.fingers(D, "l", mix_hand(RELAXED, GRIP, 0.5 * c))
        P.apply(D, off)
        P.key(f)


ACTIONS = ("Rosa_Idle", "Rosa_Walk", "Rosa_Jog", "Rosa_Work", "Rosa_Pickup")


def stage_anim():
    t0 = time.time()
    open_blend("rig")
    arm = bpy.data.objects["Armature"]
    P = Poser(arm)
    act_idle(P, arm)
    # Walk 1.4 m/s (one cycle = two steps of 0.7 m), jog 2.5 m/s = the game's base walk speed.
    act_locomotion(P, arm, "Rosa_Walk", 30, speed=1.4, duty=0.6, lift=0.09, bob=0.016, lean=4.0,
                   swing=22.0, elbow=14.0, jog=False)
    act_locomotion(P, arm, "Rosa_Jog", 22, speed=2.5, duty=0.38, lift=0.17, bob=0.025, lean=10.0,
                   swing=24.0, elbow=88.0, jog=True)
    act_work(P, arm)
    act_pickup(P, arm)
    arm.animation_data.action = bpy.data.actions["Rosa_Idle"]
    for a in bpy.data.actions:
        log(f"action {a.name}: frames {a.frame_range[0]:.0f}-{a.frame_range[1]:.0f}")
    save_blend("anim")
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, "Rosa", "rosa.blend"), compress=True, copy=True)
    log(f"stage anim done in {time.time() - t0:.1f} s")


def set_action(arm, name):
    act = bpy.data.actions[name]
    arm.animation_data.action = act
    scene = bpy.context.scene
    scene.frame_start, scene.frame_end = int(act.frame_range[0]), int(act.frame_range[1])
    return act


def preview_anim():
    open_blend("anim")
    arm = bpy.data.objects["Armature"]
    paths = []
    for name in ACTIONS:
        act = set_action(arm, name)
        a, b = int(act.frame_range[0]), int(act.frame_range[1])
        frames = [int(round(a + (b - a) * i / 6)) for i in range(6)]
        paths += render_views(f"anim_{name}", {"side": (1.0, -0.15, 0.1)}, size=(300, 440), frames=frames)
    contact_sheet(paths, os.path.join(WORK, "sheet_anim_side.png"), 6)
    paths = []
    for name in ACTIONS:
        act = set_action(arm, name)
        a, b = int(act.frame_range[0]), int(act.frame_range[1])
        frames = [int(round(a + (b - a) * i / 6)) for i in range(6)]
        paths += render_views(f"anim_{name}", {"tq": (0.7, -1.0, 0.3)}, size=(300, 440), frames=frames)
    contact_sheet(paths, os.path.join(WORK, "sheet_anim_tq.png"), 6)
    # Close-ups of the hands: idle, jog, work, grabbing.
    scene = bpy.context.scene
    paths = []
    for name, frame, side, view in (("Rosa_Idle", 20, "r", (-0.3, -1.0, 0.3)), ("Rosa_Jog", 5, "l", (0.6, -1.0, 0.4)),
                                    ("Rosa_Work", 15, "r", (-0.2, -1.0, 0.6)), ("Rosa_Work", 15, "l", (0.6, -0.6, 0.9)),
                                    ("Rosa_Pickup", 26, "r", (-0.8, -0.8, 0.5))):
        set_action(arm, name)
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        pb = arm.pose.bones[f"hand_{side}"]
        c = arm.matrix_world @ ((pb.head + pb.tail) / 2)
        paths += render_views(f"fingers_{name}_{side}", {"v": view}, size=(320, 320), centre=tuple(c), radius=0.11)
    contact_sheet(paths, os.path.join(WORK, "sheet_fingers.png"), len(paths))


SHOWCASE = (("Rosa_Idle", 1, "Stehen"), ("Rosa_Walk", 3, "Gehen 1,4 m/s"), ("Rosa_Jog", 4, "Laufen 2,5 m/s"),
            ("Rosa_Work", 2, "Arbeiten (Schloss, Schublade)"), ("Rosa_Pickup", 1, "Aufheben"))


def preview_showcase():
    """MP4 of all actions one after another, plus a still at game-camera distance."""
    open_blend("anim")
    scene = bpy.context.scene
    arm = bpy.data.objects["Armature"]
    arm.animation_data.action = None
    track = arm.animation_data.nla_tracks.new()
    start = 0
    labels = []
    for name, repeat, label in SHOWCASE:
        act = bpy.data.actions[name]
        strip = track.strips.new(name, start, act)
        strip.repeat = repeat
        length = int(round((act.frame_range[1] - act.frame_range[0]) * repeat))
        labels.append((label, start, start + length))
        start += length + 1
    scene.frame_start, scene.frame_end = 0, start - 1
    cam = setup_preview_scene()
    for label, a, b in labels:
        cu = bpy.data.curves.new(label, "FONT")
        cu.body = label
        cu.size = 0.11
        cu.align_x = "CENTER"
        tm = bpy.data.materials.new("Label")
        tm.use_nodes = True
        tb = node_of_type(tm.node_tree.nodes, "BSDF_PRINCIPLED")
        tb.inputs["Base Color"].default_value = (0.95, 0.8, 0.45, 1)
        cu.materials.append(tm)
        ob = link(bpy.data.objects.new(label, cu))
        ob.location = (0.0, 0.35, 1.92)
        ob.rotation_euler = (math.radians(90), 0, math.radians(-30))
        for f, hidden in ((a - 1, True), (a, False), (b, False), (b + 1, True)):
            ob.hide_render = hidden
            ob.keyframe_insert("hide_render", frame=max(f, 0))
    centre = Vector((0, 0, 0.95))
    direction = Vector((0.55, -1.0, 0.22)).normalized()
    cam.data.lens = 50
    cam.location = centre + direction * 4.6
    cam.rotation_euler = (centre - cam.location).to_track_quat("-Z", "Y").to_euler()
    scene.render.resolution_x, scene.render.resolution_y = 720, 900
    scene.eevee.taa_render_samples = 16
    try:
        scene.render.image_settings.media_type = "VIDEO"
    except (AttributeError, TypeError):
        pass
    scene.render.image_settings.file_format = "FFMPEG"
    scene.render.ffmpeg.format = "MPEG4"
    scene.render.ffmpeg.codec = "H264"
    scene.render.ffmpeg.constant_rate_factor = "HIGH"
    scene.render.filepath = os.path.join(WORK, "rosa_showcase.mp4")
    t0 = time.time()
    bpy.ops.render.render(animation=True)
    log(f"video {scene.render.filepath} ({scene.frame_end + 1} frames, {time.time() - t0:.0f} s)")
    # Game distance: steep camera from ~9 m, a few frames of walk and work.
    try:
        scene.render.image_settings.media_type = "IMAGE"
    except (AttributeError, TypeError):
        pass
    scene.render.image_settings.file_format = "PNG"
    for ob in bpy.data.objects:
        if ob.type == "FONT":
            ob.animation_data_clear()
            ob.hide_render = True
    cam.data.lens = 35
    centre = Vector((0, 0, 0.8))
    cam.location = centre + Vector((0.25, -0.6, 1.1)).normalized() * 9.0
    cam.rotation_euler = (centre - cam.location).to_track_quat("-Z", "Y").to_euler()
    scene.render.resolution_x, scene.render.resolution_y = 480, 360
    paths = []
    for label, a, b in labels:
        scene.frame_set(int(a + (b - a) * 0.3))
        path = os.path.join(WORK, f"game_{label.split()[0]}.png")
        scene.render.filepath = path
        bpy.ops.render.render(write_still=True)
        paths.append(path)
    contact_sheet(paths, os.path.join(WORK, "sheet_game.png"), len(paths))


def check_fbx():
    """Read the exported FBX back into an empty scene and report what arrived."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=os.path.join(EXPORT, "SK_Rosa.fbx"))
    for ob in bpy.data.objects:
        if ob.type == "ARMATURE":
            log(f"fbx armature {ob.name}: {len(ob.data.bones)} bones, root {[b.name for b in ob.data.bones if not b.parent]}")
        elif ob.type == "MESH":
            d = ob.dimensions
            log(f"fbx mesh {ob.name}: {len(ob.data.vertices)} verts, {len(ob.data.polygons)} faces, "
                f"{d.x:.2f} x {d.y:.2f} x {d.z:.2f} m, groups {len(ob.vertex_groups)}, "
                f"materials {[m.name for m in ob.data.materials]}")
    for a in bpy.data.actions:
        log(f"fbx action {a.name}: {a.frame_range[0]:.0f}-{a.frame_range[1]:.0f}")
    fbm = os.path.join(EXPORT, "SK_Rosa.fbm")   # the importer unpacks textures here
    if os.path.isdir(fbm):
        import shutil
        shutil.rmtree(fbm)


# ---------------------------------------------------------------------------------------------
# Export
# ---------------------------------------------------------------------------------------------


def export_fbx():
    open_blend("anim")
    arm = bpy.data.objects["Armature"]
    body = bpy.data.objects["Rosa"]
    arm.animation_data.action = None
    for pb in arm.pose.bones:
        pb.rotation_quaternion = Quaternion()
        pb.location = Vector()
    activate(arm)
    body.select_set(True)
    path = os.path.join(EXPORT, "SK_Rosa.fbx")
    bpy.ops.export_scene.fbx(
        filepath=path, use_selection=True, object_types={"ARMATURE", "MESH"},
        apply_scale_options="FBX_SCALE_ALL", axis_forward="-Z", axis_up="Y",
        mesh_smooth_type="FACE", use_tspace=False, add_leaf_bones=False,
        primary_bone_axis="Y", secondary_bone_axis="X", armature_nodetype="NULL",
        bake_anim=True, bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False,
        bake_anim_force_startend_keying=True, bake_anim_step=1.0, bake_anim_simplify_factor=0.0,
        path_mode="COPY", embed_textures=False)
    log("exported", path, f"{os.path.getsize(path) / 1e6:.1f} MB")


# ---------------------------------------------------------------------------------------------
# Preview renders
# ---------------------------------------------------------------------------------------------

PREVIEW_VIEWS = {
    "front": (0.0, -1.0, 0.08),
    "three_quarter": (0.75, -1.0, 0.35),
    "side": (1.0, 0.0, 0.08),
    "back": (-0.4, 1.0, 0.15),
    "game": (0.25, -0.55, 1.5),
}


def setup_preview_scene():
    scene = bpy.context.scene
    try:
        scene.render.engine = "BLENDER_EEVEE"
    except TypeError as err:
        log(f"engine: {err}")
    for attr, value in (("use_raytracing", True), ("use_shadows", True), ("taa_render_samples", 24)):
        try:
            setattr(scene.eevee, attr, value)
        except (AttributeError, TypeError):
            pass
    try:
        scene.view_settings.view_transform = "AgX"
    except TypeError:
        pass
    if "PreviewWorld" not in bpy.data.worlds:
        world = bpy.data.worlds.new("PreviewWorld")
        world.use_nodes = True
        bg = node_of_type(world.node_tree.nodes, "BACKGROUND")
        bg.inputs["Color"].default_value = (0.20, 0.21, 0.23, 1)
        bg.inputs["Strength"].default_value = 0.6
        key = bpy.data.lights.new("PreviewKey", "AREA")
        key.energy = 600
        key.size = 2.5
        k = link(bpy.data.objects.new("PreviewKey", key))
        k.location = (-2.0, -3.0, 3.5)
        k.rotation_euler = (Vector((0, 0, 0.9)) - k.location).to_track_quat("-Z", "Y").to_euler()
        rim = bpy.data.lights.new("PreviewRim", "AREA")
        rim.energy = 400
        rim.size = 2.0
        r = link(bpy.data.objects.new("PreviewRim", rim))
        r.location = (2.5, 3.0, 2.5)
        r.rotation_euler = (Vector((0, 0, 1.0)) - r.location).to_track_quat("-Z", "Y").to_euler()
        fill = bpy.data.lights.new("PreviewFill", "AREA")
        fill.energy = 250
        fill.size = 3.0
        f = link(bpy.data.objects.new("PreviewFill", fill))
        f.location = (3.0, -2.0, 1.2)
        f.rotation_euler = (Vector((0, 0, 1.0)) - f.location).to_track_quat("-Z", "Y").to_euler()
        fm = bpy.data.materials.new("PreviewFloor")
        fm.use_nodes = True
        fb = node_of_type(fm.node_tree.nodes, "BSDF_PRINCIPLED")
        fb.inputs["Base Color"].default_value = (0.30, 0.29, 0.27, 1)
        fb.inputs["Roughness"].default_value = 0.8
        fme = bpy.data.meshes.new("PreviewFloor")
        fme.from_pydata([(-20, -20, 0), (20, -20, 0), (20, 20, 0), (-20, 20, 0)], [], [(0, 1, 2, 3)])
        fme.materials.append(fm)
        link(bpy.data.objects.new("PreviewFloor", fme))
        cam = bpy.data.cameras.new("PreviewCam")
        cam.lens = 85
        c = link(bpy.data.objects.new("PreviewCam", cam))
        scene.camera = c
    scene.world = bpy.data.worlds["PreviewWorld"]
    return scene.camera


def render_views(prefix, views=PREVIEW_VIEWS, size=(520, 760), centre=(0, 0, 0.88), radius=1.0, frames=None):
    scene = bpy.context.scene
    cam = setup_preview_scene()
    scene.render.resolution_x, scene.render.resolution_y = size
    scene.render.image_settings.file_format = "PNG"
    out = []
    centre = Vector(centre)
    for name, d in views.items():
        direction = Vector(d).normalized()
        dist = radius / math.tan(cam.data.angle / 2) * 1.15
        cam.location = centre + direction * dist
        cam.rotation_euler = (centre - cam.location).to_track_quat("-Z", "Y").to_euler()
        for f in (frames or [scene.frame_current]):
            scene.frame_set(f)
            path = os.path.join(WORK, f"{prefix}_{name}" + (f"_{f:03d}" if frames else "") + ".png")
            scene.render.filepath = path
            bpy.ops.render.render(write_still=True)
            out.append(path)
    return out


def contact_sheet(paths, path, columns):
    imgs = [load_rgb(p) for p in paths]
    h, w = imgs[0].shape[:2]
    rows = (len(imgs) + columns - 1) // columns
    sheet = np.zeros((rows * h, columns * w, 3), np.float32)
    for i, im in enumerate(imgs):
        r, c = divmod(i, columns)
        sheet[r * h:(r + 1) * h, c * w:(c + 1) * w] = im
    save_rgb(sheet, path)
    log("sheet", path)


HEAD_VIEWS = {
    "front": (0.0, -1.0, 0.05),
    "three_quarter": (0.8, -1.0, 0.2),
    "side": (1.0, 0.0, 0.05),
    "back": (-0.5, 1.0, 0.2),
    "back_three_quarter": (0.9, 0.8, 0.35),
    "top": (0.2, -0.45, 1.5),
}


def preview_mesh():
    open_blend("mesh")
    paths = render_views("mesh")
    contact_sheet(paths, os.path.join(WORK, "sheet_mesh.png"), len(paths))
    paths = render_views("head", HEAD_VIEWS, size=(400, 400), centre=(0, 0.01, 1.57), radius=0.17)
    contact_sheet(paths, os.path.join(WORK, "sheet_head.png"), 3)
    ear_views = {"side": (-1.0, 0.05, 0.1), "behind": (-0.75, 0.65, 0.15), "front": (-0.6, -0.8, 0.1),
                 "above": (-0.6, 0.2, 0.8)}
    paths = render_views("ear", ear_views, size=(320, 320), centre=(-0.07, 0.037, 1.537), radius=0.055)
    contact_sheet(paths, os.path.join(WORK, "sheet_ear.png"), len(paths))
    hand_views = {"top": (0.15, -0.3, 1.0), "front": (0.1, -1.0, 0.25), "below": (0.2, -0.5, -1.0),
                  "end": (1.0, -0.4, 0.3)}
    paths = render_views("hand", hand_views, size=(360, 360), centre=(0.74, 0.02, 1.35), radius=0.13)
    contact_sheet(paths, os.path.join(WORK, "sheet_hand.png"), len(paths))


# ---------------------------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------------------------


def main():
    bpy.context.preferences.filepaths.save_version = 0   # no .blend1 backups next to rosa.blend
    a = args()
    stage = a["stage"]
    if stage in ("mesh", "all"):
        stage_mesh()
        if a["preview"]:
            preview_mesh()
    if stage == "preview_mesh":
        preview_mesh()
    if stage in ("rig", "all"):
        stage_rig()
        if a["preview"]:
            preview_rig()
    if stage in ("anim", "all"):
        stage_anim()
        if a["preview"]:
            preview_anim()
    if stage == "showcase":
        preview_showcase()
    if a["export"]:
        export_fbx()
        check_fbx()


if __name__ == "__main__":
    main()
