"""Draws the Villa Lindenhof floor plans from villa_layout.py into one HTML page.

  python Scripts/Levels/villa_plan.py            -> Docs/Villa/Villa-Grundriss.html

The page is written as an Artifact fragment (no <html>/<body> tags); browsers open it as well.
All geometry is in metres; SVG viewBoxes are in metres too, so stroke widths are real wall thicknesses.
"""

import html
import math
import os
import sys
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import villa_layout as V  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(HERE))
OUT = os.path.join(ROOT, "Docs", "Villa", "Villa-Grundriss.html")

EPS = 1e-6
SHY = "­"


def num(v, digits=2):
    """German decimal comma, trailing zeros trimmed to at least one decimal for metres."""
    s = f"{v:.{digits}f}".rstrip("0").rstrip(".")
    return s.replace(".", ",")


def m2(v):
    return f"{v:.1f}".replace(".", ",") if v < 10 else f"{v:.0f}"


def esc(s):
    return html.escape(s, quote=True)


def plain(name):
    return name.replace(SHY, "")


# ---------------------------------------------------------------------------------------------------------
# SVG helper with flipped y axis (plan y points north = up)
# ---------------------------------------------------------------------------------------------------------

class Svg:
    def __init__(self, x0, y0, x1, y1, title):
        self.x0, self.y0, self.x1, self.y1 = x0, y0, x1, y1
        self.title = title
        self.parts = []

    def X(self, x):
        return x - self.x0

    def Y(self, y):
        return self.y1 - y

    def p(self, x, y):
        return f"{self.X(x):.3f},{self.Y(y):.3f}"

    def add(self, s):
        self.parts.append(s)

    def open(self, cls):
        self.add(f'<g class="{cls}">')

    def close(self):
        self.add("</g>")

    def rect(self, x0, y0, x1, y1, cls="", extra=""):
        x0, x1 = min(x0, x1), max(x0, x1)
        y0, y1 = min(y0, y1), max(y0, y1)
        self.add(f'<rect class="{cls}" x="{self.X(x0):.3f}" y="{self.Y(y1):.3f}" width="{x1 - x0:.3f}" '
                 f'height="{y1 - y0:.3f}" {extra}/>')

    def line(self, pts, cls="", extra=""):
        d = " ".join(self.p(x, y) for x, y in pts)
        self.add(f'<polyline class="{cls}" points="{d}" {extra}/>')

    def poly(self, pts, cls="", extra=""):
        d = " ".join(self.p(x, y) for x, y in pts)
        self.add(f'<polygon class="{cls}" points="{d}" {extra}/>')

    def circle(self, cx, cy, r, cls="", extra=""):
        self.add(f'<circle class="{cls}" cx="{self.X(cx):.3f}" cy="{self.Y(cy):.3f}" r="{r:.3f}" {extra}/>')

    def text(self, x, y, s, size, cls="", anchor="middle", extra=""):
        self.add(f'<text class="{cls}" x="{self.X(x):.3f}" y="{self.Y(y):.3f}" font-size="{size:.3f}" '
                 f'text-anchor="{anchor}" {extra}>{esc(s)}</text>')

    def render(self, cls="plan"):
        w, h = self.x1 - self.x0, self.y1 - self.y0
        body = "\n".join(self.parts)
        return (f'<svg class="{cls}" viewBox="0 0 {w:.2f} {h:.2f}" role="img" aria-label="{esc(self.title)}" '
                f'xmlns="http://www.w3.org/2000/svg">\n<title>{esc(self.title)}</title>\n{DEFS}\n{body}\n</svg>')


DEFS = """<defs>
<radialGradient id="glow"><stop offset="0" style="stop-color:var(--lamp);stop-opacity:.55"/>
<stop offset=".55" style="stop-color:var(--lamp);stop-opacity:.22"/><stop offset="1" style="stop-color:var(--lamp);stop-opacity:0"/></radialGradient>
<pattern id="hatch" width=".35" height=".35" patternUnits="userSpaceOnUse" patternTransform="rotate(45)">
<line x1="0" y1="0" x2="0" y2=".35" style="stroke:var(--muted);stroke-width:.05"/></pattern>
<pattern id="grate" width=".18" height=".18" patternUnits="userSpaceOnUse">
<path d="M0 0H.18M0 0V.18" style="stroke:var(--ink);stroke-width:.03;fill:none"/></pattern>
</defs>"""


# ---------------------------------------------------------------------------------------------------------
# Geometry from the layout data
# ---------------------------------------------------------------------------------------------------------

def rooms_on(floor):
    return [r for r in V.ROOMS if r["floor"] == floor]


def inside_any(rooms, x, y):
    for r in rooms:
        x0, y0, x1, y1 = r["rect"]
        if x0 + EPS < x < x1 - EPS and y0 + EPS < y < y1 - EPS:
            return True
    return False


def door_axis(d):
    return "h" if d["swing"] in ("N", "S") else "v"


def union(intervals):
    out = []
    for a, b in sorted(intervals):
        if out and a <= out[-1][1] + EPS:
            out[-1][1] = max(out[-1][1], b)
        else:
            out.append([a, b])
    return out


def classify_walls(floor):
    """Returns wall segments (axis, c, a, b, thickness) before door gaps are cut."""
    rooms = rooms_on(floor)
    lines = {"h": defaultdict(list), "v": defaultdict(list)}
    for r in rooms:
        x0, y0, x1, y1 = r["rect"]
        lines["h"][round(y0, 3)].append((x0, x1))
        lines["h"][round(y1, 3)].append((x0, x1))
        lines["v"][round(x0, 3)].append((y0, y1))
        lines["v"][round(x1, 3)].append((y0, y1))
    segs = []
    for axis in ("h", "v"):
        for c, ivs in lines[axis].items():
            for a, b in union(ivs):
                cuts = {a, b}
                for r in rooms:
                    x0, y0, x1, y1 = r["rect"]
                    for t in ((x0, x1) if axis == "h" else (y0, y1)):
                        if a + EPS < t < b - EPS:
                            cuts.add(t)
                cuts = sorted(cuts)
                for s0, s1 in zip(cuts, cuts[1:]):
                    m = (s0 + s1) / 2
                    if axis == "h":
                        both = inside_any(rooms, m, c + 0.2) and inside_any(rooms, m, c - 0.2)
                    else:
                        both = inside_any(rooms, c + 0.2, m) and inside_any(rooms, c - 0.2, m)
                    t = V.INTERIOR_WALL if both else V.EXTERIOR_WALL
                    if segs and segs[-1][:2] == (axis, c) and abs(segs[-1][3] - s0) < EPS and segs[-1][4] == t:
                        segs[-1] = (axis, c, segs[-1][2], s1, t)
                    else:
                        segs.append((axis, c, s0, s1, t))
    return segs


def wall_thickness_at(segs, axis, c, pos):
    for ax, cc, a, b, t in segs:
        if ax == axis and abs(cc - c) < EPS and a - EPS <= pos <= b + EPS:
            return t
    return 0.0


def doors_on(floor):
    return [d for d in V.DOORS if d["floor"] == floor]


# ---------------------------------------------------------------------------------------------------------
# Drawing pieces
# ---------------------------------------------------------------------------------------------------------

def draw_room_fills(svg, floor, building=None):
    rooms = [r for r in rooms_on(floor) if building is None or r["building"] == building]
    rooms.sort(key=lambda r: -(r["rect"][2] - r["rect"][0]) * (r["rect"][3] - r["rect"][1]))
    for r in rooms:
        svg.rect(*r["rect"], cls=f"room rk-{r['kind']}")


def draw_walls(svg, floor):
    segs = classify_walls(floor)
    gaps = defaultdict(list)
    for d in doors_on(floor):
        ax = door_axis(d)
        c = d["at"][1] if ax == "h" else d["at"][0]
        pos = d["at"][0] if ax == "h" else d["at"][1]
        gaps[(ax, round(c, 3))].append((pos - d["width"] / 2, pos + d["width"] / 2))

    def perp_half(axis, c, pos):
        other = "v" if axis == "h" else "h"
        return wall_thickness_at(segs, other, pos, c) / 2

    svg.open("walls")
    for axis, c, a, b, t in segs:
        pieces = [[a, b, False, False]]
        for g0, g1 in gaps.get((axis, round(c, 3)), []):
            nxt = []
            for p0, p1, f0, f1 in pieces:
                if g1 <= p0 + EPS or g0 >= p1 - EPS:
                    nxt.append([p0, p1, f0, f1])
                    continue
                if g0 > p0 + EPS:
                    nxt.append([p0, g0, f0, True])
                if g1 < p1 - EPS:
                    nxt.append([g1, p1, True, f1])
            pieces = nxt
        for p0, p1, gap0, gap1 in pieces:
            e0 = 0.0 if gap0 else perp_half(axis, c, p0)
            e1 = 0.0 if gap1 else perp_half(axis, c, p1)
            if axis == "h":
                svg.rect(p0 - e0, c - t / 2, p1 + e1, c + t / 2, cls="wall")
            else:
                svg.rect(c - t / 2, p0 - e0, c + t / 2, p1 + e1, cls="wall")
    svg.close()
    return segs


def arc_points(cx, cy, r, a0, a1, n=14):
    return [(cx + r * math.cos(a0 + (a1 - a0) * i / n), cy + r * math.sin(a0 + (a1 - a0) * i / n)) for i in range(n + 1)]


def draw_leaf(svg, hinge, jamb, out, w, cls):
    """Single door leaf hinged at `hinge`, closed towards `jamb`, opening 90 degrees towards unit vector `out`."""
    hx, hy = hinge
    tip = (hx + out[0] * w, hy + out[1] * w)
    a_tip = math.atan2(tip[1] - hy, tip[0] - hx)
    a_jamb = math.atan2(jamb[1] - hy, jamb[0] - hx)
    d = a_jamb - a_tip
    while d > math.pi:
        d -= 2 * math.pi
    while d < -math.pi:
        d += 2 * math.pi
    svg.line(arc_points(hx, hy, w, a_tip, a_tip + d), cls=f"swing {cls}")
    svg.line([hinge, tip], cls=f"leaf {cls}")


