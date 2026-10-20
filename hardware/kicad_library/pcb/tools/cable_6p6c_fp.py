"""Builds Desktopia.pretty/Cable_6P6C_SolderPads.kicad_mod: the 6 wires of a flat 6P6C cord soldered to the board,
with a cable-tie strain relief, instead of an RJ11 jack (the HMI board sits at 45 deg under the desk edge: the cord
leaves over the board edge and lies flat against the desk, no 12 mm tall jack).

Origin: centre of the pad row. The cord comes from -y (the board edge goes on the dashed Fab line).
usage: python cable_6p6c_fp.py [out.kicad_mod]
"""
import os
import sys
import uuid

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "Desktopia.pretty", "Cable_6P6C_SolderPads.kicad_mod")

PITCH = 2.54
PAD, DRILL = 1.8, 1.0           # 26-28 AWG stranded wire, tinned: <= 0.6 mm
CABLE_W = 6.8                   # flat 6P6C cord: about 6.6 x 3.2 mm
JACKET_END = -3.0               # strip the jacket here (wires: 3-6 mm)
TIE_Y, TIE_X = -5.5, 5.8        # cable-tie slots, either side of the cord
SLOT_W, SLOT_L = 1.5, 3.2       # for a 2.5 x 1.0 mm cable tie
EDGE_Y = -9.0                   # suggested board edge
NAMES = ["+5V", "GND", "A", "B", "GND", "+5V"]
COLOURS = ["WHT", "BLK", "RED", "GRN", "YEL", "BLU"]   # usual flat-cord order (pin 1 -> 6)

xs = [(i - 2.5) * PITCH for i in range(6)]
u = lambda: '(uuid "%s")' % uuid.uuid4()
items = []


def line(a, b, layer, w=0.12, dash=False):
    stroke = "(stroke (width %g) (type %s))" % (w, "dash" if dash else "solid")
    items.append('(fp_line (start %g %g) (end %g %g) %s (layer "%s") %s)' % (a[0], a[1], b[0], b[1], stroke, layer, u()))


def rect(a, b, layer, w=0.12):
    for p, q in (((a[0], a[1]), (b[0], a[1])), ((b[0], a[1]), (b[0], b[1])), ((b[0], b[1]), (a[0], b[1])), ((a[0], b[1]), (a[0], a[1]))):
        line(p, q, layer, w)


def text(s, xy, layer, size=0.8, rot=0, thick=0.12):
    items.append('(fp_text user "%s" (at %g %g %g) (layer "%s") %s (effects (font (size %g %g) (thickness %g))))'
                 % (s, xy[0], xy[1], rot, layer, u(), size, size, thick))


# Pads: pin 1 square
for i, x in enumerate(xs):
    shape = "rect" if i == 0 else "circle"
    items.append('(pad "%d" thru_hole %s (at %g 0) (size %g %g) (drill %g) (layers "*.Cu" "*.Mask") (remove_unused_layers no) %s)'
                 % (i + 1, shape, x, PAD, PAD, DRILL, u()))

# Cable-tie slots (plain oval holes, no copper)
for x in (-TIE_X, TIE_X):
    items.append('(pad "" np_thru_hole oval (at %g %g) (size %g %g) (drill oval %g %g) (layers "*.Cu" "*.Mask") %s)'
                 % (x, TIE_Y, SLOT_W, SLOT_L, SLOT_W, SLOT_L, u()))

# Fab: cord, wires fanning out to the pads, board edge
hw = CABLE_W / 2
rect((-hw, EDGE_Y - 4), (hw, JACKET_END), "F.Fab", 0.1)
for i, x in enumerate(xs):
    wx = (i - 2.5) * (CABLE_W - 1.2) / 5
    line((wx, JACKET_END), (x, -1.0), "F.Fab", 0.1)
    text(COLOURS[i], (x, 4.8), "F.Fab", size=0.5, rot=90, thick=0.08)
line((-9, EDGE_Y), (9, EDGE_Y), "F.Fab", 0.1, dash=True)
text("board edge", (0, EDGE_Y - 0.6), "F.Fab", size=0.6, thick=0.09)
text("cable tie", (0, TIE_Y), "F.Fab", size=0.6, thick=0.09)

# Silkscreen: signal names, cable-tie path, pin 1 mark
for i, x in enumerate(xs):
    text(NAMES[i], (x, 2.3), "F.SilkS", size=0.6, rot=90, thick=0.1)
for y in (TIE_Y - SLOT_L / 2 - 0.3, TIE_Y + SLOT_L / 2 + 0.3):
    line((-TIE_X + SLOT_W / 2 + 0.3, y), (TIE_X - SLOT_W / 2 - 0.3, y), "F.SilkS")
line((xs[0] - 1.4, -1.1), (xs[0] - 1.4, 1.1), "F.SilkS")

# Courtyard
rect((-TIE_X - SLOT_W / 2 - 0.5, EDGE_Y), (TIE_X + SLOT_W / 2 + 0.5, 3.6), "F.CrtYd", 0.05)

name = os.path.splitext(os.path.basename(OUT))[0]
fp = """(footprint "%s"
	(version 20260206)
	(generator "cable_6p6c_fp.py")
	(layer "F.Cu")
	(descr "Flat 6P6C (RJ11 / RJ12) cord soldered to the board: 6 pads at 2.54 mm, cable-tie strain relief (2.5 mm tie). Pin order = cord order; the Desktopia pin-out is symmetric, either cord direction works.")
	(tags "6P6C RJ11 RJ12 cable wire solder strain relief")
	(property "Reference" "REF**" (at 0 %g 0) (layer "F.SilkS") %s (effects (font (size 1 1) (thickness 0.15))))
	(property "Value" "%s" (at 0 7.0 0) (layer "F.Fab") %s (effects (font (size 1 1) (thickness 0.15))))
	(property "Datasheet" "" (at 0 0 0) (layer "F.Fab") (hide yes) %s (effects (font (size 1 1) (thickness 0.15))))
	(property "Description" "" (at 0 0 0) (layer "F.Fab") (hide yes) %s (effects (font (size 1 1) (thickness 0.15))))
	(attr through_hole exclude_from_pos_files exclude_from_bom)
	%s
	(embedded_fonts no)
)
""" % (name, TIE_Y - SLOT_L / 2 - 1.3, u(), name, u(), u(), u(), "\n\t".join(items))
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w", encoding="utf-8", newline="\n").write(fp)
print(OUT)
