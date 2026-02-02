#!/usr/bin/env python3
"""
TinyJukebox Converter - Main CLI entry point.

Converts media for TinyTV 2 playback with metadata, thumbnails,
and proper SD card directory layout.

Supported types: tv, movie, music-video, music, photo, youtube

Usage:
    python -m converter.convert --type tv --input-dir ./episodes/ --output-dir ./sdcard/ --show "Seinfeld"
    python -m converter.convert --type movie --input-file ./matrix.mkv --output-dir ./sdcard/ --title "The Matrix"
"""

import argparse
import logging
import sys
from pathlib import Path

from .device_packager import package_show
from .movie_packager import package_movie
from .music_video_packager import package_music_videos
from .music_packager import package_music
from .photo_packager import package_photos
from .youtube_packager import package_youtube


def main():
    parser = argparse.ArgumentParser(
        description="Convert media for TinyJukebox playback",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python -m converter.convert --type tv --input-dir ~/seinfeld/ --output-dir ./sdcard/ --show "Seinfeld"
  python -m converter.convert --type tv --input-dir ~/seinfeld/ --output-dir ./sdcard/ --show "Seinfeld" --season 3
  python -m converter.convert --type movie --input-file ~/movies/matrix.mkv --output-dir ./sdcard/ --title "The Matrix"
        """,
    )

    parser.add_argument(
        "--type", type=str, default="tv",
        choices=["tv", "movie", "music-video", "music", "photo", "youtube"],
        help="Media type to convert (default: tv)",
    )
    parser.add_argument(
        "--input-dir", type=Path, default=None,
        help="Directory containing source files (for tv, music-video, music, photo)",
    )
    parser.add_argument(
        "--input-file", type=Path, default=None,
        help="Single source file (for movie)",
    )
    parser.add_argument(
        "--output-dir", type=Path, required=True,
        help="Output directory for SD card contents",
    )
    # TV-specific
    parser.add_argument(
        "--show", type=str, default=None,
        help="Show name for TV metadata lookup",
    )
    parser.add_argument(
        "--season", type=int, default=None,
        help="Process only a specific season number (TV)",
    )
    parser.add_argument(
        "--episodes", type=str, default=None,
        help="Episode range to process, e.g. '1-2' or '1,3,5' (TV, default: all)",
    )
    # Movie-specific
    parser.add_argument(
        "--title", type=str, default=None,
        help="Movie title for metadata lookup",
    )
    # Music-video / music / photo
    parser.add_argument(
        "--collection", type=str, default=None,
        help="Collection name (music-video)",
    )
    parser.add_argument(
        "--artist", type=str, default=None,
        help="Artist name (music)",
    )
    parser.add_argument(
        "--album", type=str, default=None,
        help="Album name (photo)",
    )
    # YouTube-specific
    parser.add_argument(
        "--url", type=str, default=None,
        help="YouTube video or playlist URL (youtube)",
    )
    parser.add_argument(
        "--playlist", type=str, default=None,
        help="Playlist display name (youtube, optional - auto-detected if omitted)",
    )
    parser.add_argument(
        "--cookies-from-browser", type=str, default=None,
        help="Browser to extract cookies from for YouTube auth (e.g. firefox, chrome)",
    )
    # Shared
    parser.add_argument(
        "--tmdb-key", type=str, default=None,
        help="TMDB API key or Bearer token for artwork",
    )
    parser.add_argument(
        "--quality", type=int, default=5,
        help="MJPEG quality (2-10, lower=better, default: 5)",
    )
    parser.add_argument(
        "--fps", type=int, default=18,
        help="Target frame rate (default: 18)",
    )
    parser.add_argument(
        "--boot-clip", type=Path, default=None,
        help="Optional theme music clip (5-10 seconds)",
    )
    parser.add_argument(
        "--verbose", "-v", action="store_true",
        help="Enable verbose logging",
    )

    args = parser.parse_args()

    # Configure logging
    log_level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(
        level=log_level,
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
        datefmt="%H:%M:%S",
    )

    if args.quality < 2 or args.quality > 10:
        logging.error("Quality must be between 2 and 10")
        sys.exit(1)

    if args.boot_clip and not args.boot_clip.exists():
        logging.error("Boot clip not found: %s", args.boot_clip)
        sys.exit(1)

    media_type = args.type
    success = False

    if media_type == "tv":
        if not args.input_dir:
            logging.error("--input-dir is required for TV shows")
            sys.exit(1)
        if not args.show:
            logging.error("--show is required for TV shows")
            sys.exit(1)
        if not args.input_dir.is_dir():
            logging.error("Input directory does not exist: %s", args.input_dir)
            sys.exit(1)

        episode_filter = None
        if args.episodes:
            episode_filter = set()
            for part in args.episodes.split(","):
                part = part.strip()
                if "-" in part:
                    start, end = part.split("-", 1)
                    episode_filter.update(range(int(start), int(end) + 1))
                else:
                    episode_filter.add(int(part))
            logging.info("Episode filter: %s", sorted(episode_filter))

        success = package_show(
            input_dir=args.input_dir,
            output_dir=args.output_dir,
            show_name=args.show,
            tmdb_key=args.tmdb_key,
            season_filter=args.season,
            episode_filter=episode_filter,
            quality=args.quality,
            fps=args.fps,
            boot_clip=args.boot_clip,
        )

    elif media_type == "movie":
        if not args.input_file:
            logging.error("--input-file is required for movies")
            sys.exit(1)
        if not args.title:
            logging.error("--title is required for movies")
            sys.exit(1)
        if not args.input_file.exists():
            logging.error("Input file does not exist: %s", args.input_file)
            sys.exit(1)

        success = package_movie(
            input_file=args.input_file,
            output_dir=args.output_dir,
            title=args.title,
            tmdb_key=args.tmdb_key,
            quality=args.quality,
            fps=args.fps,
        )

    elif media_type == "music-video":
        if not args.input_dir:
            logging.error("--input-dir is required for music videos")
            sys.exit(1)
        if not args.collection:
            logging.error("--collection is required for music videos")
            sys.exit(1)
        if not args.input_dir.is_dir():
            logging.error("Input directory does not exist: %s", args.input_dir)
            sys.exit(1)

        success = package_music_videos(
            input_dir=args.input_dir,
            output_dir=args.output_dir,
            collection_name=args.collection,
            quality=args.quality,
            fps=args.fps,
        )

    elif media_type == "music":
        if not args.input_dir:
            logging.error("--input-dir is required for music")
            sys.exit(1)
        if not args.artist:
            logging.error("--artist is required for music")
            sys.exit(1)
        if not args.input_dir.is_dir():
            logging.error("Input directory does not exist: %s", args.input_dir)
            sys.exit(1)

        success = package_music(
            input_dir=args.input_dir,
            output_dir=args.output_dir,
            artist_name=args.artist,
            quality=args.quality,
            fps=args.fps,
        )

    elif media_type == "photo":
        if not args.input_dir:
            logging.error("--input-dir is required for photos")
            sys.exit(1)
        if not args.album:
            logging.error("--album is required for photos")
            sys.exit(1)
        if not args.input_dir.is_dir():
            logging.error("Input directory does not exist: %s", args.input_dir)
            sys.exit(1)

        success = package_photos(
            input_dir=args.input_dir,
            output_dir=args.output_dir,
            album_name=args.album,
        )

    elif media_type == "youtube":
        if not args.url:
            logging.error("--url is required for YouTube")
            sys.exit(1)

        success = package_youtube(
            url=args.url,
            output_dir=args.output_dir,
            playlist_name=args.playlist,
            quality=args.quality,
            fps=args.fps,
            cookies_from_browser=args.cookies_from_browser,
        )

    if success:
        logging.info("Done! Copy contents of %s to your TinyTV SD card.", args.output_dir)
    else:
        logging.error("Conversion failed.")
        sys.exit(1)


if __name__ == "__main__":
    main()
