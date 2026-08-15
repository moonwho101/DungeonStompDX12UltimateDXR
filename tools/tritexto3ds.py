import struct
from collections import OrderedDict

# 3DS chunk IDs (matching your C++ importer)
MAIN3DS             = 0x4D4D
EDIT3DS             = 0x3D3D
NAMED_OBJECT        = 0x4000
TRIANGLE_MESH       = 0x4100
TRIANGLE_VERTEXLIST = 0x4110
TRIANGLE_FACELIST   = 0x4120
TRIANGLE_MATERIAL   = 0x4130
TRIANGLE_MAPPINGCOORS = 0x4140

EDIT_MATERIAL       = 0xAFFF
MAT_NAME01          = 0xA000
TEXTURE_MAP         = 0xA200
MAPPING_NAME        = 0xA300

def write_chunk(f, chunk_id, data_bytes):
    length = 6 + len(data_bytes)
    f.write(struct.pack('<HI', chunk_id, length))
    f.write(data_bytes)

def cstring(s):
    return s.encode('ascii') + b'\x00'

def parse_tritex_file(text):
    objects = []
    current_obj = None
    current_tex = None

    lines = [l.rstrip() for l in text.splitlines()]

    i = 0
    while i < len(lines):
        line = lines[i].strip()

        if not line:
            i += 1
            continue

        if line.startswith("OBJECT"):
            parts = line.split()
            objname = parts[2] if len(parts) > 2 else parts[1]
            current_obj = {
                "name": objname,
                "triangles": [],
                "textures": []
            }
            objects.append(current_obj)
            i += 1
            continue

        if line.startswith("TEXTURE"):
            _, texname = line.split()
            current_tex = texname
            current_obj["textures"].append(texname)
            i += 1
            continue

        if line.startswith("TRITEX"):
            # Parse first vertex
            parts = line.split()
            v1 = tuple(map(float, parts[1:6]))

            # Parse next two lines (must exist)
            v2_line = lines[i+1].strip()
            v3_line = lines[i+2].strip()

            v2 = tuple(map(float, v2_line.split()))
            v3 = tuple(map(float, v3_line.split()))

            current_obj["triangles"].append((current_tex, v1, v2, v3))

            i += 3
            continue

        i += 1

    return objects

def build_mesh_from_object(obj):
    vertices = []      # list of (x,y,z)
    uvs = []           # list of (u,v)
    faces = []         # list of (i1,i2,i3)
    tex_per_face = []  # texture per face

    for tex, v1, v2, v3 in obj["triangles"]:
        tri = [v1, v2, v3]
        face_indices = []
        for (x, y, z, u, v) in tri:
            idx = len(vertices)
            vertices.append((x, y, z))
            uvs.append((u, v))
            face_indices.append(idx)

        # enforce clockwise winding (Direct3D default)
        a, b, c = face_indices
        faces.append((a, c, b))

        tex_per_face.append(tex)

    return vertices, faces, uvs, tex_per_face

