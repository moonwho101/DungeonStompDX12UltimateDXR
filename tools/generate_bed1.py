"""
Generate an improved bed.3DS model with detailed geometry.
Keeps roughly the same bounding box as the original (~87x48x43 units).
Components: 4 corner posts, headboard panel, footboard panel,
2 side rails, bed platform, mattress (beveled), pillow (beveled).
"""
import struct
import os
import shutil


def orient_faces_outward(vertices, faces):
    """Flip triangles so normals point away from the mesh center."""
    if not vertices:
        return vertices, faces

    center = (
        sum(v[0] for v in vertices) / len(vertices),
        sum(v[1] for v in vertices) / len(vertices),
        sum(v[2] for v in vertices) / len(vertices),
    )

    oriented_faces = []
    for a, b, c in faces:
        va, vb, vc = vertices[a], vertices[b], vertices[c]
        ab = (vb[0] - va[0], vb[1] - va[1], vb[2] - va[2])
        ac = (vc[0] - va[0], vc[1] - va[1], vc[2] - va[2])
        normal = (
            ab[1] * ac[2] - ab[2] * ac[1],
            ab[2] * ac[0] - ab[0] * ac[2],
            ab[0] * ac[1] - ab[1] * ac[0],
        )
        centroid = (
            (va[0] + vb[0] + vc[0]) / 3.0,
            (va[1] + vb[1] + vc[1]) / 3.0,
            (va[2] + vb[2] + vc[2]) / 3.0,
        )
        outward = (
            centroid[0] - center[0],
            centroid[1] - center[1],
            centroid[2] - center[2],
        )
        facing = (
            normal[0] * outward[0]
            + normal[1] * outward[1]
            + normal[2] * outward[2]
        )
        if facing < 0.0:
            oriented_faces.append((a, c, b))
        else:
            oriented_faces.append((a, b, c))

    return vertices, oriented_faces


def validate_faces_outward(vertices, faces):
    """Raise if any triangle normal points toward the mesh center."""
    if not vertices:
        return

    center = (
        sum(v[0] for v in vertices) / len(vertices),
        sum(v[1] for v in vertices) / len(vertices),
        sum(v[2] for v in vertices) / len(vertices),
    )

    inward_faces = []
    for index, (a, b, c) in enumerate(faces):
        va, vb, vc = vertices[a], vertices[b], vertices[c]
        ab = (vb[0] - va[0], vb[1] - va[1], vb[2] - va[2])
        ac = (vc[0] - va[0], vc[1] - va[1], vc[2] - va[2])
        normal = (
            ab[1] * ac[2] - ab[2] * ac[1],
            ab[2] * ac[0] - ab[0] * ac[2],
            ab[0] * ac[1] - ab[1] * ac[0],
        )
        centroid = (
            (va[0] + vb[0] + vc[0]) / 3.0,
            (va[1] + vb[1] + vc[1]) / 3.0,
            (va[2] + vb[2] + vc[2]) / 3.0,
        )
        outward = (
            centroid[0] - center[0],
            centroid[1] - center[1],
            centroid[2] - center[2],
        )
        facing = (
            normal[0] * outward[0]
            + normal[1] * outward[1]
            + normal[2] * outward[2]
        )
        if facing < 0.0:
            inward_faces.append(index)

    if inward_faces:
        raise ValueError(f"Found inward-facing triangles: {inward_faces[:10]}")


