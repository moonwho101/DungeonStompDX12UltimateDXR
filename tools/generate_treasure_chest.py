import struct
import math
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

def make_box(x1, y1, z1, x2, y2, z2):
    """Create a box with separate vertices per face (flat shading)."""
    verts = []
    faces = []

    b = len(verts)
    verts += [(x1, y2, z1), (x2, y2, z1), (x2, y2, z2), (x1, y2, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]

    b = len(verts)
    verts += [(x2, y1, z1), (x1, y1, z1), (x1, y1, z2), (x2, y1, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]

    b = len(verts)
    verts += [(x2, y2, z1), (x2, y1, z1), (x2, y1, z2), (x2, y2, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]

    b = len(verts)
    verts += [(x1, y1, z1), (x1, y2, z1), (x1, y2, z2), (x1, y1, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]

    b = len(verts)
    verts += [(x1, y1, z2), (x1, y2, z2), (x2, y2, z2), (x2, y1, z2)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]

    b = len(verts)
    verts += [(x1, y2, z1), (x1, y1, z1), (x2, y1, z1), (x2, y2, z1)]
    faces += [(b, b+1, b+2), (b, b+2, b+3)]

    return orient_faces_outward(verts, faces)

def rotate_mesh(verts, faces, angle_deg, cx, cy, cz, axis='y'):
    """Rotate a mesh around a given center point and axis."""
    rad = math.radians(angle_deg)
    cos_a = math.cos(rad)
    sin_a = math.sin(rad)
    new_verts = []
    for x, y, z in verts:
        dx = x - cx
        dy = y - cy
        dz = z - cz
        if axis == 'y':
            nx = dx * cos_a - dz * sin_a
            ny = dy
            nz = dx * sin_a + dz * cos_a
        elif axis == 'x':
            nx = dx
            ny = dy * cos_a - dz * sin_a
            nz = dy * sin_a + dz * cos_a
        elif axis == 'z':
            nx = dx * cos_a - dy * sin_a
            ny = dx * sin_a + dy * cos_a
            nz = dz
        new_verts.append((nx + cx, ny + cy, nz + cz))
    return new_verts, faces

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

def write_3ds(filepath, objects):
    """Write a .3DS file with multiple mesh objects."""
    edit_payload = b''
    total_verts = 0
    total_faces = 0
    for obj_name, vertices, faces in objects:
        vert_data = struct.pack('<H', len(vertices))
        for v in vertices:
            vert_data += struct.pack('<fff', v[0], v[1], v[2])
        vert_chunk = struct.pack('<HI', 0x4110, 6 + len(vert_data)) + vert_data

        face_data = struct.pack('<H', len(faces))
        for f in faces:
            face_data += struct.pack('<HHHH', f[0], f[1], f[2], 0x0007)
        face_chunk = struct.pack('<HI', 0x4120, 6 + len(face_data)) + face_data

        uv_data = struct.pack('<H', len(vertices))
        for _ in vertices:
            uv_data += struct.pack('<ff', 0.0, 0.0)
        uv_chunk = struct.pack('<HI', 0x4140, 6 + len(uv_data)) + uv_data

        mesh_payload = vert_chunk + face_chunk + uv_chunk
        mesh_chunk = struct.pack('<HI', 0x4100, 6 + len(mesh_payload)) + mesh_payload

        name_bytes = obj_name.encode('ascii') + b'\x00'
        obj_payload = name_bytes + mesh_chunk
        obj_chunk = struct.pack('<HI', 0x4000, 6 + len(obj_payload)) + obj_payload
        edit_payload += obj_chunk
        
        total_verts += len(vertices)
        total_faces += len(faces)

    ver_chunk = struct.pack('<HI', 0x0002, 10) + struct.pack('<I', 3)

    edit_chunk = struct.pack('<HI', 0x3D3D, 6 + len(edit_payload)) + edit_payload

    main_payload = ver_chunk + edit_chunk
    main_chunk = struct.pack('<HI', 0x4D4D, 6 + len(main_payload)) + main_payload

    with open(filepath, 'wb') as f:
        f.write(main_chunk)
    print(f"Written: {filepath} ({len(objects)} objects, {total_verts} verts, {total_faces} faces)")

def build_chest(is_open=False):
    base_parts = []
    lock_parts = []
    
    # 1. Base Hollow Box
    # Floor
    base_parts.append(make_box(-15, -24, -20, 15, 24, -18))
    # Front
    base_parts.append(make_box(-15, -24, -18, -13, 24, 5))
    # Back
    base_parts.append(make_box(13, -24, -18, 15, 24, 5))
    # Left (+Y)
    base_parts.append(make_box(-13, 22, -18, 13, 24, 5))
    # Right (-Y)
    base_parts.append(make_box(-13, -24, -18, 13, -22, 5))
    
    # Iron bands on Base
    # base_parts.append(make_box(-16, 18, -21, 16, 20, 6)) # Left
    # base_parts.append(make_box(-16, -20, -21, 16, -18, 6)) # Right
    # base_parts.append(make_box(-16, -1, -21, 16, 1, 6)) # Center

    # Lock base (front)
    lock_parts.append(make_box(-17, -3, 0, -15, 3, 5))
    
    lid_parts = []
    lock_lid_parts = []
    
    # 2. Lid (Curved Top)
    # We build it at the top (angle 90) and rotate into 6 segments
    plank_w = 4.2
    for i in range(6):
        angle = 165 - i * 30
        rot = angle - 90
        
        # Wood plank
        v, f = make_box(-plank_w, -24, 19, plank_w, 24, 21)
        v, f = rotate_mesh(v, f, rot, 0, 0, 5)
        lid_parts.append((v, f))
        
        # Left cap (+Y)
        v, f = make_box(-plank_w, 22, 5, plank_w, 24, 19)
        v, f = rotate_mesh(v, f, rot, 0, 0, 5)
        lid_parts.append((v, f))
        
        # Right cap (-Y)
        v, f = make_box(-plank_w, -24, 5, plank_w, -22, 19)
        v, f = rotate_mesh(v, f, rot, 0, 0, 5)
        lid_parts.append((v, f))
        
        # Iron bands on Lid
        # Left band
        v, f = make_box(-plank_w-0.1, 18, 20.5, plank_w+0.1, 20, 22)
        v, f = rotate_mesh(v, f, rot, 0, 0, 5)
        lid_parts.append((v, f))
        
        # Right band
        v, f = make_box(-plank_w-0.1, -20, 20.5, plank_w+0.1, -18, 22)
        v, f = rotate_mesh(v, f, rot, 0, 0, 5)
        lid_parts.append((v, f))
        
        # Center band
        v, f = make_box(-plank_w-0.1, -1, 20.5, plank_w+0.1, 1, 22)
        v, f = rotate_mesh(v, f, rot, 0, 0, 5)
        lid_parts.append((v, f))
        
    # Lock top part (lid side)
    lock_lid_parts.append(make_box(-17, -2, 5, -15, 2, 8))
    # Lock keyhole
    lock_lid_parts.append(make_box(-18, -0.5, 2, -17, 0.5, 4))
    
    if is_open:
        # Rotate lid by -100 degrees around hinge at (X=14, Z=5)
        open_lid_parts = []
        for v, f in lid_parts:
            open_lid_parts.append(rotate_mesh(v, f, -100, 14, 0, 5))
        lid_parts = open_lid_parts
        
        open_lock_lid_parts = []
        for v, f in lock_lid_parts:
            open_lock_lid_parts.append(rotate_mesh(v, f, -100, 14, 0, 5))
        lock_lid_parts = open_lock_lid_parts
        
    chest_verts, chest_faces = combine_meshes(base_parts + lid_parts)
    lock_verts, lock_faces = combine_meshes(lock_parts + lock_lid_parts)
    
    return [("ChestC" if not is_open else "ChestO", chest_verts, chest_faces),
            ("LockC" if not is_open else "LockO", lock_verts, lock_faces)]

def main():
    closed_path = r'c:\github\DungeonStompDX12UltimateDXR\models\3ds\WoodenBox\cdoorclosedwoodbox.3ds'
    open_path = r'c:\github\DungeonStompDX12UltimateDXR\models\3ds\WoodenBox\cdooropenwoodbox.3DS'
    
    # Backup original files if they don't have .bak
    for p in [closed_path, open_path]:
        bak = p + '.bak'
        if not os.path.exists(bak) and os.path.exists(p):
            shutil.copy2(p, bak)
            print(f"Backed up {p}")
            
    closed_objects = build_chest(is_open=False)
    write_3ds(closed_path, closed_objects)
    
    open_objects = build_chest(is_open=True)
    write_3ds(open_path, open_objects)

if __name__ == '__main__':
    main()
