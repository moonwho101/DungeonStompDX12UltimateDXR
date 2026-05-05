"""
Generate a dungeon spellbook.3ds model for DungeonStomp.

Design — a thick leather-bound tome containing:
  Book body
    - Page block (visible on the 3 open edges)
    - Thick front cover (faces +Y upward)
    - Thick back cover  (faces -Y downward)
    - Spine / binding   (at -X edge)
  Spine detail
    - 3 horizontal metal reinforcement bands, slightly proud of spine face
  Front cover decoration
    - 4 L-shaped corner brackets with rivet at each bend
    - Arcane medallion: outer ring + 5 pentagram rune-marks + centre gem
  Fore-edge detail
    - Metal clasp (plate + hook)

Coordinate system (matches original bounding box):
  X : -3.9 (spine/binding)  →  +3.8 (fore-edge / open side)
  Y : -1.4 (back cover)     →  +1.6 (front cover, visible when lying flat)
  Z : -6.4 (foot)           →  +6.4 (head)
"""

import struct
import math
import os
import shutil


# ---------------------------------------------------------------------------
# Geometry helpers
# ---------------------------------------------------------------------------

def orient_faces_outward(vertices, faces):
    """Flip any triangle whose normal points toward the mesh centroid."""
    if not vertices:
        return vertices, faces
    cx = sum(v[0] for v in vertices) / len(vertices)
    cy = sum(v[1] for v in vertices) / len(vertices)
    cz = sum(v[2] for v in vertices) / len(vertices)
    result = []
    for a, b, c in faces:
        va, vb, vc = vertices[a], vertices[b], vertices[c]
        ab = (vb[0]-va[0], vb[1]-va[1], vb[2]-va[2])
        ac = (vc[0]-va[0], vc[1]-va[1], vc[2]-va[2])
        n = (ab[1]*ac[2] - ab[2]*ac[1],
             ab[2]*ac[0] - ab[0]*ac[2],
             ab[0]*ac[1] - ab[1]*ac[0])
        cen = ((va[0]+vb[0]+vc[0])/3.0,
               (va[1]+vb[1]+vc[1])/3.0,
               (va[2]+vb[2]+vc[2])/3.0)
        out = (cen[0]-cx, cen[1]-cy, cen[2]-cz)
        dot = n[0]*out[0] + n[1]*out[1] + n[2]*out[2]
        result.append((a, c, b) if dot < 0 else (a, b, c))
    return vertices, result


def make_box(x1, y1, z1, x2, y2, z2):
    """Axis-aligned box: 24 verts (4 per face), 12 triangles."""
    v, f = [], []
    for quad in [
        # Front face (y = y2)
        [(x1,y2,z1), (x2,y2,z1), (x2,y2,z2), (x1,y2,z2)],
        # Back face (y = y1)
        [(x2,y1,z1), (x1,y1,z1), (x1,y1,z2), (x2,y1,z2)],
        # Right face (x = x2)
        [(x2,y2,z1), (x2,y1,z1), (x2,y1,z2), (x2,y2,z2)],
        # Left face (x = x1)
        [(x1,y1,z1), (x1,y2,z1), (x1,y2,z2), (x1,y1,z2)],
        # Top face (z = z2)
        [(x1,y1,z2), (x1,y2,z2), (x2,y2,z2), (x2,y1,z2)],
        # Bottom face (z = z1)
        [(x1,y2,z1), (x1,y1,z1), (x2,y1,z1), (x2,y2,z1)],
    ]:
        b = len(v)
        v.extend(quad)
        f.extend([(b, b+1, b+2), (b, b+2, b+3)])
    return orient_faces_outward(v, f)