def make_box(x1, y1, z1, x2, y2, z2):
    """Create a box with separate vertices per face (flat shading).
    Returns (vertices, faces). 24 vertices, 12 triangles."""
    verts = []
    faces = []

    # Front face (y = y2)
    b = len(verts)
    verts += [(x1, y2, z1), (x2, y2, z1), (x2, y2, z2), (x1, y2, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]

    # Back face (y = y1)
    b = len(verts)
    verts += [(x2, y1, z1), (x1, y1, z1), (x1, y1, z2), (x2, y1, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]

    # Right face (x = x2)
    b = len(verts)
    verts += [(x2, y2, z1), (x2, y1, z1), (x2, y1, z2), (x2, y2, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]

    # Left face (x = x1)
    b = len(verts)
    verts += [(x1, y1, z1), (x1, y2, z1), (x1, y2, z2), (x1, y1, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]

    # Top face (z = z2)
    b = len(verts)
    verts += [(x1, y1, z2), (x1, y2, z2), (x2, y2, z2), (x2, y1, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]

    # Bottom face (z = z1)
    b = len(verts)
    verts += [(x1, y2, z1), (x1, y1, z1), (x2, y1, z1), (x2, y2, z1)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]

    verts, faces = orient_faces_outward(verts, faces)
    validate_faces_outward(verts, faces)
    return verts, faces


def make_beveled_box(x1, y1, z1, x2, y2, z2, bevel):
    """Box with beveled top edges. Top face inset by 'bevel' on X and Y.
    Returns (vertices, faces). 24 vertices, 12 triangles."""
    tx1, ty1 = x1 + bevel, y1 + bevel
    tx2, ty2 = x2 - bevel, y2 - bevel

    verts = []
    faces = []

    # Front face (trapezoid)
    b = len(verts)
    verts += [(x1, y2, z1), (x2, y2, z1), (tx2, ty2, z2), (tx1, ty2, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]

    # Back face (trapezoid)
    b = len(verts)
    verts += [(x2, y1, z1), (x1, y1, z1), (tx1, ty1, z2), (tx2, ty1, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]

    # Right face (trapezoid)
    b = len(verts)
    verts += [(x2, y2, z1), (x2, y1, z1), (tx2, ty1, z2), (tx2, ty2, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]

    # Left face (trapezoid)
    b = len(verts)
    verts += [(x1, y1, z1), (x1, y2, z1), (tx1, ty2, z2), (tx1, ty1, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]

    # Top face (inset)
    b = len(verts)
    verts += [(tx1, ty1, z2), (tx1, ty2, z2), (tx2, ty2, z2), (tx2, ty1, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]

    # Bottom face
    b = len(verts)
    verts += [(x1, y2, z1), (x1, y1, z1), (x2, y1, z1), (x2, y2, z1)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]

    verts, faces = orient_faces_outward(verts, faces)
    validate_faces_outward(verts, faces)
    return verts, faces


def make_headboard_arch(x1, y1, x2, y2, z_base, z_top, thickness, segments=8):
    """Create a headboard panel with an arched top.
    The arch runs along Y between y1 and y2.
    x1/x2 define thickness, z_base is bottom, z_top is peak of arch.
    Returns (vertices, faces)."""
    import math

    verts = []
    faces = []

    # Create arch profile points along Y
    # Bottom edge: straight line at z_base
    # Top edge: arch from z_straight to z_top
    z_straight = z_base + (z_top - z_base) * 0.6  # straight portion goes 60% up
    arch_height = z_top - z_straight

    points_y = []
    points_z_top = []
    n = segments + 1
    for i in range(n):
        t = i / segments
        py = y1 + t * (y2 - y1)
        # Arch: semicircle from y1 to y2
        angle = t * math.pi
        pz = z_straight + arch_height * math.sin(angle)
        points_y.append(py)
        points_z_top.append(pz)

    # Build front and back faces as triangle strips
    # Front face (x = x2)
    for i in range(segments):
        b = len(verts)
        # Quad: bottom-left, bottom-right, top-right, top-left
        verts += [
            (x2, points_y[i],   z_base),
            (x2, points_y[i+1], z_base),
            (x2, points_y[i+1], points_z_top[i+1]),
            (x2, points_y[i],   points_z_top[i]),
        ]
        faces += [(b, b+1, b+2), (b, b+2, b+3)]

    # Back face (x = x1)
    for i in range(segments):
        b = len(verts)
        verts += [
            (x1, points_y[i+1], z_base),
            (x1, points_y[i],   z_base),
            (x1, points_y[i],   points_z_top[i]),
            (x1, points_y[i+1], points_z_top[i+1]),
        ]
        faces += [(b, b+1, b+2), (b, b+2, b+3)]

    # Top edge (arch surface between front and back)
    for i in range(segments):
        b = len(verts)
        verts += [
            (x1, points_y[i],   points_z_top[i]),
            (x2, points_y[i],   points_z_top[i]),
            (x2, points_y[i+1], points_z_top[i+1]),
            (x1, points_y[i+1], points_z_top[i+1]),
        ]
        faces += [(b, b+1, b+2), (b, b+2, b+3)]

    # Bottom face
    b = len(verts)
    verts += [
        (x1, y2, z_base), (x1, y1, z_base),
        (x2, y1, z_base), (x2, y2, z_base),
    ]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]

    # Left side (y = y1)
    b = len(verts)
    verts += [
        (x2, y1, z_base), (x1, y1, z_base),
        (x1, y1, points_z_top[0]), (x2, y1, points_z_top[0]),
    ]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]

    # Right side (y = y2)
    b = len(verts)
    verts += [
        (x1, y2, z_base), (x2, y2, z_base),
        (x2, y2, points_z_top[-1]), (x1, y2, points_z_top[-1]),
    ]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]

    verts, faces = orient_faces_outward(verts, faces)
    validate_faces_outward(verts, faces)
    return verts, faces


def combine_meshes(mesh_list):
    """Combine multiple (vertices, faces) into one mesh."""
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
    """Write a .3DS file with a single mesh object."""

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

    # UV mapping chunk (0x4140) - default zero UVs
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

    print(f"Written: {filepath}")
    print(f"  {len(vertices)} vertices, {len(faces)} faces, {len(main_chunk)} bytes")


def build_bed():
    """Build all bed geometry components."""
    parts = []

    # === DIMENSIONS (matching original ~87x48x43 bounding box) ===
    # X: length of bed (-42 to 42)
    # Y: width of bed (-23 to 23)
    # Z: height (floor=-20, top of headboard=22)

    post_w = 3.0    # half-width of corner posts
    rail_h = 4.0    # rail height
    rail_t = 2.0    # rail thickness

    # --- 4 CORNER POSTS ---
    # Back-left post (headboard side, +X, +Y) - tall
    parts.append(make_box(38, 20, -20, 42, 24, 8))
    # Back-right post (headboard side, +X, -Y) - tall
    parts.append(make_box(38, -24, -20, 42, -20, 8))
    # Front-left post (footboard side, -X, +Y) - shorter
    parts.append(make_box(-42, 20, -20, -38, 24, -2))
    # Front-right post (footboard side, -X, -Y) - shorter
    parts.append(make_box(-42, -24, -20, -38, -20, -2))

    # --- Post caps (decorative finials on top of each post) ---
    # Back posts - small pyramidal caps
    parts.append(make_beveled_box(37, 19, 8, 43, 25, 12, 2.0))
    parts.append(make_beveled_box(37, -25, 8, 43, -19, 12, 2.0))
    # Front posts - smaller caps
    parts.append(make_beveled_box(- 43, 19, -2, -37, 25, 1, 1.5))
    parts.append(make_beveled_box(-43, -25, -2, -37, -19, 1, 1.5))

    # --- HEADBOARD (arched panel between back posts) ---
    parts.append(make_headboard_arch(
        x1=38, y1=-20, x2=42, y2=20,
        z_base=-10, z_top=22, thickness=4, segments=10
    ))

    # --- FOOTBOARD (solid panel between front posts) ---
    parts.append(make_box(-42, -20, -10, -38, 20, -3))

    # --- SIDE RAILS (connecting headboard to footboard) ---
    # Left rail (+Y side)
    parts.append(make_box(-38, 20, -12, 38, 24, -7))
    # Right rail (-Y side)
    parts.append(make_box(-38, -24, -12, 38, -20, -7))

    # --- LOWER SIDE RAILS (decorative, thinner) ---
    parts.append(make_box(-38, 21, -18, 38, 23, -14))
    parts.append(make_box(-38, -23, -18, 38, -21, -14))

    # --- BED PLATFORM / SLAT SUPPORT ---
    # Main platform
    parts.append(make_box(-36, -20, -9, 36, 20, -7))

    # Cross slats (3 slats for detail)
    for sx in [-24, 0, 24]:
        parts.append(make_box(sx - 1.5, -20, -11, sx + 1.5, 20, -9))

    # --- MATTRESS (beveled for puffy look) ---
    parts.append(make_beveled_box(-34, -18, -7, 34, 18, 2, 2.5))

    # --- PILLOW (beveled, near headboard) ---
    parts.append(make_beveled_box(18, -12, 2, 32, 12, 7, 2.0))

    # --- BLANKET FOLD (thin box draped over foot end of mattress) ---
    parts.append(make_box(-34, -18, 1.5, -20, 18, 3))

    return combine_meshes(parts)


def main():
    src = r'c:\github\DungeonStompDX12UltimateDXR\Models\3ds\bed.3DS'
    bak = r'c:\github\DungeonStompDX12UltimateDXR\Models\3ds\bed_old.3DS'

    # Backup original
    if not os.path.exists(bak):
        shutil.copy2(src, bak)
        print(f"Backed up original to {bak}")

    # Build and write
    verts, faces = build_bed()
    print(f"Bed model: {len(verts)} vertices, {len(faces)} faces")

    # Validate
    max_idx = len(verts) - 1
    for i, f in enumerate(faces):
        for vi in f:
            assert 0 <= vi <= max_idx, f"Face {i} has invalid vertex index {vi} (max {max_idx})"
    write_3ds(src, "Bed", verts, faces)


if __name__ == '__main__':
    main()
