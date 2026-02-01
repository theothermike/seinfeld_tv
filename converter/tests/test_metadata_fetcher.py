"""Tests for metadata_fetcher module with mocked API responses."""

import json
import pytest
from pathlib import Path
from unittest.mock import MagicMock, patch

from converter.metadata_fetcher import MetadataFetcher, _strip_html


# Sample TVMaze API response (abbreviated)
TVMAZE_SHOW_RESPONSE = {
    "id": 530,
    "name": "Seinfeld",
    "premiered": "1989-07-05",
    "image": {
        "medium": "https://example.com/seinfeld_medium.jpg",
        "original": "https://example.com/seinfeld_original.jpg",
    },
    "_embedded": {
        "seasons": [
            {
                "id": 1,
                "number": 1,
                "name": "Season 1",
                "premiereDate": "1989-07-05",
                "image": None,
            },
            {
                "id": 2,
                "number": 2,
                "name": "Season 2",
                "premiereDate": "1991-01-23",
                "image": {"medium": "https://example.com/s2.jpg", "original": None},
            },
        ],
        "episodes": [
            {
                "id": 1,
                "season": 1,
                "number": 1,
                "name": "The Seinfeld Chronicles",
                "airdate": "1989-07-05",
                "runtime": 23,
                "summary": "<p>Jerry finds out a woman he met is coming to <b>New York</b>.</p>",
                "image": {"medium": "https://example.com/ep1.jpg", "original": None},
            },
            {
                "id": 2,
                "season": 1,
                "number": 2,
                "name": "The Stakeout",
                "airdate": "1990-05-31",
                "runtime": 22,
                "summary": "<p>Jerry and Elaine attend a party.</p>",
                "image": None,
            },
            {
                "id": 3,
                "season": 2,
                "number": 1,
                "name": "The Ex-Girlfriend",
                "airdate": "1991-01-23",
                "runtime": 22,
                "summary": "<p>George breaks up with his girlfriend.</p>",
                "image": None,
            },
        ],
    },
}


@pytest.fixture
def fetcher(tmp_path):
    return MetadataFetcher(cache_dir=tmp_path / "cache")


class TestStripHtml:
    def test_removes_tags(self):
        assert _strip_html("<p>Hello <b>world</b></p>") == "Hello world"

    def test_empty_string(self):
        assert _strip_html("") == ""

    def test_no_tags(self):
        assert _strip_html("plain text") == "plain text"

    def test_collapses_whitespace(self):
        assert _strip_html("<p>Hello</p>  <p>World</p>") == "Hello World"


