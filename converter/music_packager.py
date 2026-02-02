"""
Music artist packager - converts a directory of music (organized by album
subdirectories) into TinyJukebox SD card format.

Creates /Music/{ArtistName}/ directory with:
  artist.sdb  - 128-byte binary metadata
  artist.raw  - 108x67 RGB565 thumbnail
  A01/
    album.sdb / album.raw
    T01.avi / T01.sdb
    T02.avi / T02.sdb

Music tracks are converted to AVI files (looped album art + audio).
"""

import logging
import re
import subprocess
import tempfile
from pathlib import Path

from .binary_writer import (
    ArtistInfo,
    MusicAlbumInfo,
    TrackInfo,
    write_artist_metadata,
    write_music_album_metadata,
    write_track_metadata,
)
from .video_converter import convert_video
from .thumbnail_generator import generate_thumbnail

logger = logging.getLogger(__name__)

MAX_ITEM_DIR_LEN = 31

AUDIO_EXTENSIONS = {".mp3", ".flac", ".ogg", ".wav", ".m4a", ".wma", ".aac"}

COVER_ART_NAMES = ["cover.jpg", "cover.png", "folder.jpg", "album.jpg"]


def sanitize_name(name: str) -> str:
    """Sanitize a name into a valid SD card directory name."""
    result = name.replace(" ", "_")
    result = re.sub(r'[^A-Za-z0-9_]', '', result)
    result = re.sub(r'_+', '_', result)
    result = result.strip('_')
    result = result[:MAX_ITEM_DIR_LEN]
    result = result.rstrip('_')
    if not result:
        result = "Artist"
    return result


def _find_cover_art(album_dir: Path) -> Path | None:
    """
    Find cover art in an album directory.

    Looks for well-known filenames first, then falls back to the first
    .jpg or .png found in the directory.
    """
    for name in COVER_ART_NAMES:
        candidate = album_dir / name
        if candidate.exists():
            logger.debug("Found cover art: %s", candidate)
            return candidate

    # Fall back to first image file
    for ext in (".jpg", ".png"):
        for img_path in sorted(album_dir.glob(f"*{ext}")):
            logger.debug("Using fallback cover art: %s", img_path)
            return img_path

    return None


def _get_audio_files(directory: Path) -> list[Path]:
    """Return sorted list of audio files in a directory."""
    files = []
    for f in sorted(directory.iterdir()):
        if f.is_file() and f.suffix.lower() in AUDIO_EXTENSIONS:
            files.append(f)
    return files


def convert_audio_to_avi(
    audio_path: Path,
    output_path: Path,
    art_path: Path = None,
    fps: int = 1,
    quality: int = 5,
) -> bool:
    """
    Convert an audio file to AVI with album art as looped video.

    Uses ffmpeg to mux a static image (looped) with the audio track.
    If no art_path is provided, a black 210x135 placeholder is used.

    Args:
        audio_path: Source audio file
        output_path: Destination AVI file
        art_path: Optional album art image
        fps: Video frame rate (default 1 for static image)
        quality: MJPEG quality (2-10, lower is better)

    Returns:
        True if conversion succeeded
    """
    output_path.parent.mkdir(parents=True, exist_ok=True)

    temp_art = None
    image_input = art_path

    if image_input is None:
        # Create a temporary black placeholder image
        try:
            from PIL import Image
            temp_art = tempfile.NamedTemporaryFile(suffix=".png", delete=False)
            img = Image.new("RGB", (210, 135), (0, 0, 0))
            img.save(temp_art.name)
            temp_art.close()
            image_input = Path(temp_art.name)
            logger.debug("Created black placeholder art: %s", image_input)
        except Exception as e:
            logger.error("Failed to create placeholder image: %s", e)
            return False

    cmd = [
        "ffmpeg",
        "-loop", "1",
        "-i", str(image_input),
        "-i", str(audio_path),
        "-c:v", "mjpeg",
        "-q:v", str(quality),
        "-vf", (
            "scale=210:135:force_original_aspect_ratio=decrease,"
            "pad=210:135:(ow-iw)/2:(oh-ih)/2:black,setsar=1"
        ),
        "-c:a", "pcm_u8",
        "-ar", "11025",
        "-ac", "1",
        "-r", str(fps),
        "-shortest",
        "-y",
        str(output_path),
    ]

    logger.info("Converting audio to AVI: %s -> %s", audio_path.name, output_path.name)
    logger.debug("ffmpeg command: %s", " ".join(cmd))

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=3600,
        )
        if result.returncode != 0:
            logger.error("ffmpeg failed for %s:\n%s", audio_path.name, result.stderr[-500:])
            return False

        logger.info("Converted successfully: %s (%.1f MB)",
                     output_path.name, output_path.stat().st_size / 1_048_576)
        return True

    except subprocess.TimeoutExpired:
        logger.error("ffmpeg timed out for: %s", audio_path.name)
        return False
    except FileNotFoundError:
        logger.error("ffmpeg not found. Please install ffmpeg.")
        return False
    finally:
        if temp_art is not None:
            Path(temp_art.name).unlink(missing_ok=True)


