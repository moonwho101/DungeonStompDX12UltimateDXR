import struct
import re

# Chunk IDs
MAIN3DS      = 0x4D4D
EDIT3DS      = 0x3D3D
EDIT_OBJECT  = 0x4000
OBJ_MESH     = 0x4100
MESH_VERT    = 0x4110
MESH_FACES   = 0x4120
MESH_UV      = 0x4140
MESH_MAT     = 0x4130
FACE_LIST    = 0x4131
MAT_ENTRY    = 0xAFFF
MAT_NAME     = 0xA000
MAT_TEX      = 0xA200
MAT_TEXFILE  = 0xA300

def chunk(cid, children=b"", raw=b""):
    """Build a chunk with optional children and raw data."""
    data = raw + children
    return struct.pack("<HH", cid, len(data) + 6) + data

def strz(s):
    return s.encode("ascii") + b"\x00"

def safe8(name):
    return name[:8]

def parse_tritex(text):
    blocks = []
    current_texture = None

    for line in text.splitlines():
        line = line.strip()

        if line.startswith("TEXTURE"):
            current_texture = safe8(line.split()[1])

        elif line.startswith("TRITEX"):
            verts = []
            parts = line.split()[1:]
            verts.append(tuple(map(float, parts)))

        elif re.match(r"^-?\d", line):
            parts = line.split()
            verts.append(tuple(map(float, parts)))
            if len(verts) == 3:
                blocks.append((current_texture, verts))
                verts = []

    return blocks

def write_3ds(blocks, filename):
    vertices = []
    uvs = []
    faces = []
    materials = {}
    vert_map = {}

    def get_index(v):
        key = (v[0], v[1], v[2], v[3], v[4])
        if key not in vert_map:
            vert_map[key] = len(vertices)
            vertices.append((v[0], v[1], v[2]))
            uvs.append((v[3], v[4]))
        return vert_map[key]

    for tex, tri in blocks:
        idx = [get_index(v) for v in tri]
        face_index = len(faces)
        faces.append(idx)
        materials.setdefault(tex, []).append(face_index)

    # Build vertex chunk
    vraw = struct.pack("<H", len(vertices))
    for x, y, z in vertices:
        vraw += struct.pack("<fff", x, y, z)
    vert_chunk = chunk(MESH_VERT, raw=vraw)

    # Build UV chunk
    uvraw = struct.pack("<H", len(uvs))
    for u, v in uvs:
        uvraw += struct.pack("<ff", u, v)
    uv_chunk = chunk(MESH_UV, raw=uvraw)

    # Build face chunk
    fraw = struct.pack("<H", len(faces))
    for a, b, c in faces:
        fraw += struct.pack("<HHH", a, b, c)
        fraw += struct.pack("<H", 0)
    face_chunk = chunk(MESH_FACES, raw=fraw)

    # Build material assignment chunks
    mat_assign_chunks = b""
    for tex, face_list in materials.items():
        name_chunk = chunk(MAT_NAME, raw=strz(tex))

        flraw = struct.pack("<H", len(face_list))
        for fi in face_list:
            flraw += struct.pack("<H", fi)
        fl_chunk = chunk(FACE_LIST, raw=flraw)

        mat_assign_chunks += chunk(MESH_MAT, children=name_chunk + fl_chunk)

    # Build mesh chunk
    mesh_chunk = chunk(OBJ_MESH, children=vert_chunk + uv_chunk + face_chunk + mat_assign_chunks)

    # Build object chunk
    objname = chunk(EDIT_OBJECT, raw=strz("TRITEX"), children=mesh_chunk)

    # Build material chunks
    mat_chunks = b""
    for tex in materials:
        name_chunk = chunk(MAT_NAME, raw=strz(tex))
        texfile_chunk = chunk(MAT_TEXFILE, raw=strz(tex + ".png"))
        tex_chunk = chunk(MAT_TEX, children=texfile_chunk)
        mat_chunks += chunk(MAT_ENTRY, children=name_chunk + tex_chunk)

    # Build EDIT chunk
    edit_chunk = chunk(EDIT3DS, children=mat_chunks + objname)

    # Build MAIN chunk
    main_chunk = chunk(MAIN3DS, children=edit_chunk)

    with open(filename, "wb") as f:
        f.write(main_chunk)

    print("Wrote:", filename)

if __name__ == "__main__":
    with open("o.txt") as f:
        data = f.read()

    blocks = parse_tritex(data)
    write_3ds(blocks, "o2.3ds")
