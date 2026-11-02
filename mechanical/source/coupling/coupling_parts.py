"""Jaw coupling of the desk drive (the shaft path of the isolation): hubs A and B in PETG, spider in TPU. CadQuery, all dimensions in mm.
Every part is modelled in its print orientation (Z up, seating face on the bed). Same geometry as the desk-drive-isolation repository.
Run `python build_coupling.py` to regenerate the STEP / STL files of the coupling.
"""
import math
import cadquery as cq


def P(r, a):
    """polar -> cartesian (angle in degrees, counter-clockwise from +X)"""
    return (r * math.cos(math.radians(a)), r * math.sin(math.radians(a)))


def sector(r_in, r_out, a0, a1, z0, h):
    """annular sector from angle a0 to a1, extruded from z0 by h"""
    am = (a0 + a1) / 2
    return (cq.Workplane("XY").workplane(offset=z0)
            .moveTo(*P(r_out, a0)).threePointArc(P(r_out, am), P(r_out, a1))
            .lineTo(*P(r_in, a1)).threePointArc(P(r_in, am), P(r_in, a0)).close().extrude(h))


# ----------------------------------------------------------------------------- jaw coupling
D_OUT = 33.0                 # hub / spider outer diameter
BASE = 6.0                   # hub base thickness
CLAW_H = 7.0                 # jaw height
CLAW_RIN = 12.5              # jaw inner radius
CLAW_ANG = 32.0              # jaw angular width
LOBE_RIN = 12.0              # spider lobe inner radius
SPIDER_H = 8.5               # spider height
WEB_T = 2.0                  # spider web ring thickness
WEB_ID = 10.0                # spider web ring bore
PCD = 16.0                   # bolt circle of drillAdapterV3 and of the flange coupler
RECESS_D = 22.2              # centering recess (mating parts are Ø22)
RECESS_H = 1.0
INS_D = 4.0                  # heat-set insert pocket (hub B)
INS_DEPTH = 4.2
THRU_D = 3.4                 # M3 clearance (hub A)
CENTER_D = 9.0               # hub B central hole (shaft end)
HEAD_D = 5.0                 # M3 screw head used here: Ø5 x 3
HEAD_H = 3.0
CB_D = 5.8                   # counterbore for the head (hub A)
CB_DEPTH = 3.2

# --- the spider is a plain PRISM (every wall vertical when it is printed flat): no overhang, no support, no small feature.
#     It stays on the hubs by a light friction fit: the lobes are slightly wider than the 28 deg gaps between the jaws.
TUBE_RIN = 10.5              # spider tube inner radius  (wall 1.6 = 4 perimeters)
TUBE_ROUT = 12.1             # spider tube outer radius
LOBE_ANG_NOMINAL = 28.0      # width of the gaps between the jaws (60 - 32)
PRELOAD = 0.1                # mm of squeeze per flank at r = 14.25 (0.8 deg in total): friction retention, tune 0.0 ... 0.2
LEAD_IN = 0.6                # 45 deg chamfer on the two top flank edges of every jaw (eases the entry of the lobes)


