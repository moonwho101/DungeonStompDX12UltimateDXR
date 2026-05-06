"""
Generate spellbook.3ds + Textures/book/spellbook.png for DungeonStomp.

Single named object with proper UV coordinates mapped to a 512x512 atlas:
  [U 0.0-0.5, V 0.0-0.5]  front/back cover leather + arcane decoration
  [U 0.5-1.0, V 0.0-0.5]  page block (parchment)
  [U 0.0-0.5, V 0.5-1.0]  spine leather
  [U 0.5-1.0, V 0.5-1.0]  metal parts (gold bands, brackets, medallion, clasp)

Geometry is fully unindexed (3 unique verts per triangle) so the engine's
per-face UV counter maps 1:1 with the vertex UV array in the 3DS chunk.

Coordinate system:
  X : -3.9 (spine)  →  +3.8 (fore-edge)
  Y : -1.4 (back)   →  +1.6 (front cover up)
  Z : -6.4 (foot)   →  +6.4 (head)
"""

import struct
import math
import os
import shutil
import zlib

# ---------------------------------------------------------------------------
# Book / page extents
# ---------------------------------------------------------------------------
BX1, BX2 = -3.9,  3.8
BY1, BY2 = -1.4,  1.6
BZ1, BZ2 = -6.4,  6.4
COVER_T   = 0.42
PG_INSET  = 0.30
PX1, PX2  = BX1 + PG_INSET, BX2
PY1, PY2  = BY1 + COVER_T,  BY2 - COVER_T
PZ1, PZ2  = BZ1 + PG_INSET, BZ2 - PG_INSET

# ---------------------------------------------------------------------------
# UV atlas sub-regions  (umin, umax, vmin, vmax)
# ---------------------------------------------------------------------------
UV_COVER = (0.01, 0.49, 0.01, 0.49)
UV_PAGES = (0.51, 0.99, 0.01, 0.49)
UV_SPINE = (0.01, 0.49, 0.51, 0.99)
UV_METAL = (0.51, 0.99, 0.51, 0.99)

# ---------------------------------------------------------------------------
# Math helpers
# ---------------------------------------------------------------------------

def _cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])

def _sub(a, b):
    return (a[0]-b[0], a[1]-b[1], a[2]-b[2])

def _normalize(v):
    l = math.sqrt(v[0]**2 + v[1]**2 + v[2]**2)
    return (v[0]/l, v[1]/l, v[2]/l) if l > 1e-9 else (0.0, 1.0, 0.0)

def _face_normal(va, vb, vc):
    return _normalize(_cross(_sub(vb, va), _sub(vc, va)))

def _clamp01(x):
    return max(0.0, min(1.0, x))

def _lerp(a, b, t):
    return a + t * (b - a)

# ---------------------------------------------------------------------------
# UV projection + atlas mapping
# ---------------------------------------------------------------------------

def _project_uv(vert, normal, wx, wy, wz):
    """Project one vertex to raw [0..1] UV based on the face's dominant axis."""
    ax, ay, az = abs(normal[0]), abs(normal[1]), abs(normal[2])
    x, y, z = vert
    dx = (wx[1] - wx[0]) or 1.0
    dy = (wy[1] - wy[0]) or 1.0
    dz = (wz[1] - wz[0]) or 1.0
    if ay >= ax and ay >= az:          # Y-dominant → project XZ
        u = (x - wx[0]) / dx
        v = (z - wz[0]) / dz
    elif az >= ax:                     # Z-dominant → project XY
        u = (x - wx[0]) / dx
        v = (y - wy[0]) / dy
    else:                              # X-dominant → project ZY
        u = (z - wz[0]) / dz
        v = (y - wy[0]) / dy
    return (_clamp01(u), _clamp01(v))


def _to_atlas(u_raw, v_raw, region):
    u0, u1, v0, v1 = region
    return (u0 + _clamp01(u_raw) * (u1 - u0),
            v0 + _clamp01(v_raw) * (v1 - v0))

# ---------------------------------------------------------------------------
# Geometry primitives  (indexed internally, expanded to unindexed on export)
# ---------------------------------------------------------------------------

