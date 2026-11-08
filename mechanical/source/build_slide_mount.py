"""Slide-in motor supports with the snap lock "sample 4" (tongue 1.5 mm, interference 0.30 mm per side).

Desk frame used everywhere (same as motor_mount.SLDPRT): x = motor axis (pointing toward the coupler), y = horizontal, + toward the vertical
wood, z = up. Origin on the motor axis. Plank underside at z = +36, vertical wood face at y = +27.

Parts
  clip_plate        flat PETG plate, 2 snap slots (sample 4) + 2 M3 counterbores. Print flat. Two needed (one per support).
  ring_support_*    ring (+ pad with 2 bosses holding the M3 inserts) the plate is screwed on. Print with the ring axis vertical.
  plot              printed post: shoulder dia 6.0, head dia 10, countersunk bore for a dia 3 wood screw. Four needed.
  cushion           TPU silent block: ring washer 12 x 1.5 on the shoulder, above and below the plate (8 needed, wheel or solid).
  clip_cut_tool     the slot + slits of one clip as a solid / DXF (post centre at the origin), to cut into your own SolidWorks plates.

    pip install cadquery ezdxf
    python build_slide_mount.py
"""
import os
import cadquery as cq

OUT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
STEP, STL, DOCS = (os.path.join(OUT, d) for d in ("step", "stl", "docs"))
for d in (STEP, STL, DOCS):
    os.makedirs(d, exist_ok=True)

# ----------------------------------------------------------------------------- desk
PLANK_Z = 36.0                 # plank underside above the motor axis
WOOD_Y = 27.0                  # vertical wood face
Y_POST_EAR = -37.0             # post on the old ear side
Y_POST_WOOD = 19.0             # post on the vertical-wood side (pitch 56)

# ----------------------------------------------------------------------------- snap lock = sample 4 of the proof of concept
POST_D, CLEAR = 6.0, 0.3
SLOT_R = POST_D / 2 + CLEAR            # 3.3
T_TONGUE, DELTA = 1.5, 0.30            # sample 4: tongue thickness, interference per side (mouth 5.4)
X_RAMP0, X_LUG0, X_LUG1 = 11.0, 12.0, 13.0
ENTRY_OPEN, SLIT_W, SLIT_X0 = 0.3, 1.0, 7.0

# ----------------------------------------------------------------------------- plate and stack
PLATE_T, PLATE_W = 6.0, 15.0           # thickness (z), width along the motor axis (x)
XC = PLATE_W / 2                       # closed-end centre of the slots
PLATE_Y0, PLATE_Y1 = -44.4, 26.6       # ear end (plank edge at -44.48), wood end (wood face at 27)
GAP = 0.3                              # vertical play between the plate and each TPU cushion (no preload, no friction when sliding in)
CUSH_T = 1.5                           # TPU cushion thickness (free state); one above and one below the plate on every plot
CUSH_OD, CUSH_ID = 12.0, 5.8           # ring washer, friction fit on the dia 6.0 shoulder (0.1 per side)
Z_PLATE_BOT = PLANK_Z - CUSH_T - GAP - PLATE_T          # 28.2 : bearing face of the pad on the ring support
SH_LEN = PLATE_T + 2 * (GAP + CUSH_T)                   # 9.6 : shoulder length of the plots (plate + 2 cushions + 2 gaps)
SCREW_Y = (-8.0, 8.0)                  # plate fasteners (M3 x 5, head dia 5 x 3 -> counterbore 5.8 x 3.2; 3.2 mm in the insert)

# ----------------------------------------------------------------------------- ring supports
RING_OD = 54.0
PAD_Y = 12.0                           # half width of the pad the plate sits on
RELIEF_Y, RELIEF_Z = 26.0, 7.4         # flat that keeps the ring 1 mm away from the vertical wood (original: y = 26, |z| < 7.28)
# ---- the two original supports, read in motor_mount_1.SLDPRT and motor_front_mount_1.SLDPRT (Parasolid faces and display lists)
REAR_D, REAR_W = 48.5, 15.0            # motor_mount: bore, width (x = 0 .. 15)
REAR_SLOT_X, REAR_SLOT_Z = (3.0, 6.0), 4.25      # side slot, both sides: x 3 .. 6, |z| < 4.25
REAR_WIN_X, REAR_WIN_Z = (6.0, 15.0), 11.3       # side window, both sides: x 6 .. 15, |z| < 11.3
FRONT_D, FRONT_W = 48.0, 12.0          # motor_front_mount: bore, width (x = 0 .. 12)
FRONT_SLOT_BOTTOM = 26.0               # four slots at 45 deg, flat bottom 26.0 from the axis
FRONT_SLOT_ANGLES, FRONT_SLOT_NARROW, FRONT_SLOT_WIDE = (45.0, 135.0, 225.0, 315.0), 4.4, 6.2     # angle from +y toward +z in the desk frame
# Where the wide slot (the detrompeur) goes: as in your SLDPRT. It is at 225 deg in the file's own frame (bottom left, wood on the left), which is
# 315 deg in the desk frame: bottom right on the end view seen from the coupler side with the wood on the right. Confirmed by you.
FRONT_KEY_ANGLE = 315.0
RING_GAP = 44.5                        # measured on the motor: gap between the two rings, edge to edge
AXIAL_SPACING = RING_GAP + REAR_W / 2 + FRONT_W / 2     # 58.0 : distance between the ring centres = between the plot rows on the plank


