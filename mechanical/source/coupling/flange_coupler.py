"""Model of the vendor's rigid flange coupler (AliExpress item 1005007347931821), variant B = 6.35 mm.
Dimensions from the seller's table: D 22, d 10, H 12, h 10, L 16 (bolt circle), e 3 (flange holes), C = M3 set screws.
Origin at the centre of the flat face, boss pointing +Z. The set-screw height (5 mm above the flange) is an assumption."""
import math
import cadquery as cq

D, d, H, h, B, L, e = 22.0, 10.0, 12.0, 10.0, 6.35, 16.0, 3.0
T = H - h                  # flange thickness = 2 mm
SET_D = 2.5                # tap drill of the M3 set screws
SET_Z = T + 5.0            # assumed set-screw height


def flange_coupler():
    body = cq.Workplane("XY").circle(D / 2).extrude(T)
    body = body.union(cq.Workplane("XY").workplane(offset=T).circle(d / 2).extrude(h))
    body = body.cut(cq.Workplane("XY").circle(B / 2).extrude(H))
    pts = [(L / 2 * math.cos(math.radians(a)), L / 2 * math.sin(math.radians(a))) for a in (0, 90, 180, 270)]
    body = body.cut(cq.Workplane("XY").pushPoints(pts).circle(e / 2).extrude(T))
    sx = cq.Workplane("YZ").center(0, SET_Z).circle(SET_D / 2).extrude(d / 2 + 1)
    sy = cq.Workplane("XZ").center(0, SET_Z).circle(SET_D / 2).extrude(-(d / 2 + 1))
    return body.cut(sx).cut(sy)
