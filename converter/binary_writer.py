"""
Binary writer for .sdb metadata files used by TinyTV firmware.

File formats:
- show.sdb:   128 bytes - Show-level metadata
- season.sdb:  64 bytes - Season-level metadata
- E##.sdb:    128 bytes - Episode-level metadata

All strings are null-padded to their fixed length.
All multi-byte integers are little-endian (RP2040 native).
"""

import struct
import logging
from dataclasses import dataclass
from pathlib import Path

logger = logging.getLogger(__name__)

# Magic bytes
SHOW_MAGIC = b"SFTV"
SEASON_MAGIC = b"SFSN"
EPISODE_MAGIC = b"SFEP"

# Current format version
FORMAT_VERSION = 1

# Struct sizes
SHOW_SIZE = 128
SEASON_SIZE = 64
EPISODE_SIZE = 128


@dataclass
class ShowInfo:
    name: str
    year: str
    season_count: int
    total_episodes: int


@dataclass
class SeasonInfo:
    season_number: int
    episode_count: int
    year: str
    title: str


@dataclass
class EpisodeInfo:
    season_number: int
    episode_number: int
    title: str
    air_date: str
    runtime_minutes: int
    description: str


def _pad_string(s: str, length: int) -> bytes:
    """Encode string to bytes, truncate or null-pad to exact length."""
    encoded = s.encode("utf-8", errors="replace")[:length]
    return encoded.ljust(length, b"\x00")


def write_show_metadata(path: Path, info: ShowInfo) -> None:
    """
    Write show.sdb file (128 bytes).

    Layout:
      0-3:    magic "SFTV" (4 bytes)
      4:      version (uint8)
      5:      season_count (uint8)
      6-7:    total_episodes (uint16 LE)
      8-55:   show_name (48 bytes, null-padded)
      56-63:  year (8 bytes, null-padded)
      64-127: reserved (64 bytes, zeroed)
    """
    data = bytearray(SHOW_SIZE)

    struct.pack_into("<4sBBH", data, 0,
                     SHOW_MAGIC,
                     FORMAT_VERSION,
                     info.season_count,
                     info.total_episodes)

    data[8:56] = _pad_string(info.name, 48)
    data[56:64] = _pad_string(info.year, 8)
    # 64-127 remain zeroed (reserved)

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(bytes(data))
    logger.info("Wrote show metadata: %s (%d seasons, %d episodes)",
                info.name, info.season_count, info.total_episodes)


def write_season_metadata(path: Path, info: SeasonInfo) -> None:
    """
    Write season.sdb file (64 bytes).

    Layout:
      0-3:   magic "SFSN" (4 bytes)
      4:     season_number (uint8)
      5:     episode_count (uint8)
      6-7:   reserved (2 bytes)
      8-15:  year (8 bytes, null-padded)
      16-39: title (24 bytes, null-padded)
      40-63: reserved (24 bytes, zeroed)
    """
    data = bytearray(SEASON_SIZE)

    struct.pack_into("<4sBB2x", data, 0,
                     SEASON_MAGIC,
                     info.season_number,
                     info.episode_count)

    data[8:16] = _pad_string(info.year, 8)
    data[16:40] = _pad_string(info.title, 24)
    # 40-63 remain zeroed (reserved)

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(bytes(data))
    logger.info("Wrote season metadata: Season %d (%d episodes)",
                info.season_number, info.episode_count)


def write_episode_metadata(path: Path, info: EpisodeInfo) -> None:
    """
    Write E##.sdb file (128 bytes).

    Layout:
      0-3:    magic "SFEP" (4 bytes)
      4:      season_number (uint8)
      5:      episode_number (uint8)
      6-7:    runtime_minutes (uint16 LE)
      8-55:   title (48 bytes, null-padded)
      56-67:  air_date (12 bytes, null-padded)
      68-123: description (56 bytes, null-padded)
      124-127: reserved (4 bytes, zeroed)
    """
    data = bytearray(EPISODE_SIZE)

    struct.pack_into("<4sBBH", data, 0,
                     EPISODE_MAGIC,
                     info.season_number,
                     info.episode_number,
                     info.runtime_minutes)

    data[8:56] = _pad_string(info.title, 48)
    data[56:68] = _pad_string(info.air_date, 12)
    data[68:124] = _pad_string(info.description, 56)
    # 124-127 remain zeroed (reserved)

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(bytes(data))
    logger.info("Wrote episode metadata: S%02dE%02d - %s",
                info.season_number, info.episode_number, info.title)


def read_show_metadata(path: Path) -> ShowInfo:
    """Read and parse a show.sdb file."""
    data = path.read_bytes()
    if len(data) != SHOW_SIZE:
        raise ValueError(f"Invalid show.sdb size: {len(data)} (expected {SHOW_SIZE})")

    magic, version, season_count, total_episodes = struct.unpack_from("<4sBBH", data, 0)
    if magic != SHOW_MAGIC:
        raise ValueError(f"Invalid magic bytes: {magic!r} (expected {SHOW_MAGIC!r})")

    name = data[8:56].rstrip(b"\x00").decode("utf-8", errors="replace")
    year = data[56:64].rstrip(b"\x00").decode("utf-8", errors="replace")

    return ShowInfo(name=name, year=year, season_count=season_count,
                    total_episodes=total_episodes)


def read_season_metadata(path: Path) -> SeasonInfo:
    """Read and parse a season.sdb file."""
    data = path.read_bytes()
    if len(data) != SEASON_SIZE:
        raise ValueError(f"Invalid season.sdb size: {len(data)} (expected {SEASON_SIZE})")

    magic, season_number, episode_count = struct.unpack_from("<4sBB", data, 0)
    if magic != SEASON_MAGIC:
        raise ValueError(f"Invalid magic bytes: {magic!r} (expected {SEASON_MAGIC!r})")

    year = data[8:16].rstrip(b"\x00").decode("utf-8", errors="replace")
    title = data[16:40].rstrip(b"\x00").decode("utf-8", errors="replace")

    return SeasonInfo(season_number=season_number, episode_count=episode_count,
                      year=year, title=title)


def read_episode_metadata(path: Path) -> EpisodeInfo:
    """Read and parse an E##.sdb file."""
    data = path.read_bytes()
    if len(data) != EPISODE_SIZE:
        raise ValueError(f"Invalid E##.sdb size: {len(data)} (expected {EPISODE_SIZE})")

    magic, season_number, episode_number, runtime = struct.unpack_from("<4sBBH", data, 0)
    if magic != EPISODE_MAGIC:
        raise ValueError(f"Invalid magic bytes: {magic!r} (expected {EPISODE_MAGIC!r})")

    title = data[8:56].rstrip(b"\x00").decode("utf-8", errors="replace")
    air_date = data[56:68].rstrip(b"\x00").decode("utf-8", errors="replace")
    description = data[68:124].rstrip(b"\x00").decode("utf-8", errors="replace")

    return EpisodeInfo(season_number=season_number, episode_number=episode_number,
                       title=title, air_date=air_date, runtime_minutes=runtime,
                       description=description)
