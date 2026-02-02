"""Tests for device_packager module."""

import pytest
from pathlib import Path
from unittest.mock import patch, MagicMock

from converter.device_packager import package_show, _write_settings, sanitize_show_name
from converter.metadata_fetcher import ShowData, SeasonData, EpisodeData
from converter.binary_writer import read_show_metadata, read_season_metadata, read_episode_metadata


MOCK_SHOW_DATA = ShowData(
    name="Seinfeld",
    year="1989",
    total_episodes=3,
    image_url=None,
    seasons=[
        SeasonData(
            number=1,
            episode_count=2,
            premiere_date="1989-07-05",
            name="Season 1",
            episodes=[
                EpisodeData(season=1, number=1, title="The Seinfeld Chronicles",
                            air_date="1989-07-05", runtime=23,
                            summary="Jerry discovers a woman is coming to NY."),
                EpisodeData(season=1, number=2, title="The Stakeout",
                            air_date="1990-05-31", runtime=22,
                            summary="Jerry and Elaine attend a party."),
            ],
        ),
        SeasonData(
            number=2,
            episode_count=1,
            premiere_date="1991-01-23",
            name="Season 2",
            episodes=[
                EpisodeData(season=2, number=1, title="The Ex-Girlfriend",
                            air_date="1991-01-23", runtime=22,
                            summary="George breaks up with his girlfriend."),
            ],
        ),
    ],
)


@pytest.fixture
def input_dir(tmp_path):
    """Create fake input video files."""
    d = tmp_path / "input"
    d.mkdir()
    (d / "Seinfeld.S01E01.mp4").write_bytes(b"fake_video")
    (d / "Seinfeld.S01E02.mp4").write_bytes(b"fake_video")
    (d / "Seinfeld.S02E01.mp4").write_bytes(b"fake_video")
    return d


@pytest.fixture
def output_dir(tmp_path):
    return tmp_path / "output"


class TestSanitizeShowName:
    def test_simple_name(self):
        assert sanitize_show_name("Seinfeld") == "Seinfeld"

    def test_spaces_to_underscores(self):
        assert sanitize_show_name("The Office") == "The_Office"

    def test_special_characters_removed(self):
        assert sanitize_show_name("It's Always Sunny!") == "Its_Always_Sunny"

    def test_parentheses_removed(self):
        assert sanitize_show_name("The Office (US)") == "The_Office_US"

    def test_multiple_spaces_collapsed(self):
        assert sanitize_show_name("Star  Trek   TNG") == "Star_Trek_TNG"

    def test_truncation(self):
        long_name = "A" * 50
        result = sanitize_show_name(long_name)
        assert len(result) <= 31

    def test_truncation_strips_trailing_underscore(self):
        # Name that would have underscore at position 31
        name = "A" * 30 + " B"
        result = sanitize_show_name(name)
        assert not result.endswith("_")
        assert len(result) <= 31

    def test_empty_string_fallback(self):
        assert sanitize_show_name("") == "Show"

    def test_only_special_chars_fallback(self):
        assert sanitize_show_name("!!!") == "Show"

    def test_leading_trailing_underscores_stripped(self):
        assert sanitize_show_name(" Hello ") == "Hello"


class TestWriteSettings:
    def test_creates_settings_file(self, tmp_path):
        path = tmp_path / "settings.txt"
        _write_settings(path, show_dir_name="Seinfeld")
        assert path.exists()
        content = path.read_text()
        assert "last_show=Seinfeld" in content
        assert "last_season=1" in content
        assert "last_episode=1" in content
        assert "volume=5" in content
        assert "slideshow_interval=5" in content
        assert "media_type=0" in content

    def test_default_empty_show_name(self, tmp_path):
        path = tmp_path / "settings.txt"
        _write_settings(path)
        content = path.read_text()
        assert "last_show=" in content


