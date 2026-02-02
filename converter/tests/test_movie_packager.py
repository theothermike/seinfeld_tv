"""Tests for movie_packager module."""

import pytest
from pathlib import Path
from unittest.mock import patch, MagicMock

from converter.movie_packager import package_movie, sanitize_name
from converter.binary_writer import read_movie_metadata


class TestSanitizeName:
    def test_simple_name(self):
        assert sanitize_name("The Matrix") == "The_Matrix"

    def test_special_characters(self):
        assert sanitize_name("Spider-Man: No Way Home") == "SpiderMan_No_Way_Home"

    def test_truncation(self):
        long_name = "A" * 50
        assert len(sanitize_name(long_name)) <= 31

    def test_empty_fallback(self):
        assert sanitize_name("!!!") == "Movie"


class TestPackageMovie:
    @patch("converter.movie_packager.generate_thumbnail")
    @patch("converter.movie_packager.convert_video")
    @patch("converter.movie_packager.MetadataFetcher")
    def test_full_pipeline(self, MockFetcher, mock_convert, mock_thumb, tmp_path):
        fetcher_instance = MockFetcher.return_value
        fetcher_instance.fetch_movie.return_value = {
            "title": "The Matrix",
            "year": "1999",
            "runtime_minutes": 136,
            "description": "A hacker discovers reality is simulated.",
            "poster_url": None,
        }
        mock_convert.return_value = True
        mock_thumb.return_value = True

        input_file = tmp_path / "matrix.mkv"
        input_file.write_bytes(b"fake_video")
        output_dir = tmp_path / "output"

        result = package_movie(
            input_file=input_file,
            output_dir=output_dir,
            title="The Matrix",
        )

        assert result is True

        # Verify files created under Movies/The_Matrix/
        movie_dir = output_dir / "Movies" / "The_Matrix"
        assert movie_dir.exists()

        # Verify metadata
        meta = read_movie_metadata(movie_dir / "movie.sdb")
        assert meta.title == "The Matrix"
        assert meta.year == "1999"
        assert meta.runtime_minutes == 136

        # Verify convert was called
        assert mock_convert.call_count == 1
        # Verify thumbnail was generated
        assert mock_thumb.call_count == 1

    @patch("converter.movie_packager.generate_thumbnail")
    @patch("converter.movie_packager.convert_video")
    @patch("converter.movie_packager.MetadataFetcher")
    def test_conversion_failure(self, MockFetcher, mock_convert, mock_thumb, tmp_path):
        fetcher_instance = MockFetcher.return_value
        fetcher_instance.fetch_movie.return_value = {
            "title": "Test", "year": "", "runtime_minutes": 0,
            "description": "", "poster_url": None,
        }
        mock_convert.return_value = False

        input_file = tmp_path / "test.mkv"
        input_file.write_bytes(b"fake")

        result = package_movie(
            input_file=input_file,
            output_dir=tmp_path / "output",
            title="Test",
        )
        assert result is False

    def test_missing_input_file(self, tmp_path):
        result = package_movie(
            input_file=tmp_path / "nonexistent.mkv",
            output_dir=tmp_path / "output",
            title="Test",
        )
        assert result is False
