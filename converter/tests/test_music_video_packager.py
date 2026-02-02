"""Tests for music_video_packager module."""

import pytest
from pathlib import Path
from unittest.mock import patch, MagicMock, call

from converter.music_video_packager import package_music_videos, sanitize_name
from converter.binary_writer import read_collection_metadata, read_video_metadata


class TestSanitizeName:
    def test_simple_name(self):
        assert sanitize_name("80s Hits") == "80s_Hits"

    def test_special_characters(self):
        assert sanitize_name("Rock & Roll: Greatest") == "Rock_Roll_Greatest"

    def test_truncation(self):
        long_name = "A" * 50
        assert len(sanitize_name(long_name)) <= 31

    def test_empty_fallback(self):
        assert sanitize_name("!!!") == "Collection"

    def test_multiple_spaces(self):
        assert sanitize_name("My   Cool   Videos") == "My_Cool_Videos"


class TestPackageMusicVideos:
    @patch("converter.music_video_packager.generate_thumbnail")
    @patch("converter.music_video_packager.convert_video")
    def test_full_pipeline(self, mock_convert, mock_thumb, tmp_path):
        mock_convert.return_value = True
        mock_thumb.return_value = True

        # Create fake input video files
        input_dir = tmp_path / "input"
        input_dir.mkdir()
        (input_dir / "dancing_queen.mp4").write_bytes(b"fake_video")
        (input_dir / "bohemian_rhapsody.mkv").write_bytes(b"fake_video")

        output_dir = tmp_path / "output"

        result = package_music_videos(
            input_dir=input_dir,
            output_dir=output_dir,
            collection_name="Classic Hits",
            year="2024",
        )

        assert result is True

        # Verify directory structure
        collection_dir = output_dir / "MusicVideos" / "Classic_Hits"
        assert collection_dir.exists()

        # Verify collection metadata
        meta = read_collection_metadata(collection_dir / "collection.sdb")
        assert meta.name == "Classic Hits"
        assert meta.year == "2024"
        assert meta.video_count == 2

        # Verify video metadata (sorted alphabetically, bohemian first)
        v1_meta = read_video_metadata(collection_dir / "V01.sdb")
        assert v1_meta.title == "bohemian rhapsody"
        assert v1_meta.video_number == 1

        v2_meta = read_video_metadata(collection_dir / "V02.sdb")
        assert v2_meta.title == "dancing queen"
        assert v2_meta.video_number == 2

        # Verify convert was called for each video
        assert mock_convert.call_count == 2

        # Verify thumbnails: one per video + one for collection
        assert mock_thumb.call_count == 3

    @patch("converter.music_video_packager.generate_thumbnail")
    @patch("converter.music_video_packager.convert_video")
    def test_partial_conversion_failure(self, mock_convert, mock_thumb, tmp_path):
        """If some videos fail to convert, the rest should still be packaged."""
        # First call fails, second succeeds
        mock_convert.side_effect = [False, True]
        mock_thumb.return_value = True

        input_dir = tmp_path / "input"
        input_dir.mkdir()
        (input_dir / "aaa_bad_video.mp4").write_bytes(b"fake")
        (input_dir / "bbb_good_video.mp4").write_bytes(b"fake")

        output_dir = tmp_path / "output"

        result = package_music_videos(
            input_dir=input_dir,
            output_dir=output_dir,
            collection_name="Mixed",
        )

        assert result is True

        # Collection should report 1 successful video
        meta = read_collection_metadata(
            output_dir / "MusicVideos" / "Mixed" / "collection.sdb"
        )
        assert meta.video_count == 1

    @patch("converter.music_video_packager.generate_thumbnail")
    @patch("converter.music_video_packager.convert_video")
    def test_all_conversions_fail(self, mock_convert, mock_thumb, tmp_path):
        mock_convert.return_value = False

        input_dir = tmp_path / "input"
        input_dir.mkdir()
        (input_dir / "bad_video.mp4").write_bytes(b"fake")

        output_dir = tmp_path / "output"

        result = package_music_videos(
            input_dir=input_dir,
            output_dir=output_dir,
            collection_name="Failures",
        )

        assert result is False

    def test_empty_input_dir(self, tmp_path):
        input_dir = tmp_path / "empty"
        input_dir.mkdir()

        result = package_music_videos(
            input_dir=input_dir,
            output_dir=tmp_path / "output",
            collection_name="Empty",
        )

        assert result is False

    def test_missing_input_dir(self, tmp_path):
        result = package_music_videos(
            input_dir=tmp_path / "nonexistent",
            output_dir=tmp_path / "output",
            collection_name="Missing",
        )

        assert result is False

    @patch("converter.music_video_packager.generate_thumbnail")
    @patch("converter.music_video_packager.convert_video")
    def test_filters_non_video_files(self, mock_convert, mock_thumb, tmp_path):
        """Non-video files should be ignored."""
        mock_convert.return_value = True
        mock_thumb.return_value = True

        input_dir = tmp_path / "input"
        input_dir.mkdir()
        (input_dir / "video.mp4").write_bytes(b"fake_video")
        (input_dir / "readme.txt").write_bytes(b"not a video")
        (input_dir / "cover.jpg").write_bytes(b"not a video")

        output_dir = tmp_path / "output"

        result = package_music_videos(
            input_dir=input_dir,
            output_dir=output_dir,
            collection_name="Filtered",
        )

        assert result is True
        assert mock_convert.call_count == 1

    @patch("converter.music_video_packager.generate_thumbnail")
    @patch("converter.music_video_packager.convert_video")
    def test_title_from_filename(self, mock_convert, mock_thumb, tmp_path):
        """Underscores in filenames should become spaces in titles."""
        mock_convert.return_value = True
        mock_thumb.return_value = True

        input_dir = tmp_path / "input"
        input_dir.mkdir()
        (input_dir / "my_great_song.webm").write_bytes(b"fake")

        output_dir = tmp_path / "output"

        package_music_videos(
            input_dir=input_dir,
            output_dir=output_dir,
            collection_name="Titles",
        )

        v1_meta = read_video_metadata(
            output_dir / "MusicVideos" / "Titles" / "V01.sdb"
        )
        assert v1_meta.title == "my great song"
