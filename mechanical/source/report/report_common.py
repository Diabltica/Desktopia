import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Polygon, Wedge, Circle
import numpy as np, math
BLUE = '#1f5fbf'
RED = '#c0392b'
TEAL = '#1D9E75'
GREY = '#8a8a8a'
DK = '#222222'
WOOD = '#9a7b4f'
plt.rcParams.update({'font.family': 'DejaVu Sans', 'pdf.fonttype': 42})
A4 = (11.69, 8.27)

def dim(ax, p1, p2, txt, tp=None, color=DK, fs=7.5, both=True, lw=0.8, pad=0.6):
    ax.annotate('', xy=p2, xytext=p1, arrowprops=dict(arrowstyle='<->' if both else '->', lw=lw, color=color, shrinkA=0, shrinkB=0))
    if txt:
        if tp is None:
            tp = ((p1[0] + p2[0]) / 2, (p1[1] + p2[1]) / 2)
        ax.text(tp[0], tp[1], txt, fontsize=fs, color=color, ha='center', va='center', bbox=dict(fc='white', ec='none', pad=pad), zorder=10)

def ext(ax, p1, p2, color=GREY, lw=0.5):
    ax.plot([p1[0], p2[0]], [p1[1], p2[1]], color=color, lw=lw, ls=(0, (3, 2)), zorder=2)

def tag(ax, xy, n, color=BLUE, r=2.0, fs=8):
    ax.add_patch(Circle(xy, r, fc='white', ec=color, lw=1.0, zorder=12))
    ax.text(xy[0], xy[1], str(n), fontsize=fs, color=color, ha='center', va='center', zorder=13, fontweight='bold')

def wood(ax, x0, x1, y0, y1):
    ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, fc='#f3ead8', ec=WOOD, lw=0.8, hatch='////', zorder=1))

def page_title(fig, t, sub=None):
    fig.text(0.03, 0.955, t, fontsize=15, fontweight='bold', color=DK, va='center')
    if sub:
        fig.text(0.03, 0.925, sub, fontsize=8.5, color='#555555', va='center')

def textblock(fig, x, y, lines, fs=8.2, lh=0.027, w=None, color=DK):
    """lines: list of (text, style) ; style in {'h','b','n','s'}"""
    yy = y
    for t, s in lines:
        if s == 'h':
            fig.text(x, yy, t, fontsize=fs + 1.3, fontweight='bold', color=BLUE, va='top')
            yy -= lh * 1.25
        elif s == 'b':
            fig.text(x, yy, t, fontsize=fs, color=color, va='top')
            yy -= lh
        elif s == 's':
            fig.text(x, yy, t, fontsize=fs - 0.9, color='#555555', va='top')
            yy -= lh * 0.95
        elif s == 'gap':
            yy -= lh * 0.5
        else:
            fig.text(x, yy, t, fontsize=fs, color=color, va='top')
            yy -= lh
    return yy

# ------------------------------------------------------------------ additions for the slide-mount report
import sys, os
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
import build_slide_mount as B                                       # every number of the report comes from the CAD script
from matplotlib.patches import Rectangle, Polygon, Wedge, Circle, FancyBboxPatch
from matplotlib.lines import Line2D

ORANGE, RING, PLOT, BRASS, SCREWC = '#f6c27a', '#c6d3ea', '#9aa0ad', '#e0c25a', '#c4c4c8'
TPUF, TPUE = '#bfe8d9', '#0F6E56'
DIM = '#222222'


def R(ax, y0, y1, z0, z1, fc, ec=DK, lw=0.8, z=3, hatch=None):
    p = Rectangle((min(y0, y1), min(z0, z1)), abs(y1 - y0), abs(z1 - z0), fc=fc, ec=ec, lw=lw, zorder=z, hatch=hatch)
    ax.add_patch(p)
    return p


def POLY(ax, pts, fc, ec=DK, lw=0.8, z=3):
    ax.add_patch(Polygon(pts, closed=True, fc=fc, ec=ec, lw=lw, zorder=z))


def center_line(ax, p1, p2, lw=0.5, z=9):
    ax.plot([p1[0], p2[0]], [p1[1], p2[1]], color=DK, lw=lw, ls='-.', zorder=z)


def leader(ax, xy_from, xy_to, color=BLUE, lw=0.8, z=11):
    ax.plot([xy_from[0], xy_to[0]], [xy_from[1], xy_to[1]], color=color, lw=lw, zorder=z)


def note(ax, xy, txt, color=DK, fs=7.2, ha='left', va='center', z=14, bold=False, box=True):
    ax.text(xy[0], xy[1], txt, fontsize=fs, color=color, ha=ha, va=va, zorder=z, fontweight='bold' if bold else 'normal',
            bbox=dict(fc='white', ec='none', pad=0.5) if box else None)