def _orient_outward(verts, faces):
    if not verts:
        return verts, faces
    cx = sum(v[0] for v in verts) / len(verts)
    cy = sum(v[1] for v in verts) / len(verts)
    cz = sum(v[2] for v in verts) / len(verts)
    result = []
    for a, b, c in faces:
        va, vb, vc = verts[a], verts[b], verts[c]
        ab = _sub(vb, va)
        ac = _sub(vc, va)
        n  = _cross(ab, ac)
        cen = ((va[0]+vb[0]+vc[0])/3, (va[1]+vb[1]+vc[1])/3, (va[2]+vb[2]+vc[2])/3)
        out = _sub(cen, (cx, cy, cz))
        dot = n[0]*out[0] + n[1]*out[1] + n[2]*out[2]
        result.append((a, c, b) if dot < 0 else (a, b, c))
    return verts, result


def make_box(x1, y1, z1, x2, y2, z2):
    v, f = [], []
    for quad in [
        [(x1,y2,z1),(x2,y2,z1),(x2,y2,z2),(x1,y2,z2)],
        [(x2,y1,z1),(x1,y1,z1),(x1,y1,z2),(x2,y1,z2)],
        [(x2,y2,z1),(x2,y1,z1),(x2,y1,z2),(x2,y2,z2)],
        [(x1,y1,z1),(x1,y2,z1),(x1,y2,z2),(x1,y1,z2)],
        [(x1,y1,z2),(x1,y2,z2),(x2,y2,z2),(x2,y1,z2)],
        [(x1,y2,z1),(x1,y1,z1),(x2,y1,z1),(x2,y2,z1)],
    ]:
        b = len(v)
        v.extend(quad)
        f.extend([(b, b+1, b+2), (b, b+2, b+3)])
    return _orient_outward(v, f)


def make_prism(cx, cz, radius, y_bot, y_top, sides=8):
    pts = [(cx + radius*math.cos(2*math.pi*i/sides),
            cz + radius*math.sin(2*math.pi*i/sides)) for i in range(sides)]
    v, f = [], []
    for i in range(sides):
        ni = (i+1) % sides
        b = len(v)
        v += [(pts[i][0], y_bot, pts[i][1]), (pts[ni][0], y_bot, pts[ni][1]),
              (pts[ni][0], y_top, pts[ni][1]), (pts[i][0],  y_top, pts[i][1])]
        f += [(b, b+1, b+2), (b, b+2, b+3)]
    b = len(v); v.append((cx, y_top, cz)); ct = b
    for pt in pts: v.append((pt[0], y_top, pt[1]))
    for i in range(sides): f.append((ct, ct+1+i, ct+1+(i+1)%sides))
    b = len(v); v.append((cx, y_bot, cz)); cb = b
    for pt in pts: v.append((pt[0], y_bot, pt[1]))
    for i in range(sides): f.append((cb, cb+1+(i+1)%sides, cb+1+i))
    return _orient_outward(v, f)


def make_ring(cx, cz, r_in, r_out, y_bot, y_top, sides=14):
    pi = [(cx + r_in *math.cos(2*math.pi*i/sides),
           cz + r_in *math.sin(2*math.pi*i/sides)) for i in range(sides)]
    po = [(cx + r_out*math.cos(2*math.pi*i/sides),
           cz + r_out*math.sin(2*math.pi*i/sides)) for i in range(sides)]
    v, f = [], []
    for i in range(sides):
        ni = (i+1) % sides
        b = len(v)
        v += [(po[i][0],y_bot,po[i][1]),(po[ni][0],y_bot,po[ni][1]),
              (po[ni][0],y_top,po[ni][1]),(po[i][0],y_top,po[i][1])]
        f += [(b,b+1,b+2),(b,b+2,b+3)]
        b = len(v)
        v += [(pi[ni][0],y_bot,pi[ni][1]),(pi[i][0],y_bot,pi[i][1]),
              (pi[i][0],y_top,pi[i][1]),(pi[ni][0],y_top,pi[ni][1])]
        f += [(b,b+1,b+2),(b,b+2,b+3)]
        b = len(v)
        v += [(pi[i][0],y_top,pi[i][1]),(pi[ni][0],y_top,pi[ni][1]),
              (po[ni][0],y_top,po[ni][1]),(po[i][0],y_top,po[i][1])]
        f += [(b,b+1,b+2),(b,b+2,b+3)]
        b = len(v)
        v += [(po[i][0],y_bot,po[i][1]),(po[ni][0],y_bot,po[ni][1]),
              (pi[ni][0],y_bot,pi[ni][1]),(pi[i][0],y_bot,pi[i][1])]
        f += [(b,b+1,b+2),(b,b+2,b+3)]
    return v, f


