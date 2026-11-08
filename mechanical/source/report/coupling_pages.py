"""Pages of the jaw coupling (shaft path of the isolation), taken from the previous modification plans and adapted to this report."""
import sys, os, tempfile
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'coupling')))
from report_common import *
from matplotlib.patches import Wedge
import numpy as np

TMP_STL = os.path.join(tempfile.gettempdir(), '_coupling_slice.stl')
DRILL_HUB_STL = os.path.join(B.OUT, 'solidworks', 'DrillAdapter+hub.STL')

def sector_patch(ax, r_in, r_out, a0, a1, fc, ec, lw=0.7, z=3):
    ax.add_patch(Wedge((0, 0), r_out, a0, a1, width=r_out - r_in, fc=fc, ec=ec, lw=lw, zorder=z))
_SLICE = None

def slice_polys():
    global _SLICE
    if _SLICE is not None:
        return _SLICE
    from coupling_parts import coupler_assembly
    import cadquery as cq, trimesh
    asm, A, B, S = coupler_assembly()
    out = {}
    for n, o in (('B', B), ('S', S), ('A', A)):
        cq.exporters.export(o, TMP_STL, tolerance=0.01, angularTolerance=0.05)
        m = trimesh.load(TMP_STL)
        sec = m.section(plane_origin=[0, 0, 0], plane_normal=[0, 1, 0])
        p2, _ = sec.to_planar(to_2D=np.array([[1, 0, 0, 0], [0, 0, 1, 0], [0, 1, 0, 0], [0, 0, 0, 1]]))
        out[n] = p2.polygons_full
    _SLICE = out
    return out

def draw_slice(ax, lw=0.5):
    P = slice_polys()
    cols = {'S': '#bfe8d9', 'B': '#bdbdc4', 'A': '#e3e3e0'}
    ecs = {'S': '#0F6E56', 'B': '#5F5E5A', 'A': '#888780'}
    zs = {'S': 3, 'B': 6, 'A': 6}
    for n in ('S', 'B', 'A'):
        for poly in P[n]:
            x, y = poly.exterior.xy
            ax.fill(x, y, fc=cols[n], ec=ecs[n], lw=lw, zorder=zs[n])
            for h in poly.interiors:
                hx, hy = h.xy
                ax.fill(hx, hy, fc='white', ec=ecs[n], lw=lw, zorder=zs[n] + 0.5 if n == 'S' else zs[n] + 1)