def make_prism(cx, cz, radius, y_bot, y_top, sides=8):
    """
    Regular-polygon prism standing along the Y axis (centred at cx, cz).
    Used for the centre gem and the 5 pentagram rune marks.
    """
    pts = [
        (cx + radius * math.cos(2*math.pi*i/sides),
         cz + radius * math.sin(2*math.pi*i/sides))
        for i in range(sides)
    ]
    v, f = [], []

    # Side faces
    for i in range(sides):
        ni = (i+1) % sides
        b = len(v)
        v += [(pts[i][0],  y_bot, pts[i][1]),
              (pts[ni][0], y_bot, pts[ni][1]),
              (pts[ni][0], y_top, pts[ni][1]),
              (pts[i][0],  y_top, pts[i][1])]
        f += [(b, b+1, b+2), (b, b+2, b+3)]

    # Top cap
    b = len(v)
    v.append((cx, y_top, cz))
    ct = b
    for pt in pts:
        v.append((pt[0], y_top, pt[1]))
    for i in range(sides):
        f.append((ct, ct+1+i, ct+1+(i+1) % sides))

    # Bottom cap
    b = len(v)
    v.append((cx, y_bot, cz))
    cb = b
    for pt in pts:
        v.append((pt[0], y_bot, pt[1]))
    for i in range(sides):
        f.append((cb, cb+1+(i+1) % sides, cb+1+i))

    return orient_faces_outward(v, f)


def make_ring(cx, cz, r_in, r_out, y_bot, y_top, sides=14):
    """
    Hollow annular prism (ring) standing along the Y axis.
    Face normals are set explicitly so that the top surface always faces +Y.
    """
    pts_i = [(cx + r_in  * math.cos(2*math.pi*i/sides),
              cz + r_in  * math.sin(2*math.pi*i/sides)) for i in range(sides)]
    pts_o = [(cx + r_out * math.cos(2*math.pi*i/sides),
              cz + r_out * math.sin(2*math.pi*i/sides)) for i in range(sides)]
    v, f = [], []

    for i in range(sides):
        ni = (i+1) % sides

        # Outer side wall  — normal points radially outward
        b = len(v)
        v += [(pts_o[i][0],  y_bot, pts_o[i][1]),
              (pts_o[ni][0], y_bot, pts_o[ni][1]),
              (pts_o[ni][0], y_top, pts_o[ni][1]),
              (pts_o[i][0],  y_top, pts_o[i][1])]
        f += [(b, b+1, b+2), (b, b+2, b+3)]

        # Inner side wall — normal points radially inward (reversed winding)
        b = len(v)
        v += [(pts_i[ni][0], y_bot, pts_i[ni][1]),
              (pts_i[i][0],  y_bot, pts_i[i][1]),
              (pts_i[i][0],  y_top, pts_i[i][1]),
              (pts_i[ni][0], y_top, pts_i[ni][1])]
        f += [(b, b+1, b+2), (b, b+2, b+3)]

        # Top annular quad — normal points +Y
        b = len(v)
        v += [(pts_i[i][0],  y_top, pts_i[i][1]),
              (pts_i[ni][0], y_top, pts_i[ni][1]),
              (pts_o[ni][0], y_top, pts_o[ni][1]),
              (pts_o[i][0],  y_top, pts_o[i][1])]
        f += [(b, b+1, b+2), (b, b+2, b+3)]

        # Bottom annular quad — normal points -Y
        b = len(v)
        v += [(pts_o[i][0],  y_bot, pts_o[i][1]),
              (pts_o[ni][0], y_bot, pts_o[ni][1]),
              (pts_i[ni][0], y_bot, pts_i[ni][1]),
              (pts_i[i][0],  y_bot, pts_i[i][1])]
        f += [(b, b+1, b+2), (b, b+2, b+3)]

    return v, f


def combine(mesh_list):
    """Merge a list of (verts, faces) pairs into one mesh."""
    all_v, all_f = [], []
    off = 0
    for verts, faces in mesh_list:
        all_v.extend(verts)
        all_f.extend((a+off, b+off, c+off) for a, b, c in faces)
        off += len(verts)
    return all_v, all_f


# ---------------------------------------------------------------------------
# 3DS binary writer
# ---------------------------------------------------------------------------