class TestMetadataFetcher:
    def test_caching(self, fetcher, tmp_path):
        """Verify responses are cached and reused."""
        mock_resp = MagicMock()
        mock_resp.json.return_value = {"test": "data"}
        mock_resp.raise_for_status = MagicMock()

        with patch.object(fetcher.session, "get", return_value=mock_resp) as mock_get:
            # First call should hit the network
            result1 = fetcher._get_json("https://example.com/api")
            assert mock_get.call_count == 1

            # Second call should use cache
            result2 = fetcher._get_json("https://example.com/api")
            assert mock_get.call_count == 1  # No additional network call
            assert result1 == result2

    def test_fetch_show_success(self, fetcher):
        """Test successful show fetch with mocked API."""
        mock_resp = MagicMock()
        mock_resp.json.return_value = TVMAZE_SHOW_RESPONSE
        mock_resp.raise_for_status = MagicMock()

        with patch.object(fetcher.session, "get", return_value=mock_resp):
            show = fetcher.fetch_show("Seinfeld")

        assert show is not None
        assert show.name == "Seinfeld"
        assert show.year == "1989"
        assert len(show.seasons) == 2
        assert show.total_episodes == 3

        # Check season 1
        s1 = show.seasons[0]
        assert s1.number == 1
        assert s1.episode_count == 2
        assert len(s1.episodes) == 2

        # Check episode data
        ep1 = s1.episodes[0]
        assert ep1.title == "The Seinfeld Chronicles"
        assert ep1.air_date == "1989-07-05"
        assert ep1.runtime == 23
        assert "New York" in ep1.summary
        assert "<" not in ep1.summary  # HTML stripped

        # Check season 2
        s2 = show.seasons[1]
        assert s2.number == 2
        assert s2.episode_count == 1

    def test_fetch_show_not_found(self, fetcher):
        """Test show fetch when API returns None."""
        mock_resp = MagicMock()
        mock_resp.raise_for_status.side_effect = Exception("404")

        import requests as req
        with patch.object(fetcher.session, "get", side_effect=req.ConnectionError("404")):
            show = fetcher.fetch_show("NonexistentShow12345")

        assert show is None

    def test_episodes_sorted_by_number(self, fetcher):
        """Verify episodes within a season are sorted."""
        mock_resp = MagicMock()
        mock_resp.json.return_value = TVMAZE_SHOW_RESPONSE
        mock_resp.raise_for_status = MagicMock()

        with patch.object(fetcher.session, "get", return_value=mock_resp):
            show = fetcher.fetch_show("Seinfeld")

        for season in show.seasons:
            numbers = [ep.number for ep in season.episodes]
            assert numbers == sorted(numbers)

    def test_skips_season_zero(self, fetcher):
        """Season 0 (specials) should be skipped."""
        response = {
            **TVMAZE_SHOW_RESPONSE,
            "_embedded": {
                "seasons": [
                    {"id": 0, "number": 0, "name": "Specials", "premiereDate": "", "image": None},
                    *TVMAZE_SHOW_RESPONSE["_embedded"]["seasons"],
                ],
                "episodes": TVMAZE_SHOW_RESPONSE["_embedded"]["episodes"],
            },
        }
        mock_resp = MagicMock()
        mock_resp.json.return_value = response
        mock_resp.raise_for_status = MagicMock()

        with patch.object(fetcher.session, "get", return_value=mock_resp):
            show = fetcher.fetch_show("Seinfeld")

        season_numbers = [s.number for s in show.seasons]
        assert 0 not in season_numbers

    def test_download_image(self, fetcher, tmp_path):
        """Test image download with caching."""
        mock_resp = MagicMock()
        mock_resp.content = b"\x89PNG\r\n\x1a\nfakeimage"
        mock_resp.raise_for_status = MagicMock()

        output_path = tmp_path / "output" / "test.jpg"

        with patch.object(fetcher.session, "get", return_value=mock_resp):
            result = fetcher.download_image("https://example.com/img.jpg", output_path)

        assert result is True
        assert output_path.exists()
        assert output_path.read_bytes() == b"\x89PNG\r\n\x1a\nfakeimage"


class TestTMDBImages:
    def test_no_key_returns_empty(self, tmp_path):
        fetcher = MetadataFetcher(cache_dir=tmp_path / "cache", tmdb_key=None)
        result = fetcher.fetch_tmdb_images("Seinfeld")
        assert result == {}

    def test_fetch_tmdb_images(self, tmp_path):
        fetcher = MetadataFetcher(cache_dir=tmp_path / "cache", tmdb_key="test_key")

        search_resp = {
            "results": [{"id": 1400, "poster_path": "/poster.jpg"}]
        }
        show_resp = {
            "seasons": [
                {"season_number": 0, "poster_path": "/specials.jpg"},
                {"season_number": 1, "poster_path": "/s1.jpg"},
            ]
        }
        season_resp = {
            "episodes": [
                {"episode_number": 1, "still_path": "/s1e1.jpg"},
                {"episode_number": 2, "still_path": None},
            ]
        }

        call_count = 0
        def mock_get_json(url, params=None):
            nonlocal call_count
            call_count += 1
            if "search/tv" in url:
                return search_resp
            elif "/season/" in url:
                return season_resp
            elif "/tv/" in url:
                return show_resp
            return None

        with patch.object(fetcher, "_tmdb_get", side_effect=mock_get_json):
            images = fetcher.fetch_tmdb_images("Seinfeld")

        assert "show" in images
        assert "s1" in images
        assert "s1e1" in images
        assert "s1e2" not in images  # No still_path
