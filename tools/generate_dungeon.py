import math
import random
from dataclasses import dataclass

# ------------------------------------------------------------
# Tile / prefab definitions
# ------------------------------------------------------------

@dataclass
class Prefab:
    name: str
    width: float
    height: float
    center_x: float
    center_z: float
    exits_by_rot: dict  # rot -> {dir: (x,z) in local space}


def build_prefabs():
    prefabs = {}

    # crossroads
    prefabs["crossroads"] = Prefab(
        "crossroads", 240, 240, 80, 120,
        {
            0:   {"N": (80,240), "S": (80,0),   "W": (-40,120), "E": (200,120)},
            90:  {"N": (200,120), "S": (-40,120), "W": (80,240), "E": (80,0)},
            180: {"N": (80,0), "S": (80,240), "W": (200,120), "E": (-40,120)},
            270: {"N": (-40,120), "S": (200,120), "W": (80,0), "E": (80,240)},
        }
    )

    # t_junction
    prefabs["t_junction"] = Prefab(
        "t_junction", 240, 200, 80, 100,
        {
            0:   {"N": (80,200), "W": (-40,100), "E": (200,100)},
            90:  {"N": (200,100), "W": (80,200), "E": (80,0)},
            180: {"N": (80,0), "W": (200,100), "E": (-40,100)},
            270: {"N": (-40,100), "W": (80,0), "E": (80,200)},
        }
    )

    # left_corner
    prefabs["left_corner"] = Prefab(
        "left_corner", 200, 200, 60, 100,
        {
            0:   {"N": (60,200), "W": (-40,100)},
            90:  {"N": (160,100), "W": (60,200)},
            180: {"N": (60,0), "W": (160,100)},
            270: {"N": (-40,100), "W": (60,0)},
        }
    )

    # ROOM2 (straight corridor)
    prefabs["ROOM2"] = Prefab(
        "ROOM2", 160, 320, 0, 0,
        {
            0:   {"N": (0,160), "S": (0,-160)},
            90:  {"N": (80,0), "S": (-80,0)},
            180: {"N": (0,-160), "S": (0,160)},
            270: {"N": (-80,0), "S": (80,0)},
        }
    )

    return prefabs


# ------------------------------------------------------------
# Grid / maze generation (Prim-style)
# ------------------------------------------------------------

GRID_W = 3
GRID_H = 3
CELL_SPACING = 540.0
GRID_ORIGIN_X = 1920.0
GRID_ORIGIN_Z = 880.0

DIRS = {
    "N": (0, -1),
    "S": (0, 1),
    "W": (-1, 0),
    "E": (1, 0),
}

OPPOSITE = {"N": "S", "S": "N", "E": "W", "W": "E"}


def grid_center(i, j):
    gx = GRID_ORIGIN_X + i * CELL_SPACING
    gz = GRID_ORIGIN_Z + j * CELL_SPACING
    return gx, gz


def in_bounds(i, j):
    return 0 <= i < GRID_W and 0 <= j < GRID_H


def generate_maze():
    # cells: True if in maze
    in_maze = [[False for _ in range(GRID_H)] for _ in range(GRID_W)]
    edges = []  # list of ((i1,j1),(i2,j2),dir_from_1_to_2)

    # start from random cell
    start_i = random.randint(0, GRID_W - 1)
    start_j = random.randint(0, GRID_H - 1)
    in_maze[start_i][start_j] = True

    frontier = []
    for d, (dx, dz) in DIRS.items():
        ni, nj = start_i + dx, start_j + dz
        if in_bounds(ni, nj):
            frontier.append(((start_i, start_j), (ni, nj), d))

    while frontier:
        idx = random.randrange(len(frontier))
        (i1, j1), (i2, j2), d = frontier.pop(idx)

        if in_maze[i2][j2]:
            continue

        # add edge
        in_maze[i2][j2] = True
        edges.append(((i1, j1), (i2, j2), d))

        # add new frontier edges
        for nd, (dx, dz) in DIRS.items():
            ni, nj = i2 + dx, j2 + dz
            if in_bounds(ni, nj) and not in_maze[ni][nj]:
                frontier.append(((i2, j2), (ni, nj), nd))

    return in_maze, edges


# ------------------------------------------------------------
# Junction classification and rotation selection
# ------------------------------------------------------------

def classify_cell(i, j, edges):
    # which directions connect from this cell
    dirs = set()
    for (i1, j1), (i2, j2), d in edges:
        if (i1, j1) == (i, j):
            dirs.add(d)
        elif (i2, j2) == (i, j):
            dirs.add(OPPOSITE[d])

    count = len(dirs)
    if count == 4:
        return "crossroads", dirs
    if count == 3:
        return "t_junction", dirs
    if count == 2:
        # straight or corner
        if ("N" in dirs and "S" in dirs) or ("E" in dirs and "W" in dirs):
            return "ROOM2", dirs
        else:
            return "left_corner", dirs
    if count == 1:
        return "ROOM2", dirs
    return None, dirs


