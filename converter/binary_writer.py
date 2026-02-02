"""
Binary writer for .sdb metadata files used by TinyJukebox firmware.

File formats:
- show.sdb:       128 bytes - TV show metadata
- season.sdb:      64 bytes - TV season metadata
- E##.sdb:        128 bytes - TV episode metadata
- movie.sdb:      128 bytes - Movie metadata
- collection.sdb: 128 bytes - Music video collection metadata
- V##.sdb:        128 bytes - Music video metadata
- artist.sdb:     128 bytes - Music artist metadata
- album.sdb:       64 bytes - Music album metadata
- T##.sdb:         64 bytes - Music track metadata
- album.sdb:       64 bytes - Photo album metadata (same size, different magic)
- P##.sdb:         64 bytes - Photo metadata

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
MOVIE_MAGIC = b"TJMV"
COLLECTION_MAGIC = b"TJVC"
VIDEO_MAGIC = b"TJVD"
ARTIST_MAGIC = b"TJMA"
MUSIC_ALBUM_MAGIC = b"TJAL"
TRACK_MAGIC = b"TJTK"
PHOTO_ALBUM_MAGIC = b"TJPA"
PHOTO_MAGIC = b"TJPH"
YOUTUBE_PLAYLIST_MAGIC = b"TJYP"
YOUTUBE_VIDEO_MAGIC = b"TJYV"

# Current format version
FORMAT_VERSION = 1

# Struct sizes
SHOW_SIZE = 128
SEASON_SIZE = 64
EPISODE_SIZE = 128
MOVIE_SIZE = 128
COLLECTION_SIZE = 128
VIDEO_SIZE = 128
ARTIST_SIZE = 128
MUSIC_ALBUM_SIZE = 64
TRACK_SIZE = 64
PHOTO_ALBUM_SIZE = 64
PHOTO_SIZE = 64
YOUTUBE_PLAYLIST_SIZE = 128
YOUTUBE_VIDEO_SIZE = 128


# ─── TV Shows ────────────────────────────────────────────────────────────────

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


# ─── Movies ──────────────────────────────────────────────────────────────────

@dataclass
class MovieInfo:
    title: str
    year: str
    runtime_minutes: int
    description: str


# ─── Music Videos ────────────────────────────────────────────────────────────

@dataclass
class CollectionInfo:
    name: str
    year: str
    video_count: int


@dataclass
class VideoInfo:
    video_number: int
    title: str
    artist: str
    runtime_minutes: int
    description: str


# ─── Music ───────────────────────────────────────────────────────────────────

@dataclass
class ArtistInfo:
    name: str
    genre: str
    album_count: int
    total_tracks: int


@dataclass
class MusicAlbumInfo:
    album_number: int
    track_count: int
    year: str
    title: str


@dataclass
class TrackInfo:
    track_number: int
    title: str
    runtime_seconds: int


# ─── Photos ──────────────────────────────────────────────────────────────────

@dataclass
class PhotoAlbumInfo:
    title: str
    photo_count: int


@dataclass
class PhotoInfo:
    photo_number: int
    caption: str
    date_taken: str


# ─── YouTube ────────────────────────────────────────────────────────────────

@dataclass
class YouTubePlaylistInfo:
    name: str
    year: str
    uploader: str
    video_count: int


@dataclass
class YouTubeVideoInfo:
    video_number: int
    title: str
    uploader: str
    upload_date: str
    runtime_minutes: int
    description: str


# ─── Helpers ─────────────────────────────────────────────────────────────────

def _pad_string(s: str, length: int) -> bytes:
    """Encode string to bytes, truncate or null-pad to exact length."""
    encoded = s.encode("utf-8", errors="replace")[:length]
    return encoded.ljust(length, b"\x00")


def _write_file(path: Path, data: bytes) -> None:
    """Write binary data to path, creating parent dirs as needed."""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


# ─── TV Show Writers/Readers ─────────────────────────────────────────────────

def write_show_metadata(path: Path, info: ShowInfo) -> None:
    """Write show.sdb file (128 bytes)."""
    data = bytearray(SHOW_SIZE)
    struct.pack_into("<4sBBH", data, 0,
                     SHOW_MAGIC, FORMAT_VERSION,
                     info.season_count, info.total_episodes)
    data[8:56] = _pad_string(info.name, 48)
    data[56:64] = _pad_string(info.year, 8)
    _write_file(path, bytes(data))
    logger.info("Wrote show metadata: %s (%d seasons, %d episodes)",
                info.name, info.season_count, info.total_episodes)


def write_season_metadata(path: Path, info: SeasonInfo) -> None:
    """Write season.sdb file (64 bytes)."""
    data = bytearray(SEASON_SIZE)
    struct.pack_into("<4sBB2x", data, 0,
                     SEASON_MAGIC, info.season_number, info.episode_count)
    data[8:16] = _pad_string(info.year, 8)
    data[16:40] = _pad_string(info.title, 24)
    _write_file(path, bytes(data))
    logger.info("Wrote season metadata: Season %d (%d episodes)",
                info.season_number, info.episode_count)


def write_episode_metadata(path: Path, info: EpisodeInfo) -> None:
    """Write E##.sdb file (128 bytes)."""
    data = bytearray(EPISODE_SIZE)
    struct.pack_into("<4sBBH", data, 0,
                     EPISODE_MAGIC, info.season_number,
                     info.episode_number, info.runtime_minutes)
    data[8:56] = _pad_string(info.title, 48)
    data[56:68] = _pad_string(info.air_date, 12)
    data[68:124] = _pad_string(info.description, 56)
    _write_file(path, bytes(data))
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


