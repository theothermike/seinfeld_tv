#!/usr/bin/env python3
"""
SeinfeldTV Converter - Main CLI entry point.

Converts TV show episodes to TinyTV 2 compatible format with
metadata, thumbnails, and proper SD card directory layout.

Usage:
    python convert.py --input-dir /path/to/episodes/ --output-dir ./sdcard/ --show "Seinfeld"
"""

import argparse
import logging
import sys
from pathlib import Path

from .device_packager import package_show


def main():
    parser = argparse.ArgumentParser(
        description="Convert TV show episodes for TinyTV 2 playback",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python -m converter.convert --input-dir ~/seinfeld/ --output-dir ./sdcard/ --show "Seinfeld"
  python -m converter.convert --input-dir ~/seinfeld/ --output-dir ./sdcard/ --show "Seinfeld" --season 3
  python -m converter.convert --input-dir ~/seinfeld/ --output-dir ./sdcard/ --show "Seinfeld" --tmdb-key YOUR_KEY
        """,
    )

    parser.add_argument(
        "--input-dir", type=Path, required=True,
        help="Directory containing source video files",
    )
    parser.add_argument(
        "--output-dir", type=Path, required=True,
        help="Output directory for SD card contents",
    )
    parser.add_argument(
        "--show", type=str, required=True,
        help="Show name for metadata lookup (e.g., 'Seinfeld')",
    )
    parser.add_argument(
        "--season", type=int, default=None,
        help="Process only a specific season number",
    )
    parser.add_argument(
        "--episodes", type=str, default=None,
        help="Episode range to process, e.g. '1-2' or '1,3,5' (default: all)",
    )
    parser.add_argument(
        "--tmdb-key", type=str, default=None,
        help="TMDB API key or Bearer token for higher-quality artwork",
    )
    parser.add_argument(
        "--quality", type=int, default=2,
        help="MJPEG quality (2-10, lower=better, default: 2)",
    )
    parser.add_argument(
        "--fps", type=int, default=24,
        help="Target frame rate (default: 24)",
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

    # Validate inputs
    if not args.input_dir.is_dir():
        logging.error("Input directory does not exist: %s", args.input_dir)
        sys.exit(1)

    if args.quality < 2 or args.quality > 10:
        logging.error("Quality must be between 2 and 10")
        sys.exit(1)

    if args.boot_clip and not args.boot_clip.exists():
        logging.error("Boot clip not found: %s", args.boot_clip)
        sys.exit(1)

    # Parse episode filter
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

    # Run packaging pipeline
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

    if success:
        logging.info("Done! Copy contents of %s to your TinyTV SD card.", args.output_dir)
    else:
        logging.error("Conversion failed.")
        sys.exit(1)


if __name__ == "__main__":
    main()