def page_coupler():
    fig = plt.figure(figsize=A4)
    page_title(fig, 'Jaw coupling — hubs A and B, TPU spider', 'Replaces the rigid drill → flange link. Hub A is the jaw end of the drill hub (page 16); hub B is screwed on the Ø22 flange (4 × M3 on Ø16).')
    ax = fig.add_axes([0.03, 0.38, 0.44, 0.52])
    ax.set_aspect('equal')
    ax.axis('off')
    ax.set_xlim(-24, 24)
    ax.set_ylim(-24, 24)
    for c in (0, 60, 120, 180, 240, 300):
        sector_patch(ax, 12.0, 16.5, c - 14, c + 14, '#5DCAA5', '#1D9E75')
    for c in (90, 210, 330):
        sector_patch(ax, 12.5, 16.5, c - 16, c + 16, '#D3D1C7', '#888780', z=4)
    for c in (30, 150, 270):
        sector_patch(ax, 12.5, 16.5, c - 16, c + 16, '#888780', '#5F5E5A', z=4)
    ax.add_patch(Wedge((0, 0), 12.1, 0, 360, width=1.6, fc='#5DCAA5', ec='#1D9E75', lw=0.7, zorder=3))
    ax.add_patch(Circle((0, 0), 4.5, fc='white', ec=DK, lw=0.6, zorder=5))
    dim(ax, (-16.5, -20.5), (16.5, -20.5), 'Ø33')
    ext(ax, (-16.5, -16), (-16.5, -21))
    ext(ax, (16.5, -16), (16.5, -21))
    ax.text(0, -23.2, 'Top view (jaw side of hub A, on the drill hub)', fontsize=8.5, ha='center', color=BLUE, fontweight='bold')
    ax.annotate('jaws A ×3\n32° · Ø25 → Ø33', xy=(0, 14.5), xytext=(-23.5, 19), fontsize=7, color=DK, ha='left', arrowprops=dict(arrowstyle='-', lw=0.5))
    ax.annotate('jaws B ×3\n32° · Ø25 → Ø33', xy=(14.5 * math.cos(math.radians(30)), 14.5 * math.sin(math.radians(30))), xytext=(15, 21), fontsize=7, color=DK, ha='left', arrowprops=dict(arrowstyle='-', lw=0.5))
    ax.annotate('TPU lobes ×6\n28° · Ø24 → Ø33', xy=(14.2, 0), xytext=(17.5, -2), fontsize=7, color=TEAL, ha='left', arrowprops=dict(arrowstyle='-', lw=0.5, color=TEAL))
    ax.annotate('TPU tube Ø21 / Ø24.2\n(wall 1.6)', xy=(-8.2, -8.2), xytext=(-23.5, -14), fontsize=7, color=TEAL, ha='left', arrowprops=dict(arrowstyle='-', lw=0.5, color=TEAL))
    ax.annotate('Ø9 centre hole\n(both hubs)', xy=(-3.2, -3.2), xytext=(-23.5, -3.6), fontsize=7, color=DK, ha='left', arrowprops=dict(arrowstyle='-', lw=0.5))
    ax2 = fig.add_axes([0.5, 0.38, 0.47, 0.52])
    ax2.set_aspect('equal')
    ax2.axis('off')
    ax2.set_xlim(-38, 38)
    ax2.set_ylim(-5, 26)
    draw_slice(ax2)
    ax2.plot([0, 0], [-1, 22], color=DK, lw=0.5, ls='-.', zorder=7)
    ax2.text(0, -3.3, 'Assembled section, plane y = 0 (lobes 180° left, 0° right)', fontsize=8, ha='center', color=BLUE, fontweight='bold')
    dim(ax2, (21, 0), (21, 20.5), '20.5')
    ext(ax2, (16.5, 0), (22, 0))
    ext(ax2, (16.5, 20.5), (22, 20.5))
    dim(ax2, (26, 6), (26, 14.5), '8.5')
    ext(ax2, (16.5, 6), (27, 6))
    ext(ax2, (16.5, 14.5), (27, 14.5))
    ax2.text(-18, 3, 'hub B (PETG)\nbase 6, Ø9 hole', fontsize=7, va='center', ha='right')
    ax2.text(-18, 17.5, 'hub A (PETG)\nbase 6, Ø9 hole\n(drill hub)', fontsize=7, va='center', ha='right')
    ax2.text(-18, 10.5, 'TPU spider\nlobes 8.5', fontsize=7, va='center', ha='right', color=TEAL)
    ax2.annotate('spider tube\n(wall 1.6)', xy=(-11.3, 11.0), xytext=(-31.5, 14.0), fontsize=6.8, color=TEAL, ha='center', va='center', arrowprops=dict(arrowstyle='-', lw=0.5, color=TEAL))
    L = [('Files: DrillAdapter+hub (solidworks/) · hub_b_flange_side · spider_tpu · assembly_coupler', 'h'),
         ('• Hub A (drill side): base Ø33 × 6, Ø22.2 × 1 centering recess, Ø9 centre hole, plain jaws with a 0.6 mm lead-in chamfer on their top flank edges.', 'b'),
         ('   It is no longer screwed: it is printed in one part with the drill adapter V4, the drill hub (page 16).', 'b'),
         ('• Hub B (flange side): base Ø33 × 6, Ø22.2 × 1 recess, 4 pockets Ø3.8 through the base for M3 × 4 × Ø4.2 inserts, Ø9 central hole for the shaft end;', 'b'),
         ('   flange screws M3 × 6, Ø5 × 3 heads on the flange side (0.5 mm clearance to the boss).', 'b'),
         ('• Spider: TPU 85A (95A if it heats up), 100 % infill, a plain prism printed flat with the lobes upright: no overhang, no support, no small feature.', 'b'),
         ('   It stays on the hubs by a light friction fit (next page): lobes 28.8° in the 28° gaps between the 32° jaws. Either face goes on either hub.', 'b'),
         ('• Hub B and drill hub in PETG (or PA-CF), 4 perimeters; hub B base on the bed, supports only under its recess (drill hub: page 16).', 'b'),
         ('   Allowable misalignment ≈ 0.3 mm / 1°; twist ≈ 1.5° (95A) to 4° (85A) at 3.3 N·m (≈ 2.7 MPa on the lobes).', 'b'),
         ("• Order: drill hub onto the drill spindle, B onto the flange, spider onto B's jaws, then slide the motor in: the jaws engage. Keep a spare spider.", 'b')]
    textblock(fig, 0.03, 0.335, L, fs=7.6, lh=0.03)
    return fig