# ─── Movie Writers/Readers ───────────────────────────────────────────────────

def write_movie_metadata(path: Path, info: MovieInfo) -> None:
    """
    Write movie.sdb file (128 bytes).

    Layout:
      0-3:    magic "TJMV" (4 bytes)
      4:      version (uint8)
      5:      reserved (uint8)
      6-7:    runtime_minutes (uint16 LE)
      8-55:   title (48 bytes, null-padded)
      56-63:  year (8 bytes, null-padded)
      64-119: description (56 bytes, null-padded)
      120-127: reserved (8 bytes, zeroed)
    """
    data = bytearray(MOVIE_SIZE)
    struct.pack_into("<4sBxH", data, 0,
                     MOVIE_MAGIC, FORMAT_VERSION, info.runtime_minutes)
    data[8:56] = _pad_string(info.title, 48)
    data[56:64] = _pad_string(info.year, 8)
    data[64:120] = _pad_string(info.description, 56)
    _write_file(path, bytes(data))
    logger.info("Wrote movie metadata: %s (%s)", info.title, info.year)


def read_movie_metadata(path: Path) -> MovieInfo:
    """Read and parse a movie.sdb file."""
    data = path.read_bytes()
    if len(data) != MOVIE_SIZE:
        raise ValueError(f"Invalid movie.sdb size: {len(data)} (expected {MOVIE_SIZE})")
    magic, version, _reserved, runtime = struct.unpack_from("<4sBBH", data, 0)
    if magic != MOVIE_MAGIC:
        raise ValueError(f"Invalid magic bytes: {magic!r} (expected {MOVIE_MAGIC!r})")
    title = data[8:56].rstrip(b"\x00").decode("utf-8", errors="replace")
    year = data[56:64].rstrip(b"\x00").decode("utf-8", errors="replace")
    description = data[64:120].rstrip(b"\x00").decode("utf-8", errors="replace")
    return MovieInfo(title=title, year=year, runtime_minutes=runtime,
                     description=description)


# ─── Music Video Writers/Readers ─────────────────────────────────────────────

def write_collection_metadata(path: Path, info: CollectionInfo) -> None:
    """
    Write collection.sdb file (128 bytes).

    Layout:
      0-3:    magic "TJVC" (4 bytes)
      4:      version (uint8)
      5:      video_count (uint8)
      6-7:    reserved (2 bytes)
      8-55:   name (48 bytes, null-padded)
      56-63:  year (8 bytes, null-padded)
      64-127: reserved (64 bytes, zeroed)
    """
    data = bytearray(COLLECTION_SIZE)
    struct.pack_into("<4sBB2x", data, 0,
                     COLLECTION_MAGIC, FORMAT_VERSION, info.video_count)
    data[8:56] = _pad_string(info.name, 48)
    data[56:64] = _pad_string(info.year, 8)
    _write_file(path, bytes(data))
    logger.info("Wrote collection metadata: %s (%d videos)", info.name, info.video_count)