def _combine(mesh_list):
    av, af, off = [], [], 0
    for verts, faces in mesh_list:
        av.extend(verts)
        af.extend((a+off, b+off, c+off) for a, b, c in faces)
        off += len(verts)
    return av, af

# ---------------------------------------------------------------------------
# Spellbook part list  →  (verts, faces, uv_region, wx, wy, wz)
# ---------------------------------------------------------------------------

def _build_parts():
    parts = []   # (verts, faces, region, wx, wy, wz)

    # World bound shortcuts
    WX, WY, WZ   = (BX1,BX2), (BY1,BY2), (BZ1,BZ2)
    PWX, PWY, PWZ = (PX1,PX2), (PY1,PY2), (PZ1,PZ2)

    # Page block
    parts.append((*make_box(PX1,PY1,PZ1, PX2,PY2,PZ2), UV_PAGES, PWX,PWY,PWZ))

    # Front cover
    parts.append((*make_box(BX1,PY2,BZ1, BX2,BY2,BZ2), UV_COVER, WX,WY,WZ))

    # Back cover
    parts.append((*make_box(BX1,BY1,BZ1, BX2,PY1,BZ2), UV_COVER, WX,WY,WZ))

    # Spine
    parts.append((*make_box(BX1,BY1,BZ1, PX1,BY2,BZ2), UV_SPINE, WX,WY,WZ))

    # Spine reinforcement bands
    for cz in (-3.5, 0.0, 3.5):
        parts.append((*make_box(BX1-0.15, BY1-0.06, cz-0.55,
                                PX1+0.07, BY2+0.06, cz+0.55),
                      UV_METAL, WX, WY, WZ))

    # Corner brackets (L-shape + rivet)
    for xs, zs in ((-1,-1),(1,-1),(-1,1),(1,1)):
        xe = BX1 if xs < 0 else BX2
        ze = BZ1 if zs < 0 else BZ2
        xi = xe + (0.45 if xs < 0 else -0.45)
        zi = ze + (0.45 if zs < 0 else -0.45)
        xf = xe + (2.05 if xs < 0 else -2.05)
        zf = ze + (2.05 if zs < 0 else -2.05)
        v1,f1 = make_box(min(xe,xi),BY2,min(ze,zf), max(xe,xi),BY2+0.13,max(ze,zf))
        v2,f2 = make_box(min(xi,xf),BY2,min(ze,zi), max(xi,xf),BY2+0.13,max(ze,zi))
        v3,f3 = make_box(xi-0.19,BY2,zi-0.19, xi+0.19,BY2+0.21,zi+0.19)
        cv,cf = _combine([(v1,f1),(v2,f2),(v3,f3)])
        parts.append((cv, cf, UV_METAL, WX, WY, WZ))

    # Arcane medallion ring
    parts.append((*make_ring(0,0,1.30,2.28, BY2,BY2+0.14, sides=14),
                  UV_METAL, WX, WY, WZ))

    # Five rune marks
    for k in range(5):
        ang = 2*math.pi*k/5 + math.pi/2
        rx, rz = 1.78*math.cos(ang), 1.78*math.sin(ang)
        parts.append((*make_prism(rx,rz,0.27,BY2,BY2+0.24,sides=6),
                      UV_METAL, WX, WY, WZ))

    # Centre gem
    parts.append((*make_prism(0,0,0.72,BY2,BY2+0.34,sides=8),
                  UV_METAL, WX, WY, WZ))

    # Clasp
    v1,f1 = make_box(BX2-0.07,-0.50,-1.15, BX2+0.30,0.50,1.15)
    v2,f2 = make_box(BX2+0.30,-0.40,-0.95, BX2+0.70,0.40,0.95)
    cv,cf = _combine([(v1,f1),(v2,f2)])
    parts.append((cv, cf, UV_METAL, WX, WY, WZ))

    return parts

