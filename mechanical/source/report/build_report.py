"""Assembly report (A4 landscape PDF) of the slide-in motor supports with their silent blocks, in the style of mount_modification_plans.pdf.
    python build_report.py        -> ../../docs/slide_mount_assembly_report.pdf
"""
import os
import sys
import math
import textwrap
import numpy as np
from matplotlib.backends.backend_pdf import PdfPages
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'coupling')))
from report_common import *

TOTAL = 18


def wrap(items, width=92):
    """items: list of (kind, text) with kind in h / b (bullet) / s (small) / gap  ->  list of (text, style) lines for textblock"""
    out = []
    for kind, t in items:
        if kind == 'h':
            out.append((t, 'h'))
        elif kind == 'gap':
            out.append(('', 'gap'))
        elif kind == 'b':
            ls = textwrap.wrap(t, width=width)
            out.append(('\u2022 ' + ls[0], 'b'))
            out += [('   ' + l, 'b') for l in ls[1:]]
        elif kind == 's':
            for l in textwrap.wrap(t, width=int(width * 1.1)):
                out.append((l, 's'))
        elif kind == 'n':          # numbered step: "1  text"
            ls = textwrap.wrap(t, width=width)
            out.append((ls[0], 'b'))
            out += [('   ' + l, 's') for l in ls[1:]]
    return out


# ============================================================================ numbers shared by the pages
E_TPU = 12.0          # MPa, printed TPU 85A (assumption, to be measured)
M_MOTOR = 0.30        # kg, motor + two supports (assumption)
ZETA = 0.10           # damping ratio of printed TPU (assumption)
KR = 200.0            # N/mm, radial stiffness of the spider coupling (assumption)
T_NOM = 3.3           # N.m, torque of the desk drive
LEVER_A, LEVER_B = abs(B.Y_POST_EAR), B.Y_POST_WOOD


def cushion_k(style='wheel'):
    """compression stiffness in N/mm of one cushion: Ec = E (1 + 2 S^2), k = Ec A / t"""
    ro, ri, t = B.CUSH_OD / 2, B.CUSH_ID / 2, B.CUSH_T
    if style == 'solid':
        A = math.pi * (ro ** 2 - ri ** 2)
        S = A / (math.pi * (B.CUSH_OD + B.CUSH_ID) * t)
    else:
        w = 1.0
        A = math.pi * (ro ** 2 - (ro - w) ** 2) + math.pi * ((ri + w) ** 2 - ri ** 2) + 6 * 1.0 * ((ro - w) - (ri + w))
        S = w / (2 * t)
    return E_TPU * (1 + 2 * S ** 2) * A / t


def f_natural(k):
    """vertical natural frequency in Hz, 4 bottom cushions in parallel (the plate rests on them)"""
    return math.sqrt(4 * k * 1e3 / M_MOTOR) / (2 * math.pi)


def split_two_posts(k, gap, kr=KR, Ts=T_NOM / 2 * 1000.0):
    """torque of one support taken by the two posts (A at 37, B at 19) with a play 'gap' on each side and equal cushions k:
    returns F_A, F_B (N) and the axis shift w (mm) when the coupling takes the unbalanced force radially (kr)"""
    # unknowns FA, FB, w :  37 FA + 19 FB = Ts ; w = (FB - FA)/kr ; 19 (FA/k + gap - w) = 37 (FB/k + gap + w)
    Mx = np.array([[LEVER_A, LEVER_B, 0.0], [-1.0 / kr, 1.0 / kr, -1.0], [LEVER_B / k, -LEVER_A / k, -(LEVER_A + LEVER_B)]])
    rhs = np.array([Ts, 0.0, (LEVER_A - LEVER_B) * gap])
    FA, FB, w = np.linalg.solve(Mx, rhs)
    return FA, FB, w


def ring_z(y, r=24.25):
    return math.sqrt(r * r - y * y)


# ============================================================================ 1  summary
def page_summary():
    zb = B.Z_PLATE_BOT; zp = zb + B.PLATE_T; zh = B.PLANK_Z - B.SH_LEN - B.HEAD_H
    kw = cushion_k('wheel'); fn = f_natural(kw)
    fig = plt.figure(figsize=A4)
    page_title(fig, 'Slide-in motor supports with silent blocks \u2014 assembly report',
               'Drill motor on a standing-desk crank axis (IKEA Trotten). Two supports slide onto four plots and lock with clip sample 4; TPU cushions and a TPU spider isolate the two vibration paths.')
    items = [
        ('h', 'Principle'),
        ('b', 'Each support is a ring that clamps the motor plus a flat clip plate screwed on the ring. The plate has two U slots open toward the coupler.'),
        ('b', f'Four plots (\u00d86 shoulders) are screwed into the plank, two per support, at y = \u221237 and +19 (pitch 56 mm). Sliding the motor along its axis engages the plots in the slots; the jaws of the coupling engage at the same time.'),
        ('b', 'Two printed tongues at the mouth of each slot spread by 0.30 mm, then close behind the plot (clip sample 4). The closed end of the slot is the forward stop. Nothing to tighten after sliding.'),
        ('gap', ''),
        ('h', 'Vibration isolation: two paths, one elastic element each'),
        ('b', f'Mount path (drill body \u2192 wood): on every plot the plate floats between two TPU cushions (8 in total) with {B.GAP} mm of play on each side. No preload, so no friction when sliding in; the torque and the weight always meet TPU, never PETG on wood.'),
        ('b', 'Shaft path (drill \u2192 desk axis): the jaw coupling with its TPU spider (hubs A and B, pages 14\u201316), carried over unchanged from the previous prototype.'),
        ('b', f'Estimate: cushion \u2248 {kw:.0f} N/mm, natural frequency \u2248 {fn:.0f} Hz, isolation above \u2248 {1.41 * fn:.0f} Hz (page 8). To be measured with the ADXL345.'),
        ('gap', ''),
        ('h', 'What changes compared with the previous prototype'),
        ('b', 'Removed: ears with bobbin silent blocks (they broke at the \u00d89 hole), blades with TPU stop patches, one wood screw per mount.'),
        ('b', 'The silent blocks move from the ears to the plots: flat cushions, no neck, no small feature. The ring supports keep your rings and detrompeurs and carry a pad instead of ear and blade.'),
        ('gap', ''),
        ('h', 'Desk constraints used'),
        ('b', f'Plank underside 36 above the axis; vertical wood face 27 from the axis; plank edge \u221244.48 on the ear side (motor_mount).'),
        ('b', f'Stack: plank 36.0 \u00b7 cushion {B.CUSH_T} \u00b7 play {B.GAP} \u00b7 plate {zp:.1f} \u2192 {zb:.1f} \u00b7 play {B.GAP} \u00b7 cushion {B.CUSH_T} \u00b7 head \u00d810 \u00d7 {B.HEAD_H} (bottom at z = {zh:.1f}).'),
        ('gap', ''),
        ('h', 'To validate'),
        ('b', 'Axial spacing between the two supports (placeholder 80 mm). Slot mouths toward +x = toward the coupler (assumed). Rings and detrompeurs are taken from your two SLDPRT (checked to 0.002 mm); only their direction along x is assumed.'),
        ('b', 'Real distance from the shaft axis to the plank underside (36.0 assumed): it sets the cushion thicknesses (page 7).'),
    ]
    textblock(fig, 0.03, 0.885, wrap(items, 100), fs=7.5, lh=0.0222)
    ax = fig.add_axes([0.61, 0.10, 0.37, 0.72])
    endview(ax)
    ax.set_xlim(-58, 40); ax.set_ylim(-32, 56)
    fig.text(0.795, 0.835, 'Section through the plot axes (rear support)', fontsize=8, ha='center', color=BLUE, fontweight='bold')
    legend(fig, [(ORANGE, DK, 'clip plate'), (RING, DK, 'ring support'), (PLOT, DK, 'plot'), (TPUF, TPUE, 'TPU cushion'), ('#f3ead8', WOOD, 'wood')], x=0.50, y=0.075)
    footer(fig, 1, TOTAL)
    return fig


# ============================================================================ 2  overview
def page_overview():
    fig = plt.figure(figsize=A4)
    page_title(fig, 'Assembly \u2014 overview', 'Section y\u2013z through the plot axes, motor axis = x (perpendicular to the page, toward the coupler). Rear support (motor_mount ring \u00d848.5) shown. Dimensions in mm.')
    ax = fig.add_axes([0.02, 0.07, 0.62, 0.82])
    endview(ax)
    zt = B.PLANK_Z
    ext(ax, (-56, 0), (-3, 0)); ext(ax, (-56, zt), (-46, zt))
    dim(ax, (-54, 0), (-54, zt), '36', tp=(-56.5, zt / 2), fs=8)
    ext(ax, (0, -28.5), (0, -33)); ext(ax, (B.WOOD_Y, -33), (B.WOOD_Y, -28))
    dim(ax, (0, -32), (B.WOOD_Y, -32), '27', tp=(B.WOOD_Y / 2, -34), fs=8)
    zz = B.PLANK_Z + 16 + 4
    ext(ax, (B.Y_POST_EAR, zz - 3), (B.Y_POST_EAR, zz + 9)); ext(ax, (B.Y_POST_WOOD, zz - 3), (B.Y_POST_WOOD, zz + 9)); ext(ax, (0, zz - 3), (0, zz + 6))
    dim(ax, (B.Y_POST_EAR, zz + 2), (0, zz + 2), '37', tp=(B.Y_POST_EAR / 2, zz + 4.2), fs=8)
    dim(ax, (0, zz + 2), (B.Y_POST_WOOD, zz + 2), '19', tp=(B.Y_POST_WOOD / 2, zz + 4.2), fs=8)
    dim(ax, (B.Y_POST_EAR, zz + 8), (B.Y_POST_WOOD, zz + 8), '56 (plot pitch)', tp=(-9, zz + 10.2), fs=8)
    note(ax, (-58, 60.5), 'plank (\u2265 15 mm)', color='#7a5a20', fs=7)
    note(ax, (B.WOOD_Y + 1.0, 3), 'wood', color='#7a5a20', fs=7)
    note(ax, (B.RELIEF_Y - 0.6, -13.0), 'side window\n(x 6 \u2192 15)', color=RED, fs=7, ha='right')

    def tg(n, xy, to):
        leader(ax, xy, to); tag(ax, xy, n, r=2.2)
    tg(1, (-33, -20), (-19.5, -18.6))
    tg(2, (-26, 20), (-24, B.Z_PLATE_BOT))
    tg(3, (21.5, 19.0), (21.5, B.PLANK_Z - B.SH_LEN - B.HEAD_H))
    tg(4, (-44, 40.0), (-40.0, B.PLANK_Z - B.CUSH_T / 2))
    legend(fig, [(ORANGE, DK, 'clip plate (PETG Matte)'), (RING, DK, 'ring support'), (PLOT, DK, 'plot'), (TPUF, TPUE, 'TPU cushion'), ('#f3ead8', WOOD, 'wood')], x=0.03, y=0.05)
    items = [
        ('h', 'Parts'),
        ('n', '1  Ring support, one per motor ring (\u00d72)'),
        ('n', '2  Clip plate, two slots, 2 \u00d7 M3 (\u00d72)'),
        ('n', '3  Plot with its \u00d83 \u00d7 25 wood screw (\u00d74)'),
        ('n', '4  TPU cushion, 2 per plot (\u00d78)'),
        ('gap', ''),
        ('h', 'Assembly order (pictures page 12)'),
        ('n', '1  Heat-set inserts. Press 2 short M3 inserts into the pad pockets of each ring support (iron \u2248 230 \u00b0C), flush with the pad face.'),
        ('n', '2  Plates. Screw each plate on its pad with 2 \u00d7 M3 \u00d7 5, mouths toward the coupler (+x). \u2264 0.3 N\u00b7m.'),
        ('n', '3  Cushions. Slide 2 cushions on the shoulder of each plot: one down to the head, one up to the flat top.'),
        ('n', '4  Plots. Drill the plank (page 11) and screw the 4 plots, flat top against the plank, at y = \u221237 and +19.'),
        ('n', '5  Slide. With the motor in its two rings, slide the whole assembly toward the coupler: each plot spreads the tongues, the clip snaps behind it, the slot end stops the motor, the coupling jaws engage.'),
        ('n', '6  Check. Turn the axis by hand: no tight spot, plate free between the cushions (0.3 mm each side). Axis offset \u2264 0.3 mm, tilt \u2264 1\u00b0.'),
        ('n', '7  Remove. Pull the motor back: the four clips release.'),
    ]
    textblock(fig, 0.665, 0.885, wrap(items, 54), fs=7.5, lh=0.0235)
    fig.text(0.665, 0.07, 'Dimensions: SolidWorks sketches (plank 36, wood 27) and build_slide_mount.py.', fontsize=6.6, color='#555555')
    footer(fig, 2, TOTAL)
    return fig