def read_collection_metadata(path: Path) -> CollectionInfo:
    """Read and parse a collection.sdb file."""
    data = path.read_bytes()
    if len(data) != COLLECTION_SIZE:
        raise ValueError(f"Invalid collection.sdb size: {len(data)} (expected {COLLECTION_SIZE})")
    magic, version, video_count = struct.unpack_from("<4sBB", data, 0)
    if magic != COLLECTION_MAGIC:
        raise ValueError(f"Invalid magic bytes: {magic!r} (expected {COLLECTION_MAGIC!r})")
    name = data[8:56].rstrip(b"\x00").decode("utf-8", errors="replace")
    year = data[56:64].rstrip(b"\x00").decode("utf-8", errors="replace")
    return CollectionInfo(name=name, year=year, video_count=video_count)


def write_video_metadata(path: Path, info: VideoInfo) -> None:
    """
    Write V##.sdb file (128 bytes).

    Layout:
      0-3:    magic "TJVD" (4 bytes)
      4:      video_number (uint8)
      5:      reserved (uint8)
      6-7:    runtime_minutes (uint16 LE)
      8-55:   title (48 bytes, null-padded)
      56-67:  artist (12 bytes, null-padded)
      68-123: description (56 bytes, null-padded)
      124-127: reserved (4 bytes, zeroed)
    """
    data = bytearray(VIDEO_SIZE)
    struct.pack_into("<4sBxH", data, 0,
                     VIDEO_MAGIC, info.video_number, info.runtime_minutes)
    data[8:56] = _pad_string(info.title, 48)
    data[56:68] = _pad_string(info.artist, 12)
    data[68:124] = _pad_string(info.description, 56)
    _write_file(path, bytes(data))
    logger.info("Wrote video metadata: V%02d - %s", info.video_number, info.title)


def read_video_metadata(path: Path) -> VideoInfo:
    """Read and parse a V##.sdb file."""
    data = path.read_bytes()
    if len(data) != VIDEO_SIZE:
        raise ValueError(f"Invalid V##.sdb size: {len(data)} (expected {VIDEO_SIZE})")
    magic, video_number, _reserved, runtime = struct.unpack_from("<4sBBH", data, 0)
    if magic != VIDEO_MAGIC:
        raise ValueError(f"Invalid magic bytes: {magic!r} (expected {VIDEO_MAGIC!r})")
    title = data[8:56].rstrip(b"\x00").decode("utf-8", errors="replace")
    artist = data[56:68].rstrip(b"\x00").decode("utf-8", errors="replace")
    description = data[68:124].rstrip(b"\x00").decode("utf-8", errors="replace")
    return VideoInfo(video_number=video_number, title=title, artist=artist,
                     runtime_minutes=runtime, description=description)


# ─── Music Writers/Readers ───────────────────────────────────────────────────

def write_artist_metadata(path: Path, info: ArtistInfo) -> None:
    """
    Write artist.sdb file (128 bytes).

    Layout:
      0-3:    magic "TJMA" (4 bytes)
      4:      version (uint8)
      5:      album_count (uint8)
      6-7:    total_tracks (uint16 LE)
      8-55:   name (48 bytes, null-padded)
      56-63:  genre (8 bytes, null-padded)
      64-127: reserved (64 bytes, zeroed)
    """
    data = bytearray(ARTIST_SIZE)
    struct.pack_into("<4sBBH", data, 0,
                     ARTIST_MAGIC, FORMAT_VERSION,
                     info.album_count, info.total_tracks)
    data[8:56] = _pad_string(info.name, 48)
    data[56:64] = _pad_string(info.genre, 8)
    _write_file(path, bytes(data))
    logger.info("Wrote artist metadata: %s (%d albums, %d tracks)",
                info.name, info.album_count, info.total_tracks)


