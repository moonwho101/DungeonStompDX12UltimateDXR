"""
Generate improved stool.3DS and table.3DS models with better geometry.
Keeps roughly the same bounding boxes as originals.

Stool original: ~22x22x23 units, 5 box objects (4 legs + seat), 130 verts, 60 faces
Table original: ~75x47x35 units, 5 box objects (4 legs + top), 130 verts, 60 faces
"""
import struct
import os
import shutil
import math


def make_box(x1, y1, z1, x2, y2, z2):
    """Box with separate verts per face (flat shading). 24 verts, 12 tris."""
    verts = []
    faces = []

    # Front (y=y2)
    b = len(verts)
    verts += [(x1, y2, z1), (x2, y2, z1), (x2, y2, z2), (x1, y2, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]
    # Back (y=y1)
    b = len(verts)
    verts += [(x2, y1, z1), (x1, y1, z1), (x1, y1, z2), (x2, y1, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]
    # Right (x=x2)
    b = len(verts)
    verts += [(x2, y2, z1), (x2, y1, z1), (x2, y1, z2), (x2, y2, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]
    # Left (x=x1)
    b = len(verts)
    verts += [(x1, y1, z1), (x1, y2, z1), (x1, y2, z2), (x1, y1, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]
    # Top (z=z2)
    b = len(verts)
    verts += [(x1, y1, z2), (x1, y2, z2), (x2, y2, z2), (x2, y1, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]
    # Bottom (z=z1)
    b = len(verts)
    verts += [(x1, y2, z1), (x1, y1, z1), (x2, y1, z1), (x2, y2, z1)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]

    return verts, faces


def make_beveled_box(x1, y1, z1, x2, y2, z2, bevel):
    """Box with beveled top edges."""
    tx1, ty1 = x1 + bevel, y1 + bevel
    tx2, ty2 = x2 - bevel, y2 - bevel

    verts = []
    faces = []

    # Front (trapezoid)
    b = len(verts)
    verts += [(x1, y2, z1), (x2, y2, z1), (tx2, ty2, z2), (tx1, ty2, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]
    # Back
    b = len(verts)
    verts += [(x2, y1, z1), (x1, y1, z1), (tx1, ty1, z2), (tx2, ty1, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]
    # Right
    b = len(verts)
    verts += [(x2, y2, z1), (x2, y1, z1), (tx2, ty1, z2), (tx2, ty2, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]
    # Left
    b = len(verts)
    verts += [(x1, y1, z1), (x1, y2, z1), (tx1, ty2, z2), (tx1, ty1, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]
    # Top (inset)
    b = len(verts)
    verts += [(tx1, ty1, z2), (tx1, ty2, z2), (tx2, ty2, z2), (tx2, ty1, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]
    # Bottom
    b = len(verts)
    verts += [(x1, y2, z1), (x1, y1, z1), (x2, y1, z1), (x2, y2, z1)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]

    return verts, faces


def make_tapered_leg(x1, y1, z1, x2, y2, z2, taper):
    """Leg that tapers inward toward the bottom (z1). taper shrinks the bottom."""
    # Top is full size, bottom is inset by taper
    bx1 = x1 + taper
    by1 = y1 + taper
    bx2 = x2 - taper
    by2 = y2 - taper

    verts = []
    faces = []

    # Front (y=y2 top, y=by2 bottom)
    b = len(verts)
    verts += [(bx1, by2, z1), (bx2, by2, z1), (x2, y2, z2), (x1, y2, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]
    # Back
    b = len(verts)
    verts += [(bx2, by1, z1), (bx1, by1, z1), (x1, y1, z2), (x2, y1, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]
    # Right
    b = len(verts)
    verts += [(bx2, by2, z1), (bx2, by1, z1), (x2, y1, z2), (x2, y2, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]
    # Left
    b = len(verts)
    verts += [(bx1, by1, z1), (bx1, by2, z1), (x1, y2, z2), (x1, y1, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]
    # Top (z=z2)
    b = len(verts)
    verts += [(x1, y1, z2), (x1, y2, z2), (x2, y2, z2), (x2, y1, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]
    # Bottom (z=z1, tapered)
    b = len(verts)
    verts += [(bx1, by2, z1), (bx1, by1, z1), (bx2, by1, z1), (bx2, by2, z1)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]

    return verts, faces


def make_cylinder(cx, cy, z1, z2, radius, segments=8):
    """Vertical cylinder centered at (cx, cy). Returns (verts, faces)."""
    verts = []
    faces = []

    # Generate ring points
    top_ring = []
    bot_ring = []
    for i in range(segments):
        angle = 2 * math.pi * i / segments
        x = cx + radius * math.cos(angle)
        y = cy + radius * math.sin(angle)
        top_ring.append((x, y, z2))
        bot_ring.append((x, y, z1))

    # Side faces (quads as 2 triangles each)
    for i in range(segments):
        j = (i + 1) % segments
        b = len(verts)
        verts += [bot_ring[i], bot_ring[j], top_ring[j], top_ring[i]]
        faces += [(b, b+1, b+2), (b, b+2, b+3)]

    # Top cap (fan)
    tc = len(verts)
    verts.append((cx, cy, z2))
    for i in range(segments):
        verts.append(top_ring[i])
    for i in range(segments):
        j = (i + 1) % segments
        faces.append((tc, tc + 1 + i, tc + 1 + j))

    # Bottom cap (fan, reversed winding)
    bc = len(verts)
    verts.append((cx, cy, z1))
    for i in range(segments):
        verts.append(bot_ring[i])
    for i in range(segments):
        j = (i + 1) % segments
        faces.append((bc, bc + 1 + j, bc + 1 + i))

    return verts, faces


def make_tapered_cylinder(cx, cy, z1, z2, r_bottom, r_top, segments=8):
    """Vertical tapered cylinder. Returns (verts, faces)."""
    verts = []
    faces = []

    top_ring = []
    bot_ring = []
    for i in range(segments):
        angle = 2 * math.pi * i / segments
        cos_a = math.cos(angle)
        sin_a = math.sin(angle)
        top_ring.append((cx + r_top * cos_a, cy + r_top * sin_a, z2))
        bot_ring.append((cx + r_bottom * cos_a, cy + r_bottom * sin_a, z1))

    # Sides
    for i in range(segments):
        j = (i + 1) % segments
        b = len(verts)
        verts += [bot_ring[i], bot_ring[j], top_ring[j], top_ring[i]]
        faces += [(b, b+1, b+2), (b, b+2, b+3)]

    # Top cap
    tc = len(verts)
    verts.append((cx, cy, z2))
    for i in range(segments):
        verts.append(top_ring[i])
    for i in range(segments):
        j = (i + 1) % segments
        faces.append((tc, tc + 1 + i, tc + 1 + j))

    # Bottom cap
    bc = len(verts)
    verts.append((cx, cy, z1))
    for i in range(segments):
        verts.append(bot_ring[i])
    for i in range(segments):
        j = (i + 1) % segments
        faces.append((bc, bc + 1 + j, bc + 1 + i))

    return verts, faces


def combine_meshes(mesh_list):
    all_verts = []
    all_faces = []
    offset = 0
    for verts, fcs in mesh_list:
        all_verts.extend(verts)
        for f in fcs:
            all_faces.append((f[0] + offset, f[1] + offset, f[2] + offset))
        offset += len(verts)
    return all_verts, all_faces


def write_3ds(filepath, object_name, vertices, faces):
    # Vertex list chunk (0x4110)
    vert_data = struct.pack('<H', len(vertices))
    for v in vertices:
        vert_data += struct.pack('<fff', v[0], v[1], v[2])
    vert_chunk = struct.pack('<HI', 0x4110, 6 + len(vert_data)) + vert_data

    # Face list chunk (0x4120)
    face_data = struct.pack('<H', len(faces))
    for f in faces:
        face_data += struct.pack('<HHHH', f[0], f[1], f[2], 0x0007)
    face_chunk = struct.pack('<HI', 0x4120, 6 + len(face_data)) + face_data

    # UV mapping chunk (0x4140)
    uv_data = struct.pack('<H', len(vertices))
    for _ in vertices:
        uv_data += struct.pack('<ff', 0.0, 0.0)
    uv_chunk = struct.pack('<HI', 0x4140, 6 + len(uv_data)) + uv_data

    # Triangle mesh chunk (0x4100)
    mesh_payload = vert_chunk + face_chunk + uv_chunk
    mesh_chunk = struct.pack('<HI', 0x4100, 6 + len(mesh_payload)) + mesh_payload

    # Named object chunk (0x4000)
    name_bytes = object_name.encode('ascii') + b'\x00'
    obj_payload = name_bytes + mesh_chunk
    obj_chunk = struct.pack('<HI', 0x4000, 6 + len(obj_payload)) + obj_payload

    # Version chunk
    ver_chunk = struct.pack('<HI', 0x0002, 10) + struct.pack('<I', 3)

    # Edit chunk (0x3D3D)
    edit_payload = obj_chunk
    edit_chunk = struct.pack('<HI', 0x3D3D, 6 + len(edit_payload)) + edit_payload

    # Main chunk (0x4D4D)
    main_payload = ver_chunk + edit_chunk
    main_chunk = struct.pack('<HI', 0x4D4D, 6 + len(main_payload)) + main_payload

    with open(filepath, 'wb') as f:
        f.write(main_chunk)

    print(f"  Written: {filepath}")
    print(f"  {len(vertices)} vertices, {len(faces)} faces, {len(main_chunk)} bytes")


def backup_file(filepath):
    base, ext = os.path.splitext(filepath)
    bak = base + "_old" + ext
    if not os.path.exists(bak):
        shutil.copy2(filepath, bak)
        print(f"  Backed up to {bak}")


# ============================================================
# STOOL - Original: ~22x22x23, 4 legs + flat seat
# New: 4 turned (cylindrical) legs, cross stretchers,
#      round-ish seat with beveled edge
# ============================================================
def build_stool():
    parts = []
    segs = 8  # cylinder segments

    # Dimensions matching original ~22x22x23 bounding box
    # Z: -12 (floor) to +12 (seat top)
    # X,Y: -11 to +11

    seat_r = 10.5   # seat radius
    seat_z1 = 7.0   # seat bottom
    seat_z2 = 11.5  # seat top

    leg_r = 1.5     # leg radius
    leg_z1 = -12.0  # leg bottom (floor)
    leg_z2 = 7.0    # leg top (meets seat bottom)

    # Leg positions (slightly inset from seat edge)
    leg_offset = 7.0
    leg_positions = [
        ( leg_offset,  leg_offset),
        ( leg_offset, -leg_offset),
        (-leg_offset,  leg_offset),
        (-leg_offset, -leg_offset),
    ]

    # --- SEAT: thick cylinder with slight taper ---
    parts.append(make_tapered_cylinder(0, 0, seat_z1, seat_z2, seat_r - 0.5, seat_r, segments=12))

    # --- 4 TURNED LEGS: tapered cylinders ---
    for lx, ly in leg_positions:
        # Main leg shaft - slightly tapered (wider at top)
        parts.append(make_tapered_cylinder(lx, ly, leg_z1, leg_z2, leg_r * 0.7, leg_r, segments=segs))
        # Decorative bulge at mid-height
        mid_z = (leg_z1 + leg_z2) / 2
        parts.append(make_cylinder(lx, ly, mid_z - 1.5, mid_z + 1.5, leg_r * 1.4, segments=segs))
        # Small foot pad at bottom
        parts.append(make_cylinder(lx, ly, leg_z1, leg_z1 + 1.0, leg_r * 1.1, segments=segs))

    # --- CROSS STRETCHERS (between legs for stability) ---
    stretch_z = -5.0
    stretch_h = 0.8
    stretch_w = 0.8

    # Stretcher along X between +Y legs
    parts.append(make_box(-leg_offset, leg_offset - stretch_w, stretch_z,
                            leg_offset, leg_offset + stretch_w, stretch_z + stretch_h))
    # Stretcher along X between -Y legs
    parts.append(make_box(-leg_offset, -leg_offset - stretch_w, stretch_z,
                            leg_offset, -leg_offset + stretch_w, stretch_z + stretch_h))
    # Stretcher along Y between +X legs
    parts.append(make_box(leg_offset - stretch_w, -leg_offset, stretch_z,
                           leg_offset + stretch_w, leg_offset, stretch_z + stretch_h))
    # Stretcher along Y between -X legs
    parts.append(make_box(-leg_offset - stretch_w, -leg_offset, stretch_z,
                           -leg_offset + stretch_w, leg_offset, stretch_z + stretch_h))

    return combine_meshes(parts)


# ============================================================
# TABLE - Original: ~75x47x35, 4 legs + flat top
# New: beveled tabletop with edge lip, 4 tapered legs with
#      decorative turnings, apron rails under the top
# ============================================================
def build_table():
    parts = []
    segs = 8

    # Dimensions matching original ~75x47x35 bounding box
    # X: -37 to 37 (length)
    # Y: -23 to 23 (width)
    # Z: -17 (floor) to 18 (tabletop)

    top_z1 = 14.0   # tabletop bottom
    top_z2 = 18.0   # tabletop top
    top_x1, top_x2 = -37.0, 37.0
    top_y1, top_y2 = -23.0, 23.0

    leg_z1 = -17.0   # floor
    leg_z2 = 14.0    # meets apron

    # Leg positions (inset from edges)
    inset = 4.0
    lx1 = top_x1 + inset
    lx2 = top_x2 - inset
    ly1 = top_y1 + inset
    ly2 = top_y2 - inset

    leg_positions = [
        (lx2, ly2),  # back-right
        (lx2, ly1),  # back-left
        (lx1, ly2),  # front-right
        (lx1, ly1),  # front-left
    ]

    leg_hw = 2.2  # leg half-width

    # --- TABLETOP: beveled box ---
    parts.append(make_beveled_box(top_x1, top_y1, top_z1, top_x2, top_y2, top_z2, 1.5))

    # --- TABLETOP EDGE TRIM (thin lip around perimeter) ---
    trim_h = 1.5
    trim_t = 1.0
    # Front edge
    parts.append(make_box(top_x1 + 2, top_y2 - trim_t, top_z1 - trim_h, top_x2 - 2, top_y2, top_z1))
    # Back edge
    parts.append(make_box(top_x1 + 2, top_y1, top_z1 - trim_h, top_x2 - 2, top_y1 + trim_t, top_z1))
    # Right edge
    parts.append(make_box(top_x2 - trim_t, top_y1 + 2, top_z1 - trim_h, top_x2, top_y2 - 2, top_z1))
    # Left edge
    parts.append(make_box(top_x1, top_y1 + 2, top_z1 - trim_h, top_x1 + trim_t, top_y2 - 2, top_z1))

    # --- APRON (horizontal rails connecting legs under tabletop) ---
    apron_z1 = 9.0
    apron_z2 = 14.0
    apron_t = 1.5  # apron thickness

    # Front apron (+Y)
    parts.append(make_box(lx1 + leg_hw, ly2 - apron_t, apron_z1, lx2 - leg_hw, ly2, apron_z2))
    # Back apron (-Y)
    parts.append(make_box(lx1 + leg_hw, ly1, apron_z1, lx2 - leg_hw, ly1 + apron_t, apron_z2))
    # Right apron (+X)
    parts.append(make_box(lx2 - apron_t, ly1 + leg_hw, apron_z1, lx2, ly2 - leg_hw, apron_z2))
    # Left apron (-X)
    parts.append(make_box(lx1, ly1 + leg_hw, apron_z1, lx1 + apron_t, ly2 - leg_hw, apron_z2))

    # --- 4 LEGS: tapered with decorative turnings ---
    for lx, ly in leg_positions:
        # Main tapered leg
        parts.append(make_tapered_leg(
            lx - leg_hw, ly - leg_hw, leg_z1,
            lx + leg_hw, ly + leg_hw, leg_z2,
            taper=0.6
        ))

        # Upper decorative block (just below apron)
        parts.append(make_box(
            lx - leg_hw - 0.3, ly - leg_hw - 0.3, apron_z1 - 1.5,
            lx + leg_hw + 0.3, ly + leg_hw + 0.3, apron_z1
        ))

        # Mid-height turned bulge (cylinder)
        mid_z = (leg_z1 + leg_z2) / 2
        parts.append(make_cylinder(lx, ly, mid_z - 2, mid_z + 2, leg_hw * 1.3, segments=segs))

        # Lower turned detail
        low_z = leg_z1 + (leg_z2 - leg_z1) * 0.25
        parts.append(make_cylinder(lx, ly, low_z - 1, low_z + 1, leg_hw * 1.15, segments=segs))

        # Foot pad
        parts.append(make_box(
            lx - leg_hw * 0.8, ly - leg_hw * 0.8, leg_z1,
            lx + leg_hw * 0.8, ly + leg_hw * 0.8, leg_z1 + 0.8
        ))

    # --- LOWER STRETCHERS (H-frame between legs) ---
    stretch_z = -6.0
    stretch_h = 1.2
    stretch_w = 0.8

    # Long stretchers along X
    parts.append(make_box(lx1, ly2 - stretch_w, stretch_z, lx2, ly2 + stretch_w, stretch_z + stretch_h))
    parts.append(make_box(lx1, ly1 - stretch_w, stretch_z, lx2, ly1 + stretch_w, stretch_z + stretch_h))

    # Cross stretcher along Y (center)
    parts.append(make_box(-stretch_w, ly1, stretch_z, stretch_w, ly2, stretch_z + stretch_h))

    return combine_meshes(parts)


def main():
    base = r'c:\github\DungeonStompDX12UltimateDXR\Models\3ds'

    # --- STOOL ---
    print("=== STOOL ===")
    stool_path = os.path.join(base, 'stool.3DS')
    backup_file(stool_path)
    verts, faces = build_stool()
    print(f"  Model: {len(verts)} vertices, {len(faces)} faces")
    # Validate
    for i, f in enumerate(faces):
        for vi in f:
            assert 0 <= vi < len(verts), f"Stool face {i} bad index {vi}"
    write_3ds(stool_path, "Stool", verts, faces)

    # --- TABLE ---
    print("\n=== TABLE ===")
    table_path = os.path.join(base, 'table.3DS')
    backup_file(table_path)
    verts, faces = build_table()
    print(f"  Model: {len(verts)} vertices, {len(faces)} faces")
    for i, f in enumerate(faces):
        for vi in f:
            assert 0 <= vi < len(verts), f"Table face {i} bad index {vi}"
    write_3ds(table_path, "Table", verts, faces)


if __name__ == '__main__':
    main()