def door_frame(d):
    """Returns (axis, wall coordinate c, jamb positions p0/p1, outward unit vector of the swing side)."""
    ax = door_axis(d)
    x, y = d["at"]
    w = d["width"]
    if ax == "h":
        out = (0, 1) if d["swing"] == "N" else (0, -1)
        return ax, y, x - w / 2, x + w / 2, out
    out = (1, 0) if d["swing"] == "E" else (-1, 0)
    return ax, x, y - w / 2, y + w / 2, out


def pt_on(ax, c, pos):
    return (pos, c) if ax == "h" else (c, pos)


def draw_door(svg, d, segs, show_locks=True):
    ax, c, p0, p1, out = door_frame(d)
    w = d["width"]
    t = wall_thickness_at(segs, ax, c, d["at"][0] if ax == "h" else d["at"][1]) or V.INTERIOR_WALL
    typ = d["type"]
    svg.open(f"door dt-{typ}")
    # threshold line across the opening
    if typ not in ("schiebetor",):
        a, b = pt_on(ax, c, p0), pt_on(ax, c, p1)
        svg.line([a, b], cls="threshold")
    hinge_pos, jamb_pos = (p0, p1) if d["hinge"] == "-" else (p1, p0)
    if typ in ("tuer", "haustuer", "glas", "stahl", "panzer", "gitter", "geheim", "pforte"):
        cls = {"glas": "glass", "geheim": "secret", "gitter": "grid", "panzer": "armour", "stahl": "steel",
               "haustuer": "main"}.get(typ, "")
        draw_leaf(svg, pt_on(ax, c, hinge_pos), pt_on(ax, c, jamb_pos), out, w, cls)
    elif typ in ("doppel", "glas2"):
        cls = "glass" if typ == "glas2" else ""
        mid = (p0 + p1) / 2
        draw_leaf(svg, pt_on(ax, c, p0), pt_on(ax, c, mid), out, w / 2, cls)
        draw_leaf(svg, pt_on(ax, c, p1), pt_on(ax, c, mid), out, w / 2, cls)
    elif typ == "schiebe":
        k = 0.05
        mid = (p0 + p1) / 2
        for (a, b), off in (((p0, mid + 0.1), -k), ((mid - 0.1, p1), k)):
            if ax == "h":
                svg.rect(a, c + off - 0.025, b, c + off + 0.025, cls="slide")
            else:
                svg.rect(c + off - 0.025, a, c + off + 0.025, b, cls="slide")
    elif typ == "tor":
        inset = 0.35
        a = pt_on(ax, c + out[0 if ax == "v" else 1] * inset, p0 + 0.1)
        b = pt_on(ax, c + out[0 if ax == "v" else 1] * inset, p1 - 0.1)
        svg.line([a, b], cls="gate-panel")
    elif typ == "schiebetor":
        a, b = pt_on(ax, c, p0), pt_on(ax, c, p1)
        svg.line([a, b], cls="gate-bar")
        y = c - 0.55
        svg.line([(p1 - 0.4, y), (p0 + 0.4, y)], cls="gate-arrow")
        svg.poly([(p0 + 0.4, y + 0.22), (p0 + 0.05, y), (p0 + 0.4, y - 0.22)], cls="gate-arrow-head")
    if show_locks and d.get("locked"):
        mid = (p0 + p1) / 2
        back = out if d.get("badge_side") == "swing" else (-out[0], -out[1])
        r = 0.34 if w >= 0.9 else 0.3
        off = t / 2 + r + 0.08
        cx, cy = pt_on(ax, c, mid)
        cx, cy = cx + back[0] * off, cy + back[1] * off
        svg.circle(cx, cy, r, cls="lock")
        label = str(d["lock"]) if d.get("lock") else "E"
        svg.text(cx, cy - r * 0.36, label, r * 1.1, cls="lock-t")
    svg.close()


def draw_windows(svg, floor, segs, building_rooms):
    for wdw in V.WINDOWS:
        if wdw["floor"] != floor:
            continue
        x, y = wdw["at"]
        w = wdw["width"]
        ax = wdw["axis"]
        c = y if ax == "h" else x
        pos = x if ax == "h" else y
        t = wall_thickness_at(segs, ax, c, pos)
        if t == 0:
            continue
        if ax == "h":
            outward = 1 if not inside_any(building_rooms, x, y + 0.4) else -1
        else:
            outward = 1 if not inside_any(building_rooms, x + 0.4, y) else -1
        typ = wdw["type"]
        a, b = pos - w / 2, pos + w / 2

        def band(off0, off1, cls):
            if ax == "h":
                svg.rect(a, c + off0, b, c + off1, cls=cls)
            else:
                svg.rect(c + off0, a, c + off1, b, cls=cls)

        def gline(off, cls="glass-line"):
            if ax == "h":
                svg.line([(a, c + off), (b, c + off)], cls=cls)
            else:
                svg.line([(c + off, a), (c + off, b)], cls=cls)

        if typ == "keller":
            o0, o1 = (t / 2, t / 2 + 0.75) if outward > 0 else (-t / 2 - 0.75, -t / 2)
            if ax == "h":
                svg.rect(a - 0.1, c + o0, b + 0.1, c + o1, cls="lightwell")
            else:
                svg.rect(c + o0, a - 0.1, c + o1, b + 0.1, cls="lightwell")
        band(-t / 2, t / 2, "win" + (" win-milch" if typ == "milch" else ""))
        if typ == "bodentief":
            for off in (-t / 4, 0, t / 4):
                gline(off)
        else:
            gline(-t / 6)
            gline(t / 6)
        if typ == "kipp":
            k = outward * (t / 2 + 0.28)
            if ax == "h":
                svg.poly([(a + 0.1, c + outward * t / 2), (b - 0.1, c + outward * t / 2), (pos, c + k)], cls="tilt")
            else:
                svg.poly([(c + outward * t / 2, a + 0.1), (c + outward * t / 2, b - 0.1), (c + k, pos)], cls="tilt")


def draw_stairs(svg, floor):
    for s in V.STAIRS:
        if s["floor"] != floor:
            continue
        x0, y0, x1, y1 = s["rect"]
        enter = s["enter"]
        if s["style"] == "hole":
            svg.rect(x0, y0, x1, y1, cls="stair-hole")
            svg.line([(x0, y0), (x1, y1)], cls="stair-x")
            svg.line([(x0, y1), (x1, y0)], cls="stair-x")
            svg.line([(x0 - 0.06, y0 - 0.06), (x0 - 0.06, y1)], cls="railing")
            svg.line([(x0 - 0.06, y0 - 0.06), (x1, y0 - 0.06)], cls="railing")
        else:
            svg.rect(x0, y0, x1, y1, cls="stair")
            step = 0.29
            if enter in ("N", "S"):
                n = int((y1 - y0) / step)
                for i in range(1, n + 1):
                    yy = y0 + i * step if enter == "S" else y1 - i * step
                    svg.line([(x0, yy), (x1, yy)], cls="tread")
            else:
                n = int((x1 - x0) / step)
                for i in range(1, n + 1):
                    xx = x0 + i * step if enter == "W" else x1 - i * step
                    svg.line([(xx, y0), (xx, y1)], cls="tread")
        # walking line with arrow
        cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
        if enter in ("N", "S"):
            sgn = 1 if enter == "S" else -1
            start = (cx, (y0 if enter == "S" else y1) + sgn * 0.25)
            end = (cx, (y1 if enter == "S" else y0) - sgn * 0.3)
            head = [(cx - 0.22, end[1] - sgn * 0.35), end, (cx + 0.22, end[1] - sgn * 0.35)]
        else:
            sgn = 1 if enter == "W" else -1
            start = ((x0 if enter == "W" else x1) + sgn * 0.25, cy)
            end = ((x1 if enter == "W" else x0) - sgn * 0.3, cy)
            head = [(end[0] - sgn * 0.35, cy - 0.22), end, (end[0] - sgn * 0.35, cy + 0.22)]
        svg.circle(start[0], start[1], 0.09, cls="walk-dot")
        svg.line([start, end], cls="walk")
        svg.line(head, cls="walk")
        # label beside the stair, rotated along it
        side = s.get("label_side", "L")
        if s["label"] and side == "S":
            # horizontal, just before the first step
            if enter in ("N", "S"):
                ly = (y0 - 0.62) if enter == "S" else (y1 + 0.38)
                svg.text(cx, ly, s["label"], 0.34, cls="stair-t")
            else:
                lx = (x0 - 0.2) if enter == "W" else (x1 + 0.2)
                svg.text(lx, cy - 0.12, s["label"], 0.34, cls="stair-t", anchor="end" if enter == "W" else "start")
        elif s["label"]:
            if enter in ("N", "S"):
                lx = x1 + 0.45 if side == "R" else x0 - 0.22
                svg.add(f'<text class="stair-t" x="{svg.X(lx):.3f}" y="{svg.Y(cy):.3f}" font-size="0.36" '
                        f'text-anchor="middle" transform="rotate(-90 {svg.X(lx):.3f} {svg.Y(cy):.3f})">'
                        f'{esc(s["label"])}</text>')
            else:
                svg.text(cx, y1 + 0.2, s["label"], 0.36, cls="stair-t")


def draw_fixtures(svg, floor, xr=None):
    svg.open("lyr-moebel")
    for f in V.FIXTURES:
        if f["floor"] != floor:
            continue
        x0, y0, x1, y1 = f["rect"]
        if xr and not (xr[0] <= x0 <= xr[1]):
            continue
        svg.rect(x0, y0, x1, y1, cls="fixture")
        if f["label"]:
            w, h = x1 - x0, y1 - y0
            size = min(0.3, max(w, h) / max(len(f["label"]), 1) * 1.7)
            if h > w * 1.6:
                cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
                svg.add(f'<text class="fixture-t" x="{svg.X(cx):.3f}" y="{svg.Y(cy):.3f}" font-size="{size:.3f}" '
                        f'text-anchor="middle" dominant-baseline="central" '
                        f'transform="rotate(-90 {svg.X(cx):.3f} {svg.Y(cy):.3f})">{esc(f["label"])}</text>')
            else:
                svg.text((x0 + x1) / 2, (y0 + y1) / 2, f["label"], size, cls="fixture-t",
                         extra='dominant-baseline="central"')
    svg.close()


