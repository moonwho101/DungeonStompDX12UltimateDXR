import struct
import sys
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

def rot_x_90(x, y, z):
    return (-x, z, y)

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
            if current_obj is not None and texname not in current_obj["textures"]:
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

            if current_obj is None:
                current_obj = {
                    "name": "OBJECT",
                    "triangles": [],
                    "textures": []
                }
                objects.append(current_obj)

            current_obj["triangles"].append((current_tex, v1, v2, v3))

            i += 3
            continue

        i += 1

    return objects

def write_3ds(filename, objects):
    with open(filename, 'wb') as f:
        # MAIN3DS chunk
        main_data = bytearray()

        # EDIT3DS chunk
        edit_data = bytearray()

        # --- Materials ---
        # Build unique texture list across all triangles
        all_textures = []
        for obj in objects:
            for tex, v1, v2, v3 in obj["triangles"]:
                if tex and tex not in all_textures:
                    all_textures.append(tex)

        mat_data = bytearray()
        for tex in all_textures:
            # EDIT_MATERIAL subchunk
            mat_sub = bytearray()

            # MAT_NAME01 chunk
            mat_name_bytes = cstring(tex)
            buf = bytearray() + mat_name_bytes
            mat_sub += struct.pack('<HI', MAT_NAME01, 6 + len(buf)) + buf

            # TEXTURE_MAP + MAPPING_NAME
            texmap_data = bytearray()
            mapname_bytes = cstring(tex)
            texmap_data += struct.pack('<HI', MAPPING_NAME, 6 + len(mapname_bytes)) + mapname_bytes

            mat_sub += struct.pack('<HI', TEXTURE_MAP, 6 + len(texmap_data)) + texmap_data

            # Wrap EDIT_MATERIAL
            mat_data += struct.pack('<HI', EDIT_MATERIAL, 6 + len(mat_sub)) + mat_sub

        edit_data += mat_data

        # --- Objects / meshes ---
        # In Import3DS.cpp, texture is assigned per 3DS NAMED_OBJECT (object_texture[total_num_objects]).
        # Therefore, triangles in each object must be grouped by texture into distinct 3DS NAMED_OBJECTs.
        for obj in objects:
            tex_groups = OrderedDict()
            for tex, v1, v2, v3 in obj["triangles"]:
                if tex not in tex_groups:
                    tex_groups[tex] = []
                tex_groups[tex].append((v1, v2, v3))

            for tex, tris in tex_groups.items():
                if len(tex_groups) == 1:
                    sub_name = obj["name"]
                else:
                    sub_name = f"{obj['name']}_{tex}" if tex else obj["name"]

                vertices = []
                uvs = []
                faces = []

                for v1, v2, v3 in tris:
                    face_indices = []
                    for (x, y, z, u, v) in (v1, v2, v3):
                        x2, y2, z2 = rot_x_90(x, y, z)
                        idx = len(vertices)
                        vertices.append((x2, y2, z2))
                        uvs.append((u, v))
                        face_indices.append(idx)

                    # Enforce clockwise winding (Direct3D default)
                    a, b, c = face_indices
                    faces.append((a, c, b))

                named_obj_data = bytearray()
                named_obj_data += cstring(sub_name)

                mesh_data = bytearray()

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

                # TRIANGLE_MATERIAL
                if tex:
                    tm_data = bytearray()
                    tm_data += cstring(tex)
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
    if len(sys.argv) > 1:
        input_file = sys.argv[1]
        output_file = sys.argv[2] if len(sys.argv) > 2 else "output.3ds"
        with open(input_file, "r", encoding="utf-8") as f:
            content = f.read()
        objs = parse_tritex_file(content)
        write_3ds(output_file, objs)
        print(f"Wrote {output_file}")
    else:
        tritex_text = """
OBJECT 191 BLOCK02

TEXTURE corridor07
TRITEX -27.65625 60.09066 -28.125 0.0005 0.9995
       32.34375 60.09066 31.875 0.999501 0.0004999999999999449
       32.34375 60.09066 -28.125 0.9995 0.999501

TRITEX -27.65625 60.09066 -21.764235 0.0005 0.893593
       32.34375 60.09066 31.875 0.999501 0.0004999999999999449
       -27.65625 60.09066 -28.125 0.0005 0.9995

TRITEX -2.61522 60.09066 31.875 0.417433 0.0004999999999999449
       32.34375 60.09066 31.875 0.999501 0.0004999999999999449
       -27.65625 60.09066 -21.764235 0.0005 0.893593

TRITEX -27.65625 60.09066 -28.125 0.0005 0.0004990000000000272
       32.34375 28.349325 -28.125 0.9995 0.528993
       -27.65625 26.72751 -28.125 0.0005 0.5559970000000001

TRITEX 32.34375 60.09066 -28.125 0.9995 0.0004990000000000272
       32.34375 28.349325 -28.125 0.9995 0.528993
       -27.65625 60.09066 -28.125 0.0005 0.0004990000000000272

TRITEX -2.61522 60.09066 31.875 0.582567 0.0004990000000000272
       32.34375 14.73504 31.875 0.0005 0.755671
       32.34375 60.09066 31.875 0.0005 0.0004990000000000272

TRITEX -27.65625 46.59066 31.875 0.9995 0.225275
       32.34375 14.73504 31.875 0.0005 0.755671
       -2.61522 60.09066 31.875 0.582567 0.0004990000000000272

TRITEX -27.65625 13.11321 31.875 0.9995 0.782675
       32.34375 14.73504 31.875 0.0005 0.755671
       -27.65625 46.59066 31.875 0.9995 0.225275

TRITEX -27.65625 60.09066 -21.764235 0.893593 0.0004990000000000272
       -27.65625 13.11321 31.875 0.0005 0.782675
       -27.65625 46.59066 31.875 0.0005 0.225275

TRITEX -27.65625 60.09066 -28.125 0.9995 0.0004990000000000272
       -27.65625 13.11321 31.875 0.0005 0.782675
       -27.65625 60.09066 -21.764235 0.893593 0.0004990000000000272

TRITEX -27.65625 26.72751 -28.125 0.9995 0.5559970000000001
       -27.65625 13.11321 31.875 0.0005 0.782675
       -27.65625 60.09066 -28.125 0.9995 0.0004990000000000272

TRITEX 32.34375 60.09066 -28.125 0.0005 0.0004990000000000272
       32.34375 14.73504 31.875 0.9995 0.755671
       32.34375 28.349325 -28.125 0.0005 0.528993

TRITEX 32.34375 60.09066 31.875 0.9995 0.0004990000000000272
       32.34375 14.73504 31.875 0.9995 0.755671
       32.34375 60.09066 -28.125 0.0005 0.0004990000000000272

TRITEX -27.65625 0.09066 -28.125 0.999501 0.9995
       32.34375 0.09066 31.875 0.0005 0.0004999999999999449
       -27.65625 0.09066 31.875 0.9995 0.0004990000000000272

TRITEX 32.34375 0.09066 -28.125 0.0005 0.999501
       32.34375 0.09066 31.875 0.0005 0.0004999999999999449
       -27.65625 0.09066 -28.125 0.999501 0.9995

TRITEX -27.65625 0.09066 -28.125 0.0005 0.999501
       32.34375 28.349325 -28.125 0.9995 0.528993
       32.34375 0.09066 -28.125 0.9995 0.999501

TRITEX -27.65625 18.80118 -28.125 0.0005 0.68797
       32.34375 28.349325 -28.125 0.9995 0.528993
       -27.65625 0.09066 -28.125 0.0005 0.999501

TRITEX -11.879895 27.153945 -28.125 0.263176 0.548896
       32.34375 28.349325 -28.125 0.9995 0.528993
       -27.65625 18.80118 -28.125 0.0005 0.68797

TRITEX -27.65625 0.09066 31.875 0.9995 0.999501
       32.34375 14.73504 31.875 0.0005 0.755671
       -27.65625 13.11321 31.875 0.9995 0.782675

TRITEX 32.34375 0.09066 31.875 0.0005 0.999501
       32.34375 14.73504 31.875 0.0005 0.755671
       -27.65625 0.09066 31.875 0.9995 0.999501

TRITEX -27.65625 18.80118 -28.125 0.9995 0.68797
       -27.65625 13.11321 31.875 0.0005 0.782675
       -27.65625 22.942439999999998 -11.443725 0.721757 0.6190180000000001

TRITEX -27.65625 0.09066 -28.125 0.9995 0.999501
       -27.65625 13.11321 31.875 0.0005 0.782675
       -27.65625 18.80118 -28.125 0.9995 0.68797

TRITEX -27.65625 0.09066 31.875 0.0005 0.999501
       -27.65625 13.11321 31.875 0.0005 0.782675
       -27.65625 0.09066 -28.125 0.9995 0.999501

TRITEX 32.34375 0.09066 -28.125 0.0005 0.999501
       32.34375 14.73504 31.875 0.9995 0.755671
       32.34375 0.09066 31.875 0.9995 0.999501

TRITEX 32.34375 28.349325 -28.125 0.0005 0.528993
       32.34375 14.73504 31.875 0.9995 0.755671
       32.34375 0.09066 -28.125 0.0005 0.999501

TRITEX -27.65625 60.09066 -4.01334 0.598041 0.0004990000000000272
       -27.65625 60.09066 -21.764235 0.893593 0.0004990000000000272
       -27.65625 55.309365 -2.76684 0.577287 0.08010799999999996

TRITEX -21.953625 60.09066 -9.54891 0.095448 0.6902079999999999
       -27.65625 60.09066 -21.764235 0.0005 0.893593
       -27.65625 60.09066 -4.01334 0.0005 0.598041

TEXTURE entrance_uni1
TRITEX -27.65625 60.09066 -21.764235 0.0005 0.893593
       -27.65625 46.59066 31.875 0.0005 0.0004990000000000272
       -2.61522 60.09066 31.875 0.417433 0.0004999999999999449

TRITEX -27.65625 18.80118 -28.125 0.0005 0.9995
       -27.65625 22.942439999999998 -11.443725 0.0005 0.721757
       -11.879895 27.153945 -28.125 0.263176 0.9995

TRITEX -27.65625 55.309365 -2.76684 0.9995 0.08010799999999996
       -21.953625 60.09066 -9.54891 0.904552 0.0004990000000000272
       -27.65625 60.09066 -4.01334 0.9995 0.0004990000000000272

TRITEX -11.879895 27.153945 -28.125 0.736824 0.9995
       -27.65625 22.942439999999998 -11.443725 0.9995 0.721757
       -27.65625 26.72751 -28.125 0.999501 0.9995
"""

        objs = parse_tritex_file(tritex_text)
        write_3ds("output.3ds", objs)
        print("Wrote output.3ds")
