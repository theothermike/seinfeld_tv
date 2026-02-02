"""Tests for photo_packager module."""

import pytest
from pathlib import Path

from PIL import Image

from converter.photo_packager import convert_photo_to_raw, package_photos, sanitize_name
from converter.binary_writer import read_photo_album_metadata, read_photo_metadata


class TestSanitizeName:
    def test_simple_name(self):
        assert sanitize_name("Vacation 2024") == "Vacation_2024"

    def test_special_characters(self):
        assert sanitize_name("Trip - New York!") == "Trip_New_York"

    def test_truncation(self):
        long_name = "A" * 50
        assert len(sanitize_name(long_name)) <= 31

    def test_empty_fallback(self):
        assert sanitize_name("!!!") == "Album"

    def test_multiple_spaces(self):
        assert sanitize_name("My   Cool   Album") == "My_Cool_Album"


class TestConvertPhotoToRaw:
    def test_output_size(self, tmp_path):
        """Output .raw file should be exactly 56,700 bytes (210x135 RGB565)."""
        # Create a test image
        img = Image.new("RGB", (400, 300), color=(128, 64, 32))
        input_path = tmp_path / "test.jpg"
        img.save(input_path)

        output_path = tmp_path / "P01.raw"
        result = convert_photo_to_raw(input_path, output_path)

        assert result is True
        assert output_path.exists()
        assert output_path.stat().st_size == 56700

    def test_with_small_image(self, tmp_path):
        """Should handle images smaller than target resolution."""
        img = Image.new("RGB", (50, 30), color=(255, 0, 0))
        input_path = tmp_path / "tiny.png"
        img.save(input_path)

        output_path = tmp_path / "P01.raw"
        result = convert_photo_to_raw(input_path, output_path)

        assert result is True
        assert output_path.stat().st_size == 56700

    def test_with_tall_image(self, tmp_path):
        """Should handle tall (portrait) images with cover crop."""
        img = Image.new("RGB", (100, 500), color=(0, 255, 0))
        input_path = tmp_path / "tall.png"
        img.save(input_path)

        output_path = tmp_path / "P01.raw"
        result = convert_photo_to_raw(input_path, output_path)

        assert result is True
        assert output_path.stat().st_size == 56700

    def test_with_wide_image(self, tmp_path):
        """Should handle wide (landscape) images with cover crop."""
        img = Image.new("RGB", (800, 100), color=(0, 0, 255))
        input_path = tmp_path / "wide.bmp"
        img.save(input_path)

        output_path = tmp_path / "P01.raw"
        result = convert_photo_to_raw(input_path, output_path)

        assert result is True
        assert output_path.stat().st_size == 56700

    def test_nonexistent_input(self, tmp_path):
        """Should return False for missing input file."""
        result = convert_photo_to_raw(
            tmp_path / "nonexistent.jpg",
            tmp_path / "P01.raw",
        )
        assert result is False

    def test_creates_parent_dirs(self, tmp_path):
        """Should create output parent directories if they don't exist."""
        img = Image.new("RGB", (210, 135), color=(100, 100, 100))
        input_path = tmp_path / "test.jpg"
        img.save(input_path)

        output_path = tmp_path / "nested" / "dir" / "P01.raw"
        result = convert_photo_to_raw(input_path, output_path)

        assert result is True
        assert output_path.exists()