def page_clip():
    fig = plt.figure(figsize=A4)
    page_title(fig, 'Spider fit — a plain prism held by a light squeeze', 'No snap, no peg, no overhang: printed flat, the spider stays on the hubs by friction between its lobes and the jaws. Dimensions in mm.')
    # ---- left : one lobe squeezed between two jaws (tangential section at r = 14.25, squeeze x5 for readability)
    ax = fig.add_axes([0.03, 0.40, 0.45, 0.50])
    ax.set_aspect('equal')
    ax.axis('off')
    gap, lobe, k = 6.96, 7.16, 5.0
    ov = (lobe - gap) / 2 * k
    ax.add_patch(Rectangle((-gap / 2 - 4.5, 0), 4.5, 7.0, fc='#8E8D88', ec='#5F5E5A', lw=1.0, zorder=3))
    ax.add_patch(Rectangle((gap / 2, 0), 4.5, 7.0, fc='#E6E4DC', ec='#888780', lw=1.0, zorder=3))
    ax.add_patch(Rectangle((-gap / 2 - ov, -0.4), gap + 2 * ov, 8.5 + 0.0, fc='#5DCAA5', ec='#1D9E75', lw=1.0, alpha=0.9, zorder=4))
    for sgn in (-1, 1):
        ax.add_patch(Rectangle((sgn * gap / 2 if sgn > 0 else -gap / 2 - ov, 0), ov, 7.0, fc='#F0B3AA', ec=RED, lw=1.0, zorder=5, hatch='////'))
    ax.set_xlim(-8.6, 8.6)
    ax.set_ylim(-2.6, 11.0)
    ax.text(0, 4.0, 'TPU lobe\n28.8°', fontsize=8.5, ha='center', va='center', color='#0F6E56', fontweight='bold', zorder=9)
    ax.text(-gap / 2 - 2.3, 3.5, 'jaw of B', fontsize=7.5, ha='center', color='white', zorder=9)
    ax.text(gap / 2 + 2.3, 3.5, 'jaw of A', fontsize=7.5, ha='center', color=DK, zorder=9)
    ax.annotate('squeeze 0.1 per side\n(drawn ×5)', xy=(gap / 2 + ov / 2, 6.0), xytext=(3.4, 9.6), fontsize=7.5, color=RED, ha='left', va='center', fontweight='bold', arrowprops=dict(arrowstyle='-', lw=0.8, color=RED))
    dim(ax, (-gap / 2, -1.5), (gap / 2, -1.5), '28° gap = 6.96', fs=7.5)
    ax.text(0, -2.4, 'Tangential section at r = 14.25', fontsize=8.2, ha='center', color=BLUE, fontweight='bold')
    ax.text(0, 10.4, 'one lobe between two jaws', fontsize=8.2, ha='center', color=BLUE, fontweight='bold')
    # ---- right : what prints, and how
    ax2 = fig.add_axes([0.54, 0.40, 0.44, 0.50])
    ax2.set_aspect('equal')
    ax2.axis('off')
    ax2.set_xlim(-6, 46)
    ax2.set_ylim(-4, 14)
    ax2.plot([-5, 45], [0, 0], color=DK, lw=2.0, zorder=2)
    ax2.text(-5, -1.0, 'bed', fontsize=7.5, color=DK)
    for x0, w in ((0, 5.0), (7.5, 5.0), (15, 5.0)):
        ax2.add_patch(Rectangle((x0, 0), w, 8.5, fc='#bfe8d9', ec='#0F6E56', lw=1.0, zorder=3))
    ax2.add_patch(Rectangle((-1.5, 0), 24.0, 8.5, fc='none', ec=GREY, lw=0.6, ls=(0, (4, 3)), zorder=2))
    ax2.annotate('', xy=(1.5, 11.2), xytext=(1.5, 8.7), arrowprops=dict(arrowstyle='<->', lw=0.9, color=DK))
    ax2.text(2.2, 10.0, '8.5', fontsize=7.5, color=DK, va='center')
    ax2.text(9.0, 11.6, 'walls 100 % vertical:', fontsize=8, color='#0F6E56', fontweight='bold', ha='left')
    ax2.text(9.0, 10.3, 'overhang 0°, no bridge, no support', fontsize=8, color='#0F6E56', ha='left')
    ax2.text(10.0, -2.6, 'side view of the spider as printed', fontsize=8.2, ha='center', color=BLUE, fontweight='bold')
    rows = [('spider', '0°'), ('TPU cushion, wheel', '0°'), ('TPU cushion, solid', '0°')]
    ax2.text(36.5, 8.2, 'max overhang\n(measured on the STL)', fontsize=7, color=DK, ha='center', va='center')
    for i, (n, v) in enumerate(rows):
        ax2.text(35.2, 5.6 - i * 1.7, n, fontsize=6.8, ha='right', va='center')
        ax2.text(36.4, 5.6 - i * 1.7, v, fontsize=8.5, ha='left', va='center', color='#0F6E56', fontweight='bold')
    L = [('How it works', 'h'),
         ('• The 6 lobes are 28.8° wide and the gaps between the jaws 28°: each lobe is squeezed by 0.1 mm per side. The spider is also captive between the two hubs.', 'b'),
         ('• Friction (estimate, to be checked): about 5 N per flank, so ≈ 30 N holding on one hub and ≈ 30 N to push the other hub on. Nothing pulls on it in service.', 'b'),
         ('• Nothing is printed that needs a support: no bead, no peg, no socket, no bridge. All the TPU parts have 0° of overhang.', 'b'),
         ('• Tuning: the squeeze is the constant PRELOAD in source/coupling/coupling_parts.py (mm per side). Print 3 spiders at 0.0 / 0.1 / 0.2 and keep the one that grips', 'b'),
         ('   firmly but can still be pressed on by hand. TPU printed 0.1 mm undersize gives no grip at all: raise PRELOAD.', 'b'),
         ('• If it still slips out during assembly: a strip of double-sided tape on the end faces, or a drop of flexible adhesive on one hub only.', 'b'),
         ('• Lead-in: 0.6 mm 45° chamfer on the two top flank edges of every jaw lets the lobes slide between the jaws.', 'b')]
    textblock(fig, 0.03, 0.335, L, fs=7.6, lh=0.033)
    return fig


