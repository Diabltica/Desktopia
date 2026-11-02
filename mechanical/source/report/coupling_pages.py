"""Pages of the jaw coupling (shaft path of the isolation), taken from the previous modification plans and adapted to this report."""
import sys, os
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'coupling')))
from report_common import *
from matplotlib.patches import Wedge
import numpy as np

def sector_patch(ax, r_in, r_out, a0, a1, fc, ec, lw=0.7, z=3):
    ax.add_patch(Wedge((0, 0), r_out, a0, a1, width=r_out - r_in, fc=fc, ec=ec, lw=lw, zorder=z))
_SLICE = None

def slice_polys():
    global _SLICE
    if _SLICE is not None:
        return _SLICE
    from coupling_parts import coupler_assembly
    import cadquery as cq, trimesh
    asm, A, B, S = coupler_assembly(with_hardware=False)
    out = {}
    for n, o in (('B', B), ('S', S), ('A', A)):
        cq.exporters.export(o, '/tmp/_s.stl', tolerance=0.01, angularTolerance=0.05)
        m = trimesh.load('/tmp/_s.stl')
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
    page_title(fig, 'Jaw coupling — hubs A and B, TPU spider', 'Replaces the rigid drill → flange link. drillAdapterV3 and the Ø22 flange stay unchanged (4 × M3 on Ø16).')
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
    for a in (0, 90, 180, 270):
        ax.add_patch(Circle((8 * math.cos(math.radians(a)), 8 * math.sin(math.radians(a))), 1.7, fc='white', ec=DK, lw=0.6, zorder=5))
    ax.add_patch(Circle((0, 0), 8, fc='none', ec=GREY, lw=0.5, ls=(0, (4, 3)), zorder=6))
    dim(ax, (-16.5, -20.5), (16.5, -20.5), 'Ø33')
    ext(ax, (-16.5, -16), (-16.5, -21))
    ext(ax, (16.5, -16), (16.5, -21))
    ax.text(0, -23.2, 'Top view (jaw side of hub A)', fontsize=8.5, ha='center', color=BLUE, fontweight='bold')
    ax.annotate('jaws A ×3\n32° · Ø25 → Ø33', xy=(0, 14.5), xytext=(-23.5, 19), fontsize=7, color=DK, ha='left', arrowprops=dict(arrowstyle='-', lw=0.5))
    ax.annotate('jaws B ×3\n32° · Ø25 → Ø33', xy=(14.5 * math.cos(math.radians(30)), 14.5 * math.sin(math.radians(30))), xytext=(15, 21), fontsize=7, color=DK, ha='left', arrowprops=dict(arrowstyle='-', lw=0.5))
    ax.annotate('TPU lobes ×6\n28° · Ø24 → Ø33', xy=(14.2, 0), xytext=(17.5, -2), fontsize=7, color=TEAL, ha='left', arrowprops=dict(arrowstyle='-', lw=0.5, color=TEAL))
    ax.annotate('TPU tube Ø21 / Ø24.2\n(wall 1.6)', xy=(-8.2, -8.2), xytext=(-23.5, -14), fontsize=7, color=TEAL, ha='left', arrowprops=dict(arrowstyle='-', lw=0.5, color=TEAL))
    ax.annotate('4 holes on Ø16\nA: Ø3.4 through\nB: Ø4.0 × 4.2 pockets', xy=(0, -8), xytext=(-23.5, -3.6), fontsize=7, color=DK, ha='left', arrowprops=dict(arrowstyle='-', lw=0.5))
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
    ax2.text(-18, 17.5, 'hub A (PETG)\nbase 6', fontsize=7, va='center', ha='right')
    ax2.text(-18, 10.5, 'TPU spider\nlobes 8.5', fontsize=7, va='center', ha='right', color=TEAL)
    ax2.annotate('spider tube\n(wall 1.6)', xy=(-11.3, 11.0), xytext=(-31.5, 14.0), fontsize=6.8, color=TEAL, ha='center', va='center', arrowprops=dict(arrowstyle='-', lw=0.5, color=TEAL))
    L = [('STEP files: hub_a_drill_side · hub_b_flange_side · spider_tpu · assembly_coupler', 'h'),
         ('• Hub A (drill side): base Ø33 × 6, Ø22.2 × 1 centering recess, 4 Ø3.4 holes on Ø16 with Ø5.8 × 3.2 counterbores for Ø5 × 3 screw heads;', 'b'),
         ('   M3 × 8 screws from the jaw side into the drillAdapterV3 inserts (page 16). Plain jaws with a 0.6 mm lead-in chamfer on their top flank edges.', 'b'),
         ('• Hub B (flange side): base Ø33 × 6, Ø22.2 × 1 recess, 4 M3 insert pockets Ø4.0 × 4.2 (as on drillAdapterV3), Ø9 central hole for the shaft end;', 'b'),
         ('   flange screws M3 × 6, Ø5 × 3 heads on the flange side (0.5 mm clearance to the boss).', 'b'),
         ('• Spider: TPU 85A (95A if it heats up), 100 % infill, a plain prism printed flat with the lobes upright: no overhang, no support, no small feature.', 'b'),
         ('   It stays on the hubs by a light friction fit (next page): lobes 28.8° in the 28° gaps between the 32° jaws. Either face goes on either hub.', 'b'),
         ('• Hubs in PETG (or PA-CF), base on the bed, 4 perimeters; supports only under the Ø22.2 × 1 recess. Allowable misalignment ≈ 0.3 mm / 1°;', 'b'),
         ('   twist ≈ 1.5° (95A) to 4° (85A) at 3.3 N·m (≈ 2.7 MPa on the lobes).', 'b'),
         ("• Order: screw B onto the flange, press the spider onto B's jaws, push A onto the spider, screw A onto drillAdapterV3. Keep a spare spider.", 'b')]
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