class TestPackagePhotos:
    def _create_test_images(self, directory, count=3):
        """Helper to create test JPEG images in a directory."""
        directory.mkdir(parents=True, exist_ok=True)
        paths = []
        for i in range(count):
            img = Image.new("RGB", (320, 240), color=(i * 50, 100, 200))
            path = directory / f"photo_{i + 1:02d}.jpg"
            img.save(path)
            paths.append(path)
        return paths

    def test_full_pipeline(self, tmp_path):
        """Package a directory of photos and verify output structure."""
        input_dir = tmp_path / "input"
        self._create_test_images(input_dir, count=3)
        output_dir = tmp_path / "output"

        result = package_photos(
            input_dir=input_dir,
            output_dir=output_dir,
            album_name="Vacation 2024",
        )

        assert result is True

        album_dir = output_dir / "Photos" / "Vacation_2024"
        assert album_dir.exists()

        # Verify album metadata
        album_meta = read_photo_album_metadata(album_dir / "album.sdb")
        assert album_meta.title == "Vacation 2024"
        assert album_meta.photo_count == 3

        # Verify album thumbnail exists
        assert (album_dir / "album.raw").exists()

        # Verify each photo
        for i in range(1, 4):
            photo_num = f"P{i:02d}"
            raw_path = album_dir / f"{photo_num}.raw"
            sdb_path = album_dir / f"{photo_num}.sdb"
            assert raw_path.exists(), f"{photo_num}.raw should exist"
            assert sdb_path.exists(), f"{photo_num}.sdb should exist"
            assert raw_path.stat().st_size == 56700

            meta = read_photo_metadata(sdb_path)
            assert meta.photo_number == i
            assert meta.caption == f"photo_{i:02d}"
            assert meta.date_taken == ""

    def test_single_photo(self, tmp_path):
        """Should work with a single photo."""
        input_dir = tmp_path / "input"
        self._create_test_images(input_dir, count=1)
        output_dir = tmp_path / "output"

        result = package_photos(
            input_dir=input_dir,
            output_dir=output_dir,
            album_name="Solo",
        )

        assert result is True
        album_dir = output_dir / "Photos" / "Solo"
        assert (album_dir / "P01.raw").exists()
        assert (album_dir / "P01.sdb").exists()
        assert (album_dir / "album.sdb").exists()
        assert (album_dir / "album.raw").exists()

    def test_empty_input_dir(self, tmp_path):
        """Should return False for an empty directory."""
        input_dir = tmp_path / "empty"
        input_dir.mkdir()
        output_dir = tmp_path / "output"

        result = package_photos(
            input_dir=input_dir,
            output_dir=output_dir,
            album_name="Empty",
        )

        assert result is False

    def test_nonexistent_input_dir(self, tmp_path):
        """Should return False for a directory that doesn't exist."""
        result = package_photos(
            input_dir=tmp_path / "nonexistent",
            output_dir=tmp_path / "output",
            album_name="Missing",
        )

        assert result is False

    def test_mixed_file_types(self, tmp_path):
        """Should only process image files, ignoring non-image files."""
        input_dir = tmp_path / "mixed"
        input_dir.mkdir()

        # Create one valid image
        img = Image.new("RGB", (200, 150), color=(50, 50, 50))
        img.save(input_dir / "photo.jpg")

        # Create non-image files that should be ignored
        (input_dir / "notes.txt").write_text("not an image")
        (input_dir / "data.csv").write_text("a,b,c")

        output_dir = tmp_path / "output"

        result = package_photos(
            input_dir=input_dir,
            output_dir=output_dir,
            album_name="Mixed",
        )

        assert result is True
        album_meta = read_photo_album_metadata(output_dir / "Photos" / "Mixed" / "album.sdb")
        assert album_meta.photo_count == 1

    def test_alphabetical_ordering(self, tmp_path):
        """Photos should be numbered in alphabetical order of filenames."""
        input_dir = tmp_path / "input"
        input_dir.mkdir()

        # Create images with names that sort differently than creation order
        for name in ["cherry.jpg", "apple.jpg", "banana.jpg"]:
            img = Image.new("RGB", (200, 150), color=(100, 100, 100))
            img.save(input_dir / name)

        output_dir = tmp_path / "output"

        result = package_photos(
            input_dir=input_dir,
            output_dir=output_dir,
            album_name="Fruits",
        )

        assert result is True
        album_dir = output_dir / "Photos" / "Fruits"

        # P01 should be apple (first alphabetically)
        meta1 = read_photo_metadata(album_dir / "P01.sdb")
        assert meta1.caption == "apple"

        meta2 = read_photo_metadata(album_dir / "P02.sdb")
        assert meta2.caption == "banana"

        meta3 = read_photo_metadata(album_dir / "P03.sdb")
        assert meta3.caption == "cherry"
