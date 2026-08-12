import struct
def bounds(filepath):
    with open(filepath, 'rb') as f: data = f.read()
    vertices = []
    def parse(data, start, end):
        pos = start
        while pos < end - 6:
            cid, clen = struct.unpack_from('<HI', data, pos)
            next_pos = pos + clen
            if cid == 0x4110:
                n = struct.unpack_from('<H', data, pos+6)[0]
                for i in range(n):
                    vertices.append(struct.unpack_from('<fff', data, pos+8+i*12))
            elif cid in [0x4D4D, 0x3D3D, 0x4000, 0x4100]:
                parse(data, pos+6 if cid != 0x4000 else pos+6 + len(data[pos+6:next_pos].split(b'\x00')[0]) + 1, next_pos)
            pos = next_pos
    parse(data, 0, len(data))
    if vertices:
        xs = [v[0] for v in vertices]
        ys = [v[1] for v in vertices]
        zs = [v[2] for v in vertices]
        print(f'{filepath} bounds: X({min(xs):.2f}, {max(xs):.2f}) Y({min(ys):.2f}, {max(ys):.2f}) Z({min(zs):.2f}, {max(zs):.2f})')
        print(f'Center: ({(min(xs)+max(xs))/2:.2f}, {(min(ys)+max(ys))/2:.2f}, {(min(zs)+max(zs))/2:.2f})')

bounds('Textures/dungeon02/torch1.3DS')
