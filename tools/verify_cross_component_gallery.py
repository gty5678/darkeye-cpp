"""Compare Python and C++ component-gallery screenshots by visual error.

This is deliberately a cross-implementation check: unlike hash baselines, it
requires matching output geometry and bounds the RGB mean absolute error for
every corresponding scenario and theme.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

try:
    from PIL import Image, ImageChops, ImageStat
except ImportError as error:  # pragma: no cover - environment-specific guard
    raise SystemExit("Pillow is required for cross-gallery visual verification") from error


THEMES = (
    "light",
    "dark",
    "red",
    "green",
    "yellow",
    "blue",
    "purple",
    "light-custom-primary",
)
PAGES = (
    "buttons",
    "text",
    "toggles",
    "inputs",
    "containers",
    "data-nav",
    "p2-experience",
    "more",
    "advanced",
    "color-icons",
    "theme",
    "setting",
)


def visual_error(left: Path, right: Path) -> tuple[float, float]:
    with Image.open(left) as left_image, Image.open(right) as right_image:
        left_rgb = left_image.convert("RGB")
        right_rgb = right_image.convert("RGB")
        if left_rgb.size != right_rgb.size:
            raise ValueError(f"geometry differs: {left_rgb.size} != {right_rgb.size}")
        difference = ImageChops.difference(left_rgb, right_rgb)
        mean_error = sum(ImageStat.Stat(difference).mean) / 3
        pixels = (
            difference.get_flattened_data()
            if hasattr(difference, "get_flattened_data")
            else difference.getdata()
        )
        changed_ratio = sum(max(pixel) > 24 for pixel in pixels) / (left_rgb.width * left_rgb.height)
        return mean_error, changed_ratio


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp-directory", type=Path, required=True)
    parser.add_argument("--python-directory", type=Path, required=True)
    parser.add_argument("--max-mean-error", type=float, default=6.0)
    parser.add_argument("--max-different-pixel-ratio", type=float, default=0.15)
    parser.add_argument("--themes", nargs="+", default=THEMES)
    parser.add_argument("--pages", nargs="+", default=PAGES)
    arguments = parser.parse_args()

    failures: list[str] = []
    errors: list[float] = []
    changed_ratios: list[float] = []
    for theme in arguments.themes:
        for page in arguments.pages:
            relative_path = Path(theme) / f"component-gallery-{page}.png"
            cpp_path = arguments.cpp_directory / relative_path
            python_path = arguments.python_directory / relative_path
            if not cpp_path.is_file() or not python_path.is_file():
                failures.append(f"missing pair: {relative_path}")
                continue
            try:
                error, changed_ratio = visual_error(cpp_path, python_path)
            except ValueError as exception:
                failures.append(f"{relative_path}: {exception}")
                continue
            errors.append(error)
            changed_ratios.append(changed_ratio)
            if error > arguments.max_mean_error:
                failures.append(
                    f"{relative_path}: mean RGB error {error:.3f} exceeds "
                    f"{arguments.max_mean_error:.3f}"
                )
            if changed_ratio > arguments.max_different_pixel_ratio:
                failures.append(
                    f"{relative_path}: changed-pixel ratio {changed_ratio:.2%} exceeds "
                    f"{arguments.max_different_pixel_ratio:.2%}"
                )

    if failures:
        print("Cross-gallery visual verification failed:", file=sys.stderr)
        print("\n".join(f"- {failure}" for failure in failures), file=sys.stderr)
        return 1
    print(
        "Cross-gallery visual verification passed "
        f"({len(errors)} pairs; mean={sum(errors) / len(errors):.3f}; max={max(errors):.3f}; "
        f"changed-max={max(changed_ratios):.2%})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