# ---------------------------------------------------------------------------
# Expand to unindexed geometry with per-vertex UVs
# ---------------------------------------------------------------------------

def _expand(parts):
    """
    Returns (flat_verts, flat_uvs, sequential_faces).
    Face i uses vertices (3i, 3i+1, 3i+2) — no vertex sharing.
    This makes the engine's UV counter align exactly with the vertex array.
    """
    out_v, out_uv, out_f = [], [], []
    for verts, faces, region, wx, wy, wz in parts:
        for ia, ib, ic in faces:
            va, vb, vc = verts[ia], verts[ib], verts[ic]
            n = _face_normal(va, vb, vc)
            base = len(out_v)
            out_v.extend([va, vb, vc])
            for vert in (va, vb, vc):
                ur, vr = _project_uv(vert, n, wx, wy, wz)
                out_uv.append(_to_atlas(ur, vr, region))
            out_f.append((base, base+1, base+2))
    return out_v, out_uv, out_f

# ---------------------------------------------------------------------------
# 3DS writer — single object
# ---------------------------------------------------------------------------

def write_3ds(filepath, verts, uvs, faces):
    TEX_NAME = b'bookc.bmp'
    MAT_NAME = b'SpellbookMat'

    # MATERIAL chunk 0xAFFF  —  name + texture map filename
    # MAT_NAME 0xA000
    mat_name_data = MAT_NAME + b'\x00'
    mat_name_chunk = struct.pack('<HI', 0xA000, 6 + len(mat_name_data)) + mat_name_data
    # MAP_FILENAME 0xA300
    map_fname_data = TEX_NAME + b'\x00'
    map_fname_chunk = struct.pack('<HI', 0xA300, 6 + len(map_fname_data)) + map_fname_data
    # TEXTURE_MAP 0xA200
    texmap_chunk = struct.pack('<HI', 0xA200, 6 + len(map_fname_chunk)) + map_fname_chunk
    # MATERIAL 0xAFFF
    mat_payload = mat_name_chunk + texmap_chunk
    mat_chunk = struct.pack('<HI', 0xAFFF, 6 + len(mat_payload)) + mat_payload

    # VERT_LIST 0x4110
    vd = struct.pack('<H', len(verts))
    for x, y, z in verts:
        vd += struct.pack('<fff', x, y, z)
    vc = struct.pack('<HI', 0x4110, 6+len(vd)) + vd

    # MAP_COORDS 0x4140  (one UV per vertex, order matches vertex list)
    ud = struct.pack('<H', len(uvs))
    for u, v in uvs:
        ud += struct.pack('<ff', u, v)
    uc = struct.pack('<HI', 0x4140, 6+len(ud)) + ud

    # MSH_MAT_GROUP 0x4130 — assign all faces to SpellbookMat (inside FACE_LIST)
    mg_data = MAT_NAME + b'\x00' + struct.pack('<H', len(faces))
    for i in range(len(faces)):
        mg_data += struct.pack('<H', i)
    mg_chunk = struct.pack('<HI', 0x4130, 6 + len(mg_data)) + mg_data

    # FACE_LIST 0x4120  (face data + material group sub-chunk)
    fd = struct.pack('<H', len(faces))
    for a, b, c in faces:
        fd += struct.pack('<HHHH', a, b, c, 0x0007)
    fc_payload = fd + mg_chunk
    fc = struct.pack('<HI', 0x4120, 6 + len(fc_payload)) + fc_payload

    # TRI_MESH 0x4100
    mesh_pay = vc + uc + fc
    mc = struct.pack('<HI', 0x4100, 6+len(mesh_pay)) + mesh_pay

    # NAMED_OBJ 0x4000
    nb = b'Spellbook\x00'
    oc = struct.pack('<HI', 0x4000, 6+len(nb)+len(mc)) + nb + mc

    # EDIT3DS 0x3D3D  (material chunk comes before named object)
    ver3d = struct.pack('<HI', 0x3D3E, 10) + struct.pack('<I', 3)
    edit  = struct.pack('<HI', 0x3D3D, 6+len(ver3d)+len(mat_chunk)+len(oc)) + ver3d + mat_chunk + oc

    # MAIN 0x4D4D
    ver   = struct.pack('<HI', 0x0002, 10) + struct.pack('<I', 3)
    body  = ver + edit
    main  = struct.pack('<HI', 0x4D4D, 6+len(body)) + body

    with open(filepath, 'wb') as fh:
        fh.write(main)
    print(f"Written : {filepath}")
    print(f"  1 object   Verts : {len(verts)}   Faces : {len(faces)}")

