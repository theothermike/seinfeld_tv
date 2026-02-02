"""Tests for youtube_packager module."""

import json
import subprocess
from pathlib import Path
from unittest.mock import patch, MagicMock

import pytest

from converter.youtube_packager import (
    sanitize_name,
    _extract_year,
    _format_upload_date,
    get_video_info,
    package_youtube,
)
from converter.binary_writer import (
    read_youtube_playlist_metadata,
    read_youtube_video_metadata,
    YOUTUBE_PLAYLIST_SIZE,
    YOUTUBE_VIDEO_SIZE,
)


# ─── sanitize_name ──────────────────────────────────────────────────────────

class TestSanitizeName:
    def test_basic(self):
        assert sanitize_name("My Playlist") == "My_Playlist"

    def test_special_chars(self):
        assert sanitize_name("Test! @#$% Videos") == "Test_Videos"

    def test_truncation(self):
        long_name = "A" * 50
        result = sanitize_name(long_name)
        assert len(result) <= 31

    def test_empty_fallback(self):
        assert sanitize_name("!!!") == "YouTube"

    def test_multiple_underscores_collapsed(self):
        assert sanitize_name("a   b") == "a_b"

    def test_trailing_underscores_stripped(self):
        assert sanitize_name("test_") == "test"


# ─── Date Helpers ───────────────────────────────────────────────────────────

class TestDateHelpers:
    def test_extract_year_yyyymmdd(self):
        assert _extract_year("20230415") == "2023"

    def test_extract_year_with_dashes(self):
        assert _extract_year("2023-04-15") == "2023"

    def test_extract_year_empty(self):
        assert _extract_year("") == ""

    def test_extract_year_short(self):
        assert _extract_year("ab") == ""

    def test_format_upload_date_yyyymmdd(self):
        assert _format_upload_date("20230415") == "2023-04-15"

    def test_format_upload_date_already_formatted(self):
        # Already has dashes - cleaned and reformatted
        assert _format_upload_date("2023-04-15") == "2023-04-15"

    def test_format_upload_date_empty(self):
        assert _format_upload_date("") == ""


# ─── get_video_info ─────────────────────────────────────────────────────────

class TestGetVideoInfo:
    def test_single_video(self):
        video_json = json.dumps({
            "title": "Test Video",
            "uploader": "TestUser",
            "upload_date": "20240115",
            "duration": 300,
            "webpage_url": "https://youtube.com/watch?v=abc123",
            "thumbnail": "https://i.ytimg.com/vi/abc123/maxresdefault.jpg",
            "description": "A test video.",
        })

        mock_result = MagicMock()
        mock_result.returncode = 0
        mock_result.stdout = video_json + "\n"
        mock_result.stderr = ""

        with patch("subprocess.run", return_value=mock_result) as mock_run:
            videos = get_video_info("https://youtube.com/watch?v=abc123")

        assert len(videos) == 1
        assert videos[0]["title"] == "Test Video"
        assert videos[0]["duration"] == 300
        mock_run.assert_called_once()

    def test_playlist(self):
        video1 = json.dumps({"title": "Video 1", "duration": 100})
        video2 = json.dumps({"title": "Video 2", "duration": 200})

        mock_result = MagicMock()
        mock_result.returncode = 0
        mock_result.stdout = video1 + "\n" + video2 + "\n"
        mock_result.stderr = ""

        with patch("subprocess.run", return_value=mock_result):
            videos = get_video_info("https://youtube.com/playlist?list=PLxyz")

        assert len(videos) == 2
        assert videos[0]["title"] == "Video 1"
        assert videos[1]["title"] == "Video 2"

    def test_yt_dlp_not_found(self):
        with patch("subprocess.run", side_effect=FileNotFoundError):
            videos = get_video_info("https://youtube.com/watch?v=abc")

        assert videos == []

    def test_timeout(self):
        with patch("subprocess.run", side_effect=subprocess.TimeoutExpired(cmd="yt-dlp", timeout=120)):
            videos = get_video_info("https://youtube.com/watch?v=abc")

        assert videos == []

    def test_failure(self):
        mock_result = MagicMock()
        mock_result.returncode = 1
        mock_result.stdout = ""
        mock_result.stderr = "Error: video not found"

        with patch("subprocess.run", return_value=mock_result):
            videos = get_video_info("https://youtube.com/watch?v=invalid")

        assert videos == []


# ─── package_youtube ────────────────────────────────────────────────────────

