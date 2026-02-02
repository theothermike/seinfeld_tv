"""Tests for music_packager module."""

import subprocess
import pytest
from pathlib import Path
from unittest.mock import patch, MagicMock, call

from converter.music_packager import (
    convert_audio_to_avi,
    package_music,
    sanitize_name,
)
from converter.binary_writer import (
    read_artist_metadata,
    read_music_album_metadata,
    read_track_metadata,
)


class TestSanitizeName:
    def test_simple_name(self):
        assert sanitize_name("Pink Floyd") == "Pink_Floyd"

    def test_special_characters(self):
        assert sanitize_name("AC/DC") == "ACDC"

    def test_truncation(self):
        long_name = "A" * 50
        assert len(sanitize_name(long_name)) <= 31

    def test_empty_fallback(self):
        assert sanitize_name("!!!") == "Artist"

    def test_spaces_and_hyphens(self):
        assert sanitize_name("Nine Inch Nails") == "Nine_Inch_Nails"

    def test_multiple_special_chars(self):
        assert sanitize_name("Guns N' Roses") == "Guns_N_Roses"

    def test_trailing_underscore_after_truncation(self):
        # If truncation lands right after an underscore, it should be stripped
        name = "A" * 30 + "_B"
        result = sanitize_name(name)
        assert len(result) <= 31
        assert not result.endswith("_")


class TestConvertAudioToAvi:
    @patch("converter.music_packager.subprocess.run")
    def test_with_art_path(self, mock_run, tmp_path):
        """Verify ffmpeg is called with correct args when art is provided."""
        mock_run.return_value = MagicMock(returncode=0)

        audio = tmp_path / "song.mp3"
        audio.write_bytes(b"fake_audio")
        art = tmp_path / "cover.jpg"
        art.write_bytes(b"fake_image")
        output = tmp_path / "output" / "T01.avi"

        # Create the output file so stat() works in the success log
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(b"fake_avi")

        result = convert_audio_to_avi(audio, output, art_path=art, fps=1, quality=5)

        assert result is True
        assert mock_run.call_count == 1

        cmd = mock_run.call_args[0][0]
        assert cmd[0] == "ffmpeg"
        assert "-loop" in cmd
        assert "1" in cmd
        assert str(art) in cmd
        assert str(audio) in cmd
        assert "-c:v" in cmd
        assert "mjpeg" in cmd
        assert "-c:a" in cmd
        assert "pcm_u8" in cmd
        assert "-ar" in cmd
        assert "11025" in cmd
        assert "-ac" in cmd
        assert "-shortest" in cmd
        assert "-r" in cmd
        assert str(output) in cmd

    @patch("converter.music_packager.subprocess.run")
    def test_without_art_path_uses_placeholder(self, mock_run, tmp_path):
        """Verify a black placeholder image is created when no art provided."""
        mock_run.return_value = MagicMock(returncode=0)

        audio = tmp_path / "song.mp3"
        audio.write_bytes(b"fake_audio")
        output = tmp_path / "T01.avi"
        output.write_bytes(b"fake_avi")

        result = convert_audio_to_avi(audio, output)

        assert result is True
        assert mock_run.call_count == 1

        cmd = mock_run.call_args[0][0]
        # The image input should be a temp file (not None)
        loop_idx = cmd.index("-loop")
        # After "-loop", "1", "-i", <image_path>
        image_path = cmd[loop_idx + 3]
        assert image_path.endswith(".png")

    @patch("converter.music_packager.subprocess.run")
    def test_ffmpeg_failure_returns_false(self, mock_run, tmp_path):
        """Verify False is returned when ffmpeg exits with non-zero."""
        mock_run.return_value = MagicMock(returncode=1, stderr="error output")

        audio = tmp_path / "song.mp3"
        audio.write_bytes(b"fake_audio")
        output = tmp_path / "T01.avi"

        result = convert_audio_to_avi(audio, output)
        assert result is False

    @patch("converter.music_packager.subprocess.run")
    def test_ffmpeg_not_found_returns_false(self, mock_run, tmp_path):
        """Verify False when ffmpeg binary is not found."""
        mock_run.side_effect = FileNotFoundError("ffmpeg not found")

        audio = tmp_path / "song.mp3"
        audio.write_bytes(b"fake_audio")
        output = tmp_path / "T01.avi"

        result = convert_audio_to_avi(audio, output)
        assert result is False

    @patch("converter.music_packager.subprocess.run")
    def test_ffmpeg_timeout_returns_false(self, mock_run, tmp_path):
        """Verify False when ffmpeg times out."""
        mock_run.side_effect = subprocess.TimeoutExpired(cmd="ffmpeg", timeout=3600)

        audio = tmp_path / "song.mp3"
        audio.write_bytes(b"fake_audio")
        output = tmp_path / "T01.avi"

        result = convert_audio_to_avi(audio, output)
        assert result is False


