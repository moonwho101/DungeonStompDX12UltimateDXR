#!/usr/bin/env python3
"""
generate_blocks.py

Generates BLOCK01.3ds and BLOCK02.3ds 3DS models and exports their corresponding BMP textures
in Textures/Dungeon02/3ds/ formatted for Import3DS.cpp and TDSLoader.

Texture mappings:
  entrance_uni2 -> e2.bmp
  entrance_uni1 -> e1.bmp
  corridor07    -> c07.bmp
"""

import struct
import os
import sys
from PIL import Image

# 3DS Chunk IDs (matching Import3DS.cpp / Import3DS.hpp)
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

# Texture mapping dictionary
TEX_MAP = {
    'entrance_uni2': 'e2.bmp',
    'entrance_uni1': 'e1.bmp',
    'corridor07': 'c07.bmp'
}

# Image source files
IMG_SOURCES = {
    'e1.bmp': 'Textures/Dungeon02/entrance_uni1.jpg',
    'e2.bmp': 'Textures/Dungeon02/entrance_uni2.jpg',
    'c07.bmp': 'Textures/Dungeon02/corridor07.jpg'
}

def cstring(s):
    return s.encode('ascii') + b'\x00'

def rot_x_90(x, y, z):
    return (-x, z, y)

def convert_bmp_textures(output_dir):
    os.makedirs(output_dir, exist_ok=True)
    for bmp_name, src_jpg in IMG_SOURCES.items():
        if os.path.exists(src_jpg):
            out_path = os.path.join(output_dir, bmp_name)
            img = Image.open(src_jpg)
            img.convert('RGB').save(out_path, format='BMP')
            print(f"Exported BMP texture: {out_path}")
        else:
            print(f"Warning: Source texture {src_jpg} not found")

def parse_objects_dat(objects_dat_path):
    if not os.path.exists(objects_dat_path):
        return None

    with open(objects_dat_path, 'r', encoding='utf-8', errors='ignore') as f:
        lines = f.readlines()

    objects = {}
    curr_obj = None
    curr_tex = None

    for line in lines:
        l = line.strip()
        if not l:
            continue
        if l.startswith('OBJECT '):
            parts = l.split()
            obj_id = parts[1]
            obj_name = parts[2] if len(parts) > 2 else f'OBJ_{obj_id}'
            curr_obj = {'id': obj_id, 'name': obj_name, 'triangles': [], 'textures': set()}
            objects[obj_name] = curr_obj
        elif l.startswith('TEXTURE '):
            curr_tex = l.split()[1]
            if curr_obj:
                curr_obj['textures'].add(curr_tex)
        elif l.startswith('TRITEX '):
            parts = l.split()
            v1 = (float(parts[1]), float(parts[2]), float(parts[3]), float(parts[4]), float(parts[5]))
            curr_obj['current_tritex'] = [v1]
        elif curr_obj and 'current_tritex' in curr_obj:
            parts = l.split()
            if len(parts) == 5:
                v = (float(parts[0]), float(parts[1]), float(parts[2]), float(parts[3]), float(parts[4]))
                curr_obj['current_tritex'].append(v)
                if len(curr_obj['current_tritex']) == 3:
                    curr_obj['triangles'].append((curr_tex, curr_obj['current_tritex']))
                    del curr_obj['current_tritex']

    return objects