class TestPackageYouTube:
    @pytest.fixture
    def output_dir(self, tmp_path):
        return tmp_path / "sdcard"

    def _make_video_info(self, idx=1, title="Test Video", duration=300):
        return {
            "title": title,
            "uploader": "TestChannel",
            "channel": "TestChannel",
            "upload_date": "20240115",
            "duration": duration,
            "webpage_url": f"https://youtube.com/watch?v=vid{idx}",
            "thumbnail": f"https://i.ytimg.com/vi/vid{idx}/maxresdefault.jpg",
            "description": "A test video description.",
            "playlist_title": "Test Playlist",
        }

    @patch("converter.youtube_packager.generate_thumbnail")
    @patch("converter.youtube_packager.convert_video")
    @patch("converter.youtube_packager.download_video")
    @patch("converter.youtube_packager.download_thumbnail")
    @patch("converter.youtube_packager.get_video_info")
    def test_single_video_creates_structure(
        self, mock_info, mock_dl_thumb, mock_dl_video, mock_convert, mock_gen_thumb,
        output_dir,
    ):
        mock_info.return_value = [self._make_video_info()]
        mock_dl_video.return_value = True
        mock_dl_thumb.return_value = False
        mock_convert.return_value = True
        mock_gen_thumb.return_value = True

        # Create a fake AVI so the final thumbnail generation has a file
        playlist_dir = output_dir / "YouTube" / "My_Videos"
        playlist_dir.mkdir(parents=True)
        (playlist_dir / "Y01.avi").write_bytes(b"fake")

        result = package_youtube(
            url="https://youtube.com/watch?v=abc123",
            output_dir=output_dir,
            playlist_name="My Videos",
        )

        assert result is True
        assert (playlist_dir / "playlist.sdb").exists()
        assert (playlist_dir / "Y01.sdb").exists()

        # Verify playlist metadata
        playlist_meta = read_youtube_playlist_metadata(playlist_dir / "playlist.sdb")
        assert playlist_meta.name == "My Videos"
        assert playlist_meta.video_count == 1
        assert playlist_meta.uploader == "TestChannel"

        # Verify video metadata
        video_meta = read_youtube_video_metadata(playlist_dir / "Y01.sdb")
        assert video_meta.video_number == 1
        assert video_meta.title == "Test Video"
        assert video_meta.runtime_minutes == 5  # 300s / 60

    @patch("converter.youtube_packager.generate_thumbnail")
    @patch("converter.youtube_packager.convert_video")
    @patch("converter.youtube_packager.download_video")
    @patch("converter.youtube_packager.download_thumbnail")
    @patch("converter.youtube_packager.get_video_info")
    def test_playlist_creates_multiple_videos(
        self, mock_info, mock_dl_thumb, mock_dl_video, mock_convert, mock_gen_thumb,
        output_dir,
    ):
        mock_info.return_value = [
            self._make_video_info(1, "Video One", 120),
            self._make_video_info(2, "Video Two", 240),
        ]
        mock_dl_video.return_value = True
        mock_dl_thumb.return_value = False
        mock_convert.return_value = True
        mock_gen_thumb.return_value = True

        playlist_dir = output_dir / "YouTube" / "Test_Playlist"
        playlist_dir.mkdir(parents=True)
        (playlist_dir / "Y01.avi").write_bytes(b"fake")

        result = package_youtube(
            url="https://youtube.com/playlist?list=PLxyz",
            output_dir=output_dir,
            playlist_name="Test Playlist",
        )

        assert result is True
        assert (playlist_dir / "Y01.sdb").exists()
        assert (playlist_dir / "Y02.sdb").exists()

        playlist_meta = read_youtube_playlist_metadata(playlist_dir / "playlist.sdb")
        assert playlist_meta.video_count == 2

    @patch("converter.youtube_packager.get_video_info")
    def test_no_videos_found_returns_false(self, mock_info, output_dir):
        mock_info.return_value = []

        result = package_youtube(
            url="https://youtube.com/watch?v=invalid",
            output_dir=output_dir,
            playlist_name="Test",
        )

        assert result is False

    @patch("converter.youtube_packager.generate_thumbnail")
    @patch("converter.youtube_packager.convert_video")
    @patch("converter.youtube_packager.download_video")
    @patch("converter.youtube_packager.download_thumbnail")
    @patch("converter.youtube_packager.get_video_info")
    def test_all_downloads_fail_returns_false(
        self, mock_info, mock_dl_thumb, mock_dl_video, mock_convert, mock_gen_thumb,
        output_dir,
    ):
        mock_info.return_value = [self._make_video_info()]
        mock_dl_video.return_value = False  # Download fails
        mock_dl_thumb.return_value = False
        mock_convert.return_value = True
        mock_gen_thumb.return_value = True

        result = package_youtube(
            url="https://youtube.com/watch?v=abc",
            output_dir=output_dir,
            playlist_name="Test",
        )

        assert result is False

    @patch("converter.youtube_packager.generate_thumbnail")
    @patch("converter.youtube_packager.convert_video")
    @patch("converter.youtube_packager.download_video")
    @patch("converter.youtube_packager.download_thumbnail")
    @patch("converter.youtube_packager.get_video_info")
    def test_auto_detects_playlist_name(
        self, mock_info, mock_dl_thumb, mock_dl_video, mock_convert, mock_gen_thumb,
        output_dir,
    ):
        mock_info.return_value = [self._make_video_info()]
        mock_dl_video.return_value = True
        mock_dl_thumb.return_value = False
        mock_convert.return_value = True
        mock_gen_thumb.return_value = True

        # Pre-create the expected directory
        playlist_dir = output_dir / "YouTube" / "Test_Playlist"
        playlist_dir.mkdir(parents=True)
        (playlist_dir / "Y01.avi").write_bytes(b"fake")

        result = package_youtube(
            url="https://youtube.com/watch?v=abc123",
            output_dir=output_dir,
            playlist_name=None,  # Auto-detect
        )

        assert result is True
        # Should have used playlist_title from video info
        playlist_meta = read_youtube_playlist_metadata(playlist_dir / "playlist.sdb")
        assert playlist_meta.name == "Test Playlist"
