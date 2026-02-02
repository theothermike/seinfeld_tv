"""Tests for binary_writer module - round-trip and format verification."""

import struct
import pytest
from pathlib import Path

from converter.binary_writer import (
    # TV Shows
    ShowInfo, SeasonInfo, EpisodeInfo,
    write_show_metadata, read_show_metadata,
    write_season_metadata, read_season_metadata,
    write_episode_metadata, read_episode_metadata,
    SHOW_MAGIC, SEASON_MAGIC, EPISODE_MAGIC,
    SHOW_SIZE, SEASON_SIZE, EPISODE_SIZE,
    # Movies
    MovieInfo,
    write_movie_metadata, read_movie_metadata,
    MOVIE_MAGIC, MOVIE_SIZE,
    # Music Videos
    CollectionInfo, VideoInfo,
    write_collection_metadata, read_collection_metadata,
    write_video_metadata, read_video_metadata,
    COLLECTION_MAGIC, VIDEO_MAGIC,
    COLLECTION_SIZE, VIDEO_SIZE,
    # Music
    ArtistInfo, MusicAlbumInfo, TrackInfo,
    write_artist_metadata, read_artist_metadata,
    write_music_album_metadata, read_music_album_metadata,
    write_track_metadata, read_track_metadata,
    ARTIST_MAGIC, MUSIC_ALBUM_MAGIC, TRACK_MAGIC,
    ARTIST_SIZE, MUSIC_ALBUM_SIZE, TRACK_SIZE,
    # Photos
    PhotoAlbumInfo, PhotoInfo,
    write_photo_album_metadata, read_photo_album_metadata,
    write_photo_metadata, read_photo_metadata,
    PHOTO_ALBUM_MAGIC, PHOTO_MAGIC,
    PHOTO_ALBUM_SIZE, PHOTO_SIZE,
    # Shared
    FORMAT_VERSION,
)


@pytest.fixture
def tmp_dir(tmp_path):
    return tmp_path


# ─── TV Shows ────────────────────────────────────────────────────────────────

class TestShowMetadata:
    def test_round_trip(self, tmp_dir):
        path = tmp_dir / "show.sdb"
        original = ShowInfo(
            name="Seinfeld",
            year="1989",
            season_count=9,
            total_episodes=180,
        )
        write_show_metadata(path, original)
        loaded = read_show_metadata(path)

        assert loaded.name == original.name
        assert loaded.year == original.year
        assert loaded.season_count == original.season_count
        assert loaded.total_episodes == original.total_episodes

    def test_file_size(self, tmp_dir):
        path = tmp_dir / "show.sdb"
        write_show_metadata(path, ShowInfo("Test", "2024", 1, 10))
        assert path.stat().st_size == SHOW_SIZE

    def test_magic_bytes(self, tmp_dir):
        path = tmp_dir / "show.sdb"
        write_show_metadata(path, ShowInfo("Test", "2024", 1, 10))
        data = path.read_bytes()
        assert data[:4] == SHOW_MAGIC

    def test_version(self, tmp_dir):
        path = tmp_dir / "show.sdb"
        write_show_metadata(path, ShowInfo("Test", "2024", 1, 10))
        data = path.read_bytes()
        assert data[4] == FORMAT_VERSION

    def test_long_name_truncated(self, tmp_dir):
        path = tmp_dir / "show.sdb"
        long_name = "A" * 100
        write_show_metadata(path, ShowInfo(long_name, "2024", 1, 10))
        loaded = read_show_metadata(path)
        assert len(loaded.name) == 48

    def test_invalid_magic_raises(self, tmp_dir):
        path = tmp_dir / "show.sdb"
        path.write_bytes(b"\x00" * SHOW_SIZE)
        with pytest.raises(ValueError, match="Invalid magic"):
            read_show_metadata(path)

    def test_invalid_size_raises(self, tmp_dir):
        path = tmp_dir / "show.sdb"
        path.write_bytes(b"\x00" * 64)
        with pytest.raises(ValueError, match="Invalid show.sdb size"):
            read_show_metadata(path)

    def test_creates_parent_dirs(self, tmp_dir):
        path = tmp_dir / "deep" / "nested" / "show.sdb"
        write_show_metadata(path, ShowInfo("Test", "2024", 1, 10))
        assert path.exists()


