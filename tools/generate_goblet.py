import struct
import math
import os
from PIL import Image, ImageDraw, ImageFilter

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


def make_revolved_surface(profile_points, segments=24, u_range=(0.0, 1.0), v_range=(0.0, 1.0)):
    """
    Revolves a 2D profile curve (r, z) around Z-axis to create a 3D surface.
    profile_points: list of (r, z) tuples from bottom to top.
    Returns (verts, faces, uvs).
    """
    verts = []
    faces = []
    uvs = []

    n_profile = len(profile_points)

    # Generate grid of vertices and UVs
    for i in range(n_profile):
        r, z = profile_points[i]
        v_coord = v_range[0] + (v_range[1] - v_range[0]) * (float(i) / max(1, n_profile - 1))

        for j in range(segments):
            angle = 2.0 * math.pi * j / segments
            x = r * math.cos(angle)
            y = r * math.sin(angle)
            u_coord = u_range[0] + (u_range[1] - u_range[0]) * (float(j) / segments)

            verts.append((x, y, z))
            uvs.append((u_coord, v_coord))

    # Connect grid into quad faces (split as 2 triangles each)
    for i in range(n_profile - 1):
        for j in range(segments):
            j_next = (j + 1) % segments

            idx0 = i * segments + j
            idx1 = i * segments + j_next
            idx2 = (i + 1) * segments + j_next
            idx3 = (i + 1) * segments + j

            # Quad as two triangles (idx0, idx1, idx2) and (idx0, idx2, idx3)
            faces.append((idx0, idx1, idx2))
            faces.append((idx0, idx2, idx3))

    return verts, faces, uvs


def make_jewel_sphere(cx, cy, cz, radius=2.2, latitude_segs=8, longitude_segs=12):
    """Generates a small faceted gemstone sphere centered at (cx, cy, cz)."""
    verts = []
    faces = []
    uvs = []

    for i in range(latitude_segs + 1):
        lat = -math.pi / 2.0 + math.pi * i / latitude_segs
        z = cz + radius * math.sin(lat)
        r = radius * math.cos(lat)
        v = float(i) / latitude_segs

        for j in range(longitude_segs):
            lon = 2.0 * math.pi * j / longitude_segs
            x = cx + r * math.cos(lon)
            y = cy + r * math.sin(lon)
            u = float(j) / longitude_segs

            verts.append((x, y, z))
            uvs.append((u, v))

    for i in range(latitude_segs):
        for j in range(longitude_segs):
            j_next = (j + 1) % longitude_segs
            idx0 = i * longitude_segs + j
            idx1 = i * longitude_segs + j_next
            idx2 = (i + 1) * longitude_segs + j_next
            idx3 = (i + 1) * longitude_segs + j

            faces.append((idx0, idx1, idx2))
            faces.append((idx0, idx2, idx3))

    return verts, faces, uvs


def combine_meshes(mesh_list):
    """Combines a list of (verts, faces, uvs) into a single mesh."""
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


