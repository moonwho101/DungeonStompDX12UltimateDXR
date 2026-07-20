import math

def rotate(x, z, angle_deg):
    angle_rad = math.radians(angle_deg)
    cosine = math.cos(angle_rad)
    sine = math.sin(angle_rad)
    rx = x * cosine - z * sine
    rz = x * sine + z * cosine
    return rx, rz

def run():
    objects_dat_path = r"c:\github\DungeonStompDX12UltimateDXR\bin\Objects.dat"
    vertices = []
    
    # Parse Objects.dat for right_curve_road
    with open(objects_dat_path, 'r') as f:
        lines = f.readlines()
        
    inside_target = False
    for line in lines:
        line = line.strip()
        if not line:
            continue
        if line.startswith("OBJECT"):
            if "right_curve_road" in line:
                inside_target = True
            else:
                inside_target = False
        elif inside_target:
            if line.startswith("QUADTEX") or line.startswith("QUAD") or line.startswith("TRI") or line.startswith("TRITEX"):
                # Parse vertex coordinates
                pass
            # Let's split by whitespace and check if we have lines with 3 or 5 numbers
            parts = line.split()
            if len(parts) >= 3:
                try:
                    # check if first three parts are numbers
                    x = float(parts[0])
                    y = float(parts[1])
                    z = float(parts[2])
                    vertices.append((x, z))
                except ValueError:
                    pass

    # Deduplicate vertices
    unique_verts = sorted(list(set(vertices)))
    print("Parsed unique local vertices of right_curve_road:")
    for v in unique_verts:
        print(f"  {v}")
        
    # The 6 segments coordinates and rotations
    rc_pieces = [
        (-220.00, -140.00, 0),
        (-207.73, -46.83, 345),
        (-171.77, 39.98, 330),
        (-114.56, 114.54, 315),
        (-40.00, 171.74, 300),
        (46.82, 207.70, 285)
    ]
    
    all_world_verts = []
    for seg_idx, (dx, dz, dr) in enumerate(rc_pieces):
        for vx, vz in unique_verts:
            # Rotate local vertex by the segment's rotation (dr)
            rx, rz = rotate(vx, vz, dr)
            # Translate by segment's local offset (dx, dz)
            wx = rx + dx
            wz = rz + dz
            all_world_verts.append((wx, wz))
            
    min_x = min(w[0] for w in all_world_verts)
    max_x = max(w[0] for w in all_world_verts)
    min_z = min(w[1] for w in all_world_verts)
    max_z = max(w[1] for w in all_world_verts)
    
    print("\nCombined bounding box (raw floats):")
    print(f"  min_x: {min_x:.6f}")
    print(f"  min_z: {min_z:.6f}")
    print(f"  max_x: {max_x:.6f}")
    print(f"  max_z: {max_z:.6f}")
    
    print("\nInteger rounded bounding box:")
    print(f"  ({math.floor(min_x)}, {math.floor(min_z)}, {math.ceil(max_x)}, {math.ceil(max_z)})")

if __name__ == "__main__":
    run()

