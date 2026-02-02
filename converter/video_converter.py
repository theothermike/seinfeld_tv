"""
ffmpeg wrapper for converting video files to TinyTV 2 compatible format.

Target format:
- Resolution: 210x135 (exact TinyTV 2 video viewport)
- Video codec: MJPEG
- Audio codec: PCM unsigned 8-bit
- Audio sample rate: 11025 Hz mono
- Container: AVI
"""

import re
import subprocess
import logging
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

logger = logging.getLogger(__name__)

# Regex patterns for detecting season/episode from filenames
EPISODE_PATTERNS = [
    re.compile(r"[Ss](\d+)[Ee](\d+)"),           # S01E01
    re.compile(r"(\d+)x(\d+)"),                     # 1x01
    re.compile(r"[Ss]eason\s*(\d+).*[Ee]pisode\s*(\d+)", re.IGNORECASE),
    re.compile(r"(\d{1,2})(\d{2})\b"),              # 101 (season 1, episode 01)
]

TARGET_WIDTH = 210
TARGET_HEIGHT = 135
DEFAULT_FPS = 18
DEFAULT_QUALITY = 5  # MJPEG quality (lower = better, 2-10)
AUDIO_SAMPLE_RATE = 11025


@dataclass
class EpisodeFile:
    path: Path
    season: int
    episode: int


def detect_episode_info(filepath: Path) -> Optional[tuple[int, int]]:
    """
    Try to detect season and episode number from a filename.
    Returns (season, episode) tuple or None if undetectable.
    """
    name = filepath.stem
    for pattern in EPISODE_PATTERNS:
        match = pattern.search(name)
        if match:
            season = int(match.group(1))
            episode = int(match.group(2))
            if 1 <= season <= 99 and 1 <= episode <= 999:
                logger.debug("Detected S%02dE%02d from: %s", season, episode, name)
                return (season, episode)
    return None


def scan_input_directory(input_dir: Path, season_filter: Optional[int] = None) -> list[EpisodeFile]:
    """
    Scan a directory for video files and detect episode info.
    Files are sorted by season then episode number.
    """
    video_extensions = {".mp4", ".mkv", ".avi", ".mov", ".wmv", ".flv", ".webm", ".m4v"}
    episodes = []

    for filepath in sorted(input_dir.rglob("*")):
        if filepath.suffix.lower() not in video_extensions:
            continue

        info = detect_episode_info(filepath)
        if info is None:
            logger.warning("Could not detect episode info for: %s", filepath.name)
            continue

        season, episode = info
        if season_filter is not None and season != season_filter:
            continue

        episodes.append(EpisodeFile(path=filepath, season=season, episode=episode))

    episodes.sort(key=lambda e: (e.season, e.episode))
    logger.info("Found %d episode files in %s", len(episodes), input_dir)
    return episodes


def convert_video(
    input_path: Path,
    output_path: Path,
    fps: int = DEFAULT_FPS,
    quality: int = DEFAULT_QUALITY,
) -> bool:
    """
    Convert a video file to TinyTV format using ffmpeg.

    Args:
        input_path: Source video file
        output_path: Destination AVI file
        fps: Target frame rate
        quality: MJPEG quality (2-10, lower is better)

    Returns:
        True if conversion succeeded
    """
    output_path.parent.mkdir(parents=True, exist_ok=True)

    cmd = [
        "ffmpeg",
        "-i", str(input_path),
        "-vf", (
            f"scale={TARGET_WIDTH}:{TARGET_HEIGHT}"
            f":force_original_aspect_ratio=decrease,"
            f"pad={TARGET_WIDTH}:{TARGET_HEIGHT}"
            f":(ow-iw)/2:(oh-ih)/2"
        ),
        "-vcodec", "mjpeg",
        "-q:v", str(quality),
        "-acodec", "pcm_u8",
        "-ar", str(AUDIO_SAMPLE_RATE),
        "-ac", "1",
        "-r", str(fps),
        "-y",
        str(output_path),
    ]

    logger.info("Converting: %s -> %s", input_path.name, output_path.name)
    logger.debug("ffmpeg command: %s", " ".join(cmd))

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=3600,  # 1 hour timeout for long episodes
        )
        if result.returncode != 0:
            logger.error("ffmpeg failed for %s:\n%s", input_path.name, result.stderr[-500:])
            return False

        logger.info("Converted successfully: %s (%.1f MB)",
                     output_path.name, output_path.stat().st_size / 1_048_576)
        return True

    except subprocess.TimeoutExpired:
        logger.error("ffmpeg timed out for: %s", input_path.name)
        return False
    except FileNotFoundError:
        logger.error("ffmpeg not found. Please install ffmpeg.")
        return False


def convert_boot_clip(
    input_path: Path,
    output_path: Path,
    max_duration: int = 10,
    fps: int = DEFAULT_FPS,
    quality: int = DEFAULT_QUALITY,
) -> bool:
    """Convert a short boot clip (theme music) with duration limit."""
    output_path.parent.mkdir(parents=True, exist_ok=True)

    cmd = [
        "ffmpeg",
        "-i", str(input_path),
        "-t", str(max_duration),
        "-vf", (
            f"scale={TARGET_WIDTH}:{TARGET_HEIGHT}"
            f":force_original_aspect_ratio=decrease,"
            f"pad={TARGET_WIDTH}:{TARGET_HEIGHT}"
            f":(ow-iw)/2:(oh-ih)/2"
        ),
        "-vcodec", "mjpeg",
        "-q:v", str(quality),
        "-acodec", "pcm_u8",
        "-ar", str(AUDIO_SAMPLE_RATE),
        "-ac", "1",
        "-r", str(fps),
        "-y",
        str(output_path),
    ]

    logger.info("Converting boot clip: %s", input_path.name)
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
        if result.returncode != 0:
            logger.error("Boot clip conversion failed:\n%s", result.stderr[-500:])
            return False
        return True
    except (subprocess.TimeoutExpired, FileNotFoundError) as e:
        logger.error("Boot clip conversion error: %s", e)
        return False
