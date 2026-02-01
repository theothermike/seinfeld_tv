"""
Device packager - orchestrates SD card output directory assembly.

Coordinates metadata fetching, video conversion, thumbnail generation,
and binary metadata writing to produce a complete SD card layout.
"""

import logging
import re
from pathlib import Path
from typing import Optional

from .metadata_fetcher import MetadataFetcher, ShowData
from .binary_writer import (
    ShowInfo, SeasonInfo, EpisodeInfo,
    write_show_metadata, write_season_metadata, write_episode_metadata,
)
from .video_converter import (
    scan_input_directory, convert_video, convert_boot_clip, EpisodeFile,
)
from .thumbnail_generator import generate_thumbnail

logger = logging.getLogger(__name__)

# Maximum length for sanitized show directory names (matching firmware SHOW_DIR_LEN - 1)
MAX_SHOW_DIR_LEN = 31


def sanitize_show_name(name: str) -> str:
    """
    Sanitize a show name into a valid directory name for the SD card.

    Converts to alphanumeric + underscores, truncates to MAX_SHOW_DIR_LEN chars.
    Leading/trailing underscores are stripped.

    Examples:
        "Seinfeld" -> "Seinfeld"
        "It's Always Sunny" -> "Its_Always_Sunny"
        "The Office (US)" -> "The_Office_US"
    """
    # Replace spaces with underscores
    result = name.replace(" ", "_")
    # Strip anything that isn't alphanumeric or underscore
    result = re.sub(r'[^A-Za-z0-9_]', '', result)
    # Collapse multiple underscores
    result = re.sub(r'_+', '_', result)
    # Strip leading/trailing underscores
    result = result.strip('_')
    # Truncate
    result = result[:MAX_SHOW_DIR_LEN]
    # Strip trailing underscore after truncation
    result = result.rstrip('_')
    # Fallback if empty
    if not result:
        result = "Show"
    return result


