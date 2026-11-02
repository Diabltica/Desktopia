"""IKEA-style assembly drawings of docs/img/ built from the parametric CAD of build_slide_mount.py.
    python steps_images.py [1 2 3 4]
"""
import sys
import cadquery as cq
from ikea import Step, METAL, WOOD, YOURS, PETG, GHOST, TPU, Z, INK
import build_slide_mount as B

ORANGE, BRASS = "#f6c27a", "#e0c25a"
L_SPACING = 80.0                      # placeholder spacing between the two supports


def screw_m3(length=6.0, head_d=5.0, head_h=3.0):
    """M3 cap screw, head at the bottom, shank pointing +Z"""
    return cq.Workplane("XY").circle(head_d / 2).extrude(head_h).union(cq.Workplane("XY").workplane(offset=head_h).circle(1.5).extrude(length))


def wood_screw(length=25):
    return cq.Workplane("XY").circle(3.0).extrude(1.6).union(cq.Workplane("XY").workplane(offset=1.6).circle(1.5).extrude(length - 1.6))


def insert_m3():
    return cq.Workplane("XY").circle(2.1).circle(1.3).extrude(4.2)


def flip(wp):
    return wp.rotate((0, 0, 0), (1, 0, 0), 180)


def plate_at(xc, dz=0.0):
    return B.clip_plate().translate((xc - B.XC, 0, B.Z_PLATE_BOT + dz))


def plot_at(xc, y, dz=0.0):
    return B.plot().translate((xc, y, B.PLANK_Z - B.SH_LEN - B.HEAD_H + dz))


def cushions_at(xc, y, dz=0.0):
    """the two TPU cushions of a plot in their final position (top one against the plank, bottom one on the head)"""
    c = B.cushion()
    return [c.translate((xc, y, B.PLANK_Z - B.CUSH_T + dz)), c.translate((xc, y, B.Z_PLATE_BOT - B.GAP - B.CUSH_T + dz))]


STEPS = {}


def step(n):
    def deco(f):
        STEPS[n] = f
        return f
    return deco


@step(1)
def s1():
    s = Step(1, cam=(1.0, -1.15, 0.85))
    s.add(B.ring_support("rear"), YOURS)
    for ys in B.SCREW_Y:
        s.add(insert_m3().translate((0, ys, B.Z_PLATE_BOT + 12)), BRASS)
        s.guide((0, ys, B.Z_PLATE_BOT + 12), (0, ys, B.Z_PLATE_BOT - 3.0))
    s.arrow((16, 8, B.Z_PLATE_BOT + 22), (16, 8, B.Z_PLATE_BOT + 8))
    s.qty((0, 0, B.Z_PLATE_BOT + 22), 2, dx=-4, dy=14)
    s.text((0, 22, B.Z_PLATE_BOT), "short heat-set\nM3 inserts", dx=10, dy=-8, fs=8.5, bold=False)
    s.save("step_1_inserts.png")


@step(2)
def s2():
    s = Step(2, cam=(1.0, -1.15, 0.85))
    s.add(B.ring_support("rear"), YOURS)
    s.add(plate_at(0.0, 16.0), ORANGE)
    for ys in B.SCREW_Y:
        s.add(flip(screw_m3(5.0)).translate((0, ys, B.Z_PLATE_BOT + 16 + 6.0 + 20)), METAL)
        s.guide((0, ys, B.Z_PLATE_BOT + 16 + 6.0 + 17), (0, ys, B.Z_PLATE_BOT - 3))
    s.qty((0, 0, B.Z_PLATE_BOT + 16 + 6.0 + 22), 2, dx=-4, dy=18)
    s.text((7.5, -44, B.Z_PLATE_BOT + 19), "mouths toward\nthe coupler (+x)", dx=-8, dy=-22, fs=8.5, bold=False)
    s.save("step_2_plate_on_ring.png")


@step(3)
def s3():
    s = Step(3, cam=(1.0, -1.15, 0.85))
    top = B.SH_LEN + B.HEAD_H
    for k, xc in enumerate((0.0, 26.0)):
        s.add(B.plot().translate((xc, 0, 0)), PETG)
        s.add(B.cushion().translate((xc, 0, top + 24)), TPU)
        s.add(B.cushion().translate((xc, 0, top + 12)), TPU)
        s.guide((xc, 0, top + 24), (xc, 0, B.HEAD_H + 0.2))
    s.arrow((13, -14, top + 30), (13, -14, top + 10))
    s.qty((0, 0, top + 30), 2, dx=-34, dy=0)
    s.text((13, 0, B.HEAD_H), "head side:\ncushion on the head", dx=0, dy=-26, fs=8.5, bold=False)
    s.text((13, 0, top + 12), "friction fit on\nthe \u00d86.0 shoulder", dx=52, dy=6, fs=8.5, bold=False)
    s.save("step_3_cushions_on_plots.png")


@step(4)
def s4():
    s = Step(4, cam=(1.0, -1.15, -0.55))
    plank = cq.Workplane("XY").box(110, 100, 16, centered=False).translate((-22, -52, B.PLANK_Z))
    s.add(plank, WOOD)
    for xc in (0.0, L_SPACING):
        for y in (B.Y_POST_EAR, B.Y_POST_WOOD):
            s.add(plot_at(xc, y, -18), PETG)
            for c in cushions_at(xc, y, -18):
                s.add(c, TPU)
            s.add(wood_screw(25).translate((xc, y, B.PLANK_Z - B.SH_LEN - B.HEAD_H - 18 - 16)), METAL)
            s.guide((xc, y, B.PLANK_Z - B.SH_LEN - B.HEAD_H - 18 - 16 + 25), (xc, y, B.PLANK_Z + 0.5))
    s.qty((0, -37, -26), 4, dx=-30, dy=0)
    s.text((L_SPACING / 2, -37, -30), "same pitch (56) on both supports", dx=20, dy=-22, fs=8.5, bold=False)
    s.save("step_4_plots_in_plank.png")


@step(5)
def s5(cam=(1.0, -0.95, -0.28), name="step_5_slide_in.png"):
    s = Step(5, cam=cam)
    plank = cq.Workplane("XY").box(150, 96, 16, centered=False).translate((-70, -48, B.PLANK_Z))
    wood = cq.Workplane("XY").box(150, 8, 56, centered=False).translate((-70, B.WOOD_Y, -20))
    s.add(plank, WOOD)
    s.add(wood, WOOD)
    for xc in (0.0, L_SPACING):
        for y in (B.Y_POST_EAR, B.Y_POST_WOOD):
            s.add(plot_at(xc, y), PETG)
            for c in cushions_at(xc, y):
                s.add(c, TPU)
    for xc, kind in ((0.0, "rear"), (L_SPACING, "front")):
        x0 = xc - 32.0
        s.add(B.ring_support(kind).translate((x0, 0, 0)), YOURS)
        s.add(plate_at(x0), ORANGE)
    s.arrow((-48, 0, -4), (-14, 0, -4), lw=3.0)
    s.text((-48, 0, -4), "slide toward\nthe coupler", dx=-6, dy=-24, fs=10)
    s.text((L_SPACING, 0, B.PLANK_Z + 16), "4 clicks", dx=0, dy=34, fs=12, box=True)
    s.save(name)


if __name__ == "__main__":
    todo = [int(a) for a in sys.argv[1:]] or sorted(STEPS)
    for n in todo:
        STEPS[n]()