def hub(kind):
    """kind 'A' = drill side (screws into the drillAdapterV3 inserts), 'B' = flange side (heat-set inserts).
    Both hubs: 3 plain jaws (32 deg, r 12.5 -> 16.5, 7 high) with a 0.6 mm lead-in chamfer on their top flank edges."""
    R = D_OUT / 2
    body = cq.Workplane("XY").circle(R).extrude(BASE)
    centers = [90, 210, 330] if kind == "A" else [30, 150, 270]
    for c in centers:
        body = body.union(sector(CLAW_RIN, R, c - CLAW_ANG / 2, c + CLAW_ANG / 2, BASE, CLAW_H))
    body = body.cut(cq.Workplane("XY").circle(RECESS_D / 2).extrude(RECESS_H))
    pts = [P(PCD / 2, a) for a in (0, 90, 180, 270)]
    if kind == "A":
        body = body.cut(cq.Workplane("XY").pushPoints(pts).circle(THRU_D / 2).extrude(BASE))
        body = body.cut(cq.Workplane("XY").workplane(offset=BASE - CB_DEPTH).pushPoints(pts).circle(CB_D / 2).extrude(CB_DEPTH))
    else:
        body = body.cut(cq.Workplane("XY").workplane(offset=RECESS_H).pushPoints(pts).circle(INS_D / 2).extrude(INS_DEPTH))
        body = body.cut(cq.Workplane("XY").circle(CENTER_D / 2).extrude(BASE))
    # lead-in: 45 deg chamfer on the two top flank edges of every jaw (the jaws are plain: nothing to print but vertical walls)
    zt = BASE + CLAW_H
    for c in centers:
        for sign in (+1, -1):
            tri = [(0.05, zt + 0.05), (-(LEAD_IN + 0.05), zt + 0.05), (0.05, zt - LEAD_IN - 0.05)]
            tri = [(sign * y, z) for (y, z) in tri]                           # interior of the jaw is toward -y for sign = +1
            prism = cq.Workplane("YZ").workplane(offset=CLAW_RIN - 0.2).polyline(tri).close().extrude(D_OUT / 2 - CLAW_RIN + 0.6)
            body = body.cut(prism.rotate((0, 0, 0), (0, 0, 1), c + sign * CLAW_ANG / 2))
    return body


def hub_a():
    return hub("A")


def hub_b():
    return hub("B")


def spider(preload=PRELOAD):
    """TPU spider: 6 lobes on a thin tube (wall 1.6). A pure prism: print it flat (axis vertical), no overhang, no support.
    Each lobe is wider than its 28 deg gap by 2 x preload (mm at r = 14.25): a light friction fit that keeps it on the hubs."""
    R = D_OUT / 2
    ang = LOBE_ANG_NOMINAL + math.degrees(2 * preload / 14.25)
    s = cq.Workplane("XY").circle(TUBE_ROUT).circle(TUBE_RIN).extrude(SPIDER_H)
    for c in (0, 60, 120, 180, 240, 300):
        s = s.union(sector(LOBE_RIN, R, c - ang / 2, c + ang / 2, 0, SPIDER_H))
    return s


# ----------------------------------------------------------------------------- coupling assembly (check model)
def coupler_assembly(with_hardware=True):
    """hub B (bottom) + spider + hub A (flipped, top). Returns (cq.Assembly, A, B, S) in assembled position."""
    A = hub("A").rotate((0, 0, 0), (0, 1, 0), 180).translate((0, 0, BASE * 2 + SPIDER_H))
    B = hub("B")
    S = spider().translate((0, 0, BASE))
    asm = cq.Assembly(name="coupler")
    asm.add(B, name="hub_b_flange_side", color=cq.Color(0.55, 0.55, 0.62))
    asm.add(S, name="spider_tpu", color=cq.Color(0.1, 0.7, 0.55))
    asm.add(A, name="hub_a_drill_side", color=cq.Color(0.8, 0.8, 0.8))
    if with_hardware:
        pts = [P(PCD / 2, a) for a in (0, 90, 180, 270)]
        z_floor = BASE + SPIDER_H + CB_DEPTH            # counterbore floor of hub A (assembled frame)
        head = cq.Workplane("XY").workplane(offset=z_floor - HEAD_H).pushPoints(pts).circle(HEAD_D / 2).extrude(HEAD_H)
        shank = cq.Workplane("XY").workplane(offset=z_floor).pushPoints(pts).circle(1.49).extrude(8.0)
        asm.add(head.union(shank), name="screws_M3x8_hub_a", color=cq.Color(0.25, 0.25, 0.25))
        z_end = BASE * 2 + SPIDER_H - RECESS_H
        adapter = cq.Workplane("XY").workplane(offset=z_end).circle(11.0).extrude(10.0)
        adapter = adapter.cut(cq.Workplane("XY").workplane(offset=z_end).pushPoints(pts).circle(2.0).extrude(10.0))
        adapter = adapter.cut(cq.Workplane("XY").workplane(offset=z_end).circle(4.5).extrude(4.7))
        asm.add(adapter, name="drill_adapter_excerpt_for_checking_only", color=cq.Color(0.45, 0.55, 0.7))
    return asm, A, B, S
