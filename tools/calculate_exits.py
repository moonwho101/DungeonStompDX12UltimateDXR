import math

def rotate(x, z, angle):
    if angle == 0:   return x, z
    if angle == 90:  return -z, x
    if angle == 180: return -x, -z
    if angle == 270: return z, -x
    return x, z

# From the analysis, let me work backwards to find true exit positions
# Taking a CROSSING01 at (2500, 960) rot 90 that connects to CORRIDOR01 at (2840, 900)

print("Calculating CROSSING01 exit positions:")
print("=" * 70)

# Example 1: CROSSING01 at (2500, 960) rot 90, exit should be at (2840, 900)
crossing_center = (2500, 960)
target_exit = (2840, 900)
crossing_rot = 90

# Local rotated = world_exit - center
local_rotated = (target_exit[0] - crossing_center[0], target_exit[1] - crossing_center[1])
print(f"Local rotated position: {local_rotated}")

# Rotate back to get local unrotated
# rot 90 means we rotated by +90, so rotate back by -90 (which is +270)
local_unrotated = rotate(local_rotated[0], local_rotated[1], 270)
print(f"Local unrotated (SOUTH exit): {local_unrotated}")

# Example 2: CROSSING01 at (2500, 960) rot 90, exit should be at (2420, 1320)  
target_exit2 = (2420, 1320)
local_rotated2 = (target_exit2[0] - crossing_center[0], target_exit2[1] - crossing_center[1])
print(f"\nLocal rotated position 2: {local_rotated2}")
local_unrotated2 = rotate(local_rotated2[0], local_rotated2[1], 270)
print(f"Local unrotated (EAST exit): {local_unrotated2}")

print("\n" + "=" * 70)
print("Calculating CORRIDOR01 exit positions:")
print("=" * 70)

# Example: CORRIDOR01 at (3200, 900) rot 0, exit should be at (2840, 900)
corridor_center = (3200, 900)
target_exit3 = (2840, 900)
corridor_rot = 0

local3 = (target_exit3[0] - corridor_center[0], target_exit3[1] - corridor_center[1])
print(f"Local unrotated (WEST exit): {local3}")

# CORRIDOR01 at (3200, 900) rot 0, exit should be at (3560, 900)
target_exit4 = (3560, 900)
local4 = (target_exit4[0] - corridor_center[0], target_exit4[1] - corridor_center[1])
print(f"Local unrotated (EAST exit): {local4}")

# Verify with another example - CORRIDOR01 at (3980, 1680) rot 90
corridor_center2 = (3980, 1680)
# Should connect to (3900, 2020)
target_exit5 = (3900, 2020)
corridor_rot2 = 90

local_rotated5 = (target_exit5[0] - corridor_center2[0], target_exit5[1] - corridor_center2[1])
print(f"\nCORRIDOR rot 90, local rotated: {local_rotated5}")
local_unrotated5 = rotate(local_rotated5[0], local_rotated5[1], 270)
print(f"Local unrotated (EAST exit): {local_unrotated5}")

print("\n" + "=" * 70)
print("SUMMARY - Correct exit positions:")
print("=" * 70)
print("CROSSING01 exits should be:")
print(f"  SOUTH: {local_unrotated}")
print(f"  EAST:  {local_unrotated2}")
print("\nCORRIDOR01 exits should be:")
print(f"  WEST:  {local3}")
print(f"  EAST:  {local4} (or {local_unrotated5} averaged)")
