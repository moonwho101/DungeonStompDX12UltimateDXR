import os
import random
import math
import argparse

DIR_N = (0, 1)
DIR_S = (0, -1)
DIR_W = (-1, 0)
DIR_E = (1, 0)

# NEW OBJECTS - CORRIDOR01 and CROSSING01
OBJECTS = {
    'CORRIDOR01': [
        {'pos': (-360, 0), 'out': DIR_W},
        {'pos': (360, 0), 'out': DIR_E}
    ],
    'CROSSING01': [
        {'pos': (0, -360), 'out': DIR_S, 'y': 30},
        {'pos': (360, 0), 'out': DIR_E, 'y': 30}
    ]
}

# NEW BOUNDING BOXES based on TRITEX floor dimensions
BOUNDING_BOXES = {
    'CORRIDOR01':   (-100, -360, 100, 360),
    'CROSSING01':   (-100, -100, 360, 360)
}

# PRE-DEFINED FLOOR SLOTS FOR ROOM ITEMS (to avoid collisions)
# FLOOR SLOTS for new objects
FLOOR_SLOTS = {
    'CORRIDOR01': [
        (0.0, -200.0),
        (0.0, -100.0),
        (0.0, 0.0),
        (0.0, 100.0),
        (0.0, 200.0)
    ],
    'CROSSING01': [
        (0.0, 0.0),
        (100.0, 100.0),
        (200.0, 100.0),
        (100.0, 200.0),
        (200.0, 200.0)
    ]
}


def get_world_bounds(name, ox, oz, rot):
    min_x, min_z, max_x, max_z = BOUNDING_BOXES[name]
    corners = [(min_x, min_z), (max_x, min_z), (min_x, max_z), (max_x, max_z)]
    rotated = [rotate(cx, cz, rot) for cx, cz in corners]
    wx = [c[0] + ox for c in rotated]
    wz = [c[1] + oz for c in rotated]
    return min(wx), min(wz), max(wx), max(wz)

def rotate(x, z, angle):
    if angle == 0:   return x, z
    if angle == 90:  return -z, x
    if angle == 180: return -x, -z
    if angle == 270: return z, -x
    return x, z

def rotate_dir(dx, dz, angle):
    return rotate(dx, dz, angle)

def _prefer_exit_index(open_exits, placed, prefer_loop_chance=0.20):
    """
    Heuristic: sometimes prefer exits that are near existing pieces to create loops.
    Returns an index into open_exits or None to pick randomly.
    """
    if not placed:
        return None
    if random.random() >= prefer_loop_chance:
        return None
    best_idx = None
    best_score = -1e9
    for i, ex in enumerate(open_exits):
        score = 0.0
        for p in placed:
            dx = ex['wx'] - p['x']
            dz = ex['wz'] - p['z']
            dist = math.hypot(dx, dz)
            # closer pieces increase score; very close strongly preferred
            score += max(0, 2000 - dist)
        if score > best_score:
            best_score = score
            best_idx = i
    return best_idx