# ============================================================================ 3  stack details
def page_stack():
    fig = plt.figure(figsize=A4)
    page_title(fig, 'Assembly \u2014 stack and clearances (dimensioned details)', 'Three zoomed sections of the same drawing. z up, y toward the wood. Dimensions in mm.')
    zt, zb = B.PLANK_Z, B.Z_PLATE_BOT
    zp = zb + B.PLATE_T
    zs = zt - B.SH_LEN
    zh = zs - B.HEAD_H
    z1 = zt - B.CUSH_T; z4 = zb - B.GAP
    H = 0.47
    ax = fig.add_axes([0.02, 0.39, 0.31, H]); endview(ax); ax.set_xlim(-50, -24); ax.set_ylim(17, 43)
    ax.set_title('A \u2014 plot on the ear side (y = \u221237)', fontsize=8.2, color=BLUE, loc='left', fontweight='bold')
    yc = -29.6
    for z0, z1_, t in ((zt, z1, '1.5 cushion'), (z1, zp, f'{B.GAP}'), (zp, zb, '6.0 plate'), (zb, z4, f'{B.GAP}'), (z4, zs, '1.5 cushion'), (zs, zh, f'{B.HEAD_H} head')):
        dim(ax, (yc, z0), (yc, z1_), '', fs=7)
        ax.text(yc + 0.5, (z0 + z1_) / 2, t, fontsize=6.6, va='center', ha='left', color=DK, zorder=14, bbox=dict(fc='white', ec='none', pad=0.2))
    dim(ax, (-25.8, zt), (-25.8, zh), '', fs=7)
    note(ax, (-26.3, 21.0), f'{B.SH_LEN + B.HEAD_H:.1f}', fs=7, ha='right')
    dim(ax, (-42, 21.0), (-32, 21.0), '\u00d810 head', tp=(-37, 19.6), fs=7)
    ext(ax, (-42, 21.5), (-42, zh)); ext(ax, (-32, 21.5), (-32, zh))
    note(ax, (-49.5, 41.2), 'plate end \u221244.4, plank edge \u221244.48', fs=6.4, color=RED)
    note(ax, (-49.5, 26.3), 'slot 6.6, post \u00d86.0', fs=6.6)
    leader(ax, (-44.0, 26.9), (-40.3, 31.6), color=BLUE)
    ax = fig.add_axes([0.345, 0.39, 0.31, H]); endview(ax); ax.set_xlim(6, 34); ax.set_ylim(15, 43)
    ax.set_title('B \u2014 plot on the wood side (y = +19)', fontsize=8.2, color=BLUE, loc='left', fontweight='bold')
    dim(ax, (25, 22.4), (27, 22.4), '', fs=7); note(ax, (26, 21.2), '2.0', fs=7, ha='center')
    ext(ax, (25, 26.6), (25, 22.0)); ext(ax, (27, 22.0), (27, 23.0))
    leader(ax, (26.8, 31.0), (22.5, 38.8), color=RED); note(ax, (22.3, 38.8), 'plate end to wood 0.4', fs=7, ha='right', color=RED)
    dim(ax, (12, 28.0), (13, 28.0), '', fs=7); note(ax, (11.7, 31.2), '1.0 pad\u2013cushion', fs=7, ha='right')
    leader(ax, (11.8, 30.6), (12.6, 28.2), color=BLUE)
    ax.annotate('', xy=(13.6, ring_z(14.0, 27.0)), xytext=(13.6, zh), arrowprops=dict(arrowstyle='<->', lw=0.8, color=DK))
    note(ax, (12.6, 19.4), f'{zh - ring_z(14.0, 27.0):.1f} (head corner\nto ring)', fs=6.8, ha='right')
    leader(ax, (12.7, 20.4), (13.6, 23.5), color=BLUE)
    note(ax, (7.0, 16.2), 'ring \u00d754', fs=7, color='#555555')
    ax = fig.add_axes([0.67, 0.39, 0.31, H]); endview(ax); ax.set_xlim(-13.5, 18.5); ax.set_ylim(15, 47)
    ax.set_title('C \u2014 plate on the pad (screw at y = +8)', fontsize=8.2, color=BLUE, loc='left', fontweight='bold')
    dim(ax, (12.6, zb), (12.6, zp), '', fs=7); note(ax, (13.0, (zb + zp) / 2), '6.0', fs=7)
    dim(ax, (13.9, zb - B.INSERT_DEPTH), (13.9, zb), '', fs=7); note(ax, (14.3, zb - 1.6), f'{B.INSERT_DEPTH}', fs=7)
    dim(ax, (6.0, 20.8), (10.0, 20.8), '', fs=7); note(ax, (8.0, 19.6), '\u00d84.0 pocket', fs=7, ha='center')
    dim(ax, (5.1, zp + 1.6), (10.9, zp + 1.6), '', fs=7); note(ax, (8.0, zp + 3.0), '\u00d85.8 \u00d7 3.2 counterbore', fs=7, ha='center')
    note(ax, (-13.2, 44.4), 'M3 \u00d7 5: head 3.0 (0.2 below the face),\n2.0 mm into the insert, 1.0 free', fs=6.8)
    note(ax, (-13.2, 17.4), f'pocket floor to bore: \u2265 {zb - B.INSERT_DEPTH - ring_z(6.0):.1f} (inner edge)', fs=6.8)
    rows = [['Item', 'Value', 'Comment'],
            ['plank underside', 'z = 36.0', 'from the SolidWorks sketch'],
            ['top cushion', f'{zt:.1f} \u2192 {z1:.1f}', 'against the plank, friction fit on the shoulder'],
            ['plate top / bottom', f'{zp:.1f} / {zb:.1f}', f'{B.GAP} play above and below, no preload'],
            ['bottom cushion', f'{z4:.1f} \u2192 {zs:.1f}', 'on the head, friction fit on the shoulder'],
            ['plot shoulder / head', f'\u00d86.0 \u00d7 {B.SH_LEN} / \u00d810 \u00d7 {B.HEAD_H}', f'head bottom at z = {zh:.1f}'],
            ['pad top = plate bearing face', f'z = {zb:.1f}', f'{zb - 27:.1f} above the ring top (27.0)'],
            ['plate end (ear / wood side)', 'y = \u221244.4 / +26.6', 'plank edge \u221244.48; wood face 27'],
            ['ring to wood', '1.0', 'relief flat at y = 26, |z| < 7.4'],
            ['head B to ring', f'{zh - ring_z(14.0, 27.0):.1f}', 'closest point, ring \u00d754 at y = 14']]
    table(fig, 0.03, 0.33, [0.0, 0.17, 0.33], rows, lh=0.0275, fs=7.3, widths=0.52)
    items = [('h', 'Notes'),
             ('b', f'The plate is carried by the pad and floats between the cushions ({B.GAP} mm of play each side): the plots only guide it, hold it down and take the couple, always through TPU.'),
             ('b', 'Plot B is the tight one: 2.0 to the wood, 1.0 to the pad, 0.8 to the ring. If the wood face is not at 27 from the axis, move plot B (the slot has no adjustment).'),
             ('b', 'The plank edge on the ear side limits plot A and the plate end (motor_mount side \u221244.48; front mount side \u221248.09).')]
    textblock(fig, 0.60, 0.33, wrap(items, 54), fs=7.5, lh=0.0245)
    footer(fig, 3, TOTAL)
    return fig



# ============================================================================ clip geometry helpers

def clip_polys(y0=0.0, swap=False):
    """returns (slot polygon, [slit specs]) of one clip centred on the post axis (x = XC, y = y0). swap=True returns (y, x) pairs."""
    m = B.POST_D / 2 - B.DELTA
    ye = B.SLOT_R + B.ENTRY_OPEN
    th = np.linspace(np.pi / 2, 3 * np.pi / 2, 40)
    slot = [(B.PLATE_W, y0 + ye), (B.X_LUG1, y0 + m), (B.X_LUG0, y0 + m), (B.X_RAMP0, y0 + B.SLOT_R), (B.XC, y0 + B.SLOT_R)] + \
           [(B.XC + B.SLOT_R * np.cos(t), y0 + B.SLOT_R * np.sin(t)) for t in th[1:-1]] + \
           [(B.XC, y0 - B.SLOT_R), (B.X_RAMP0, y0 - B.SLOT_R), (B.X_LUG0, y0 - m), (B.X_LUG1, y0 - m), (B.PLATE_W, y0 - ye)]
    y_in = B.SLOT_R + B.T_TONGUE
    slits = [(B.SLIT_X0, B.PLATE_W + 0.4, y0 + sg * (y_in + B.SLIT_W / 2)) for sg in (1, -1)]       # (x start, x end, y centre), width SLIT_W
    if swap:
        slot = [(y, x) for (x, y) in slot]
    return slot, slits



def draw_clip(ax, y0=0.0, swap=False, fc=ORANGE, ec=DK, lw=0.8, z=3, plate=None, post=False):
    slot, slits = clip_polys(y0, swap)
    if plate is not None:                      # (x0, x1, y0, y1)
        x0, x1, ya, yb = plate
        if swap:
            R(ax, ya, yb, x0, x1, fc, ec=ec, lw=lw, z=z - 1)
        else:
            R(ax, x0, x1, ya, yb, fc, ec=ec, lw=lw, z=z - 1)
    POLY(ax, slot, 'white', ec=ec, lw=lw, z=z)
    for xs, xe, yc in slits:
        if swap:
            ax.add_patch(FancyBboxPatch((yc - B.SLIT_W / 2, xs), B.SLIT_W, xe - xs, boxstyle='round,pad=0,rounding_size=0.45', fc='white', ec=ec, lw=lw, zorder=z))
        else:
            ax.add_patch(FancyBboxPatch((xs, yc - B.SLIT_W / 2), xe - xs, B.SLIT_W, boxstyle='round,pad=0,rounding_size=0.45', fc='white', ec=ec, lw=lw, zorder=z))
    if post:
        cx, cy = (y0, B.XC) if swap else (B.XC, y0)
        ax.add_patch(Circle((cx, cy), B.POST_D / 2, fc=PLOT, ec=DK, lw=lw, zorder=z + 1))



