"""Drilling jig for the four plots: a light frame held against the plank underside, with a drill guide on each plot position.

Frame of the jig = desk frame seen from below: x = motor axis, rows of holes at x = 0 and x = AXIAL_SPACING (the two ring centres),
y = toward the vertical wood (holes at y = -37 and +19, wood face at y = 27), z = 0 the face against the plank, guides toward -z in use.
Modelled in print orientation: the plank face on the bed, guides and lips up. Symmetric end to end, so it has no wrong way along x.

  lips      two short walls against the vertical wood face (y = 27): they set y and keep the rows square to the wood. Each lip is
            15 long, the width of a clip plate: its ends are the plate edges, to line up with marks taken from the motor (README).
  guides    tubes for the dia 2.0-2.2 pilot drill, GUIDE_H high: set the depth stop at 14 + GUIDE_H from the tip.
  frame     4 x 1.6 bars only (a closed rectangle through the four guides, it keeps the far guides at the right spacing): about 3 g.

    python build_drill_jig.py
"""
import cadquery as cq
import build_slide_mount as B

L = B.AXIAL_SPACING
ROWS_X = (0.0, L)
HOLES_Y = (B.Y_POST_EAR, B.Y_POST_WOOD)
BAR_W, BAR_T = 4.0, 1.6                # frame bars: width, thickness (stiff in the plane, the jig is pressed flat on the plank)
GUIDE_OD, GUIDE_D, GUIDE_H = 5.6, 2.4, 8.0      # drill guides, wall 1.6 = 4 perimeters; the hole prints about 2.2-2.3: ream it once with the bit
LIP_T, LIP_H = 2.0, 6.0                # lips against the vertical wood face
CORNER = 1.0                           # chamfer on the edge that goes into the plank / wood corner (glue, rounded corner)


def bar(x0, y0, x1, y1):
    """straight bar of the frame between two points of the plane z = 0"""
    if y0 == y1:
        return cq.Workplane("XY").box(abs(x1 - x0) + BAR_W, BAR_W, BAR_T, centered=(True, True, False)).translate(((x0 + x1) / 2, y0, 0))
    return cq.Workplane("XY").box(BAR_W, abs(y1 - y0) + BAR_W, BAR_T, centered=(True, True, False)).translate((x0, (y0 + y1) / 2, 0))


def drill_jig():
    y_fence = B.WOOD_Y - BAR_W / 2
    body = bar(ROWS_X[0], y_fence, ROWS_X[1], y_fence)                     # along the wood face
    body = body.union(bar(ROWS_X[0], HOLES_Y[0], ROWS_X[1], HOLES_Y[0]))  # through the two ear-side guides
    for x in ROWS_X:
        body = body.union(bar(x, HOLES_Y[0], x, y_fence))                  # one arm per row
        lip = cq.Workplane("XY").box(B.PLATE_W, LIP_T, LIP_H, centered=(True, False, False)).translate((x, B.WOOD_Y - LIP_T, 0))
        body = body.union(lip)
        for y in HOLES_Y:
            body = body.union(cq.Workplane("XY").center(x, y).circle(GUIDE_OD / 2).extrude(GUIDE_H))
    # the frame stops at the wood face: trim the bar ends that the box() overhang pushed past y = 27 and past the lips' ends
    body = body.intersect(cq.Workplane("XY").box(L + B.PLATE_W, 200.0, 50.0, centered=(True, False, False)).translate((L / 2, B.WOOD_Y - 200.0, 0)))
    for x in ROWS_X:
        for y in HOLES_Y:
            body = body.cut(cq.Workplane("XY").center(x, y).circle(GUIDE_D / 2).extrude(GUIDE_H))
    corner = cq.Workplane("YZ").polyline([(B.WOOD_Y + 0.1, -0.1), (B.WOOD_Y - CORNER, -0.1), (B.WOOD_Y + 0.1, CORNER + 0.1)]).close()
    return body.cut(corner.extrude(L + 2 * B.PLATE_W).translate((-B.PLATE_W, 0, 0)))


if __name__ == "__main__":
    print(f"drilling jig, rows {L:.1f} apart")
    jig = drill_jig()
    B.export(jig, "drill_jig")
    print(f"   {jig.val().Volume() / 1000:.2f} cm3")
