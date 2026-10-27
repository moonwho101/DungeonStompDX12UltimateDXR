// generator.js - Procedural Dungeon Level Generator converted from tools/generate_dungeonNewObjects.py

export const DIR_N = [0, 1];
export const DIR_S = [0, -1];
export const DIR_W = [-1, 0];
export const DIR_E = [1, 0];

export const OBJECTS = {
    'CORRIDOR01': [
        { 'pos': [-368, 0], 'out': DIR_W },
        { 'pos': [355, 0], 'out': DIR_E }
    ],
    'CORRIDOR02': [
        { 'pos': [-284, 0], 'out': DIR_W, 'y': 72 - 74.5 },
        { 'pos': [295, 0], 'out': DIR_E, 'y': 414 - 74.5 }
    ],
    'CORRIDOR03': [
        { 'pos': [-180, 0], 'out': DIR_W, 'y': 0 },
        { 'pos': [180, 0], 'out': DIR_E, 'y': 0 }
    ],
    'CROSSING01': [
        { 'pos': [-60, -340], 'out': DIR_S, 'y': 0 },
        { 'pos': [365, 80], 'out': DIR_E, 'y': 0 }
    ],
    'CROSSING02': [
        { 'pos': [0, 384], 'out': DIR_N, 'y': 0 },
        { 'pos': [0, -369], 'out': DIR_S, 'y': 0 },
        { 'pos': [-406, 0], 'out': DIR_W, 'y': 0 },
        { 'pos': [405, 0], 'out': DIR_E, 'y': 0 }
    ],
    'CROSSING03': [
        { 'pos': [0, -334], 'out': DIR_S, 'y': 0 },
        { 'pos': [-412, 20], 'out': DIR_W, 'y': 0 },
        { 'pos': [399, 20], 'out': DIR_E, 'y': 0 }
    ],
    'ROOM02': [
        { 'pos': [0, -173], 'out': DIR_S, 'y': 0 }
    ],
    'ROOM05': [
        { 'pos': [455, -305 - 145], 'out': DIR_E, 'y': 0 }
    ],
    'ROOM06': [
        { 'pos': [0, -311], 'out': DIR_S, 'y': 0 }
    ]
};

export const BOUNDING_BOXES_FULL = {
    'CORRIDOR01': [-372, -297, 358, 303],
    'CORRIDOR02': [-288, -298, 298, 302],
    'CORRIDOR03': [-185, -299, 182, 301],
    'CROSSING01': [-447, -453, 482, 413],
    'CROSSING02': [-486, -458, 491, 475],
    'CROSSING03': [-492, -422, 485, 420],
    'ROOM02':     [-255, -173, 249, 165],
    'ROOM06':     [-297, -338, 294, 339],
    'ROOM05':     [-505, -831, 517, 839]
};

export const FLOOR_SLOTS = {
    'CORRIDOR01': [
        [0.0, -100.0],
        [0.0, 0.0],
        [0.0, 100.0]
    ],
    'CORRIDOR02': [
        [-20.0, -200.0],
        [-20.0, -100.0],
        [-20.0, 0.0],
        [-20.0, 100.0],
        [-20.0, 200.0]
    ],
    'CORRIDOR03': [
        [0.0, -100.0],
        [0.0, 0.0],
        [0.0, 100.0]
    ],
    'CROSSING01': [
        [0.0, 0.0],
        [0.0, -100.0],
        [100.0, 0.0]
    ],
    'CROSSING02': [
        [0.0, 0.0],
        [0.0, -100.0],
        [100.0, 0.0]
    ],
    'CROSSING03': [
        [0.0, 0.0],
        [0.0, -100.0],
        [100.0, 0.0]
    ],
    'ROOM02': [
        [0.0, 0.0],
        [-120.0, -60.0],
        [120.0, -60.0],
        [-120.0, 60.0],
        [120.0, 60.0]
    ],
    'ROOM06': [
        [0.0, 0.0],
        [-140.0, -120.0],
        [140.0, -120.0],
        [-140.0, 120.0],
        [140.0, 120.0],
        [0.0, -200.0],
        [0.0, 200.0]
    ],
    'ROOM05': [
        [0.0, 0.0],
        [-150.0, -200.0],
        [150.0, -200.0],
        [-150.0, 200.0],
        [150.0, 200.0],
        [0.0, -300.0],
        [0.0, 300.0]
    ]
};

export function rotate(x, z, angle) {
    if (angle === 0)   return [x, z];
    if (angle === 90)  return [-z, x];
    if (angle === 180) return [-x, -z];
    if (angle === 270) return [z, -x];
    return [x, z];
}

export function rotate_dir(dx, dz, angle) {
    return rotate(dx, dz, angle);
}