# drill adapter V4, measured on solidworks/DrillAdapter+hub.STL (the part is made in SolidWorks, not in the CAD scripts)
BODY_D, FLANGE_D = 19.6, 22.0          # threaded body, flange (z 20 to 25, in the recess of hub A)
THREAD_D, THREAD_P, THREAD_L = (12.0, 13.6), 1.5, 15.0      # threaded bore for the drill spindle: minor / major dia, pitch, depth
HOLE_D, CSK_D = 5.6, 9.0               # central hole, countersunk 90 deg at its top


def _slice_stl(path, angle):
    """section of an STL by the plane through its vertical axis at `angle` (deg).
    Returns the polygons in (distance along the angle, z from the bottom) and the mesh."""
    import trimesh
    m = trimesh.load(path)
    (x0, y0, z0), (x1, y1, _) = m.bounds
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    a = math.radians(angle)
    u, n = (math.cos(a), math.sin(a)), (-math.sin(a), math.cos(a))
    sec = m.section(plane_origin=[cx, cy, z0], plane_normal=[n[0], n[1], 0])
    to_2d = np.array([[u[0], u[1], 0, -(u[0] * cx + u[1] * cy)], [0, 0, 1, -z0], [n[0], n[1], 0, -(n[0] * cx + n[1] * cy)], [0, 0, 0, 1]])
    p2, _ = sec.to_planar(to_2D=to_2d)
    return p2.polygons_full, m


