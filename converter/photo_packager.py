"""
Photo album packager - converts a directory of photos into TinyJukebox SD card format.

Creates /Photos/{AlbumName}/ directory with:
  album.sdb  - 64-byte binary metadata (photo album)
  album.raw  - 108x67 RGB565 thumbnail of first photo
  P01.raw / P01.sdb
  P02.raw / P02.sdb
  ...

Photos are stored as 210x135 RGB565 big-endian .raw files (56,700 bytes each).
"""

import logging
import re
from pathlib import Path

from PIL import Image

from .binary_writer import PhotoAlbumInfo, PhotoInfo, write_photo_album_metadata, write_photo_metadata
from .thumbnail_generator import generate_thumbnail, image_to_rgb565

logger = logging.getLogger(__name__)

MAX_ITEM_DIR_LEN = 31

# Full-screen photo dimensions for TinyTV 2
PHOTO_WIDTH = 210
PHOTO_HEIGHT = 135
PHOTO_SIZE_BYTES = PHOTO_WIDTH * PHOTO_HEIGHT * 2  # 56,700 bytes (RGB565)

# Supported image extensions (case-insensitive)
IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png", ".bmp", ".tiff", ".webp"}


def sanitize_name(name: str) -> str:
    """Sanitize a name into a valid SD card directory name."""
    result = name.replace(" ", "_")
    result = re.sub(r'[^A-Za-z0-9_]', '', result)
    result = re.sub(r'_+', '_', result)
    result = result.strip('_')
    result = result[:MAX_ITEM_DIR_LEN]
    result = result.rstrip('_')
    if not result:
        result = "Album"
    return result


def convert_photo_to_raw(
    input_path: Path,
    output_path: Path,
    width: int = PHOTO_WIDTH,
    height: int = PHOTO_HEIGHT,
) -> bool:
    """
    Convert an image file to RGB565 big-endian .raw format.

    Uses cover crop: resizes to fill the target dimensions, then center-crops
    to exactly width x height.

    Args:
        input_path: Source image file
        output_path: Destination .raw file
        width: Target width in pixels (default 210)
        height: Target height in pixels (default 135)

    Returns:
        True on success, False on error
    """
    try:
        img = Image.open(input_path)
        img = img.convert("RGB")

        # Cover crop: resize to fill, then center crop
        src_w, src_h = img.size
        target_ratio = width / height
        src_ratio = src_w / src_h

        if src_ratio > target_ratio:
            # Source is wider - fit height, crop width
            new_h = height
            new_w = int(src_w * height / src_h)
        else:
            # Source is taller - fit width, crop height
            new_w = width
            new_h = int(src_h * width / src_w)

        img = img.resize((new_w, new_h), Image.Resampling.LANCZOS)

        # Center crop to exact dimensions
        left = (new_w - width) // 2
        top = (new_h - height) // 2
        img = img.crop((left, top, left + width, top + height))

        raw_data = image_to_rgb565(img)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_bytes(raw_data)

        logger.info("Converted photo: %s -> %s (%d bytes)", input_path.name, output_path.name, len(raw_data))
        return True

    except Exception as e:
        logger.error("Failed to convert photo %s: %s", input_path, e)
        return False


def package_photos(
    input_dir: Path,
    output_dir: Path,
    album_name: str,
    quality: int = 5,
    fps: int = 18,
) -> bool:
    """
    Convert a directory of photos for TinyJukebox playback.

    Args:
        input_dir: Directory containing source images
        output_dir: SD card output directory (root)
        album_name: Album name for metadata and directory
        quality: Unused, kept for API consistency with other packagers
        fps: Unused, kept for API consistency with other packagers

    Returns:
        True if at least one photo was converted successfully
    """
    input_dir = Path(input_dir)
    output_dir = Path(output_dir)

    if not input_dir.exists() or not input_dir.is_dir():
        logger.error("Input directory does not exist: %s", input_dir)
        return False

    # Scan for image files, sorted alphabetically
    image_files = sorted(
        f for f in input_dir.iterdir()
        if f.is_file() and f.suffix.lower() in IMAGE_EXTENSIONS
    )

    if not image_files:
        logger.error("No image files found in: %s", input_dir)
        return False

    # Create album directory under /Photos/
    dir_name = sanitize_name(album_name)
    photos_dir = output_dir / "Photos"
    album_dir = photos_dir / dir_name
    album_dir.mkdir(parents=True, exist_ok=True)
    logger.info("Photo album directory: Photos/%s", dir_name)

    # Convert each photo
    converted_count = 0
    first_image_path = None

    for i, image_file in enumerate(image_files, start=1):
        photo_num = f"P{i:02d}"

        # Convert to full-screen .raw
        raw_path = album_dir / f"{photo_num}.raw"
        if convert_photo_to_raw(image_file, raw_path):
            converted_count += 1
            if first_image_path is None:
                first_image_path = image_file

            # Write photo metadata
            caption = image_file.stem
            write_photo_metadata(
                album_dir / f"{photo_num}.sdb",
                PhotoInfo(
                    photo_number=i,
                    caption=caption,
                    date_taken="",
                ),
            )
        else:
            logger.warning("Skipping photo %s: conversion failed", image_file.name)

    if converted_count == 0:
        logger.error("No photos were converted successfully")
        return False

    # Write album metadata
    write_photo_album_metadata(
        album_dir / "album.sdb",
        PhotoAlbumInfo(
            title=album_name,
            photo_count=converted_count,
        ),
    )

    # Generate album thumbnail from first image (108x67)
    generate_thumbnail(
        album_dir / "album.raw",
        image_path=first_image_path,
    )

    logger.info("Photo album packaged: %s (%d photos)", album_name, converted_count)
    return True