def legend(fig, items, x=0.03, y=0.045, fs=7.5):
    """items: list of (fill, edge, label)"""
    ax = fig.add_axes([0, 0, 1, 1]); ax.axis('off'); ax.set_xlim(0, 1); ax.set_ylim(0, 1)
    xx = x
    for fc, ec, lab in items:
        ax.add_patch(Rectangle((xx, y - 0.008), 0.018, 0.016, fc=fc, ec=ec, lw=0.9))
        ax.text(xx + 0.024, y, lab, fontsize=fs, va='center', color=DK)
        xx += 0.03 + 0.0062 * len(lab)


def table(fig, x0, y0, colx, rows, lh=0.027, fs=7.6, header=True, widths=None, hl_rows=(), color=DK):
    """rows: list of lists of str ; colx: x offsets of the columns (figure fraction, relative to x0)"""
    ax = fig.add_axes([0, 0, 1, 1]); ax.axis('off'); ax.set_xlim(0, 1); ax.set_ylim(0, 1)
    ax.set_zorder(20); ax.patch.set_alpha(0)
    yy = y0
    for i, row in enumerate(rows):
        if i in hl_rows:
            ax.add_patch(Rectangle((x0 - 0.005, yy - lh * 0.85), (widths or 0.5), lh * 0.95, fc='#fff3d6', ec='none'))
        for cx, cell in zip(colx, row):
            ax.text(x0 + cx, yy - lh * 0.4, cell, fontsize=fs, color=(BLUE if (header and i == 0) else color),
                    fontweight='bold' if (header and i == 0) else 'normal', va='center')
        yy -= lh
        if header and i == 0:
            ax.plot([x0 - 0.005, x0 + (widths or 0.5)], [yy + lh * 0.12, yy + lh * 0.12], color=BLUE, lw=0.7)
    return yy


def footer(fig, n, total, title='Slide-in motor supports - assembly report'):
    fig.text(0.03, 0.018, title, fontsize=6.8, color='#888888')
    fig.text(0.97, 0.018, f'{n} / {total}', fontsize=6.8, color='#888888', ha='right')


# ------------------------------------------------------------------ end view of the assembly (y to the right, z up), section through the plot axes
import math


def ring_section(ax, kind='rear', section='window', z0=3.0, lw=0.8, flat=True):
    """end view of the ring of one original support, read in your SLDPRT files.
    rear (motor_mount, bore 48.5): section 'rim' (x 0-3, closed), 'slot' (x 3-6, side slots |z| < 4.25), 'window' (x 6-15, |z| < 11.3), flat at y = 26 for x 0-6
    front (motor_front_mount, bore 48, desk frame): four slots at 45 deg, bottom 26.0 from the axis, the wide one (6.2) is the detrompeur"""
    P = B
    bore = (P.REAR_D if kind == 'rear' else P.FRONT_D) / 2
    ro = P.RING_OD / 2
    ax.add_patch(Wedge((0, 0), ro, 0, 360, width=ro - bore, fc=RING, ec=DK, lw=lw, zorder=z0))
    if kind == 'rear':
        if section in ('rim', 'slot') and flat:
            R(ax, P.RELIEF_Y, ro + 0.3, -P.RELIEF_Z, P.RELIEF_Z, 'white', ec='none', z=z0 + 0.6)
            ax.plot([P.RELIEF_Y, P.RELIEF_Y], [-7.28, 7.28], color=DK, lw=lw, zorder=z0 + 1)
        if section in ('slot', 'window'):
            h = P.REAR_SLOT_Z[1] if False else (P.REAR_SLOT_Z if section == 'slot' else P.REAR_WIN_Z)
            yi = math.sqrt(bore ** 2 - h ** 2)
            for sg in (1, -1):
                R(ax, sg * (yi - 0.5), sg * (26.99 if sg > 0 else 27.3), -h, h, 'white', ec='none', z=z0 + 0.6)
                yo = min(math.sqrt(ro ** 2 - h ** 2), P.RELIEF_Y if (sg > 0 and section == 'slot') else 99.0)
                for hh in (h, -h):
                    ax.plot([sg * yi, sg * yo], [hh, hh], color=DK, lw=lw, zorder=z0 + 1)
    else:
        r0 = bore - 0.5
        for ang, wd in P.front_slots():
            a = math.radians(ang); ux, uy = math.cos(a), math.sin(a); tx, ty = -uy, ux; w2 = wd / 2; rb = P.FRONT_SLOT_BOTTOM
            pts = [(ux * r0 + tx * w2, uy * r0 + ty * w2), (ux * rb + tx * w2, uy * rb + ty * w2), (ux * rb - tx * w2, uy * rb - ty * w2), (ux * r0 - tx * w2, uy * r0 - ty * w2)]
            POLY(ax, pts, 'white', ec=(RED if wd > 5 else DK), lw=(1.3 if wd > 5 else 0.8), z=z0 + 1)
        if flat:
            R(ax, P.RELIEF_Y, ro + 0.3, -P.RELIEF_Z, P.RELIEF_Z, 'white', ec='none', z=z0 + 0.6)
            ax.plot([P.RELIEF_Y, P.RELIEF_Y], [-7.28, 7.28], color=DK, lw=lw, zorder=z0 + 1)


