"""
YouTube video/playlist packager - downloads and converts YouTube videos
into TinyJukebox SD card format using yt-dlp.

Creates /YouTube/{PlaylistName}/ directory with:
  playlist.sdb  - 128-byte binary metadata
  playlist.raw  - 108x67 RGB565 thumbnail
  Y01.avi / Y01.sdb / Y01.raw
  Y02.avi / Y02.sdb / Y02.raw
  ...
"""

import json
import logging
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Optional

from .binary_writer import (
    YouTubePlaylistInfo,
    YouTubeVideoInfo,
    write_youtube_playlist_metadata,
    write_youtube_video_metadata,
)
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
        result = "YouTube"
    return result


def _extract_year(date_str: str) -> str:
    """Extract a 4-digit year from a date string like '20230415' or '2023-04-15'."""
    if not date_str:
        return ""
    # yt-dlp typically returns YYYYMMDD
    cleaned = date_str.replace("-", "")
    if len(cleaned) >= 4 and cleaned[:4].isdigit():
        return cleaned[:4]
    return ""


def _format_upload_date(date_str: str) -> str:
    """Format yt-dlp date string (YYYYMMDD) to YYYY-MM-DD."""
    if not date_str:
        return ""
    cleaned = date_str.replace("-", "")
    if len(cleaned) == 8 and cleaned.isdigit():
        return f"{cleaned[:4]}-{cleaned[4:6]}-{cleaned[6:8]}"
    return date_str[:12]  # truncate to field size


def get_video_info(url: str, cookies_from_browser: Optional[str] = None) -> list[dict]:
    """
    Fetch video metadata from YouTube using yt-dlp --dump-json.

    Returns a list of video info dicts. For single videos, returns a
    one-element list. For playlists, returns all videos in the playlist.

    Args:
        url: YouTube video or playlist URL
        cookies_from_browser: Browser name to extract cookies from (e.g. "firefox", "chrome")
    """
    cmd = [sys.executable, "-m", "yt_dlp", "--dump-json", "--ignore-errors", "--no-warnings"]
    if cookies_from_browser:
        cmd.extend(["--cookies-from-browser", cookies_from_browser])
    cmd.append(url)

    logger.info("Fetching video info from: %s", url)

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=120,
        )
    except subprocess.TimeoutExpired:
        logger.error("Timeout fetching video info from: %s", url)
        return []
    except FileNotFoundError:
        logger.error("yt-dlp not found. Install it: pip install yt-dlp")
        return []

    if result.returncode != 0 and not result.stdout.strip():
        logger.error("yt-dlp failed: %s", result.stderr.strip())
        return []

    # yt-dlp outputs one JSON object per line (one per video)
    videos = []
    for line in result.stdout.strip().split("\n"):
        line = line.strip()
        if not line:
            continue
        try:
            info = json.loads(line)
            videos.append(info)
        except json.JSONDecodeError:
            logger.warning("Failed to parse yt-dlp JSON line")
            continue

    logger.info("Found %d video(s)", len(videos))
    return videos


def download_video(url: str, output_path: Path, cookies_from_browser: Optional[str] = None) -> bool:
    """
    Download a video from YouTube using yt-dlp.

    Downloads the best available format as mp4 (or best available).
    """
    cmd = [
        sys.executable, "-m", "yt_dlp",
        "-f", "best[ext=mp4]/best",
        "--no-warnings",
        "--no-playlist",
        "-o", str(output_path),
    ]
    if cookies_from_browser:
        cmd.extend(["--cookies-from-browser", cookies_from_browser])
    cmd.append(url)

    logger.info("Downloading video to: %s", output_path)

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=600,
        )
    except subprocess.TimeoutExpired:
        logger.error("Timeout downloading video: %s", url)
        return False
    except FileNotFoundError:
        logger.error("yt-dlp not found. Install it: pip install yt-dlp")
        return False

    if result.returncode != 0:
        logger.error("yt-dlp download failed: %s", result.stderr.strip())
        return False

    if not output_path.exists():
        logger.error("Downloaded file not found at: %s", output_path)
        return False

    logger.info("Download complete: %s", output_path)
    return True


def download_thumbnail(thumbnail_url: str, output_path: Path) -> bool:
    """Download a thumbnail image from a URL."""
    if not thumbnail_url:
        return False

    try:
        import requests
        response = requests.get(thumbnail_url, timeout=30)
        response.raise_for_status()
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_bytes(response.content)
        logger.info("Downloaded thumbnail to: %s", output_path)
        return True
    except Exception as e:
        logger.warning("Failed to download thumbnail: %s", e)
        return False


