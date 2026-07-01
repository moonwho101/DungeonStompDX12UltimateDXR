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

BOUNDING_BOXES = {
    'ROOM2': (-78, -158, 78, 158),
    'crossroads': (-38, 2, 198, 238),
    't_junction': (-38, 2, 198, 238),
    'left_corner': (-38, 2, 158, 198)
}

def get_world_bounds(name, ox, oz, rot):
    min_x, min_z, max_x, max_z = BOUNDING_BOXES[name]
    corners = [
        (min_x, min_z),
        (max_x, min_z),
        (min_x, max_z),
        (max_x, max_z)
    ]
    rotated = [rotate(cx, cz, rot) for cx, cz in corners]
    wx = [c[0] + ox for c in rotated]
    wz = [c[1] + oz for c in rotated]
    return min(wx), min(wz), max(wx), max(wz)

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
    print("Starting dungeon generation...")
    placed = []
    entities = []
    failed_exits = []
    entity_id_idx = 100
    
    # Start with a single room at some coordinate base
    print("Placing initial starting ROOM2...")
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
            'wdx': rd[0], 'wdz': rd[1],
            'source_name': 'ROOM2'
        })
        
    num_objects_to_place = 125
    
    # AABB collision detection to prevent overlapping
    def check_collision(nx, nz, n_name, n_rot):
        n_minx, n_minz, n_maxx, n_maxz = get_world_bounds(n_name, nx, nz, n_rot)
        for p in placed:
            p_minx, p_minz, p_maxx, p_maxz = get_world_bounds(p['name'], p['x'], p['z'], p['rot'])
            # Check for rectangle overlap
            if (n_minx < p_maxx and n_maxx > p_minx and
                n_minz < p_maxz and n_maxz > p_minz):
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
        source_name = O.get('source_name', 'ROOM2')
        
        placed_new = False
        
        if source_name != 'ROOM2':
            # Intersections and corners can only attach to ROOM2
            types = ['ROOM2']
        else:
            # ROOM2 can attach to anything
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
                        
                        # Apply AABB collision check
                        if not check_collision(Ox, Oz, cand_name, ang):
                            placed.append({
                                'name': cand_name,
                                'x': Ox,
                                'z': Oz,
                                'rot': ang
                            })
                            placed_new = True
                            print(f"Placed {cand_name} at ({Ox:.1f}, {Oz:.1f}) [Rot: {ang}]")
                            
                            if cand_name == 'ROOM2':
                                r = random.random()
                                ent_type = None
                                if r < 0.15:
                                    ent_type = 'POTION'
                                    entities.append({'type': 'POTION', 'name': '-1', 'x': Ox, 'y': -22.0, 'z': Oz, 'rot': 0, 'id': entity_id_idx, 'state': 0})
                                elif r < 0.30:
                                    ent_type = 'COIN'
                                    entities.append({'type': 'COIN', 'name': '-1', 'x': Ox, 'y': -185.0, 'z': Oz, 'rot': 0, 'id': entity_id_idx, 'state': 0})
                                elif r < 0.45:
                                    ent_type = 'GOBLIN'
                                    entities.append({'type': 'GOBLIN', 'name': 'goblin', 'x': Ox, 'y': 10.0, 'z': Oz, 'rot': 0, 'id': entity_id_idx, 'state': 0})
                                elif r < 0.55:
                                    ent_type = 'OGRE'
                                    entities.append({'type': 'OGRE', 'name': 'ogre', 'x': Ox, 'y': 10.0, 'z': Oz, 'rot': 0, 'id': entity_id_idx, 'state': 0})
                                
                                if ent_type:
                                    print(f"  -> Spawned {ent_type} in ROOM2")
                                    entity_id_idx += 1
                            
                            # Add its newly made exits
                            for i, other_ext in enumerate(cand_exits):
                                if i == ext_idx: continue
                                op = rotate(other_ext['pos'][0], other_ext['pos'][1], ang)
                                od = rotate_dir(other_ext['out'][0], other_ext['out'][1], ang)
                                open_exits.append({
                                    'wx': Ox + op[0],
                                    'wz': Oz + op[1],
                                    'wdx': od[0],
                                    'wdz': od[1],
                                    'source_name': cand_name
                                })
                        break 
        
        if not placed_new:
            # We couldn't place anything here due to collisions. Keep it to cap it later!
            failed_exits.append(O)
    
    open_exits.extend(failed_exits)
    
    # Build the set of exit positions that are already connected between two placed pieces.
    # This can happen when two different pieces generate exits at the same world position;
    # the first gets connected successfully, but the second fails (collision) and would 
    # otherwise be incorrectly walled.
    all_piece_exits = []
    for p in placed:
        for ext in OBJECTS[p['name']]:
            rp = rotate(ext['pos'][0], ext['pos'][1], p['rot'])
            rd = rotate_dir(ext['out'][0], ext['out'][1], p['rot'])
            all_piece_exits.append((round(p['x'] + rp[0]), round(p['z'] + rp[1]), rd[0], rd[1]))

    connected_positions = set()
    for (ex, ez, edx, edz) in all_piece_exits:
        for (ex2, ez2, edx2, edz2) in all_piece_exits:
            if ex == ex2 and ez == ez2 and edx == -edx2 and edz == -edz2:
                connected_positions.add((ex, ez))

    dead_end_exits = [o for o in open_exits if (round(o['wx']), round(o['wz'])) not in connected_positions]
    skipped = len(open_exits) - len(dead_end_exits)
    if skipped:
        print(f"  Skipping {skipped} exits that are already connected to placed pieces.")

    print(f"Generation loop completed. {len(placed)} tiles placed. Sealing {len(dead_end_exits)} open exits with dead-end walls...")
    
    # Cap only truly unconnected exits with a dead-end wall
    for open_ex in dead_end_exits:
        wx, wz = open_ex['wx'], open_ex['wz']
        wdx, wdz = open_ex['wdx'], open_ex['wdz']
        
        # We need the normal of the wall to face into the tunnel opposing the exit direction.
        # It's an east-west piece naturally. We apply an additional 180deg flip so the 
        # textured face points inwards instead of outwards.
        ndx, ndz = -wdx, -wdz
        wall_rot = 0
        if ndx == 0 and ndz == -1: wall_rot = 270
        elif ndx == 1 and ndz == 0: wall_rot = 0
        elif ndx == 0 and ndz == 1: wall_rot = 90
        elif ndx == -1 and ndz == 0: wall_rot = 180
        
        # If the wall is originally generated shifting halfway to its right,
        # We shift its local center 80 units to the 'right' relative to its facing normal.
        wx += ndz * 80
        wz += -ndx * 80
        
        entities.append({
            'type': 'wall',
            'name': 'cobblestone4',
            'x': wx,
            'y': 0.0,
            'z': wz,
            'rot': wall_rot,
            'id': entity_id_idx,
            'state': 0
        })
        entity_id_idx += 1

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
            
        for e in entities:
            if e['type'] == 'wall':
                f.write(f"OBJECT !wall0-240-160\n")
                f.write(f"CO_ORDINATES {e['x']:.6f} {e['y']:.6f} {e['z']:.6f}\n")
                f.write(f"ROT_ANGLE {e['rot']} 0 {e['name']} {e['id']} 0\n")
            else:
                f.write(f"OBJECT !monster1\n")
                f.write(f"CO_ORDINATES {e['x']:.6f} {e['y']:.6f} {e['z']:.6f}\n")
                f.write(f"ROT_ANGLE {e['rot']} {e['type']} {e['name']} {e['id']} {e['state']}\n")
            
        f.write("END_FILE\n")

    num_walls = sum(1 for e in entities if e['type'] == 'wall')
    num_mobs = len(entities) - num_walls
    print(f"Successfully saved to bin/level1.map! ({len(placed)} tiles, {num_mobs} entities, {num_walls} walls)")

if __name__ == '__main__':
    generate()