"""Checks the rings of build_slide_mount.py against your SolidWorks parts.

    python compare_rings.py motor_mount.SLDPRT motor_front_mount.SLDPRT

Every vertex of your part that lies on the ring (radius 23.9 to 27.02 mm, inside the ring width) must lie on the rebuilt ring. The front
mount file is mirrored with respect to the desk frame (wood on -y), hence the y flip; its wide slot is then at 315 degrees (FRONT_KEY_ANGLE)."""
import os
import sys
import tempfile
import numpy as np
import trimesh
import cadquery as cq
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
import build_slide_mount as B
from sldprt_faces import faces


def check(path, kind):
    core, w = B.ring_core(kind)
    f = os.path.join(tempfile.gettempdir(), '_ring_check.stl')
    cq.exporters.export(core, f, tolerance=0.005, angularTolerance=0.05)
    mesh = trimesh.load(f)
    V = np.vstack([p for p, n in faces(path)])
    if kind == 'front':
        V = V * np.array([1, -1, 1])
    r = np.hypot(V[:, 1], V[:, 2])
    P = V[(r > 23.9) & (r < 27.02) & (V[:, 0] >= -0.01) & (V[:, 0] <= w + 0.01)]
    d = np.abs(trimesh.proximity.signed_distance(mesh, P))
    print(f'{os.path.basename(path)}: {len(P)} vertices on the ring, max distance to the rebuilt ring {d.max():.3f} mm, beyond 0.05 mm: {(d > 0.05).sum()}')


if __name__ == '__main__':
    check(sys.argv[1], 'rear')
    check(sys.argv[2], 'front')