def choose_rotation_for_piece(prefab, open_dirs):
    # pick a rotation whose exits cover all open_dirs
    for rot, exits in prefab.exits_by_rot.items():
        if all(d in exits for d in open_dirs):
            return rot
    # fallback: any rotation that has at least one matching dir
    for rot, exits in prefab.exits_by_rot.items():
        if any(d in exits for d in open_dirs):
            return rot
    # last resort
    return 0


# ------------------------------------------------------------
# World placement with offset correction
# ------------------------------------------------------------

@dataclass
class Placed:
    name: str
    wx: float
    wz: float
    rot: int
    prefab: Prefab


def rotated_center(prefab: Prefab, rot: int):
    theta = math.radians(rot)
    cx = prefab.center_x
    cz = prefab.center_z
    cxp = cx * math.cos(theta) - cz * math.sin(theta)
    czp = cx * math.sin(theta) + cz * math.cos(theta)
    return cxp, czp


def place_at_grid_center(prefab: Prefab, gx: float, gz: float, rot: int):
    cxp, czp = rotated_center(prefab, rot)
    wx = gx - cxp
    wz = gz - czp
    return wx, wz


def aabb_world(prefab: Prefab, wx: float, wz: float, rot: int):
    if rot in (0, 180):
        w = prefab.width
        h = prefab.height
    else:
        w = prefab.height
        h = prefab.width
    min_x = wx - w / 2.0
    max_x = wx + w / 2.0
    min_z = wz - h / 2.0
    max_z = wz + h / 2.0
    return min_x, max_x, min_z, max_z


def overlaps(a, b):
    ax1, ax2, az1, az2 = a
    bx1, bx2, bz1, bz2 = b
    return not (ax2 <= bx1 or bx2 <= ax1 or az2 <= bz1 or bz2 <= az1)


# ------------------------------------------------------------
# Corridor placement between cells
# ------------------------------------------------------------

def place_corridor_between(prefabs, i1, j1, i2, j2, d):
    gx1, gz1 = grid_center(i1, j1)
    gx2, gz2 = grid_center(i2, j2)
    mid_x = (gx1 + gx2) / 2.0
    mid_z = (gz1 + gz2) / 2.0

    room2 = prefabs["ROOM2"]

    if d in ("E", "W"):
        # horizontal connection: rotate 270, slight Z offset
        rot = 270
        gx = mid_x
        gz = mid_z - 20.0
    else:
        # vertical connection: rotate 0, slight X offset
        rot = 0
        gx = mid_x + 20.0
        gz = mid_z

    wx, wz = place_at_grid_center(room2, gx, gz, rot)
    return Placed("ROOM2", wx, wz, rot, room2)


# ------------------------------------------------------------
# Main dungeon generation
# ------------------------------------------------------------

def generate_dungeon():
    prefabs = build_prefabs()
    in_maze, edges = generate_maze()

    placed = []

    # place junctions / rooms at each active cell
    for i in range(GRID_W):
        for j in range(GRID_H):
            if not in_maze[i][j]:
                continue

            piece_name, dirs = classify_cell(i, j, edges)
            if piece_name is None:
                continue

            prefab = prefabs[piece_name]
            rot = choose_rotation_for_piece(prefab, dirs)
            gx, gz = grid_center(i, j)
            wx, wz = place_at_grid_center(prefab, gx, gz, rot)

            placed.append(Placed(piece_name, wx, wz, rot, prefab))

    # place corridors for each edge
    for (i1, j1), (i2, j2), d in edges:
        corridor = place_corridor_between(prefabs, i1, j1, i2, j2, d)
        placed.append(corridor)

    # collision detection: simple rejection (for now just report overlaps)
    aabbs = []
    non_overlapping = []
    for p in placed:
        box = aabb_world(p.prefab, p.wx, p.wz, p.rot)
        if any(overlaps(box, other) for other in aabbs):
            # skip overlapping piece
            continue
        aabbs.append(box)
        non_overlapping.append(p)

    # choose startpos at first placed cell center
    start = None
    for p in non_overlapping:
        if p.name in ("crossroads", "t_junction", "ROOM2", "left_corner"):
            start = p
            break

    return non_overlapping, start


# ------------------------------------------------------------
# Map export
# ------------------------------------------------------------

def export_map(placed, start):
    lines = []
    for p in placed:
        lines.append(f"OBJECT {p.name}")
        lines.append(f"CO_ORDINATES {p.wx:.6f} 0.000000 {p.wz:.6f}")
        lines.append(f"ROT_ANGLE {p.rot}")

    if start is not None:
        lines.append("OBJECT startpos")
        lines.append(f"CO_ORDINATES {start.wx:.6f} 0.000000 {start.wz:.6f}")
        lines.append("ROT_ANGLE 0")

    lines.append("END_FILE")
    return "\n".join(lines)


# ------------------------------------------------------------
# Main entry
# ------------------------------------------------------------

if __name__ == "__main__":
    random.seed(42)

    placed, start = generate_dungeon()
    map_text = export_map(placed, start)

    # For now just print; you can redirect to bin/level1.map
    print(map_text)