class TestSeasonMetadata:
    def test_round_trip(self, tmp_dir):
        path = tmp_dir / "season.sdb"
        original = SeasonInfo(
            season_number=3,
            episode_count=23,
            year="1991",
            title="Season 3",
        )
        write_season_metadata(path, original)
        loaded = read_season_metadata(path)

        assert loaded.season_number == original.season_number
        assert loaded.episode_count == original.episode_count
        assert loaded.year == original.year
        assert loaded.title == original.title

    def test_file_size(self, tmp_dir):
        path = tmp_dir / "season.sdb"
        write_season_metadata(path, SeasonInfo(1, 5, "2024", "Season 1"))
        assert path.stat().st_size == SEASON_SIZE

    def test_magic_bytes(self, tmp_dir):
        path = tmp_dir / "season.sdb"
        write_season_metadata(path, SeasonInfo(1, 5, "2024", "Season 1"))
        data = path.read_bytes()
        assert data[:4] == SEASON_MAGIC

    def test_invalid_magic_raises(self, tmp_dir):
        path = tmp_dir / "season.sdb"
        path.write_bytes(b"\x00" * SEASON_SIZE)
        with pytest.raises(ValueError, match="Invalid magic"):
            read_season_metadata(path)


class TestEpisodeMetadata:
    def test_round_trip(self, tmp_dir):
        path = tmp_dir / "E01.sdb"
        original = EpisodeInfo(
            season_number=3,
            episode_number=7,
            title="The Pez Dispenser",
            air_date="1992-01-15",
            runtime_minutes=22,
            description="Jerry's girlfriend is distracted by a Pez dispenser.",
        )
        write_episode_metadata(path, original)
        loaded = read_episode_metadata(path)

        assert loaded.season_number == original.season_number
        assert loaded.episode_number == original.episode_number
        assert loaded.title == original.title
        assert loaded.air_date == original.air_date
        assert loaded.runtime_minutes == original.runtime_minutes
        assert loaded.description == original.description

    def test_file_size(self, tmp_dir):
        path = tmp_dir / "E01.sdb"
        write_episode_metadata(path, EpisodeInfo(1, 1, "Pilot", "1989-07-05", 23, "Test"))
        assert path.stat().st_size == EPISODE_SIZE

    def test_magic_bytes(self, tmp_dir):
        path = tmp_dir / "E01.sdb"
        write_episode_metadata(path, EpisodeInfo(1, 1, "Pilot", "1989-07-05", 23, "Test"))
        data = path.read_bytes()
        assert data[:4] == EPISODE_MAGIC

    def test_long_description_truncated(self, tmp_dir):
        path = tmp_dir / "E01.sdb"
        long_desc = "X" * 200
        write_episode_metadata(path, EpisodeInfo(1, 1, "Test", "2024-01-01", 22, long_desc))
        loaded = read_episode_metadata(path)
        assert len(loaded.description) == 56

    def test_long_title_truncated(self, tmp_dir):
        path = tmp_dir / "E01.sdb"
        long_title = "T" * 100
        write_episode_metadata(path, EpisodeInfo(1, 1, long_title, "2024-01-01", 22, "Desc"))
        loaded = read_episode_metadata(path)
        assert len(loaded.title) == 48

    def test_binary_layout(self, tmp_dir):
        """Verify exact byte positions match firmware expectations."""
        path = tmp_dir / "E01.sdb"
        write_episode_metadata(path, EpisodeInfo(
            season_number=5, episode_number=12,
            title="The Stall", air_date="1994-01-06",
            runtime_minutes=22, description="Elaine needs toilet paper."
        ))
        data = path.read_bytes()

        # Check struct fields at exact offsets
        assert data[0:4] == b"SFEP"
        assert data[4] == 5   # season
        assert data[5] == 12  # episode
        assert struct.unpack_from("<H", data, 6)[0] == 22  # runtime

        # Title starts at offset 8
        title_bytes = data[8:56].rstrip(b"\x00")
        assert title_bytes == b"The Stall"

        # Air date at offset 56
        date_bytes = data[56:68].rstrip(b"\x00")
        assert date_bytes == b"1994-01-06"

        # Description at offset 68
        desc_bytes = data[68:124].rstrip(b"\x00")
        assert desc_bytes == b"Elaine needs toilet paper."


