"""Line-art (hidden-line-removed) renderer for CadQuery shapes, drawn with matplotlib in an IKEA-like style."""
import re, math, numpy as np, tempfile, os
import cadquery as cq, trimesh
from cadquery.occ_impl.exporters.svg import getSVG
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.collections import PolyCollection, LineCollection

CAM=(1.0,-1.15,0.85)           # camera position direction (front-right-above)

def _solid_polylines(shape, proj):
    svg=getSVG(shape,opts={"width":100,"height":100,"projectionDir":proj,"showAxes":False,"showHidden":False})
    solid=svg.split("<!-- solid lines -->")[1]
    lines=[]
    for d in re.findall(r'd="([^"]+)"',solid):
        for seg in re.split(r'(?=M)',d):
            nums=re.findall(r'-?\d+\.?\d*(?:e-?\d+)?',seg)
            if len(nums)<4 or len(nums)%2: continue
            pts=np.array(nums,float).reshape(-1,2)
            lines.append(pts)
    return lines

def occ_frame(V):
    """(X, Y) axes used by OCC's HLR projector for a projector built from gp_Ax2(origin, V); V points toward the viewer.
    Re-implementation of gp_Ax2's automatic choice of the X direction; Y = V x X."""
    V = np.array(V, float); V /= np.linalg.norm(V)
    A, B, C = V; Aa, Ba, Ca = abs(A), abs(B), abs(C)
    if Ba <= Aa and Ba <= Ca:
        D = (-C, 0.0, A) if Aa > Ca else (C, 0.0, -A)
    elif Aa <= Ba and Aa <= Ca:
        D = (0.0, -C, B) if Ba > Ca else (0.0, C, -B)
    else:
        D = (-B, A, 0.0) if Aa > Ba else (B, -A, 0.0)
    D = np.array(D, float)
    X = D - V * (D @ V); X /= np.linalg.norm(X)
    return X, np.cross(V, X)


def screen_frame(cam):
    """(right, up) of a camera placed at direction `cam` looking at the origin, +Z up on the screen"""
    V = np.array(cam, float); V /= np.linalg.norm(V)
    L = -V                                           # look direction
    right = np.cross(L, [0, 0, 1.0])
    if np.linalg.norm(right) < 1e-9: right = np.array([1.0, 0, 0])
    right /= np.linalg.norm(right)
    up = np.cross(right, L); up /= np.linalg.norm(up)
    return right, up


def to_mesh(shape):
    sh=shape.val() if hasattr(shape,"val") else shape
    f=tempfile.mktemp(suffix=".stl"); cq.exporters.export(cq.Workplane(obj=sh) if not hasattr(shape,"val") else shape,f,tolerance=0.02,angularTolerance=0.15)
    m=trimesh.load(f); os.remove(f); return m

class Scene:
    def __init__(self, cam=CAM):
        self.cam = tuple(cam)
        c = np.array(cam, float); self.v = c / np.linalg.norm(c)          # unit vector toward the viewer
        X, Y = occ_frame(self.v)                                           # frame of the HLR polylines
        r, u = screen_frame(self.v)                                        # frame of the picture (+Z up)
        self.R = np.array([[r @ X, r @ Y], [u @ X, u @ Y]])                # HLR 2D -> picture 2D (pure rotation)
        self.M = np.vstack([r, u]); self.o = np.zeros(2)
        self.parts = []
    def add(self,shape,fill="#ffffff",lw=1.1,ec="#111111",ls="-",label=None,alpha=1.0):
        sh=shape.val() if hasattr(shape,"val") else shape
        self.parts.append(dict(shape=sh,fill=fill,lw=lw,ec=ec,ls=ls,label=label,alpha=alpha,wp=shape))
        return self
    def translate(self,idx,v): pass
    def project(self,p3): return (self.M@np.asarray(p3,float).T).T+self.o
    def draw(self,ax):
        # depth sort : far -> near. camera sits at +proj direction ; larger p.d = nearer
        items=[]
        for p in self.parts:
            bb=p["shape"].BoundingBox(); c=np.array([bb.center.x,bb.center.y,bb.center.z])
            items.append((float(c@self.v),p))
        items.sort(key=lambda t:t[0])    # far (small p.toward_viewer) first, near last
        allpts=[]
        for k,(_,p) in enumerate(items):
            m=to_mesh(p["wp"]); tri=m.vertices[m.faces]
            tri2=np.stack([self.project(t) for t in tri])
            pc=PolyCollection(tri2,facecolors=p["fill"],edgecolors=p["fill"],linewidths=0.35,alpha=p["alpha"],zorder=2+2*k); ax.add_collection(pc)
            lines=[l@self.R.T for l in _solid_polylines(p["shape"],self.cam)]
            lc=LineCollection(lines,colors=p["ec"],linewidths=p["lw"],linestyles=p["ls"],zorder=3+2*k,capstyle="round"); ax.add_collection(lc)
            allpts.append(tri2.reshape(-1,2))
        pts=np.vstack(allpts); self.bounds=(pts[:,0].min(),pts[:,0].max(),pts[:,1].min(),pts[:,1].max())
        ax.set_aspect("equal"); ax.axis("off")
        return self.bounds