def _slice_hub(kind):
    from coupling_parts import hub
    import cadquery as cq, trimesh
    cq.exporters.export(hub(kind), '/tmp/_h.stl', tolerance=0.01, angularTolerance=0.05)
    m = trimesh.load('/tmp/_h.stl')
    sec = m.section(plane_origin=[0, 0, 0], plane_normal=[0, 1, 0])
    p2, _ = sec.to_planar(to_2D=np.array([[1, 0, 0, 0], [0, 0, 1, 0], [0, 1, 0, 0], [0, 0, 0, 1]]))
    return p2.polygons_full

def page_vis():
    fig = plt.figure(figsize=A4)
    page_title(fig, 'Hub screws — recesses for Ø5 × 3 heads', 'Section y = 0 of hub A (hub frame: recess at the bottom, jaws on top), through one screw. Dimensions in mm.')
    ax = fig.add_axes([0.03, 0.3, 0.5, 0.6])
    ax.set_aspect('equal')
    ax.axis('off')
    ax.set_xlim(2.6, 14.0)
    ax.set_ylim(-9.5, 8.2)
    for poly in _slice_hub('A'):
        x, y = poly.exterior.xy
        ax.fill(x, y, fc='#e3e3e0', ec='#888780', lw=0.9, zorder=3)
        for h in poly.interiors:
            ax.fill(*h.xy, fc='white', ec='#888780', lw=0.9, zorder=4)
    ax.add_patch(Rectangle((2.6, -9.0), 11.0 - 2.6, 10.0, fc='#dfe7ee', ec='#5b6b7a', lw=0.9, zorder=2))
    ax.add_patch(Rectangle((6.0, -9.0 + 0.0), 4.0, 10.0, fc='white', ec='#5b6b7a', lw=0.7, ls='--', zorder=2.5))
    ax.add_patch(Rectangle((6.0, -4.7), 4.0, 5.7, fc='#f0e4c8', ec='#9a7b4f', lw=0.7, zorder=2.6))
    ax.add_patch(Rectangle((5.5, 2.8), 5.0, 3.0, fc='#9c9c9c', ec='#555555', lw=0.8, zorder=6))
    ax.add_patch(Rectangle((6.5, -5.2), 3.0, 8.0, fc='#9c9c9c', ec='#555555', lw=0.8, zorder=6))
    ax.plot([8, 8], [-9, 7.5], color=DK, lw=0.5, ls='-.', zorder=9)
    dim(ax, (5.1, 7.0), (10.9, 7.0), 'Ø5.8', fs=7.5, color=BLUE)
    ext(ax, (5.1, 6), (5.1, 7.4))
    ext(ax, (10.9, 6), (10.9, 7.4))
    dim(ax, (5.5, 5.1), (10.5, 5.1), 'head Ø5', fs=7, color=DK)
    dim(ax, (12.2, 2.8), (12.2, 6.0), None, color=BLUE)
    ax.text(12.4, 4.4, '3.2', fontsize=7.5, color=BLUE, va='center')
    ext(ax, (10.9, 6), (12.6, 6))
    ext(ax, (10.9, 2.8), (12.6, 2.8))
    dim(ax, (12.9, 1.0), (12.9, 2.8), None, color=RED)
    ax.text(13.05, 1.9, '1.8', fontsize=7.5, color=RED, va='center')
    ext(ax, (11.1, 1), (13.3, 1))
    dim(ax, (3.4, -5.2), (3.4, 2.8), None, color=TEAL)
    ax.text(3.2, -1.2, 'M3 × 8', fontsize=7.5, color=TEAL, ha='right', va='center')
    ext(ax, (6.5, -5.2), (3.8, -5.2))
    ext(ax, (5.5, 2.8), (3.8, 2.8))
    ax.text(4.4, -3.2, '6.2 in\nthe insert', fontsize=6.8, color=TEAL, ha='left', va='center')
    ax.text(8, -7.6, 'drillAdapterV3 (excerpt) · insert', fontsize=7, ha='center', color='#5b6b7a')
    ax.annotate('head 0.2 below the face', xy=(10.5, 5.8), xytext=(11.2, 7.4), fontsize=6.8, color=DK, ha='left', arrowprops=dict(arrowstyle='-', lw=0.5))
    ax.text(8, -9.3, 'Ø3.4 (hole) · Ø5.8 × 3.2 (counterbore)', fontsize=7.5, ha='center', color=BLUE, fontweight='bold')
    ax2 = fig.add_axes([0.56, 0.4, 0.4, 0.5])
    ax2.set_aspect('equal')
    ax2.axis('off')
    ax2.set_xlim(-19, 19)
    ax2.set_ylim(-19, 19)
    for c in (90, 210, 330):
        sector_patch(ax2, 12.5, 16.5, c - 16, c + 16, '#D3D1C7', '#888780')
    ax2.add_patch(Circle((0, 0), 16.5, fc='#f2f2ef', ec='#888780', lw=0.8, zorder=1))
    for c in (90, 210, 330):
        sector_patch(ax2, 12.5, 16.5, c - 16, c + 16, '#D3D1C7', '#888780', z=3)
    for a in (0, 90, 180, 270):
        cx, cy = (8 * math.cos(math.radians(a)), 8 * math.sin(math.radians(a)))
        ax2.add_patch(Circle((cx, cy), 2.9, fc='white', ec=BLUE, lw=1.0, ls='--', zorder=4))
        ax2.add_patch(Circle((cx, cy), 1.7, fc='white', ec=DK, lw=0.7, zorder=5))
    ax2.annotate('4 × Ø5.8 (head) / Ø3.4', xy=(8 * math.cos(math.radians(45)), 8 * math.sin(math.radians(45))), xytext=(-18.5, 17.2), fontsize=7, color=BLUE, ha='left', arrowprops=dict(arrowstyle='-', lw=0.5, color=BLUE))
    ax2.text(0, -18.3, 'Hub A, jaw side', fontsize=7.8, ha='center', color=BLUE, fontweight='bold')
    L = [('Screws, tightening, hub B', 'h'),
         ('• Ø5 × 3 heads: recessed 0.2 mm below the face of A, so nothing of the spider touches them. The counterbore edge (r = 10.9) undercuts', 'b'),
         ('   0.4 mm of the 1.6 mm wide tube end face at 4 places: harmless. No interference in the check assembly.', 'b'),
         ('• M3 × 8 screws recommended: 1.8 mm of base + 6.2 mm in the insert. M3 × 6 minimum (4.2 mm). Moderate tightening (≤ 0.3 N·m):', 'b'),
         ('   the bearing area under the head is only 10 mm² on 1.8 mm of PETG.', 'b'),
         ('• Hub B: nothing to add inside. The flange screw heads are on its outer face; with Ø5 the clearance to the Ø10 boss is 0.5 mm', 'b'),
         ('   (head edge at r = 5.5 mm). M3 × 6 screws (2 mm flange + 4 mm insert).', 'b'),
         ('• assembly_coupler.step contains the 4 screws and a simplified extract of drillAdapterV3 (Ø22 × 10, Ø4 holes, Ø9 recess), for checking only.', 'b')]
    textblock(fig, 0.03, 0.285, L, fs=7.6, lh=0.029)
    return fig