# ─── Movies ──────────────────────────────────────────────────────────────────

class TestMovieMetadata:
    def test_round_trip(self, tmp_dir):
        path = tmp_dir / "movie.sdb"
        original = MovieInfo(
            title="The Matrix",
            year="1999",
            runtime_minutes=136,
            description="A hacker learns about the true nature of reality.",
        )
        write_movie_metadata(path, original)
        loaded = read_movie_metadata(path)

        assert loaded.title == original.title
        assert loaded.year == original.year
        assert loaded.runtime_minutes == original.runtime_minutes
        assert loaded.description == original.description

    def test_file_size(self, tmp_dir):
        path = tmp_dir / "movie.sdb"
        write_movie_metadata(path, MovieInfo("Test", "2024", 90, "Desc"))
        assert path.stat().st_size == MOVIE_SIZE

    def test_magic_bytes(self, tmp_dir):
        path = tmp_dir / "movie.sdb"
        write_movie_metadata(path, MovieInfo("Test", "2024", 90, "Desc"))
        data = path.read_bytes()
        assert data[:4] == MOVIE_MAGIC

    def test_version(self, tmp_dir):
        path = tmp_dir / "movie.sdb"
        write_movie_metadata(path, MovieInfo("Test", "2024", 90, "Desc"))
        data = path.read_bytes()
        assert data[4] == FORMAT_VERSION

    def test_binary_layout(self, tmp_dir):
        path = tmp_dir / "movie.sdb"
        write_movie_metadata(path, MovieInfo(
            title="Blade Runner", year="1982",
            runtime_minutes=117, description="A blade runner hunts replicants."
        ))
        data = path.read_bytes()

        assert data[0:4] == b"TJMV"
        assert data[4] == FORMAT_VERSION
        assert struct.unpack_from("<H", data, 6)[0] == 117
        assert data[8:56].rstrip(b"\x00") == b"Blade Runner"
        assert data[56:64].rstrip(b"\x00") == b"1982"
        assert data[64:120].rstrip(b"\x00") == b"A blade runner hunts replicants."

    def test_invalid_magic_raises(self, tmp_dir):
        path = tmp_dir / "movie.sdb"
        path.write_bytes(b"\x00" * MOVIE_SIZE)
        with pytest.raises(ValueError, match="Invalid magic"):
            read_movie_metadata(path)

    def test_invalid_size_raises(self, tmp_dir):
        path = tmp_dir / "movie.sdb"
        path.write_bytes(b"\x00" * 64)
        with pytest.raises(ValueError, match="Invalid movie.sdb size"):
            read_movie_metadata(path)

    def test_long_title_truncated(self, tmp_dir):
        path = tmp_dir / "movie.sdb"
        write_movie_metadata(path, MovieInfo("T" * 100, "2024", 90, "Desc"))
        loaded = read_movie_metadata(path)
        assert len(loaded.title) == 48


# ─── Music Videos ────────────────────────────────────────────────────────────