def write_3ds(filepath, objects, materials):
    """
    Writes a complete 3DS file with materials and sub-objects.
    objects: list of (obj_name, verts, faces, uvs, mat_name)
    materials: list of (mat_name, tex_filename)
    """
    def make_chunk(cid, data):
        return struct.pack('<HI', cid, 6 + len(data)) + data

    main_payload = make_chunk(0x0002, struct.pack('<I', 3)) # Version 3

    edit_payload = b''

    # Materials Chunk (0xAFFF)
    for mat_name, tex_filename in materials:
        mat_chunks = b''
        # Material Name (0xA000)
        mat_chunks += make_chunk(0xA000, mat_name.encode('ascii') + b'\x00')

        # Texture Map Chunk (0xA200) -> Map Name (0xA300)
        tex_chunks = make_chunk(0xA300, tex_filename.encode('ascii') + b'\x00')
        mat_chunks += make_chunk(0xA200, tex_chunks)

        edit_payload += make_chunk(0xAFFF, mat_chunks)

    # Object Chunks (0x4000)
    for obj_name, verts, faces, uvs, mat_name in objects:
        mesh_chunks = b''

        # Vertex List (0x4110)
        vdata = struct.pack('<H', len(verts))
        for v in verts:
            vdata += struct.pack('<fff', v[0], v[1], v[2])
        mesh_chunks += make_chunk(0x4110, vdata)

        # UV Coordinates (0x4140)
        uvdata = struct.pack('<H', len(uvs))
        for uv in uvs:
            uvdata += struct.pack('<ff', uv[0], uv[1])
        mesh_chunks += make_chunk(0x4140, uvdata)

        # Face List (0x4120)
        fdata = struct.pack('<H', len(faces))
        for f in faces:
            fdata += struct.pack('<HHHH', f[0], f[1], f[2], 0x0007)

        # Material Group (0x4130) inside Face List
        mgdata = mat_name.encode('ascii') + b'\x00' + struct.pack('<H', len(faces))
        for i in range(len(faces)):
            mgdata += struct.pack('<H', i)

        fdata += make_chunk(0x4130, mgdata)
        mesh_chunks += make_chunk(0x4120, fdata)

        # Triangle Mesh Chunk (0x4100)
        mesh_chunk = make_chunk(0x4100, mesh_chunks)

        # Named Object Chunk (0x4000)
        obj_payload = obj_name.encode('ascii') + b'\x00' + mesh_chunk
        edit_payload += make_chunk(0x4000, obj_payload)

    main_payload += make_chunk(0x3D3D, edit_payload)
    file_data = make_chunk(0x4D4D, main_payload)

    with open(filepath, 'wb') as f:
        f.write(file_data)

    print(f"Written 3DS file: {filepath}")


def build_goblet_model():
    """Builds the 3D geometry for the ornate chalice and jewel embellishments."""
    segments = 24

    # Profile curve for the goblet body (r, z) matching ~25x25x35 original bounds
    # Z range: -16.9 to +17.9
    profile = [
        # Base lip and tiered foot
        (11.3, -16.9),
        (11.3, -15.8),
        (10.5, -15.1),
        (10.0, -14.2),
        (6.5,  -12.0),
        (4.5,  -10.5),

        # Lower stem with knop 1
        (2.2,  -8.0),
        (2.8,  -6.0),
        (3.8,  -4.0), # Central stem knop
        (2.8,  -2.0),
        (2.2,   0.0),

        # Upper stem with knop 2
        (2.8,   2.5),
        (4.2,   4.5), # Upper stem decorative collar
        (3.2,   6.0),
        (4.5,   7.5),

        # Cup outer body (flared chalice)
        (7.5,   9.5),
        (10.2, 12.0),
        (11.5, 14.5),
        (12.2, 17.0),
        (12.2, 17.9), # Rim tip

        # Cup rim bevel and interior bowl (hollowed out)
        (11.2, 17.5),
        (10.2, 15.0),
        (8.5,  12.0),
        (5.5,   9.0),
        (2.0,   7.5),
        (0.0,   7.0), # Center bottom of interior cup
    ]

    # Generate main chalice mesh
    chalice_mesh = make_revolved_surface(profile, segments=segments, u_range=(0.0, 1.0), v_range=(0.0, 1.0))
    chalice_verts, chalice_faces, chalice_uvs = chalice_mesh
    chalice_verts, chalice_faces = orient_faces_outward(chalice_verts, chalice_faces)

    # Generate embedded gemstone jewels around the upper knop collar (z=4.5)
    jewel_meshes = []
    num_jewels = 6
    jewel_r = 4.2
    jewel_z = 4.5

    for k in range(num_jewels):
        angle = 2.0 * math.pi * k / num_jewels
        jx = jewel_r * math.cos(angle)
        jy = jewel_r * math.sin(angle)
        jz = jewel_z
        j_mesh = make_jewel_sphere(jx, jy, jz, radius=1.3, latitude_segs=8, longitude_segs=10)
        jewel_meshes.append(j_mesh)

    jewels_verts, jewels_faces, jewels_uvs = combine_meshes(jewel_meshes)
    jewels_verts, jewels_faces = orient_faces_outward(jewels_verts, jewels_faces)

    objects = [
        ("Cylinder01", chalice_verts, chalice_faces, chalice_uvs, "Material #8"),
        ("Object01", jewels_verts, jewels_faces, jewels_uvs, "Material #9"),
    ]

    materials = [
        ("Material #8", "GOBLET.BMP"),
        ("Material #9", "GOBJWL.BMP"),
    ]

    return objects, materials


