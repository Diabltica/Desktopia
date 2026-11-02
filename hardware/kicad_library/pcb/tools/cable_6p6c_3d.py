"""Builds Desktopia.3dshapes/Cable_6P6C_SolderPads.wrl: the flat 6P6C cord soldered to J3 of the HMI board, for the
KiCad 3D view of the Desktopia:Cable_6P6C_SolderPads footprint (same origin: centre of the pad row).

The cord lies against the BOTTOM of the board: its jacket is stripped 3 mm before the pads, the six wires fan out
to the holes, go through the board and are soldered on the top. A black 2.5 mm cable tie goes around the jacket,
through the two slots, and across the top between them; its head is under the cord. The cord stops about 1 cm past
the board edge suggested by the footprint. Board thickness: 1.6 mm.

usage: python cable_6p6c_3d.py [out.wrl]
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from vrml import Mesh, bezier, shape, write       # noqa: E402

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "Desktopia.3dshapes", "Cable_6P6C_SolderPads.wrl")

# Footprint dimensions (cable_6p6c_fp.py), y flipped: the cord comes from +y here (the footprint's -y)
PITCH = 2.54
JACKET_END = 3.0                # the jacket stops here, the wires go on to the pads (y = 0)
TIE_Y, TIE_X = 5.5, 5.8         # cable-tie slots
EDGE_Y = 9.0                    # suggested board edge
BOARD_T = 1.6

# Flat 6P6C cord: 6.6 x 2.4 mm jacket, six 28 AWG wires 1 mm apart (0.9 mm insulation, 0.5 mm tinned copper)
CORD_W, CORD_T, CORD_R = 6.6, 2.4, 0.9
CORD_END = EDGE_Y + 10.0        # about 1 cm of cord past the board
WIRE_PITCH, WIRE_R, CORE_R = 1.0, 0.45, 0.25
TIE_W, TIE_T = 2.5, 1.0         # cable tie band; head 4.0 x 3.5 x 2.6 mm
COLOURS = ["white", "black", "red", "green", "yellow", "blue"]   # usual flat-cord order, pin 1 -> 6

BOTTOM = -BOARD_T               # z of the board's bottom face
CORD_Z = BOTTOM - CORD_T / 2    # cord axis

# Materials: diffuse, specular, shininess, transparency, emissive
plastic = lambda rgb: (rgb, (0.3, 0.3, 0.3), 0.4, 0.0, None)
MAT = {
    "jacket": ((0.62, 0.63, 0.66), (0.35, 0.35, 0.35), 0.35, 0.0, None),     # silver satin
    "white": plastic((0.88, 0.88, 0.86)), "black": plastic((0.05, 0.05, 0.05)), "red": plastic((0.75, 0.06, 0.05)),
    "green": plastic((0.05, 0.50, 0.15)), "yellow": plastic((0.90, 0.75, 0.05)), "blue": plastic((0.06, 0.20, 0.75)),
    "tin": ((0.75, 0.75, 0.78), (0.9, 0.9, 0.9), 0.8, 0.0, None),
    "copper": ((0.72, 0.42, 0.20), (0.6, 0.45, 0.3), 0.6, 0.0, None),
    "nylon": ((0.035, 0.035, 0.04), (0.25, 0.25, 0.25), 0.5, 0.0, None),
}


def rounded_rect(hw, hh, r, n=5):
    """Section (u, v) of a rectangle 2hw x 2hh with rounded corners, counter-clockwise."""
    pts = []
    for cu, cv, a0 in ((hw - r, hh - r, 0), (-hw + r, hh - r, 90), (-hw + r, -hh + r, 180), (hw - r, -hh + r, 270)):
        for k in range(n + 1):
            a = math.radians(a0 + 90 * k / n)
            pts.append((cu + r * math.cos(a), cv + r * math.sin(a)))
    return pts


def chaikin(pts, rounds=4):
    """Rounds the corners of a closed polygon."""
    for _ in range(rounds):
        out = []
        for i, p in enumerate(pts):
            q = pts[(i + 1) % len(pts)]
            out += [tuple(0.75 * a + 0.25 * b for a, b in zip(p, q)), tuple(0.25 * a + 0.75 * b for a, b in zip(p, q))]
        pts = out
    return pts


def disc(mesh, x, y, z, r, seg=16):
    """Disc facing +y."""
    mesh.poly([(x + r * math.cos(-2 * math.pi * k / seg), y, z + r * math.sin(-2 * math.pi * k / seg)) for k in range(seg)])


def build():
    parts = []

    # Jacket from the strip point to the cut end, lying under the board; the six wire ends show on the cut
    jacket = Mesh()
    jacket.sweep([(0, JACKET_END, CORD_Z), (0, CORD_END, CORD_Z)], rounded_rect(CORD_W / 2, CORD_T / 2, CORD_R))
    parts.append(shape("jacket", jacket, MAT["jacket"]))
    xs_cord = [(i - 2.5) * WIRE_PITCH for i in range(6)]
    xs_pad = [(i - 2.5) * PITCH for i in range(6)]
    copper = Mesh()
    for x in xs_cord:
        disc(copper, x, CORD_END + 0.02, CORD_Z, CORE_R)

    # Wires: insulated from the jacket to 1 mm before the pad, then tinned copper through the hole
    tin = Mesh()
    for i, (xc, xp) in enumerate(zip(xs_cord, xs_pad)):
        wire = Mesh()
        low = BOTTOM - WIRE_R                                 # resting against the board
        wire.tube(bezier((xc, JACKET_END + 0.4, CORD_Z), (xc, JACKET_END - 0.7, CORD_Z), (xp, 2.2, low), (xp, 1.1, low), 16), WIRE_R)
        disc(wire, xc, CORD_END + 0.01, CORD_Z, WIRE_R)       # its end on the cut
        parts.append(shape("wire %d %s" % (i + 1, COLOURS[i]), wire, MAT[COLOURS[i]]))
        core = bezier((xp, 1.5, low), (xp, 0.35, low), (xp, 0.0, low), (xp, 0.0, BOTTOM + 0.2), 10) + [(xp, 0.0, 1.0)]
        tin.tube(core, CORE_R, seg=10)
        # Solder: a fillet on the top pad, a smaller one where the wire enters underneath
        tin.revolve(xp, 0.0, [(0.9, 0.035), (0.72, 0.1), (0.55, 0.22), (0.42, 0.38), (0.33, 0.56), (0.28, 0.75)])
        tin.revolve(xp, 0.0, [(0.3, BOTTOM - 0.45), (0.45, BOTTOM - 0.2), (0.75, BOTTOM - 0.035)])
    parts += [shape("tin", tin, MAT["tin"]), shape("copper", copper, MAT["copper"])]

    # Cable tie: around the jacket, up through both slots, across the top; head under the cord
    hug = CORD_W / 2 + 0.3
    loop = chaikin([(-TIE_X, TIE_T / 2), (TIE_X, TIE_T / 2), (TIE_X, BOTTOM - 0.4), (hug, BOTTOM - CORD_T - TIE_T / 2),
                    (-hug, BOTTOM - CORD_T - TIE_T / 2), (-TIE_X, BOTTOM - 0.4)])
    tie = Mesh()
    tie.sweep([(x, TIE_Y, z) for x, z in loop], [(-TIE_W / 2, -TIE_T / 2), (TIE_W / 2, -TIE_T / 2),
                                                (TIE_W / 2, TIE_T / 2), (-TIE_W / 2, TIE_T / 2)], closed=True)
    head_top = BOTTOM - CORD_T - 0.1
    tie.box(-2.0, 2.0, TIE_Y - 1.75, TIE_Y + 1.75, head_top - 2.6, head_top)
    tie.box(2.0, 2.7, TIE_Y - TIE_W / 2, TIE_Y + TIE_W / 2, head_top - 2.2, head_top - 1.2)   # tail, cut short
    parts.append(shape("cable tie", tie, MAT["nylon"]))

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    write(OUT, "Flat 6P6C cord soldered to the HMI board (J3) - generated by hardware/kicad_library/pcb/tools/cable_6p6c_3d.py",
          parts)
    print("%s: %d shapes, %.0f kB" % (OUT, len(parts), os.path.getsize(OUT) / 1024))


build()