class TestCollectionMetadata:
    def test_round_trip(self, tmp_dir):
        path = tmp_dir / "collection.sdb"
        original = CollectionInfo(name="80s Hits", year="1985", video_count=12)
        write_collection_metadata(path, original)
        loaded = read_collection_metadata(path)

        assert loaded.name == original.name
        assert loaded.year == original.year
        assert loaded.video_count == original.video_count

    def test_file_size(self, tmp_dir):
        path = tmp_dir / "collection.sdb"
        write_collection_metadata(path, CollectionInfo("Test", "2024", 5))
        assert path.stat().st_size == COLLECTION_SIZE

    def test_magic_bytes(self, tmp_dir):
        path = tmp_dir / "collection.sdb"
        write_collection_metadata(path, CollectionInfo("Test", "2024", 5))
        data = path.read_bytes()
        assert data[:4] == COLLECTION_MAGIC

    def test_invalid_magic_raises(self, tmp_dir):
        path = tmp_dir / "collection.sdb"
        path.write_bytes(b"\x00" * COLLECTION_SIZE)
        with pytest.raises(ValueError, match="Invalid magic"):
            read_collection_metadata(path)


class TestVideoMetadata:
    def test_round_trip(self, tmp_dir):
        path = tmp_dir / "V01.sdb"
        original = VideoInfo(
            video_number=1,
            title="Take On Me",
            artist="a-ha",
            runtime_minutes=4,
            description="Iconic music video with pencil sketch animation.",
        )
        write_video_metadata(path, original)
        loaded = read_video_metadata(path)

        assert loaded.video_number == original.video_number
        assert loaded.title == original.title
        assert loaded.artist == original.artist
        assert loaded.runtime_minutes == original.runtime_minutes
        assert loaded.description == original.description

    def test_file_size(self, tmp_dir):
        path = tmp_dir / "V01.sdb"
        write_video_metadata(path, VideoInfo(1, "Test", "Art", 3, "Desc"))
        assert path.stat().st_size == VIDEO_SIZE

    def test_magic_bytes(self, tmp_dir):
        path = tmp_dir / "V01.sdb"
        write_video_metadata(path, VideoInfo(1, "Test", "Art", 3, "Desc"))
        data = path.read_bytes()
        assert data[:4] == VIDEO_MAGIC

    def test_binary_layout(self, tmp_dir):
        path = tmp_dir / "V01.sdb"
        write_video_metadata(path, VideoInfo(
            video_number=3, title="Thriller", artist="MJ",
            runtime_minutes=14, description="Zombie dance."
        ))
        data = path.read_bytes()

        assert data[0:4] == b"TJVD"
        assert data[4] == 3  # video number
        assert struct.unpack_from("<H", data, 6)[0] == 14
        assert data[8:56].rstrip(b"\x00") == b"Thriller"
        assert data[56:68].rstrip(b"\x00") == b"MJ"
        assert data[68:124].rstrip(b"\x00") == b"Zombie dance."

    def test_invalid_magic_raises(self, tmp_dir):
        path = tmp_dir / "V01.sdb"
        path.write_bytes(b"\x00" * VIDEO_SIZE)
        with pytest.raises(ValueError, match="Invalid magic"):
            read_video_metadata(path)


# ─── Music ───────────────────────────────────────────────────────────────────