def front_slots(key_angle=None):
    """the four slots of the front ring as (angle, width): three narrow and the wide one at key_angle"""
    k = FRONT_KEY_ANGLE if key_angle is None else key_angle
    return tuple((a, FRONT_SLOT_WIDE if abs(a - k) < 1.0 else FRONT_SLOT_NARROW) for a in FRONT_SLOT_ANGLES)
INSERT_L, INSERT_D, INSERT_DEPTH = 4.0, 3.8, 4.5      # M3 x 4 x OD4.2 heat-set insert (body dia 3.77) and its pocket: a little tight, 0.5 deeper
BOSS_D, BOSS_H = 8.0, 1.0              # boss on the pad around each insert: the pad alone is too thin above the bore for an L4 insert
PLATE_RECESS_D, PLATE_RECESS_H = BOSS_D + 0.4, BOSS_H + 0.1      # recess under the plate over each boss (it also centres the plate)

# ----------------------------------------------------------------------------- plot
HEAD_D, HEAD_H = 10.0, 2.5
BORE_D, CSK_D = 3.4, 6.6               # dia 3 wood screw, countersunk head dia 6


def clip_wires(y0=0.0, xc=XC):
    """closed 2D outlines (slot, two slits) of the clip centred on the post axis (xc, y0). Mouth toward +x."""
    m = POST_D / 2 - DELTA
    ye = SLOT_R + ENTRY_OPEN
    up = [(PLATE_W + 1.0 - XC + xc, ye), (PLATE_W - XC + xc, ye), (X_LUG1 - XC + xc, m), (X_LUG0 - XC + xc, m), (X_RAMP0 - XC + xc, SLOT_R), (xc, SLOT_R)]
    w = cq.Workplane("XY").moveTo(up[0][0], y0 + up[0][1])
    for (x, y) in up[1:]:
        w = w.lineTo(x, y0 + y)
    w = w.threePointArc((xc - SLOT_R, y0), (xc, y0 - SLOT_R))
    for (x, y) in [(X_RAMP0 - XC + xc, -SLOT_R), (X_LUG0 - XC + xc, -m), (X_LUG1 - XC + xc, -m), (PLATE_W - XC + xc, -ye), (PLATE_W + 1.0 - XC + xc, -ye)]:
        w = w.lineTo(x, y0 + y)
    return w.close()


def clip_cut(y0=0.0, xc=XC, z0=-0.5, h=PLATE_T + 1.0):
    """solid of one clip (slot + two relief slits) to subtract from a plate, z from z0 to z0 + h"""
    slot = clip_wires(y0, xc).extrude(h).translate((0, 0, z0))
    y_in = SLOT_R + T_TONGUE
    x_end = PLATE_W - XC + xc + 1.5
    x_start = SLIT_X0 - XC + xc
    for s in (1, -1):
        slit = (cq.Workplane("XY").center((x_start + x_end) / 2, y0 + s * (y_in + SLIT_W / 2)).slot2D(x_end - x_start, SLIT_W, 0).extrude(h).translate((0, 0, z0)))
        slot = slot.union(slit)
    return slot


def clip_plate():
    """plate in print orientation: z = 0 is the face that sits on the pad (bottom, on the bed), x along the motor axis, mouth at x = 15.
    Under each screw, a recess takes the boss of the pad."""
    body = cq.Workplane("XY").box(PLATE_W, PLATE_Y1 - PLATE_Y0, PLATE_T, centered=(False, False, False)).translate((0, PLATE_Y0, 0))
    for yp in (Y_POST_EAR, Y_POST_WOOD):
        body = body.cut(clip_cut(yp))
    for ys in SCREW_Y:
        body = body.cut(cq.Workplane("XY").center(XC, ys).circle(1.7).extrude(PLATE_T))
        body = body.cut(cq.Workplane("XY").workplane(offset=PLATE_T - 3.2).center(XC, ys).circle(2.9).extrude(3.2))
        body = body.cut(cq.Workplane("XY").center(XC, ys).circle(PLATE_RECESS_D / 2).extrude(PLATE_RECESS_H))
    return body


