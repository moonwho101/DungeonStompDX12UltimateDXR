import os
import random
import math

DIR_N = (0, 1)
DIR_S = (0, -1)
DIR_W = (-1, 0)
DIR_E = (1, 0)

OBJECTS = {
    'ROOM2': [
        {'pos': (0, -160), 'out': DIR_S},
        {'pos': (0, 160), 'out': DIR_N}
    ],
    'crossroads': [
        {'pos': (80, 0), 'out': DIR_S},
        {'pos': (80, 240), 'out': DIR_N},
        {'pos': (-40, 120), 'out': DIR_W},
        {'pos': (200, 120), 'out': DIR_E}
    ],
    't_junction': [
        {'pos': (80, 0), 'out': DIR_S},
        {'pos': (-40, 120), 'out': DIR_W},
        {'pos': (200, 120), 'out': DIR_E}
    ],
    'left_corner': [
        {'pos': (80, 0), 'out': DIR_S},
        {'pos': (-40, 120), 'out': DIR_W}
    ]
}

def rotate(x, z, angle):
    # Depending on coordinate handedness, DX12 matches CCW or CW.
    # We found that 90 degrees CCW maps cleanly for our derived offsets.
    if angle == 0: return x, z
    if angle == 90: return -z, x
    if angle == 180: return -x, -z
    if angle == 270: return z, -x
    return x, z

def rotate_dir(dx, dz, angle):
    return rotate(dx, dz, angle)

def generate():
    placed = []
    
    # Start with a single room at some coordinate base
    placed.append({
        'name': 'ROOM2',
        'x': 2000,
        'z': 1000,
        'rot': 0
    })
    
    open_exits = []
    # Add exits for the first room
    for ext in OBJECTS['ROOM2']:
        rp = rotate(ext['pos'][0], ext['pos'][1], 0)
        rd = rotate_dir(ext['out'][0], ext['out'][1], 0)
        wx = 2000 + rp[0]
        wz = 1000 + rp[1]
        open_exits.append({
            'wx': wx, 'wz': wz,
            'wdx': rd[0], 'wdz': rd[1]
        })
        
    num_objects_to_place = 50
    
    # A rough collision detection using simple center thresholds
    def check_collision(nx, nz, n_name):
        for p in placed:
            dx = p['x'] - nx
            dz = p['z'] - nz
            dist = math.sqrt(dx*dx + dz*dz)
            # Threshold distance. Since we are grid snapping tunnels, 
            # objects sharing exits don't map centers exactly linearly,
            # but they should be at least ~140 units apart.
            if dist < 140:
                return True
        return False

    for _ in range(num_objects_to_place):
        if not open_exits:
            break
        
        # Pop random open exit
        exit_idx = random.randint(0, len(open_exits)-1)
        O = open_exits.pop(exit_idx)
        wx, wz = O['wx'], O['wz']
        wdx, wdz = O['wdx'], O['wdz']
        
        placed_new = False
        
        types = list(OBJECTS.keys())
        random.shuffle(types)
        
        for cand_name in types:
            if placed_new: break
            cand_exits = OBJECTS[cand_name]
            
            indices = list(range(len(cand_exits)))
            random.shuffle(indices)
            
            for ext_idx in indices:
                if placed_new: break
                loc_ext = cand_exits[ext_idx]
                lx, lz = loc_ext['pos']
                ldx, ldz = loc_ext['out']
                
                rots = [0, 90, 180, 270]
                random.shuffle(rots)
                for ang in rots:
                    rdx, rdz = rotate_dir(ldx, ldz, ang)
                    # We check if the rotated out normal exactly opposes our open exit normal
                    if rdx == -wdx and rdz == -wdz:
                        rp = rotate(lx, lz, ang)
                        Ox = wx - rp[0]
                        Oz = wz - rp[1]
                        
                        # Apply rough circle collision distance
                        if not check_collision(Ox, Oz, cand_name):
                            placed.append({
                                'name': cand_name,
                                'x': Ox,
                                'z': Oz,
                                'rot': ang
                            })
                            placed_new = True
                            
                            # Add its newly made exits
                            for i, other_ext in enumerate(cand_exits):
                                if i == ext_idx: continue
                                op = rotate(other_ext['pos'][0], other_ext['pos'][1], ang)
                                od = rotate_dir(other_ext['out'][0], other_ext['out'][1], ang)
                                open_exits.append({
                                    'wx': Ox + op[0],
                                    'wz': Oz + op[1],
                                    'wdx': od[0],
                                    'wdz': od[1]
                                })
                        break 
    
    out_file = os.path.join(os.path.dirname(__file__), '..', 'bin', 'level1.map')
    # Save safely
    with open(out_file, 'w') as f:
        # Include start position 
        f.write("OBJECT startpos\n")
        f.write("CO_ORDINATES 2000.000000 0.000000 1000.000000\n")
        f.write("ROT_ANGLE 0\n")
        
        for p in placed:
            f.write(f"OBJECT {p['name']}\n")
            f.write(f"CO_ORDINATES {p['x']:.6f} 0.000000 {p['z']:.6f}\n")
            f.write(f"ROT_ANGLE {p['rot']}\n")
            
        f.write("END_FILE\n")

if __name__ == '__main__':
    generate()
    print("Dungeon generated at bin/level1.map.")