def page_plate():
    fig = plt.figure(figsize=A4)
    page_title(fig, 'Clip plate \u2014 drawing (\u00d72, identical)', 'Plan view from the plank side, motor axis x vertical (mouth edge at the top, toward the coupler), y horizontal. Printed flat, bottom face (on the pad) on the bed. Dimensions in mm.')
    ax = fig.add_axes([0.03, 0.40, 0.94, 0.47]); ax.set_aspect('equal'); ax.axis('off')
    ax.set_xlim(-49, 30); ax.set_ylim(-9.5, 20.5)
    draw_clip(ax, B.Y_POST_EAR, swap=True, plate=(0, B.PLATE_W, B.PLATE_Y0, B.PLATE_Y1), post=False)
    draw_clip(ax, B.Y_POST_WOOD, swap=True, post=False)
    for ys in B.SCREW_Y:
        ax.add_patch(Circle((ys, B.XC), 1.7, fc='white', ec=DK, lw=0.8, zorder=4))
        ax.add_patch(Circle((ys, B.XC), 2.9, fc='none', ec=DK, lw=0.6, ls=(0, (3, 2)), zorder=4))
    # pad outlines
    R(ax, -B.PAD_Y, B.PAD_Y, 0, 15, 'none', ec=GREY, lw=0.8, z=6, hatch=None).set_linestyle((0, (4, 3)))
    R(ax, -B.PAD_Y, B.PAD_Y, 1.5, 13.5, 'none', ec=BLUE, lw=0.8, z=6).set_linestyle((0, (4, 3)))
    note(ax, (-11.7, 16.6), 'pad of the \u00d748.5 ring (15 wide)', fs=6.6, color='#666666', ha='left')
    note(ax, (-11.7, -1.7), 'pad of the \u00d748 ring (12 wide)', fs=6.6, color=BLUE, ha='left')
    center_line(ax, (0, -4.5), (0, 19.5))
    note(ax, (0.6, 19.4), 'motor axis (y = 0)', fs=6.8, ha='left')
    for yp in (B.Y_POST_EAR, B.Y_POST_WOOD):
        center_line(ax, (yp, -4.5), (yp, 18.5))
        ax.plot([yp - 2, yp + 2], [B.XC, B.XC], color=DK, lw=0.5, zorder=9)
    # dimensions, chain below the plate
    ys_ = [B.PLATE_Y0, B.Y_POST_EAR, -8.0, 8.0, B.Y_POST_WOOD, B.PLATE_Y1]
    for y in ys_:
        ext(ax, (y, -0.3), (y, -5.6))
    for a, b_, t in zip(ys_[:-1], ys_[1:], ['7.4', '29', '16', '11', '7.6']):
        dim(ax, (a, -4.6), (b_, -4.6), t, tp=((a + b_) / 2, -4.6), fs=7.5)
    ext(ax, (B.PLATE_Y0, -0.3), (B.PLATE_Y0, -8.3)); ext(ax, (B.PLATE_Y1, -0.3), (B.PLATE_Y1, -8.3))
    dim(ax, (B.PLATE_Y0, -7.6), (B.PLATE_Y1, -7.6), '71.0', tp=(-9, -7.6), fs=8)
    dim(ax, (B.PLATE_Y1 + 1.8, 0), (B.PLATE_Y1 + 1.8, 15), '15', tp=(B.PLATE_Y1 + 3.3, 7.5), fs=8)
    ext(ax, (B.PLATE_Y1, 0), (B.PLATE_Y1 + 2.6, 0)); ext(ax, (B.PLATE_Y1, 15), (B.PLATE_Y1 + 2.6, 15))
    note(ax, (-48.5, 17.0), 'mouth edge: toward the coupler (+x)', fs=7.2, color=RED, ha='left')
    ax.annotate('', xy=(-30, 19.2), xytext=(-30, 15.4), arrowprops=dict(arrowstyle='-|>', lw=1.6, color=RED))
    note(ax, (-48.5, 8.6), 'clip A (ear side)\nsee page 5', fs=7, ha='left', color=BLUE)
    note(ax, (23.8, 13.2), 'clip B\n(wood side)', fs=7, ha='left', color=BLUE)
    note(ax, (-8.0, 11.6), '2 \u00d7 \u00d83.4, counterbore\n\u00d85.8 \u00d7 3.2 (top face)', fs=6.8, ha='center')
    # section through the screw (x-z), bottom left
    ax2 = fig.add_axes([0.04, 0.06, 0.24, 0.24]); ax2.set_aspect('equal'); ax2.axis('off'); ax2.set_xlim(-3, 19); ax2.set_ylim(-5, 9)
    R(ax2, 0, 15, 0, 6, ORANGE, z=3)
    R(ax2, B.XC - 2.9, B.XC + 2.9, 2.8, 6, 'white', z=4); R(ax2, B.XC - 1.7, B.XC + 1.7, 0, 2.8, 'white', z=4)
    center_line(ax2, (B.XC, -1.5), (B.XC, 7.2))
    dim(ax2, (-1.2, 0), (-1.2, 6), '6.0', tp=(-2.4, 3), fs=7)
    dim(ax2, (B.XC - 2.9, 7.0), (B.XC + 2.9, 7.0), '', fs=7); note(ax2, (B.XC, 8.0), '\u00d85.8 \u00d7 3.2', fs=7, ha='center')
    dim(ax2, (B.XC - 1.7, -1.0), (B.XC + 1.7, -1.0), '', fs=7); note(ax2, (B.XC, -2.2), '\u00d83.4', fs=7, ha='center')
    dim(ax2, (16.2, 0), (16.2, 2.8), '', fs=7); note(ax2, (16.8, 1.4), '2.8', fs=7)
    ax2.set_title('Section through a screw (y = \u00b18)', fontsize=7.8, color=BLUE, loc='left', fontweight='bold')
    items = [('h', 'Notes'),
             ('b', 'Two identical plates, one per ring support. The two slots are the clip of sample 4 (page 5), pitch 56, centred on y = \u221237 and y = +19.'),
             ('b', 'The plate is fixed to the pad by 2 \u00d7 M3 \u00d7 5 only; it is centred on the ring in x (15 mm plate on a 12 mm ring: 1.5 mm overhang each side).'),
             ('b', 'Print flat so that the tongues bend in the plane of the layers: 5 walls, no support, bottom face on the bed.'),
             ('b', 'If your coupler is on the other side of the motor, use the mirrored plate (same part, mouths toward \u2212x).')]
    textblock(fig, 0.32, 0.31, wrap(items, 80), fs=7.7, lh=0.0245)
    footer(fig, 4, TOTAL)
    return fig



def page_clip():
    fig = plt.figure(figsize=A4)
    page_title(fig, 'Snap clip \u2014 sample 4 (dimensioned) and how it works', 'Plan view, origin on the plot axis, mouth toward +x. Tongue 1.5 mm, interference 0.30 mm per side, mouth 5.4 mm for a \u00d86.0 plot. Dimensions in mm.')
    ax = fig.add_axes([0.02, 0.10, 0.55, 0.78]); ax.set_aspect('equal'); ax.axis('off')
    ax.set_xlim(-8.5, 12.5); ax.set_ylim(-11.5, 9.5)
    xo = -B.XC                                 # shift so that the post axis is the origin
    R(ax, xo, xo + B.PLATE_W, -8.5, 8.5, ORANGE, z=1)
    m = B.POST_D / 2 - B.DELTA; ye = B.SLOT_R + B.ENTRY_OPEN
    th = np.linspace(np.pi / 2, 3 * np.pi / 2, 50)
    xr0, xl0, xl1, xe = B.X_RAMP0 + xo, B.X_LUG0 + xo, B.X_LUG1 + xo, B.PLATE_W + xo
    slot = [(xe, ye), (xl1, m), (xl0, m), (xr0, B.SLOT_R), (0, B.SLOT_R)] + [(B.SLOT_R * np.cos(t), B.SLOT_R * np.sin(t)) for t in th[1:-1]] + [(0, -B.SLOT_R), (xr0, -B.SLOT_R), (xl0, -m), (xl1, -m), (xe, -ye)]
    POLY(ax, slot, 'white', z=2, lw=1.1)
    y_in = B.SLOT_R + B.T_TONGUE; x0 = B.SLIT_X0 + xo
    for sg in (1, -1):
        ax.add_patch(FancyBboxPatch((x0, sg * (y_in + B.SLIT_W / 2) - B.SLIT_W / 2), xe - x0 + 0.4, B.SLIT_W, boxstyle='round,pad=0,rounding_size=0.45', fc='white', ec=DK, lw=1.0, zorder=2))
    ax.add_patch(Circle((0, 0), B.POST_D / 2, fc=PLOT, ec=DK, lw=1.0, zorder=3)); ax.text(0, 0, 'plot\n\u00d86.0', ha='center', va='center', fontsize=8, zorder=4)
    ax.add_patch(Circle((xl0 + 0.5, 0), B.POST_D / 2, fc='none', ec=RED, lw=1.0, ls=(0, (4, 3)), zorder=5))
    note(ax, (xl0 + 0.5, -4.6), 'plot in the lug', fs=7, color=RED, ha='center')
    center_line(ax, (-5.0, 0), (10.5, 0))
    ax.plot([-4.4, 7.4], [0, 0], color=DK, lw=0, zorder=0)
    dim(ax, (-3.0, -B.SLOT_R), (-3.0, B.SLOT_R), '', fs=7.5); note(ax, (-4.1, 0), '6.6', fs=7.5, ha='center')
    dim(ax, (xl0 + 0.5, -m), (xl0 + 0.5, m), '', fs=7.5); note(ax, (xl0 + 0.5, 1.6), 'mouth 5.4', fs=7.5, ha='center', color=GREEN if False else TEAL)
    dim(ax, (xe + 1.0, -ye), (xe + 1.0, ye), '', fs=7.5); note(ax, (xe + 1.1, 0), 'entry\n7.2', fs=7.2, ha='left')
    ext(ax, (xe, -ye), (xe + 1.4, -ye)); ext(ax, (xe, ye), (xe + 1.4, ye))
    dim(ax, (0, -9.8), (xe, -9.8), '7.5 from the plot axis to the plate edge', tp=(xe / 2 + 0.5, -10.6), fs=7.5)
    for a, b_, t in ((0, xr0, '3.5'), (xr0, xl0, '1'), (xl0, xl1, '1'), (xl1, xe, '2')):
        dim(ax, (a, -8.9), (b_, -8.9), '', fs=7); note(ax, ((a + b_) / 2, -9.0 + 0.0), t, fs=7, ha='center')
    for x in (0, xr0, xl0, xl1, xe):
        ext(ax, (x, -8.5), (x, -9.9))
    dim(ax, (x0 - 1.0, B.SLOT_R), (x0 - 1.0, y_in), '', fs=7, color=RED); note(ax, (x0 - 1.1, (B.SLOT_R + y_in) / 2), '1.5', fs=7.2, color=RED, ha='right')
    dim(ax, (x0 - 1.0, y_in), (x0 - 1.0, y_in + B.SLIT_W), '', fs=7, color=RED); note(ax, (x0 - 1.1, y_in + 0.5), '1.0', fs=7.2, color=RED, ha='right')
    note(ax, (x0 - 0.2, y_in + 1.9), 'slit starts at x = \u22120.5 (rounded end)', fs=7, color=RED, ha='left')
    for sg in (1, -1):
        ax.annotate('', xy=(xl0 + 0.5, sg * 5.5), xytext=(xl0 + 0.5, sg * 4.5), arrowprops=dict(arrowstyle='-|>', lw=1.3, color=RED), zorder=9)
    note(ax, (xl0 + 0.9, 6.3), '\u00b10.30', fs=7.2, color=RED, ha='left')
    note(ax, (-8.3, 8.9), 'plate 15 wide, 6 thick (z)', fs=7.4, ha='left')
    # ---- sequence (right)
    seq = [('1  entry', 7.0), ('2  tongues spread 0.30', xl0 + 0.5), ('3  latched, plot in the slot end', 0.0)]
    for k, (lab, px) in enumerate(seq):
        a2 = fig.add_axes([0.62, 0.675 - k * 0.215, 0.34, 0.20]); a2.set_aspect('equal'); a2.axis('off'); a2.set_xlim(-9.0, 12.0); a2.set_ylim(-7.2, 7.2)
        R(a2, xo, xo + B.PLATE_W, -7, 7, ORANGE, z=1)
        POLY(a2, slot, 'white', z=2, lw=0.8)
        for sg in (1, -1):
            a2.add_patch(FancyBboxPatch((x0, sg * (y_in + B.SLIT_W / 2) - B.SLIT_W / 2), xe - x0 + 0.4, B.SLIT_W, boxstyle='round,pad=0,rounding_size=0.45', fc='white', ec=DK, lw=0.7, zorder=2))
        a2.add_patch(Circle((px, 0), B.POST_D / 2, fc=PLOT, ec=DK, lw=0.8, zorder=3))
        if k == 1:
            for sg in (1, -1):
                a2.annotate('', xy=(px, sg * 5.3), xytext=(px, sg * 4.3), arrowprops=dict(arrowstyle='-|>', lw=1.1, color=RED), zorder=9)
        a2.annotate('', xy=(px - 4.8, 0), xytext=(px - 3.6, 0), arrowprops=dict(arrowstyle='-|>', lw=0.0, color='white')) if False else None
        a2.set_title(lab, fontsize=8, color=BLUE, loc='left', fontweight='bold')
    items = [('h', 'Numbers (beam theory, to be measured)'),
             ('s', 'Tongue = cantilever 5 mm long, 1.5 thick, 6 high, flexural modulus 2 GPa, friction 0.3.'),
             ('s', 'Push-through: tongue force \u2248 24 N each; unlatching \u2248 50 N per clip (\u00b150 %).'),
             ('s', 'Strain at the tongue root \u2248 2.7 %: close to the PETG Matte limit in the layer plane (\u2248 3 %), above it across layers (\u2248 1 %), hence the flat print.'),
             ('s', 'Four clips in parallel: pulling the motor out \u2248 210 N. Measure; if too stiff use sample 3 (page 17).')]
    textblock(fig, 0.60, 0.225, wrap(items, 66), fs=7.3, lh=0.0215)
    footer(fig, 5, TOTAL)
    return fig



