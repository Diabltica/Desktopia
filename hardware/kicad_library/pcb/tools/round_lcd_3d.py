"""Builds JLCImport.3dshapes/RoundLCD.wrl: the 1.28" GC9A01 round LCD module, with materials, for the KiCad 3D view.

KiCad cannot map image textures on 3D models, so the screen content is made of geometry: every lit pixel of a
240 x 240 screenshot of the real UI (written by firmware/sim/run.sh) becomes a small self-lit quad (one run of
same-coloured pixels per quad). Same outline, origin and height as RoundLCD.step, which stays for the STEP export
(kicad-cli pcb export step --subst-models picks the .step of the same name).

usage: python round_lcd_3d.py [screenshot.ppm] [out.wrl]
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", "..", ".."))
SHOT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "firmware", "sim", "shots", "02_moving_up.ppm")
OUT = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "..", "JLCImport.3dshapes", "RoundLCD.wrl")

# Module dimensions (mm), same frame as RoundLCD.step: z = 0 on the top of the module PCB, header tab towards -y
PCB_R, PCB_T = 19.0, 1.6
TAB_X, TAB_Y0, TAB_Y1 = 11.96, -26.47, -14.77
PANEL_R, PANEL_Z0, PANEL_Z1 = 17.8, 0.0, 1.45      # display panel: backlight frame + glass
ACTIVE_R = 16.2                                     # 1.28" active area: 240 px over 32.4 mm
GLASS_T = 0.15
PINS_X = [(i - 3) * 2.54 for i in range(7)]
PINS_Y = -24.71
SCREEN_ROT = 90                                     # screen content rotation (degrees, counter-clockwise)
SEG = 96

# Materials: diffuse, specular, shininess, transparency, emissive
MAT = {
    "pcb":      ((0.03, 0.12, 0.42), (0.25, 0.25, 0.3), 0.35, 0.0, None),
    "fr4":      ((0.55, 0.48, 0.30), (0.1, 0.1, 0.1), 0.1, 0.0, None),
    "gold":     ((0.80, 0.62, 0.22), (0.9, 0.8, 0.5), 0.8, 0.0, None),
    "tin":      ((0.75, 0.75, 0.78), (0.9, 0.9, 0.9), 0.8, 0.0, None),
    "frame":    ((0.82, 0.82, 0.80), (0.4, 0.4, 0.4), 0.4, 0.0, None),
    "bezel":    ((0.008, 0.008, 0.01), (0.35, 0.35, 0.35), 0.9, 0.0, None),
    "screen":   ((0.03, 0.035, 0.045), (0.12, 0.12, 0.12), 0.6, 0.0, None),
    "glass":    ((0.0, 0.0, 0.0), (0.3, 0.3, 0.32), 0.95, 0.93, None),
    "fpc":      ((0.72, 0.42, 0.06), (0.5, 0.4, 0.2), 0.6, 0.15, None),
    "plastic":  ((0.03, 0.03, 0.03), (0.15, 0.15, 0.15), 0.2, 0.0, None),
    "ic":       ((0.06, 0.06, 0.06), (0.2, 0.2, 0.2), 0.3, 0.0, None),
    "ceramic":  ((0.55, 0.45, 0.30), (0.2, 0.2, 0.2), 0.3, 0.0, None),
    "resistor": ((0.04, 0.04, 0.04), (0.1, 0.1, 0.1), 0.2, 0.0, None),
}


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

    def cylinder(self, cx, cy, r, z0, z1, top=True, bottom=True, seg=SEG):
        ring = [(cx + r * math.cos(2 * math.pi * k / seg), cy + r * math.sin(2 * math.pi * k / seg)) for k in range(seg)]
        for k in range(seg):
            (xa, ya), (xb, yb) = ring[k], ring[(k + 1) % seg]
            self.quad((xa, ya, z0), (xb, yb, z0), (xb, yb, z1), (xa, ya, z1))
        if top:
            self.poly([(x, y, z1) for x, y in ring])
        if bottom:
            self.poly([(x, y, z0) for x, y in reversed(ring)])

    def annulus(self, cx, cy, r0, r1, z, seg=SEG):
        for k in range(seg):
            a, b = 2 * math.pi * k / seg, 2 * math.pi * (k + 1) / seg
            self.quad((cx + r0 * math.cos(a), cy + r0 * math.sin(a), z), (cx + r1 * math.cos(a), cy + r1 * math.sin(a), z),
                      (cx + r1 * math.cos(b), cy + r1 * math.sin(b), z), (cx + r0 * math.cos(b), cy + r0 * math.sin(b), z))


def shape(name, mesh, mat=None):
    dif, spec, shin, transp, emis = mat or MAT[name]
    f3 = lambda c: "%.4g %.4g %.4g" % c
    m = "diffuseColor %s specularColor %s shininess %.3g transparency %.3g ambientIntensity 0.3" % (f3(dif), f3(spec), shin, transp)
    if emis:
        m += " emissiveColor %s" % f3(emis)
    pts = ", ".join("%.5f %.5f %.5f" % tuple(c / 2.54 for c in p) for p in mesh.v)   # KiCad VRML unit: 0.1 inch
    idx = ", ".join(", ".join(map(str, f)) + ", -1" for f in mesh.f)
    return ("Shape { # %s\n appearance Appearance { material Material { %s } }\n"
            " geometry IndexedFaceSet { creaseAngle 0.5 coord Coordinate { point [ %s ] }\n coordIndex [ %s ] } }\n"
            % (name, m, pts, idx))


def read_ppm(path):
    magic, size, maxval, px = open(path, "rb").read().split(b"\n", 3)   # simulator format: "P6\nW H\n255\n" + RGB
    assert magic == b"P6" and maxval == b"255", path
    w, h = map(int, size.split())
    return w, h, px[: w * h * 3]


def screen_shapes():
    w, h, px = read_ppm(SHOT)
    p = 2 * ACTIVE_R / w
    z = PANEL_Z1 + 0.003
    rot = math.radians(SCREEN_ROT)
    cr, sr = math.cos(rot), math.sin(rot)
    tf = lambda x, y: (x * cr - y * sr, x * sr + y * cr, z)
    meshes = {}
    for row in range(h):
        col = 0
        while col < w:
            c = px[(row * w + col) * 3: (row * w + col) * 3 + 3]
            run = col + 1
            while run < w and px[(row * w + run) * 3: (row * w + run) * 3 + 3] == c:
                run += 1
            x0, x1 = (col - w / 2) * p, (run - w / 2) * p
            y1, y0 = (h / 2 - row) * p, (h / 2 - row - 1) * p
            if max(c) > 60 and math.hypot((x0 + x1) / 2, (y0 + y1) / 2) < ACTIVE_R:   # lit pixel inside the round panel
                meshes.setdefault(bytes(c), Mesh()).quad(tf(x0, y0), tf(x1, y0), tf(x1, y1), tf(x0, y1))
            col = run
    out = []
    for c, m in sorted(meshes.items()):
        rgb = tuple(v / 255 for v in c)
        out.append(shape("pixels %02x%02x%02x" % tuple(c), m,
                         (tuple(v * 0.25 for v in rgb), (0.0, 0.0, 0.0), 0.1, 0.0, tuple(v * 0.95 for v in rgb))))
    return out


def build():
    parts = []
    # Module PCB: round board + header tab, blue solder mask, FR4 edge
    pcb, edge = Mesh(), Mesh()
    pcb.poly([(PCB_R * math.cos(2 * math.pi * k / SEG), PCB_R * math.sin(2 * math.pi * k / SEG), 0.0) for k in range(SEG)])
    pcb.poly([(PCB_R * math.cos(2 * math.pi * k / SEG), PCB_R * math.sin(2 * math.pi * k / SEG), -PCB_T)
              for k in reversed(range(SEG))])
    edge.cylinder(0, 0, PCB_R, -PCB_T, 0.0, top=False, bottom=False)
    pcb.box(-TAB_X, TAB_X, TAB_Y0, TAB_Y1, -PCB_T, 0.0)
    parts += [shape("pcb", pcb), shape("fr4", edge)]

    # Header: gold pads on both sides, black plastic housing under the tab, tinned pins with a solder fillet on top
    gold, tin, plastic = Mesh(), Mesh(), Mesh()
    for x in PINS_X:
        gold.annulus(x, PINS_Y, 0.45, 0.85, 0.004)
        gold.annulus(x, PINS_Y, 0.45, 0.85, -PCB_T - 0.004)
        tin.cylinder(x, PINS_Y, 0.55, 0.0, 0.35, seg=24)
        tin.box(x - 0.32, x + 0.32, PINS_Y - 0.32, PINS_Y + 0.32, -4.6, -PCB_T)
    plastic.box(-8.89, 8.89, PINS_Y - 1.27, PINS_Y + 1.27, -2.6, -PCB_T - 0.001)
    parts += [shape("gold", gold), shape("tin", tin), shape("plastic", plastic)]

    # Parts on the back of the module: LDO, level shifter, decoupling capacitors, resistors
    ic, cer, res = Mesh(), Mesh(), Mesh()
    ic.box(-1.45, 1.45, -9.65, -8.35, -PCB_T - 1.0, -PCB_T)                     # SOT-23 LDO
    ic.box(4.0, 7.0, -4.0, 1.0, -PCB_T - 0.9, -PCB_T)                           # driver IC
    for x, y in ((-5.0, -9.0), (-5.0, -6.5), (2.5, -10.5), (-8.0, 2.0)):
        cer.box(x - 0.8, x + 0.8, y - 0.4, y + 0.4, -PCB_T - 0.8, -PCB_T)
    for x, y in ((-8.0, -2.0), (-8.0, -4.0), (9.5, -8.0), (0.0, 6.0)):
        res.box(x - 0.8, x + 0.8, y - 0.4, y + 0.4, -PCB_T - 0.45, -PCB_T)
    parts += [shape("ic", ic), shape("ceramic", cer), shape("resistor", res)]

    # Display panel: light backlight frame, black bezel ring and the dark active area, glass on top
    frame, bezel, screen, glass = Mesh(), Mesh(), Mesh(), Mesh()
    frame.cylinder(0, 0, PANEL_R, PANEL_Z0 + 0.05, PANEL_Z1 - 0.2, top=False)
    bezel.cylinder(0, 0, PANEL_R - 0.05, PANEL_Z1 - 0.2, PANEL_Z1, top=False, bottom=False)
    bezel.annulus(0, 0, ACTIVE_R, PANEL_R - 0.05, PANEL_Z1)
    screen.poly([(ACTIVE_R * math.cos(2 * math.pi * k / SEG), ACTIVE_R * math.sin(2 * math.pi * k / SEG), PANEL_Z1)
                 for k in range(SEG)])
    glass.cylinder(0, 0, PANEL_R - 0.05, PANEL_Z1 + 0.01, PANEL_Z1 + GLASS_T)
    parts += [shape("frame", frame), shape("bezel", bezel), shape("screen", screen)]
    parts += screen_shapes()

    # Display flex cable, folded from the panel edge down onto the PCB towards the header
    fpc = Mesh()
    fpc.box(-4.5, 4.5, -PANEL_R - 0.15, -PANEL_R + 0.3, PANEL_Z0 + 0.02, PANEL_Z1 - 0.25)
    fpc.box(-4.5, 4.5, -PANEL_R - 1.6, -PANEL_R + 0.3, PANEL_Z0 + 0.01, PANEL_Z0 + 0.13)
    parts.append(shape("fpc", fpc))
    parts.append(shape("glass", glass))       # last: transparent

    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("#VRML V2.0 utf8\n# GC9A01 1.28in round LCD module - generated by hardware/kicad_library/pcb/tools/round_lcd_3d.py\n")
        f.write("".join(parts))                   # no Transform node: KiCad applies its scale twice
    print("%s: %d shapes, %.0f kB" % (OUT, len(parts), os.path.getsize(OUT) / 1024))


build()
