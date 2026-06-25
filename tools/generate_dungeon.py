#!/usr/bin/env python3
"""Generate bin/level1.map from the connected square-dungeon template.

Level 1 is a 3x3 junction grid: four corners (left_corner), four edge
T-junctions, one centre crossroads, and twelve ROOM2 corridor segments
linking them.  Piece positions and rotations come from the original
level1.map layout so corridor CONNECTION points line up.
"""

from __future__ import annotations

import argparse
import math
import random
from pathlib import Path

# Canonical connected layout taken from bin/level1.map.bak
TEMPLATE_PIECES: list[tuple[str, float, float, int]] = [
    ("left_corner", 1980.0, 980.0, 180),
    ("ROOM2", 2180.0, 860.0, 270),
    ("ROOM2", 1900.0, 1140.0, 0),
    ("t_junction", 2020.0, 1340.0, 90),
    ("t_junction", 2540.0, 980.0, 180),
    ("ROOM2", 2460.0, 1140.0, 180),
    ("ROOM2", 2180.0, 1420.0, 270),
    ("crossroads", 2580.0, 1340.0, 90),
    ("ROOM2", 2740.0, 860.0, 90),
    ("ROOM2", 2740.0, 1420.0, 90),
    ("ROOM2", 1900.0, 1700.0, 180),
    ("left_corner", 2020.0, 1900.0, 90),
    ("ROOM2", 2180.0, 1980.0, 90),
    ("t_junction", 2380.0, 1860.0, 0),
    ("ROOM2", 2460.0, 1700.0, 0),
    ("ROOM2", 2740.0, 1980.0, 90),
    ("left_corner", 2900.0, 940.0, 270),
    ("ROOM2", 3020.0, 1140.0, 0),
    ("t_junction", 2900.0, 1500.0, 270),
    ("ROOM2", 3020.0, 1700.0, 0),
    ("left_corner", 2940.0, 1860.0, 0),
]

# Corner piece positions used for startpos / exit placement.
CORNER_POSITIONS: list[tuple[float, float]] = [
    (1980.0, 980.0),   # NW
    (2900.0, 940.0),   # NE
    (2020.0, 1900.0),  # SW
    (2940.0, 1860.0),  # SE
]

DEFAULT_STARTPOS = (1880.0, 840.0, 0)

PIVOT_X = 2580.0
PIVOT_Z = 1340.0

MONSTERS = (
    ("GOBLIN", "goblin"),
    ("SKELETON2", "skeleton2"),
    ("WOLF", "wolf"),
    ("MUMMY", "mummy"),
    ("SORB", "sorb"),
)

TREASURES = (
    "armour",
    "COIN",
    "diamond",
    "POTION",
    "spellbook",
    "bread1",
    "cheese1",
)

# Corridor midpoints for entity placement (centres of ROOM2 segments).
CORRIDOR_CENTERS: list[tuple[float, float]] = [
    (2180.0, 860.0),
    (1900.0, 1140.0),
    (2460.0, 1140.0),
    (2180.0, 1420.0),
    (2740.0, 860.0),
    (2740.0, 1420.0),
    (1900.0, 1700.0),
    (2180.0, 1980.0),
    (2460.0, 1700.0),
    (2740.0, 1980.0),
    (3020.0, 1140.0),
    (3020.0, 1700.0),
]