def room_net_area(r, rooms):
    x0, y0, x1, y1 = r["rect"]
    area = (x1 - x0) * (y1 - y0)
    for o in rooms:
        if o is r:
            continue
        a0, b0, a1, b1 = o["rect"]
        if x0 - EPS <= a0 and a1 <= x1 + EPS and y0 - EPS <= b0 and b1 <= y1 + EPS and (a1 - a0) * (b1 - b0) < area:
            area -= (a1 - a0) * (b1 - b0)
    return area


def has_children(r, rooms):
    x0, y0, x1, y1 = r["rect"]
    for o in rooms:
        if o is r:
            continue
        a0, b0, a1, b1 = o["rect"]
        if x0 - EPS <= a0 and a1 <= x1 + EPS and y0 - EPS <= b0 and b1 <= y1 + EPS and \
                (a1 - a0) * (b1 - b0) < (x1 - x0) * (y1 - y0):
            return True
    return False


def draw_room_labels(svg, floor, fs, building=None, compact=False):
    rooms = rooms_on(floor)
    for r in rooms:
        if building and r["building"] != building:
            continue
        if "label_at" in r and r["label_at"] is None:
            continue
        x0, y0, x1, y1 = r["rect"]
        cx, cy = r.get("label_at") or ((x0 + x1) / 2, (y0 + y1) / 2)
        avail = (x1 - x0) - 0.5
        size = fs
        name = r["name"]
        lines = [plain(name)]
        if len(plain(name)) * size * 0.5 > avail and SHY in name:
            parts = name.split(SHY)
            lines = [parts[0] + "-", "".join(parts[1:])]
        longest = max(len(s) for s in lines)
        if longest * size * 0.5 > avail:
            size = max(avail / (longest * 0.5), fs * 0.55)
        sub = []
        if not compact:
            area = room_net_area(r, rooms)
            if has_children(r, rooms):
                sub.append(f"{m2(area)} m²")
            else:
                sub.append(f"{num(x1 - x0, 1)} × {num(y1 - y0, 1)} m · {m2(area)} m²")
        lh = size * 1.05
        total = len(lines) * lh + (len(sub) * fs * 0.72 * 1.15 if sub else 0)
        top = cy + total / 2 - size * 0.8
        for i, s in enumerate(lines):
            svg.text(cx, top - i * lh, s, size, cls="room-t")
        if sub and " · " in sub[0] and len(sub[0]) * fs * 0.62 * 0.6 > avail:
            sub = [sub[0].split(" · ")[1]]
        if sub:
            sub_size = min(fs * 0.62, avail / (len(sub[0]) * 0.6)) if not compact else fs * 0.6
            sub_size = max(sub_size, fs * 0.42)
            svg.text(cx, top - len(lines) * lh + size * 0.12, sub[0], sub_size, cls="room-sub")


def draw_markers(svg, kinds, floors, size=0.55):
    for mk in V.MARKERS:
        if mk["kind"] not in kinds or mk["floor"] not in floors:
            continue
        x, y = mk["at"]
        layer = {"einstieg": "lyr-einstiege", "ziel": "lyr-ziele", "sicherheit": "lyr-ziele"}[mk["kind"]]
        svg.open(f"{layer} mk mk-{mk['kind']}")
        r = size
        if mk["kind"] == "einstieg":
            svg.poly([(x, y + r * 1.15), (x + r * 1.15, y), (x, y - r * 1.15), (x - r * 1.15, y)], cls="mk-shape")
        elif mk["kind"] == "ziel":
            svg.circle(x, y, r, cls="mk-shape")
        else:
            svg.rect(x - r * 0.9, y - r * 0.9, x + r * 0.9, y + r * 0.9, cls="mk-shape")
        svg.text(x, y - r * 0.34, mk["code"], r * 0.95, cls="mk-t")
        svg.add(f"<title>{esc(mk['code'] + ' ' + mk['text'])}</title>")
        svg.close()


def draw_north_and_scale(svg, scale_len=10, at_scale=None, at_north=None, size=1.0):
    nx, ny = at_north
    svg.open("compass")
    svg.circle(nx, ny, 1.25 * size, cls="compass-ring")
    svg.poly([(nx, ny + 1.05 * size), (nx + 0.42 * size, ny - 0.55 * size), (nx, ny - 0.2 * size),
              (nx - 0.42 * size, ny - 0.55 * size)], cls="compass-needle")
    svg.text(nx, ny + 1.55 * size, "N", 0.9 * size, cls="compass-t")
    svg.close()
    sx, sy = at_scale
    svg.open("scale")
    seg = scale_len / 2
    svg.rect(sx, sy, sx + seg, sy + 0.32 * size, cls="scale-a")
    svg.rect(sx + seg, sy, sx + scale_len, sy + 0.32 * size, cls="scale-b")
    for i, v in enumerate((0, seg, scale_len)):
        svg.text(sx + v, sy - 0.75 * size, f"{num(v, 0)}" + (" m" if i == 2 else ""), 0.62 * size, cls="scale-t")
    svg.close()


def draw_dimension(svg, a, b, c, axis, text, size=0.5, ext=0.6):
    """Dimension line from a to b at coordinate c (axis h = horizontal line at y=c)."""
    svg.open("dim")
    if axis == "h":
        svg.line([(a, c), (b, c)], cls="dim-line")
        for p in (a, b):
            svg.line([(p, c - ext / 2), (p, c + ext / 2)], cls="dim-line")
            svg.line([(p - 0.18, c - 0.18), (p + 0.18, c + 0.18)], cls="dim-tick")
        svg.text((a + b) / 2, c + 0.22, text, size, cls="dim-t")
    else:
        svg.line([(c, a), (c, b)], cls="dim-line")
        for p in (a, b):
            svg.line([(c - ext / 2, p), (c + ext / 2, p)], cls="dim-line")
            svg.line([(c - 0.18, p - 0.18), (c + 0.18, p + 0.18)], cls="dim-tick")
        cx, cy = svg.X(c - 0.22), svg.Y((a + b) / 2)
        svg.add(f'<text class="dim-t" x="{cx:.3f}" y="{cy:.3f}" font-size="{size:.3f}" text-anchor="middle" '
                f'transform="rotate(-90 {cx:.3f} {cy:.3f})">{esc(text)}</text>')
    svg.close()


# ---------------------------------------------------------------------------------------------------------
# Outdoor parts
# ---------------------------------------------------------------------------------------------------------

def draw_surface(svg, s):
    kind = s["kind"]
    shape = s["shape"]
    cls = f"surf s-{kind}"
    if shape[0] == "rect":
        svg.rect(*shape[1], cls=cls)
    elif shape[0] == "circle":
        svg.circle(*shape[1], cls=cls)
    elif shape[0] == "ring":
        cx, cy, ro, ri = shape[1]
        svg.circle(cx, cy, (ro + ri) / 2, cls=f"ring s-{kind}", extra=f'style="stroke-width:{ro - ri:.3f}"')
    elif shape[0] == "path":
        pts, width = shape[1], shape[2]
        svg.line(pts, cls=f"pathway s-{kind}", extra=f'style="stroke-width:{width:.3f}"')


def draw_fence(svg):
    gates = [d for d in V.DOORS if d["floor"] == "Aussen"]
    pts = V.FENCE
    svg.open("fence")
    for (ax0, ay0), (ax1, ay1) in zip(pts, pts[1:]):
        horizontal = abs(ay0 - ay1) < EPS
        a, b = (min(ax0, ax1), max(ax0, ax1)) if horizontal else (min(ay0, ay1), max(ay0, ay1))
        c = ay0 if horizontal else ax0
        pieces = [(a, b)]
        for g in gates:
            gax, gc, g0, g1, _ = door_frame(g)
            if (gax == "h") != horizontal or abs(gc - c) > EPS:
                continue
            nxt = []
            for p0, p1 in pieces:
                if g1 <= p0 or g0 >= p1:
                    nxt.append((p0, p1))
                    continue
                if g0 > p0:
                    nxt.append((p0, g0))
                if g1 < p1:
                    nxt.append((g1, p1))
            pieces = nxt
        for p0, p1 in pieces:
            if horizontal:
                svg.line([(p0, c), (p1, c)], cls="fence-line")
            else:
                svg.line([(c, p0), (c, p1)], cls="fence-line")
            n = max(1, int((p1 - p0) / 2.5))
            for i in range(n + 1):
                q = p0 + (p1 - p0) * i / n
                x, y = (q, c) if horizontal else (c, q)
                svg.rect(x - 0.11, y - 0.11, x + 0.11, y + 0.11, cls="fence-post")
    svg.close()
    for g in gates:
        draw_door(svg, g, [], show_locks=True)


def draw_trees(svg):
    svg.open("trees")
    for x, y, r, kind in V.TREES:
        svg.circle(x, y, r, cls=f"tree t-{kind}")
        if kind == "laub":
            svg.circle(x, y, r * 0.62, cls="tree-inner")
        svg.circle(x, y, 0.16, cls="trunk")
    svg.close()


