"""Small VRML 2.0 writer for the KiCad 3D models of this library (round_lcd_3d.py, cable_6p6c_3d.py).

Coordinates in mm, in the footprint's frame: x right, y towards the top of the board view (the footprint's -y),
z out of the board's top face. KiCad reads VRML in 0.1 inch and applies its own scale on top of a Transform node,
so the files have none.
"""
import math


class Mesh:
    def __init__(self):
        self.v, self.f = [], []

    def quad(self, a, b, c, d):
        i = len(self.v)
        self.v += [a, b, c, d]
        self.f.append((i, i + 1, i + 2, i + 3))

    def poly(self, pts):
        i = len(self.v)
        self.v += pts
        self.f.append(tuple(range(i, i + len(pts))))

    def box(self, x0, x1, y0, y1, z0, z1):
        p = [(x, y, z) for z in (z0, z1) for y in (y0, y1) for x in (x0, x1)]
        for a, b, c, d in ((0, 2, 3, 1), (4, 5, 7, 6), (0, 1, 5, 4), (2, 6, 7, 3), (0, 4, 6, 2), (1, 3, 7, 5)):
            self.quad(p[a], p[b], p[c], p[d])

    def cylinder(self, cx, cy, r, z0, z1, top=True, bottom=True, seg=96):
        ring = [(cx + r * math.cos(2 * math.pi * k / seg), cy + r * math.sin(2 * math.pi * k / seg)) for k in range(seg)]
        for k in range(seg):
            (xa, ya), (xb, yb) = ring[k], ring[(k + 1) % seg]
            self.quad((xa, ya, z0), (xb, yb, z0), (xb, yb, z1), (xa, ya, z1))
        if top:
            self.poly([(x, y, z1) for x, y in ring])
        if bottom:
            self.poly([(x, y, z0) for x, y in reversed(ring)])

    def annulus(self, cx, cy, r0, r1, z, seg=96):
        for k in range(seg):
            a, b = 2 * math.pi * k / seg, 2 * math.pi * (k + 1) / seg
            self.quad((cx + r0 * math.cos(a), cy + r0 * math.sin(a), z), (cx + r1 * math.cos(a), cy + r1 * math.sin(a), z),
                      (cx + r1 * math.cos(b), cy + r1 * math.sin(b), z), (cx + r0 * math.cos(b), cy + r0 * math.sin(b), z))

    def revolve(self, cx, cy, profile, seg=24):
        """Surface of revolution around the vertical axis through (cx, cy): profile = [(radius, z), ...]."""
        for (r0, z0), (r1, z1) in zip(profile, profile[1:]):
            for k in range(seg):
                a, b = 2 * math.pi * k / seg, 2 * math.pi * (k + 1) / seg
                self.quad((cx + r0 * math.cos(a), cy + r0 * math.sin(a), z0), (cx + r0 * math.cos(b), cy + r0 * math.sin(b), z0),
                          (cx + r1 * math.cos(b), cy + r1 * math.sin(b), z1), (cx + r1 * math.cos(a), cy + r1 * math.sin(a), z1))

    def sweep(self, path, section, closed=False, caps=True):
        """Extrudes a 2D section [(u, v), ...] (u, v along the path's two normals) along a 3D path. Rotation-minimising
        frames, so a bent wire does not twist."""
        tangents = []
        for i in range(len(path)):
            a = path[i - 1] if (i > 0 or closed) else path[i]
            b = path[(i + 1) % len(path)] if (i < len(path) - 1 or closed) else path[i]
            tangents.append(_unit(_sub(b, a)))
        t0 = tangents[0]
        n = _unit(_cross(t0, (0, 0, 1) if abs(t0[2]) < 0.9 else (1, 0, 0)))
        rings = []
        for p, t in zip(path, tangents):
            n = _unit(_sub(n, _scale(t, _dot(n, t))))                # carry the previous normal over
            b = _cross(t, n)
            rings.append([_add(p, _add(_scale(n, u), _scale(b, v))) for u, v in section])
        m = len(section)
        for i in range(len(rings) if closed else len(rings) - 1):
            ra, rb = rings[i], rings[(i + 1) % len(rings)]
            for k in range(m):
                self.quad(ra[k], ra[(k + 1) % m], rb[(k + 1) % m], rb[k])
        if caps and not closed:
            self.poly(list(reversed(rings[0])))
            self.poly(rings[-1])

    def tube(self, path, r, seg=12, caps=True):
        self.sweep(path, [(r * math.cos(2 * math.pi * k / seg), r * math.sin(2 * math.pi * k / seg)) for k in range(seg)],
                   caps=caps)


def bezier(p0, p1, p2, p3, n=12):
    """n + 1 points of the cubic Bezier curve p0 .. p3."""
    pts = []
    for i in range(n + 1):
        t = i / n
        w = ((1 - t) ** 3, 3 * t * (1 - t) ** 2, 3 * t * t * (1 - t), t ** 3)
        pts.append(tuple(sum(wk * pk[j] for wk, pk in zip(w, (p0, p1, p2, p3))) for j in range(3)))
    return pts


def shape(name, mesh, mat):
    """mat = (diffuse, specular, shininess, transparency, emissive or None)."""
    dif, spec, shin, transp, emis = mat
    f3 = lambda c: "%.4g %.4g %.4g" % c
    m = "diffuseColor %s specularColor %s shininess %.3g transparency %.3g ambientIntensity 0.3" % (f3(dif), f3(spec), shin, transp)
    if emis:
        m += " emissiveColor %s" % f3(emis)
    pts = ", ".join("%.5f %.5f %.5f" % tuple(c / 2.54 for c in p) for p in mesh.v)   # KiCad VRML unit: 0.1 inch
    idx = ", ".join(", ".join(map(str, f)) + ", -1" for f in mesh.f)
    return ("Shape { # %s\n appearance Appearance { material Material { %s } }\n"
            " geometry IndexedFaceSet { creaseAngle 0.5 coord Coordinate { point [ %s ] }\n coordIndex [ %s ] } }\n"
            % (name, m, pts, idx))


def write(path, title, parts):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("#VRML V2.0 utf8\n# %s\n" % title)
        f.write("".join(parts))


def _add(a, b): return tuple(x + y for x, y in zip(a, b))
def _sub(a, b): return tuple(x - y for x, y in zip(a, b))
def _scale(a, k): return tuple(x * k for x in a)
def _dot(a, b): return sum(x * y for x, y in zip(a, b))
def _cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def _unit(a):
    n = math.sqrt(_dot(a, a)) or 1.0
    return _scale(a, 1 / n)