def write_3ds(filename, objects):
    with open(filename, 'wb') as f:
        # MAIN3DS chunk
        main_data = bytearray()

        # EDIT3DS chunk
        edit_data = bytearray()

        # --- Materials ---
        # Build unique texture list across all objects
        all_textures = []
        for obj in objects:
            for t in obj["textures"]:
                if t not in all_textures:
                    all_textures.append(t)

        mat_data = bytearray()
        for tex in all_textures:
            # EDIT_MATERIAL subchunk
            mat_sub = bytearray()

            # MAT_NAME01
            mat_name = tex  # use texture name as material name
            mat_name_bytes = cstring(mat_name)
            mat_sub_matname = mat_name_bytes
            write_chunk_bytes = struct.pack  # just alias

            # MAT_NAME01 chunk
            mat_sub_chunk = bytearray()
            mat_sub_chunk += mat_name_bytes
            write_chunk_bytes = struct.pack
            # We'll wrap MAT_NAME01 properly:
            buf = bytearray()
            buf += mat_name_bytes
            mat_sub += struct.pack('<HI', MAT_NAME01, 6 + len(buf)) + buf

            # TEXTURE_MAP + MAPPING_NAME
            texmap_data = bytearray()
            # MAPPING_NAME chunk inside TEXTURE_MAP
            mapname_bytes = cstring(tex)
            texmap_data += struct.pack('<HI', MAPPING_NAME, 6 + len(mapname_bytes)) + mapname_bytes

            mat_sub += struct.pack('<HI', TEXTURE_MAP, 6 + len(texmap_data)) + texmap_data

            # Wrap EDIT_MATERIAL
            mat_data += struct.pack('<HI', EDIT_MATERIAL, 6 + len(mat_sub)) + mat_sub

        edit_data += mat_data

        # --- Objects / meshes ---
        for obj in objects:
            obj_data = bytearray()

            # NAMED_OBJECT
            name_bytes = cstring(obj["name"])
            named_obj_data = bytearray()
            named_obj_data += name_bytes

            # TRIANGLE_MESH
            mesh_data = bytearray()

            vertices, faces, uvs, tex_per_face = build_mesh_from_object(obj)

            # TRIANGLE_VERTEXLIST
            vlist = bytearray()
            vlist += struct.pack('<H', len(vertices))
            for (x, y, z) in vertices:
                vlist += struct.pack('<fff', y, x, z)
            mesh_data += struct.pack('<HI', TRIANGLE_VERTEXLIST, 6 + len(vlist)) + vlist

            # TRIANGLE_FACELIST
            flist = bytearray()
            flist += struct.pack('<H', len(faces))
            for (a, b, c) in faces:
                flist += struct.pack('<HHH', a, b, c)
                flist += struct.pack('<H', 0)  # face flags
            mesh_data += struct.pack('<HI', TRIANGLE_FACELIST, 6 + len(flist)) + flist

            # TRIANGLE_MAPPINGCOORS
            mcoords = bytearray()
            mcoords += struct.pack('<H', len(uvs))
            for (u, v) in uvs:
                mcoords += struct.pack('<ff', u, v)
            mesh_data += struct.pack('<HI', TRIANGLE_MAPPINGCOORS, 6 + len(mcoords)) + mcoords

            # TRIANGLE_MATERIAL (assign first texture to all faces)
            if obj["textures"]:
                matname = obj["textures"][0]
                tm_data = bytearray()
                tm_data += cstring(matname)
                tm_data += struct.pack('<H', len(faces))
                for i in range(len(faces)):
                    tm_data += struct.pack('<H', i)
                mesh_data += struct.pack('<HI', TRIANGLE_MATERIAL, 6 + len(tm_data)) + tm_data

            # Wrap TRIANGLE_MESH
            named_obj_data += struct.pack('<HI', TRIANGLE_MESH, 6 + len(mesh_data)) + mesh_data

            # Wrap NAMED_OBJECT
            edit_data += struct.pack('<HI', NAMED_OBJECT, 6 + len(named_obj_data)) + named_obj_data

        # Wrap EDIT3DS
        main_data += struct.pack('<HI', EDIT3DS, 6 + len(edit_data)) + edit_data

        # Write MAIN3DS
        f.write(struct.pack('<HI', MAIN3DS, 6 + len(main_data)))
        f.write(main_data)