def draw_lights(svg, glow=True):
    if glow:
        svg.open("lyr-licht")
        for x, y, kind, r, _ in V.LIGHTS:
            if kind == "bm":
                svg.circle(x, y, r, cls="light-bm")
            else:
                svg.circle(x, y, r, cls="light-glow")
        svg.close()
    svg.open("lights")
    for x, y, kind, r, note in V.LIGHTS:
        size = {"laterne": 0.42, "poller": 0.24, "wand": 0.3, "bm": 0.34, "pool": 0.26, "boden": 0.26}[kind]
        svg.circle(x, y, size, cls=f"lamp lamp-{kind}")
        if kind in ("laterne", "wand", "bm"):
            k = size * 0.7
            svg.line([(x - k, y - k), (x + k, y + k)], cls="lamp-x")
            svg.line([(x - k, y + k), (x + k, y - k)], cls="lamp-x")
        if note:
            svg.add(f"<title>{esc(note)} ({kind})</title>")
    svg.close()


# ---------------------------------------------------------------------------------------------------------
# Whole drawings
# ---------------------------------------------------------------------------------------------------------

def site_plan():
    svg = Svg(-4.0, -10.0, 78.0, 53.0, "Lageplan Villa Lindenhof")
    svg.rect(-4, -10, 78, 53, cls="ground")
    svg.rect(*V.PLOT, cls="lawn")
    for s in V.SURFACES:
        draw_surface(svg, s)
    # neighbour house
    svg.rect(*V.NEIGHBOUR_HOUSE, cls="neighbour-house")
    svg.text((V.NEIGHBOUR_HOUSE[0] + V.NEIGHBOUR_HOUSE[2]) / 2, (V.NEIGHBOUR_HOUSE[1] + V.NEIGHBOUR_HOUSE[3]) / 2,
             "Nachbarhaus", 1.0, cls="label-muted")
    for h in V.HEDGES:
        svg.rect(*h["rect"], cls="hedge" + (" hedge-low" if h["height"] < 1.5 else ""))
    # buildings: ground floor of house and remise
    svg.rect(*V.CANOPY["rect"], cls="canopy")
    draw_room_fills(svg, "EG")
    draw_stairs(svg, "Aussen")
    segs = draw_walls(svg, "EG")
    draw_windows(svg, "EG", segs, rooms_on("EG"))
    draw_stairs(svg, "EG")
    for d in doors_on("EG"):
        draw_door(svg, d, segs)
    # outdoor cellar stair walls
    svg.line([(45.75, 24.3), (45.75, 30.0)], cls="retaining")
    svg.line([(44.15, 24.3), (45.75, 24.3)], cls="retaining-low")
    b = V.BALCONY["rect"]
    svg.rect(*b, cls="balcony-outline")
    draw_room_labels(svg, "EG", 0.55, compact=True)
    draw_trees(svg)
    draw_fence(svg)
    draw_lights(svg)
    svg.open("site-labels")
    for x, y, text, size, style in V.SITE_LABELS:
        svg.text(x, y, text.replace(SHY, ""), size, cls=f"site-t st-{style}")
    svg.close()
    draw_markers(svg, ("einstieg",), ("Aussen", "EG", "OG", "KG"), size=0.7)
    draw_markers(svg, ("sicherheit", "ziel"), ("Aussen",), size=0.7)
    draw_dimension(svg, 0, 60, 52.3, "h", "60 m", size=0.8)
    draw_dimension(svg, 0, 48, -2.6, "v", "48 m", size=0.8)
    draw_north_and_scale(svg, scale_len=10, at_scale=(62.0, -7.4), at_north=(75.4, -6.6), size=1.0)
    return svg.render("plan site")


def floor_plan(floor):
    svg = Svg(19.0, 12.4, 48.6, 35.0, f"{V.FLOORS[floor]['name']} Haupthaus")
    svg.rect(19.0, 12.4, 48.6, 35.0, cls="paper")
    house = [r for r in rooms_on(floor) if r["building"] == "Haus"]
    # outdoor context, faint
    if floor == "EG":
        svg.rect(22, 30, 46, 34, cls="ctx")
        svg.rect(31.0, 14.0, 34.0, 16.0, cls="ctx")
        svg.text(34, 32.4, "Terrasse", 0.55, cls="ctx-t")
        svg.text(35.9, 14.75, "Eingang", 0.42, cls="ctx-t")
        draw_stairs(svg, "Aussen")
        svg.line([(45.75, 24.3), (45.75, 30.0)], cls="retaining")
    elif floor == "OG":
        svg.rect(*V.BALCONY["rect"], cls="balcony")
        svg.line([(37.0, 31.8), (43.0, 31.8)], cls="railing")
        svg.text(40.0, 30.7, "Balkon", 0.5, cls="ctx-t")
        svg.rect(30.6, 14.0, 34.4, 16.0, cls="ctx-dash")
        svg.text(32.5, 14.75, "Vordach", 0.42, cls="ctx-t")
    elif floor == "KG":
        svg.rect(44.15, 24.3, 45.75, 30.0, cls="ctx")
        svg.line([(45.75, 24.3), (45.75, 30.0)], cls="retaining")
    draw_room_fills(svg, floor, building="Haus")
    draw_fixtures(svg, floor, xr=(20, 47))
    segs = draw_walls(svg, floor)
    draw_windows(svg, floor, segs, house)
    draw_stairs(svg, floor)
    for d in doors_on(floor):
        if 20 < d["at"][0] < 47:
            draw_door(svg, d, segs)
    draw_room_labels(svg, floor, 0.46, building="Haus")
    draw_markers(svg, ("einstieg", "ziel", "sicherheit"), (floor,), size=0.42)
    draw_dimension(svg, 22, 44, 13.2, "h", "22,00 m", size=0.42, ext=0.45)
    draw_dimension(svg, 16, 30, 19.75, "v", "14,00 m", size=0.42, ext=0.45)
    draw_north_and_scale(svg, scale_len=5, at_scale=(41.0, 33.6), at_north=(47.4, 33.0), size=0.5)
    return svg.render("plan floor")


def remise_plan():
    svg = Svg(0.6, 15.4, 17.6, 28.8, "Remise mit Garage, Werkstatt und Geräteraum")
    svg.rect(0.6, 15.4, 17.6, 28.8, cls="paper")
    svg.rect(3, 15.4, 15, 18, cls="ctx")
    svg.text(9.0, 16.4, "Vorplatz", 0.5, cls="ctx-t")
    draw_room_fills(svg, "EG", building="Remise")
    draw_fixtures(svg, "EG", xr=(0, 16))
    rem = [r for r in rooms_on("EG") if r["building"] == "Remise"]
    segs = classify_walls("EG")
    segs = [s for s in segs if s[1] < 16 and (s[0] == "v" or s[2] < 16)]
    # draw walls only for the remise (reuse the generic routine on a filtered copy)
    gaps = defaultdict(list)
    for d in doors_on("EG"):
        if d["at"][0] < 16:
            ax = door_axis(d)
            c = d["at"][1] if ax == "h" else d["at"][0]
            pos = d["at"][0] if ax == "h" else d["at"][1]
            gaps[(ax, round(c, 3))].append((pos - d["width"] / 2, pos + d["width"] / 2))
    svg.open("walls")
    for axis, c, a, b, t in segs:
        if axis == "h" and a > 16:
            continue
        pieces = [[a, b, False, False]]
        for g0, g1 in gaps.get((axis, round(c, 3)), []):
            nxt = []
            for p0, p1, f0, f1 in pieces:
                if g1 <= p0 + EPS or g0 >= p1 - EPS:
                    nxt.append([p0, p1, f0, f1])
                    continue
                if g0 > p0 + EPS:
                    nxt.append([p0, g0, f0, True])
                if g1 < p1 - EPS:
                    nxt.append([g1, p1, True, f1])
            pieces = nxt
        for p0, p1, gap0, gap1 in pieces:
            other = "v" if axis == "h" else "h"
            e0 = 0.0 if gap0 else wall_thickness_at(segs, other, p0, c) / 2
            e1 = 0.0 if gap1 else wall_thickness_at(segs, other, p1, c) / 2
            if axis == "h":
                svg.rect(p0 - e0, c - t / 2, p1 + e1, c + t / 2, cls="wall")
            else:
                svg.rect(c - t / 2, p0 - e0, c + t / 2, p1 + e1, cls="wall")
    svg.close()
    draw_windows(svg, "EG", segs, rem)
    for d in doors_on("EG"):
        if d["at"][0] < 16:
            draw_door(svg, d, segs)
    draw_room_labels(svg, "EG", 0.46, building="Remise")
    draw_markers(svg, ("ziel",), ("EG",), size=0.42)
    draw_dimension(svg, 3, 15, 27.2, "h", "12,00 m", size=0.42, ext=0.45)
    draw_dimension(svg, 18, 25, 1.6, "v", "7,00 m", size=0.42, ext=0.45)
    draw_north_and_scale(svg, scale_len=5, at_scale=(10.8, 27.9), at_north=(16.6, 27.4), size=0.45)
    return svg.render("plan small")


# ---------------------------------------------------------------------------------------------------------
# Tables and page
# ---------------------------------------------------------------------------------------------------------

DOOR_TYPES = {
    "haustuer": "Haustür (massiv)", "tuer": "Holztür", "doppel": "Flügeltür", "glas": "Glastür",
    "glas2": "Glas-Flügeltür", "schiebe": "Schiebetür", "stahl": "Stahltür", "panzer": "Panzertür",
    "gitter": "Gittertür", "geheim": "Geheimtür (Regal)", "offen": "Durchgang", "tor": "Sektionaltor",
    "schiebetor": "Schiebetor", "pforte": "Gartentür",
}
FLOOR_NAMES = {"KG": "Keller", "EG": "Erdgeschoss", "OG": "1. Stock", "Aussen": "Zaun"}
KIND_NAMES = {"wohnen": "Wohnen", "schlafen": "Schlafen", "nass": "Bad / WC", "service": "Versorgung",
              "verkehr": "Flur / Treppe", "technik": "Technik", "wert": "Wertvolles", "garage": "Garage"}


