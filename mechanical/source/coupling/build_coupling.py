"""Exports the jaw coupling (shaft path) into ../../step and ../../stl.
    python build_coupling.py
"""
import os
import cadquery as cq
from coupling_parts import *
from flange_coupler import flange_coupler

OUT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
STEP, STL = os.path.join(OUT, "step"), os.path.join(OUT, "stl")


def export(obj, name, stl=True):
    cq.exporters.export(obj, os.path.join(STEP, name + ".step"))
    if stl:
        cq.exporters.export(obj, os.path.join(STL, name + ".stl"), tolerance=0.01, angularTolerance=0.1)
    print("  ", name)


if __name__ == "__main__":
    print("jaw coupling")
    export(hub_a(), "hub_a_drill_side", stl=False)   # STEP only: merged with the drill adapter in ../../solidworks (drill hub)
    export(hub_b(), "hub_b_flange_side")
    export(spider(), "spider_tpu")
    export(flange_coupler(), "flange_coupler_6.35mm", stl=False)
    asm, A, B, S = coupler_assembly(with_hardware=True)
    asm.save(os.path.join(STEP, "assembly_coupler.step"))
    # flange coupler seated in hub B with its 4 M3 x 6 screws and the desk shaft: fit check
    cp = flange_coupler().rotate((0, 0, 0), (1, 0, 0), 180).translate((0, 0, RECESS_H))
    pts = [P(PCD / 2, a) for a in (0, 90, 180, 270)]
    head = cq.Workplane("XY").workplane(offset=-1.0 - HEAD_H).pushPoints(pts).circle(HEAD_D / 2).extrude(HEAD_H)
    shank = cq.Workplane("XY").workplane(offset=-1.0).pushPoints(pts).circle(1.49).extrude(6.0)
    shaft = cq.Workplane("XY").workplane(offset=-40).circle(3.175).extrude(43)
    a2 = cq.Assembly(name="coupler_on_hub_b")
    a2.add(hub_b(), name="hub_b_flange_side", color=cq.Color(0.55, 0.55, 0.62))
    a2.add(cp, name="flange_coupler", color=cq.Color(0.8, 0.8, 0.82))
    a2.add(head.union(shank), name="screws_M3x6", color=cq.Color(0.25, 0.25, 0.25))
    a2.add(shaft, name="desk_shaft_6.35", color=cq.Color(0.9, 0.65, 0.2))
    a2.save(os.path.join(STEP, "assembly_coupler_on_hub_b.step"))
    print("done")
