"""Tests for thumbnail_generator module."""

import pytest
from pathlib import Path
from unittest.mock import patch, MagicMock

from PIL import Image

from converter.thumbnail_generator import (
    rgb888_to_rgb565,
    image_to_rgb565,
    generate_thumbnail_from_image,
    generate_placeholder_thumbnail,
    generate_thumbnail,
    THUMB_WIDTH,
    THUMB_HEIGHT,
    THUMB_SIZE_BYTES,
)


class TestRgb565Conversion:
    def test_black(self):
        assert rgb888_to_rgb565(0, 0, 0) == 0x0000

    def test_white(self):
        # RGB565: R=31, G=63, B=31 -> 0xFFFF
        result = rgb888_to_rgb565(255, 255, 255)
        assert result == 0xFFFF

    def test_pure_red(self):
        # R=31 << 11 = 0xF800
        result = rgb888_to_rgb565(255, 0, 0)
        assert result == 0xF800

    def test_pure_green(self):
        # G=63 << 5 = 0x07E0
        result = rgb888_to_rgb565(0, 255, 0)
        assert result == 0x07E0

    def test_pure_blue(self):
        # B=31 = 0x001F
        result = rgb888_to_rgb565(0, 0, 255)
        assert result == 0x001F


class TestImageToRgb565:
    def test_output_size(self):
        img = Image.new("RGB", (THUMB_WIDTH, THUMB_HEIGHT), (0, 0, 0))
        data = image_to_rgb565(img)
        assert len(data) == THUMB_SIZE_BYTES

    def test_black_image(self):
        img = Image.new("RGB", (2, 2), (0, 0, 0))
        data = image_to_rgb565(img)
        assert data == b"\x00\x00" * 4

    def test_white_image(self):
        img = Image.new("RGB", (1, 1), (255, 255, 255))
        data = image_to_rgb565(img)
        assert data == b"\xff\xff"

    def test_big_endian_byte_order(self):
        """Verify RGB565 values are stored in big-endian order."""
        img = Image.new("RGB", (1, 1), (255, 0, 0))  # Pure red = 0xF800
        data = image_to_rgb565(img)
        assert data[0] == 0xF8  # High byte first
        assert data[1] == 0x00  # Low byte second


class TestGenerateThumbnailFromImage:
    def test_generates_correct_size(self, tmp_path):
        # Create a test image
        img = Image.new("RGB", (640, 480), (128, 64, 32))
        img_path = tmp_path / "test.png"
        img.save(img_path)

        output_path = tmp_path / "thumb.raw"
        result = generate_thumbnail_from_image(img_path, output_path)

        assert result is True
        assert output_path.exists()
        assert output_path.stat().st_size == THUMB_SIZE_BYTES

    def test_handles_small_image(self, tmp_path):
        img = Image.new("RGB", (50, 30), (0, 255, 0))
        img_path = tmp_path / "small.png"
        img.save(img_path)

        output_path = tmp_path / "thumb.raw"
        result = generate_thumbnail_from_image(img_path, output_path)

        assert result is True
        assert output_path.stat().st_size == THUMB_SIZE_BYTES

    def test_handles_wide_image(self, tmp_path):
        img = Image.new("RGB", (1920, 200), (0, 0, 255))
        img_path = tmp_path / "wide.png"
        img.save(img_path)

        output_path = tmp_path / "thumb.raw"
        result = generate_thumbnail_from_image(img_path, output_path)

        assert result is True
        assert output_path.stat().st_size == THUMB_SIZE_BYTES

    def test_creates_parent_dirs(self, tmp_path):
        img = Image.new("RGB", (100, 100), (0, 0, 0))
        img_path = tmp_path / "test.png"
        img.save(img_path)

        output_path = tmp_path / "deep" / "nested" / "thumb.raw"
        result = generate_thumbnail_from_image(img_path, output_path)

        assert result is True
        assert output_path.exists()


class TestGeneratePlaceholderThumbnail:
    def test_correct_size(self, tmp_path):
        output = tmp_path / "placeholder.raw"
        result = generate_placeholder_thumbnail(output)
        assert result is True
        assert output.stat().st_size == THUMB_SIZE_BYTES

    def test_with_text(self, tmp_path):
        output = tmp_path / "placeholder.raw"
        result = generate_placeholder_thumbnail(output, text="S01E01")
        assert result is True
        assert output.stat().st_size == THUMB_SIZE_BYTES


class TestGenerateThumbnail:
    def test_prefers_local_image(self, tmp_path):
        img = Image.new("RGB", (200, 150), (100, 100, 100))
        img_path = tmp_path / "source.png"
        img.save(img_path)

        output = tmp_path / "thumb.raw"
        result = generate_thumbnail(output, image_path=img_path)

        assert result is True
        assert output.stat().st_size == THUMB_SIZE_BYTES

    def test_falls_back_to_placeholder(self, tmp_path):
        output = tmp_path / "thumb.raw"
        result = generate_thumbnail(output)

        assert result is True
        assert output.stat().st_size == THUMB_SIZE_BYTES

    def test_tries_url_before_video(self, tmp_path):
        img = Image.new("RGB", (200, 150), (50, 50, 50))
        img_bytes = b""
        import io
        buf = io.BytesIO()
        img.save(buf, format="PNG")
        img_bytes = buf.getvalue()

        output = tmp_path / "thumb.raw"
        video_path = tmp_path / "video.mp4"
        video_path.touch()

        def mock_download(url, path):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(img_bytes)
            return True

        result = generate_thumbnail(
            output,
            image_url="https://example.com/img.jpg",
            video_path=video_path,
            download_func=mock_download,
        )

        assert result is True
        assert output.stat().st_size == THUMB_SIZE_BYTES
