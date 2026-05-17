import struct
import os

def parse_md2(filepath):
    if not os.path.exists(filepath):
        print(f"\n[Error] File not found: {filepath}")
        return
        
    with open(filepath, 'rb') as f:
        data = f.read()
    
    # Read header
    ident, version = struct.unpack_from('<ii', data, 0)
    skinwidth, skinheight = struct.unpack_from('<ii', data, 8)
    framesize = struct.unpack_from('<i', data, 16)[0]
    num_skins, num_verts, num_tex_verts, num_faces = struct.unpack_from('<iiii', data, 20)
    num_glcmds, num_frames = struct.unpack_from('<ii', data, 36)
    offset_skins, offset_tex_verts, offset_faces = struct.unpack_from('<iii', data, 44)
    offset_frames, offset_glcmds, offset_end = struct.unpack_from('<iii', data, 56)
    
    print(f"\n=== {os.path.basename(filepath)} ===")
    print(f"Magic: {ident} (expected 844121161)")
    print(f"Version: {version} (expected 8)")
    print(f"Skin: {skinwidth}x{skinheight}")
    print(f"Frame size: {framesize} bytes")
    print(f"Num skins: {num_skins}, verts: {num_verts}, tex_verts: {num_tex_verts}, faces: {num_faces}")
    print(f"Num GL cmds: {num_glcmds}, frames: {num_frames}")
    print(f"Offsets: skins={offset_skins}, tex_verts={offset_tex_verts}, faces={offset_faces}")
    print(f"         frames={offset_frames}, glcmds={offset_glcmds}, end={offset_end}")
    
    # Read skins
    print(f"\nSkins:")
    for i in range(num_skins):
        skin_name = data[offset_skins + i*64 : offset_skins + i*64 + 64]
        skin_name = skin_name.split(b'\x00')[0].decode('ascii', errors='replace')
        print(f"  Skin {i}: '{skin_name}'")
    
    # Read frame names
    print(f"\nFrame names:")
    for i in range(min(num_frames, 20)):  # Show up to 20 frames
        frame_offset = offset_frames + i * framesize
        # scale (3 floats) + translate (3 floats) + name (16 bytes)
        scale = struct.unpack_from('<fff', data, frame_offset)
        translate = struct.unpack_from('<fff', data, frame_offset + 12)
        name = data[frame_offset + 24 : frame_offset + 40]
        name = name.split(b'\x00')[0].decode('ascii', errors='replace')
        print(f"  Frame {i}: '{name}' scale=({scale[0]:.4f},{scale[1]:.4f},{scale[2]:.4f}) translate=({translate[0]:.1f},{translate[1]:.1f},{translate[2]:.1f})")
    if num_frames > 20:
        print(f"  ... and {num_frames - 20} more frames")
    
    # Read GL commands (just count/types)
    pos = offset_glcmds
    cmd_count = 0
    total_verts = 0
    while pos < offset_end:
        cmd = struct.unpack_from('<i', data, pos)[0]
        pos += 4
        if cmd == 0:
            break
        n = abs(cmd)
        cmd_count += 1
        total_verts += n
        pos += n * 12  # each vert: float s, float t, int index
    print(f"\nGL Commands: {cmd_count} strips/fans, {total_verts} total verts")
    
    # Show first frame vertex bounding box
    frame_offset = offset_frames
    scale = struct.unpack_from('<fff', data, frame_offset)
    translate = struct.unpack_from('<fff', data, frame_offset + 12)
    
    min_x = min_y = min_z = 999999
    max_x = max_y = max_z = -999999
    vert_offset = frame_offset + 40  # after scale(12) + translate(12) + name(16) = 40
    for j in range(num_verts):
        vx, vy, vz, _ = struct.unpack_from('<BBBB', data, vert_offset + j*4)
        rx = scale[0] * vx + translate[0]
        ry = scale[1] * vy + translate[1]
        rz = scale[2] * vz + translate[2]
        min_x = min(min_x, rx); max_x = max(max_x, rx)
        min_y = min(min_y, ry); max_y = max(max_y, ry)
        min_z = min(min_z, rz); max_z = max(max_z, rz)
    print(f"Frame 0 bounds:")
    print(f"  X: {min_x:.1f} to {max_x:.1f} (width {max_x-min_x:.1f})")
    print(f"  Y: {min_y:.1f} to {max_y:.1f} (height {max_y-min_y:.1f})")
    print(f"  Z: {min_z:.1f} to {max_z:.1f} (depth {max_z-min_z:.1f})")

base = r"c:\github\DungeonStompDX12UltimateDXR\Models\Players\Knight"
parse_md2(os.path.join(base, "axe.md2"))
parse_md2(os.path.join(base, "bastard_sword.md2"))
