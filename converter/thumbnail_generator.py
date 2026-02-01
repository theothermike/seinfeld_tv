"""
Thumbnail generator for TinyTV episode/season browser.

Generates RGB565 big-endian raw thumbnail files for display on the TinyTV 2.
Thumbnails are 108x67 pixels = 14,472 bytes each.
"""

import subprocess
import logging
from pathlib import Path
from typing import Optional

from PIL import Image

logger = logging.getLogger(__name__)

THUMB_WIDTH = 108
THUMB_HEIGHT = 67
THUMB_SIZE_BYTES = THUMB_WIDTH * THUMB_HEIGHT * 2  # 14,472 bytes (RGB565)

# Frame to extract from video if no image available (in seconds)
DEFAULT_EXTRACT_TIME = 60


def rgb888_to_rgb565(r: int, g: int, b: int) -> int:
    """Convert RGB888 to RGB565 big-endian value."""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def image_to_rgb565(img: Image.Image) -> bytes:
    """
    Convert a PIL Image to RGB565 big-endian raw bytes.
    Image must already be resized to target dimensions.
    """
    img = img.convert("RGB")
    pixels = img.load()
    data = bytearray(img.width * img.height * 2)

    idx = 0
    for y in range(img.height):
        for x in range(img.width):
            r, g, b = pixels[x, y]
            val = rgb888_to_rgb565(r, g, b)
            # Big-endian byte order (high byte first)
            data[idx] = (val >> 8) & 0xFF
            data[idx + 1] = val & 0xFF
            idx += 2

    return bytes(data)


def generate_thumbnail_from_image(
    image_path: Path,
    output_path: Path,
) -> bool:
    """
    Generate RGB565 thumbnail from an image file.
    Resizes to 108x67 with letterboxing if needed.
    """
    try:
        img = Image.open(image_path)
        img.thumbnail((THUMB_WIDTH, THUMB_HEIGHT), Image.Resampling.LANCZOS)

        # Create padded image with black background
        padded = Image.new("RGB", (THUMB_WIDTH, THUMB_HEIGHT), (0, 0, 0))
        x_offset = (THUMB_WIDTH - img.width) // 2
        y_offset = (THUMB_HEIGHT - img.height) // 2
        padded.paste(img, (x_offset, y_offset))

        raw_data = image_to_rgb565(padded)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_bytes(raw_data)

        logger.info("Generated thumbnail: %s (%d bytes)", output_path.name, len(raw_data))
        return True

    except Exception as e:
        logger.error("Failed to generate thumbnail from %s: %s", image_path, e)
        return False


def generate_thumbnail_from_video(
    video_path: Path,
    output_path: Path,
    time_offset: int = DEFAULT_EXTRACT_TIME,
) -> bool:
    """
    Extract a frame from a video and generate RGB565 thumbnail.
    Falls back to frame at 10s if the specified time exceeds duration.
    """
    # Extract frame using ffmpeg to a temporary PNG
    temp_frame = output_path.with_suffix(".tmp.png")

    for t in [time_offset, 10, 1]:
        cmd = [
            "ffmpeg",
            "-ss", str(t),
            "-i", str(video_path),
            "-vframes", "1",
            "-y",
            str(temp_frame),
        ]

        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            if result.returncode == 0 and temp_frame.exists() and temp_frame.stat().st_size > 0:
                success = generate_thumbnail_from_image(temp_frame, output_path)
                temp_frame.unlink(missing_ok=True)
                return success
        except (subprocess.TimeoutExpired, FileNotFoundError) as e:
            logger.warning("Frame extraction at %ds failed: %s", t, e)
            continue

    temp_frame.unlink(missing_ok=True)
    logger.error("Failed to extract frame from: %s", video_path)
    return False


def generate_placeholder_thumbnail(output_path: Path, text: str = "") -> bool:
    """Generate a simple placeholder thumbnail with optional text."""
    try:
        img = Image.new("RGB", (THUMB_WIDTH, THUMB_HEIGHT), (32, 32, 32))

        if text:
            from PIL import ImageDraw
            draw = ImageDraw.Draw(img)
            # Use default font - small enough for thumbnail
            bbox = draw.textbbox((0, 0), text)
            text_w = bbox[2] - bbox[0]
            text_h = bbox[3] - bbox[1]
            x = (THUMB_WIDTH - text_w) // 2
            y = (THUMB_HEIGHT - text_h) // 2
            draw.text((x, y), text, fill=(180, 180, 180))

        raw_data = image_to_rgb565(img)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_bytes(raw_data)
        return True

    except Exception as e:
        logger.error("Failed to generate placeholder: %s", e)
        return False


def generate_thumbnail(
    output_path: Path,
    image_path: Optional[Path] = None,
    image_url: Optional[str] = None,
    video_path: Optional[Path] = None,
    download_func=None,
) -> bool:
    """
    Generate a thumbnail using the best available source.

    Priority: local image > downloaded URL image > video frame > placeholder
    """
    # Try local image first
    if image_path and image_path.exists():
        return generate_thumbnail_from_image(image_path, output_path)

    # Try downloading from URL
    if image_url and download_func:
        temp_image = output_path.with_suffix(".tmp.jpg")
        if download_func(image_url, temp_image):
            success = generate_thumbnail_from_image(temp_image, output_path)
            temp_image.unlink(missing_ok=True)
            if success:
                return True

    # Try extracting from video
    if video_path and video_path.exists():
        return generate_thumbnail_from_video(video_path, output_path)

    # Generate placeholder
    logger.warning("No image source available, generating placeholder for: %s", output_path)
    return generate_placeholder_thumbnail(output_path)
