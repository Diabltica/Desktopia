"""Helpers to compose IKEA-style exploded assembly drawings from CadQuery parts (uses lineart.py)."""
import os, sys, math
import numpy as np
import cadquery as cq
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Circle
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lineart import Scene

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))
IMG = os.path.join(ROOT, "docs", "img")

# fill colours : TPU teal, PETG light grey, hardware mid grey, wood, "your part" blue
TPU, PETG, METAL, WOOD, YOURS, GHOST = "#bfe9dc", "#eeeeee", "#c4c4c8", "#f1dfbd", "#d6e3f7", "#f7f7f7"
INK = "#111111"


class Step:
    def __init__(self, number=None, size=(6.4, 4.8), cam=(1.0, -1.15, 0.85)):
        self.sc = Scene(cam)
        self.pts = []                     # extra 3D points (+ screen offsets) that must stay inside the picture
        self.number = number
        self.size = size
        self.deco = []                   # callables executed after drawing, with (ax, scene)

    def add(self, wp, fill=PETG, emph=True, ls="-", lw=None):
        self.sc.add(wp, fill=fill, lw=(lw if lw else (1.35 if emph else 0.8)), ec=(INK if emph else "#777777"), ls=ls)
        return self

    # ---- decorations (3D points -> projected)
    def guide(self, a, b, color="#555555"):
        self.pts += [(a, 0, 0), (b, 0, 0)]
        self.deco.append(lambda ax, sc, a=a, b=b: ax.plot(*zip(sc.project(a), sc.project(b)), ls=(0, (4, 3)), lw=0.8, color=color, zorder=500))
        return self

    def arrow(self, a, b, lw=2.2, color=INK):
        self.pts += [(a, 0, 0), (b, 0, 0)]
        def f(ax, sc, a=a, b=b):
            ax.annotate("", xy=sc.project(b), xytext=sc.project(a), zorder=600,
                        arrowprops=dict(arrowstyle="-|>,head_length=0.7,head_width=0.35", lw=lw, color=color, shrinkA=0, shrinkB=0))
        self.deco.append(f); return self

    def text(self, p, s, dx=0, dy=0, fs=11, bold=True, ha="center", box=False, color=INK):
        self.pts += [(p, dx, dy)]
        def f(ax, sc, p=p, s=s):
            x, y = sc.project(p)
            ax.text(x + dx, y + dy, s, fontsize=fs, fontweight="bold" if bold else "normal", ha=ha, va="center", color=color,
                    zorder=700, bbox=(dict(boxstyle="round,pad=0.25", fc="white", ec=INK, lw=1.0) if box else None))
        self.deco.append(f); return self

    def qty(self, p, n, dx=0, dy=0):
        return self.text(p, f"{n}×", dx=dx, dy=dy, fs=15)

    def save(self, name, pad=6.0):
        fig = plt.figure(figsize=self.size, dpi=150); fig.patch.set_facecolor("white")
        ax = fig.add_axes([0.0, 0.0, 1.0, 1.0])
        b = list(self.sc.draw(ax))
        for p3, dx, dy in self.pts:
            x, y = self.sc.project(p3); x += dx; y += dy
            b = [min(b[0], x - 4), max(b[1], x + 4), min(b[2], y - 4), max(b[3], y + 4)]
        for d in self.deco: d(ax, self.sc)
        w, h = b[1] - b[0] + 2 * pad, b[3] - b[2] + 2 * pad
        aspect = self.size[0] / self.size[1]
        if w / h < aspect:   # widen
            cx = (b[0] + b[1]) / 2; w = h * aspect; ax.set_xlim(cx - w / 2, cx + w / 2); ax.set_ylim(b[2] - pad, b[3] + pad)
        else:
            cy = (b[2] + b[3]) / 2; h = w / aspect; ax.set_ylim(cy - h / 2, cy + h / 2); ax.set_xlim(b[0] - pad, b[1] + pad)
        if self.number is not None:
            fig.text(0.035, 0.935, str(self.number), fontsize=30, fontweight="bold", ha="left", va="center", color=INK,
                     bbox=dict(boxstyle="round,pad=0.18", fc="white", ec=INK, lw=2.2))
        os.makedirs(IMG, exist_ok=True)
        fig.savefig(os.path.join(IMG, name), dpi=150, facecolor="white"); plt.close(fig)
        print("   ", name)


def Z(wp, dz): return wp.translate((0, 0, dz))


def flip_x(wp): return wp.rotate((0, 0, 0), (1, 0, 0), 180)