def generate(start_x=5200, start_z=2600, seed=None, num_objects_to_place=350):
    """
    Enhanced generator that uses only the original OBJECTS and BOUNDING_BOXES.
    - seed: optional RNG seed for reproducible results
    - num_objects_to_place: how many pieces to attempt to place
    """
    if seed is not None:
        random.seed(seed)

    print("Starting dungeon generation...")
    placed = []
    entities = []
    failed_exits = []
    entity_id_idx = 100

    # Start with a single corridor at some coordinate base
    print("Placing initial starting CORRIDOR01...")
    placed.append({'name': 'CORRIDOR01', 'x': start_x, 'y': 0.0, 'z': start_z, 'rot': 0})

    open_exits = []
    # Add exits for the first room
    for ext in OBJECTS['CORRIDOR01']:
        rp = rotate(ext['pos'][0], ext['pos'][1], 0)
        rd = rotate_dir(ext['out'][0], ext['out'][1], 0)
        open_exits.append({
            'wx': start_x + rp[0],
            'wy': 0.0 + ext.get('y', 0),
            'wz': start_z + rp[1],
            'wdx': rd[0], 'wdz': rd[1],
            'source_name': 'ROOM2'
        })

    def check_collision(nx, nz, n_name, n_rot):
        n_minx, n_minz, n_maxx, n_maxz = get_world_bounds(n_name, nx, nz, n_rot)
        for p in placed:
            p_minx, p_minz, p_maxx, p_maxz = get_world_bounds(p['name'], p['x'], p['z'], p['rot'])
            if (n_minx < p_maxx and n_maxx > p_minx and
                    n_minz < p_maxz and n_maxz > p_minz):
                return True
        return False

    for _ in range(num_objects_to_place):
        if not open_exits:
            break

        # sometimes prefer exits near existing pieces to create loops
        preferred_idx = _prefer_exit_index(open_exits, placed, prefer_loop_chance=0.18)
        if preferred_idx is not None:
            exit_idx = preferred_idx
        else:
            exit_idx = random.randint(0, len(open_exits) - 1)

        O = open_exits.pop(exit_idx)
        wx, wy, wz = O['wx'], O['wy'], O['wz']
        wdx, wdz = O['wdx'], O['wdz']
        source_name = O.get('source_name', 'ROOM2')

        placed_new = False

        # CROSSING01 can only attach to CORRIDOR01
        if source_name == 'CROSSING01':
            types = ['CORRIDOR01']
        else:
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
                    if rdx == -wdx and rdz == -wdz:
                        rp = rotate(lx, lz, ang)
                        Ox = wx - rp[0]
                        # Piece centre y: exit world-y minus the local y-offset of this exit
                        Oy = wy - loc_ext.get('y', 0)
                        # Clamp to max y=0 (no levels above ground)
                        Oy = min(0, Oy)
                        Oz = wz - rp[1]

                        if not check_collision(Ox, Oz, cand_name, ang):
                            placed.append({'name': cand_name, 'x': Ox, 'y': Oy, 'z': Oz, 'rot': ang})
                            placed_new = True
                            print(f"Placed {cand_name} at ({Ox:.1f}, {Oy:.1f}, {Oz:.1f}) [Rot: {ang}]")

                            # Initialize floor slots for this room
                            room_slots = list(FLOOR_SLOTS[cand_name]) if cand_name in FLOOR_SLOTS else []
                            random.shuffle(room_slots)

                            # --- Spotlights and lamp posts (depth-aware color) ---
                            if random.random() < 0.25:
                                s_y = Oy + random.choice([200.0, 300.0, 400.0])

                                hue = random.random()
                                sat = random.uniform(0.4, 1.0)
                                val = random.uniform(0.3, 0.9)

                                import colorsys
                                r, g, b = colorsys.hsv_to_rgb(hue, sat, val)
                                color = (r, g, b)

                                entities.append({'type': 'lamp_post', 'x': Ox, 'y': s_y, 'z': Oz, 'rot': 0, 'id': entity_id_idx, 'state': 0, 'name': ''})

                                # Compute and store deterministic light direction now (not at write time)
                                dx = random.uniform(-0.4, 0.4)
                                dz = random.uniform(-0.4, 0.4)
                                dy = -1.0
                                length = math.sqrt(dx*dx + dy*dy + dz*dz)
                                dx /= length
                                dy /= length
                                dz /= length

                                entities.append({'type': 'LIGHT_SOURCE','name': 'Spotlight','x': Ox,'y': s_y,'z': Oz,'rot': 0,'id': entity_id_idx,'state': 0,'color': color,'dir': (dx, dy, dz)})
                                print(f"  -> Spawned Spotlight overhead")

                            # --- Propagate exits; carry the y-offset of each outgoing exit ---
                            for i, other_ext in enumerate(cand_exits):
                                if i == ext_idx: continue
                                op = rotate(other_ext['pos'][0], other_ext['pos'][1], ang)
                                od = rotate_dir(other_ext['out'][0], other_ext['out'][1], ang)
                                open_exits.append({
                                    'wx': Ox + op[0],
                                    'wy': Oy + other_ext.get('y', 0),
                                    'wz': Oz + op[1],
                                    'wdx': od[0], 'wdz': od[1],
                                    'source_name': cand_name
                                })
                        break

        if not placed_new:
            failed_exits.append(O)

    open_exits.extend(failed_exits)

    # Determine which exits are already satisfied by two adjacent pieces
    all_piece_exits = []
    for p in placed:
        for ext in OBJECTS[p['name']]:
            rp = rotate(ext['pos'][0], ext['pos'][1], p['rot'])
            rd = rotate_dir(ext['out'][0], ext['out'][1], p['rot'])
            ey = round(p.get('y', 0) + ext.get('y', 0))
            all_piece_exits.append((round(p['x'] + rp[0]), ey, round(p['z'] + rp[1]), rd[0], rd[1]))

    connected_positions = set()
    for (ex, ey, ez, edx, edz) in all_piece_exits:
        for (ex2, ey2, ez2, edx2, edz2) in all_piece_exits:
            if ex == ex2 and ey == ey2 and ez == ez2 and edx == -edx2 and edz == -edz2:
                connected_positions.add((ex, ey, ez))

    dead_end_exits = [
        o for o in open_exits
        if (round(o['wx']), round(o['wy']), round(o['wz'])) not in connected_positions
    ]
    skipped = len(open_exits) - len(dead_end_exits)
    if skipped:
        print(f"  Skipping {skipped} exits that are already connected to placed pieces.")

    print(f"Generation loop completed. {len(placed)} tiles placed. "
          f"Sealing {len(dead_end_exits)} open exits with dead-end walls...")

    for open_ex in dead_end_exits:
        wx, wy, wz = open_ex['wx'], open_ex['wy'], open_ex['wz']
        wdx, wdz = open_ex['wdx'], open_ex['wdz']
        ndx, ndz = -wdx, -wdz
        wall_rot = 0
        if   ndx == 0 and ndz == -1: wall_rot = 270
        elif ndx == 1 and ndz ==  0: wall_rot = 0
        elif ndx == 0 and ndz ==  1: wall_rot = 90
        elif ndx == -1 and ndz == 0: wall_rot = 180
        wx += ndz * 80
        wz += -ndx * 80
        entities.append({
            'type': 'wall', 'name': 'cobblestone4',
            'x': wx, 'y': wy, 'z': wz,
            'rot': wall_rot, 'id': entity_id_idx, 'state': 0
        })
        entity_id_idx += 1

    # --- Add a single teleport exit at the deepest dungeon location ---
    if placed:
        deepest_piece = min(placed, key=lambda p: p.get('y', 0.0))
        tx = deepest_piece['x']
        ty = deepest_piece['y']
        tz = deepest_piece['z']
        deepest_name = deepest_piece['name']
        deepest_rot = deepest_piece['rot']

        # To avoid collisions with the teleporter, move any entities in the deepest piece
        # away from the center (0,0) to another free floor slot if possible.
        if deepest_name in FLOOR_SLOTS:
            used_local_slots = set()
            for e in entities:
                dx = e['x'] - tx
                dz = e['z'] - tz
                lx, lz = rotate(dx, dz, (360 - deepest_rot) % 360)
                used_local_slots.add((round(lx), round(lz)))
            
            # Find an unused floor slot
            free_slots = [slot for slot in FLOOR_SLOTS[deepest_name] if slot != (0.0, 0.0) and (round(slot[0]), round(slot[1])) not in used_local_slots]
            if free_slots:
                target_slot = random.choice(free_slots)
                rx, rz = rotate(target_slot[0], target_slot[1], deepest_rot)
                new_wx = tx + rx
                new_wz = tz + rz
                
                # Relocate any entity that was at (tx, tz) to the new slot
                for e in entities:
                    if abs(e['x'] - tx) < 1.0 and abs(e['z'] - tz) < 1.0 and e['type'] not in ('circle', 'spiral', '!flarenohit', 'lamp_post', 'LIGHT_SOURCE'):
                        e['x'] = new_wx
                        e['z'] = new_wz
                        print(f"  -> Relocating entity {e['type']} in deepest room from center to slot {target_slot}")


        # Teleport base circle (slightly below ground)
        entities.append({
            'type': 'circle',
            'x': tx, 'y': ty - 41.0, 'z': tz,
            'rot': 0,
            'name': '0',
            'id': entity_id_idx,
            'state': 2
        })
        entity_id_idx += 1

        # Teleport spiral
        entities.append({
            'type': 'spiral',
            'x': tx, 'y': ty, 'z': tz,
            'rot': 0,
            'name': '-1',
            'id': entity_id_idx,
            'state': 3
        })
        entity_id_idx += 1

        # Teleport flare burst
        entities.append({
            'type': '!flarenohit',
            'x': tx, 'y': ty, 'z': tz,
            'rot': 0,
            'name': 'flare@1',
            'id': entity_id_idx,
            'state': 2
        })

        entity_id_idx += 1

        print(f"  -> Teleport exit added at deepest dungeon location ({tx:.1f}, {ty:.1f}, {tz:.1f})")


    out_file = os.path.join(os.path.dirname(__file__), '..', 'bin', 'level1.map')
    with open(out_file, 'w') as f:
        f.write("OBJECT startpos\n")
        f.write(f"CO_ORDINATES {start_x:.6f} 0.000000 {start_z:.6f}\n")
        f.write("ROT_ANGLE 0\n")

        for p in placed:
            if p['name'] in ('left_curve', 'right_curve'):
                continue
            f.write(f"OBJECT {p['name']}\n")
            f.write(f"CO_ORDINATES {p['x']:.6f} {p.get('y', 0.0):.6f} {p['z']:.6f}\n")
            f.write(f"ROT_ANGLE {p['rot']}\n")

        for e in entities:
            t = e['type']
            if t == 'wall':
                f.write(f"OBJECT !wall0-240-160\n")
                f.write(f"CO_ORDINATES {e['x']:.6f} {e['y']:.6f} {e['z']:.6f}\n")
                f.write(f"ROT_ANGLE {e['rot']} 0 {e['name']} {e['id']} 0\n")
            elif t == 'torch':
                f.write(f"OBJECT torch\n")
                f.write(f"CO_ORDINATES {e['x']:.6f} {e['y']:.6f} {e['z']:.6f}\n")
                f.write(f"ROT_ANGLE {e['rot']}\n")
            elif t == '!flamesnohit':
                f.write(f"OBJECT !flamesnohit\n")
                f.write(f"CO_ORDINATES {e['x']:.6f} {e['y']:.6f} {e['z']:.6f}\n")
                f.write(f"ROT_ANGLE {e['rot']} 0 {e['name']} {e['state']} 0\n")
            elif t == 'lamp_post':
                f.write(f"OBJECT lamp_post\n")
                f.write(f"CO_ORDINATES {e['x']:.6f} {e['y']:.6f} {e['z']:.6f}\n")
                f.write(f"ROT_ANGLE {e['rot']}\n")

            elif t == 'LIGHT_SOURCE':
                # Use the direction stored at spawn time for determinism
                dx, dy, dz = e.get('dir', (0.0, -1.0, 0.0))

                light_color = e.get('color', (1.0, 1.0, 1.0))
                f.write(
                    f"LIGHT_SOURCE {e['name']} POS {e['x']:.6f} {e['y']:.6f} {e['z']:.6f}"
                    f" DIR {dx:.6f} {dy:.6f} {dz:.6f} "
                    f"COLOUR {light_color[0]:.6f} {light_color[1]:.6f} {light_color[2]:.6f}\n"
                )
            elif t in ('dframe', 'curve'):
                f.write(f"OBJECT {t}\n")
                f.write(f"CO_ORDINATES {e['x']:.6f} {e['y']:.6f} {e['z']:.6f}\n")
                f.write(f"ROT_ANGLE {e['rot']}\n")
            elif t.startswith('door'):
                f.write(f"OBJECT {t}\n")
                f.write(f"CO_ORDINATES {e['x']:.6f} {e['y']:.6f} {e['z']:.6f}\n")
                # preserve state field (secret doors use state=2)
                f.write(f"ROT_ANGLE {e['rot']} {e['state']}\n")
            elif t == 'slope_stairs':
                f.write(f"OBJECT slope_stairs\n")
                f.write(f"CO_ORDINATES {e['x']:.6f} {e['y']:.6f} {e['z']:.6f}\n")
                f.write(f"ROT_ANGLE {e['rot']}\n")
            elif t in ('left_curve_road', 'right_curve_road'):
                f.write(f"OBJECT {t}\n")
                f.write(f"CO_ORDINATES {e['x']:.6f} {e['y']:.6f} {e['z']:.6f}\n")
                f.write(f"ROT_ANGLE {e['rot']}\n")
            elif t in ('!flarenohit'):                
                f.write(f"OBJECT {t}\n")
                f.write(f"CO_ORDINATES {e['x']:.6f} {e['y']:.6f} {e['z']:.6f}\n")
                f.write(f"ROT_ANGLE {e['rot']} 3 {e.get('name','')} {e.get('id',0)} {e.get('state',0)}\n")
            else:
                f.write(f"OBJECT !monster1\n")
                f.write(f"CO_ORDINATES {e['x']:.6f} {e['y']:.6f} {e['z']:.6f}\n")
                f.write(f"ROT_ANGLE {e['rot']} {t} {e.get('name','')} {e.get('id',0)} {e.get('state',0)}\n")

        f.write("END_FILE\n")

    num_walls = sum(1 for e in entities if e['type'] == 'wall')
    num_mobs  = len(entities) - num_walls
    print(f"Successfully saved to bin/level1.map! ({len(placed)} tiles, {num_mobs} entities, {num_walls} walls)")
    return len(placed)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", type=int, default=None, help="Seed for deterministic dungeon generation")
    parser.add_argument("--x", type=int, default=5200)
    parser.add_argument("--z", type=int, default=2600)
    parser.add_argument("--count", type=int, default=350)
    args = parser.parse_args()

    generate(start_x=args.x, start_z=args.z, seed=args.seed, num_objects_to_place=args.count)