def room_table(floor, building):
    rooms = [r for r in rooms_on(floor) if r["building"] == building]
    all_rooms = rooms_on(floor)
    rows = []
    for r in sorted(rooms, key=lambda r: (-r["rect"][3], r["rect"][0])):
        x0, y0, x1, y1 = r["rect"]
        area = room_net_area(r, all_rooms)
        rows.append(
            f'<tr><td><span class="sw rk-{r["kind"]}"></span>{esc(plain(r["name"]))}</td>'
            f'<td class="num">{m2(area)}&nbsp;m²</td><td>{esc(r["note"])}</td></tr>')
    return ('<table class="rooms"><thead><tr><th>Raum</th><th class="num">Fläche</th><th>Was drin ist / wozu</th>'
            '</tr></thead><tbody>' + "".join(rows) + "</tbody></table>")


def door_table():
    rows = []
    order = {"Aussen": 0, "EG": 1, "OG": 2, "KG": 3}
    doors = [d for d in V.DOORS if d["type"] != "offen"]
    doors.sort(key=lambda d: (order[d["floor"]], 0 if d["locked"] else 1, -(d.get("lock") or 0)))
    for d in doors:
        if d["locked"]:
            lock = f'<span class="pill pill-lock">{d["lock"]}</span>' if d.get("lock") else \
                '<span class="pill pill-lock">elektr.</span>'
        else:
            lock = '<span class="pill pill-open">offen</span>'
        where = FLOOR_NAMES[d["floor"]]
        if d["id"].startswith("r_"):
            where = "Remise"
        rows.append(f'<tr><td>{esc(d["name"])}</td><td>{where}</td><td>{DOOR_TYPES[d["type"]]}</td>'
                    f'<td class="num">{num(d["width"], 2)}&nbsp;m</td><td>{lock}</td><td>{esc(d["note"])}</td></tr>')
    return ('<table class="doors"><thead><tr><th>Tür / Tor</th><th>Wo</th><th>Art</th><th class="num">Breite</th>'
            '<th>Schloss</th><th>Hinweis</th></tr></thead><tbody>' + "".join(rows) + "</tbody></table>")


def marker_list(kind):
    items = []
    for mk in V.MARKERS:
        if mk["kind"] != kind:
            continue
        where = FLOOR_NAMES.get(mk["floor"], "")
        if mk["floor"] == "Aussen":
            where = "Gelände"
        items.append(f'<li><span class="code code-{kind}">{mk["code"]}</span><span>{esc(mk["text"])}'
                     f' <em>· {where}</em></span></li>')
    return f'<ul class="markers">{"".join(items)}</ul>'


def stats():
    rooms = [r for r in V.ROOMS if r.get("label_at", 1) is not None]
    doors = [d for d in V.DOORS if d["type"] != "offen"]
    return {
        "rooms": len(rooms),
        "doors": len(doors),
        "locked": len([d for d in doors if d["locked"]]),
        "windows": len(V.WINDOWS),
        "lights": len(V.LIGHTS),
        "trees": len(V.TREES),
        "area": sum(room_net_area(r, rooms_on(r["floor"])) for r in V.ROOMS if r["building"] == "Haus"),
    }