def write_3ds(filepath, objects):
    """
    Write a binary .3DS file.
    objects : list of (name_str, vertices, faces)
    vertices: list of (x, y, z)
    faces   : list of (i0, i1, i2)
    """
    edit_payload = b''
    total_v = total_f = 0

    for name, verts, faces in objects:
        # Vertex list  0x4110
        vd = struct.pack('<H', len(verts))
        for x, y, z in verts:
            vd += struct.pack('<fff', x, y, z)
        vc = struct.pack('<HI', 0x4110, 6 + len(vd)) + vd

        # Face list  0x4120
        fd = struct.pack('<H', len(faces))
        for a, b, c in faces:
            fd += struct.pack('<HHHH', a, b, c, 0x0007)
        fc = struct.pack('<HI', 0x4120, 6 + len(fd)) + fd

        # UV mapping  0x4140  (all zeros — no texture coords needed)
        ud = struct.pack('<H', len(verts))
        for _ in verts:
            ud += struct.pack('<ff', 0.0, 0.0)
        uc = struct.pack('<HI', 0x4140, 6 + len(ud)) + ud

        # Triangle mesh chunk  0x4100
        mesh_payload = vc + fc + uc
        mc = struct.pack('<HI', 0x4100, 6 + len(mesh_payload)) + mesh_payload

        # Named object chunk  0x4000
        nb = name.encode('ascii') + b'\x00'
        op = nb + mc
        oc = struct.pack('<HI', 0x4000, 6 + len(op)) + op

        edit_payload += oc
        total_v += len(verts)
        total_f += len(faces)

    ver  = struct.pack('<HI', 0x0002, 10) + struct.pack('<I', 3)
    edit = struct.pack('<HI', 0x3D3D, 6 + len(edit_payload)) + edit_payload
    body = ver + edit
    main = struct.pack('<HI', 0x4D4D, 6 + len(body)) + body

    with open(filepath, 'wb') as fh:
        fh.write(main)

    print(f"Written : {filepath}")
    print(f"Objects : {len(objects)}   Verts : {total_v}   Faces : {total_f}")


# ---------------------------------------------------------------------------
# Spellbook geometry builder
# ---------------------------------------------------------------------------

