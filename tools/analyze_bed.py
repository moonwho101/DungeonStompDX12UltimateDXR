import struct

with open(r'c:\github\DungeonStompDX12UltimateDXR\Models\3ds\bed.3DS', 'rb') as f:
    data = f.read()

vertices = []
faces = []

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
            print("Object:", name_bytes.decode("ascii", errors="replace"))
            parse(data, inner, cend)
        elif cid in (0x4D4D, 0x3D3D, 0x4100):
            parse(data, pos+6, cend)
        pos = cend

parse(data, 0, len(data))
xs = [v[0] for v in vertices]
ys = [v[1] for v in vertices]
zs = [v[2] for v in vertices]
print(f"Vertices: {len(vertices)}, Faces: {len(faces)}")
print(f"X: {min(xs):.3f} to {max(xs):.3f} (span {max(xs)-min(xs):.3f})")
print(f"Y: {min(ys):.3f} to {max(ys):.3f} (span {max(ys)-min(ys):.3f})")
print(f"Z: {min(zs):.3f} to {max(zs):.3f} (span {max(zs)-min(zs):.3f})")
print("Vertices:")
for i, v in enumerate(vertices):
    print(f"  {i}: ({v[0]:.3f}, {v[1]:.3f}, {v[2]:.3f})")
print("Faces:")
for i, fc in enumerate(faces):
    print(f"  {i}: {fc}")