class TestPackageMusic:
    @patch("converter.music_packager.generate_thumbnail")
    @patch("converter.music_packager.convert_audio_to_avi")
    def test_full_pipeline(self, mock_convert, mock_thumb, tmp_path):
        """Test packaging 2 albums with 2 tracks each."""
        mock_convert.return_value = True
        mock_thumb.return_value = True

        # Set up input directory with 2 albums
        input_dir = tmp_path / "input"
        album1 = input_dir / "Album One"
        album2 = input_dir / "Album Two"
        album1.mkdir(parents=True)
        album2.mkdir(parents=True)

        # Create audio files
        (album1 / "track_a.mp3").write_bytes(b"audio")
        (album1 / "track_b.flac").write_bytes(b"audio")
        (album2 / "song_x.ogg").write_bytes(b"audio")
        (album2 / "song_y.wav").write_bytes(b"audio")

        # Add cover art to album1
        (album1 / "cover.jpg").write_bytes(b"image")

        output_dir = tmp_path / "output"

        result = package_music(
            input_dir=input_dir,
            output_dir=output_dir,
            artist_name="Test Artist",
            genre="Rock",
        )

        assert result is True

        # Verify directory structure
        artist_dir = output_dir / "Music" / "Test_Artist"
        assert artist_dir.exists()

        # Verify artist metadata
        artist_meta = read_artist_metadata(artist_dir / "artist.sdb")
        assert artist_meta.name == "Test Artist"
        assert artist_meta.genre == "Rock"
        assert artist_meta.album_count == 2
        assert artist_meta.total_tracks == 4

        # Verify album metadata
        album1_meta = read_music_album_metadata(artist_dir / "A01" / "album.sdb")
        assert album1_meta.album_number == 1
        assert album1_meta.track_count == 2
        assert album1_meta.title == "Album One"

        album2_meta = read_music_album_metadata(artist_dir / "A02" / "album.sdb")
        assert album2_meta.album_number == 2
        assert album2_meta.track_count == 2
        assert album2_meta.title == "Album Two"

        # Verify track metadata
        t01 = read_track_metadata(artist_dir / "A01" / "T01.sdb")
        assert t01.track_number == 1
        assert t01.title == "track_a"
        assert t01.runtime_seconds == 0

        t02 = read_track_metadata(artist_dir / "A01" / "T02.sdb")
        assert t02.track_number == 2
        assert t02.title == "track_b"

        # Verify convert_audio_to_avi was called 4 times (2 tracks per album)
        assert mock_convert.call_count == 4

        # Verify thumbnails: 2 album thumbnails + 1 artist thumbnail
        assert mock_thumb.call_count == 3

    @patch("converter.music_packager.generate_thumbnail")
    @patch("converter.music_packager.convert_audio_to_avi")
    def test_empty_input_dir_returns_false(self, mock_convert, mock_thumb, tmp_path):
        """Test that an empty input directory returns False."""
        input_dir = tmp_path / "empty_input"
        input_dir.mkdir()
        output_dir = tmp_path / "output"

        result = package_music(
            input_dir=input_dir,
            output_dir=output_dir,
            artist_name="Nobody",
        )

        assert result is False
        assert mock_convert.call_count == 0

    @patch("converter.music_packager.generate_thumbnail")
    @patch("converter.music_packager.convert_audio_to_avi")
    def test_nonexistent_input_dir_returns_false(self, mock_convert, mock_thumb, tmp_path):
        """Test that a non-existent input directory returns False."""
        result = package_music(
            input_dir=tmp_path / "does_not_exist",
            output_dir=tmp_path / "output",
            artist_name="Ghost",
        )

        assert result is False

    @patch("converter.music_packager.generate_thumbnail")
    @patch("converter.music_packager.convert_audio_to_avi")
    def test_all_conversions_fail_returns_false(self, mock_convert, mock_thumb, tmp_path):
        """Test that if all audio conversions fail, result is False."""
        mock_convert.return_value = False
        mock_thumb.return_value = True

        input_dir = tmp_path / "input"
        album = input_dir / "BadAlbum"
        album.mkdir(parents=True)
        (album / "broken.mp3").write_bytes(b"bad")

        result = package_music(
            input_dir=input_dir,
            output_dir=tmp_path / "output",
            artist_name="Broken",
        )

        assert result is False

    @patch("converter.music_packager.generate_thumbnail")
    @patch("converter.music_packager.convert_audio_to_avi")
    def test_cover_art_priority(self, mock_convert, mock_thumb, tmp_path):
        """Test that cover.jpg is preferred over other images."""
        mock_convert.return_value = True
        mock_thumb.return_value = True

        input_dir = tmp_path / "input"
        album = input_dir / "MyAlbum"
        album.mkdir(parents=True)
        (album / "track.mp3").write_bytes(b"audio")
        (album / "random.jpg").write_bytes(b"img1")
        (album / "cover.jpg").write_bytes(b"img2")

        result = package_music(
            input_dir=input_dir,
            output_dir=tmp_path / "output",
            artist_name="Tester",
        )

        assert result is True

        # Check that convert_audio_to_avi was called with cover.jpg as art_path
        art_arg = mock_convert.call_args[1].get("art_path") or mock_convert.call_args[0][2] if len(mock_convert.call_args[0]) > 2 else mock_convert.call_args[1].get("art_path")
        assert art_arg is not None
        assert art_arg.name == "cover.jpg"

    @patch("converter.music_packager.generate_thumbnail")
    @patch("converter.music_packager.convert_audio_to_avi")
    def test_albums_with_no_audio_skipped(self, mock_convert, mock_thumb, tmp_path):
        """Test that subdirectories with no audio files are skipped."""
        mock_convert.return_value = True
        mock_thumb.return_value = True

        input_dir = tmp_path / "input"
        # Album with no audio
        empty_album = input_dir / "EmptyAlbum"
        empty_album.mkdir(parents=True)
        (empty_album / "readme.txt").write_bytes(b"not audio")

        # Album with audio
        good_album = input_dir / "GoodAlbum"
        good_album.mkdir(parents=True)
        (good_album / "song.mp3").write_bytes(b"audio")

        result = package_music(
            input_dir=input_dir,
            output_dir=tmp_path / "output",
            artist_name="Tester",
        )

        assert result is True

        artist_meta = read_artist_metadata(
            tmp_path / "output" / "Music" / "Tester" / "artist.sdb"
        )
        # Only 1 album should be counted (the one with audio)
        assert artist_meta.album_count == 1
        assert artist_meta.total_tracks == 1