def rotate_map(
    x: float, z: float, rot: int, map_rotation: int
) -> tuple[float, float, int]:
    """Rotate a piece around the dungeon pivot."""
    steps = (map_rotation // 90) % 4
    px, pz = PIVOT_X, PIVOT_Z
    rx, rz = x, z
    piece_rot = rot
    for _ in range(steps):
        dx = rx - px
        dz = rz - pz
        rx = px + dz
        rz = pz - dx
        piece_rot = (piece_rot + 90) % 360
    return rx, rz, piece_rot


def transform_pieces(map_rotation: int) -> list[tuple[str, float, float, int]]:
    if map_rotation % 360 == 0:
        return list(TEMPLATE_PIECES)
    return [
        (name, *rotate_map(x, z, rot, map_rotation))
        for name, x, z, rot in TEMPLATE_PIECES
    ]


def transform_point(x: float, z: float, map_rotation: int) -> tuple[float, float]:
    rx, rz, _ = rotate_map(x, z, 0, map_rotation)
    return rx, rz


def format_piece(name: str, x: float, z: float, rot: int) -> list[str]:
    return [
        f"OBJECT {name}",
        f"CO_ORDINATES {x:.6f} 0.000000 {z:.6f} ",
        f"ROT_ANGLE {rot}",
    ]


def format_entity(
    x: float,
    z: float,
    rot: int,
    kind: str,
    subtype: str,
    entity_id: int,
    flags: int = 0,
    y: float = -20.0,
) -> list[str]:
    return [
        "OBJECT !monster1",
        f"CO_ORDINATES {x:.6f} {y:.6f} {z:.6f} ",
        f"ROT_ANGLE {rot} {kind} {subtype} {entity_id} {flags}",
    ]


def build_map_lines(
    map_rotation: int = 0,
    seed: int | None = None,
    add_entities: bool = True,
) -> list[str]:
    rng = random.Random(seed)
    lines: list[str] = []

    for name, x, z, rot in transform_pieces(map_rotation):
        lines.extend(format_piece(name, x, z, rot))

    corners = [transform_point(x, z, map_rotation) for x, z in CORNER_POSITIONS]
    start_corner = corners[0]
    exit_corner = corners[-1]

    sx = start_corner[0] - 20.0
    sz = start_corner[1] - 140.0
    if map_rotation % 360 == 0:
        sx, sz = DEFAULT_STARTPOS[0], DEFAULT_STARTPOS[1]
    lines.extend(format_piece("startpos", sx, sz, 0))

    entity_id = 1
    if add_entities:
        ex = exit_corner[0] + 120.0
        ez = exit_corner[1] + 120.0
        lines.extend(format_entity(ex, ez, 0, "spiral", "-1", entity_id, flags=2, y=0.0))
        entity_id += 1

        corridors = [transform_point(x, z, map_rotation) for x, z in CORRIDOR_CENTERS]
        rng.shuffle(corridors)

        for x, z in corridors[:4]:
            monster, subtype = rng.choice(MONSTERS)
            lines.extend(format_entity(x, z, 0, monster, subtype, entity_id))
            entity_id += 1

        for x, z in corridors[4:8]:
            treasure = rng.choice(TREASURES)
            lines.extend(
                format_entity(
                    x + rng.randint(-15, 15),
                    z + rng.randint(-15, 15),
                    0,
                    treasure,
                    "-1",
                    entity_id,
                )
            )
            entity_id += 1

    lines.append("END_FILE")
    return lines


def write_map(path: Path, lines: list[str]) -> None:
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--rotation",
        type=int,
        choices=(0, 90, 180, 270),
        default=None,
        help="Rotate entire dungeon around the crossroads (default: random)",
    )
    parser.add_argument("--seed", type=int, default=None, help="Random seed")
    parser.add_argument(
        "--no-entities",
        action="store_true",
        help="Emit structure only (no monsters, treasure, or spiral)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path(__file__).resolve().parent.parent / "bin" / "level1.map",
        help="Output map file path",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    rng = random.Random(args.seed)
    rotation = args.rotation if args.rotation is not None else 0
    lines = build_map_lines(
        map_rotation=rotation,
        seed=args.seed,
        add_entities=not args.no_entities,
    )
    write_map(args.output, lines)
    object_count = sum(1 for line in lines if line.startswith("OBJECT "))
    print(
        f"Wrote {args.output} "
        f"(rotation={rotation}, seed={args.seed}, {object_count} objects)"
    )


if __name__ == "__main__":
    main()
