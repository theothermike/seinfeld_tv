#!/usr/bin/env python3
"""Generate mediaIcons.h from 48x48 PNG source icons.

Reads PNG files from firmware/TinyJukebox/icons_src/ and converts each to a
PROGMEM uint16_t array of RGB565 values. Black (0,0,0) pixels are treated
as transparent at draw time.

Can also generate simple pixel-art icons programmatically when no PNG sources
exist (--generate flag).

Usage:
    .venv/bin/python scripts/generate_icons.py [--generate]
"""

import argparse
import struct
import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw
except ImportError:
    print("Pillow is required: pip install Pillow", file=sys.stderr)
    sys.exit(1)

ICON_SIZE = 48
ICONS_SRC = Path(__file__).parent.parent / "firmware" / "TinyJukebox" / "icons_src"
OUTPUT = Path(__file__).parent.parent / "firmware" / "TinyJukebox" / "mediaIcons.h"

# Icon names matching MediaType enum order + Settings
ICON_NAMES = [
    "tv",
    "movies",
    "music_videos",
    "music",
    "photos",
    "youtube",
    "settings",
]

ICON_ARRAY_NAMES = [
    "iconTV",
    "iconMovies",
    "iconMusicVideos",
    "iconMusic",
    "iconPhotos",
    "iconYouTube",
    "iconSettings",
]


def rgb_to_565(r: int, g: int, b: int) -> int:
    """Convert 8-bit RGB to RGB565."""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def png_to_rgb565(img: Image.Image) -> list[int]:
    """Convert a PIL Image to a flat list of RGB565 values."""
    img = img.convert("RGB").resize((ICON_SIZE, ICON_SIZE), Image.NEAREST)
    pixels = []
    for y in range(ICON_SIZE):
        for x in range(ICON_SIZE):
            r, g, b = img.getpixel((x, y))
            pixels.append(rgb_to_565(r, g, b))
    return pixels


def draw_filled_circle(draw, cx, cy, r, color):
    """Draw a filled circle."""
    draw.ellipse([cx - r, cy - r, cx + r, cy + r], fill=color)


def draw_rect(draw, x, y, w, h, color):
    """Draw a filled rectangle."""
    draw.rectangle([x, y, x + w - 1, y + h - 1], fill=color)


def draw_rounded_rect(draw, x, y, w, h, radius, color):
    """Draw a rounded rectangle."""
    draw.rounded_rectangle([x, y, x + w - 1, y + h - 1], radius=radius, fill=color)