def page_integration():
    fig = plt.figure(figsize=A4)
    page_title(fig, 'Using the clip in your own SolidWorks supports', 'Files: clip_sample4_profile.dxf (outline), clip_cut_tool.step (solid), docs/clip_sample4_drawing.png. Origin on the plot axis, mouth toward +x.')
    items = [('h', 'Two ways'),
             ('b', 'DXF: open clip_sample4_profile.dxf in SolidWorks (File > Open, import as a 2D sketch), copy the sketch and paste it into a sketch on the face of your plate; place the origin on the plot axis. Cut-Extrude through the 6 mm of the plate.'),
             ('b', 'Solid: Insert > Part, clip_cut_tool.step (6 mm), origin on the plot axis, then Combine > Subtract with your plate body.'),
             ('gap', ''),
             ('h', 'Rules to keep the validated behaviour'),
             ('b', 'Print the plate flat (page 13). The clip is only validated in that orientation.'),
             ('b', 'Keep the plate 6 thick and at least 15 wide along x, slot centred in x (7.5 from each edge) and the slit start at x = \u22120.5.'),
             ('b', 'Keep 1.6 mm or more of material outside the outer slit (slit outer edge at \u00b15.8 from the plot axis).'),
             ('b', 'Plot \u00d86.0 exactly: the interference is 0.30 per side only with that diameter.'),
             ('gap', ''),
             ('h', 'Coordinates from the plot axis (x toward the mouth, y across)'),
             ('s', 'closed end R3.3 centred on the axis, straight walls y = \u00b13.3 from x = 0 to 3.5'),
             ('s', 'inner ramp to y = \u00b12.7 at x = 4.5, lug flat x = 4.5 to 5.5 at y = \u00b12.7'),
             ('s', 'entry ramp to y = \u00b13.6 at the plate edge x = 7.5'),
             ('s', 'relief slits 1.0 wide, y = 4.8 to 5.8 and \u22124.8 to \u22125.8, from x = \u22120.5 (rounded end) through the edge')]
    textblock(fig, 0.03, 0.86, wrap(items, 78), fs=7.7, lh=0.0245)
    # right : clip family
    fig.text(0.60, 0.86, 'The six clips of the proof of concept', fontsize=9.5, fontweight='bold', color=BLUE)
    rows = [['#', 'Tongue', 'Interf.', 'Mouth', 'Per clip*', '4 clips*']]
    fam = [(1, 0.8, 0.30), (2, 1.0, 0.30), (3, 1.2, 0.30), (4, 1.5, 0.30), (5, 1.0, 0.45), (6, 1.2, 0.45)]
    for n, t, d in fam:
        k = 2000.0 * 6.0 * t ** 3 / (4 * 5.0 ** 3)
        F = 2 * k * d * ((0.3 + d) + 0.3) / (1 - 0.3 * (0.3 + d))
        rows.append([str(n) + ('  (selected)' if n == 4 else ''), f'{t:.1f}', f'{d:.2f}', f'{2 * (3.0 - d):.1f}', f'\u2248 {F:.0f} N', f'\u2248 {4 * F:.0f} N'])
    table(fig, 0.60, 0.835, [0.0, 0.085, 0.14, 0.195, 0.25, 0.315], rows, lh=0.03, fs=7.4, widths=0.37, hl_rows=(4,))
    items = [('s', '* Beam theory: tongue = 5 mm cantilever, flexural modulus 2 GPa, friction 0.3, two tongues per clip; real values are expected to differ by 50 % or more. The coupons of the proof of concept were printed to measure them.'),
             ('gap', ''),
             ('h', 'If sample 4 is too stiff'),
             ('b', 'Reprint the plates with sample 3 (tongue 1.2) or 2 (tongue 1.0): change T_TONGUE and DELTA at the top of build_slide_mount.py, or the tongue and slit offsets in your sketch (slit y = 3.3 + tongue).'),
             ('b', 'The ring supports do not change.'),
             ('gap', ''),
             ('h', 'If the tongues show white stress marks'),
             ('b', 'The root strain of sample 4 is \u2248 2.7 %. Stress marks after cycling mean the tongue is at its limit: go to sample 3.')]
    textblock(fig, 0.60, 0.60, wrap(items, 60), fs=7.5, lh=0.0245)
    ax = fig.add_axes([0.62, 0.04, 0.33, 0.20]); ax.set_aspect('equal'); ax.axis('off'); ax.set_xlim(-9.0, 12.0); ax.set_ylim(-7.2, 7.2)
    xo = -B.XC
    R(ax, xo, xo + B.PLATE_W, -7, 7, ORANGE, z=1)
    m = B.POST_D / 2 - B.DELTA; ye = B.SLOT_R + B.ENTRY_OPEN
    th = np.linspace(np.pi / 2, 3 * np.pi / 2, 50)
    slot = [(B.PLATE_W + xo, ye), (B.X_LUG1 + xo, m), (B.X_LUG0 + xo, m), (B.X_RAMP0 + xo, B.SLOT_R), (0, B.SLOT_R)] + [(B.SLOT_R * np.cos(t), B.SLOT_R * np.sin(t)) for t in th[1:-1]] + [(0, -B.SLOT_R), (B.X_RAMP0 + xo, -B.SLOT_R), (B.X_LUG0 + xo, -m), (B.X_LUG1 + xo, -m), (B.PLATE_W + xo, -ye)]
    POLY(ax, slot, 'white', z=2, lw=0.9)
    for sg in (1, -1):
        ax.add_patch(FancyBboxPatch((B.SLIT_X0 + xo, sg * (B.SLOT_R + B.T_TONGUE + B.SLIT_W / 2) - B.SLIT_W / 2), B.PLATE_W + xo - B.SLIT_X0 - xo + 0.4, B.SLIT_W, boxstyle='round,pad=0,rounding_size=0.45', fc='white', ec=DK, lw=0.8, zorder=2))
    ax.add_patch(Circle((0, 0), 3.0, fc=PLOT, ec=DK, lw=0.8, zorder=3))
    footer(fig, 17, TOTAL)
    return fig



# ============================================================================ 6  plot
def page_plot():
    zt = B.PLANK_Z; zs = zt - B.SH_LEN; zh = zs - B.HEAD_H; zb = B.Z_PLATE_BOT; zp = zb + B.PLATE_T
    top = B.HEAD_H + B.SH_LEN
    fig = plt.figure(figsize=A4)
    page_title(fig, 'Plot \u2014 part drawing and installation (\u00d74)', 'Printed PETG Matte, head on the bed, no support. Axial section, z up. Dimensions in mm.')
    ax = fig.add_axes([0.03, 0.12, 0.36, 0.76]); ax.set_aspect('equal'); ax.axis('off'); ax.set_xlim(-12, 12); ax.set_ylim(-4.5, top + 3.5)
    csk = (B.CSK_D - B.BORE_D) / 2
    right = [(B.CSK_D / 2, 0), (B.HEAD_D / 2 - 0.3, 0), (B.HEAD_D / 2, 0.3), (B.HEAD_D / 2, B.HEAD_H), (B.POST_D / 2, B.HEAD_H), (B.POST_D / 2, top), (B.BORE_D / 2, top), (B.BORE_D / 2, csk)]
    left = [(-x, z) for (x, z) in right]
    POLY(ax, right, PLOT, z=3); POLY(ax, left, PLOT, z=3)
    center_line(ax, (0, -2.0), (0, top + 1.6))
    dim(ax, (-5.0, -1.2), (5.0, -1.2), '\u00d810', tp=(0, -2.1), fs=8); ext(ax, (-5, 0), (-5, -1.6)); ext(ax, (5, 0), (5, -1.6))
    dim(ax, (-3.0, top + 1.2), (3.0, top + 1.2), '\u00d86.0 (shoulder)', tp=(0, top + 2.1), fs=8); ext(ax, (-3, top), (-3, top + 1.6)); ext(ax, (3, top), (3, top + 1.6))
    dim(ax, (-1.7, top - 1.4), (1.7, top - 1.4), '', fs=7); note(ax, (0, top - 2.4), '\u00d83.4 bore', fs=7.5, ha='center')
    dim(ax, (-3.3, 0.5), (3.3, 0.5), '', fs=7); note(ax, (0, 1.2), '\u00d86.6 csk', fs=6.8, ha='center')
    dim(ax, (7.0, 0), (7.0, B.HEAD_H), '', fs=7); note(ax, (8.0, B.HEAD_H / 2), f'{B.HEAD_H}', fs=8); ext(ax, (5, B.HEAD_H), (7.4, B.HEAD_H)); ext(ax, (5, 0), (7.4, 0))
    dim(ax, (7.0, B.HEAD_H), (7.0, top), '', fs=7); note(ax, (8.2, (B.HEAD_H + top) / 2), f'{B.SH_LEN}', fs=8); ext(ax, (3, top), (7.4, top))
    dim(ax, (10.4, 0), (10.4, top), '', fs=7); note(ax, (11.6, top / 2), f'{top:.1f}', fs=8); ext(ax, (7.4, 0), (10.8, 0)); ext(ax, (7.4, top), (10.8, top))
    note(ax, (-11.5, 0.45), '0.3 chamfer', fs=6.8, ha='left'); leader(ax, (-8.0, 0.45), (-4.8, 0.15), color=BLUE)
    note(ax, (-11.5, 2.4), 'countersink\n1.6 deep (90\u00b0)', fs=6.8, ha='left'); leader(ax, (-8.1, 2.6), (-2.6, 0.8), color=BLUE)
    note(ax, (-11.5, 8.0), 'cushions slide on\nthe shoulder (page 7)', fs=6.8, ha='left', color=TPUE); leader(ax, (-8.0, 8.6), (-3.0, 7.0), color=TPUE)
    note(ax, (-11.5, top - 0.2), 'flat top against\nthe plank', fs=6.8, ha='left'); leader(ax, (-8.2, top - 0.6), (-2.8, top), color=BLUE)
    ax.annotate('', xy=(-9.5, -3.6), xytext=(-9.5, -2.6), arrowprops=dict(arrowstyle='-|>', lw=1.0, color=RED))
    note(ax, (-9.0, -3.3), 'print: head down on the bed (z = 0)', fs=7, ha='left', color=RED)
    ax2 = fig.add_axes([0.42, 0.14, 0.26, 0.72]); ax2.set_aspect('equal'); ax2.axis('off'); ax2.set_xlim(-12, 12); ax2.set_ylim(8, 56)
    wood(ax2, -12, 12, zt, zt + 16)
    R(ax2, -12, -3.3, zb, zp, ORANGE, z=3); R(ax2, 3.3, 12, zb, zp, ORANGE, z=3)
    for zc0 in (zt - B.CUSH_T, zb - B.GAP - B.CUSH_T):
        for sg in (-1, 1):
            R(ax2, sg * 3.0, sg * 6.0, zc0, zc0 + B.CUSH_T, TPUF, ec=TPUE, lw=0.8, z=3.5)
    R(ax2, -3, 3, zs, zt, PLOT, z=4); R(ax2, -5, 5, zh, zs, PLOT, z=4)
    POLY(ax2, [(-3.3, zh), (3.3, zh), (1.7, zh + 1.6), (-1.7, zh + 1.6)], 'white', z=5)
    R(ax2, -1.7, 1.7, zh + 1.6, zt, 'white', ec='none', z=5)
    POLY(ax2, [(-3.0, zh), (3.0, zh), (1.5, zh + 1.5), (-1.5, zh + 1.5)], SCREWC, z=6, lw=0.6)
    R(ax2, -1.5, 1.5, zh + 1.5, zh + 25, SCREWC, lw=0.6, z=6)
    center_line(ax2, (0, zh - 2), (0, zt + 18))
    dim(ax2, (-8.5, zh), (-8.5, zh + 25), '\u00d83 \u00d7 25', tp=(-10.9, zh + 12.5), fs=7.5)
    dim(ax2, (8.0, zt), (8.0, zh + 25), '', fs=7); note(ax2, (8.4, (zt + zh + 25) / 2 + 1), f'\u2248 {25 - top:.0f} in\nthe wood', fs=7)
    note(ax2, (8.4, zt + 16 + 1.5), 'plank \u2265 15', fs=7, color='#7a5a20')
    note(ax2, (6.4, 31.0), 'plate', fs=7, ha='left')
    note(ax2, (6.4, zt - 0.75), 'cushion', fs=6.6, ha='left', color=TPUE); note(ax2, (6.4, zb - B.GAP - B.CUSH_T / 2), 'cushion', fs=6.6, ha='left', color=TPUE)
    note(ax2, (-11.8, 54.8), 'pilot \u00d82.0\u20132.2 \u00d7 14', fs=7, ha='left', color=RED)
    ax2.set_title('Installed (plot A shown)', fontsize=8.2, color=BLUE, loc='left', fontweight='bold')
    items = [('h', 'Fit and print'),
             ('b', 'The shoulder diameter sets the clip interference (0.30 per side at \u00d86.0) and the grip of the cushions: measure the printed plot with calipers, 6.0 \u00b1 0.05.'),
             ('b', 'Head down, no support: the countersink is a 45\u00b0 cone, the 0.3 chamfer takes the first-layer squash.'),
             ('b', '4 walls: the 1.3 mm wall around the bore is solid. The plot is lightly loaded: \u2248 30 N axial (the couple) and a few N sideways, \u2248 2 MPa across the layers at the head (PETG Matte: 30 MPa).'),
             ('gap', ''),
             ('h', 'Hardware'),
             ('b', f'Wood screw \u00d83 \u00d7 25, countersunk head \u00d86, pilot \u00d82.0\u20132.2, \u2248 {25 - top:.0f} mm in the wood. With a plank thinner than 15 mm use \u00d83 \u00d7 20.'),
             ('b', 'Alternative: M5 shoulder screw (shoulder \u00d86 \u00d7 10) in an M5 wood insert, with the same two cushions.')]
    textblock(fig, 0.71, 0.88, wrap(items, 46), fs=7.5, lh=0.024)
    footer(fig, 6, TOTAL)
    return fig