class TestPackageShow:
    @patch("converter.device_packager.generate_thumbnail")
    @patch("converter.device_packager.convert_video")
    @patch("converter.device_packager.MetadataFetcher")
    def test_full_pipeline(self, MockFetcher, mock_convert, mock_thumb,
                           input_dir, output_dir):
        """Test the full packaging pipeline with mocked externals."""
        # Set up mocks
        fetcher_instance = MockFetcher.return_value
        fetcher_instance.fetch_show.return_value = MOCK_SHOW_DATA
        fetcher_instance.fetch_tmdb_images.return_value = {}
        mock_convert.return_value = True
        mock_thumb.return_value = True

        result = package_show(
            input_dir=input_dir,
            output_dir=output_dir,
            show_name="Seinfeld",
        )

        assert result is True

        # Show files now live under output_dir/TV/Seinfeld/
        show_dir = output_dir / "TV" / "Seinfeld"

        # Verify show.sdb in show subdirectory
        show_meta = read_show_metadata(show_dir / "show.sdb")
        assert show_meta.name == "Seinfeld"
        assert show_meta.season_count == 2
        assert show_meta.total_episodes == 3

        # Verify show.raw thumbnail was generated
        thumb_calls = [str(c[0][0]) for c in mock_thumb.call_args_list]
        assert any("show.raw" in c for c in thumb_calls)

        # Verify season directories and metadata under show dir
        s1_meta = read_season_metadata(show_dir / "S01" / "season.sdb")
        assert s1_meta.season_number == 1
        assert s1_meta.episode_count == 2

        s2_meta = read_season_metadata(show_dir / "S02" / "season.sdb")
        assert s2_meta.season_number == 2
        assert s2_meta.episode_count == 1

        # Verify episode metadata
        ep_meta = read_episode_metadata(show_dir / "S01" / "E01.sdb")
        assert ep_meta.title == "The Seinfeld Chronicles"
        assert ep_meta.season_number == 1
        assert ep_meta.episode_number == 1

        # Verify convert was called for each episode
        assert mock_convert.call_count == 3

        # Verify settings.txt at root level (not in show dir)
        assert (output_dir / "settings.txt").exists()
        settings_content = (output_dir / "settings.txt").read_text()
        assert "last_show=Seinfeld" in settings_content

    @patch("converter.device_packager.MetadataFetcher")
    def test_show_not_found(self, MockFetcher, input_dir, output_dir):
        fetcher_instance = MockFetcher.return_value
        fetcher_instance.fetch_show.return_value = None

        result = package_show(input_dir=input_dir, output_dir=output_dir, show_name="NotAShow")
        assert result is False

    def test_empty_input_dir(self, tmp_path):
        empty_input = tmp_path / "empty"
        empty_input.mkdir()
        output = tmp_path / "output"

        with patch("converter.device_packager.MetadataFetcher") as MockFetcher:
            fetcher_instance = MockFetcher.return_value
            fetcher_instance.fetch_show.return_value = MOCK_SHOW_DATA

            result = package_show(input_dir=empty_input, output_dir=output, show_name="Seinfeld")
        assert result is False

    @patch("converter.device_packager.generate_thumbnail")
    @patch("converter.device_packager.convert_video")
    @patch("converter.device_packager.MetadataFetcher")
    def test_season_filter(self, MockFetcher, mock_convert, mock_thumb,
                           input_dir, output_dir):
        fetcher_instance = MockFetcher.return_value
        fetcher_instance.fetch_show.return_value = MOCK_SHOW_DATA
        fetcher_instance.fetch_tmdb_images.return_value = {}
        mock_convert.return_value = True
        mock_thumb.return_value = True

        result = package_show(
            input_dir=input_dir, output_dir=output_dir,
            show_name="Seinfeld", season_filter=1,
        )

        assert result is True

        show_dir = output_dir / "TV" / "Seinfeld"
        show_meta = read_show_metadata(show_dir / "show.sdb")
        assert show_meta.season_count == 1
        assert show_meta.total_episodes == 2

        # Only S01 should exist under show dir
        assert (show_dir / "S01").exists()
        assert not (show_dir / "S02").exists()

    @patch("converter.device_packager.generate_thumbnail")
    @patch("converter.device_packager.convert_video")
    @patch("converter.device_packager.MetadataFetcher")
    def test_partial_conversion_failure(self, MockFetcher, mock_convert, mock_thumb,
                                        input_dir, output_dir):
        """If some conversions fail, packaging should still succeed partially."""
        fetcher_instance = MockFetcher.return_value
        fetcher_instance.fetch_show.return_value = MOCK_SHOW_DATA
        fetcher_instance.fetch_tmdb_images.return_value = {}
        # First conversion succeeds, rest fail
        mock_convert.side_effect = [True, False, False]
        mock_thumb.return_value = True

        result = package_show(
            input_dir=input_dir, output_dir=output_dir, show_name="Seinfeld",
        )

        # Should still succeed since at least one episode converted
        assert result is True

    @patch("converter.device_packager.generate_thumbnail")
    @patch("converter.device_packager.convert_video")
    @patch("converter.device_packager.MetadataFetcher")
    def test_multi_show_packaging(self, MockFetcher, mock_convert, mock_thumb,
                                   tmp_path):
        """Running converter twice with different shows creates both subdirs."""
        fetcher_instance = MockFetcher.return_value
        fetcher_instance.fetch_tmdb_images.return_value = {}
        mock_convert.return_value = True
        mock_thumb.return_value = True

        output = tmp_path / "output"

        # Package first show
        input1 = tmp_path / "input1"
        input1.mkdir()
        (input1 / "Seinfeld.S01E01.mp4").write_bytes(b"fake_video")

        fetcher_instance.fetch_show.return_value = MOCK_SHOW_DATA
        package_show(input_dir=input1, output_dir=output, show_name="Seinfeld")

        # Package second show
        input2 = tmp_path / "input2"
        input2.mkdir()
        (input2 / "Friends.S01E01.mp4").write_bytes(b"fake_video")

        friends_data = ShowData(
            name="Friends",
            year="1994",
            total_episodes=1,
            image_url=None,
            seasons=[
                SeasonData(
                    number=1, episode_count=1, premiere_date="1994-09-22",
                    name="Season 1",
                    episodes=[
                        EpisodeData(season=1, number=1, title="The Pilot",
                                    air_date="1994-09-22", runtime=22,
                                    summary="Six friends hang out."),
                    ],
                ),
            ],
        )
        fetcher_instance.fetch_show.return_value = friends_data
        package_show(input_dir=input2, output_dir=output, show_name="Friends")

        # Both show directories should exist under TV/
        assert (output / "TV" / "Seinfeld" / "show.sdb").exists()
        assert (output / "TV" / "Friends" / "show.sdb").exists()

        # Settings should not have been overwritten (first show wins)
        settings_content = (output / "settings.txt").read_text()
        assert "last_show=Seinfeld" in settings_content

        # Verify both shows' metadata
        seinfeld_meta = read_show_metadata(output / "TV" / "Seinfeld" / "show.sdb")
        assert seinfeld_meta.name == "Seinfeld"

        friends_meta = read_show_metadata(output / "TV" / "Friends" / "show.sdb")
        assert friends_meta.name == "Friends"