def plot():
    """printed plot, printed head down: head dia 10 x 3.5 (countersunk bore), shoulder dia 6.0 x 6.6, flat top against the plank"""
    csk = (CSK_D - BORE_D) / 2
    pts = [(CSK_D / 2, 0), (HEAD_D / 2 - 0.3, 0), (HEAD_D / 2, 0.3), (HEAD_D / 2, HEAD_H), (POST_D / 2, HEAD_H), (POST_D / 2, HEAD_H + SH_LEN),
           (BORE_D / 2, HEAD_H + SH_LEN), (BORE_D / 2, csk)]
    return cq.Workplane("XZ").polyline(pts).close().revolve(360, (0, 0, 0), (0, 1, 0))


def cushion(style="wheel"):
    """TPU silent block: flat ring washer, printed flat (0 deg overhang, no support), friction fit on the plot shoulder.
    wheel = outer ring + inner ring + 6 spokes (open frame, ~3.5x softer); solid = plain washer."""
    ro, ri = CUSH_OD / 2, CUSH_ID / 2
    if style == "solid":
        return cq.Workplane("XY").circle(ro).circle(ri).extrude(CUSH_T)
    w = 1.0
    body = cq.Workplane("XY").circle(ro).circle(ro - w).extrude(CUSH_T)
    body = body.union(cq.Workplane("XY").circle(ri + w).circle(ri).extrude(CUSH_T))
    for k in range(6):
        sp = (cq.Workplane("XY").box((ro - w) - (ri + w) + 0.2, 1.0, CUSH_T, centered=(False, True, False))
              .translate((ri + w - 0.1, 0, 0)).rotate((0, 0, 0), (0, 0, 1), 60 * k))
        body = body.union(sp)
    return body


def _ring_blank(inner_d, width):
    """annulus dia 54, axis x, x from 0 to width"""
    return cq.Workplane("YZ").circle(RING_OD / 2).circle(inner_d / 2).extrude(width)


def ring_core(kind, key_angle=None):
    """ring of the original support with its motor detrompeurs, no ear, no blade, in the support's own x range (0 .. width)"""
    if kind == "rear":
        w = REAR_W
        r = _ring_blank(REAR_D, w)
        x0, x1 = REAR_SLOT_X
        r = r.cut(cq.Workplane("XY").box(x1 - x0, 80.0, 2 * REAR_SLOT_Z, centered=(False, True, True)).translate((x0, 0, 0)))
        x0, x1 = REAR_WIN_X
        r = r.cut(cq.Workplane("XY").box(x1 - x0 + 1.0, 80.0, 2 * REAR_WIN_Z, centered=(False, True, True)).translate((x0, 0, 0)))
    else:
        w = FRONT_W
        r = _ring_blank(FRONT_D, w)
        for ang, wd in front_slots(key_angle):
            k = cq.Workplane("XY").box(w + 1.0, FRONT_SLOT_BOTTOM - 20.0, wd, centered=(False, False, True)).translate((-0.5, 20.0, 0))
            r = r.cut(k.rotate((0, 0, 0), (1, 0, 0), ang))
    r = r.cut(cq.Workplane("XY").box(w + 1.0, 3.0, 2 * RELIEF_Z, centered=(False, False, True)).translate((-0.5, RELIEF_Y, 0)))
    return r, w


def ring_support(kind="rear", key_angle=None):
    """ring support in the desk frame, centred on x = 0: original ring + detrompeurs, pad with its bosses and insert pockets for the plate"""
    core, w = ring_core(kind, key_angle)
    bore = (REAR_D if kind == "rear" else FRONT_D) / 2
    pad = cq.Workplane("XY").box(w, 2 * PAD_Y, Z_PLATE_BOT - 20.0, centered=(False, True, False)).translate((0, 0, 20.0))
    for ys in SCREW_Y:
        pad = pad.union(cq.Workplane("XY").workplane(offset=Z_PLATE_BOT).center(w / 2, ys).circle(BOSS_D / 2).extrude(BOSS_H))
    body = core.union(pad)
    body = body.cut(cq.Workplane("YZ").circle(bore).extrude(w + 2).translate((-1, 0, 0)))
    z_top = Z_PLATE_BOT + BOSS_H
    for ys in SCREW_Y:
        body = body.cut(cq.Workplane("XY").workplane(offset=z_top - INSERT_DEPTH).center(w / 2, ys).circle(INSERT_D / 2).extrude(INSERT_DEPTH + 0.5))
    return body.translate((-w / 2, 0, 0))


def to_print(shape, axis_up="x"):
    """rotate a desk-frame ring support so that its axis is vertical, face on the bed"""
    s = shape.rotate((0, 0, 0), (0, 1, 0), -90)           # x -> +z
    bb = s.val().BoundingBox()
    return s.translate((-bb.xmin - (bb.xlen / 2), -bb.ymin - (bb.ylen / 2), -bb.zmin))


