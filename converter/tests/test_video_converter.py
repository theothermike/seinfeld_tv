"""Tests for video_converter module."""

import pytest
from pathlib import Path
from unittest.mock import patch, MagicMock

from converter.video_converter import (
    detect_episode_info,
    scan_input_directory,
    convert_video,
    convert_boot_clip,
    TARGET_WIDTH,
    TARGET_HEIGHT,
    AUDIO_SAMPLE_RATE,
)


class TestDetectEpisodeInfo:
    def test_standard_format(self):
        assert detect_episode_info(Path("Seinfeld.S03E07.mkv")) == (3, 7)

    def test_lowercase(self):
        assert detect_episode_info(Path("seinfeld.s01e01.mp4")) == (1, 1)

    def test_x_format(self):
        assert detect_episode_info(Path("seinfeld.1x01.avi")) == (1, 1)

    def test_full_word_format(self):
        assert detect_episode_info(Path("Season 3 Episode 12.mp4")) == (3, 12)

    def test_compact_format(self):
        assert detect_episode_info(Path("seinfeld_301.mp4")) == (3, 1)

    def test_no_match(self):
        assert detect_episode_info(Path("random_video.mp4")) is None

    def test_with_path_components(self):
        assert detect_episode_info(Path("/data/shows/Seinfeld.S05E12.The.Stall.mkv")) == (5, 12)

    def test_double_digit_season(self):
        assert detect_episode_info(Path("show.S10E05.mp4")) == (10, 5)

    def test_high_episode_number(self):
        assert detect_episode_info(Path("show.S01E100.mp4")) == (1, 100)


class TestScanInputDirectory:
    def test_finds_video_files(self, tmp_path):
        # Create test video files
        (tmp_path / "Seinfeld.S01E01.mp4").touch()
        (tmp_path / "Seinfeld.S01E02.mkv").touch()
        (tmp_path / "Seinfeld.S02E01.avi").touch()
        (tmp_path / "readme.txt").touch()  # Should be ignored

        episodes = scan_input_directory(tmp_path)
        assert len(episodes) == 3
        assert episodes[0].season == 1
        assert episodes[0].episode == 1
        assert episodes[1].season == 1
        assert episodes[1].episode == 2
        assert episodes[2].season == 2
        assert episodes[2].episode == 1

    def test_season_filter(self, tmp_path):
        (tmp_path / "S01E01.mp4").touch()
        (tmp_path / "S02E01.mp4").touch()
        (tmp_path / "S03E01.mp4").touch()

        episodes = scan_input_directory(tmp_path, season_filter=2)
        assert len(episodes) == 1
        assert episodes[0].season == 2

    def test_empty_directory(self, tmp_path):
        episodes = scan_input_directory(tmp_path)
        assert episodes == []

    def test_sorted_output(self, tmp_path):
        # Create files out of order
        (tmp_path / "S02E03.mp4").touch()
        (tmp_path / "S01E01.mp4").touch()
        (tmp_path / "S02E01.mp4").touch()
        (tmp_path / "S01E05.mp4").touch()

        episodes = scan_input_directory(tmp_path)
        keys = [(e.season, e.episode) for e in episodes]
        assert keys == [(1, 1), (1, 5), (2, 1), (2, 3)]

    def test_recursive_scan(self, tmp_path):
        season_dir = tmp_path / "Season 1"
        season_dir.mkdir()
        (season_dir / "S01E01.mp4").touch()

        episodes = scan_input_directory(tmp_path)
        assert len(episodes) == 1

    def test_ignores_non_video_files(self, tmp_path):
        (tmp_path / "S01E01.txt").touch()
        (tmp_path / "S01E01.srt").touch()
        (tmp_path / "S01E01.nfo").touch()

        episodes = scan_input_directory(tmp_path)
        assert len(episodes) == 0


class TestConvertVideo:
    def test_successful_conversion(self, tmp_path):
        input_file = tmp_path / "input.mp4"
        output_file = tmp_path / "output.avi"
        input_file.touch()

        mock_result = MagicMock()
        mock_result.returncode = 0

        # Make output file exist after "conversion"
        def side_effect(*args, **kwargs):
            output_file.write_bytes(b"fake_avi_data")
            return mock_result

        with patch("converter.video_converter.subprocess.run", side_effect=side_effect):
            result = convert_video(input_file, output_file)

        assert result is True

    def test_ffmpeg_failure(self, tmp_path):
        input_file = tmp_path / "input.mp4"
        output_file = tmp_path / "output.avi"
        input_file.touch()

        mock_result = MagicMock()
        mock_result.returncode = 1
        mock_result.stderr = "Error: invalid input"

        with patch("converter.video_converter.subprocess.run", return_value=mock_result):
            result = convert_video(input_file, output_file)

        assert result is False

    def test_ffmpeg_not_found(self, tmp_path):
        input_file = tmp_path / "input.mp4"
        output_file = tmp_path / "output.avi"
        input_file.touch()

        with patch("converter.video_converter.subprocess.run", side_effect=FileNotFoundError):
            result = convert_video(input_file, output_file)

        assert result is False

    def test_ffmpeg_timeout(self, tmp_path):
        import subprocess
        input_file = tmp_path / "input.mp4"
        output_file = tmp_path / "output.avi"
        input_file.touch()

        with patch("converter.video_converter.subprocess.run",
                    side_effect=subprocess.TimeoutExpired("ffmpeg", 3600)):
            result = convert_video(input_file, output_file)

        assert result is False

    def test_creates_output_directory(self, tmp_path):
        input_file = tmp_path / "input.mp4"
        output_file = tmp_path / "deep" / "nested" / "output.avi"
        input_file.touch()

        mock_result = MagicMock()
        mock_result.returncode = 0

        def side_effect(*args, **kwargs):
            output_file.write_bytes(b"fake")
            return mock_result

        with patch("converter.video_converter.subprocess.run", side_effect=side_effect):
            convert_video(input_file, output_file)

        assert output_file.parent.exists()

    def test_ffmpeg_command_args(self, tmp_path):
        """Verify correct ffmpeg arguments are passed."""
        input_file = tmp_path / "input.mp4"
        output_file = tmp_path / "output.avi"
        input_file.touch()

        mock_result = MagicMock()
        mock_result.returncode = 0

        def side_effect(cmd, **kwargs):
            output_file.write_bytes(b"fake")
            return mock_result

        with patch("converter.video_converter.subprocess.run", side_effect=side_effect) as mock_run:
            convert_video(input_file, output_file, fps=24, quality=5)

        cmd = mock_run.call_args[0][0]
        assert "ffmpeg" in cmd[0]
        assert "-vcodec" in cmd
        assert "mjpeg" in cmd
        assert "-acodec" in cmd
        assert "pcm_u8" in cmd
        assert "-ar" in cmd
        assert str(AUDIO_SAMPLE_RATE) in cmd
        assert "-ac" in cmd
        assert "1" in cmd
        assert "-q:v" in cmd
        assert "5" in cmd
        assert "-r" in cmd
        assert "24" in cmd


class TestConvertBootClip:
    def test_includes_duration_limit(self, tmp_path):
        input_file = tmp_path / "theme.mp4"
        output_file = tmp_path / "boot.avi"
        input_file.touch()

        mock_result = MagicMock()
        mock_result.returncode = 0

        with patch("converter.video_converter.subprocess.run", return_value=mock_result) as mock_run:
            convert_boot_clip(input_file, output_file, max_duration=10)

        cmd = mock_run.call_args[0][0]
        assert "-t" in cmd
        assert "10" in cmd
