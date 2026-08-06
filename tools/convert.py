import re

SCALE = 30.0

vertices = []
uvs = []
faces = []
materials = []

current_material = None

with open("room2.obj", "r") as f:
    for line in f:
        line = line.strip()

        if line.startswith("v "):
            _, x, y, z = line.split()
            vertices.append((float(x), float(y), float(z)))

        elif line.startswith("vt "):
            _, u, v = line.split()
            uvs.append((float(u), float(v)))

        elif line.startswith("usemtl "):
            # Extract material name and drop .jpg
            mat = line.split()[1]
            mat = mat.replace(".jpg", "").replace(".png", "")
            current_material = mat

        elif line.startswith("f "):
            parts = line.split()[1:]
            face = []
            for p in parts:
                v_idx, uv_idx = p.split("/")[:2]
                face.append((int(v_idx)-1, int(uv_idx)-1))

            # Store face with its material
            materials.append((current_material, face))


with open("corridor.ds", "w") as out:

    last_mat = None

    for mat, face in materials:

        # If material changed, emit new TEXTURE block
        if mat != last_mat:
            out.write(f"\nTEXTURE {mat}\n")
            last_mat = mat

        out.write("TRITEX ")

        for (vi, ui) in face:
            x, y, z = vertices[vi]
            u, v = uvs[ui]

            # Apply scale factor
            x *= SCALE
            y *= SCALE
            z *= SCALE

            # Flip V to fix upside-down textures
            v = 1.0 - v

            out.write(f"{x} {y} {z} {u} {v}\n       ")

        out.write("\n")
