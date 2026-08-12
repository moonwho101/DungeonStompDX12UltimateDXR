import re
import sys

def parse_tritex_file(path):
    # Regex to capture 3 floats after "TRITEX" or on following lines
  # More robust float pattern: matches ints or floats
    float_pattern = r"(-?\d+(?:\.\d+)?)"
    line_pattern = re.compile(rf"{float_pattern}\s+{float_pattern}\s+{float_pattern}")

    xs, ys, zs = [], [], []

    with open(path, "r") as f:
        for line in f:
            m = line_pattern.search(line)
            if m:
                x, y, z = map(float, m.groups())
                xs.append(x)
                ys.append(y)
                zs.append(z)

    return xs, ys, zs


def compute_bbox(xs, ys, zs):
    return (
        min(xs), min(ys), min(zs),
        max(xs), max(ys), max(zs)
    )


def main():
    if len(sys.argv) < 2:
        print("Usage: python tritex_bbox.py <file.tritex>")
        return

    path = sys.argv[1]
    xs, ys, zs = parse_tritex_file(path)

    if not xs:
        print("No TRITEX vertices found.")
        return

    minx, miny, minz, maxx, maxy, maxz = compute_bbox(xs, ys, zs)

    print("Bounding Box:")
    print(f"  Min = ({minx}, {miny}, {minz})")
    print(f"  Max = ({maxx}, {maxy}, {maxz})")
    print(f"  Box = ({round(minx)}, {round(minz)}, {round(maxx)}, {round(maxz)})")

if __name__ == "__main__":
    main()