export function get_world_bounds_full(name, ox, oz, rot) {
    const [min_x, min_z, max_x, max_z] = BOUNDING_BOXES_FULL[name];
    const corners = [[min_x, min_z], [max_x, min_z], [min_x, max_z], [max_x, max_z]];
    const rotated = corners.map(([cx, cz]) => rotate(cx, cz, rot));
    const wx = rotated.map(c => c[0] + ox);
    const wz = rotated.map(c => c[1] + oz);
    return [Math.min(...wx), Math.min(...wz), Math.max(...wx), Math.max(...wz)];
}

function createRng(seed) {
    if (seed === undefined || seed === null) {
        return Math.random;
    }
    let s = (seed ^ 0xDEADBEEF) >>> 0;
    return function() {
        s = (s + 0x6D2B79F5) | 0;
        let t = Math.imul(s ^ (s >>> 15), 1 | s);
        t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
}

function _prefer_exit_index(open_exits, placed, prefer_loop_chance = 0.20, rng = Math.random) {
    if (!placed || placed.length === 0) return null;
    if (rng() >= prefer_loop_chance) return null;
    let best_idx = null;
    let best_score = -1e9;
    for (let i = 0; i < open_exits.length; i++) {
        const ex = open_exits[i];
        let score = 0.0;
        for (let j = 0; j < placed.length; j++) {
            const p = placed[j];
            const dx = ex.wx - p.x;
            const dz = ex.wz - p.z;
            const dist = Math.hypot(dx, dz);
            score += Math.max(0, 2000 - dist);
        }
        if (score > best_score) {
            best_score = score;
            best_idx = i;
        }
    }
    return best_idx;
}

function hsvToRgb(h, s, v) {
    let r = 0, g = 0, b = 0;
    let i = Math.floor(h * 6);
    let f = h * 6 - i;
    let p = v * (1 - s);
    let q = v * (1 - f * s);
    let t = v * (1 - (1 - f) * s);
    switch (i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        case 5: r = v; g = p; b = q; break;
    }
    return [r, g, b];
}

function shuffle(arr, rng) {
    for (let i = arr.length - 1; i > 0; i--) {
        const j = Math.floor(rng() * (i + 1));
        const temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
    return arr;
}

function sample(arr, k, rng) {
    const copy = arr.slice();
    shuffle(copy, rng);
    return copy.slice(0, Math.min(k, copy.length));
}

export function generateDungeon({ startX = 5200, startZ = 2600, seed = null, numObjectsToPlace = 350 } = {}) {
    const rng = createRng(seed);

    const randint = (a, b) => Math.floor(rng() * (b - a + 1)) + a;
    const choice = (arr) => arr[Math.floor(rng() * arr.length)];
    const uniform = (a, b) => a + rng() * (b - a);

    console.log("Starting dungeon generation...");
    const placed = [];
    const entities = [];
    const failed_exits = [];
    let entity_id_idx = 100;

    console.log("Placing initial starting CORRIDOR01...");
    const start_bounds = get_world_bounds_full('CORRIDOR01', startX, startZ, 0);
    const start_piece = { 'name': 'CORRIDOR01', 'x': startX, 'y': 0.0, 'z': startZ, 'rot': 0, 'bounds': start_bounds, 'used_slots': new Set(['0,0']) };
    placed.push(start_piece);

    // Place start circle (without spiral/teleport) at startpos center
    entities.push({
        'type': 'circle',
        'x': startX, 'y': -41.0 + 40.0, 'z': startZ,
        'rot': 0, 'name': '0', 'id': entity_id_idx, 'state': 2
    });
    entity_id_idx++;

    const open_exits = [];
    for (const ext of OBJECTS['CORRIDOR01']) {
        const rp = rotate(ext.pos[0], ext.pos[1], 0);
        const rd = rotate_dir(ext.out[0], ext.out[1], 0);
        open_exits.push({
            'wx': startX + rp[0],
            'wy': 0.0 + (ext.y || 0),
            'wz': startZ + rp[1],
            'wdx': rd[0], 'wdz': rd[1],
            'source_name': 'CORRIDOR01',
            'source_piece': start_piece
        });
    }

    function check_collision(nx, nz, n_name, n_rot, attach_ex) {
        const [n_minx, n_minz, n_maxx, n_maxz] = get_world_bounds_full(n_name, nx, nz, n_rot);
        const source_piece = attach_ex.source_piece;

        for (let i = 0; i < placed.length; i++) {
            const p = placed[i];
            if (p === source_piece) continue;

            const [p_minx, p_minz, p_maxx, p_maxz] = p.bounds;
            if (n_minx < p_maxx && n_maxx > p_minx &&
                n_minz < p_maxz && n_maxz > p_minz) {
                return true;
            }
        }
        return false;
    }

    for (let step = 0; step < numObjectsToPlace; step++) {
        if (open_exits.length === 0) break;

        const preferred_idx = _prefer_exit_index(open_exits, placed, 0.18, rng);
        let exit_idx;
        if (preferred_idx !== null) {
            exit_idx = preferred_idx;
        } else {
            exit_idx = Math.floor(rng() * open_exits.length);
        }

        const O = open_exits.splice(exit_idx, 1)[0];
        const wx = O.wx, wy = O.wy, wz = O.wz;
        const wdx = O.wdx, wdz = O.wdz;
        const source_name = O.source_name || placed[0].name;

        let placed_new = false;
        const types = Object.keys(OBJECTS);
        shuffle(types, rng);

        for (const cand_name of types) {
            if (cand_name === "ROOM05" || cand_name === "ROOM06" || cand_name === "ROOM02") {
                if (rng() > 0.20) continue;
            }

            if (cand_name === "ROOM05") {
                if (!source_name.startsWith("CORRIDOR")) continue;
            }

            if (cand_name === "CROSSING01" || cand_name === "CROSSING02" || cand_name === "CROSSING03") {
                if (source_name === "ROOM05") continue;
                if (!source_name.startsWith("CORRIDOR")) continue;
            }

            if (placed_new) break;
            const cand_exits = OBJECTS[cand_name];

            const indices = Array.from({ length: cand_exits.length }, (_, k) => k);
            shuffle(indices, rng);

            for (const ext_idx of indices) {
                if (placed_new) break;
                const loc_ext = cand_exits[ext_idx];
                const [lx, lz] = loc_ext.pos;
                const [ldx, ldz] = loc_ext.out;

                const rots = [0, 90, 180, 270];
                shuffle(rots, rng);

                for (const ang of rots) {
                    const [rdx, rdz] = rotate_dir(ldx, ldz, ang);
                    if (rdx === -wdx && rdz === -wdz) {
                        const rp = rotate(lx, lz, ang);
                        const Ox = wx - rp[0];
                        const Oy = wy - (loc_ext.y || 0);
                        const Oz = wz - rp[1];

                        if (!check_collision(Ox, Oz, cand_name, ang, O)) {
                            const cand_bounds = get_world_bounds_full(cand_name, Ox, Oz, ang);
                            const new_piece = { 'name': cand_name, 'x': Ox, 'y': Oy, 'z': Oz, 'rot': ang, 'bounds': cand_bounds };
                            placed.push(new_piece);
                            placed_new = true;

                            if (cand_name === 'CROSSING01' || cand_name === 'CROSSING02' || cand_name === 'CROSSING03') {
                                for (const c_ext of cand_exits) {
                                    if (rng() < 0.40) {
                                        const [clx, clz] = c_ext.pos;
                                        const out_dir = c_ext.out;
                                        let base_door_rot = 0;

                                        if (out_dir === DIR_S) base_door_rot = 0;
                                        else if (out_dir === DIR_E) base_door_rot = 90;
                                        else if (out_dir === DIR_N) base_door_rot = 180;
                                        else if (out_dir === DIR_W) base_door_rot = 270;

                                        const door_rot = (base_door_rot + ang) % 360;
                                        const [rx, rz] = rotate(clx, clz, ang);
                                        const door_wx = Ox + rx;
                                        const door_wy = Oy + (c_ext.y || 0) + 75.0;
                                        const door_wz = Oz + rz;

                                        entities.push({
                                            'type': 'door55',
                                            'name': '0',
                                            'x': door_wx,
                                            'y': door_wy,
                                            'z': door_wz,
                                            'rot': door_rot,
                                            'id': entity_id_idx,
                                            'state': -1
                                        });
                                        entity_id_idx++;
                                    }
                                }
                            }

                            if (cand_name === 'ROOM05' || cand_name === 'CORRIDOR01' || cand_name === 'ROOM06') {
                                let num_torches = randint(2, 4);
                                let all_torch_positions = [];

                                if (cand_name === 'CORRIDOR01') {
                                    all_torch_positions = [
                                        [-200, -155, 270],
                                        [0, -155, 270],
                                        [200, -155, 270],
                                        [-200, 155, 90],
                                        [0, 155, 90],
                                        [200, 155, 90]
                                    ];
                                } else if (cand_name === 'ROOM05') {
                                    num_torches = randint(4, 8);
                                    all_torch_positions = [
                                        [-334 - 25, -431, 180],
                                        [-334 - 25, -231, 180],
                                        [-334 - 25, -31,  180],
                                        [-334 - 25, 169,  180],
                                        [-334 - 25, 369,  180],
                                        [415, -631, 0],
                                        [415, -231, 0],
                                        [415, -31,  0],
                                        [415, 169,  0],
                                        [415, 369,  0],
                                        [-174, -791 + 180, 270],
                                        [26,   -791 + 180, 270],
                                        [226,  -791 + 180, 270],
                                        [426,  -791 + 180, 270],
                                        [-174, 655 + 40, 90],
                                        [26,   655 + 40, 90],
                                        [226,  655 + 40, 90],
                                        [426,  655 + 40, 90]
                                    ];
                                } else if (cand_name === 'ROOM06') {
                                    all_torch_positions = [
                                        [-254 + 125, -311 + 200, 180],
                                        [-254 + 60, -311 + 390, 270],
                                        [251 - 125, -311 + 200, 0],
                                        [251 - 60, -311 + 390, 270],
                                        [-254 + 200, 296 - 20, 90],
                                        [-254 + 400, 296 - 20, 90]
                                    ];
                                }

                                const torch_positions = num_torches > 0 ? sample(all_torch_positions, num_torches, rng) : [];

                                const base_colors = [
                                    [1.0, 1.0, 1.0], [1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0],
                                    [1.0, 1.0, 0.0], [0.0, 1.0, 1.0], [1.0, 0.0, 1.0], [1.0, 0.5, 0.0],
                                    [0.5, 0.0, 1.0], [1.0, 0.75, 0.8]
                                ];
                                const base_color = choice(base_colors);
                                const torch_r = Math.round(base_color[0] * uniform(9.0, 13.0) * 10) / 10;
                                const torch_g = Math.round(base_color[1] * uniform(9.0, 13.0) * 10) / 10;
                                const torch_b = Math.round(base_color[2] * uniform(9.0, 13.0) * 10) / 10;

                                for (const [torch_x, torch_z, base_rot] of torch_positions) {
                                    const rotated_pos = rotate(torch_x, torch_z, ang);
                                    const world_x = Ox + rotated_pos[0];
                                    const world_z = Oz + rotated_pos[1];
                                    const torch_y = Oy + 100.0;
                                    const torch_rot = (base_rot + ang) % 360;

                                    entities.push({
                                        'type': '!monster1',
                                        'name': '0',
                                        'x': world_x,
                                        'y': torch_y,
                                        'z': world_z,
                                        'rot': torch_rot,
                                        'id': entity_id_idx,
                                        'state': 2
                                    });
                                    entity_id_idx++;

                                    const lamp_y = Oy + 120.0;
                                    entities.push({
                                        'type': 'lamp_post',
                                        'x': world_x,
                                        'y': lamp_y,
                                        'z': world_z,
                                        'rot': 0,
                                        'id': entity_id_idx,
                                        'state': 0,
                                        'name': ''
                                    });
                                    entity_id_idx++;

                                    entities.push({
                                        'type': 'LIGHT_SOURCE',
                                        'name': 'flicker',
                                        'x': world_x,
                                        'y': lamp_y,
                                        'z': world_z,
                                        'rot': 0,
                                        'id': entity_id_idx,
                                        'state': 0,
                                        'color': [torch_r, torch_g, torch_b],
                                        'dir': [0.0, -1.0, 0.0]
                                    });
                                    entity_id_idx++;
                                }
                            }

                            if (cand_name !== 'CORRIDOR02') {
                                const light_y = Oy + 170.0;
                                entities.push({ 'type': 'lamp_post', 'x': Ox, 'y': light_y, 'z': Oz, 'rot': 0, 'id': entity_id_idx, 'state': 0, 'name': '' });
                                entity_id_idx++;

                                const center_r = Math.round(uniform(9.0, 14.0) * 10) / 10;
                                const center_g = Math.round(uniform(9.0, 14.0) * 10) / 10;
                                const center_b = Math.round(uniform(9.0, 14.0) * 10) / 10;

                                entities.push({ 'type': 'LIGHT_SOURCE', 'name': 'flicker', 'x': Ox, 'y': light_y, 'z': Oz, 'rot': 0, 'id': entity_id_idx, 'state': 0, 'color': [center_r, center_g, center_b], 'dir': [0.0, -1.0, 0.0] });
                                entities.push({ 'type': 'torch1', 'name': '0', 'x': Ox + 0.0, 'y': Oy + 140.0, 'z': Oz + 0.0, 'rot': 0, 'id': entity_id_idx, 'state': 0 });
                                entity_id_idx++;

                                if (rng() < 0.55) {
                                    const s_y = Oy + choice([200.0, 300.0, 400.0]);
                                    const hue = rng();
                                    const sat = uniform(0.4, 1.0);
                                    const val = uniform(0.3, 0.9);
                                    const color = hsvToRgb(hue, sat, val);

                                    entities.push({ 'type': 'lamp_post', 'x': Ox, 'y': s_y, 'z': Oz, 'rot': 0, 'id': entity_id_idx, 'state': 0, 'name': '' });
                                    entity_id_idx++;

                                    let dx = uniform(-0.4, 0.4);
                                    let dz = uniform(-0.4, 0.4);
                                    let dy = -1.0;
                                    const length = Math.sqrt(dx * dx + dy * dy + dz * dz);
                                    dx /= length;
                                    dy /= length;
                                    dz /= length;

                                    entities.push({ 'type': 'LIGHT_SOURCE', 'name': 'Spotlight', 'x': Ox, 'y': s_y, 'z': Oz, 'rot': 0, 'id': entity_id_idx, 'state': 0, 'color': color, 'dir': [dx, dy, dz] });
                                    entity_id_idx++;
                                }

                                const depth = Math.max(0.0, -Oy);
                                const level = Math.min(3, Math.floor(depth / 280));
                                const d_rot = choice([0, 45, 90, 135, 180, 225, 270, 315]);

                                let num_items;
                                if (cand_name.startsWith("ROOM05")) {
                                    num_items = randint(4, 6);
                                } else if (cand_name.startsWith("CROSSING")) {
                                    num_items = randint(2, 3);
                                } else {
                                    num_items = randint(1, 3);
                                }

                                const lastPlaced = placed[placed.length - 1];

                                for (let it = 0; it < num_items; it++) {
                                    const r = rng();
                                    let ent_type = null;

                                    let local_x = 0.0, local_z = 0.0;
                                    if (FLOOR_SLOTS[cand_name]) {
                                        if (!lastPlaced.used_slots) {
                                            lastPlaced.used_slots = new Set();
                                        }

                                        const available_slots = FLOOR_SLOTS[cand_name].filter(
                                            slot => !lastPlaced.used_slots.has(`${slot[0]},${slot[1]}`)
                                        );

                                        if (available_slots.length > 0) {
                                            const picked = choice(available_slots);
                                            local_x = picked[0];
                                            local_z = picked[1];
                                            lastPlaced.used_slots.add(`${local_x},${local_z}`);
                                        }
                                    }

                                    const [slot_world_x, slot_world_z] = rotate(local_x, local_z, ang);
                                    const wx_item = Ox + slot_world_x;
                                    const wz_item = Oz + slot_world_z;

                                    if (cand_name === 'CORRIDOR02') continue;

                                    if (r < 0.10) {
                                        ent_type = choice(['POTION', 'cheese1', 'GOBLET', 'pot1', 'pot2', 'pot3', 'mushroom']);
                                        entities.push({ 'type': ent_type, 'name': '-1', 'x': wx_item, 'y': Oy + 40.0, 'z': wz_item, 'rot': d_rot, 'id': entity_id_idx, 'state': 0 });
                                    } else if (r < 0.25) {
                                        ent_type = choice(['rock1', 'rock2', 'skull', 'QUARTZ', 'cdoorclosedchest', 'block01', 'block02']);
                                        let st = 0;
                                        if (ent_type === 'cdoorclosedchest') {
                                            st = level < 2 ? choice([0, 1]) : choice([0, 1, 2]);
                                        }
                                        let adjust = 10.0;
                                        if (ent_type === 'block01' || ent_type === 'block02') {
                                            adjust = -11.0;
                                        }
                                        entities.push({ 'type': ent_type, 'name': '0', 'x': wx_item, 'y': Oy + adjust, 'z': wz_item, 'rot': d_rot, 'id': entity_id_idx, 'state': st });
                                    } else if (r < 0.28) {
                                        ent_type = 'armour';
                                        entities.push({ 'type': ent_type, 'name': '-1', 'x': wx_item, 'y': Oy + 60.0, 'z': wz_item, 'rot': d_rot, 'id': entity_id_idx, 'state': 0 });
                                    } else if (r < 0.30) {
                                        ent_type = 'COIN';
                                        entities.push({ 'type': 'COIN', 'name': '-1', 'x': wx_item, 'y': Oy + 60.0, 'z': wz_item, 'rot': d_rot, 'id': entity_id_idx, 'state': 0 });
                                    } else if (r < 0.35) {
                                        ent_type = 'SPELLBOOK';
                                        entities.push({ 'type': 'spellbook', 'name': '-1', 'x': wx_item, 'y': Oy + 40.0, 'z': wz_item, 'rot': d_rot, 'id': entity_id_idx, 'state': 0 });
                                    } else if (r < 0.40) {
                                        const scroll_type = choice(['SCROLL-HEALING-', 'SCROLL-MAGICMISSLE-', 'SCROLL-FIREBALL-', 'SCROLL-LIGHTNING-']);
                                        ent_type = scroll_type;
                                        entities.push({ 'type': scroll_type, 'name': '-1', 'x': wx_item, 'y': Oy + 40.0, 'z': wz_item, 'rot': d_rot, 'id': entity_id_idx, 'state': 0 });
                                    } else if (r < 0.45) {
                                        let weapon_types;
                                        if (level === 0) weapon_types = ['BASTARDSWORD', 'FLAMESWORD', 'BATTLEAXE'];
                                        else if (level === 1) weapon_types = ['ICESWORD', 'LIGHTNINGSWORD', 'MORNINGSTAR'];
                                        else weapon_types = ['SPLITSWORD', 'SPIKEDFLAIL', 'SUPERFLAMESWORD'];
                                        const weapon_type = choice(weapon_types);
                                        ent_type = weapon_type;
                                        entities.push({ 'type': weapon_type, 'name': '-1', 'x': wx_item, 'y': Oy + 80.0, 'z': wz_item, 'rot': d_rot, 'id': entity_id_idx, 'state': 0 });
                                    } else {
                                        let possible_mobs;
                                        if (level === 0) possible_mobs = ['GOBLIN', 'TENTACLE', 'SKELETON2', 'INSECT'];
                                        else if (level === 1) possible_mobs = ['OGRE', 'CORPSE', 'MUMMY', 'WOLF', 'COBRA', 'OGRO', 'CENTAUR', 'CRAZ', 'HYDRA', 'OLUVE'];
                                        else if (level === 2) possible_mobs = ['NECROMANCER', 'SORCERER', 'WRAITH', 'PHANTOM', 'KNIGHT', 'SLAVE', 'SORB', 'RIDER', 'FULIMO'];
                                        else possible_mobs = ['FAERIE', 'BAUUL', 'DEMONESS', 'DRAGON', 'IMP', 'OKTA', 'DEMON', 'OKTA'];

                                        ent_type = choice(possible_mobs);
                                        const name_val = ent_type.toLowerCase();
                                        entities.push({
                                            'type': ent_type,
                                            'name': name_val,
                                            'x': wx_item,
                                            'y': Oy + 50.0 + (depth / 140.0) * 2.0,
                                            'z': wz_item,
                                            'rot': d_rot,
                                            'id': entity_id_idx,
                                            'state': 0
                                        });
                                    }

                                    if (ent_type) entity_id_idx++;
                                }
                            }

                            for (let i = 0; i < cand_exits.length; i++) {
                                if (i === ext_idx) continue;
                                const other_ext = cand_exits[i];
                                const op = rotate(other_ext.pos[0], other_ext.pos[1], ang);
                                const od = rotate_dir(other_ext.out[0], other_ext.out[1], ang);
                                open_exits.push({
                                    'wx': Ox + op[0],
                                    'wy': Oy + (other_ext.y || 0),
                                    'wz': Oz + op[1],
                                    'wdx': od[0], 'wdz': od[1],
                                    'source_name': cand_name,
                                    'source_piece': new_piece
                                });
                            }
                        }
                        break;
                    }
                }
            }
        }

        if (!placed_new) {
            failed_exits.push(O);
        }
    }

    open_exits.push(...failed_exits);

    const all_piece_exits = [];
    for (const p of placed) {
        for (const ext of OBJECTS[p.name]) {
            const rp = rotate(ext.pos[0], ext.pos[1], p.rot);
            const rd = rotate_dir(ext.out[0], ext.out[1], p.rot);
            const ey = Math.round((p.y || 0) + (ext.y || 0));
            all_piece_exits.push([Math.round(p.x + rp[0]), ey, Math.round(p.z + rp[1]), rd[0], rd[1]]);
        }
    }

    const connected_positions = new Set();
    for (const [ex, ey, ez, edx, edz] of all_piece_exits) {
        for (const [ex2, ey2, ez2, edx2, edz2] of all_piece_exits) {
            if (ex === ex2 && ey === ey2 && ez === ez2 && edx === -edx2 && edz === -edz2) {
                connected_positions.add(`${ex},${ey},${ez}`);
            }
        }
    }

    const dead_end_exits = open_exits.filter(o =>
        !connected_positions.has(`${Math.round(o.wx)},${Math.round(o.wy)},${Math.round(o.wz)}`)
    );

    for (const open_ex of dead_end_exits) {
        let wx = open_ex.wx, wy = open_ex.wy, wz = open_ex.wz;
        const wdx = open_ex.wdx, wdz = open_ex.wdz;
        const ndx = -wdx, ndz = -wdz;
        let wall_rot = 0;

        if (ndx === 0 && ndz === -1) wall_rot = 270;
        else if (ndx === 1 && ndz === 0) wall_rot = 0;
        else if (ndx === 0 && ndz === 1) wall_rot = 90;
        else if (ndx === -1 && ndz === 0) wall_rot = 180;

        wx += ndz * 160;
        wz += -ndx * 160;

        entities.push({
            'type': 'wall', 'name': 'corridor10',
            'x': wx, 'y': wy, 'z': wz,
            'rot': wall_rot, 'id': entity_id_idx, 'state': 0
        });
        entity_id_idx++;
    }

    if (placed.length > 0) {
        // Filter out CORRIDOR02 (a slope) from being selected for exit circle/spiral portal
        const candidate_pieces = placed.filter(p => p.name !== 'CORRIDOR02');
        let deepest_piece = candidate_pieces.length > 0 ? candidate_pieces[0] : placed[0];
        for (let i = 1; i < candidate_pieces.length; i++) {
            if ((candidate_pieces[i].y || 0) < (deepest_piece.y || 0)) {
                deepest_piece = candidate_pieces[i];
            }
        }

        const tx = deepest_piece.x;
        const ty = deepest_piece.y || 0.0;
        const tz = deepest_piece.z;
        const deepest_name = deepest_piece.name;
        const deepest_rot = deepest_piece.rot;

        if (FLOOR_SLOTS[deepest_name]) {
            const used_local_slots = new Set();
            for (const e of entities) {
                const dx = e.x - tx;
                const dz = e.z - tz;
                const [lx, lz] = rotate(dx, dz, (360 - deepest_rot) % 360);
                used_local_slots.add(`${Math.round(lx)},${Math.round(lz)}`);
            }

            const free_slots = FLOOR_SLOTS[deepest_name].filter(
                slot => !(slot[0] === 0.0 && slot[1] === 0.0) &&
                        !used_local_slots.has(`${Math.round(slot[0])},${Math.round(slot[1])}`)
            );

            if (free_slots.length > 0) {
                const target_slot = choice(free_slots);
                const [rx, rz] = rotate(target_slot[0], target_slot[1], deepest_rot);
                const new_wx = tx + rx;
                const new_wz = tz + rz;

                for (const e of entities) {
                    if (Math.abs(e.x - tx) < 1.0 && Math.abs(e.z - tz) < 1.0 &&
                        !['circle', 'spiral', '!flarenohit', 'lamp_post', 'LIGHT_SOURCE'].includes(e.type)) {
                        e.x = new_wx;
                        e.z = new_wz;
                    }
                }
            }
        }

        entities.push({
            'type': 'circle',
            'x': tx, 'y': ty - 41.0 + 40.0, 'z': tz,
            'rot': 0, 'name': '0', 'id': entity_id_idx, 'state': 2
        });
        entity_id_idx++;

        entities.push({
            'type': 'spiral',
            'x': tx, 'y': ty + 40.0, 'z': tz,
            'rot': 0, 'name': '-1', 'id': entity_id_idx, 'state': 2
        });
        entity_id_idx++;
    }

    const lines = [];
    const links = [];
    let obj_id = 0;

    lines.push("OBJECT startpos");
    lines.push(`CO_ORDINATES ${startX.toFixed(6)} 0.000000 ${startZ.toFixed(6)}`);
    lines.push("ROT_ANGLE 90");
    links.push({ x: startX, z: startZ, rot: 90, id: obj_id });
    obj_id++;

    for (const p of placed) {
        if (p.name === 'left_curve' || p.name === 'right_curve') continue;
        lines.push(`OBJECT ${p.name}`);
        lines.push(`CO_ORDINATES ${p.x.toFixed(6)} ${(p.y || 0.0).toFixed(6)} ${p.z.toFixed(6)}`);
        lines.push(`ROT_ANGLE ${p.rot}`);
        links.push({ x: p.x, z: p.z, rot: p.rot, id: obj_id });
        obj_id++;
    }

    for (const e of entities) {
        const t = e.type;

        if (t === 'wall') {
            lines.push("OBJECT !wall0-240-320");
            lines.push(`CO_ORDINATES ${e.x.toFixed(6)} ${e.y.toFixed(6)} ${e.z.toFixed(6)}`);
            lines.push(`ROT_ANGLE ${e.rot} 0 ${e.name} ${e.id} 0`);
            links.push({ x: e.x, z: e.z, rot: e.rot, id: obj_id });
            obj_id++;
        } else if (t === 'torch') {
            lines.push("OBJECT torch");
            lines.push(`CO_ORDINATES ${e.x.toFixed(6)} ${e.y.toFixed(6)} ${e.z.toFixed(6)}`);
            lines.push(`ROT_ANGLE ${e.rot}`);
            links.push({ x: e.x, z: e.z, rot: e.rot, id: obj_id });
            obj_id++;
        } else if (t === '!flamesnohit') {
            lines.push("OBJECT !flamesnohit");
            lines.push(`CO_ORDINATES ${e.x.toFixed(6)} ${e.y.toFixed(6)} ${e.z.toFixed(6)}`);
            lines.push(`ROT_ANGLE ${e.rot} 0 ${e.name} ${e.state} 0`);
            links.push({ x: e.x, z: e.z, rot: e.rot, id: obj_id });
            obj_id++;
        } else if (t === 'lamp_post') {
            lines.push("OBJECT lamp_post");
            lines.push(`CO_ORDINATES ${e.x.toFixed(6)} ${e.y.toFixed(6)} ${e.z.toFixed(6)}`);
            lines.push(`ROT_ANGLE ${e.rot}`);
            links.push({ x: e.x, z: e.z, rot: e.rot, id: obj_id });
            obj_id++;
        } else if (t === 'LIGHT_SOURCE') {
            const [dx, dy, dz] = e.dir || [0.0, -1.0, 0.0];
            const [r, g, b] = e.color || [1.0, 1.0, 1.0];
            lines.push(
                `LIGHT_SOURCE ${e.name} POS ${e.x.toFixed(6)} ${e.y.toFixed(6)} ${e.z.toFixed(6)}` +
                ` DIR ${dx.toFixed(6)} ${dy.toFixed(6)} ${dz.toFixed(6)}` +
                ` COLOUR ${r.toFixed(6)} ${g.toFixed(6)} ${b.toFixed(6)}`
            );
        } else if (t === 'dframe' || t === 'curve') {
            lines.push(`OBJECT ${t}`);
            lines.push(`CO_ORDINATES ${e.x.toFixed(6)} ${e.y.toFixed(6)} ${e.z.toFixed(6)}`);
            lines.push(`ROT_ANGLE ${e.rot}`);
            links.push({ x: e.x, z: e.z, rot: e.rot, id: obj_id });
            obj_id++;
        } else if (t.startsWith('door')) {
            lines.push(`OBJECT ${t}`);
            lines.push(`CO_ORDINATES ${e.x.toFixed(6)} ${e.y.toFixed(6)} ${e.z.toFixed(6)}`);
            lines.push(`ROT_ANGLE ${e.rot} ${e.state}`);
            links.push({ x: e.x, z: e.z, rot: e.rot, id: obj_id });
            obj_id++;
        } else if (t === 'slope_stairs') {
            lines.push("OBJECT slope_stairs");
            lines.push(`CO_ORDINATES ${e.x.toFixed(6)} ${e.y.toFixed(6)} ${e.z.toFixed(6)}`);
            lines.push(`ROT_ANGLE ${e.rot}`);
            links.push({ x: e.x, z: e.z, rot: e.rot, id: obj_id });
            obj_id++;
        } else if (t === 'left_curve_road' || t === 'right_curve_road') {
            lines.push(`OBJECT ${t}`);
            lines.push(`CO_ORDINATES ${e.x.toFixed(6)} ${e.y.toFixed(6)} ${e.z.toFixed(6)}`);
            lines.push(`ROT_ANGLE ${e.rot}`);
            links.push({ x: e.x, z: e.z, rot: e.rot, id: obj_id });
            obj_id++;
        } else if (t === '!flarenohit') {
            lines.push(`OBJECT ${t}`);
            lines.push(`CO_ORDINATES ${e.x.toFixed(6)} ${e.y.toFixed(6)} ${e.z.toFixed(6)}`);
            lines.push(`ROT_ANGLE ${e.rot} 3 ${e.name || ''} ${e.id || 0} ${e.state || 0}`);
            links.push({ x: e.x, z: e.z, rot: e.rot, id: obj_id });
            obj_id++;
        } else if (t === '!monster1') {
            lines.push("OBJECT !monster1");
            lines.push(`CO_ORDINATES ${e.x.toFixed(6)} ${e.y.toFixed(6)} ${e.z.toFixed(6)}`);
            lines.push(`ROT_ANGLE ${e.rot} torch2 ${e.name || '0'} ${e.id || 0} 0`);
            links.push({ x: e.x, z: e.z, rot: e.rot, id: obj_id });
            obj_id++;
        } else {
            lines.push("OBJECT !monster1");
            lines.push(`CO_ORDINATES ${e.x.toFixed(6)} ${e.y.toFixed(6)} ${e.z.toFixed(6)}`);
            lines.push(`ROT_ANGLE ${e.rot} ${t} ${e.name || ''} ${e.id || 0} ${e.state || 0}`);
            links.push({ x: e.x, z: e.z, rot: e.rot, id: obj_id });
            obj_id++;
        }
    }

    lines.push("OBJECT text");
    lines.push("CO_ORDINATES 2020.000000 0.000000 1020.000000");
    lines.push("ROT_ANGLE 0 largedungeon");
    links.push({ x: 2020.0, z: 1020.0, rot: 0, id: obj_id });
    obj_id++;

    for (const link of links) {
        lines.push(`LINK ${link.x.toFixed(6)} ${link.z.toFixed(6)} ${Math.floor(link.rot)} ${link.id}`);
    }

    lines.push("END_FILE");

    const mapText = lines.join("\n") + "\n";
    return mapText;
}
