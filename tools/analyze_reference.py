import math

def rotate(x, z, angle):
    if angle == 0:   return x, z
    if angle == 90:  return -z, x
    if angle == 180: return -x, -z
    if angle == 270: return z, -x
    return x, z

# Parse the reference map
lines = open('../bin/level1tile02corner.map').readlines()

pieces = []
i = 0
while i < len(lines):
    line = lines[i].strip()
    if line.startswith('OBJECT ') and not line.startswith('OBJECT startpos'):
        obj_type = line.split()[1]
        if i+1 < len(lines) and 'CO_ORDINATES' in lines[i+1]:
            parts = lines[i+1].strip().split()
            x, y, z = float(parts[1]), float(parts[2]), float(parts[3])
            if i+2 < len(lines) and 'ROT_ANGLE' in lines[i+2]:
                rot = int(lines[i+2].strip().split()[1])
                pieces.append({'type': obj_type, 'x': x, 'y': y, 'z': z, 'rot': rot})
    i += 1

print('Pieces in reference map:')
print('-' * 70)
for i, p in enumerate(pieces):
    print('%d: %-12s at (%7.1f, %4.0f, %7.1f) rot %3d' % 
          (i, p['type'], p['x'], p['y'], p['z'], p['rot']))

# Define exit positions for each type (local coords)
CORRIDOR_EXITS = [(-360, 0), (360, 0)]
CROSSING_EXITS = [(0, -360), (360, 0)]

print('\n\nAnalyzing connections:')
print('=' * 70)

for i, p1 in enumerate(pieces):
    for j, p2 in enumerate(pieces):
        if i >= j: continue
        
        # Get exits for each piece
        if p1['type'] == 'CORRIDOR01':
            exits1 = CORRIDOR_EXITS
        else:
            exits1 = CROSSING_EXITS
            
        if p2['type'] == 'CORRIDOR01':
            exits2 = CORRIDOR_EXITS
        else:
            exits2 = CROSSING_EXITS
        
        # Check all exit pairs
        for ex1 in exits1:
            for ex2 in exits2:
                # Rotate and position exits
                wx1, wz1 = rotate(ex1[0], ex1[1], p1['rot'])
                wx1 += p1['x']
                wz1 += p1['z']
                
                wx2, wz2 = rotate(ex2[0], ex2[1], p2['rot'])
                wx2 += p2['x']
                wz2 += p2['z']
                
                # Check if close
                dx = wx1 - wx2
                dz = wz1 - wz2
                dist = math.sqrt(dx*dx + dz*dz)
                
                if dist < 100:  # Within 100 units = likely a connection
                    print('\n%s[%d] exit %s (rot %d) -> %s[%d] exit %s (rot %d)' % 
                          (p1['type'], i, ex1, p1['rot'], p2['type'], j, ex2, p2['rot']))
                    print('  Exit 1 world: (%.1f, %.1f)' % (wx1, wz1))
                    print('  Exit 2 world: (%.1f, %.1f)' % (wx2, wz2))
                    print('  Delta: dx=%.1f, dz=%.1f, dist=%.1f' % (dx, dz, dist))
                    print('  Center-to-center: dx=%.1f, dz=%.1f' % 
                          (p1['x'] - p2['x'], p1['z'] - p2['z']))