def package_show(
    input_dir: Path,
    output_dir: Path,
    show_name: str,
    tmdb_key: Optional[str] = None,
    season_filter: Optional[int] = None,
    episode_filter: Optional[set[int]] = None,
    quality: int = 5,
    fps: int = 24,
    boot_clip: Optional[Path] = None,
) -> bool:
    """
    Full pipeline: fetch metadata, convert videos, generate thumbnails,
    write binary metadata, assemble SD card output.

    Args:
        input_dir: Directory containing source video files
        output_dir: SD card output directory
        show_name: Show name for API lookup
        tmdb_key: Optional TMDB API key for better artwork
        season_filter: Optional season number to process only one season
        quality: MJPEG quality (2-10, lower is better)
        fps: Target frame rate
        boot_clip: Optional path to theme music clip

    Returns:
        True if packaging succeeded (at least partially)
    """
    output_dir.mkdir(parents=True, exist_ok=True)

    # Fetch metadata
    fetcher = MetadataFetcher(tmdb_key=tmdb_key)
    show_data = fetcher.fetch_show(show_name)
    if show_data is None:
        logger.error("Failed to fetch show metadata for: %s", show_name)
        return False

    # Fetch TMDB images if key provided
    tmdb_images = fetcher.fetch_tmdb_images(show_name) if tmdb_key else {}

    # Scan input directory for video files
    episode_files = scan_input_directory(input_dir, season_filter=season_filter)
    if not episode_files:
        logger.error("No episode files found in: %s", input_dir)
        return False

    # Build lookup of episode files by (season, episode)
    file_lookup: dict[tuple[int, int], EpisodeFile] = {
        (ef.season, ef.episode): ef for ef in episode_files
    }

    # Filter show data to only seasons we have files for
    available_seasons = {ef.season for ef in episode_files}
    if season_filter is not None:
        available_seasons = {season_filter} & available_seasons

    seasons_to_process = [
        s for s in show_data.seasons if s.number in available_seasons
    ]

    if not seasons_to_process:
        logger.error("No matching seasons found between metadata and files")
        return False

    # Count total episodes we'll process
    total_ep_count = sum(
        sum(1 for ep in s.episodes
            if (s.number, ep.number) in file_lookup
            and (episode_filter is None or ep.number in episode_filter))
        for s in seasons_to_process
    )

    # Create show subdirectory using sanitized name
    show_dir_name = sanitize_show_name(show_data.name)
    show_dir = output_dir / show_dir_name
    show_dir.mkdir(parents=True, exist_ok=True)
    logger.info("Show directory: %s", show_dir_name)

    # Write show metadata
    write_show_metadata(
        show_dir / "show.sdb",
        ShowInfo(
            name=show_data.name,
            year=show_data.year,
            season_count=len(seasons_to_process),
            total_episodes=total_ep_count,
        ),
    )

    # Generate show title card thumbnail
    show_image_url = tmdb_images.get("show") or show_data.image_url
    first_ep_file = None
    for s in seasons_to_process:
        for ep in s.episodes:
            if (s.number, ep.number) in file_lookup:
                first_ep_file = file_lookup[(s.number, ep.number)]
                break
        if first_ep_file:
            break
    generate_thumbnail(
        show_dir / "show.raw",
        image_url=show_image_url,
        video_path=first_ep_file.path if first_ep_file else None,
        download_func=fetcher.download_image,
    )

    # Convert boot clip if provided (goes to root output_dir)
    if boot_clip and boot_clip.exists():
        convert_boot_clip(boot_clip, output_dir / "boot.avi", fps=fps, quality=quality)

    # Write default settings (root level, don't overwrite existing)
    settings_path = output_dir / "settings.txt"
    if not settings_path.exists():
        _write_settings(settings_path, show_dir_name=show_dir_name)

    # Process each season
    converted_count = 0
    for season_data in seasons_to_process:
        s_num = season_data.number
        season_dir = show_dir / f"S{s_num:02d}"
        season_dir.mkdir(parents=True, exist_ok=True)

        # Episodes in this season that we have files for (and match episode filter)
        season_episodes = [
            ep for ep in season_data.episodes
            if (s_num, ep.number) in file_lookup
            and (episode_filter is None or ep.number in episode_filter)
        ]

        # Write season metadata
        write_season_metadata(
            season_dir / "season.sdb",
            SeasonInfo(
                season_number=s_num,
                episode_count=len(season_episodes),
                year=season_data.premiere_date[:4] if season_data.premiere_date else show_data.year,
                title=season_data.name or f"Season {s_num}",
            ),
        )

        # Generate season thumbnail
        season_image_url = tmdb_images.get(f"s{s_num}") or season_data.image_url
        first_ep_file = file_lookup.get((s_num, season_episodes[0].number)) if season_episodes else None
        generate_thumbnail(
            season_dir / "thumb.raw",
            image_url=season_image_url,
            video_path=first_ep_file.path if first_ep_file else None,
            download_func=fetcher.download_image,
        )

        # Process each episode
        for ep_data in season_episodes:
            ep_file = file_lookup[(s_num, ep_data.number)]
            ep_prefix = f"E{ep_data.number:02d}"

            # Convert video
            output_avi = season_dir / f"{ep_prefix}.avi"
            if convert_video(ep_file.path, output_avi, fps=fps, quality=quality):
                converted_count += 1
            else:
                logger.warning("Skipping failed conversion: S%02dE%02d", s_num, ep_data.number)
                continue

            # Write episode metadata
            write_episode_metadata(
                season_dir / f"{ep_prefix}.sdb",
                EpisodeInfo(
                    season_number=s_num,
                    episode_number=ep_data.number,
                    title=ep_data.title,
                    air_date=ep_data.air_date,
                    runtime_minutes=ep_data.runtime,
                    description=ep_data.summary[:56] if ep_data.summary else "",
                ),
            )

            # Generate episode thumbnail
            ep_image_url = tmdb_images.get(f"s{s_num}e{ep_data.number}") or ep_data.image_url
            generate_thumbnail(
                season_dir / f"{ep_prefix}.raw",
                image_url=ep_image_url,
                video_path=ep_file.path,
                download_func=fetcher.download_image,
            )

    logger.info("Packaging complete: %d/%d episodes converted", converted_count, total_ep_count)
    return converted_count > 0


def _write_settings(path: Path, show_dir_name: str = "") -> None:
    """Write default settings.txt for firmware state persistence."""
    path.write_text(
        "# SeinfeldTV Settings\n"
        f"last_show={show_dir_name}\n"
        "last_season=1\n"
        "last_episode=1\n"
        "volume=5\n"
    )