def write_3ds_model(filepath, obj_name, triangles):
    vertices = []
    uvs = []
    faces = []
    tex_per_face = []

    for tex, tri in triangles:
        bmp_tex = TEX_MAP.get(tex, tex)
        face_indices = []
        for (x, y, z, u, v) in tri:
            x2, y2, z2 = rot_x_90(x, y, z)
            idx = len(vertices)
            vertices.append((x2, y2, z2))
            uvs.append((u, v))
            face_indices.append(idx)

        a, b, c = face_indices
        faces.append((a, c, b))
        tex_per_face.append(bmp_tex)

    unique_textures = []
    for t in tex_per_face:
        if t not in unique_textures:
            unique_textures.append(t)

    main_data = bytearray()
    edit_data = bytearray()

    # Materials
    for tex in unique_textures:
        mat_sub = bytearray()

        # MAT_NAME01
        mat_name_bytes = cstring(tex)
        mat_sub += struct.pack('<HI', MAT_NAME01, 6 + len(mat_name_bytes)) + mat_name_bytes

        # TEXTURE_MAP + MAPPING_NAME
        mapname_bytes = cstring(tex)
        texmap_data = struct.pack('<HI', MAPPING_NAME, 6 + len(mapname_bytes)) + mapname_bytes
        mat_sub += struct.pack('<HI', TEXTURE_MAP, 6 + len(texmap_data)) + texmap_data

        edit_data += struct.pack('<HI', EDIT_MATERIAL, 6 + len(mat_sub)) + mat_sub

    # Mesh Object
    named_obj_data = bytearray()
    named_obj_data += cstring(obj_name)

    mesh_data = bytearray()

    # TRIANGLE_VERTEXLIST
    vlist = bytearray()
    vlist += struct.pack('<H', len(vertices))
    for (x, y, z) in vertices:
        vlist += struct.pack('<fff', y, x, z)
    mesh_data += struct.pack('<HI', TRIANGLE_VERTEXLIST, 6 + len(vlist)) + vlist

    # TRIANGLE_FACELIST + TRIANGLE_MATERIAL
    flist = bytearray()
    flist += struct.pack('<H', len(faces))
    for (a, b, c) in faces:
        flist += struct.pack('<HHH', a, b, c)
        flist += struct.pack('<H', 0x0007)

    # TRIANGLE_MATERIAL subchunks inside FACELIST
    for tex in unique_textures:
        assigned_faces = [i for i, t in enumerate(tex_per_face) if t == tex]
        if assigned_faces:
            tm_data = bytearray()
            tm_data += cstring(tex)
            tm_data += struct.pack('<H', len(assigned_faces))
            for f_idx in assigned_faces:
                tm_data += struct.pack('<H', f_idx)
            flist += struct.pack('<HI', TRIANGLE_MATERIAL, 6 + len(tm_data)) + tm_data

    mesh_data += struct.pack('<HI', TRIANGLE_FACELIST, 6 + len(flist)) + flist

    # TRIANGLE_MAPPINGCOORS
    mcoords = bytearray()
    mcoords += struct.pack('<H', len(uvs))
    for (u, v) in uvs:
        mcoords += struct.pack('<ff', u, v)
    mesh_data += struct.pack('<HI', TRIANGLE_MAPPINGCOORS, 6 + len(mcoords)) + mcoords

    # Wrap TRIANGLE_MESH into NAMED_OBJECT
    named_obj_data += struct.pack('<HI', TRIANGLE_MESH, 6 + len(mesh_data)) + mesh_data
    edit_data += struct.pack('<HI', NAMED_OBJECT, 6 + len(named_obj_data)) + named_obj_data

    # Version
    ver_chunk = struct.pack('<HI', 0x0002, 10) + struct.pack('<I', 3)

    main_payload = ver_chunk + struct.pack('<HI', EDIT3DS, 6 + len(edit_data)) + edit_data
    main_chunk = struct.pack('<HI', MAIN3DS, 6 + len(main_payload)) + main_payload

    with open(filepath, 'wb') as f:
        f.write(main_chunk)
    print(f"Generated 3DS model: {filepath} ({len(main_chunk)} bytes)")

def main():
    root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    output_dir = os.path.join(root_dir, 'Textures', 'Dungeon02', '3ds')
    objects_dat_path = os.path.join(root_dir, 'bin', 'Objects.dat')

    convert_bmp_textures(output_dir)

    objects = parse_objects_dat(objects_dat_path)
    if not objects or 'BLOCK01' not in objects or 'BLOCK02' not in objects:
        print("Error: Could not parse BLOCK01 / BLOCK02 from Objects.dat")
        sys.exit(1)

    block01_path = os.path.join(output_dir, 'BLOCK01.3ds')
    block02_path = os.path.join(output_dir, 'BLOCK02.3ds')

    write_3ds_model(block01_path, 'BLOCK01', objects['BLOCK01']['triangles'])
    write_3ds_model(block02_path, 'BLOCK02', objects['BLOCK02']['triangles'])

if __name__ == '__main__':
    main()