def package_youtube(
    url: str,
    output_dir: Path,
    playlist_name: Optional[str] = None,
    quality: int = 5,
    fps: int = 18,
    cookies_from_browser: Optional[str] = None,
) -> bool:
    """
    Download and convert YouTube videos for TinyJukebox playback.

    Args:
        url: YouTube video or playlist URL
        output_dir: SD card output directory (root)
        playlist_name: Display name for the playlist (auto-detected if None)
        quality: MJPEG quality (2-10)
        fps: Target frame rate
        cookies_from_browser: Browser name to extract cookies from (e.g. "firefox", "chrome")

    Returns:
        True if at least one video was converted successfully
    """
    output_dir = Path(output_dir)

    # Fetch metadata for all videos
    videos = get_video_info(url, cookies_from_browser=cookies_from_browser)
    if not videos:
        logger.error("No videos found at: %s", url)
        return False

    # Determine playlist name
    if not playlist_name:
        # Try to get playlist title from first video
        first = videos[0]
        playlist_name = first.get("playlist_title") or first.get("uploader") or first.get("channel") or "YouTube"
        logger.info("Auto-detected playlist name: %s", playlist_name)

    # Get uploader from first video
    uploader = videos[0].get("uploader") or videos[0].get("channel") or ""

    # Get year from first video's upload date
    first_date = videos[0].get("upload_date") or ""
    year = _extract_year(first_date)

    # Create playlist directory under /YouTube/
    dir_name = sanitize_name(playlist_name)
    playlist_dir = output_dir / "YouTube" / dir_name
    playlist_dir.mkdir(parents=True, exist_ok=True)
    logger.info("YouTube playlist directory: YouTube/%s", dir_name)

    # Convert each video
    converted_count = 0
    first_thumb_source = None

    with tempfile.TemporaryDirectory() as tmp_dir:
        tmp_path = Path(tmp_dir)

        for idx, video_info in enumerate(videos, start=1):
            video_num = f"Y{idx:02d}"
            video_title = video_info.get("title") or f"Video {idx}"
            video_url = video_info.get("webpage_url") or video_info.get("url") or url
            video_uploader = video_info.get("uploader") or video_info.get("channel") or ""
            video_date = _format_upload_date(video_info.get("upload_date") or "")
            duration_secs = video_info.get("duration") or 0
            runtime_minutes = int(duration_secs / 60) if duration_secs else 0
            video_description = video_info.get("description") or ""

            logger.info("Processing %s: %s", video_num, video_title)

            # Download video
            download_path = tmp_path / f"{video_num}_source.mp4"
            if not download_video(video_url, download_path, cookies_from_browser=cookies_from_browser):
                logger.error("Download failed for: %s", video_title)
                continue

            # Convert to TinyTV format
            output_avi = playlist_dir / f"{video_num}.avi"
            if not convert_video(download_path, output_avi, fps=fps, quality=quality):
                logger.error("Video conversion failed for: %s", video_title)
                continue

            # Write video metadata
            write_youtube_video_metadata(
                playlist_dir / f"{video_num}.sdb",
                YouTubeVideoInfo(
                    video_number=idx,
                    title=video_title,
                    uploader=video_uploader,
                    upload_date=video_date,
                    runtime_minutes=runtime_minutes,
                    description=video_description,
                ),
            )

            # Generate video thumbnail
            # Prefer YouTube thumbnail, fall back to video frame extraction
            thumb_url = video_info.get("thumbnail") or ""
            thumb_image_path = tmp_path / f"{video_num}_thumb.jpg"
            thumb_downloaded = download_thumbnail(thumb_url, thumb_image_path)

            generate_thumbnail(
                playlist_dir / f"{video_num}.raw",
                image_path=thumb_image_path if thumb_downloaded else None,
                video_path=download_path,
            )

            if first_thumb_source is None:
                if thumb_downloaded:
                    first_thumb_source = ("image", thumb_image_path)
                else:
                    first_thumb_source = ("video", download_path)

            converted_count += 1

            # Clean up source video to save disk space
            if download_path.exists():
                download_path.unlink()

    if converted_count == 0:
        logger.error("No videos were converted successfully")
        return False

    # Write playlist metadata
    write_youtube_playlist_metadata(
        playlist_dir / "playlist.sdb",
        YouTubePlaylistInfo(
            name=playlist_name,
            year=year,
            uploader=uploader,
            video_count=converted_count,
        ),
    )

    # Generate playlist thumbnail from first video's source
    # At this point temp files are cleaned up, so use the first converted AVI
    first_avi = playlist_dir / "Y01.avi"
    generate_thumbnail(
        playlist_dir / "playlist.raw",
        video_path=first_avi if first_avi.exists() else None,
    )

    logger.info("YouTube playlist packaged: %s (%d videos)", playlist_name, converted_count)
    return True