# ---------------------------------------------------------------------------
# PNG texture generator  (no external dependencies)
# ---------------------------------------------------------------------------

def _write_png(path, pixels, w, h):
    def pack_row(row):
        return b'\x00' + bytes(c for px in row for c in px)
    raw = b''.join(pack_row(pixels[y]) for y in range(h))
    comp = zlib.compress(raw, 6)

    def chunk(tag, data):
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', crc)

    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)))
        f.write(chunk(b'IDAT', comp))
        f.write(chunk(b'IEND', b''))
    print(f"Written : {path}  ({w}x{h})")


def _noise(x, y, scale=15):
    s = (x * 1000 + y + x * y * 7) & 0x7FFFFFFF
    s = (s ^ (s >> 16)) * 0x45d9f3b & 0x7FFFFFFF
    s = (s ^ (s >> 16)) * 0x45d9f3b & 0x7FFFFFFF
    return (s & 0xFF) / 255.0 * scale


def generate_texture(path, size=512):
    W = H = size
    half = size // 2

    img = [[(0, 0, 0)] * W for _ in range(H)]

    def sp(x, y, r, g, b):
        if 0 <= x < W and 0 <= y < H:
            img[y][x] = (int(r) & 0xFF, int(g) & 0xFF, int(b) & 0xFF)

    def fill(x1, y1, x2, y2, r, g, b):
        for y in range(y1, y2):
            for x in range(x1, x2):
                sp(x, y, r, g, b)

    def circle(cx, cy, rad, col, thick=2):
        for dy in range(-rad-thick, rad+thick+1):
            for dx in range(-rad-thick, rad+thick+1):
                d = math.sqrt(dx*dx + dy*dy)
                if rad - thick <= d <= rad + thick:
                    sp(cx+dx, cy+dy, *col)

    def line(x1, y1, x2, y2, col, thick=2):
        dx, dy = abs(x2-x1), abs(y2-y1)
        sx = 1 if x1 < x2 else -1
        sy = 1 if y1 < y2 else -1
        err = dx - dy
        x, y = x1, y1
        while True:
            for tx in range(-thick//2, thick//2+1):
                for ty in range(-thick//2, thick//2+1):
                    sp(x+tx, y+ty, *col)
            if x == x2 and y == y2:
                break
            e2 = 2 * err
            if e2 > -dy:
                err -= dy; x += sx
            if e2 < dx:
                err += dx; y += sy

    LEATHER = (72, 24, 8)
    GOLD    = (210, 172, 42)
    GOLD_DK = (155, 118, 18)
    SPINE_C = (42, 14, 5)
    PAGE_BG = (240, 233, 208)
    PAGE_LN = (175, 162, 132)
    METAL   = (195, 158, 38)
    METAL_H = (230, 200, 85)
    METAL_D = (128, 98, 18)
    GEM_A   = (72, 188, 228)
    GEM_B   = (38, 140, 208)

    # ── Region 1: Cover leather  (top-left, cols 0..half, rows 0..half) ──
    cx, cy = half // 2, half // 2
    for y in range(half):
        for x in range(half):
            n = _noise(x, y, 18)
            dist = min(x, y, half-1-x, half-1-y)
            if dist < 9:                               # gold border
                t = dist / 9.0
                r = GOLD_DK[0] + t * (LEATHER[0] - GOLD_DK[0])
                g = GOLD_DK[1] + t * (LEATHER[1] - GOLD_DK[1])
                b = GOLD_DK[2] + t * (LEATHER[2] - GOLD_DK[2])
                sp(x, y, r + n*0.3, g + n*0.1, b + n*0.05)
            else:
                sp(x, y, LEATHER[0]+n*0.5, LEATHER[1]+n*0.2, LEATHER[2]+n*0.1)

    # Outer ring
    circle(cx, cy, 90, GOLD, thick=3)
    circle(cx, cy, 78, GOLD_DK, thick=2)

    # Pentagram (connect every-other vertex of pentagon)
    PR = 68
    def pp(k):
        a = 2*math.pi*k/5 - math.pi/2
        return (int(cx + PR*math.cos(a)), int(cy + PR*math.sin(a)))
    for i, j in ((0,2),(2,4),(4,1),(1,3),(3,0)):
        line(*pp(i), *pp(j), GOLD, thick=2)

    # Rune dots at pentagram tips
    for k in range(5):
        px, py = pp(k)
        circle(px, py, 7, GOLD, thick=2)

    # Centre gem
    GR = 13
    for dy in range(-GR-2, GR+3):
        for dx in range(-GR-2, GR+3):
            d = math.sqrt(dx*dx + dy*dy)
            if d <= GR:
                ang = math.atan2(dy, dx)
                seg = int(ang / (math.pi / 4)) % 2
                sp(cx+dx, cy+dy, *(GEM_A if seg == 0 else GEM_B))
            elif d <= GR + 2:
                sp(cx+dx, cy+dy, *GOLD)

    # ── Region 2: Page block  (top-right, cols half..W, rows 0..half) ──
    for y in range(half):
        for x in range(half, W):
            n = _noise(x, y, 12)
            sp(x, y, PAGE_BG[0]-int(n*0.4), PAGE_BG[1]-int(n*0.4), PAGE_BG[2]-int(n*0.3))
    for ly in range(14, half - 4, 10):
        for x in range(half + 6, W - 6):
            img[ly][x] = PAGE_LN

    # ── Region 3: Spine leather  (bottom-left, cols 0..half, rows half..H) ──
    for y in range(half, H):
        for x in range(half):
            n = _noise(x, y, 22)
            sp(x, y, SPINE_C[0]+int(n*0.7), SPINE_C[1]+int(n*0.3), SPINE_C[2]+int(n*0.1))
    # Three gold bands
    for by in (half + half//5, half + half//2, half + 4*half//5):
        bh = 16
        for y in range(by - bh//2, by + bh//2):
            for x in range(0, half):
                if 0 <= y < H:
                    t = math.sin((y - (by - bh//2)) / bh * math.pi)
                    hl = int(t * 38)
                    sp(x, y, min(255, GOLD[0]-18+hl), min(255, GOLD[1]-18+hl), min(255, GOLD[2]-4+hl//4))

    # ── Region 4: Metal / gold  (bottom-right, cols half..W, rows half..H) ──
    mc2, mr2 = half + half//2, half + half//2
    for y in range(half, H):
        for x in range(half, W):
            n = _noise(x, y, 14)
            dx2 = (x - mc2) / (half * 0.7)
            dy2 = (y - mr2) / (half * 0.7)
            t = _clamp01(1.0 - math.sqrt(dx2*dx2 + dy2*dy2))
            r = _lerp(METAL_D[0], METAL_H[0], t) + n * 0.4
            g = _lerp(METAL_D[1], METAL_H[1], t) + n * 0.3
            b = _lerp(METAL_D[2], METAL_H[2], t) + n * 0.1
            sp(x, y, r, g, b)

    _write_png(path, img, W, H)

# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    base_3ds = os.path.normpath(os.path.join(os.path.dirname(__file__), '..', 'Models', '3ds'))
    base_tex = os.path.normpath(os.path.join(os.path.dirname(__file__), '..', 'Textures', 'book'))

    model_path = os.path.join(base_3ds, 'spellbook.3ds')
    bak_path   = os.path.join(base_3ds, 'spellbook_orig.3ds')
    tex_path   = os.path.join(base_tex, 'spellbook.png')

    if not os.path.exists(bak_path) and os.path.exists(model_path):
        shutil.copy2(model_path, bak_path)
        print(f"Backup  : {bak_path}")

    parts = _build_parts()
    verts, uvs, faces = _expand(parts)
    write_3ds(model_path, verts, uvs, faces)

    os.makedirs(base_tex, exist_ok=True)
    generate_texture(tex_path)


if __name__ == '__main__':
    main()