# ============================================================================ 7  silent blocks: TPU cushions
def wheel_view(ax, style='wheel'):
    ax.set_aspect('equal'); ax.axis('off')
    ro, ri, w = B.CUSH_OD / 2, B.CUSH_ID / 2, 1.0
    if style == 'solid':
        ax.add_patch(Wedge((0, 0), ro, 0, 360, width=ro - ri, fc=TPUF, ec=TPUE, lw=1.0, zorder=3))
    else:
        ax.add_patch(Wedge((0, 0), ro, 0, 360, width=w, fc=TPUF, ec=TPUE, lw=1.0, zorder=3))
        ax.add_patch(Wedge((0, 0), ri + w, 0, 360, width=w, fc=TPUF, ec=TPUE, lw=1.0, zorder=3))
        for k in range(6):
            a = math.radians(60 * k); ux, uy = math.cos(a), math.sin(a); tx, ty = -uy, ux
            r0, r1 = ri + w - 0.1, ro - w + 0.1
            pts = [(ux * r0 + tx * 0.5, uy * r0 + ty * 0.5), (ux * r1 + tx * 0.5, uy * r1 + ty * 0.5), (ux * r1 - tx * 0.5, uy * r1 - ty * 0.5), (ux * r0 - tx * 0.5, uy * r0 - ty * 0.5)]
            POLY(ax, pts, TPUF, ec=TPUE, lw=0.8, z=3)
    ax.add_patch(Circle((0, 0), B.POST_D / 2, fc='none', ec=GREY, lw=0.7, ls=(0, (3, 2)), zorder=6))
    ax.set_xlim(-8.5, 8.5); ax.set_ylim(-8.5, 8.5)


def page_cushion():
    kw, ks = cushion_k('wheel'), cushion_k('solid')
    fig = plt.figure(figsize=A4)
    page_title(fig, 'Silent blocks \u2014 TPU cushions (\u00d78) around the plate on each plot',
               'Flat ring washers printed in TPU 85A, 0\u00b0 overhang, no support, no small feature. Two per plot, friction fit on the \u00d86.0 shoulder. Dimensions in mm.')
    ax = fig.add_axes([0.02, 0.50, 0.25, 0.38]); wheel_view(ax, 'wheel')
    dim(ax, (-B.CUSH_OD / 2, -7.2), (B.CUSH_OD / 2, -7.2), '\u00d812', tp=(0, -8.0), fs=8)
    note(ax, (0.0, 7.4), 'wheel (default)', fs=8.2, ha='center', color=BLUE, bold=True)
    note(ax, (-8.3, 5.2), 'outer ring 1.0', fs=6.8, ha='left'); note(ax, (-8.3, 3.9), 'inner ring 1.0', fs=6.8, ha='left')
    note(ax, (-8.3, -3.9), '6 spokes 1.0', fs=6.8, ha='left')
    note(ax, (2.6, 0.0), 'ID 5.8', fs=6.8, ha='left', color='#555555')
    ax = fig.add_axes([0.28, 0.50, 0.17, 0.38]); wheel_view(ax, 'solid')
    note(ax, (0.0, 7.4), 'solid (stiff variant)', fs=8.2, ha='center', color=BLUE, bold=True)
    note(ax, (0.0, -7.6), f'thickness {B.CUSH_T}, both variants', fs=7.2, ha='center')
    ax = fig.add_axes([0.47, 0.47, 0.27, 0.41]); endview(ax); ax.set_xlim(-45, -29); ax.set_ylim(22, 38.5)
    ax.set_title('Installed, section through plot A', fontsize=8.2, color=BLUE, loc='left', fontweight='bold')
    zt, zb = B.PLANK_Z, B.Z_PLATE_BOT
    note(ax, (-44.8, 36.6), 'top cushion 1.5, against the plank', fs=6.8, color=TPUE)
    note(ax, (-44.8, 31.2), f'plate floats: {B.GAP} above and below', fs=6.8)
    note(ax, (-44.8, 22.9), 'bottom cushion 1.5, on the head', fs=6.8, color=TPUE)
    items = [('h', 'What it does'),
             ('b', f'The plate sits between two cushions with {B.GAP} mm of play on each side: nothing is pinched, so it slides in without friction and the clip is not disturbed.'),
             ('b', 'Under torque or weight the plate rests on a cushion, never on PETG or wood. The play is a few tenths of a degree of rotation.'),
             ('b', 'Both directions of rotation are covered: the post that is pushed up meets the top cushion, the one pushed down meets the bottom cushion.')]
    textblock(fig, 0.755, 0.885, wrap(items, 34), fs=7.3, lh=0.0235)
    rows = [['Variant', 'Stiffness*', 'Natural freq.*', 'Use'],
            ['wheel (default)', f'\u2248 {kw:.0f} N/mm', f'\u2248 {f_natural(kw):.0f} Hz', 'filters from \u2248 650 Hz'],
            ['solid', f'\u2248 {ks:.0f} N/mm', f'\u2248 {f_natural(ks):.0f} Hz', 'resonates near 1 kHz: avoid'],
            ['wheel, 2 walls', 'softer', 'lower', 'first thing to try']]
    table(fig, 0.03, 0.445, [0.0, 0.12, 0.23, 0.34], rows, lh=0.027, fs=7.4, widths=0.46, hl_rows=(1,))
    fig.text(0.03, 0.325, f'* TPU 85A E \u2248 {E_TPU:.0f} MPa (assumed), 4 bottom cushions in parallel, mass {M_MOTOR} kg; page 8. Measure before trusting.', fontsize=6.6, color='#555555')
    items = [('h', 'Print and fit'),
             ('b', 'TPU 85A on the external spool, dried (70 \u00b0C, 8 h), 225 \u00b0C, bed 35 \u00b0C, 20 mm/s, fan 100 %, textured plate without glue (settings page 13). 2 walls, thin-wall detection on, 3 top and bottom layers.'),
             ('b', 'The inner ring is \u00d75.8 on a \u00d86.0 shoulder: a firm friction fit (0.1 mm per side) keeps each cushion where it is put. TPU printed undersize needs no more; if it slips, print the ID at 5.7.'),
             ('b', 'Stiffness tuning: print 3 sets at 2 / 3 / 4 walls, measure on the plank with the ADXL345, keep the softest set that does not let the motor move under load.')]
    textblock(fig, 0.03, 0.305, wrap(items, 92), fs=7.3, lh=0.0225)
    items = [('h', 'Set the thickness for your desk'),
             ('b', 'The plate is fixed to the ring, the plots hang from the plank: the two plays depend on H, the real distance from the shaft axis to the plank underside (36.0 assumed).'),
             ('b', f'With d = H \u2212 36: top cushion {B.CUSH_T} + d, bottom cushion {B.CUSH_T} \u2212 d keep {B.GAP} mm of play on both sides (layers of 0.2). Or set PLANK_Z in build_slide_mount.py: the plate height, the pad and the plots are recomputed.'),
             ('b', 'Measure H on the desk before printing the ring supports and the plots.')]
    textblock(fig, 0.55, 0.445, wrap(items, 66), fs=7.3, lh=0.0235)
    footer(fig, 7, TOTAL)
    return fig


# ============================================================================ 8  vibration paths and estimate
def box(ax, x, y, w, h, txt, fc='#f4f4f4', ec=DK, fs=7.4, tc=DK, bold=False):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle='round,pad=0.02,rounding_size=0.12', fc=fc, ec=ec, lw=1.0, zorder=3))
    ax.text(x + w / 2, y + h / 2, txt, fontsize=fs, ha='center', va='center', color=tc, zorder=4, fontweight='bold' if bold else 'normal', linespacing=1.2)


def arrow(ax, p, q, color=DK, ls='-', lw=1.3):
    ax.annotate('', xy=q, xytext=p, arrowprops=dict(arrowstyle='-|>', lw=lw, color=color, ls=ls), zorder=5)


