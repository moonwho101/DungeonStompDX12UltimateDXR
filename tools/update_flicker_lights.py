#!/usr/bin/env python3
"""
Update flicker light sources in .map files to have a specific color.
This script reads a .map file and sets any light source of type 'flicker' 
to have a color of 9.000000 9.000000 9.000000.
"""

import re
import sys
from pathlib import Path


def update_flicker_lights(input_file, output_file=None, target_color="9.000000 9.000000 9.000000"):
    """
    Update flicker light sources in a .map file with the specified color.
    
    Args:
        input_file (str): Path to the input .map file
        output_file (str, optional): Path to the output file. If None, overwrites input file.
        target_color (str): The color values to set (default: "9.000000 9.000000 9.000000")
    
    Returns:
        int: Number of light sources updated
    """
    input_path = Path(input_file)
    
    if not input_path.exists():
        print(f"Error: File '{input_file}' not found.")
        return 0
    
    # Read the file
    with open(input_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Pattern to match flicker light sources
    # LIGHT_SOURCE flicker POS ... DIR ... COLOUR r g b
    pattern = r'(LIGHT_SOURCE\s+flicker\s+POS\s+[\d.\-]+\s+[\d.\-]+\s+[\d.\-]+\s+DIR\s+[\d.\-]+\s+[\d.\-]+\s+[\d.\-]+\s+COLOUR\s+)[\d.\-]+\s+[\d.\-]+\s+[\d.\-]+'
    
    # Count matches before replacement
    matches = re.findall(pattern, content)
    count = len(matches)
    
    # Replace the color values for flicker lights
    # Use a lambda to avoid backreference issues with \1 followed by digits
    updated_content = re.sub(pattern, lambda m: m.group(1) + target_color, content)
    
    # Determine output file
    if output_file is None:
        output_path = input_path
    else:
        output_path = Path(output_file)
    
    # Write the updated content
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(updated_content)
    
    return count


def main():
    """Main function to handle command-line arguments."""
    if len(sys.argv) < 2:
        print("Usage: python update_flicker_lights.py <input.map> [output.map] [r g b]")
        print("\nExamples:")
        print("  python update_flicker_lights.py level2.map")
        print("  python update_flicker_lights.py level2.map level2_updated.map")
        print("  python update_flicker_lights.py level2.map level2_updated.map 10.0 10.0 10.0")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else None
    
    # Handle custom color values
    if len(sys.argv) >= 5:
        target_color = f"{sys.argv[3]} {sys.argv[4]} {sys.argv[5]}"
    else:
        target_color = "9.000000 9.000000 9.000000"
    
    # Update the lights
    count = update_flicker_lights(input_file, output_file, target_color)
    
    # Report results
    if count > 0:
        output_name = output_file if output_file else input_file
        print(f"✓ Updated {count} flicker light source(s) in '{output_name}'")
        print(f"  New color: {target_color}")
    else:
        print(f"No flicker light sources found in '{input_file}'")


if __name__ == "__main__":
    main()