def _fill(ax, polys, fc='#e3e3e0', ec='#888780', lw=0.8, z=3):
    for poly in polys:
        ax.fill(*poly.exterior.xy, fc=fc, ec=ec, lw=lw, zorder=z)
        for h in poly.interiors:
            ax.fill(*h.xy, fc='white', ec=ec, lw=lw, zorder=z + 1)


def page_drill_hub():
    from coupling_parts import BASE, CLAW_H, D_OUT
    fig = plt.figure(figsize=A4)
    page_title(fig, 'Drill hub — drill adapter V4 and hub A in one part',
               'Section through the axis of solidworks/DrillAdapter+hub.STL, made in SolidWorks from drillAdapterV4.SLDPRT and hub A. Dimensions in mm, measured on the STL.')
    polys, m = _slice_stl(DRILL_HUB_STL, 346.0)                 # through one jaw (right) and the gap opposite to it (left)
    H = m.extents[2]
    hub_body = max(m.split(only_watertight=False), key=lambda b: b.extents[0])
    zh = hub_body.bounds[0][2] - m.bounds[0][2]                # bottom face of hub A
    zb = zh + BASE                                             # top of the base = foot of the jaws
    rb, rf, ro = BODY_D / 2, FLANGE_D / 2, D_OUT / 2

    # ---- left: the section, dimensioned
    ax = fig.add_axes([0.03, 0.29, 0.50, 0.61])
    ax.set_aspect('equal'); ax.axis('off')
    ax.set_xlim(-36, 32); ax.set_ylim(-7, H + 6)
    _fill(ax, polys)
    center_line(ax, (0, -3), (0, H + 2))
    ax.text(0, THREAD_L / 2, 'drill\nadapter V4', fontsize=7, ha='center', va='center', color='#5F5E5A')
    ax.text(0, (zb + H) / 2, 'hub A', fontsize=7, ha='center', va='center', color='#5F5E5A')
    dim(ax, (-rb, -3.0), (rb, -3.0), f'Ø{BODY_D}, 6 flutes', fs=7.5)
    ext(ax, (-rb, 0), (-rb, -4)); ext(ax, (rb, 0), (rb, -4))
    dim(ax, (-ro, H + 3), (ro, H + 3), f'Ø{D_OUT:.0f}', fs=7.5)
    ext(ax, (-ro, zb), (-ro, H + 4)); ext(ax, (ro, H), (ro, H + 4))
    dim(ax, (14.0, 0), (14.0, zh), f'{zh:.0f}', fs=7.5)
    ext(ax, (rb, 0), (28, 0)); ext(ax, (ro, zh), (22, zh))
    dim(ax, (21.0, zh), (21.0, zb), f'{BASE:.0f}', fs=7.5)
    dim(ax, (21.0, zb), (21.0, H), f'{CLAW_H:.0f}', fs=7.5)
    ext(ax, (ro, zb), (22, zb)); ext(ax, (ro, H), (28, H))
    dim(ax, (27.0, 0), (27.0, H), f'{H:.0f}', fs=7.5)
    for xy, to, txt in (((-35, THREAD_L / 2), (-THREAD_D[1] / 2, THREAD_L / 2), f'threaded bore Ø{THREAD_D[0]} / {THREAD_D[1]}\npitch {THREAD_P}, {THREAD_L:.0f} deep\n(drill spindle)'),
                        ((-35, 16.5), (-rf, zh - 1.5), f'flange Ø{FLANGE_D:.0f}, in the\nØ22.2 × 1 recess of hub A'),
                        ((-35, 21.5), (-HOLE_D / 2, 21.5), f'Ø{HOLE_D} hole, countersunk\n90° to Ø{CSK_D:.0f}'),
                        ((-35, zh + 3.5), (-CSK_D / 2, zh + 3.5), f'Ø{CSK_D:.0f} through the base')):
        note(ax, xy, txt, fs=7)
        leader(ax, (-19.5, xy[1]), to, color=DK, lw=0.5)
    ax.text(-2, -5.8, 'Section through the axis and one jaw', fontsize=8.5, ha='center', color=BLUE, fontweight='bold')

    # ---- right: how it prints
    ax2 = fig.add_axes([0.56, 0.29, 0.41, 0.61])
    ax2.set_aspect('equal'); ax2.axis('off')
    ax2.set_xlim(-24, 42); ax2.set_ylim(-7, H + 6)
    _fill(ax2, polys, lw=0.6)
    ax2.plot([-22, 22], [0, 0], color=DK, lw=2.0, zorder=5)
    ax2.text(-22, -1.8, 'bed', fontsize=7.5, color=DK)
    for s in (-1, 1):
        R(ax2, s * (rf + 0.1), s * ro, 0, zh, 'white', ec=GREY, lw=0.6, z=2, hatch='////')
    note(ax2, (18.5, zh / 2), f'support, only under\nthe Ø{D_OUT:.0f} base\n({ro - rf:.1f} mm overhang)', fs=7, color=DK)
    note(ax2, (18.5, (zb + H) / 2), 'jaws up:\nno support', fs=7, color=DK)
    ax2.text(0, THREAD_L / 2, 'thread\naxis\nvertical', fontsize=7, ha='center', va='center', color='#5F5E5A')
    ax2.text(0, -5.8, 'Print: threaded end on the bed, jaws up', fontsize=8.5, ha='center', color=BLUE, fontweight='bold')

    L = [('Drill hub: one printed part, no screws', 'h'),
         ('• Drill adapter V4 (threaded bore for the drill spindle) and hub A, joined in SolidWorks: solidworks/DrillAdapter+hub.SLDASM (drillAdapterV4, hub_a_drill_side).', 'b'),
         ('• Compared with the previous version: the 4 M3 × 8 screws and the 4 inserts between them are gone, and the jaws are centred on the bore by construction.', 'b'),
         ('• Same jaws as before (3 × 32°, Ø25 → Ø33, 7 high, 0.6 mm lead-in): spider and hub B unchanged. Central Ø5.6 hole, reached through the Ø9 hole of the base.', 'b'),
         ('• PETG, 4 perimeters, threaded end on the bed, support only under the Ø33 base. The STL holds two bodies (adapter, hub) that touch on the Ø22 face:', 'b'),
         ('   keep them as one object in the slicer. First print: screw it onto the spindle by hand; it must go on without play and without forcing.', 'b'),
         ('Hub B', 'h'),
         ('• Flange screws M3 × 6 (2 mm flange + 4 mm insert); their Ø5 × 3 heads sit on its outer face, 0.5 mm clear of the Ø10 boss (head edge at r = 5.5).', 'b')]
    textblock(fig, 0.03, 0.265, L, fs=7.6, lh=0.029)
    return fig
