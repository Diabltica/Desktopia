"""Builds the Desktopia.pretty/PluggableHeader_*.kicad_mod footprints of the motherboard's pluggable terminal blocks
(J1 / J5 / J6 / J7): KiCad's Phoenix Contact header footprints (the Kefa KF2EDGRC and KANGNEX 15EDGRC headers copy
them), same pads and outlines, with two 3D models: KiCad's header and the mated plug of pluggable_plug_3d.py.
The plug's outline is drawn on F.Fab: it stands out of the courtyard, past the board edge.

usage: python pluggable_header_fp.py [out dir]
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from pluggable_plug_3d import PLUGS, outline       # noqa: E402

OUT_DIR = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "Desktopia.pretty")
KICAD_FP = os.environ.get("KICAD_FOOTPRINTS", r"C:/Program Files/KiCad/10.0/share/kicad/footprints")
PLUG_MODEL = "${KIPRJMOD}/../kicad_library/pcb/Desktopia.3dshapes/%s.wrl"

FOOTPRINTS = [
    ("PluggableHeader_2EDGRC_1x02_P5.08mm_Horizontal", "Connector_Phoenix_MSTB",
     "PhoenixContact_MSTBA_2,5_2-G-5,08_1x02_P5.08mm_Horizontal", "Plug_2EDGK_1x02_P5.08mm",
     "Pluggable terminal block header, 5.08 mm, 2 positions, right angle, closed ends (Kefa KF2EDGRC-5.08-2P, "
     "Phoenix MSTBA 2,5/2-G-5,08 footprint), with its KF2EDGK-5.08-2P plug in the 3D view",
     "terminal block pluggable 2EDG KF2EDGRC MSTBA 5.08mm plug"),
    ("PluggableHeader_2EDGRC_1x02_P7.62mm_Horizontal", "Connector_Phoenix_GMSTB",
     "PhoenixContact_GMSTBA_2,5_2-G-7,62_1x02_P7.62mm_Horizontal", "Plug_2EDGK_1x02_P7.62mm",
     "Pluggable terminal block header, 7.62 mm, 2 positions, right angle, closed ends (Kefa KF2EDGRC-7.62-2P, "
     "Phoenix GMSTBA 2,5/2-G-7,62 footprint), with its KF2EDGK-7.62-2P plug in the 3D view",
     "terminal block pluggable 2EDG KF2EDGRC GMSTBA 7.62mm plug"),
    ("PluggableHeader_15EDGRC_1x03_P3.81mm_Horizontal", "Connector_Phoenix_MC",
     "PhoenixContact_MC_1,5_3-G-3.81_1x03_P3.81mm_Horizontal", "Plug_15EDGK_1x03_P3.81mm",
     "Pluggable terminal block header, 3.81 mm, 3 positions, right angle, closed ends (KANGNEX WJ15EDGRC-3.81-03P, "
     "Phoenix MC 1,5/3-G-3,81 footprint), with its WJ15EDGK-3.81-03P plug in the 3D view",
     "terminal block pluggable 15EDG WJ15EDGRC MC 3.81mm plug"),
]

MODEL = """	(model "%s"
		(offset
			(xyz 0 0 0)
		)
		(scale
			(xyz 1 1 1)
		)
		(rotate
			(xyz 0 0 0)
		)
	)
"""


def fab_plug(x0, x1, y0, y1):
    """Plug outline on F.Fab (footprint frame: y down) and a label."""
    pts = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
    out = ""
    for (xa, ya), (xb, yb) in zip(pts, pts[1:] + pts[:1]):
        out += ('\t(fp_line\n\t\t(start %.3f %.3f)\n\t\t(end %.3f %.3f)\n\t\t(stroke\n\t\t\t(width 0.1)\n'
                '\t\t\t(type dash)\n\t\t)\n\t\t(layer "F.Fab")\n\t)\n' % (xa, ya, xb, yb))
    out += ('\t(fp_text user "plug"\n\t\t(at %.3f %.3f 0)\n\t\t(layer "F.Fab")\n\t\t(effects\n\t\t\t(font\n'
            '\t\t\t\t(size 1 1)\n\t\t\t\t(thickness 0.15)\n\t\t\t)\n\t\t)\n\t)\n' % ((x0 + x1) / 2, (y0 + y1) / 2))
    return out


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    for name, lib, src, plug, descr, tags in FOOTPRINTS:
        s = open(os.path.join(KICAD_FP, lib + ".pretty", src + ".kicad_mod"), encoding="utf-8").read()
        s = s.replace('(footprint "%s"' % src, '(footprint "%s"' % name, 1)
        s = s.replace('(property "Value" "%s"' % src, '(property "Value" "%s"' % name, 1)
        s = re.sub(r'\(generator "[^"]*"\)', '(generator "pluggable_header_fp.py")', s, count=1)
        s = re.sub(r'\(descr "[^"]*"\)', '(descr "%s")' % descr, s, count=1)
        s = re.sub(r'\(tags "[^"]*"\)', '(tags "%s")' % tags, s, count=1)
        header_model = re.search(r'\t\(model "([^"]+)"', s).group(1)
        s = s[:s.index('\t(model "')].rstrip('\n') + '\n'
        x0, x1, yf, yt, yr, zb, zt = outline(PLUGS[plug])
        s = s[:s.index('\t(embedded_fonts')] + fab_plug(x0, x1, -yf, -yr) + s[s.index('\t(embedded_fonts'):]
        s += MODEL % header_model + MODEL % (PLUG_MODEL % plug) + ")\n"
        with open(os.path.join(OUT_DIR, name + ".kicad_mod"), "w", encoding="utf-8", newline="\n") as f:
            f.write(s)
        print(name, "<-", lib + ":" + src, "+", plug)


if __name__ == "__main__":
    main()