def generate_tv_icon() -> Image.Image:
    """Retro CRT TV with antenna - blue-gray body."""
    img = Image.new("RGB", (ICON_SIZE, ICON_SIZE), (0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Antenna
    antenna_color = (160, 170, 180)
    draw.line([(24, 4), (16, 14)], fill=antenna_color, width=2)
    draw.line([(24, 4), (32, 14)], fill=antenna_color, width=2)

    # TV body - blue-gray
    body_color = (80, 100, 130)
    draw_rounded_rect(draw, 6, 14, 36, 26, 3, body_color)

    # Screen - darker blue
    screen_color = (30, 50, 90)
    draw_rect(draw, 10, 18, 22, 18, screen_color)

    # Screen glare
    glare_color = (60, 80, 120)
    draw.line([(12, 20), (12, 28)], fill=glare_color, width=1)

    # Knobs on right side
    knob_color = (200, 200, 200)
    draw_filled_circle(draw, 36, 22, 2, knob_color)
    draw_filled_circle(draw, 36, 30, 2, knob_color)

    # Stand/legs
    stand_color = (60, 75, 100)
    draw_rect(draw, 12, 40, 4, 4, stand_color)
    draw_rect(draw, 32, 40, 4, 4, stand_color)

    return img


def generate_movies_icon() -> Image.Image:
    """Film clapperboard - black/white."""
    img = Image.new("RGB", (ICON_SIZE, ICON_SIZE), (0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Clapper top (striped part) - angled
    top_color = (50, 50, 55)
    draw.polygon([(6, 8), (42, 8), (42, 18), (6, 18)], fill=top_color)

    # Diagonal stripes on clapper
    stripe_color = (220, 220, 220)
    for sx in range(6, 42, 8):
        draw.polygon(
            [(sx, 8), (sx + 4, 8), (sx + 6, 18), (sx + 2, 18)], fill=stripe_color
        )

    # Board body
    board_color = (45, 45, 50)
    draw_rect(draw, 6, 18, 36, 22, board_color)

    # Text lines on board
    line_color = (180, 180, 180)
    draw.line([(10, 24), (38, 24)], fill=line_color, width=1)
    draw.line([(10, 30), (30, 30)], fill=line_color, width=1)
    draw.line([(10, 36), (34, 36)], fill=line_color, width=1)

    return img


def generate_music_videos_icon() -> Image.Image:
    """Play triangle with music note - combined icon."""
    img = Image.new("RGB", (ICON_SIZE, ICON_SIZE), (0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Play triangle outline
    play_color = (100, 200, 100)
    draw.polygon([(10, 8), (38, 24), (10, 40)], outline=play_color, fill=None)
    # Thicker outline
    draw.polygon([(11, 9), (37, 24), (11, 39)], outline=play_color, fill=None)

    # Music note inside
    note_color = (220, 220, 100)
    # Stem
    draw.line([(26, 16), (26, 32)], fill=note_color, width=2)
    # Note head
    draw_filled_circle(draw, 23, 32, 3, note_color)
    # Flag
    draw.line([(27, 16), (31, 20)], fill=note_color, width=2)

    return img


def generate_music_icon() -> Image.Image:
    """Double eighth note - classic music symbol."""
    img = Image.new("RGB", (ICON_SIZE, ICON_SIZE), (0, 0, 0))
    draw = ImageDraw.Draw(img)

    note_color = (100, 180, 255)
    highlight = (150, 210, 255)

    # Left stem
    draw.line([(14, 10), (14, 34)], fill=note_color, width=2)
    # Right stem
    draw.line([(34, 10), (34, 30)], fill=note_color, width=2)

    # Beam connecting stems at top
    draw.line([(14, 10), (34, 10)], fill=highlight, width=3)
    # Second beam
    draw.line([(14, 15), (34, 15)], fill=highlight, width=2)

    # Left note head (oval)
    draw.ellipse([8, 32, 18, 40], fill=note_color)
    # Right note head
    draw.ellipse([28, 28, 38, 36], fill=note_color)

    return img


def generate_photos_icon() -> Image.Image:
    """Camera with lens."""
    img = Image.new("RGB", (ICON_SIZE, ICON_SIZE), (0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Camera body
    body_color = (120, 120, 130)
    draw_rounded_rect(draw, 6, 16, 36, 22, 3, body_color)

    # Viewfinder bump
    bump_color = (100, 100, 110)
    draw_rect(draw, 18, 12, 12, 6, bump_color)

    # Lens outer ring
    lens_outer = (70, 70, 80)
    draw_filled_circle(draw, 24, 27, 8, lens_outer)

    # Lens inner
    lens_inner = (40, 40, 60)
    draw_filled_circle(draw, 24, 27, 5, lens_inner)

    # Lens highlight
    lens_hl = (80, 80, 100)
    draw_filled_circle(draw, 22, 25, 2, lens_hl)

    # Flash
    flash_color = (255, 255, 200)
    draw_rect(draw, 34, 18, 4, 3, flash_color)

    return img


def generate_youtube_icon() -> Image.Image:
    """Red rounded rectangle with white play triangle."""
    img = Image.new("RGB", (ICON_SIZE, ICON_SIZE), (0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Red rounded rectangle background
    yt_red = (255, 0, 0)
    draw_rounded_rect(draw, 4, 10, 40, 28, 8, yt_red)

    # White play triangle
    white = (255, 255, 255)
    draw.polygon([(19, 17), (33, 24), (19, 31)], fill=white)

    return img


def generate_settings_icon() -> Image.Image:
    """Gear/cog wheel."""
    img = Image.new("RGB", (ICON_SIZE, ICON_SIZE), (0, 0, 0))
    draw = ImageDraw.Draw(img)
    import math

    cx, cy = 24, 24
    gear_color = (160, 160, 170)
    inner_color = (80, 80, 90)

    # Draw gear teeth
    num_teeth = 8
    outer_r = 18
    inner_r = 13
    tooth_half_angle = math.pi / (num_teeth * 2)

    for i in range(num_teeth):
        angle = i * 2 * math.pi / num_teeth
        # Outer tooth points
        a1 = angle - tooth_half_angle
        a2 = angle + tooth_half_angle
        x1 = cx + outer_r * math.cos(a1)
        y1 = cy + outer_r * math.sin(a1)
        x2 = cx + outer_r * math.cos(a2)
        y2 = cy + outer_r * math.sin(a2)
        # Draw tooth as small rectangle from inner to outer
        ix1 = cx + inner_r * math.cos(a1)
        iy1 = cy + inner_r * math.sin(a1)
        ix2 = cx + inner_r * math.cos(a2)
        iy2 = cy + inner_r * math.sin(a2)
        draw.polygon([(x1, y1), (x2, y2), (ix2, iy2), (ix1, iy1)], fill=gear_color)

    # Main gear circle
    draw_filled_circle(draw, cx, cy, inner_r, gear_color)

    # Inner hole
    draw_filled_circle(draw, cx, cy, 7, inner_color)

    # Center dot
    draw_filled_circle(draw, cx, cy, 3, gear_color)

    return img


GENERATORS = {
    "tv": generate_tv_icon,
    "movies": generate_movies_icon,
    "music_videos": generate_music_videos_icon,
    "music": generate_music_icon,
    "photos": generate_photos_icon,
    "youtube": generate_youtube_icon,
    "settings": generate_settings_icon,
}


def generate_header(icons: dict[str, list[int]]) -> str:
    """Generate the C header file content."""
    lines = [
        "//-------------------------------------------------------------------------------",
        "//  TinyJukebox - Media Type Icons (48x48 RGB565)",
        "//",
        "//  Auto-generated by scripts/generate_icons.py - DO NOT EDIT MANUALLY",
        "//  Each icon is 48x48 pixels, stored as RGB565 in PROGMEM.",
        "//  Black (0x0000) pixels are treated as transparent by drawIcon().",
        "//-------------------------------------------------------------------------------",
        "",
        "#ifndef MEDIA_ICONS_H",
        "#define MEDIA_ICONS_H",
        "",
        "#include <stdint.h>",
        "#include <Arduino.h>",
        "",
        f"#define ICON_W {ICON_SIZE}",
        f"#define ICON_H {ICON_SIZE}",
        f"#define ICON_PIXELS ({ICON_SIZE} * {ICON_SIZE})",
        "",
    ]

    for name, array_name in zip(ICON_NAMES, ICON_ARRAY_NAMES):
        pixels = icons[name]
        lines.append(
            f"static const uint16_t PROGMEM {array_name}[ICON_PIXELS] = {{"
        )

        # Write 12 values per line
        for i in range(0, len(pixels), 12):
            chunk = pixels[i : i + 12]
            hex_vals = ", ".join(f"0x{v:04X}" for v in chunk)
            comma = "," if i + 12 < len(pixels) else ""
            lines.append(f"  {hex_vals}{comma}")

        lines.append("};")
        lines.append("")

    # Icon lookup table
    lines.append("// Icon lookup table indexed by MediaType enum (+ Settings at end)")
    lines.append(
        "static const uint16_t* const PROGMEM mediaIcons[] = {"
    )
    for array_name in ICON_ARRAY_NAMES:
        lines.append(f"  {array_name},")
    lines.append("};")
    lines.append("")
    lines.append("#endif // MEDIA_ICONS_H")
    lines.append("")

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="Generate mediaIcons.h")
    parser.add_argument(
        "--generate",
        action="store_true",
        help="Generate pixel-art icons programmatically (no PNG sources needed)",
    )
    args = parser.parse_args()

    icons = {}

    if args.generate:
        ICONS_SRC.mkdir(parents=True, exist_ok=True)
        for name in ICON_NAMES:
            print(f"Generating {name} icon...")
            img = GENERATORS[name]()
            img.save(ICONS_SRC / f"{name}.png")
            icons[name] = png_to_rgb565(img)
    else:
        for name in ICON_NAMES:
            png_path = ICONS_SRC / f"{name}.png"
            if not png_path.exists():
                print(f"Missing {png_path}, run with --generate first", file=sys.stderr)
                sys.exit(1)
            print(f"Reading {png_path}...")
            img = Image.open(png_path)
            icons[name] = png_to_rgb565(img)

    header = generate_header(icons)
    OUTPUT.write_text(header)
    print(f"Wrote {OUTPUT}")

    # Stats
    total_pixels = ICON_SIZE * ICON_SIZE * len(ICON_NAMES)
    total_bytes = total_pixels * 2
    print(f"Total: {len(ICON_NAMES)} icons, {total_bytes:,} bytes ({total_bytes / 1024:.1f} KB)")


if __name__ == "__main__":
    main()