def export(obj, name):
    cq.exporters.export(obj, os.path.join(STEP, name + ".step"))
    cq.exporters.export(obj, os.path.join(STL, name + ".stl"), tolerance=0.01, angularTolerance=0.1)
    print("  ", name)


def assembly(axial_spacing=AXIAL_SPACING):
    """check assembly in the desk frame"""
    asm = cq.Assembly(name="slide_mount_check")
    plank = cq.Workplane("XY").box(axial_spacing + 80, 100, 18, centered=(True, False, False)).translate((axial_spacing / 2, -55, PLANK_Z))
    wood = cq.Workplane("XY").box(axial_spacing + 80, 14, 56, centered=(True, False, False)).translate((axial_spacing / 2, WOOD_Y, -20))
    asm.add(plank, name="plank", color=cq.Color(0.93, 0.85, 0.65))
    asm.add(wood, name="vertical_wood", color=cq.Color(0.93, 0.85, 0.65))
    for xc, (kind, nm) in ((0.0, ("rear", "motor_mount")), (axial_spacing, ("front", "front_mount"))):
        asm.add(ring_support(kind).translate((xc, 0, 0)), name="ring_support_" + nm, color=cq.Color(0.45, 0.55, 0.75))
        asm.add(clip_plate().translate((xc - XC, 0, Z_PLATE_BOT)), name="clip_plate_" + nm, color=cq.Color(0.95, 0.65, 0.25))
        for yp in (Y_POST_EAR, Y_POST_WOOD):
            asm.add(plot().translate((xc, yp, PLANK_Z - SH_LEN - HEAD_H)), name=f"plot_{nm}_{yp:+.0f}", color=cq.Color(0.6, 0.6, 0.65))
            asm.add(cushion().translate((xc, yp, PLANK_Z - CUSH_T)), name=f"cushion_top_{nm}_{yp:+.0f}", color=cq.Color(0.3, 0.75, 0.6))
            asm.add(cushion().translate((xc, yp, Z_PLATE_BOT - GAP - CUSH_T)), name=f"cushion_bottom_{nm}_{yp:+.0f}", color=cq.Color(0.3, 0.75, 0.6))
    return asm, plank, wood


if __name__ == "__main__":
    print("slide-in supports, snap lock sample 4")
    plate = clip_plate()
    export(plate, "clip_plate")
    export(plate.mirror("YZ", (XC, 0, 0)), "clip_plate_mirrored")      # same plate with the mouths toward -x (if your coupler is on the other side)
    pl = plot()
    export(pl, "plot")
    for nm, kind in (("motor_mount", "rear"), ("front_mount", "front")):
        rs = ring_support(kind)
        export(rs, "ring_support_" + nm + "_desk_frame")
        cq.exporters.export(to_print(rs), os.path.join(STL, f"ring_support_{nm}_print.stl"), tolerance=0.01, angularTolerance=0.1)
    for st in ("wheel", "solid"):
        export(cushion(st), "cushion_" + st)
    cw = cushion("wheel")
    cq.exporters.export(cq.Workplane("XY").add([cw.translate((i * 14.0, j * 14.0, 0)).val() for i in range(4) for j in range(2)]), os.path.join(STL, "cushions_x8.stl"), tolerance=0.01, angularTolerance=0.1)
    # print layouts
    plates = plate.add(plate.translate((20.0, 0, 0)))
    cq.exporters.export(cq.Workplane("XY").add(plate.val()).add(plate.translate((20.0, 0, 0)).val()), os.path.join(STL, "clip_plates_x2.stl"), tolerance=0.01, angularTolerance=0.1)
    cq.exporters.export(cq.Workplane("XY").add([pl.translate((i * 16.0, j * 16.0, 0)).val() for i in range(2) for j in range(2)]), os.path.join(STL, "plots_x4.stl"), tolerance=0.01, angularTolerance=0.1)
    # clip as a cutting tool, post centre at the origin (for your own SolidWorks plates)
    export(clip_cut(0.0, 0.0, z0=0.0, h=PLATE_T), "clip_cut_tool")
    wires = [clip_wires(0.0, 0.0).vals()[0]]
    y_in = SLOT_R + T_TONGUE
    x0, x1 = SLIT_X0 - XC, PLATE_W - XC + 1.5
    for sgn in (1, -1):
        wires.append(cq.Workplane("XY").center((x0 + x1) / 2, sgn * (y_in + SLIT_W / 2)).slot2D(x1 - x0, SLIT_W, 0).vals()[0])
    cq.exporters.export(cq.Workplane("XY").add(wires), os.path.join(STEP, "clip_sample4_profile.dxf"))
    print("   clip_sample4_profile.dxf")
    asm, _, _ = assembly()
    asm.save(os.path.join(STEP, "assembly_check.step"))
    print("   assembly_check")