def read_artist_metadata(path: Path) -> ArtistInfo:
    """Read and parse an artist.sdb file."""
    data = path.read_bytes()
    if len(data) != ARTIST_SIZE:
        raise ValueError(f"Invalid artist.sdb size: {len(data)} (expected {ARTIST_SIZE})")
    magic, version, album_count, total_tracks = struct.unpack_from("<4sBBH", data, 0)
    if magic != ARTIST_MAGIC:
        raise ValueError(f"Invalid magic bytes: {magic!r} (expected {ARTIST_MAGIC!r})")
    name = data[8:56].rstrip(b"\x00").decode("utf-8", errors="replace")
    genre = data[56:64].rstrip(b"\x00").decode("utf-8", errors="replace")
    return ArtistInfo(name=name, genre=genre, album_count=album_count,
                      total_tracks=total_tracks)


def write_music_album_metadata(path: Path, info: MusicAlbumInfo) -> None:
    """
    Write album.sdb file for music (64 bytes).

    Layout:
      0-3:   magic "TJAL" (4 bytes)
      4:     album_number (uint8)
      5:     track_count (uint8)
      6-7:   reserved (2 bytes)
      8-15:  year (8 bytes, null-padded)
      16-39: title (24 bytes, null-padded)
      40-63: reserved (24 bytes, zeroed)
    """
    data = bytearray(MUSIC_ALBUM_SIZE)
    struct.pack_into("<4sBB2x", data, 0,
                     MUSIC_ALBUM_MAGIC, info.album_number, info.track_count)
    data[8:16] = _pad_string(info.year, 8)
    data[16:40] = _pad_string(info.title, 24)
    _write_file(path, bytes(data))
    logger.info("Wrote music album metadata: %s (%d tracks)", info.title, info.track_count)


def read_music_album_metadata(path: Path) -> MusicAlbumInfo:
    """Read and parse a music album.sdb file."""
    data = path.read_bytes()
    if len(data) != MUSIC_ALBUM_SIZE:
        raise ValueError(f"Invalid album.sdb size: {len(data)} (expected {MUSIC_ALBUM_SIZE})")
    magic, album_number, track_count = struct.unpack_from("<4sBB", data, 0)
    if magic != MUSIC_ALBUM_MAGIC:
        raise ValueError(f"Invalid magic bytes: {magic!r} (expected {MUSIC_ALBUM_MAGIC!r})")
    year = data[8:16].rstrip(b"\x00").decode("utf-8", errors="replace")
    title = data[16:40].rstrip(b"\x00").decode("utf-8", errors="replace")
    return MusicAlbumInfo(album_number=album_number, track_count=track_count,
                          year=year, title=title)


def write_track_metadata(path: Path, info: TrackInfo) -> None:
    """
    Write T##.sdb file (64 bytes).

    Layout:
      0-3:   magic "TJTK" (4 bytes)
      4:     track_number (uint8)
      5:     reserved (uint8)
      6-7:   runtime_seconds (uint16 LE)
      8-55:  title (48 bytes, null-padded)
      56-63: reserved (8 bytes, zeroed)
    """
    data = bytearray(TRACK_SIZE)
    struct.pack_into("<4sBxH", data, 0,
                     TRACK_MAGIC, info.track_number, info.runtime_seconds)
    data[8:56] = _pad_string(info.title, 48)
    _write_file(path, bytes(data))
    logger.info("Wrote track metadata: T%02d - %s", info.track_number, info.title)


def read_track_metadata(path: Path) -> TrackInfo:
    """Read and parse a T##.sdb file."""
    data = path.read_bytes()
    if len(data) != TRACK_SIZE:
        raise ValueError(f"Invalid T##.sdb size: {len(data)} (expected {TRACK_SIZE})")
    magic, track_number, _reserved, runtime = struct.unpack_from("<4sBBH", data, 0)
    if magic != TRACK_MAGIC:
        raise ValueError(f"Invalid magic bytes: {magic!r} (expected {TRACK_MAGIC!r})")
    title = data[8:56].rstrip(b"\x00").decode("utf-8", errors="replace")
    return TrackInfo(track_number=track_number, title=title, runtime_seconds=runtime)


