import struct
import sys

def analyze_3ds(filepath):
    with open(filepath, 'rb') as f:
        data = f.read()

    vertices = []
    faces = []
    objects = []

    def parse(data, start, end):
        pos = start
        while pos < end - 6:
            cid = struct.unpack_from('<H', data, pos)[0]
            clen = struct.unpack_from('<I', data, pos+2)[0]
            cend = pos + clen
            if cid == 0x4110:
                n = struct.unpack_from('<H', data, pos+6)[0]
                for i in range(n):
                    x, y, z = struct.unpack_from('<fff', data, pos+8+i*12)
                    vertices.append((x, y, z))
            elif cid == 0x4120:
                n = struct.unpack_from('<H', data, pos+6)[0]
                for i in range(n):
                    v0, v1, v2, fl = struct.unpack_from('<HHHH', data, pos+8+i*8)
                    faces.append((v0, v1, v2))
            elif cid == 0x4000:
                inner = pos + 6
                while inner < cend and data[inner] != 0:
                    inner += 1
                inner += 1
                name_bytes = data[pos+6:inner-1]
                objects.append(name_bytes.decode("ascii", errors="replace"))
                parse(data, inner, cend)
            elif cid in (0x4D4D, 0x3D3D, 0x4100):
                parse(data, pos+6, cend)
            pos = cend

    parse(data, 0, len(data))
    
    print(f"\n=== {filepath} ({len(data)} bytes) ===")
    print(f"Objects: {objects}")
    print(f"Vertices: {len(vertices)}, Faces: {len(faces)}")
    if vertices:
        xs = [v[0] for v in vertices]
        ys = [v[1] for v in vertices]
        zs = [v[2] for v in vertices]
        print(f"X: {min(xs):.3f} to {max(xs):.3f} (span {max(xs)-min(xs):.3f})")
        print(f"Y: {min(ys):.3f} to {max(ys):.3f} (span {max(ys)-min(ys):.3f})")
        print(f"Z: {min(zs):.3f} to {max(zs):.3f} (span {max(zs)-min(zs):.3f})")
        for i, v in enumerate(vertices):
            print(f"  {i}: ({v[0]:.3f}, {v[1]:.3f}, {v[2]:.3f})")

for f in sys.argv[1:]:
    analyze_3ds(f)