def page_paths():
    kw, ks = cushion_k('wheel'), cushion_k('solid')
    fn_w, fn_s = f_natural(kw), f_natural(ks)
    FA, FB, w_ = split_two_posts(kw, B.GAP)
    Fnom = T_NOM / 2 * 1000.0 / (LEVER_A + LEVER_B)

    def T(f, fn):
        r = f / fn
        return math.sqrt(1 + (2 * ZETA * r) ** 2) / math.sqrt((1 - r * r) ** 2 + (2 * ZETA * r) ** 2)
    fig = plt.figure(figsize=A4)
    page_title(fig, 'Vibration paths and isolation estimate', 'Two elastic elements, one per path. The numbers are estimates from assumed TPU properties: they tell what to expect, the ADXL345 tells what you get.')
    ax = fig.add_axes([0.02, 0.50, 0.50, 0.38]); ax.axis('off'); ax.set_xlim(0, 10.5); ax.set_ylim(0, 6.2)
    box(ax, 0.1, 2.4, 1.9, 1.5, 'drill motor\n\u2248 0.3 kg', fc='#ececec', bold=True)
    box(ax, 2.6, 4.5, 2.1, 1.1, 'ring supports\n+ clip plates')
    box(ax, 5.1, 4.5, 2.0, 1.1, 'TPU cushions\n(8, page 7)', fc=TPUF, ec=TPUE, tc=TPUE, bold=True)
    box(ax, 7.5, 4.5, 1.5, 1.1, 'plots +\nscrews')
    box(ax, 9.2, 4.5, 1.2, 1.1, 'plank\n(desk)', fc='#f3ead8', ec=WOOD)
    box(ax, 2.6, 0.5, 2.6, 1.1, 'jaw coupling\nTPU spider (14\u201316)', fc=TPUF, ec=TPUE, tc=TPUE, bold=True)
    box(ax, 5.8, 0.5, 2.4, 1.1, 'desk crank axis', fc='#ececec')
    box(ax, 8.6, 0.5, 1.8, 1.1, 'desk frame', fc='#f3ead8', ec=WOOD)
    arrow(ax, (2.0, 3.5), (2.6, 4.8)); arrow(ax, (4.7, 5.05), (5.1, 5.05)); arrow(ax, (7.1, 5.05), (7.5, 5.05)); arrow(ax, (9.0, 5.05), (9.2, 5.05))
    arrow(ax, (2.0, 2.8), (2.6, 1.2)); arrow(ax, (5.2, 1.05), (5.8, 1.05)); arrow(ax, (8.2, 1.05), (8.6, 1.05))
    ax.text(3.6, 5.85, 'mount path: torque ripple and weight, vertical at the plots', fontsize=7.4, color=BLUE, fontweight='bold')
    ax.text(2.6, 0.12, 'shaft path: torque and axial / radial shaft vibration', fontsize=7.4, color=BLUE, fontweight='bold')
    box(ax, 4.4, 2.5, 5.6, 1.2, 'rigid bypasses left: cables, the slot end (x), the plate on\nthe plank above a 0.3 mm travel (overload only)', fc='white', ec=RED, tc=RED, fs=7.0)
    ax.plot([5.4, 5.4], [3.7, 4.5], color=RED, lw=1.0, ls=(0, (3, 2)))
    ax2 = fig.add_axes([0.56, 0.52, 0.41, 0.34])
    fr = np.logspace(math.log10(50), math.log10(5000), 400)
    ax2.loglog(fr, [T(f, fn_w) for f in fr], color=TPUE, lw=1.8, label=f'wheel, f\u2099 \u2248 {fn_w:.0f} Hz')
    ax2.loglog(fr, [T(f, fn_s) for f in fr], color=RED, lw=1.2, ls='--', label=f'solid, f\u2099 \u2248 {fn_s:.0f} Hz')
    ax2.axhline(1.0, color=GREY, lw=1.0, ls=':', label='rigid (previous version here)')
    for f0, lab in ((300, 'rotor\n(assumed)'), (1000, '1 kHz'), (2000, '2 kHz\n(gears, assumed)')):
        ax2.axvline(f0, color='#999999', lw=0.7, ls='-.'); ax2.text(f0 * 1.04, 0.012, lab, fontsize=6.4, color='#555555', va='bottom')
    ax2.set_xlim(50, 5000); ax2.set_ylim(0.01, 10)
    ax2.set_xlabel('frequency (Hz)', fontsize=7.5); ax2.set_ylabel('transmissibility (force into the plank / force from the motor)', fontsize=6.6)
    ax2.tick_params(labelsize=7); ax2.grid(True, which='both', lw=0.3, alpha=0.5)
    ax2.legend(fontsize=6.8, loc='upper left', frameon=False)
    ax2.set_title(f'Vertical transmissibility, damping ratio {ZETA}', fontsize=8.2, color=BLUE, loc='left', fontweight='bold')
    rows = [['Quantity (estimate)', 'wheel', 'solid'],
            ['cushion stiffness', f'{kw:.0f} N/mm', f'{ks:.0f} N/mm'],
            ['natural frequency (4 bottom cushions)', f'{fn_w:.0f} Hz', f'{fn_s:.0f} Hz'],
            ['transmissibility at 1 kHz', f'{T(1000, fn_w):.2f}  ({20 * math.log10(T(1000, fn_w)):.0f} dB)', f'{T(1000, fn_s):.1f}  (resonance)'],
            ['transmissibility at 2 kHz', f'{T(2000, fn_w):.3f}  ({20 * math.log10(T(2000, fn_w)):.0f} dB)', f'{T(2000, fn_s):.2f}  ({20 * math.log10(T(2000, fn_s)):.0f} dB)'],
            ['deflection under the nominal couple', f'{Fnom / kw:.2f} mm', f'{Fnom / ks:.2f} mm'],
            ['rotation to take up the plays', f'{math.degrees(B.GAP / LEVER_B):.1f}\u00b0 at B, {math.degrees(B.GAP / LEVER_A):.1f}\u00b0 at A', 'same']]
    table(fig, 0.03, 0.455, [0.0, 0.24, 0.36], rows, lh=0.028, fs=7.3, widths=0.50, hl_rows=())
    items = [('h', 'Reading'),
             ('b', f'The wheel isolates above about {1.41 * fn_w:.0f} Hz: gear whine and PWM harmonics, not the rotor frequency. For lower frequencies the cushion would have to be much softer, which the 3.3 N\u00b7m torque and the 0.3 mm alignment tolerance do not allow.'),
             ('b', 'The solid washer is stiffer and sits on a resonance near 1 kHz: keep it as a spare, not as the default.'),
             ('h', 'Torque split between the two plots of a support'),
             ('b', f'Nominal couple per support {T_NOM / 2:.2f} N\u00b7m: {Fnom:.0f} N if the posts shared it equally. With equal plays and cushions, post A (lever 37) meets its cushion first: {FA:.0f} N at A, {FB:.0f} N at B, and the {FA - FB:.0f} N difference goes radially through the coupling (assumed {KR:.0f} N/mm): axis shift \u2248 {abs(w_):.2f} mm, inside the 0.3 mm the coupling accepts.'),
             ('b', 'Smaller plays reduce it (GAP in the script); matching the plays to the lever arms (about 2:1 between A and B) nearly removes it. Check the axis offset on the desk (page 18).')]
    textblock(fig, 0.56, 0.455, wrap(items, 62), fs=7.2, lh=0.0225)
    items = [('h', 'Assumptions'),
             ('s', 'TPU 85A printed E \u2248 12 MPa; damping ratio 0.10; moving mass 0.3 kg (motor 0.2\u20130.3 kg from your note, plus two supports); rotor and gear frequencies are typical values: replace them by your own spectrum.'),
             ('gap', ''),
             ('h', 'Measure it'),
             ('b', 'ADXL345 on the plank near plot A and on the ring support, same axis, same PWM. Record 10 s at three PWM values. The ratio of the two spectra is the transmissibility: compare with this curve and with the rigid version (cushions removed, plate resting on the heads).')]
    textblock(fig, 0.03, 0.255, wrap(items, 84), fs=7.3, lh=0.0225)
    footer(fig, 8, TOTAL)
    return fig



# ============================================================================ 9-10  ring supports (geometry of your two original supports)
def pad_only(ax, zb=None):
    P = B
    zb = P.Z_PLATE_BOT if zb is None else zb
    R(ax, -P.PAD_Y, P.PAD_Y, 24.4, zb, RING, ec='none', z=4.2)
    ax.plot([-P.PAD_Y, -P.PAD_Y], [24.2, zb], color=DK, lw=0.8, zorder=4.3); ax.plot([P.PAD_Y, P.PAD_Y], [24.2, zb], color=DK, lw=0.8, zorder=4.3)
    ax.plot([-P.PAD_Y, P.SCREW_Y[0] - P.INSERT_D / 2], [zb, zb], color=DK, lw=0.8, zorder=4.4)
    ax.plot([P.SCREW_Y[0] + P.INSERT_D / 2, P.SCREW_Y[1] - P.INSERT_D / 2], [zb, zb], color=DK, lw=0.8, zorder=4.4)
    ax.plot([P.SCREW_Y[1] + P.INSERT_D / 2, P.PAD_Y], [zb, zb], color=DK, lw=0.8, zorder=4.4)
    for ys in P.SCREW_Y:
        R(ax, ys - P.INSERT_D / 2, ys + P.INSERT_D / 2, zb - P.INSERT_DEPTH, zb, 'white', lw=0.7, z=4.5)


def rear_section(ax, section, title):
    ax.set_aspect('equal'); ax.axis('off'); ax.set_xlim(-34, 34); ax.set_ylim(-34, 36)
    ring_section(ax, 'rear', section)
    pad_only(ax)
    center_line(ax, (0, -30), (0, 31)); ax.plot([-4, 4], [0, 0], color=DK, lw=0.5, zorder=9)
    ax.set_title(title, fontsize=8.4, color=BLUE, loc='left', fontweight='bold')
    dim(ax, (-24.25, -2.5), (24.25, -2.5), '', fs=7); note(ax, (0, -5.4), '\u00d848.5', fs=7.6, ha='center', bold=True)
    dim(ax, (-27, -31), (27, -31), '\u00d854', tp=(0, -33.0), fs=7.6); ext(ax, (-27, -27), (-27, -31.6)); ext(ax, (27, -27), (27, -31.6))
    return ax


def page_ring_rear():
    P = B
    fig = plt.figure(figsize=A4)
    page_title(fig, 'Ring support motor_mount (\u00d71) \u2014 geometry of your original',
               'Ring, side slots and windows taken from motor_mount_1.SLDPRT: every original vertex lies within 0.002 mm of this model. Ear, blade and gussets give way to the pad. Dimensions in mm.')
    ax = fig.add_axes([0.01, 0.43, 0.235, 0.45]); rear_section(ax, 'rim', 'A\u2013A  x = 0 \u2192 3  (closed rim)')
    note(ax, (P.RELIEF_Y - 0.4, 9.0), 'flat y = 26\n|z| < 7.3', fs=6.8, ha='right', color=RED)
    ax = fig.add_axes([0.25, 0.43, 0.235, 0.45]); rear_section(ax, 'slot', 'B\u2013B  x = 3 \u2192 6  (side slots)')
    dim(ax, (-31.0, -P.REAR_SLOT_Z), (-31.0, P.REAR_SLOT_Z), '', fs=7); note(ax, (-29.0, 8.0), '8.5', fs=7.6, ha='left', color=RED)
    ax = fig.add_axes([0.49, 0.43, 0.235, 0.45]); rear_section(ax, 'window', 'C\u2013C  x = 6 \u2192 15  (side windows)')
    dim(ax, (-31.0, -P.REAR_WIN_Z), (-31.0, P.REAR_WIN_Z), '', fs=7); note(ax, (-29.5, 16.5), '22.6', fs=7.6, ha='left', color=RED)
    # side elevation (x horizontal, z vertical), viewed from +y, cropped to |z| < 17 (the ring continues to z = +-27)
    ax = fig.add_axes([0.745, 0.44, 0.24, 0.44]); ax.set_aspect('equal'); ax.axis('off'); ax.set_xlim(-5, 22); ax.set_ylim(-19.5, 21.5)
    R(ax, 0, 15, -17, 17, RING, z=3)
    for zz in (17, -17):
        ax.plot([0, 15], [zz, zz], color=DK, lw=0.8, ls=(0, (6, 2)), zorder=5)
    R(ax, 3, 6, -P.REAR_SLOT_Z, P.REAR_SLOT_Z, 'white', z=4); R(ax, 6, 15.4, -P.REAR_WIN_Z, P.REAR_WIN_Z, 'white', ec='none', z=4)
    for zz in (P.REAR_WIN_Z, -P.REAR_WIN_Z):
        ax.plot([6, 15], [zz, zz], color=DK, lw=0.8, zorder=5)
    ax.plot([6, 6], [-P.REAR_WIN_Z, P.REAR_WIN_Z], color=DK, lw=0.8, zorder=5)
    for xx, lab in ((1.5, 'A'), (4.5, 'B'), (10.5, 'C')):
        ax.plot([xx, xx], [-17.5, 17.5], color=BLUE, lw=0.7, ls=(0, (6, 3)), zorder=6); note(ax, (xx, 19.0), lab, fs=8, ha='center', color=BLUE, bold=True)
    for (x0, x1, t) in ((0, 3, '3'), (3, 6, '3'), (6, 15, '9')):
        dim(ax, (x0, -18.4), (x1, -18.4), t, tp=((x0 + x1) / 2, -19.6), fs=7.2)
    dim(ax, (17.0, -P.REAR_WIN_Z), (17.0, P.REAR_WIN_Z), '', fs=7); note(ax, (17.6, 0.0), '22.6', fs=7.4, ha='left', color=RED)
    dim(ax, (-1.4, -P.REAR_SLOT_Z), (-1.4, P.REAR_SLOT_Z), '', fs=7); note(ax, (-1.9, 0.0), '8.5', fs=7.4, ha='right', color=RED)
    note(ax, (11.0, 13.2), 'x = 0 on the bed:\nthe steps face up', fs=6.6, ha='center', color=RED)
    ax.set_title('Side view from +y (side wall, |z| < 17)', fontsize=8.0, color=BLUE, loc='left', fontweight='bold')
    rows = [['Feature', 'Value', 'Read in'],
            ['bore / outside, width', '\u00d748.5 / \u00d754, 15', 'cylinders R24.25, R27, x 0 \u2192 15'],
            ['side slot, both sides', 'x 3 \u2192 6, |z| < 4.25', 'planes x = 3, x = 6, z = \u00b14.25'],
            ['side window, both sides', 'x 6 \u2192 15, |z| < 11.3', 'planes x = 6, z = \u00b111.3'],
            ['relief flat on the wood side', 'y = 26, |z| < 7.3, x 0 \u2192 6', 'plane y = 26 (your 45\u00b0 chamfer up to the blade is not kept)'],
            ['pad, pockets (new)', f'24 wide, top z = {P.Z_PLATE_BOT:.1f}', f'pockets \u00d84.0 \u00d7 {P.INSERT_DEPTH} at y = \u00b18']]
    table(fig, 0.03, 0.385, [0.0, 0.17, 0.36], rows, lh=0.0275, fs=7.3, widths=0.56)
    items = [('h', 'What these steps are'),
             ('b', 'They are the detrompeurs of the drill body: the ring is closed for the first 3 mm, narrows to side slots for 3 mm, then opens into two side windows. The drill cannot be fitted the wrong way round and the windows take its torque.'),
             ('b', 'Printed with the x = 0 rim on the bed (closed ring down), the steps face upward and the windows open upward: no support, nothing to bridge. Same orientation as the file.'),
             ('b', 'Behind the rim the ring is two arcs: the pad sits on the upper one, carried by the 6 mm rim. 4 walls, and look at the first print for flex under the couple.'),
             ('b', 'Not reproduced: ear, blade, gussets and fillets of your original, replaced by the pad.')]
    textblock(fig, 0.64, 0.385, wrap(items, 60), fs=7.3, lh=0.0235)
    footer(fig, 9, TOTAL)
    return fig