class TestArtistMetadata:
    def test_round_trip(self, tmp_dir):
        path = tmp_dir / "artist.sdb"
        original = ArtistInfo(
            name="Pink Floyd",
            genre="Rock",
            album_count=15,
            total_tracks=165,
        )
        write_artist_metadata(path, original)
        loaded = read_artist_metadata(path)

        assert loaded.name == original.name
        assert loaded.genre == original.genre
        assert loaded.album_count == original.album_count
        assert loaded.total_tracks == original.total_tracks

    def test_file_size(self, tmp_dir):
        path = tmp_dir / "artist.sdb"
        write_artist_metadata(path, ArtistInfo("Test", "Pop", 1, 10))
        assert path.stat().st_size == ARTIST_SIZE

    def test_magic_bytes(self, tmp_dir):
        path = tmp_dir / "artist.sdb"
        write_artist_metadata(path, ArtistInfo("Test", "Pop", 1, 10))
        data = path.read_bytes()
        assert data[:4] == ARTIST_MAGIC

    def test_binary_layout(self, tmp_dir):
        path = tmp_dir / "artist.sdb"
        write_artist_metadata(path, ArtistInfo(
            name="Beatles", genre="Rock", album_count=13, total_tracks=213
        ))
        data = path.read_bytes()

        assert data[0:4] == b"TJMA"
        assert data[4] == FORMAT_VERSION
        assert data[5] == 13  # album_count
        assert struct.unpack_from("<H", data, 6)[0] == 213  # total_tracks
        assert data[8:56].rstrip(b"\x00") == b"Beatles"
        assert data[56:64].rstrip(b"\x00") == b"Rock"

    def test_invalid_magic_raises(self, tmp_dir):
        path = tmp_dir / "artist.sdb"
        path.write_bytes(b"\x00" * ARTIST_SIZE)
        with pytest.raises(ValueError, match="Invalid magic"):
            read_artist_metadata(path)


class TestMusicAlbumMetadata:
    def test_round_trip(self, tmp_dir):
        path = tmp_dir / "album.sdb"
        original = MusicAlbumInfo(
            album_number=1,
            track_count=13,
            year="1973",
            title="Dark Side of the Moon",
        )
        write_music_album_metadata(path, original)
        loaded = read_music_album_metadata(path)

        assert loaded.album_number == original.album_number
        assert loaded.track_count == original.track_count
        assert loaded.year == original.year
        assert loaded.title == original.title

    def test_file_size(self, tmp_dir):
        path = tmp_dir / "album.sdb"
        write_music_album_metadata(path, MusicAlbumInfo(1, 10, "2024", "Album"))
        assert path.stat().st_size == MUSIC_ALBUM_SIZE

    def test_magic_bytes(self, tmp_dir):
        path = tmp_dir / "album.sdb"
        write_music_album_metadata(path, MusicAlbumInfo(1, 10, "2024", "Album"))
        data = path.read_bytes()
        assert data[:4] == MUSIC_ALBUM_MAGIC

    def test_long_title_truncated(self, tmp_dir):
        path = tmp_dir / "album.sdb"
        write_music_album_metadata(path, MusicAlbumInfo(1, 10, "2024", "T" * 50))
        loaded = read_music_album_metadata(path)
        assert len(loaded.title) == 24

    def test_invalid_magic_raises(self, tmp_dir):
        path = tmp_dir / "album.sdb"
        path.write_bytes(b"\x00" * MUSIC_ALBUM_SIZE)
        with pytest.raises(ValueError, match="Invalid magic"):
            read_music_album_metadata(path)


class TestTrackMetadata:
    def test_round_trip(self, tmp_dir):
        path = tmp_dir / "T01.sdb"
        original = TrackInfo(
            track_number=5,
            title="Money",
            runtime_seconds=383,
        )
        write_track_metadata(path, original)
        loaded = read_track_metadata(path)

        assert loaded.track_number == original.track_number
        assert loaded.title == original.title
        assert loaded.runtime_seconds == original.runtime_seconds

    def test_file_size(self, tmp_dir):
        path = tmp_dir / "T01.sdb"
        write_track_metadata(path, TrackInfo(1, "Test", 240))
        assert path.stat().st_size == TRACK_SIZE

    def test_magic_bytes(self, tmp_dir):
        path = tmp_dir / "T01.sdb"
        write_track_metadata(path, TrackInfo(1, "Test", 240))
        data = path.read_bytes()
        assert data[:4] == TRACK_MAGIC

    def test_binary_layout(self, tmp_dir):
        path = tmp_dir / "T01.sdb"
        write_track_metadata(path, TrackInfo(
            track_number=3, title="Time", runtime_seconds=413
        ))
        data = path.read_bytes()

        assert data[0:4] == b"TJTK"
        assert data[4] == 3  # track_number
        assert struct.unpack_from("<H", data, 6)[0] == 413
        assert data[8:56].rstrip(b"\x00") == b"Time"

    def test_invalid_magic_raises(self, tmp_dir):
        path = tmp_dir / "T01.sdb"
        path.write_bytes(b"\x00" * TRACK_SIZE)
        with pytest.raises(ValueError, match="Invalid magic"):
            read_track_metadata(path)


