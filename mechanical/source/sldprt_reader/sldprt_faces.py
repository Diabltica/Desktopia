"""Minimal reader of the tessellated faces stored in a SolidWorks .SLDPRT (no SolidWorks needed).

A .SLDPRT is a set of named streams (names are nibble-swapped, data is raw deflate). The stream "Contents/DisplayLists" holds, for every
face, an array of vertex positions then an array of normals (float32, metres). Planar faces have one constant normal, so their plane is exact
(offset = n . p) and their vertices are the corners of the face. Coordinates are returned in mm, in the part's own frame.
"""
import struct
import zlib
import numpy as np

MARK = b'\x14\x00\x06\x00\x08\x00'
REC = b'\x0c\x00\x00\x00\x64\x00\x00\x00\x02\x00\x00\x00'


def _unswap(b):
    return bytes(((x & 0xF) << 4) | (x >> 4) for x in b)


def streams(path):
    """dict name -> decompressed bytes (first occurrence of each stream)"""
    d = open(path, 'rb').read()
    out, pos = {}, 0
    while True:
        i = d.find(MARK, pos)
        if i < 0:
            break
        pos = i + 1
        h = i + 6
        try:
            _, _, comp, nl = struct.unpack('<IIII', d[h + 4:h + 20])
        except struct.error:
            continue
        if not 3 <= nl <= 100:
            continue
        try:
            name = _unswap(d[h + 20:h + 20 + nl]).decode('ascii')
        except UnicodeDecodeError:
            continue
        if not all(32 <= ord(c) < 127 for c in name):
            continue
        try:
            data = zlib.decompressobj(-15).decompress(d[h + 20 + nl:h + 20 + nl + comp + 64])
        except zlib.error:
            continue
        if data and name not in out:
            out[name] = data
    return out


def faces(path):
    """list of (vertices mm [n,3], normals [n,3]) for every tessellated face of the part"""
    d = streams(path)['Contents/DisplayLists'] if 'Contents/DisplayLists' in streams(path) else None
    if d is None:
        raise KeyError('no display list in ' + path)
    arrs, i = [], 0
    while True:
        i = d.find(REC, i)
        if i < 0:
            break
        n = struct.unpack_from('<I', d, i + 12)[0]
        a = i + 16
        if n > 0 and a + n * 12 <= len(d):
            arr = np.frombuffer(d[a:a + n * 12], dtype='<f4').reshape(n, 3).astype(float)
            unit = np.allclose(np.linalg.norm(arr, axis=1), 1, atol=1e-3)
            arrs.append(('normal' if unit else ('pos' if np.abs(arr).max() < 0.1 else 'other'), arr))
        else:
            arrs.append(('empty', None))
        i += 1
    out, k = [], 0
    while k < len(arrs) - 1:
        if arrs[k][0] == 'pos' and arrs[k + 1][0] == 'normal' and len(arrs[k + 1][1]) == len(arrs[k][1]):
            out.append((arrs[k][1] * 1000.0, arrs[k + 1][1]))
            k += 2
        else:
            k += 1
    return out