def create_goblet_textures(output_dir):
    """Generates high-quality procedural textures for Goblet.bmp and GobJwl.bmp."""
    os.makedirs(output_dir, exist_ok=True)

    # 1. Goblet.bmp (Gold chalice texture)
    w, h = 512, 512
    img_goblet = Image.new('RGB', (w, h), (180, 140, 40))
    draw = ImageDraw.Draw(img_goblet)

    # Gold metallic base gradient and engraving bands
    for y in range(h):
        # Vertical gradient simulation for metallic reflection
        v_norm = y / float(h)
        band_factor = 0.8 + 0.35 * math.sin(v_norm * math.pi * 12.0)

        r = int(min(255, max(0, 210 * band_factor + 30)))
        g = int(min(255, max(0, 160 * band_factor + 20)))
        b = int(min(255, max(0, 45 * band_factor + 10)))

        draw.line([(0, y), (w - 1, y)], fill=(r, g, b))

    # Filigree engraving lines and rim borders
    border_color = (245, 210, 90)
    shadow_color = (110, 75, 15)

    # Horizontal decorative bands
    for y_band in [40, 80, 160, 280, 420, 480]:
        draw.rectangle([0, y_band, w - 1, y_band + 6], fill=border_color)
        draw.rectangle([0, y_band + 7, w - 1, y_band + 9], fill=shadow_color)

    # Filigree arc patterns
    for x in range(0, w, 32):
        draw.arc([x, 100, x + 32, 140], start=0, end=180, fill=border_color, width=2)
        draw.arc([x, 300, x + 32, 340], start=180, end=360, fill=border_color, width=2)

    img_goblet = img_goblet.filter(ImageFilter.SMOOTH_MORE)

    goblet_bmp_path = os.path.join(output_dir, 'Goblet.bmp')
    goblet_dds_path = os.path.join(output_dir, 'Goblet.dds')
    img_goblet.save(goblet_bmp_path)
    img_goblet.save(goblet_dds_path) # Direct copy for DirectX fallback

    # 2. GobJwl.bmp (Ruby gemstone texture)
    jw, jh = 256, 256
    img_jwl = Image.new('RGB', (jw, jh), (120, 10, 20))
    draw_jwl = ImageDraw.Draw(img_jwl)

    cx, cy = jw / 2.0, jh / 2.0
    max_r = min(cx, cy) * 0.9

    for y in range(jh):
        for x in range(jw):
            dx = (x - cx) / max_r
            dy = (y - cy) / max_r
            dist = math.sqrt(dx * dx + dy * dy)

            if dist <= 1.0:
                # Radial jewel highlight with faceted reflection simulation
                hl = max(0.0, 1.0 - math.sqrt((dx + 0.3) ** 2 + (dy + 0.3) ** 2))
                facet = 0.85 + 0.15 * math.cos(math.atan2(dy, dx) * 8.0)

                r = int(min(255, (160 + hl * 95) * facet))
                g = int(min(255, (15 + hl * 130) * facet))
                b = int(min(255, (30 + hl * 130) * facet))
                draw_jwl.point((x, y), fill=(r, g, b))
            else:
                # Gold mounting bezel edge
                draw_jwl.point((x, y), fill=(215, 170, 50))

    jwl_bmp_path = os.path.join(output_dir, 'GobJwl.bmp')
    jwl_dds_path = os.path.join(output_dir, 'GobJwl.dds')
    img_jwl.save(jwl_bmp_path)
    img_jwl.save(jwl_dds_path)

    print(f"Generated textures in: {output_dir}")


def main():
    target_dir = 'Models/3ds/Goblet'
    os.makedirs(target_dir, exist_ok=True)

    # Build 3DS model
    objects, materials = build_goblet_model()
    model_3ds_path = os.path.join(target_dir, 'Goblet.3ds')
    write_3ds(model_3ds_path, objects, materials)

    # Build BMP / DDS textures
    create_goblet_textures(target_dir)


if __name__ == '__main__':
    main()