def build_spellbook():
    objects = []

    # ── Book extents ────────────────────────────────────────────────────────
    BX1, BX2 = -3.9,  3.8   # X: spine (−) to fore-edge (+)
    BY1, BY2 = -1.4,  1.6   # Y: back cover (−) to front cover (+)
    BZ1, BZ2 = -6.4,  6.4   # Z: foot (−) to head (+)

    COVER_T  = 0.42          # leather cover thickness (Y)
    PG_INSET = 0.30          # page block inset from outer book edge

    # Page block extents
    PX1 = BX1 + PG_INSET    # pages start just inside the spine
    PX2 = BX2               # pages run to the fore-edge
    PY1 = BY1 + COVER_T     # pages sit above the back cover
    PY2 = BY2 - COVER_T     # pages sit below the front cover
    PZ1 = BZ1 + PG_INSET   # inset from foot
    PZ2 = BZ2 - PG_INSET   # inset from head

    # ── 1.  Page block ──────────────────────────────────────────────────────
    v, f = make_box(PX1, PY1, PZ1, PX2, PY2, PZ2)
    objects.append(("Pages", v, f))

    # ── 2.  Front cover (top face when lying flat, faces +Y) ───────────────
    v, f = make_box(BX1, PY2, BZ1, BX2, BY2, BZ2)
    objects.append(("CoverFront", v, f))

    # ── 3.  Back cover (faces −Y) ───────────────────────────────────────────
    v, f = make_box(BX1, BY1, BZ1, BX2, PY1, BZ2)
    objects.append(("CoverBack", v, f))

    # ── 4.  Spine (binding, at −X edge) ─────────────────────────────────────
    v, f = make_box(BX1, BY1, BZ1, PX1, BY2, BZ2)
    objects.append(("Spine", v, f))

    # ── 5.  Spine reinforcement bands ───────────────────────────────────────
    #        Three horizontal metal straps, slightly proud of the spine face.
    BAND_PROUD = 0.15
    BAND_HT    = 0.55   # half-height of each band in Z
    BAND_X1    = BX1 - BAND_PROUD
    BAND_X2    = PX1 + 0.07
    BAND_Y1    = BY1 - 0.06
    BAND_Y2    = BY2 + 0.06

    for k, cz in enumerate([-3.5, 0.0, 3.5]):
        v, f = make_box(BAND_X1, BAND_Y1, cz - BAND_HT,
                        BAND_X2, BAND_Y2, cz + BAND_HT)
        objects.append((f"SpineBand{k}", v, f))

    # ── 6.  Corner brackets on front cover ──────────────────────────────────
    #        L-shaped raised metal pieces at each of the 4 corners.
    ARM_LEN   = 2.05    # length of each arm of the L
    ARM_W     = 0.45    # width of each arm
    BRK_PROUD = 0.13    # height above cover surface (Y direction)
    BRK_Y1    = BY2
    BRK_Y2    = BY2 + BRK_PROUD

    for x_sign, z_sign, nm in [(-1,-1,"BrktBL"), ( 1,-1,"BrktBR"),
                                (-1, 1,"BrktTL"), ( 1, 1,"BrktTR")]:
        x_edge = BX1 if x_sign < 0 else BX2
        z_edge = BZ1 if z_sign < 0 else BZ2

        x_inner = x_edge + (ARM_W  if x_sign < 0 else -ARM_W)
        z_inner = z_edge + (ARM_W  if z_sign < 0 else -ARM_W)
        x_far   = x_edge + (ARM_LEN if x_sign < 0 else -ARM_LEN)
        z_far   = z_edge + (ARM_LEN if z_sign < 0 else -ARM_LEN)

        # Arm running along the Z edge (from corner toward book centre in Z)
        v1, f1 = make_box(
            min(x_edge, x_inner), BRK_Y1, min(z_edge, z_far),
            max(x_edge, x_inner), BRK_Y2, max(z_edge, z_far))

        # Arm running along the X edge (non-overlapping with the Z arm)
        v2, f2 = make_box(
            min(x_inner, x_far), BRK_Y1, min(z_edge, z_inner),
            max(x_inner, x_far), BRK_Y2, max(z_edge, z_inner))

        # Rivet square at the inner corner of the L
        rv_r = 0.19
        v3, f3 = make_box(
            x_inner - rv_r, BRK_Y1, z_inner - rv_r,
            x_inner + rv_r, BRK_Y2 + 0.08, z_inner + rv_r)

        brkt_v, brkt_f = combine([(v1, f1), (v2, f2), (v3, f3)])
        objects.append((nm, brkt_v, brkt_f))

    # ── 7.  Arcane medallion on front cover ─────────────────────────────────
    #        Outer ring + five pentagram rune marks + raised centre gem.
    MED_Y0 = BY2            # base of medallion sits on the cover surface

    # Outer decorative ring
    v, f = make_ring(0.0, 0.0, 1.30, 2.28, MED_Y0, MED_Y0 + 0.14, sides=14)
    objects.append(("MedRing", v, f))

    # Five rune marks at 72° intervals (pentagram points), radius = 1.78
    for k in range(5):
        angle = 2*math.pi * k / 5 + math.pi/2   # first mark at top (+Z)
        rx = 1.78 * math.cos(angle)
        rz = 1.78 * math.sin(angle)
        v, f = make_prism(rx, rz, 0.27, MED_Y0, MED_Y0 + 0.24, sides=6)
        objects.append((f"RunePt{k}", v, f))

    # Centre gem — octagonal, tallest element of the medallion
    v, f = make_prism(0.0, 0.0, 0.72, MED_Y0, MED_Y0 + 0.34, sides=8)
    objects.append(("MedGem", v, f))

    # ── 8.  Metal clasp on fore-edge (+X side) ──────────────────────────────
    CL_X1     = BX2
    CL_X2     = BX2 + 0.30     # plate protrudes beyond edge
    CL_Y1, CL_Y2 = -0.40, 0.40
    CL_Z1, CL_Z2 = -0.95, 0.95

    # Clasp body plate (slightly larger than hook)
    v1, f1 = make_box(CL_X1 - 0.07, CL_Y1 - 0.10, CL_Z1 - 0.20,
                      CL_X2,         CL_Y2 + 0.10, CL_Z2 + 0.20)

    # Hook loop that the latch engages
    v2, f2 = make_box(CL_X2,        CL_Y1, CL_Z1,
                      CL_X2 + 0.40, CL_Y2, CL_Z2)

    clasp_v, clasp_f = combine([(v1, f1), (v2, f2)])
    objects.append(("Clasp", clasp_v, clasp_f))

    return objects


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    base_dir = os.path.join(os.path.dirname(__file__), '..', 'Models', '3ds')
    path = os.path.normpath(os.path.join(base_dir, 'spellbook.3ds'))
    bak  = os.path.normpath(os.path.join(base_dir, 'spellbook_orig.3ds'))

    if not os.path.exists(bak) and os.path.exists(path):
        shutil.copy2(path, bak)
        print(f"Backup  : {bak}")

    objects = build_spellbook()
    write_3ds(path, objects)


if __name__ == '__main__':
    main()
