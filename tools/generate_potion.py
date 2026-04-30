import struct
import math
import os
import shutil

def orient_faces_outward(vertices, faces):
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

def make_frustum(r_bottom, r_top, z_bottom, z_top, uv_scale=(1.0, 1.0), segments=16):
    verts = []
    faces = []
    uvs = []
    
    points_b = []
    points_t = []
    for i in range(segments):
        angle = 2.0 * math.pi * i / segments
        cx = math.cos(angle)
        cy = math.sin(angle)
        points_b.append((r_bottom * cx, r_bottom * cy, z_bottom))
        points_t.append((r_top * cx, r_top * cy, z_top))
        
    for i in range(segments):
        ni = (i + 1) % segments
        b = len(verts)
        verts += [points_b[i], points_b[ni], points_t[ni], points_t[i]]
        
        u0 = float(i) / segments * uv_scale[0]
        u1 = float(i + 1) / segments * uv_scale[0]
        v0 = 1.0 * uv_scale[1]
        v1 = 0.0 * uv_scale[1]
        uvs += [(u0, v0), (u1, v0), (u1, v1), (u0, v1)]
        
        faces += [(b, b+1, b+2), (b, b+2, b+3)]
        
    b = len(verts)
    verts.append((0, 0, z_bottom))
    uvs.append((0.5 * uv_scale[0], 0.5 * uv_scale[1]))
    center_b = b
    for i in range(segments):
        verts.append(points_b[i])
        angle = 2.0 * math.pi * i / segments
        u = (0.5 + 0.5 * math.cos(angle)) * uv_scale[0]
        v = (0.5 + 0.5 * math.sin(angle)) * uv_scale[1]
        uvs.append((u, v))
    for i in range(segments):
        ni = (i + 1) % segments
        faces.append((center_b, center_b + 1 + ni, center_b + 1 + i))
        
    b = len(verts)
    verts.append((0, 0, z_top))
    uvs.append((0.5 * uv_scale[0], 0.5 * uv_scale[1]))
    center_t = b
    for i in range(segments):
        verts.append(points_t[i])
        angle = 2.0 * math.pi * i / segments
        u = (0.5 + 0.5 * math.cos(angle)) * uv_scale[0]
        v = (0.5 + 0.5 * math.sin(angle)) * uv_scale[1]
        uvs.append((u, v))
    for i in range(segments):
        ni = (i + 1) % segments
        faces.append((center_t, center_t + 1 + i, center_t + 1 + ni))
        
    verts, faces = orient_faces_outward(verts, faces)
    return verts, faces, uvs

def combine_meshes(mesh_list):
    all_verts = []
    all_faces = []
    all_uvs = []
    offset = 0
    for verts, fcs, uvs in mesh_list:
        all_verts.extend(verts)
        all_uvs.extend(uvs)
        for f in fcs:
            all_faces.append((f[0] + offset, f[1] + offset, f[2] + offset))
        offset += len(verts)
    return all_verts, all_faces, all_uvs

def write_3ds(filepath, object_name, vertices, faces, uvs):
    vert_data = struct.pack('<H', len(vertices))
    for v in vertices:
        vert_data += struct.pack('<fff', v[0], v[1], v[2])
    vert_chunk = struct.pack('<HI', 0x4110, 6 + len(vert_data)) + vert_data

    face_data = struct.pack('<H', len(faces))
    for f in faces:
        face_data += struct.pack('<HHHH', f[0], f[1], f[2], 0x0007)
    face_chunk = struct.pack('<HI', 0x4120, 6 + len(face_data)) + face_data

    uv_data = struct.pack('<H', len(uvs))
    for uv in uvs:
        uv_data += struct.pack('<ff', uv[0], uv[1])
    uv_chunk = struct.pack('<HI', 0x4140, 6 + len(uv_data)) + uv_data

    mesh_payload = vert_chunk + face_chunk + uv_chunk
    mesh_chunk = struct.pack('<HI', 0x4100, 6 + len(mesh_payload)) + mesh_payload

    name_bytes = object_name.encode('ascii') + b'\x00'
    obj_payload = name_bytes + mesh_chunk
    obj_chunk = struct.pack('<HI', 0x4000, 6 + len(obj_payload)) + obj_payload

    ver_chunk = struct.pack('<HI', 0x0002, 10) + struct.pack('<I', 3)

    edit_payload = obj_chunk
    edit_chunk = struct.pack('<HI', 0x3D3D, 6 + len(edit_payload)) + edit_payload

    main_payload = ver_chunk + edit_chunk
    main_chunk = struct.pack('<HI', 0x4D4D, 6 + len(main_payload)) + main_payload

    with open(filepath, 'wb') as f:
        f.write(main_chunk)

    print(f"Written: {filepath}")

def build_potion():
    parts = []
    
    parts.append(make_frustum(4.0, 9.5, -9.5, -5.0, uv_scale=(1.0, 1.0), segments=16))
    parts.append(make_frustum(9.5, 9.5, -5.0, -2.0, uv_scale=(1.0, 1.0), segments=16))
    parts.append(make_frustum(9.5, 3.5, -2.0, 6.0, uv_scale=(1.0, 1.0), segments=16))
    parts.append(make_frustum(3.5, 3.0, 6.0, 14.0, uv_scale=(1.0, 1.0), segments=16))
    parts.append(make_frustum(4.5, 4.5, 14.0, 15.5, uv_scale=(1.0, 1.0), segments=16))
    parts.append(make_frustum(2.5, 3.0, 15.5, 18.0, uv_scale=(1.0, 1.0), segments=10))
    parts.append(make_frustum(9.8, 9.8, -4.0, -3.0, uv_scale=(1.0, 1.0), segments=16))
    
    return combine_meshes(parts)

def main():
    src = r'c:\github\DungeonStompDX12UltimateDXR\models\3ds\potion.3DS'
    verts, faces, uvs = build_potion()
    print(f"Potion model: {len(verts)} vertices, {len(faces)} faces, {len(uvs)} uvs")
    write_3ds(src, "Potion", verts, faces, uvs)

if __name__ == '__main__':
    main()