def endview(ax, kind='rear', section='window', plank_t=16.0, tags=True):
    P = B
    ax.set_aspect('equal'); ax.axis('off')
    zt = P.PLANK_Z
    wood(ax, -44.48, 44.48, zt, zt + plank_t)
    wood(ax, P.WOOD_Y, P.WOOD_Y + 7, -30, zt)
    ring_section(ax, kind, section)
    # pad + pockets + inserts + M3 screws
    zb = P.Z_PLATE_BOT
    R(ax, -P.PAD_Y, P.PAD_Y, 24.4, zb, RING, ec='none', z=4.2)
    ax.plot([-P.PAD_Y, -P.PAD_Y], [24.2, zb], color=DK, lw=0.8, zorder=4.3)
    ax.plot([P.PAD_Y, P.PAD_Y], [24.2, zb], color=DK, lw=0.8, zorder=4.3)
    for ys in P.SCREW_Y:
        R(ax, ys - P.INSERT_D / 2, ys + P.INSERT_D / 2, zb - P.INSERT_DEPTH, zb, 'white', lw=0.7, z=4.5)
        R(ax, ys - 2.1, ys + 2.1, zb - 3.0, zb, BRASS, lw=0.6, z=4.6)
    # plate with its gaps
    zp = zb + P.PLATE_T
    R(ax, P.PLATE_Y0, P.PLATE_Y1, zb, zp, ORANGE, z=5)
    for yp in (P.Y_POST_EAR, P.Y_POST_WOOD):
        R(ax, yp - P.SLOT_R, yp + P.SLOT_R, zb, zp, 'white', z=5.5)
    for ys in P.SCREW_Y:
        R(ax, ys - 2.9, ys + 2.9, zp - 3.2, zp, 'white', z=5.5)
        R(ax, ys - 1.7, ys + 1.7, zb, zp - 3.2, 'white', z=5.5)
        R(ax, ys - 2.5, ys + 2.5, zp - 3.2 + 0.2, zp - 0.2, SCREWC, lw=0.6, z=6)
        R(ax, ys - 1.5, ys + 1.5, zp - 3.2 - 5.0, zp - 3.2, SCREWC, lw=0.6, z=6)
    # plots + wood screws
    zs = zt - P.SH_LEN
    zh = zs - P.HEAD_H
    for yp in (P.Y_POST_EAR, P.Y_POST_WOOD):
        R(ax, yp - P.POST_D / 2, yp + P.POST_D / 2, zs, zt, PLOT, z=6.5)
        R(ax, yp - P.HEAD_D / 2, yp + P.HEAD_D / 2, zh, zs, PLOT, z=6.5)
        POLY(ax, [(yp - P.CSK_D / 2, zh), (yp + P.CSK_D / 2, zh), (yp + P.BORE_D / 2, zh + 1.6), (yp - P.BORE_D / 2, zh + 1.6)], 'white', z=6.8)
        R(ax, yp - P.BORE_D / 2, yp + P.BORE_D / 2, zh + 1.6, zt, 'white', ec='none', z=6.8)
        POLY(ax, [(yp - 3.0, zh), (yp + 3.0, zh), (yp + 1.5, zh + 1.5), (yp - 1.5, zh + 1.5)], SCREWC, z=7, lw=0.6)
        R(ax, yp - 1.5, yp + 1.5, zh + 1.5, zh + 25.0, SCREWC, lw=0.6, z=7)
        for zc0 in (zt - P.CUSH_T, P.Z_PLATE_BOT - P.GAP - P.CUSH_T):
            for sg in (-1, 1):
                y0c = yp + sg * P.POST_D / 2
                R(ax, y0c, yp + sg * P.CUSH_OD / 2, zc0, zc0 + P.CUSH_T, TPUF, ec=TPUE, lw=0.8, z=6.6)
        center_line(ax, (yp, zh - 3), (yp, zt + plank_t + 3))
    center_line(ax, (0, -P.RING_OD / 2 - 3), (0, 22), z=9)
    ax.plot([-4, 4], [0, 0], color=DK, lw=0.5, zorder=9)
    ax.set_xlim(-60, 42)
    ax.set_ylim(-36, 66)


def title_block(fig, t, sub=None):
    page_title(fig, t, sub)