def slot_detail(ax, wd, title, key=False):
    P = B
    ax.set_aspect('equal'); ax.axis('off'); ax.set_xlim(-8.5, 8.5); ax.set_ylim(21.5, 29.5)
    R(ax, -8, 8, 24.0, 27.0, RING, z=3)
    R(ax, -wd / 2, wd / 2, 23.0, P.FRONT_SLOT_BOTTOM, 'white', ec=(RED if key else DK), lw=(1.4 if key else 0.9), z=4)
    ax.plot([-8, 8], [24.0, 24.0], color=DK, lw=0.8, zorder=5)
    R(ax, -wd / 2, wd / 2, 23.0, 24.0, 'white', ec='none', z=4.2)
    dim(ax, (-wd / 2, 22.4), (wd / 2, 22.4), '', fs=7); note(ax, (0, 21.9), f'{wd}', fs=8, ha='center', color=(RED if key else DK), bold=True)
    dim(ax, (wd / 2 + 1.2, 24.0), (wd / 2 + 1.2, P.FRONT_SLOT_BOTTOM), '', fs=7); note(ax, (wd / 2 + 1.5, 25.0), '2.0', fs=7.4, ha='left')
    dim(ax, (wd / 2 + 1.2, P.FRONT_SLOT_BOTTOM), (wd / 2 + 1.2, 27.0), '', fs=7); note(ax, (wd / 2 + 1.5, 26.5), '1.0', fs=7.0, ha='left')
    ax.set_title(title, fontsize=8.0, color=(RED if key else BLUE), loc='left', fontweight='bold')


def page_ring_front():
    P = B
    fig = plt.figure(figsize=A4)
    page_title(fig, 'Ring support motor_front_mount (\u00d71) \u2014 four slots, one of them the detrompeur',
               'From motor_front_mount_1.SLDPRT, within 0.002 mm. Desk frame (wood on +y): your file is mirrored, so its wide slot at 225\u00b0 is at 315\u00b0 here (bottom right), as you confirmed.')
    ax = fig.add_axes([0.02, 0.07, 0.46, 0.80]); ax.set_aspect('equal'); ax.axis('off'); ax.set_xlim(-36, 36); ax.set_ylim(-34, 40)
    ring_section(ax, 'front', 'x'); pad_only(ax)
    center_line(ax, (0, -31), (0, 33)); ax.plot([-4, 4], [0, 0], color=DK, lw=0.5, zorder=9)
    dim(ax, (-24, -3.0), (24, -3.0), '', fs=7); note(ax, (0, -6.0), '\u00d848', fs=8, ha='center', bold=True)
    dim(ax, (-27, -32), (27, -32), '\u00d854', tp=(0, -34), fs=8); ext(ax, (-27, -27), (-27, -32.6)); ext(ax, (27, -27), (27, -32.6))
    for ang, wd in P.front_slots():
        a = math.radians(ang); r = 30.5
        note(ax, (r * math.cos(a), r * math.sin(a)), f'{ang:.0f}\u00b0', fs=7.4, ha='center', color=(RED if wd > 5 else '#555555'), bold=(wd > 5))
    ka = math.radians(P.FRONT_KEY_ANGLE); kx, ky = 25.0 * math.cos(ka), 25.0 * math.sin(ka)
    if kx > 0:
        note(ax, (kx + 4.5, ky + 2.0), 'detrompeur 6.2', fs=7.6, ha='left', color=RED, bold=True); leader(ax, (kx + 4.2, ky + 1.2), (kx + 0.8, ky - 0.5), color=RED)
    else:
        note(ax, (kx - 4.5, ky + 8.0), 'detrompeur 6.2', fs=7.6, ha='right', color=RED, bold=True); leader(ax, (kx - 4.0, ky + 7.0), (kx - 0.5, ky + 1.0), color=RED)
    note(ax, (-35.0, 6.0), 'narrow slots\n4.4 \u00d7 3', fs=7.2, ha='left', color='#444444')
    note(ax, (P.RELIEF_Y + 0.7, 9.5), 'flat y = 26\n|z| < 7.3', fs=6.8, ha='left', color=RED)
    ax.set_title('End view (desk frame)', fontsize=8.4, color=BLUE, loc='left', fontweight='bold')
    a1 = fig.add_axes([0.52, 0.60, 0.20, 0.27]); slot_detail(a1, 4.4, 'narrow slot (45\u00b0, 135\u00b0, 225\u00b0)')
    a2 = fig.add_axes([0.75, 0.60, 0.22, 0.27]); slot_detail(a2, 6.2, 'wide slot (315\u00b0): the key', key=True)
    rows = [['Feature', 'Value', 'Read in'],
            ['bore / outside, width', '\u00d748 / \u00d754, 12', 'cylinders R24, R27, x 0 \u2192 12'],
            ['slots, flat bottom', '26.0 from the axis', '2.0 beyond the bore, on the diagonals'],
            ['narrow slots (\u00d73)', '4.4 wide, at 45\u00b0, 135\u00b0, 225\u00b0', 'planes at \u00b12.2 from the diagonal'],
            ['wide slot, the key (\u00d71)', '6.2 wide, at 315\u00b0 (bottom right)', 'planes at \u00b13.1 from the diagonal'],
            ['relief flat on the wood side', 'y = 26, |z| < 7.3, full width', 'plane y = 26'],
            ['pad, pockets (new)', f'24 wide, top z = {P.Z_PLATE_BOT:.1f}', f'pockets \u00d84.0 \u00d7 {P.INSERT_DEPTH} at y = \u00b18']]
    table(fig, 0.52, 0.52, [0.0, 0.135, 0.30], rows, lh=0.0275, fs=7.2, widths=0.46)
    items = [('h', 'Orientation and print'),
             ('b', 'The slots are prismatic along x and the ring is symmetric end to end: either face can go on the bed. 4 walls, no support.'),
             ('b', 'The key is at the bottom right on this view (coupler side, wood on the right): your file has it at 225\u00b0 in its own frame (wood on the left), which becomes 315\u00b0 here. FRONT_KEY_ANGLE in the script moves it.'),
             ('b', 'Width 12, as in your file (the ear widening to 15 was not applied there); the plate overhangs the ring by 1.5 mm each side.')]
    textblock(fig, 0.52, 0.30, wrap(items, 62), fs=7.3, lh=0.0235)
    footer(fig, 10, TOTAL)
    return fig





def page_layout(L=80.0):
    fig = plt.figure(figsize=A4)
    page_title(fig, 'Plank layout \u2014 drilling points for the four plots', 'View from below (plank underside), motor axis x horizontal toward the coupler. The spacing L between the two supports is not known yet (placeholder 80 mm). Dimensions in mm.')
    ax = fig.add_axes([0.02, 0.10, 0.62, 0.78]); ax.set_aspect('equal'); ax.axis('off'); ax.set_xlim(-26, L + 26); ax.set_ylim(-52, 40)
    wood(ax, -24, L + 24, -44.48, 44.48)
    ax.add_patch(Rectangle((-24, 27), L + 48, 7, fc='#e6d5b2', ec=WOOD, lw=0.8, hatch='\\\\\\\\', zorder=2))
    note(ax, (-23, 30.4), 'vertical wood (face y = 27)', fs=7, color='#7a5a20')
    center_line(ax, (-24, 0), (L + 24, 0), z=9); note(ax, (L + 12, 2.0), 'motor axis y = 0', fs=7, ha='left')
    for xc, w, nm in ((0.0, 15.0, 'rear support (motor_mount)'), (L, 12.0, 'front support')):
        R(ax, xc - 7.5, xc + 7.5, B.PLATE_Y0, B.PLATE_Y1, ORANGE, ec=DK, lw=0.8, z=4)
        R(ax, xc - w / 2, xc + w / 2, -27, 26, 'none', ec=BLUE, lw=0.9, z=5).set_linestyle((0, (4, 3)))
        for y in (B.Y_POST_EAR, B.Y_POST_WOOD):
            ax.add_patch(Circle((xc, y), B.HEAD_D / 2, fc='none', ec=GREY, lw=0.7, ls=(0, (3, 2)), zorder=6))
            ax.add_patch(Circle((xc, y), B.POST_D / 2, fc=PLOT, ec=DK, lw=0.8, zorder=7))
            ax.add_patch(Circle((xc, y), 1.7, fc='white', ec=DK, lw=0.6, zorder=8))
            center_line(ax, (xc - 7.5, y), (xc + 7.5, y), z=9)
        note(ax, (xc, -48.6), nm, fs=7.4, ha='center', color=BLUE, bold=True)
        center_line(ax, (xc, -46), (xc, 40), z=9)
    # dimensions
    for xc in (0.0, L):
        ext(ax, (xc + 8.0, B.Y_POST_EAR), (xc + 15, B.Y_POST_EAR)); ext(ax, (xc + 8.0, B.Y_POST_WOOD), (xc + 15, B.Y_POST_WOOD)); ext(ax, (xc + 8.0, 0), (xc + 15, 0))
        dim(ax, (xc + 13, 0), (xc + 13, B.Y_POST_EAR), '37', tp=(xc + 15.3, B.Y_POST_EAR / 2), fs=7.8)
        dim(ax, (xc + 13, 0), (xc + 13, B.Y_POST_WOOD), '19', tp=(xc + 15.3, B.Y_POST_WOOD / 2), fs=7.8)
    dim(ax, (-14, B.PLATE_Y0 - 0.0), (-14, -44.48), '', fs=7) if False else None
    dim(ax, (0, 38.5), (L, 38.5), 'L = distance between the ring centres (to measure)', tp=(L / 2, 36.6), fs=8, color=RED)
    ext(ax, (0, 34.5), (0, 39.2)); ext(ax, (L, 34.5), (L, 39.2))
    dim(ax, (-9.5, -44.48), (-9.5, B.Y_POST_EAR), '', fs=7); note(ax, (-10.0, -41.0), '7.5 to the\nplank edge', fs=6.8, ha='right')
    ext(ax, (-9.5, B.Y_POST_EAR), (-1.0, B.Y_POST_EAR))
    dim(ax, (-13.0, B.Y_POST_WOOD), (-13.0, 27), '', fs=7); note(ax, (-13.6, 23.0), '8.0 to the\nwood face', fs=6.8, ha='right')
    ext(ax, (-13.0, 27), (-8.0, 27)); ext(ax, (-13.0, B.Y_POST_WOOD), (-6.5, B.Y_POST_WOOD))
    rows = [['Point', 'x', 'y', 'Drill'],
            ['P1  rear, ear side', '0', '\u221237.0', '\u00d82.0\u20132.2 \u00d7 14'],
            ['P2  rear, wood side', '0', '+19.0', '\u00d82.0\u20132.2 \u00d7 14'],
            ['P3  front, ear side', 'L', '\u221237.0', '\u00d82.0\u20132.2 \u00d7 14'],
            ['P4  front, wood side', 'L', '+19.0', '\u00d82.0\u20132.2 \u00d7 14']]
    table(fig, 0.66, 0.86, [0.0, 0.135, 0.178, 0.228], rows, lh=0.03, fs=7.4, widths=0.32)
    items = [('s', 'x = 0 on the rear ring centre, y from the motor axis (+ toward the wood). Tolerance of the pitch between P1 and P2: \u00b10.3 (the slots have 0.3 mm play per side).'),
             ('gap', ''),
             ('h', 'Marking before a jig exists'),
             ('b', 'Fit the plates on the rings and offer the assembly to the plank, the clip mouths toward the coupler, the slot ends at the plot positions. Mark each plot centre through the closed end of its slot with a \u00d86 punch or a pencil tip: \u00b10.3 mm.'),
             ('b', 'Or give me L and I generate a printed drilling jig referenced on the vertical wood face and the plank edge.'),
             ('gap', ''),
             ('h', 'Drilling'),
             ('b', 'Pilot \u00d82.0\u20132.2 for a \u00d83 wood screw, 14 mm deep (the screw is 25 mm, the plot is 12.1 mm long, so \u2248 13 mm enters the wood). Check the plank thickness first.'),
             ('b', 'Screw the plots with the flat top fully against the plank. Do not over-tighten: the plot is printed.')]
    textblock(fig, 0.66, 0.67, wrap(items, 56), fs=7.4, lh=0.0235)
    legend(fig, [(ORANGE, DK, 'clip plate'), ('white', BLUE, 'ring footprint (dashed)'), (PLOT, DK, 'plot'), ('#f3ead8', WOOD, 'plank')], x=0.03, y=0.05)
    footer(fig, 11, TOTAL)
    return fig




