"""
Music video collection packager - converts a directory of music videos
into TinyJukebox SD card format.

Creates /MusicVideos/{CollectionName}/ directory with:
  collection.sdb  - 128-byte binary metadata
  collection.raw  - 108x67 RGB565 thumbnail
  V01.avi / V01.sdb / V01.raw
  V02.avi / V02.sdb / V02.raw
  ...
"""

import logging
import re
from pathlib import Path

from .binary_writer import CollectionInfo, VideoInfo, write_collection_metadata, write_video_metadata
from .video_converter import convert_video
from .thumbnail_generator import generate_thumbnail

logger = logging.getLogger(__name__)

MAX_ITEM_DIR_LEN = 31

VIDEO_EXTENSIONS = {".mp4", ".mkv", ".avi", ".mov", ".flv", ".wmv", ".webm"}


def sanitize_name(name: str) -> str:
    """Sanitize a name into a valid SD card directory name."""
    result = name.replace(" ", "_")
    result = re.sub(r'[^A-Za-z0-9_]', '', result)
    result = re.sub(r'_+', '_', result)
    result = result.strip('_')
    result = result[:MAX_ITEM_DIR_LEN]
    result = result.rstrip('_')
    if not result:
        result = "Collection"
    return result


def package_music_videos(
    input_dir: Path,
    output_dir: Path,
    collection_name: str,
    year: str = "",
    quality: int = 5,
    fps: int = 18,
) -> bool:
    """
    Convert a directory of music video files for TinyJukebox playback.

    Args:
        input_dir: Directory containing source video files
        output_dir: SD card output directory (root)
        collection_name: Display name for the collection
        year: Optional year string for metadata
        quality: MJPEG quality (2-10)
        fps: Target frame rate

    Returns:
        True if at least one video was converted successfully
    """
    input_dir = Path(input_dir)
    output_dir = Path(output_dir)

    if not input_dir.exists():
        logger.error("Input directory does not exist: %s", input_dir)
        return False

    # Scan for video files, sorted alphabetically
    video_files = sorted(
        f for f in input_dir.iterdir()
        if f.is_file() and f.suffix.lower() in VIDEO_EXTENSIONS
    )

    if not video_files:
        logger.error("No video files found in: %s", input_dir)
        return False

    logger.info("Found %d video files in %s", len(video_files), input_dir)

    # Create collection directory under /MusicVideos/
    dir_name = sanitize_name(collection_name)
    collection_dir = output_dir / "MusicVideos" / dir_name
    collection_dir.mkdir(parents=True, exist_ok=True)
    logger.info("Collection directory: MusicVideos/%s", dir_name)

    # Convert each video
    converted_count = 0
    first_converted_source = None

    for idx, video_file in enumerate(video_files, start=1):
        video_num = f"V{idx:02d}"
        title = video_file.stem.replace("_", " ")

        logger.info("Processing %s: %s", video_num, title)

        # Convert video
        output_avi = collection_dir / f"{video_num}.avi"
        if not convert_video(video_file, output_avi, fps=fps, quality=quality):
            logger.error("Video conversion failed for: %s", video_file)
            continue

        # Write video metadata
        write_video_metadata(
            collection_dir / f"{video_num}.sdb",
            VideoInfo(
                video_number=idx,
                title=title,
                artist="",
                runtime_minutes=0,
                description="",
            ),
        )

        # Generate video thumbnail
        generate_thumbnail(
            collection_dir / f"{video_num}.raw",
            video_path=video_file,
        )

        if first_converted_source is None:
            first_converted_source = video_file

        converted_count += 1

    if converted_count == 0:
        logger.error("No videos were converted successfully")
        return False

    # Write collection metadata
    write_collection_metadata(
        collection_dir / "collection.sdb",
        CollectionInfo(
            name=collection_name,
            year=year,
            video_count=converted_count,
        ),
    )

    # Generate collection thumbnail from first converted video
    generate_thumbnail(
        collection_dir / "collection.raw",
        video_path=first_converted_source,
    )

    logger.info("Collection packaged: %s (%d videos)", collection_name, converted_count)
    return True