if __name__ == "__main__":
    # Paste your TRITEX text here
    tritex_text = """OBJECT 190 BLOCK01

TEXTURE entrance_uni2
TRITEX 32.34375 60.079035 6.23439 0.572584 0.0004990000000000272
       32.34375 0.079035 -28.125 0.0005 0.999501
       32.34375 52.088474999999995 -28.125 0.0005 0.13354200000000005
       
TRITEX 32.34375 60.079035 31.875 0.9995 0.0004990000000000272
       32.34375 0.079035 -28.125 0.0005 0.999501
       32.34375 60.079035 6.23439 0.572584 0.0004990000000000272
       
TRITEX 32.34375 0.079035 31.875 0.9995 0.999501
       32.34375 0.079035 -28.125 0.0005 0.999501
       32.34375 60.079035 31.875 0.9995 0.0004990000000000272
       
TRITEX -27.65625 52.088474999999995 31.875 0.0005 0.13354200000000005
       -27.65625 0.079035 -28.125 0.9995 0.999501
       -27.65625 0.079035 31.875 0.0005 0.999501
       
TRITEX -27.65625 38.134995 -28.125 0.9995 0.36586799999999997
       -27.65625 0.079035 -28.125 0.9995 0.999501
       -27.65625 52.088474999999995 31.875 0.0005 0.13354200000000005
       
TRITEX 32.34375 60.079035 31.875 0.0005 0.0004990000000000272
       -27.65625 0.079035 31.875 0.9995 0.999501
       32.34375 0.079035 31.875 0.0005 0.999501
       
TRITEX 6.70314 60.079035 31.875 0.427416 0.0004990000000000272
       -27.65625 0.079035 31.875 0.9995 0.999501
       32.34375 60.079035 31.875 0.0005 0.0004990000000000272
       
TRITEX -27.65625 52.088474999999995 31.875 0.9995 0.13354200000000005
       -27.65625 0.079035 31.875 0.9995 0.999501
       6.70314 60.079035 31.875 0.427416 0.0004990000000000272
       
TRITEX 32.34375 52.088474999999995 -28.125 0.9995 0.13354200000000005
       -27.65625 0.079035 -28.125 0.0005 0.999501
       -27.65625 38.134995 -28.125 0.0005 0.36586799999999997
       
TRITEX 32.34375 0.079035 -28.125 0.9995 0.999501
       -27.65625 0.079035 -28.125 0.0005 0.999501
       32.34375 52.088474999999995 -28.125 0.9995 0.13354200000000005
       
TRITEX 6.70314 60.079035 31.875 0.572584 0.0004999999999999449
       32.34375 60.079035 31.875 0.999501 0.0004999999999999449
       32.34375 60.079035 6.23439 0.9995 0.427416
       
TRITEX 32.34375 0.079035 31.875 0.0005 0.0004999999999999449
       -27.65625 0.079035 -28.125 0.999501 0.9995
       32.34375 0.079035 -28.125 0.0005 0.999501
       
TRITEX -27.65625 0.079035 31.875 0.9995 0.0004990000000000272
       -27.65625 0.079035 -28.125 0.999501 0.9995
       32.34375 0.079035 31.875 0.0005 0.0004999999999999449
       
TRITEX -27.65625 60.079035 -28.125 0.0005 0.9995
       32.34375 60.079035 6.23439 0.9995 0.427416
       32.34375 60.079035 -28.125 0.9995 0.999501
       
TRITEX -27.65625 60.079035 31.875 0.0005 0.0004990000000000272
       32.34375 60.079035 6.23439 0.9995 0.427416
       -27.65625 60.079035 -28.125 0.0005 0.9995
       
TRITEX 6.70314 60.079035 31.875 0.572584 0.0004999999999999449
       32.34375 60.079035 6.23439 0.9995 0.427416
       -27.65625 60.079035 31.875 0.0005 0.0004990000000000272
       
TRITEX -27.65625 46.026075 -28.125 0.0005 0.23448199999999997
       32.34375 52.088474999999995 -28.125 0.9995 0.13354200000000005
       -10.78689 42.058094999999995 -28.125 0.281375 0.30054800000000004
       
TRITEX -27.65625 60.079035 -28.125 0.0005 0.0004990000000000272
       32.34375 52.088474999999995 -28.125 0.9995 0.13354200000000005
       -27.65625 46.026075 -28.125 0.0005 0.23448199999999997
       
TRITEX 32.34375 60.079035 -28.125 0.9995 0.0004990000000000272
       32.34375 52.088474999999995 -28.125 0.9995 0.13354200000000005
       -27.65625 60.079035 -28.125 0.0005 0.0004990000000000272
       
TRITEX -27.65625 60.079035 31.875 0.9995 0.0004990000000000272
       -27.65625 52.088474999999995 31.875 0.9995 0.13354200000000005
       6.70314 60.079035 31.875 0.427416 0.0004990000000000272
       
TRITEX -27.65625 60.079035 -28.125 0.9995 0.0004990000000000272
       -27.65625 52.088474999999995 31.875 0.0005 0.13354200000000005
       -27.65625 60.079035 31.875 0.0005 0.0004990000000000272
       
TRITEX -27.65625 46.026075 -28.125 0.9995 0.23448199999999997
       -27.65625 52.088474999999995 31.875 0.0005 0.13354200000000005
       -27.65625 60.079035 -28.125 0.9995 0.0004990000000000272
       
TRITEX -27.65625 42.079905 -11.161904999999999 0.717065 0.30018500000000004
       -27.65625 52.088474999999995 31.875 0.0005 0.13354200000000005
       -27.65625 46.026075 -28.125 0.9995 0.23448199999999997
       
TRITEX 32.34375 60.079035 6.23439 0.572584 0.0004990000000000272
       32.34375 52.088474999999995 -28.125 0.0005 0.13354200000000005
       32.34375 60.079035 -28.125 0.0005 0.0004990000000000272

TEXTURE entrance_uni1
TRITEX -10.78689 42.058094999999995 -28.125 0.718625 0.9995
       -27.65625 42.079905 -11.161904999999999 0.9995 0.7170650000000001
       -27.65625 46.026075 -28.125 0.999501 0.9995
       
TRITEX -10.78689 42.058094999999995 -28.125 0.281375 0.9995
       -27.65625 38.134995 -28.125 0.0005 0.9995
       -27.65625 42.079905 -11.161904999999999 0.0005 0.7170650000000001
"""

    objs = parse_tritex_file(tritex_text)
    write_3ds("output.3ds", objs)
    print("Wrote output.3ds")