CSS = r"""
/* Layout: one reading column for text, wide figure bands for the plans; plans sit on "drafting paper". */
:root{
  --paper:#f4f6f5; --sheet:#ffffff; --ink:#1b2630; --muted:#5b6b76; --rule:#cdd5d9; --accent:#1f6f8b;
  --wall:#1b2630; --win-bg:#ffffff; --glass:#2f7fbf; --stair:#eef1f2;
  --rk-wohnen:#f0e2cb; --rk-schlafen:#d8e4f2; --rk-nass:#cfeaee; --rk-service:#ebe8d6; --rk-verkehr:#e8ebed;
  --rk-technik:#e1dbea; --rk-wert:#f5d5c2; --rk-garage:#e1e2de;
  --lawn:#d6e5c8; --ground:#e3e9df; --kies:#e6e0d1; --pflaster:#d5d3cd; --stein:#dcd8d0; --holz:#e1c9a7;
  --wasser:#8fcbe2; --beet:#c3d6a4; --strasse:#c3c8cc; --gehweg:#d9dcde; --feldweg:#ddd5c2; --nachbar:#e5e8e1;
  --hedge:#5f8f4e; --hedge-low:#8db57a; --tree:#6fa65b; --tree-edge:#3f6e33; --fence:#2c3a44;
  --lamp:#f0b400; --lamp-edge:#7a5b00; --entry:#d4480f; --target:#a87400; --security:#a3214f; --lock:#c62828;
  --font-display:"Barlow Condensed","Arial Narrow",system-ui,sans-serif;
  --font-body:"Barlow",system-ui,-apple-system,"Segoe UI",sans-serif;
  --font-data:"IBM Plex Mono",ui-monospace,"Cascadia Mono",Consolas,monospace;
}
@media (prefers-color-scheme: dark){:root:not([data-theme="light"]){
  --paper:#0d151d; --sheet:#121d27; --ink:#dbe5ec; --muted:#8da0ad; --rule:#26384a; --accent:#6cc3e0;
  --wall:#e3ebf0; --win-bg:#121d27; --glass:#7cc4f5; --stair:#1a2733;
  --rk-wohnen:#3a3124; --rk-schlafen:#203148; --rk-nass:#1c3c43; --rk-service:#313324; --rk-verkehr:#1e2b36;
  --rk-technik:#2d293b; --rk-wert:#46302a; --rk-garage:#272c31;
  --lawn:#15241a; --ground:#101a14; --kies:#2b2925; --pflaster:#2c3036; --stein:#2b2d31; --holz:#3a2e21;
  --wasser:#1b5470; --beet:#203320; --strasse:#222a31; --gehweg:#2a3238; --feldweg:#2c2820; --nachbar:#18221b;
  --hedge:#2f5a31; --hedge-low:#3e6b3c; --tree:#3b7044; --tree-edge:#7fbf86; --fence:#c4d0d8;
  --lamp:#ffd34d; --lamp-edge:#ffe58f; --entry:#ff8a4c; --target:#f2c14e; --security:#f06595; --lock:#ff6b6b;
  color-scheme:dark;
}}
:root[data-theme="dark"]{
  --paper:#0d151d; --sheet:#121d27; --ink:#dbe5ec; --muted:#8da0ad; --rule:#26384a; --accent:#6cc3e0;
  --wall:#e3ebf0; --win-bg:#121d27; --glass:#7cc4f5; --stair:#1a2733;
  --rk-wohnen:#3a3124; --rk-schlafen:#203148; --rk-nass:#1c3c43; --rk-service:#313324; --rk-verkehr:#1e2b36;
  --rk-technik:#2d293b; --rk-wert:#46302a; --rk-garage:#272c31;
  --lawn:#15241a; --ground:#101a14; --kies:#2b2925; --pflaster:#2c3036; --stein:#2b2d31; --holz:#3a2e21;
  --wasser:#1b5470; --beet:#203320; --strasse:#222a31; --gehweg:#2a3238; --feldweg:#2c2820; --nachbar:#18221b;
  --hedge:#2f5a31; --hedge-low:#3e6b3c; --tree:#3b7044; --tree-edge:#7fbf86; --fence:#c4d0d8;
  --lamp:#ffd34d; --lamp-edge:#ffe58f; --entry:#ff8a4c; --target:#f2c14e; --security:#f06595; --lock:#ff6b6b;
  color-scheme:dark;
}
*{box-sizing:border-box}
body{background:var(--paper);color:var(--ink);font-family:var(--font-body);font-size:16px;line-height:1.55;
  padding-left:max(16px,env(safe-area-inset-left,0px));padding-right:max(16px,env(safe-area-inset-right,0px));
  padding-block:28px 64px}
.wrap{max-width:1240px;margin:0 auto;display:flex;flex-direction:column;gap:44px}
header{display:flex;flex-direction:column;gap:14px;max-width:880px}
.eyebrow{font-family:var(--font-data);font-size:12.5px;letter-spacing:.06em;text-transform:uppercase;color:var(--muted)}
h1{font-family:var(--font-display);font-weight:700;font-size:clamp(40px,6vw,64px);line-height:1;margin:0;
  letter-spacing:.01em;text-wrap:balance}
h2{font-family:var(--font-display);font-weight:600;font-size:30px;line-height:1.1;margin:0;text-wrap:balance}
h3{font-family:var(--font-display);font-weight:600;font-size:21px;margin:0}
p{margin:0;max-width:68ch}
.lead{font-size:18px;color:var(--ink)}
.facts{display:flex;flex-wrap:wrap;gap:8px 22px;font-family:var(--font-data);font-size:13.5px;color:var(--muted);
  font-variant-numeric:tabular-nums}
.facts b{color:var(--ink);font-weight:500}
section{display:flex;flex-direction:column;gap:16px}
.sec-head{display:flex;flex-wrap:wrap;align-items:baseline;gap:6px 18px}
.sec-head .tag{font-family:var(--font-data);font-size:12.5px;color:var(--muted);letter-spacing:.04em}
.figure{background:var(--sheet);border:1px solid var(--rule);border-radius:6px;overflow-x:auto}
.figure svg{display:block;width:100%;height:auto;min-width:760px}
.figure svg.small{min-width:520px}
.controls{position:sticky;top:env(safe-area-inset-top,0px);z-index:5;display:flex;flex-wrap:wrap;gap:8px 18px;
  align-items:center;padding:10px 14px;background:var(--sheet);border:1px solid var(--rule);border-radius:6px;
  font-size:14px}
@media (max-width:700px){.controls{position:static}}
body::before{content:"";position:fixed;inset:0 0 auto;height:env(safe-area-inset-top,0px);z-index:10;background:var(--paper)}
.controls .lbl{font-family:var(--font-data);font-size:12px;letter-spacing:.06em;text-transform:uppercase;color:var(--muted)}
.controls label{display:inline-flex;align-items:center;gap:7px;cursor:pointer;user-select:none}
.controls input{accent-color:var(--accent);width:16px;height:16px}
.controls input:focus-visible{outline:2px solid var(--accent);outline-offset:2px}
.nav{display:flex;flex-wrap:wrap;gap:6px 14px;margin-left:auto}
.nav a{color:var(--accent);text-decoration:none;font-weight:500}
.nav a:hover,.nav a:focus-visible{text-decoration:underline}
.split{display:flex;flex-direction:column;gap:18px}
.split > *{min-width:0}
.below{display:grid;grid-template-columns:minmax(0,1fr);gap:14px 36px;align-items:start}
@media (min-width:960px){.below{grid-template-columns:minmax(0,2fr) minmax(0,1fr)}}
.below > *{min-width:0}
.note{font-size:14.5px;color:var(--muted)}
table{border-collapse:collapse;width:100%;font-size:14.5px}
th{font-family:var(--font-data);font-weight:500;font-size:12px;letter-spacing:.05em;text-transform:uppercase;
  color:var(--muted);text-align:left;padding:6px 8px;border-bottom:1px solid var(--rule)}
td{padding:7px 8px;border-bottom:1px solid var(--rule);vertical-align:top}
td.num,th.num{text-align:right;font-family:var(--font-data);font-variant-numeric:tabular-nums;white-space:nowrap}
.table-wrap{overflow-x:auto}
.sw{display:inline-block;width:11px;height:11px;border-radius:2px;margin-right:8px;vertical-align:-1px;
  border:1px solid var(--rule)}
.sw.rk-wohnen{background:var(--rk-wohnen)}.sw.rk-schlafen{background:var(--rk-schlafen)}.sw.rk-nass{background:var(--rk-nass)}
.sw.rk-service{background:var(--rk-service)}.sw.rk-verkehr{background:var(--rk-verkehr)}.sw.rk-technik{background:var(--rk-technik)}
.sw.rk-wert{background:var(--rk-wert)}.sw.rk-garage{background:var(--rk-garage)}
.pill{display:inline-block;font-family:var(--font-data);font-size:12px;padding:1px 8px;border-radius:999px;
  border:1px solid currentColor;white-space:nowrap}
.pill-lock{color:var(--lock)}.pill-open{color:var(--muted)}
.cols{display:grid;grid-template-columns:repeat(auto-fit,minmax(min(100%,340px),1fr));gap:22px 30px}
.cols > *{min-width:0;display:flex;flex-direction:column;gap:10px}
.markers{list-style:none;margin:0;padding:0;display:flex;flex-direction:column;gap:8px;font-size:15px}
.markers li{display:flex;gap:10px;align-items:flex-start}
.markers em{color:var(--muted);font-style:normal;font-size:13.5px}
.code{flex:none;font-family:var(--font-data);font-size:12px;font-weight:500;min-width:30px;text-align:center;
  padding:1px 5px;border-radius:3px;color:var(--sheet)}
.code-einstieg{background:var(--entry)}.code-ziel{background:var(--target)}.code-sicherheit{background:var(--security)}
.legend{display:grid;grid-template-columns:repeat(auto-fit,minmax(min(100%,210px),1fr));gap:8px 22px;font-size:14px}
.legend div{display:flex;align-items:center;gap:10px}
.legend svg{flex:none;width:34px;height:20px}
ol.q{margin:0;padding-left:22px;display:flex;flex-direction:column;gap:9px;max-width:80ch}
ol.q b{font-weight:600}
.soft{color:var(--muted);font-style:normal}
ul.plain{margin:0;padding-left:20px;display:flex;flex-direction:column;gap:6px;max-width:80ch}
code{font-family:var(--font-data);font-size:13px;background:var(--stair);padding:1px 5px;border-radius:3px}
.hide-licht .lyr-licht,.hide-einstiege .lyr-einstiege,.hide-ziele .lyr-ziele,.hide-moebel .lyr-moebel{display:none}

/* SVG drawing classes */
svg text{font-family:var(--font-body)}
.paper{fill:var(--sheet)}
.ground{fill:var(--ground)}
.lawn{fill:var(--lawn)}
.room{stroke:none}
.rk-wohnen{fill:var(--rk-wohnen)}.rk-schlafen{fill:var(--rk-schlafen)}.rk-nass{fill:var(--rk-nass)}
.rk-service{fill:var(--rk-service)}.rk-verkehr{fill:var(--rk-verkehr)}.rk-technik{fill:var(--rk-technik)}
.rk-wert{fill:var(--rk-wert)}.rk-garage{fill:var(--rk-garage)}
.wall{fill:var(--wall)}
.win{fill:var(--win-bg);stroke:var(--wall);stroke-width:.03}
.win-milch{fill:url(#hatch)}
.glass-line{stroke:var(--glass);stroke-width:.035;fill:none}
.tilt{fill:var(--entry);opacity:.9}
.lightwell{fill:url(#grate);stroke:var(--ink);stroke-width:.04}
.threshold{stroke:var(--muted);stroke-width:.03;stroke-dasharray:.12 .08;fill:none}
.leaf{stroke:var(--ink);stroke-width:.06;fill:none;stroke-linecap:round}
.leaf.main{stroke-width:.1}
.leaf.steel{stroke-width:.1}
.leaf.armour{stroke-width:.2;stroke:var(--target)}
.leaf.glass{stroke:var(--glass);stroke-width:.07}
.leaf.secret{stroke:var(--security);stroke-dasharray:.12 .08}
.leaf.grid{stroke-dasharray:.06 .06;stroke-width:.09}
.swing{stroke:var(--muted);stroke-width:.025;fill:none}
.swing.secret{stroke:var(--security);stroke-dasharray:.12 .08}
.swing.armour{stroke:var(--target)}
.slide{fill:var(--glass)}
.gate-panel{stroke:var(--ink);stroke-width:.1;stroke-dasharray:.3 .15;fill:none}
.gate-bar{stroke:var(--fence);stroke-width:.22;fill:none}
.gate-arrow{stroke:var(--fence);stroke-width:.08;fill:none}
.gate-arrow-head{fill:var(--fence)}
.lock{fill:var(--sheet);stroke:var(--lock);stroke-width:.06}
.lock-t{fill:var(--lock);font-family:var(--font-data);font-weight:500}
.stair{fill:var(--stair);stroke:var(--ink);stroke-width:.04}
.tread{stroke:var(--ink);stroke-width:.02}
.stair-hole{fill:var(--sheet);stroke:var(--ink);stroke-width:.04;stroke-dasharray:.15 .1}
.stair-x{stroke:var(--rule);stroke-width:.03}
.railing{stroke:var(--ink);stroke-width:.06;fill:none}
.walk{stroke:var(--accent);stroke-width:.05;fill:none;stroke-linejoin:round}
.walk-dot{fill:var(--accent)}
.stair-t{fill:var(--accent);font-family:var(--font-data)}
.fixture{fill:none;stroke:var(--muted);stroke-width:.03}
.fixture-t{fill:var(--muted)}
.room-t{fill:var(--ink);font-family:var(--font-display);font-weight:600}
.room-sub{fill:var(--muted);font-family:var(--font-data)}
.mk-shape{stroke:var(--sheet);stroke-width:.06}
.mk-einstieg .mk-shape{fill:var(--entry)}
.mk-ziel .mk-shape{fill:var(--target)}
.mk-sicherheit .mk-shape{fill:var(--security)}
.mk-t{fill:var(--sheet);font-family:var(--font-data);font-weight:500}
.compass-ring{fill:var(--sheet);stroke:var(--ink);stroke-width:.05}
.compass-needle{fill:var(--ink)}
.compass-t{fill:var(--ink);font-family:var(--font-display);font-weight:700}
.scale-a{fill:var(--ink)}.scale-b{fill:var(--sheet);stroke:var(--ink);stroke-width:.05}
.scale-t{fill:var(--ink);font-family:var(--font-data)}
.dim-line{stroke:var(--muted);stroke-width:.025;fill:none}
.dim-tick{stroke:var(--ink);stroke-width:.05}
.dim-t{fill:var(--muted);font-family:var(--font-data)}
.ctx{fill:var(--stein);opacity:.55}
.ctx-dash{fill:none;stroke:var(--muted);stroke-width:.04;stroke-dasharray:.2 .12}
.ctx-t{fill:var(--muted);font-family:var(--font-data)}
.earth{fill:none}
.balcony{fill:var(--holz);opacity:.7;stroke:var(--ink);stroke-width:.04}
.balcony-outline{fill:none;stroke:var(--ink);stroke-width:.06;stroke-dasharray:.3 .2}
.canopy{fill:var(--pflaster);stroke:var(--ink);stroke-width:.04;stroke-dasharray:.25 .15}
.retaining{stroke:var(--wall);stroke-width:.2}
.retaining-low{stroke:var(--wall);stroke-width:.1}
.surf{stroke:none}
.s-strasse{fill:var(--strasse)}.s-gehweg{fill:var(--gehweg)}.s-feldweg{fill:var(--feldweg)}.s-nachbar{fill:var(--nachbar)}
.s-kies{fill:var(--kies)}.s-pflaster{fill:var(--pflaster)}.s-stein{fill:var(--stein)}.s-holz{fill:var(--holz)}
.s-wasser{fill:var(--wasser)}.s-beet{fill:var(--beet)}
.ring.s-kies{fill:none;stroke:var(--kies)}
.pathway{fill:none;stroke-linejoin:miter;stroke-linecap:butt}
.pathway.s-pflaster{stroke:var(--pflaster)}
.pathway.s-trittstein{stroke:var(--pflaster);stroke-dasharray:.6 .45}
rect.s-wasser{stroke:var(--glass);stroke-width:.08}
.neighbour-house{fill:var(--rk-garage);stroke:var(--muted);stroke-width:.08}
.label-muted{fill:var(--muted);font-family:var(--font-display);font-weight:600}
.hedge{fill:var(--hedge)}
.hedge-low{fill:var(--hedge-low)}
.tree{fill:var(--tree);fill-opacity:.62;stroke:var(--tree-edge);stroke-width:.07}
.t-obst{fill-opacity:.48}
.t-nadel{fill-opacity:.85}
.tree-inner{fill:none;stroke:var(--tree-edge);stroke-width:.04;stroke-opacity:.5}
.trunk{fill:var(--tree-edge)}
.fence-line{stroke:var(--fence);stroke-width:.09;fill:none}
.fence-post{fill:var(--fence)}
.light-glow{fill:url(#glow)}
.light-bm{fill:var(--lamp);fill-opacity:.07;stroke:var(--lamp-edge);stroke-width:.07;stroke-dasharray:.35 .25}
.lamp{fill:var(--lamp);stroke:var(--lamp-edge);stroke-width:.05}
.lamp-x{stroke:var(--lamp-edge);stroke-width:.05}
.site-t{font-family:var(--font-display);font-weight:600;fill:var(--muted)}
.st-street{fill:var(--muted);letter-spacing:.08em}
.st-water{fill:var(--ink)}
.st-building{fill:var(--ink);letter-spacing:.04em}
@media (prefers-reduced-motion:no-preference){.figure svg .lyr-licht{transition:opacity .2s}}
"""

