"""Figures of the slide-mount package: assembly sections and the dimensioned drawing of the clip (sample 4).
    python make_figures.py          (after build_slide_mount.py)
"""
import os
import tempfile
import numpy as np
import trimesh
import cadquery as cq
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle
import build_slide_mount as B

DOCS = B.DOCS
COL = {"plank": "#f1dfbd", "vertical_wood": "#f1dfbd", "ring": "#c6d3ea", "clip_plate": "#f6c27a", "plot": "#9aa0ad", "cushion": "#bfe8d9"}


def assembly_meshes():
    asm, _, _ = B.assembly()
    out = []
    for ch in asm.children:
        shp = ch.obj.val() if hasattr(ch.obj, "val") else ch.obj
        if ch.loc is not None:
            shp = shp.moved(ch.loc)
        tmp = os.path.join(tempfile.gettempdir(), "_part.stl")
        cq.exporters.export(cq.Workplane("XY").add(shp), tmp, tolerance=0.01, angularTolerance=0.1)
        key = [k for k in COL if ch.name.startswith(k)][0]
        out.append((ch.name, key, trimesh.load(tmp)))
    return out


def section(ax, meshes, origin, normal, T, xlim, ylim, title):
    for name, key, m in meshes:
        sec = m.section(plane_origin=origin, plane_normal=normal)
        if sec is None:
            continue
        p2, _ = sec.to_planar(to_2D=T)
        for poly in p2.polygons_full:
            ax.fill(*poly.exterior.xy, fc=COL[key], ec="#222", lw=0.7, zorder=2)
            for h in poly.interiors:
                ax.fill(*h.xy, fc="white", ec="#222", lw=0.7, zorder=3)
    ax.set_aspect("equal"); ax.set_xlim(*xlim); ax.set_ylim(*ylim); ax.set_title(title, fontsize=9); ax.grid(True, lw=0.2, alpha=0.4)


def fig_sections():
    meshes = assembly_meshes()
    fig, axs = plt.subplots(1, 2, figsize=(14, 6.2), gridspec_kw={"width_ratios": [1.15, 1]})
    T_end = np.array([[0, 1, 0, 0], [0, 0, 1, 0], [1, 0, 0, 0], [0, 0, 0, 1.0]])         # (x, y, z) -> (y, z)
    section(axs[0], meshes, [0, 0, 0], [1, 0, 0], T_end, (-50, 34), (-30, 50), "End view, section through the posts of the rear support (y toward the wood, z up)")
    axs[0].axhline(36, color="#b8860b", lw=0.6, ls="--"); axs[0].text(-49, 37, "plank underside z = 36", fontsize=7, color="#7a5a20")
    axs[0].text(27.5, -8, "wood y = 27", fontsize=7, rotation=90, color="#7a5a20")
    section(axs[1], meshes, [0, 0, 32.7], [0, 0, 1], np.eye(4), (-12, 12), (-50, 34), "Plan section at mid-plate (x = motor axis, toward the coupler on the right)")
    fig.tight_layout(); fig.savefig(os.path.join(DOCS, "assembly_sections.png"), dpi=110); plt.close(fig)


def fig_clip():
    """dimensioned drawing of one clip, origin on the post axis, mouth toward +x"""
    m = B.POST_D / 2 - B.DELTA; ye = B.SLOT_R + B.ENTRY_OPEN
    xr0, xl0, xl1, xe = B.X_RAMP0 - B.XC, B.X_LUG0 - B.XC, B.X_LUG1 - B.XC, B.PLATE_W - B.XC      # 3.5, 4.5, 5.5, 7.5
    fig, ax = plt.subplots(figsize=(9, 6.6))
    ax.add_patch(Rectangle((-B.XC, -8.0), B.PLATE_W, 16.0, fc="#f6c27a", ec="#222", lw=1.2, zorder=1))
    th = np.linspace(np.pi / 2, 3 * np.pi / 2, 60)
    slot = [(xe, ye), (xl1, m), (xl0, m), (xr0, B.SLOT_R), (0, B.SLOT_R)] + [(B.SLOT_R * np.cos(t), B.SLOT_R * np.sin(t)) for t in th[1:-1]] + \
           [(0, -B.SLOT_R), (xr0, -B.SLOT_R), (xl0, -m), (xl1, -m), (xe, -ye)]
    ax.fill([p[0] for p in slot], [p[1] for p in slot], fc="white", ec="#222", lw=1.2, zorder=2)
    y_in = B.SLOT_R + B.T_TONGUE
    x0 = B.SLIT_X0 - B.XC
    for s in (1, -1):
        ax.add_patch(Rectangle((x0, y_in if s > 0 else -(y_in + B.SLIT_W)), xe - x0 + 0.01, B.SLIT_W, fc="white", ec="#222", lw=1.0, zorder=2))
    ax.add_patch(plt.Circle((0, 0), B.POST_D / 2, fc="#9aa0ad", ec="#222", lw=1.0, zorder=3)); ax.text(0, 0, "post\n\u00d86.0", ha="center", va="center", fontsize=8, zorder=4)
    ax.plot([-4.5, 9.5], [0, 0], color="#777", lw=0.6, ls="-.", zorder=5)

    def dim(p, q, txt, off=(0, 0), col="#1f5fbf"):
        ax.annotate("", xy=q, xytext=p, arrowprops=dict(arrowstyle="<->", lw=1.0, color=col), zorder=8)
        ax.text((p[0] + q[0]) / 2 + off[0], (p[1] + q[1]) / 2 + off[1], txt, fontsize=8, color=col, ha="center", va="center", zorder=9, bbox=dict(fc="white", ec="none", pad=0.6))
    dim((-3.0, -B.SLOT_R), (-3.0, B.SLOT_R), "6.6", (-1.0, 0))
    dim((xl0 + 0.5, -m), (xl0 + 0.5, m), "5.4 (mouth)", (0, 0.0))
    dim((0, -9.7), (xe, -9.7), "7.5 from the post axis to the plate edge", (0, -0.65))
    for (p, q, t) in (((0, xr0, "3.5")), ((xr0, xl0, "1")), ((xl0, xl1, "1")), ((xl1, xe, "2"))):
        dim((p, -8.6), (q, -8.6), t, (0, 0.55))
    dim((x0, y_in - B.T_TONGUE + 0.0 + 0.0), (x0, y_in), "1.5", (-0.9, 0.0), "#c0392b")
    dim((x0 - 1.4, y_in), (x0 - 1.4, y_in + B.SLIT_W), "1.0", (-0.9, 0.0), "#c0392b")
    ax.text(x0, y_in + 1.9, "slit starts at x = %.1f (rounded end)" % x0, fontsize=7.5, color="#c0392b", ha="left")
    ax.text(xe + 0.3, ye + 0.15, "entry \u00b1%.1f at the edge" % ye, fontsize=7.5, color="#444", ha="left")
    ax.text(-B.XC + 0.3, 7.4, "plate 15 wide (x), 6 thick, tongue 1.5, interference 0.30 per side (sample 4); mouth toward +x", fontsize=8, ha="left")
    ax.set_aspect("equal"); ax.set_xlim(-8.5, 13.5); ax.set_ylim(-10.6, 9.0); ax.axis("off")
    ax.set_title("Snap clip, sample 4 \u2014 top view, origin on the post axis (mm)", fontsize=10)
    fig.tight_layout(); fig.savefig(os.path.join(DOCS, "clip_sample4_drawing.png"), dpi=130); plt.close(fig)


if __name__ == "__main__":
    fig_sections(); fig_clip(); print("figures ok")
