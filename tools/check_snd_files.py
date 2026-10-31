import os
import glob
from pathlib import Path


def check_snd_files():
    # Resolve repository paths relative to script location
    script_dir = Path(__file__).resolve().parent
    repo_root = script_dir.parent
    bin_dir = repo_root / "bin"
    sounds_dir = repo_root / "Sounds"

    if not bin_dir.is_dir():
        print(f"Error: bin directory not found at {bin_dir}")
        return
    if not sounds_dir.is_dir():
        print(f"Error: Sounds directory not found at {sounds_dir}")
        return

    # Gather all existing sound files in the Sounds directory (case-insensitive)
    sound_files = os.listdir(sounds_dir)
    sound_files_lower = {f.lower(): f for f in sound_files}
    sound_stem_lower = {os.path.splitext(f)[0].lower(): f for f in sound_files}

    snd_files = sorted(bin_dir.glob("*.snd"))
    if not snd_files:
        print(f"No .snd files found in {bin_dir}")
        return

    mismatches = []

    for snd_path in snd_files:
        with open(snd_path, "r", encoding="utf-8", errors="ignore") as f:
            lines = [line.strip() for line in f if line.strip()]

        for entry in lines:
            entry_lower = entry.lower()
            # Check exact filename or filename with extension matching stem
            matched = None
            if entry_lower in sound_files_lower:
                matched = sound_files_lower[entry_lower]
            elif entry_lower in sound_stem_lower:
                matched = sound_stem_lower[entry_lower]

            if not matched:
                # Find suggestions if possible
                suggestions = []
                # Check death vs die replacement
                if "death" in entry_lower:
                    alt = entry_lower.replace("death", "die")
                    if alt in sound_stem_lower:
                        suggestions.append(sound_stem_lower[alt])
                mismatches.append((snd_path.name, entry, suggestions))

    print(f"Checked {len(snd_files)} .snd files in {bin_dir}")
    print(f"Total mismatches found: {len(mismatches)}\n")
    print("Mismatches:")
    print("-" * 60)
    for snd_name, entry, suggestions in mismatches:
        sug_str = f" (did you mean: {', '.join(suggestions)}?)" if suggestions else ""
        print(f"{snd_name:<20} -> {entry}{sug_str}")
    print("-" * 60)

    return mismatches


if __name__ == "__main__":
    check_snd_files()
