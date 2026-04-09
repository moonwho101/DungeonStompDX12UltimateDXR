from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter


def build_alpha(max_rgb: np.ndarray, black_cutoff: float, white_cutoff: float, gamma: float, blur_radius: float) -> np.ndarray:
    alpha = np.clip((max_rgb - black_cutoff) / max(white_cutoff - black_cutoff, 1.0), 0.0, 1.0)
    alpha = np.power(alpha, gamma, dtype=np.float32)
    alpha_image = Image.fromarray(np.round(alpha * 255.0).astype(np.uint8), mode="L")
    if blur_radius > 0.0:
        alpha_image = alpha_image.filter(ImageFilter.GaussianBlur(radius=blur_radius))
    return np.asarray(alpha_image, dtype=np.uint8)


def decontaminate_rgb(rgb: np.ndarray, alpha: np.ndarray, min_alpha: float) -> np.ndarray:
    alpha_norm = np.maximum(alpha.astype(np.float32) / 255.0, min_alpha)
    rgb_out = rgb.astype(np.float32) / alpha_norm[..., None]
    return np.clip(np.round(rgb_out), 0, 255).astype(np.uint8)


def process_image(path: Path, black_cutoff: float, white_cutoff: float, gamma: float, blur_radius: float, min_alpha: float) -> None:
    image = Image.open(path).convert("RGBA")
    rgba = np.asarray(image, dtype=np.uint8)
    rgb = rgba[..., :3]
    max_rgb = rgb.max(axis=2).astype(np.float32)
    alpha = build_alpha(max_rgb, black_cutoff, white_cutoff, gamma, blur_radius)
    rgb_out = decontaminate_rgb(rgb, alpha, min_alpha)
    out = np.dstack((rgb_out, alpha))
    Image.fromarray(out, mode="RGBA").save(path)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Apply a soft alpha mask to flame textures on black backgrounds.")
    parser.add_argument("pattern", nargs="?", default="Textures/flame1/*.png", help="Glob pattern for images to process.")
    parser.add_argument("--black-cutoff", type=float, default=8.0, help="Values at or below this intensity become fully transparent.")
    parser.add_argument("--white-cutoff", type=float, default=96.0, help="Values at or above this intensity become fully opaque.")
    parser.add_argument("--gamma", type=float, default=0.85, help="Power curve for soft edge shaping. Lower keeps dim edges softer.")
    parser.add_argument("--blur-radius", type=float, default=1.2, help="Gaussian blur radius applied to the generated alpha channel.")
    parser.add_argument("--min-alpha", type=float, default=0.35, help="Lower clamp for RGB decontamination to avoid dark edge halos.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = Path(__file__).resolve().parents[1]
    files = sorted(root.glob(args.pattern))
    if not files:
        print(f"No files matched pattern: {args.pattern}")
        return 1

    for path in files:
        process_image(path, args.black_cutoff, args.white_cutoff, args.gamma, args.blur_radius, args.min_alpha)
        print(f"Processed {path.relative_to(root)}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())