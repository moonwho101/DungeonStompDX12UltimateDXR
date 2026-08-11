import shutil
import os

# Directory containing the level files
BASE = "./"   # change if needed

extensions = ["map", "mod", "cmp"]

for n in range(1, 16):
    for ext in extensions:
        src = os.path.join(BASE, f"level{n}.{ext}")
        tmp = os.path.join(BASE, f"level{n}a.{ext}")
        dst = os.path.join(BASE, f"level{n+1}.{ext}") if n < 15 else None

        # 1. Copy levelN.* → levelNa.*
        shutil.copyfile(src, tmp)

        # 2. Copy levelNa.* → level(N+1).*
        if dst:
            shutil.copyfile(tmp, dst)

        # 3. Copy levelNa.* → levelN.*
        shutil.copyfile(tmp, src)

print("Done.")