# ─── Photo Writers/Readers ───────────────────────────────────────────────────

def write_photo_album_metadata(path: Path, info: PhotoAlbumInfo) -> None:
    """
    Write album.sdb file for photos (64 bytes).

    Layout:
      0-3:   magic "TJPA" (4 bytes)
      4:     version (uint8)
      5:     photo_count (uint8)
      6-7:   reserved (2 bytes)
      8-55:  title (48 bytes, null-padded)
      56-63: reserved (8 bytes, zeroed)
    """
    data = bytearray(PHOTO_ALBUM_SIZE)
    struct.pack_into("<4sBB2x", data, 0,
                     PHOTO_ALBUM_MAGIC, FORMAT_VERSION, info.photo_count)
    data[8:56] = _pad_string(info.title, 48)
    _write_file(path, bytes(data))
    logger.info("Wrote photo album metadata: %s (%d photos)", info.title, info.photo_count)


def read_photo_album_metadata(path: Path) -> PhotoAlbumInfo:
    """Read and parse a photo album.sdb file."""
    data = path.read_bytes()
    if len(data) != PHOTO_ALBUM_SIZE:
        raise ValueError(f"Invalid album.sdb size: {len(data)} (expected {PHOTO_ALBUM_SIZE})")
    magic, version, photo_count = struct.unpack_from("<4sBB", data, 0)
    if magic != PHOTO_ALBUM_MAGIC:
        raise ValueError(f"Invalid magic bytes: {magic!r} (expected {PHOTO_ALBUM_MAGIC!r})")
    title = data[8:56].rstrip(b"\x00").decode("utf-8", errors="replace")
    return PhotoAlbumInfo(title=title, photo_count=photo_count)


def write_photo_metadata(path: Path, info: PhotoInfo) -> None:
    """
    Write P##.sdb file (64 bytes).

    Layout:
      0-3:   magic "TJPH" (4 bytes)
      4:     photo_number (uint8)
      5-7:   reserved (3 bytes)
      8-55:  caption (48 bytes, null-padded)
      56-63: date_taken (8 bytes, null-padded)
    """
    data = bytearray(PHOTO_SIZE)
    struct.pack_into("<4sB3x", data, 0,
                     PHOTO_MAGIC, info.photo_number)
    data[8:56] = _pad_string(info.caption, 48)
    data[56:64] = _pad_string(info.date_taken, 8)
    _write_file(path, bytes(data))
    logger.info("Wrote photo metadata: P%02d - %s", info.photo_number, info.caption)


def read_photo_metadata(path: Path) -> PhotoInfo:
    """Read and parse a P##.sdb file."""
    data = path.read_bytes()
    if len(data) != PHOTO_SIZE:
        raise ValueError(f"Invalid P##.sdb size: {len(data)} (expected {PHOTO_SIZE})")
    magic, photo_number = struct.unpack_from("<4sB", data, 0)
    if magic != PHOTO_MAGIC:
        raise ValueError(f"Invalid magic bytes: {magic!r} (expected {PHOTO_MAGIC!r})")
    caption = data[8:56].rstrip(b"\x00").decode("utf-8", errors="replace")
    date_taken = data[56:64].rstrip(b"\x00").decode("utf-8", errors="replace")
    return PhotoInfo(photo_number=photo_number, caption=caption, date_taken=date_taken)


# ─── YouTube Writers/Readers ────────────────────────────────────────────────