# ============================================================================ 11  assembly sequence (pictures)
def page_steps():
    from PIL import Image
    fig = plt.figure(figsize=A4)
    page_title(fig, 'Assembly sequence', 'IKEA-style pictures generated from the CAD (docs/img). Plate = orange, ring support = blue, plot = grey, TPU cushion = green, wood = plank.')
    cells = [('step_1_inserts.png', '1  Press 2 short M3 heat-set inserts into each pad (iron \u2248 230 \u00b0C), flush with the pad face.'),
             ('step_2_plate_on_ring.png', '2  Screw the plate on the pad: 2 \u00d7 M3 \u00d7 5, mouths toward the coupler (+x). \u2264 0.3 N\u00b7m (10 mm\u00b2 under the head).'),
             ('step_3_cushions_on_plots.png', '3  Slide 2 cushions on every plot: one down to the head, one up to the flat top. Friction fit.'),
             ('step_4_plots_in_plank.png', '4  Pilot \u00d82.0\u20132.2 \u00d7 14, then screw the 4 plots (\u00d83 \u00d7 25) with the flat top fully against the plank.'),
             ('step_5_slide_in.png', '5  Slide the motor (rings and plates on) toward the coupler: the four clips click, the slot ends stop it.')]
    pos = [(0.02, 0.50), (0.345, 0.50), (0.67, 0.50), (0.02, 0.10), (0.345, 0.10)]
    for (fn, cap), (x, y) in zip(cells, pos):
        ax = fig.add_axes([x, y, 0.315, 0.355]); ax.axis('off')
        ax.imshow(Image.open(os.path.join(B.OUT, 'docs', 'img', fn)))
        fig.text(x + 0.005, y - 0.012, '\n'.join(textwrap.wrap(cap, 62)), fontsize=7.2, color=DK, va='top', linespacing=1.3)
    items = [('h', 'Check'),
             ('s', 'Turn the axis by hand: no tight spot. The plates must be free on the plots (0.3 mm above and below, no preload). Axis offset \u2264 0.3 mm, tilt \u2264 1\u00b0 at the coupling.'),
             ('gap', ''),
             ('h', 'Remove'),
             ('s', 'Pull the motor back along its axis: the four clips release one after the other.'),
             ('gap', ''),
             ('h', 'Coupling'),
             ('s', 'Order of the jaw coupling (hubs, spider): pages 14 to 16.')]
    textblock(fig, 0.68, 0.40, wrap(items, 52), fs=7.6, lh=0.0245)
    footer(fig, 12, TOTAL)
    return fig


# ============================================================================ 12  print settings and bill of materials
def page_bom():
    fig = plt.figure(figsize=A4)
    page_title(fig, 'Print settings and bill of materials', 'Bambu Lab A1 mini, 0.4 mm nozzle, Bambu Studio. Dry the filaments before printing. Files in stl/ (step/ for CAD).')
    fig.text(0.03, 0.865, 'Printed parts', fontsize=9.5, fontweight='bold', color=BLUE)
    rows = [['ID', 'Part', 'Qty', 'Material', 'Orientation', 'Walls / infill', 'File'],
            ['A', 'Clip plate', '2', 'PETG Matte', 'flat, bottom face on the bed', '5 walls, 25 %', 'clip_plates_x2.stl'],
            ['B', 'Plot', '4', 'PETG Matte', 'head on the bed', '4 walls (solid)', 'plots_x4.stl'],
            ['C', 'TPU cushion, wheel (silent block)', '8', 'TPU 85A', 'flat', '2 walls, 3 top/bottom', 'cushions_x8.stl'],
            ['D', 'Ring support, motor_mount (\u00d748.5 \u00d7 15)', '1', 'PETG', 'ring axis vertical', '4 walls, 25 %', 'ring_support_motor_mount_print.stl'],
            ['E', 'Ring support, front mount (\u00d748 \u00d7 12)', '1', 'PETG', 'ring axis vertical', '4 walls, 25 %', 'ring_support_front_mount_print.stl'],
            ['F', 'Coupling hub A (drill side)', '1', 'PETG', 'base on the bed', '4 perimeters', 'hub_a_drill_side.stl'],
            ['G', 'Coupling hub B (flange side)', '1', 'PETG', 'base on the bed', '4 perimeters', 'hub_b_flange_side.stl'],
            ['H', 'Spider (TPU, coupling)', '1 + 1', 'TPU 85A', 'flat, lobes upright', '100 %, 4 walls', 'spider_tpu.stl'],
            ['\u2013', 'optional: plate mirrored, solid cushion', '\u2013', 'as A / C', 'as A / C', 'as A / C', 'clip_plate_mirrored.stl, cushion_solid.stl']]
    table(fig, 0.03, 0.84, [0.0, 0.028, 0.255, 0.292, 0.36, 0.505, 0.62], rows, lh=0.026, fs=7.1, widths=0.94)
    fig.text(0.03, 0.563, 'Hardware', fontsize=9.5, fontweight='bold', color=BLUE)
    rows = [['ID', 'Item', 'Qty', 'Used for'],
            ['1', 'Wood screw \u00d83 \u00d7 25, countersunk head \u00d86', '4', f'plots into the plank, pilot \u00d82.0\u20132.2 \u00d7 14, \u2248 {25 - B.SH_LEN - B.HEAD_H:.0f} mm in the wood'],
            ['2', 'M3 heat-set insert, short (L \u2264 3.0, pocket \u00d84.0 \u00d7 3.2)', '4', 'ring supports, 2 per pad'],
            ['3', 'M3 \u00d7 5 cap screw, head \u00d85 \u00d7 3', '4', 'plates on the pads (counterbore \u00d85.8 \u00d7 3.2)'],
            ['4', 'M3 \u00d7 8 cap screw, head \u00d85 \u00d7 3', '4', 'hub A onto drillAdapterV3 (your part)'],
            ['5', 'M3 heat-set insert (pocket \u00d84.0 \u00d7 4.2)', '4', 'hub B'],
            ['6', 'M3 \u00d7 6 cap screw, head \u00d85 \u00d7 3', '4', 'flange coupler onto hub B'],
            ['7', 'Rigid flange coupler, bore 6.35, flange \u00d822, bolt circle \u00d816', '1', 'seller\u2019s part (model in step/flange_coupler_6.35mm.step)']]
    table(fig, 0.03, 0.538, [0.0, 0.028, 0.37, 0.405], rows, lh=0.026, fs=7.1, widths=0.94)
    fig.text(0.03, 0.31, 'Slicer settings', fontsize=9.5, fontweight='bold', color=BLUE)
    rows = [['Setting', 'PETG Matte (plates, plots, rings, hubs)', 'TPU 85A (cushions, spider)'],
            ['Feeding', 'AMS lite or external spool', 'external spool only, spool raised'],
            ['Drying', 'yes (PETG Matte absorbs moisture)', '70 \u00b0C for 8 h, then dry storage'],
            ['Nozzle / bed', '255 \u00b0C (230\u2013260) / 70 \u00b0C (65\u201375)', '225 \u00b0C (215 if it strings) / 35 \u00b0C, no glue'],
            ['Layer / width / speed', '0.2 / 0.42 mm / standard, fan 30\u201350 %', '0.2 / 0.42 mm / 20 mm/s (15 first layer), fan 100 %'],
            ['Support', 'none (hubs: only under the \u00d822.2 \u00d7 1 recess)', 'none; retraction 0.8 mm at 25 mm/s; 2 mm\u00b3/s max'],
            ['First print', 'one clip plate: it must click like sample 4', 'one cushion on a plot: firm friction fit']]
    table(fig, 0.03, 0.285, [0.0, 0.13, 0.50], rows, lh=0.026, fs=7.0, widths=0.94)
    footer(fig, 13, TOTAL)
    return fig


# ============================================================================ 17  checks, validation plan, open items
def page_checks():
    zb = B.Z_PLATE_BOT
    fig = plt.figure(figsize=A4)
    page_title(fig, 'Checks, validation plan and open items', 'What was verified in CAD, what has to be measured on the printed parts, and what is still unknown.')
    left = [('h', 'Verified in CAD (not printed yet)'),
            ('b', 'Every STL is watertight; the check assembly (2 supports, 4 plots, 8 cushions, plank, wood) has no interference other than the designed 0.1 mm friction fit of the cushions.'),
            ('b', f'Clearances: plate to cushions {B.GAP} (above, below), plate to plots 0.3 (around), head B to ring {B.PLANK_Z - B.SH_LEN - B.HEAD_H - ring_z(14.0, 27.0):.1f}, plate end to wood 0.4, ring to wood 1.0.'),
            ('b', 'The clip is the sample 4 geometry of the proof of concept, generated from the same constants. The coupling files are those of the previous repository (same volumes).'),
            ('gap', ''),
            ('h', 'Validation plan (in this order)'),
            ('n', 'T1  Calipers: printed plot \u00d86.0 \u00b1 0.05, slot 6.6, mouth 5.4, cushion ID 5.8 on the shoulder.'),
            ('n', 'T2  On the desk: measure H, the shaft axis to the plank underside (36.0 assumed) and set the cushion thicknesses (page 7).'),
            ('n', 'T3  Bench: one support on two plots at pitch 56; 10 slide-ins; click, unlatching force with a scale along the axis.'),
            ('n', 'T4  On the desk: turn the axis by hand; axis offset \u2264 0.3 mm, tilt \u2264 1\u00b0; run at low PWM and watch the offset under the couple (page 8).'),
            ('n', 'T5  Cycles and creep: 50 insert/remove, then 48 h latched; look for white marks at the slit ends; re-measure.'),
            ('n', 'T6  Vibration: ADXL345 on the plank and on the ring, with and without the cushions; compare with page 8 and keep the softest cushion that holds.')]
    textblock(fig, 0.03, 0.86, wrap(left, 72), fs=7.5, lh=0.0238)
    right = [('h', 'Open items'),
             ('b', 'Axial spacing L between the two supports: unknown (placeholder 80). Needed for the plank layout and a drilling jig.'),
             ('b', 'Mouth direction: +x assumed toward the coupler; mirrored plate provided.'),
             ('b', 'Rings: taken from your SLDPRT, all original vertices within 0.002 mm. Open: the direction of the rear ring along x is as in your file (closed rim at x = 0). The key of the front ring is at the bottom right, as in your file.'),
             ('b', 'Plank \u2265 15 mm thick for the \u00d83 \u00d7 25 screws, otherwise \u00d83 \u00d7 20. H (36.0) sets the cushion thicknesses.'),
             ('b', 'Extraction force \u2248 4 \u00d7 53 N \u2248 210 N by calculation: to be measured; sample 3 is the fallback.'),
             ('b', f'Axial float: with clip sample 4 a plot can move about 3.7 mm between the slot end and the lugs. The coupling jaws (7 mm) accept it, but a motor that creeps back loses engagement; the spider\u2019s friction does not hold it. If it matters, move the lug 1.5 mm toward the slot end (a new clip to validate).'),
             ('b', f'Plate to pad joint: the couple (1.65 N\u00b7m per support) puts \u2248 80 N of tension on one M3 insert (estimate), with only {zb - B.INSERT_DEPTH - ring_z(6.0):.1f} mm of ring wall under a pocket.'),
             ('b', 'Cushion stiffness, damping and the 0.3 mm plays are assumptions: page 8 is an estimate, the ADXL345 decides.'),
             ('gap', ''),
             ('h', 'Assumptions'),
             ('s', 'Plank underside 36 and wood face 27 from the SolidWorks sketches; plank 15\u201316 mm thick; M3 insert pocket \u00d84.0 \u00d7 3.2 (short inserts); screws \u00d83 \u00d7 25 DIN 7997 type; torque 3.3 N\u00b7m shared by two supports.')]
    textblock(fig, 0.52, 0.86, wrap(right, 66), fs=7.4, lh=0.0235)
    footer(fig, 18, TOTAL)
    return fig


def build(out):
    import coupling_pages as cp
    seq = [page_summary, page_overview, page_stack, page_plate, page_clip, page_plot, page_cushion, page_paths, page_ring_rear, page_ring_front, page_layout, page_steps, page_bom,
           cp.page_coupler, cp.page_clip, cp.page_vis, page_integration, page_checks]
    assert len(seq) == TOTAL
    with PdfPages(out) as pdf:
        for n, f in enumerate(seq, 1):
            fig = f()
            if f.__module__ == 'coupling_pages':
                footer(fig, n, TOTAL)
            pdf.savefig(fig)
            plt.close(fig)
        d = pdf.infodict()
        d['Title'] = 'Slide-in motor supports with silent blocks - assembly report'
        d['Author'] = 'Claude'
    print('ok', out)


if __name__ == '__main__':
    build(os.path.join(B.OUT, 'docs', 'slide_mount_assembly_report.pdf'))