# ─── Photos ──────────────────────────────────────────────────────────────────

class TestPhotoAlbumMetadata:
    def test_round_trip(self, tmp_dir):
        path = tmp_dir / "album.sdb"
        original = PhotoAlbumInfo(title="Vacation 2024", photo_count=42)
        write_photo_album_metadata(path, original)
        loaded = read_photo_album_metadata(path)

        assert loaded.title == original.title
        assert loaded.photo_count == original.photo_count

    def test_file_size(self, tmp_dir):
        path = tmp_dir / "album.sdb"
        write_photo_album_metadata(path, PhotoAlbumInfo("Test", 5))
        assert path.stat().st_size == PHOTO_ALBUM_SIZE

    def test_magic_bytes(self, tmp_dir):
        path = tmp_dir / "album.sdb"
        write_photo_album_metadata(path, PhotoAlbumInfo("Test", 5))
        data = path.read_bytes()
        assert data[:4] == PHOTO_ALBUM_MAGIC

    def test_version(self, tmp_dir):
        path = tmp_dir / "album.sdb"
        write_photo_album_metadata(path, PhotoAlbumInfo("Test", 5))
        data = path.read_bytes()
        assert data[4] == FORMAT_VERSION

    def test_invalid_magic_raises(self, tmp_dir):
        path = tmp_dir / "album.sdb"
        path.write_bytes(b"\x00" * PHOTO_ALBUM_SIZE)
        with pytest.raises(ValueError, match="Invalid magic"):
            read_photo_album_metadata(path)


class TestPhotoMetadata:
    def test_round_trip(self, tmp_dir):
        path = tmp_dir / "P01.sdb"
        original = PhotoInfo(
            photo_number=7,
            caption="Sunset at the beach",
            date_taken="20240815",
        )
        write_photo_metadata(path, original)
        loaded = read_photo_metadata(path)

        assert loaded.photo_number == original.photo_number
        assert loaded.caption == original.caption
        assert loaded.date_taken == original.date_taken

    def test_file_size(self, tmp_dir):
        path = tmp_dir / "P01.sdb"
        write_photo_metadata(path, PhotoInfo(1, "Test", "20240101"))
        assert path.stat().st_size == PHOTO_SIZE

    def test_magic_bytes(self, tmp_dir):
        path = tmp_dir / "P01.sdb"
        write_photo_metadata(path, PhotoInfo(1, "Test", "20240101"))
        data = path.read_bytes()
        assert data[:4] == PHOTO_MAGIC

    def test_binary_layout(self, tmp_dir):
        path = tmp_dir / "P01.sdb"
        write_photo_metadata(path, PhotoInfo(
            photo_number=12, caption="Mountain view", date_taken="20231225"
        ))
        data = path.read_bytes()

        assert data[0:4] == b"TJPH"
        assert data[4] == 12  # photo_number
        assert data[8:56].rstrip(b"\x00") == b"Mountain view"
        assert data[56:64].rstrip(b"\x00") == b"20231225"

    def test_invalid_magic_raises(self, tmp_dir):
        path = tmp_dir / "P01.sdb"
        path.write_bytes(b"\x00" * PHOTO_SIZE)
        with pytest.raises(ValueError, match="Invalid magic"):
            read_photo_metadata(path)

    def test_long_caption_truncated(self, tmp_dir):
        path = tmp_dir / "P01.sdb"
        write_photo_metadata(path, PhotoInfo(1, "C" * 100, "20240101"))
        loaded = read_photo_metadata(path)
        assert len(loaded.caption) == 48
