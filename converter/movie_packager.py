"""
Movie packager - converts a single movie into TinyJukebox SD card format.

Creates /Movies/{MovieName}/ directory with:
  movie.sdb  - 128-byte binary metadata
  movie.raw  - 108x67 RGB565 thumbnail
  movie.avi  - Converted video
"""

import logging
import re
from pathlib import Path
from typing import Optional

from .metadata_fetcher import MetadataFetcher
from .binary_writer import MovieInfo, write_movie_metadata
from .video_converter import convert_video
from .thumbnail_generator import generate_thumbnail

logger = logging.getLogger(__name__)

MAX_ITEM_DIR_LEN = 31


def sanitize_name(name: str) -> str:
    """Sanitize a name into a valid SD card directory name."""
    result = name.replace(" ", "_")
    result = re.sub(r'[^A-Za-z0-9_]', '', result)
    result = re.sub(r'_+', '_', result)
    result = result.strip('_')
    result = result[:MAX_ITEM_DIR_LEN]
    result = result.rstrip('_')
    if not result:
        result = "Movie"
    return result


def package_movie(
    input_file: Path,
    output_dir: Path,
    title: str,
    tmdb_key: Optional[str] = None,
    quality: int = 5,
    fps: int = 18,
) -> bool:
    """
    Convert a single movie file for TinyJukebox playback.

    Args:
        input_file: Source video file
        output_dir: SD card output directory (root)
        title: Movie title for metadata lookup
        tmdb_key: Optional TMDB API key
        quality: MJPEG quality (2-10)
        fps: Target frame rate

    Returns:
        True if packaging succeeded
    """
    if not input_file.exists():
        logger.error("Input file does not exist: %s", input_file)
        return False

    # Fetch metadata from TMDB
    fetcher = MetadataFetcher(tmdb_key=tmdb_key)
    movie_data = fetcher.fetch_movie(title)

    # Create movie directory under /Movies/
    dir_name = sanitize_name(movie_data["title"])
    movies_dir = output_dir / "Movies"
    movie_dir = movies_dir / dir_name
    movie_dir.mkdir(parents=True, exist_ok=True)
    logger.info("Movie directory: Movies/%s", dir_name)

    # Convert video
    output_avi = movie_dir / "movie.avi"
    if not convert_video(input_file, output_avi, fps=fps, quality=quality):
        logger.error("Video conversion failed for: %s", input_file)
        return False

    # Write metadata
    write_movie_metadata(
        movie_dir / "movie.sdb",
        MovieInfo(
            title=movie_data["title"],
            year=movie_data["year"],
            runtime_minutes=movie_data["runtime_minutes"],
            description=movie_data["description"],
        ),
    )

    # Generate thumbnail
    generate_thumbnail(
        movie_dir / "movie.raw",
        image_url=movie_data["poster_url"],
        video_path=input_file,
        download_func=fetcher.download_image,
    )

    logger.info("Movie packaged: %s", movie_data["title"])
    return True
