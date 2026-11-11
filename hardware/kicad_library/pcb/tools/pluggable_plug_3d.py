"""Builds the Desktopia.3dshapes/Plug_*.wrl models: the screw plugs of the motherboard's pluggable terminal blocks
(J1 / J5 / J6 / J7), mated in their header, for the Desktopia:PluggableHeader_* footprints (pluggable_header_fp.py).
Each footprint shows KiCad's Phoenix Contact header model plus one of these plugs.

Frame: the one of KiCad's header models (Phoenix Contact MSTBA 2,5/..-G-5,08, GMSTBA 2,5/..-G-7,62 and
MC 1,5/..-G-3,81; the Kefa KF2EDGRC and KANGNEX 15EDGRC headers copy them): origin on pin 1, x along the row,
mating face towards -y (the footprint's +y), z up from the board top.

The plugs (Kefa KF2EDGK, KANGNEX 15EDGK) copy Phoenix Contact's, whose datasheets give:
  MSTB 2,5/2-ST-5,08    width 10.16, height 15, length 18.3
  GMSTB 2,5/2-ST-7,62   width 13.24, height 15, length 18.2
  MC 1,5/3-ST-3,81      width 12.22, height 11.1, length 16.1
The front tongue fills the header's cavity (measured on KiCad's header models: opening, back wall, floor and roof)
around the header pins; the rest of the plug stands in front of the header: one screw per pole on top (M3, M2 for the
3.81 mm plug), the wire entries at the back.

usage: python pluggable_plug_3d.py [out dir]
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from vrml import Mesh, shape, write       # noqa: E402

OUT_DIR = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "Desktopia.3dshapes")

# name: plug outline (Phoenix) + header cavity (KiCad header model, model frame)
PLUGS = {
    "Plug_2EDGK_1x02_P5.08mm": dict(
        n=2, pitch=5.08, width=10.16, height=15.0, length=18.3, chamfer=1.0,
        opening=-10.0, back=-1.5, cav_x=(-2.74, 7.82), cav_z=(1.6, 6.9),
        screw_r=1.75, screw_depth=3.0, slot=0.6, wire=3.2, wire_z=0.36, wire_depth=4.0,
        title="Kefa KF2EDGK-5.08-2P plug (Phoenix MSTB 2,5/2-ST-5,08) in a KF2EDGRC-5.08-2P header"),
    "Plug_2EDGK_1x02_P7.62mm": dict(
        n=2, pitch=7.62, width=13.24, height=15.0, length=18.2, chamfer=1.0,
        opening=-10.0, back=-1.5, cav_x=(-3.01, 10.63), cav_z=(1.6, 6.9),
        screw_r=1.75, screw_depth=3.0, slot=0.6, wire=3.2, wire_z=0.36, wire_depth=4.0,
        title="Kefa KF2EDGK-7.62-2P plug (Phoenix GMSTB 2,5/2-ST-7,62) in a KF2EDGRC-7.62-2P header"),
    "Plug_15EDGK_1x03_P3.81mm": dict(
        n=3, pitch=3.81, width=12.22, height=11.1, length=16.1, chamfer=0.8,
        opening=-8.0, back=-1.4, cav_x=(-1.5, 9.12), cav_z=(0.5, 6.5),
        screw_r=1.3, screw_depth=2.2, slot=0.45, wire=2.3, wire_z=0.36, wire_depth=3.0,
        title="KANGNEX WJ15EDGK-3.81-03P plug (Phoenix MC 1,5/3-ST-3,81) in a WJ15EDGRC-3.81-03P header"),
}

GAP = 0.05                      # between the plug and the header's front face
CLEAR = 0.1                     # between the tongue and the cavity walls

MAT = {
    "body": ((0.337, 0.678, 0.443), (0.2, 0.2, 0.2), 0.3, 0.0, None),      # the green of KiCad's header models
    "metal": ((0.824, 0.820, 0.780), (0.9, 0.9, 0.9), 0.7, 0.0, None),     # nickel-plated screws and clamps
    "dark": ((0.05, 0.10, 0.06), (0.05, 0.05, 0.05), 0.1, 0.0, None),       # inside the holes
}


def _normal(pts):
    """Newell normal of a planar polygon."""
    nx = ny = nz = 0.0
    for (x0, y0, z0), (x1, y1, z1) in zip(pts, pts[1:] + pts[:1]):
        nx += (y0 - y1) * (z0 + z1)
        ny += (z0 - z1) * (x0 + x1)
        nz += (x0 - x1) * (y0 + y1)
    return nx, ny, nz


def face(mesh, pts, n):
    """Adds a convex polygon, wound so that its normal points along n."""
    pts = list(pts)
    if sum(a * b for a, b in zip(_normal(pts), n)) < 0:
        pts.reverse()
    mesh.poly(pts)


def ray_to_rect(cx, cy, a, rect):
    """Point where the ray from (cx, cy) at angle a leaves the rectangle (x0, x1, y0, y1)."""
    x0, x1, y0, y1 = rect
    dx, dy = math.cos(a), math.sin(a)
    t = []
    if dx > 1e-12:
        t.append((x1 - cx) / dx)
    if dx < -1e-12:
        t.append((x0 - cx) / dx)
    if dy > 1e-12:
        t.append((y1 - cy) / dy)
    if dy < -1e-12:
        t.append((y0 - cy) / dy)
    k = min(t)
    return cx + k * dx, cy + k * dy


def plate_with_hole(mesh, rect, cx, cy, r, z, seg=48):
    """Horizontal rectangle at height z (normal +z) with a round hole: quads from the circle to the edge, with the
    four corners among the rays so that each quad ends on one side of the rectangle."""
    x0, x1, y0, y1 = rect
    angles = {round(2 * math.pi * k / seg, 9) for k in range(seg)}
    for px, py in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)):
        angles.add(round(math.atan2(py - cy, px - cx) % (2 * math.pi), 9))
    angles = sorted(angles)
    for a, b in zip(angles, angles[1:] + [angles[0] + 2 * math.pi]):
        ea, eb = ray_to_rect(cx, cy, a, rect), ray_to_rect(cx, cy, b, rect)
        face(mesh, [(cx + r * math.cos(a), cy + r * math.sin(a), z), (ea[0], ea[1], z), (eb[0], eb[1], z),
                    (cx + r * math.cos(b), cy + r * math.sin(b), z)], (0, 0, 1))


def outline(p):
    """(x0, x1, yf, yt, yr, zb, zt): plug sides, front face, tongue tip, rear face, bottom and top (model frame)."""
    xc = (p["n"] - 1) * p["pitch"] / 2
    x0, x1 = xc - p["width"] / 2, xc + p["width"] / 2
    zb = p["cav_z"][0] + CLEAR                          # the plug rests on the cavity floor
    yf = p["opening"] - GAP                             # plug front face, against the header
    yt = p["back"] - 0.3                                # tongue tip, short of the cavity back wall
    yr = yf - (p["length"] - (yt - yf))                 # rear face (wire entries)
    return x0, x1, yf, yt, yr, zb, zb + p["height"]


def build(p):
    body, metal, dark = Mesh(), Mesh(), Mesh()
    n, pitch, c = p["n"], p["pitch"], p["chamfer"]
    x0, x1, yf, yt, yr, zb, zt = outline(p)

    # tongue in the header cavity
    cx0, cx1 = p["cav_x"]
    tx0, tx1 = max(x0, cx0 + CLEAR), min(x1, cx1 - CLEAR)
    body.box(tx0, tx1, yf, yt, zb, p["cav_z"][1] - CLEAR)

    # body: side profile (y, z) extruded along x, chamfered on the top edges
    prof = [(yf, zb), (yr, zb), (yr, zt - c), (yr + c, zt), (yf - c, zt), (yf, zt - c)]
    face(body, [(x0, y, z) for y, z in prof], (-1, 0, 0))
    face(body, [(x1, y, z) for y, z in prof], (1, 0, 0))
    face(body, [(x0, yr, zb), (x1, yr, zb), (x1, yf, zb), (x0, yf, zb)], (0, 0, -1))
    face(body, [(x0, yf, zb), (x1, yf, zb), (x1, yf, zt - c), (x0, yf, zt - c)], (0, 1, 0))
    face(body, [(x0, yf, zt - c), (x1, yf, zt - c), (x1, yf - c, zt), (x0, yf - c, zt)], (0, 1, 1))
    face(body, [(x0, yr, zt - c), (x1, yr, zt - c), (x1, yr + c, zt), (x0, yr + c, zt)], (0, -1, 1))

    # top: one screw per pole, half way along the body
    r, ys = p["screw_r"], (yf + yr) / 2
    ya, yb = yr + c, yf - c
    h = min(r + 1.0, ys - ya, yb - ys)
    face(body, [(x0, ya, zt), (x1, ya, zt), (x1, ys - h, zt), (x0, ys - h, zt)], (0, 0, 1))
    face(body, [(x0, ys + h, zt), (x1, ys + h, zt), (x1, yb, zt), (x0, yb, zt)], (0, 0, 1))
    zs = zt - p["screw_depth"]
    seg = 48
    for i in range(n):
        xi = i * pitch
        cell = (max(x0, xi - pitch / 2), min(x1, xi + pitch / 2), ys - h, ys + h)
        if i == 0:
            cell = (x0,) + cell[1:]
        if i == n - 1:
            cell = (cell[0], x1) + cell[2:]
        plate_with_hole(body, cell, xi, ys, r, zt, seg)
        for k in range(seg):                            # hole wall, facing the axis
            a, b = 2 * math.pi * k / seg, 2 * math.pi * (k + 1) / seg
            pa, pb = (xi + r * math.cos(a), ys + r * math.sin(a)), (xi + r * math.cos(b), ys + r * math.sin(b))
            face(dark, [(pa[0], pa[1], zs), (pb[0], pb[1], zs), (pb[0], pb[1], zt), (pa[0], pa[1], zt)],
                 (-math.cos((a + b) / 2), -math.sin((a + b) / 2), 0))
        rh = r - 0.1                                    # slotted screw head at the bottom of the hole
        face(metal, [(xi + rh * math.cos(2 * math.pi * k / seg), ys + rh * math.sin(2 * math.pi * k / seg), zs + 0.01)
                     for k in range(seg)], (0, 0, 1))
        sw, sl = p["slot"] / 2, rh * 0.95
        face(dark, [(xi - sw, ys - sl, zs + 0.02), (xi + sw, ys - sl, zs + 0.02), (xi + sw, ys + sl, zs + 0.02),
                    (xi - sw, ys + sl, zs + 0.02)], (0, 0, 1))

    # rear: one square wire entry per pole
    s = p["wire"] / 2
    zw = zb + p["wire_z"] * p["height"]
    za, zc = zw - s, zw + s
    face(body, [(x0, yr, zb), (x1, yr, zb), (x1, yr, za), (x0, yr, za)], (0, -1, 0))
    face(body, [(x0, yr, zc), (x1, yr, zc), (x1, yr, zt - c), (x0, yr, zt - c)], (0, -1, 0))
    edges = [x0] + [v for i in range(n) for v in (i * pitch - s, i * pitch + s)] + [x1]
    for xa, xb in zip(edges[0::2], edges[1::2]):
        face(body, [(xa, yr, za), (xb, yr, za), (xb, yr, zc), (xa, yr, zc)], (0, -1, 0))
    yd = yr + p["wire_depth"]
    for i in range(n):
        xa, xb = i * pitch - s, i * pitch + s
        face(dark, [(xa, yr, za), (xb, yr, za), (xb, yd, za), (xa, yd, za)], (0, 0, 1))
        face(dark, [(xa, yr, zc), (xb, yr, zc), (xb, yd, zc), (xa, yd, zc)], (0, 0, -1))
        face(dark, [(xa, yr, za), (xa, yd, za), (xa, yd, zc), (xa, yr, zc)], (1, 0, 0))
        face(dark, [(xb, yr, za), (xb, yd, za), (xb, yd, zc), (xb, yr, zc)], (-1, 0, 0))
        face(metal, [(xa, yd, za), (xb, yd, za), (xb, yd, zc), (xa, yd, zc)], (0, -1, 0))   # the clamp
    return body, metal, dark


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    for name, p in PLUGS.items():
        body, metal, dark = build(p)
        write(os.path.join(OUT_DIR, name + ".wrl"), p["title"],
              [shape("body", body, MAT["body"]), shape("screws and clamps", metal, MAT["metal"]),
               shape("holes", dark, MAT["dark"])])
        print(name, len(body.v) + len(metal.v) + len(dark.v), "vertices")


if __name__ == "__main__":
    main()