LEGEND_ITEMS = [
    ('<rect x="2" y="7" width="30" height="6" style="fill:var(--wall)"/>', "Wand (außen 30 cm, innen 15 cm)"),
    ('<rect x="2" y="6" width="30" height="8" style="fill:var(--win-bg);stroke:var(--wall);stroke-width:1"/>'
     '<line x1="2" y1="9" x2="32" y2="9" style="stroke:var(--glass);stroke-width:1.2"/>'
     '<line x1="2" y1="11" x2="32" y2="11" style="stroke:var(--glass);stroke-width:1.2"/>', "Fenster"),
    ('<rect x="2" y="6" width="30" height="8" style="fill:var(--win-bg);stroke:var(--wall);stroke-width:1"/>'
     '<polygon points="6,6 28,6 17,1" style="fill:var(--entry)"/>', "Kippfenster (gekippt)"),
    ('<rect x="4" y="2" width="26" height="12" style="fill:url(#grate);stroke:var(--ink)"/>'
     '<rect x="2" y="14" width="30" height="5" style="fill:var(--win-bg);stroke:var(--wall)"/>', "Kellerfenster mit Lichtschacht"),
    ('<line x1="4" y1="18" x2="4" y2="2" style="stroke:var(--ink);stroke-width:2"/>'
     '<path d="M4 2 A16 16 0 0 1 20 18" style="fill:none;stroke:var(--muted);stroke-width:1"/>', "Tür mit Aufschlag"),
    ('<rect x="2" y="8" width="18" height="2" style="fill:var(--glass)"/><rect x="14" y="11" width="18" height="2" '
     'style="fill:var(--glass)"/>', "Schiebetür (Glas)"),
    ('<circle cx="17" cy="10" r="8" style="fill:var(--sheet);stroke:var(--lock);stroke-width:1.6"/>'
     '<text x="17" y="14" font-size="11" text-anchor="middle" style="fill:var(--lock);font-family:var(--font-data)">5</text>',
     "verschlossen, Schloss-Stufe"),
    ('<rect x="9" y="1" width="16" height="18" style="fill:var(--stair);stroke:var(--ink)"/>'
     '<line x1="9" y1="6" x2="25" y2="6" style="stroke:var(--ink)"/><line x1="9" y1="11" x2="25" y2="11" style="stroke:var(--ink)"/>'
     '<line x1="17" y1="17" x2="17" y2="3" style="stroke:var(--accent);stroke-width:1.4"/>', "Treppe, Pfeil = Laufrichtung"),
    ('<circle cx="17" cy="10" r="9" style="fill:url(#glow)"/><circle cx="17" cy="10" r="3" style="fill:var(--lamp);stroke:var(--lamp-edge)"/>',
     "Leuchte mit Lichtkegel"),
    ('<circle cx="17" cy="10" r="8.5" style="fill:none;stroke:var(--lamp-edge);stroke-width:1.2;stroke-dasharray:3 2"/>'
     '<circle cx="17" cy="10" r="3" style="fill:var(--lamp);stroke:var(--lamp-edge)"/>', "Strahler mit Bewegungsmelder"),
    ('<circle cx="17" cy="10" r="9" style="fill:var(--tree);fill-opacity:.62;stroke:var(--tree-edge)"/>', "Baum (Krone)"),
    ('<rect x="2" y="5" width="30" height="10" style="fill:var(--hedge)"/>', "Hecke 2,2 m (heller: 1,2 m)"),
    ('<line x1="2" y1="10" x2="32" y2="10" style="stroke:var(--fence);stroke-width:1.6"/>'
     '<rect x="2" y="8" width="4" height="4" style="fill:var(--fence)"/><rect x="28" y="8" width="4" height="4" style="fill:var(--fence)"/>',
     "Zaun 1,8 m"),
    ('<polygon points="17,1 26,10 17,19 8,10" style="fill:var(--entry)"/>', "E = möglicher Einstieg"),
    ('<circle cx="17" cy="10" r="8" style="fill:var(--target)"/>', "Z = Ziel / Beute"),
    ('<rect x="10" y="3" width="14" height="14" style="fill:var(--security)"/>', "A = Alarm / Sicherheit"),
]


def legend():
    items = "".join(f'<div><svg viewBox="0 0 34 20" aria-hidden="true">{DEFS_SMALL}{g}</svg><span>{t}</span></div>'
                    for g, t in LEGEND_ITEMS)
    return f'<div class="legend">{items}</div>'


DEFS_SMALL = ('<defs><radialGradient id="glow-s"><stop offset="0" style="stop-color:var(--lamp);stop-opacity:.6"/>'
              '<stop offset="1" style="stop-color:var(--lamp);stop-opacity:0"/></radialGradient></defs>')


