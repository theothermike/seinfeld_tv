"""Tests for binary_writer module - round-trip and format verification."""

import struct
import pytest
from pathlib import Path

from converter.binary_writer import (
    ShowInfo, SeasonInfo, EpisodeInfo,
    write_show_metadata, read_show_metadata,
    write_season_metadata, read_season_metadata,
    write_episode_metadata, read_episode_metadata,
    SHOW_MAGIC, SEASON_MAGIC, EPISODE_MAGIC,
    SHOW_SIZE, SEASON_SIZE, EPISODE_SIZE,
    FORMAT_VERSION,
)


@pytest.fixture
def tmp_dir(tmp_path):
    return tmp_path


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