def write_youtube_playlist_metadata(path: Path, info: YouTubePlaylistInfo) -> None:
    """
    Write playlist.sdb file for YouTube (128 bytes).

    Layout:
      0-3:    magic "TJYP" (4 bytes)
      4:      version (uint8)
      5:      video_count (uint8)
      6-7:    reserved (2 bytes)
      8-55:   name (48 bytes, null-padded)
      56-63:  year (8 bytes, null-padded)
      64-87:  uploader (24 bytes, null-padded)
      88-127: reserved (40 bytes, zeroed)
    """
    data = bytearray(YOUTUBE_PLAYLIST_SIZE)
    struct.pack_into("<4sBB2x", data, 0,
                     YOUTUBE_PLAYLIST_MAGIC, FORMAT_VERSION, info.video_count)
    data[8:56] = _pad_string(info.name, 48)
    data[56:64] = _pad_string(info.year, 8)
    data[64:88] = _pad_string(info.uploader, 24)
    _write_file(path, bytes(data))
    logger.info("Wrote YouTube playlist metadata: %s (%d videos)", info.name, info.video_count)


def read_youtube_playlist_metadata(path: Path) -> YouTubePlaylistInfo:
    """Read and parse a YouTube playlist.sdb file."""
    data = path.read_bytes()
    if len(data) != YOUTUBE_PLAYLIST_SIZE:
        raise ValueError(f"Invalid playlist.sdb size: {len(data)} (expected {YOUTUBE_PLAYLIST_SIZE})")
    magic, version, video_count = struct.unpack_from("<4sBB", data, 0)
    if magic != YOUTUBE_PLAYLIST_MAGIC:
        raise ValueError(f"Invalid magic bytes: {magic!r} (expected {YOUTUBE_PLAYLIST_MAGIC!r})")
    name = data[8:56].rstrip(b"\x00").decode("utf-8", errors="replace")
    year = data[56:64].rstrip(b"\x00").decode("utf-8", errors="replace")
    uploader = data[64:88].rstrip(b"\x00").decode("utf-8", errors="replace")
    return YouTubePlaylistInfo(name=name, year=year, uploader=uploader,
                               video_count=video_count)


def write_youtube_video_metadata(path: Path, info: YouTubeVideoInfo) -> None:
    """
    Write Y##.sdb file for YouTube (128 bytes).

    Layout:
      0-3:    magic "TJYV" (4 bytes)
      4:      video_number (uint8)
      5:      reserved (uint8)
      6-7:    runtime_minutes (uint16 LE)
      8-55:   title (48 bytes, null-padded)
      56-67:  uploader (12 bytes, null-padded)
      68-79:  upload_date (12 bytes, null-padded, "YYYY-MM-DD")
      80-123: description (44 bytes, null-padded)
      124-127: reserved (4 bytes, zeroed)
    """
    data = bytearray(YOUTUBE_VIDEO_SIZE)
    struct.pack_into("<4sBxH", data, 0,
                     YOUTUBE_VIDEO_MAGIC, info.video_number, info.runtime_minutes)
    data[8:56] = _pad_string(info.title, 48)
    data[56:68] = _pad_string(info.uploader, 12)
    data[68:80] = _pad_string(info.upload_date, 12)
    data[80:124] = _pad_string(info.description, 44)
    _write_file(path, bytes(data))
    logger.info("Wrote YouTube video metadata: Y%02d - %s", info.video_number, info.title)


def read_youtube_video_metadata(path: Path) -> YouTubeVideoInfo:
    """Read and parse a YouTube Y##.sdb file."""
    data = path.read_bytes()
    if len(data) != YOUTUBE_VIDEO_SIZE:
        raise ValueError(f"Invalid Y##.sdb size: {len(data)} (expected {YOUTUBE_VIDEO_SIZE})")
    magic, video_number, _reserved, runtime = struct.unpack_from("<4sBBH", data, 0)
    if magic != YOUTUBE_VIDEO_MAGIC:
        raise ValueError(f"Invalid magic bytes: {magic!r} (expected {YOUTUBE_VIDEO_MAGIC!r})")
    title = data[8:56].rstrip(b"\x00").decode("utf-8", errors="replace")
    uploader = data[56:68].rstrip(b"\x00").decode("utf-8", errors="replace")
    upload_date = data[68:80].rstrip(b"\x00").decode("utf-8", errors="replace")
    description = data[80:124].rstrip(b"\x00").decode("utf-8", errors="replace")
    return YouTubeVideoInfo(video_number=video_number, title=title, uploader=uploader,
                            upload_date=upload_date, runtime_minutes=runtime,
                            description=description)
