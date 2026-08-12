SHIFT = -21.00699   # your delta

def is_vertex_line(stripped):
    parts = stripped.split()
    if len(parts) != 5:
        return False
    # parts should be: x y z u v
    try:
        float(parts[0])
        float(parts[1])
        float(parts[2])
        float(parts[3])
        float(parts[4])
        return True
    except ValueError:
        return False

def process_file(input_path, output_path):
    with open(input_path, "r") as infile, open(output_path, "w") as outfile:
        for line in infile:
            stripped = line.lstrip()

            # Case 1: TRITEX line
            if stripped.startswith("TRITEX"):
                parts = stripped.split()
                # TRITEX x y z u v
                x = float(parts[1])
                y = float(parts[2])
                z = float(parts[3])
                u = parts[4]
                v = parts[5]

                new_y = y + SHIFT
                indent = line[:len(line) - len(stripped)]

                outfile.write(f"{indent}TRITEX {x:.8f} {new_y:.8f} {z:.8f} {u} {v}\n")
                continue

            # Case 2: continuation vertex line (indented)
            if is_vertex_line(stripped):
                parts = stripped.split()
                x = float(parts[0])
                y = float(parts[1])
                z = float(parts[2])
                u = parts[3]
                v = parts[4]

                new_y = y + SHIFT
                indent = line[:len(line) - len(stripped)]

                outfile.write(f"{indent}{x:.8f} {new_y:.8f} {z:.8f} {u} {v}\n")
                continue

            # Everything else unchanged
            outfile.write(line)

    print("Done. Updated file written to:", output_path)


# Example:
# process_file("OBJECT182.txt", "OBJECT182_shifted.txt")



# Example usage:
process_file("o.txt", "o2.txt")