def page():
    st = stats()
    site = site_plan()
    eg, og, kg = floor_plan("EG"), floor_plan("OG"), floor_plan("KG")
    rem = remise_plan()
    legend_html = legend().replace("url(#glow)", "url(#glow-s)")
    parts = []
    parts.append(f"""<title>Villa Lindenhof</title>
<meta name="claude:viewport-fit-sides" content="cover">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Barlow+Condensed:wght@500;600;700&family=Barlow:wght@400;500;600&family=IBM+Plex+Mono:wght@400;500&display=swap">
<style>{CSS}</style>
<div class="wrap">
<header>
  <div class="eyebrow">der clou 3 · Demo-Level · Grundriss v1 · Bauabschnitt 1 gebaut · 8. November 2026</div>
  <h1>Villa Lindenhof</h1>
  <p class="lead">Eine freistehende Villa am Ortsrand, {esc(V.ADDRESS)}: Keller, Erdgeschoss und 1. Stock, dazu eine Remise mit
  Garage und Werkstatt, Pool, Garten, Zaun und Hecke. Diese Fassung legt nur den Grundriss fest. Möbel und Beute sind
  angedeutet, damit man sieht, wofür die Räume da sind.</p>
  <div class="facts">
    <span>Grundstück <b>60 × 48 m</b></span><span>Haupthaus <b>22 × 14 m</b>, 3 Ebenen</span>
    <span>Wohnfläche Haus <b>≈ {st['area']:.0f} m²</b></span><span><b>{st['rooms']}</b> Räume</span>
    <span><b>{st['doors']}</b> Türen und Tore, <b>{st['locked']}</b> verschlossen</span>
    <span><b>{st['windows']}</b> Fenster</span><span><b>{st['lights']}</b> Außenleuchten</span>
    <span><b>{st['trees']}</b> Bäume</span>
  </div>
</header>

<div class="controls" role="group" aria-label="Ebenen im Plan ein- und ausblenden">
  <span class="lbl">Im Plan zeigen</span>
  <label><input type="checkbox" id="t-licht" data-layer="licht" checked> Lichtkegel</label>
  <label><input type="checkbox" id="t-einstiege" data-layer="einstiege" checked> Einstiege</label>
  <label><input type="checkbox" id="t-ziele" data-layer="ziele" checked> Ziele &amp; Sicherheit</label>
  <label><input type="checkbox" id="t-moebel" data-layer="moebel" checked> Möbel</label>
  <nav class="nav" aria-label="Abschnitte"><a href="#lageplan">Lageplan</a><a href="#eg">EG</a><a href="#og">1. Stock</a>
  <a href="#kg">Keller</a><a href="#remise">Remise</a><a href="#tueren">Türen</a><a href="#entscheidungen">Entscheidungen</a><a href="#fenster">Fenster</a></nav>
</div>

<section id="lageplan">
  <div class="sec-head"><h2>Lageplan</h2><span class="tag">Grundstück mit Erdgeschoss · Maßstab in Metern</span></div>
  <div class="figure">{site}</div>
  {legend_html}
  <div class="cols">
    <div><h3>Wie man reinkommt</h3>{marker_list("einstieg")}</div>
    <div><h3>Licht und Schatten</h3>
      <ul class="plain">
        <li>Hell sind Tor, Vorfahrt und Haustür. Von der Straße sieht man über die niedrige Ligusterhecke bis zur Haustür.</li>
        <li>Garage, Seiteneingang und Kelleraußentreppe haben Strahler mit Bewegungsmelder (gestrichelt). Sie gehen erst an,
        wenn jemand vorbeigeht.</li>
        <li>Dunkel bleiben die Westseite hinter der Remise, die Nordhecke mit dem Hintertor und die Obstwiese im Osten.</li>
        <li>Der Pool leuchtet von unten. Wer über die Terrasse geht, steht im Licht.</li>
        <li>Der Nachbar wohnt hinter der Osthecke. Was auf der Ostseite kracht, hört er zuerst.</li>
      </ul>
    </div>
    <div><h3>Ziele und Sicherheit</h3>{marker_list("ziel")}{marker_list("sicherheit")}</div>
  </div>
</section>

<section id="eg">
  <div class="sec-head"><h2>Erdgeschoss</h2><span class="tag">Haupthaus · Fußboden ± 0,00 m · Raumhöhe 2,80 m</span></div>
  <div class="split"><div class="figure">{eg}</div>
  <div class="below"><div class="table-wrap">{room_table("EG", "Haus")}</div>
  <p class="note">Die Halle ist der Knoten: links die Kellertür, rechts die Treppe nach oben, geradeaus Esszimmer und
  Wohnzimmer. Die Bibliothek erreicht man nur über das Wohnzimmer.</p></div></div>
</section>

<section id="og">
  <div class="sec-head"><h2>1. Stock</h2><span class="tag">Haupthaus · Fußboden + 3,10 m · Raumhöhe 2,70 m · zweiter Bauabschnitt</span></div>
  <div class="split"><div class="figure">{og}</div>
  <div class="below"><div class="table-wrap">{room_table("OG", "Haus")}</div>
  <p class="note">Die Treppe kommt in der Galerie an. Alles liegt an einem Flur. Die Elternsuite im Osten hat Ankleide,
  Bad und den Balkon über der Terrasse.</p></div></div>
</section>

<section id="kg">
  <div class="sec-head"><h2>Keller</h2><span class="tag">Haupthaus · Fußboden − 2,90 m · Raumhöhe 2,55 m</span></div>
  <div class="split"><div class="figure">{kg}</div>
  <div class="below"><div class="table-wrap">{room_table("KG", "Haus")}</div>
  <p class="note">Zwei Wege führen in den Keller: die Treppe aus der Halle und die Außentreppe im Osten. Der Tresorraum
  liegt unter der Bibliothek. Technik mit Alarmzentrale liegt am anderen Ende des Flurs.</p></div></div>
</section>

<section id="remise">
  <div class="sec-head"><h2>Remise</h2><span class="tag">Nebengebäude · eingeschossig · 12 × 7 m</span></div>
  <div class="split"><div class="figure">{rem}</div>
  <div class="below"><div class="table-wrap">{room_table("EG", "Remise")}</div>
  <p class="note">Die Remise ist ein Werkzeuglager: Wer zuerst hierher geht, findet Brecheisen, Bolzenschneider und die Leiter
  für den Balkon. Dafür steht man vor der Garage im Licht des Bewegungsmelders.</p></div></div>
</section>

<section id="tueren">
  <div class="sec-head"><h2>Türen und Tore</h2><span class="tag">Schloss-Stufe wie im Spiel (0–10) · Durchgänge ohne Tür nicht aufgeführt</span></div>
  <div class="table-wrap">{door_table()}</div>
</section>

<section id="spiel">
  <div class="sec-head"><h2>Für das Spiel</h2></div>
  <div class="cols">
    <div><h3>Vier Wege zum Tresor</h3>
      <ul class="plain">
        <li><b>Frech:</b> Pforte → Haustür (Schloss 6) → Alarm am Bedienteil ausschalten → Halle → Kellertür → Tresorraum.
        Kurz, aber im Licht und von der Straße zu sehen.</li>
        <li><b>Leise:</b> Hintertor → Trittsteine → Küchentür (Glas, Schloss 3) → Flur → Halle → Keller.
        Lang und dunkel.</li>
        <li><b>Handwerker:</b> Garage hinten → Werkzeug holen → Seiteneingang (Schloss 4). Bewegungsmelder an der Garage.</li>
        <li><b>Keller direkt:</b> Ostseite → Kelleraußentür (Stahl, Schloss 5) → Kellerflur. Nah am Nachbarn.</li>
      </ul>
    </div>
    <div><h3>Laufzeiten bei 2,5 m/s</h3>
      <ul class="plain">
        <li>Einfahrtstor → Haustür: ca. 30 m, 12 s</li>
        <li>Hintertor → Küchentür: ca. 32 m, 13 s</li>
        <li>Haustür → Tresorraum (über die Kellertreppe): ca. 22 m, 9 s</li>
        <li>Garage → Seiteneingang: ca. 14 m, 6 s</li>
        <li>Mit voller Last (Tempo 58 %) fast doppelt so lang.</li>
      </ul>
    </div>
    <div><h3>Maße für den Bau</h3>
      <ul class="plain">
        <li>Ebenen: Keller − 2,90 m, Erdgeschoss ± 0, 1. Stock + 3,10 m.</li>
        <li>Haupttreppe 1,6 m breit, 5 m lang, 18 Stufen. Kellertreppe 1,5 m breit, 4,45 m lang, 16 Stufen.</li>
        <li>Türen mindestens 1,0 m breit, damit die Wegfindung (Radius 30 cm, 10-cm-Raster) durchpasst.</li>
        <li>Daten: <code>Scripts/Levels/villa_layout.py</code>. Diese Seite: <code>Scripts/Levels/villa_plan.py</code>.</li>
      </ul>
    </div>
  </div>
</section>

<section id="entscheidungen">
  <div class="sec-head"><h2>Entscheidungen</h2><span class="tag">mit dem User abgestimmt am 8. November 2026</span></div>
  <p class="note"><b>Stand:</b> Bauabschnitt 1 ist gebaut (Level L_Villa01: Grundstück, Remise, Erdgeschoss, Keller). Die
  Kamera zeigt eine Ebene. Noch offen: Fenster als Einstieg, Bewegungsmelder, Alarm, 1. Stock, Modelle statt Kisten.</p>
  <ol class="q">
    <li><b>Stockwerke in der Kamera:</b> Die Kamera zeigt eine Ebene, alles darüber wird ausgeblendet. Sie folgt der
    ausgewählten Figur, mit Bild auf / Bild ab schaltet man selbst um.</li>
    <li><b>Fenster sind gleich Einstiege.</b> Kippfenster, Glas einschlagen und später die Leiter zum Balkon gehören von
    Anfang an dazu. Die Regeln unten sind bestätigt.</li>
    <li><b>Bauabschnitte:</b> Zuerst Grundstück, Remise, Erdgeschoss und Keller mit Tresorraum. Der 1. Stock folgt als
    zweiter Abschnitt, mit ihm Leiter und Balkon.</li>
    <li><b>Das Haus ist leer.</b> Die Familie ist im Urlaub. Gefahr kommt vom Nachbarn, vom Licht und vom Alarm.</li>
    <li><b>Geheimkammer bleibt drin.</b> Das Regal ist vorerst eine verschlossene Tür. Das Finden kommt später mit dem Nebel.</li>
    <li><b>Pool und Garten</b> sind vorerst Kulisse <em class="soft">(mein Vorschlag, gilt bis zum Widerspruch)</em>.</li>
    <li><b>Bewegungsmelder:</b> Die Lampe geht an, wenn eine Figur in den gestrichelten Kreis kommt. Steht der Nachbar in
    Sichtweite, wird er misstrauischer <em class="soft">(mein Vorschlag)</em>.</li>
  </ol>
</section>

<section id="fenster">
  <div class="sec-head"><h2>Fenster als Einstieg</h2><span class="tag">Regeln bestätigt · Mechanik noch nicht gebaut</span></div>
  <div class="table-wrap"><table class="doors"><thead><tr><th>Fenster</th><th>Was man tut</th><th class="num">Dauer</th>
  <th>Lärm</th><th>Bedingung</th></tr></thead><tbody>
    <tr><td>gekipptes Fenster</td><td>durchgreifen, Griff drehen, öffnen</td><td class="num">8&nbsp;s</td><td>leise (Klicken)</td>
    <td>Fertigkeit Schlösser ab 3, eine Hand frei</td></tr>
    <tr><td>geschlossenes Fenster</td><td>mit dem Brecheisen aufhebeln</td><td class="num">6&nbsp;s</td><td>laut (Holz kracht, ca. 6&nbsp;m)</td>
    <td>Brecheisen griffbereit, Kraft ab 4, beide Hände frei</td></tr>
    <tr><td>jedes Fenster</td><td>Glas einschlagen</td><td class="num">1&nbsp;s</td><td>sehr laut (wie die Vitrine, 17&nbsp;m)</td>
    <td>Brecheisen, danach Scherben (später: Verletzung?)</td></tr>
    <tr><td>offenes Fenster</td><td>durchsteigen</td><td class="num">3&nbsp;s</td><td>leise</td>
    <td>nichts Großes in den Händen, kein Rucksack bei kleinen Fenstern</td></tr>
    <tr><td>Kellerfenster</td><td>vergittert, vorerst kein Einstieg</td><td class="num">–</td><td>–</td>
    <td>später mit Bolzenschneider</td></tr>
  </tbody></table></div>
  <ul class="plain">
    <li>Kleine Fenster (Gäste-WC 0,7&nbsp;m) lassen nur ohne Rucksack durch. Beute reicht man vorher durch oder legt sie aufs Fensterbrett.</li>
    <li>Der Alarm überwacht Haustür, Terrassentüren und die Kelleraußentür, aber keine Fenster. Deshalb sind Fenster interessant.</li>
    <li>Fenster gehen in der Planung wie Türen: Radialmenü am Fenster, Schritt im Plan, „Wer durchsteigt, öffnet“ gilt nicht.
    Erst öffnen, dann durchsteigen.</li>
  </ul>
</section>
</div>
<script>
(function(){{
  var key = "villa-layers";
  var saved = {{}};
  try {{ saved = JSON.parse(localStorage.getItem(key) || "{{}}") || {{}}; }} catch (e) {{ saved = {{}}; }}
  var boxes = document.querySelectorAll(".controls input[data-layer]");
  function apply(){{
    var state = {{}};
    boxes.forEach(function(b){{
      document.body.classList.toggle("hide-" + b.dataset.layer, !b.checked);
      state[b.dataset.layer] = b.checked;
    }});
    try {{ localStorage.setItem(key, JSON.stringify(state)); }} catch (e) {{}}
  }}
  boxes.forEach(function(b){{
    if (saved.hasOwnProperty(b.dataset.layer)) b.checked = !!saved[b.dataset.layer];
    b.addEventListener("change", apply);
  }});
  apply();
}})();
</script>
""")
    return "".join(parts)


def main():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    text = page()
    with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text)
    print(f"wrote {OUT} ({len(text) // 1024} KB)")


if __name__ == "__main__":
    main()