def package_music(
    input_dir: Path,
    output_dir: Path,
    artist_name: str,
    genre: str = "",
    quality: int = 5,
    fps: int = 1,
) -> bool:
    """
    Convert a directory of music albums for TinyJukebox playback.

    The input_dir should contain album subdirectories, each with audio files.
    Audio files are converted to AVI format with album art as video.

    Args:
        input_dir: Source directory containing album subdirectories
        output_dir: SD card output directory (root)
        artist_name: Artist name for metadata
        genre: Optional genre string
        quality: MJPEG quality (2-10)
        fps: Target frame rate for video (1 for static art)

    Returns:
        True if at least one track was converted successfully
    """
    input_dir = Path(input_dir)
    output_dir = Path(output_dir)

    if not input_dir.exists():
        logger.error("Input directory does not exist: %s", input_dir)
        return False

    # Find album subdirectories (sorted alphabetically)
    album_dirs = sorted(
        [d for d in input_dir.iterdir() if d.is_dir()],
        key=lambda d: d.name,
    )

    if not album_dirs:
        logger.error("No album subdirectories found in: %s", input_dir)
        return False

    # Create artist output directory
    dir_name = sanitize_name(artist_name)
    music_dir = output_dir / "Music"
    artist_dir = music_dir / dir_name
    artist_dir.mkdir(parents=True, exist_ok=True)
    logger.info("Artist directory: Music/%s", dir_name)

    total_tracks = 0
    album_count = 0
    first_cover_art = None

    for album_idx, album_src_dir in enumerate(album_dirs, start=1):
        audio_files = _get_audio_files(album_src_dir)
        if not audio_files:
            logger.warning("No audio files in album directory: %s", album_src_dir.name)
            continue

        album_num_str = f"A{album_idx:02d}"
        album_out_dir = artist_dir / album_num_str
        album_out_dir.mkdir(parents=True, exist_ok=True)

        # Find cover art for this album
        cover_art = _find_cover_art(album_src_dir)
        if cover_art and first_cover_art is None:
            first_cover_art = cover_art

        # Generate album thumbnail
        generate_thumbnail(
            album_out_dir / "album.raw",
            image_path=cover_art,
        )

        # Convert tracks
        track_count = 0
        for track_idx, audio_file in enumerate(audio_files, start=1):
            track_num_str = f"T{track_idx:02d}"
            output_avi = album_out_dir / f"{track_num_str}.avi"

            if convert_audio_to_avi(
                audio_path=audio_file,
                output_path=output_avi,
                art_path=cover_art,
                fps=fps,
                quality=quality,
            ):
                # Write track metadata
                track_title = audio_file.stem
                write_track_metadata(
                    album_out_dir / f"{track_num_str}.sdb",
                    TrackInfo(
                        track_number=track_idx,
                        title=track_title,
                        runtime_seconds=0,
                    ),
                )
                track_count += 1
                total_tracks += 1
                logger.info("Packaged track: %s/%s - %s",
                            album_num_str, track_num_str, track_title)
            else:
                logger.error("Failed to convert track: %s", audio_file.name)

        if track_count > 0:
            # Write album metadata
            write_music_album_metadata(
                album_out_dir / "album.sdb",
                MusicAlbumInfo(
                    album_number=album_idx,
                    track_count=track_count,
                    year="",
                    title=album_src_dir.name,
                ),
            )
            album_count += 1
            logger.info("Packaged album: %s - %s (%d tracks)",
                        album_num_str, album_src_dir.name, track_count)

    if total_tracks == 0:
        logger.error("No tracks were converted for artist: %s", artist_name)
        return False

    # Write artist metadata
    write_artist_metadata(
        artist_dir / "artist.sdb",
        ArtistInfo(
            name=artist_name,
            genre=genre,
            album_count=album_count,
            total_tracks=total_tracks,
        ),
    )

    # Generate artist thumbnail from first album's cover art
    generate_thumbnail(
        artist_dir / "artist.raw",
        image_path=first_cover_art,
    )

    logger.info("Music packaged: %s (%d albums, %d tracks)",
                artist_name, album_count, total_tracks)
    return True
