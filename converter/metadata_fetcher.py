"""
TVMaze + TMDB API client for fetching show/season/episode metadata and images.

TVMaze is the primary source (no auth required).
TMDB provides artwork (free API key required).
All responses are cached locally to avoid repeated requests.
"""

import json
import hashlib
import logging
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

import requests

logger = logging.getLogger(__name__)

TVMAZE_BASE = "https://api.tvmaze.com"
TMDB_BASE = "https://api.themoviedb.org/3"
TMDB_IMAGE_BASE = "https://image.tmdb.org/t/p"

DEFAULT_CACHE_DIR = Path(__file__).parent / "cache"


@dataclass
class EpisodeData:
    season: int
    number: int
    title: str
    air_date: str
    runtime: int
    summary: str
    image_url: Optional[str] = None


@dataclass
class SeasonData:
    number: int
    episode_count: int
    premiere_date: str
    name: str
    episodes: list[EpisodeData] = field(default_factory=list)
    image_url: Optional[str] = None


@dataclass
class ShowData:
    name: str
    year: str
    seasons: list[SeasonData] = field(default_factory=list)
    total_episodes: int = 0
    image_url: Optional[str] = None


def _strip_html(text: str) -> str:
    """Remove HTML tags from API response text."""
    if not text:
        return ""
    import re
    clean = re.sub(r"<[^>]+>", "", text)
    # Collapse whitespace
    clean = re.sub(r"\s+", " ", clean).strip()
    return clean


class MetadataFetcher:
    def __init__(self, cache_dir: Optional[Path] = None, tmdb_key: Optional[str] = None):
        self.cache_dir = cache_dir or DEFAULT_CACHE_DIR
        self.cache_dir.mkdir(parents=True, exist_ok=True)
        self.tmdb_key = tmdb_key
        # Detect if tmdb_key is a Bearer token (JWT) vs a v3 API key
        self._tmdb_is_bearer = tmdb_key and tmdb_key.startswith("eyJ")
        self.session = requests.Session()
        self.session.headers.update({"Accept": "application/json"})

    def _cache_path(self, url: str) -> Path:
        """Generate a cache file path from a URL."""
        url_hash = hashlib.md5(url.encode()).hexdigest()
        return self.cache_dir / f"{url_hash}.json"

    def _get_cached(self, url: str) -> Optional[dict]:
        """Return cached response if available."""
        cache_file = self._cache_path(url)
        if cache_file.exists():
            logger.debug("Cache hit: %s", url)
            return json.loads(cache_file.read_text())
        return None

    def _save_cache(self, url: str, data: dict) -> None:
        """Save response to cache."""
        cache_file = self._cache_path(url)
        cache_file.write_text(json.dumps(data, indent=2))
        logger.debug("Cached: %s", url)

    def _get_json(self, url: str, params: Optional[dict] = None) -> Optional[dict]:
        """Fetch JSON from URL with caching."""
        full_url = url
        if params:
            from urllib.parse import urlencode
            full_url = f"{url}?{urlencode(params, doseq=True)}"

        cached = self._get_cached(full_url)
        if cached is not None:
            return cached

        logger.info("Fetching: %s", full_url)
        try:
            resp = self.session.get(url, params=params, timeout=30)
            resp.raise_for_status()
            data = resp.json()
            self._save_cache(full_url, data)
            return data
        except requests.RequestException as e:
            logger.error("API request failed: %s - %s", full_url, e)
            return None

    def fetch_show(self, show_name: str) -> Optional[ShowData]:
        """
        Fetch complete show data from TVMaze with embedded seasons and episodes.
        Returns ShowData with all seasons and episodes populated.
        """
        url = f"{TVMAZE_BASE}/singlesearch/shows"
        params = {"q": show_name, "embed[]": ["seasons", "episodes"]}
        data = self._get_json(url, params)
        if data is None:
            logger.error("Show not found: %s", show_name)
            return None

        show_name = data.get("name", show_name)
        premiered = data.get("premiered", "")
        year = premiered[:4] if premiered else ""
        show_image = None
        if data.get("image"):
            show_image = data["image"].get("original") or data["image"].get("medium")

        # Parse embedded seasons
        seasons_data = data.get("_embedded", {}).get("seasons", [])
        episodes_data = data.get("_embedded", {}).get("episodes", [])

        # Group episodes by season
        episodes_by_season: dict[int, list[dict]] = {}
        for ep in episodes_data:
            s = ep.get("season", 0)
            episodes_by_season.setdefault(s, []).append(ep)

        seasons = []
        total_episodes = 0
        for s in seasons_data:
            s_num = s.get("number", 0)
            if s_num == 0:
                continue  # skip specials

            s_episodes_raw = episodes_by_season.get(s_num, [])
            s_episodes = []
            for ep in sorted(s_episodes_raw, key=lambda x: x.get("number", 0)):
                ep_image = None
                if ep.get("image"):
                    ep_image = ep["image"].get("original") or ep["image"].get("medium")

                s_episodes.append(EpisodeData(
                    season=s_num,
                    number=ep.get("number", 0),
                    title=ep.get("name", "Unknown"),
                    air_date=ep.get("airdate", ""),
                    runtime=ep.get("runtime", 0) or 0,
                    summary=_strip_html(ep.get("summary", "")),
                    image_url=ep_image,
                ))

            s_image = None
            if s.get("image"):
                s_image = s["image"].get("original") or s["image"].get("medium")

            premiere = s.get("premiereDate", "")
            s_year = premiere[:4] if premiere else year

            seasons.append(SeasonData(
                number=s_num,
                episode_count=len(s_episodes),
                premiere_date=premiere,
                name=s.get("name", f"Season {s_num}"),
                episodes=s_episodes,
                image_url=s_image,
            ))
            total_episodes += len(s_episodes)

        show = ShowData(
            name=show_name,
            year=year,
            seasons=sorted(seasons, key=lambda s: s.number),
            total_episodes=total_episodes,
            image_url=show_image,
        )

        logger.info("Fetched show: %s (%s) - %d seasons, %d episodes",
                     show.name, show.year, len(show.seasons), show.total_episodes)
        return show

    def _tmdb_params(self, extra: Optional[dict] = None) -> dict:
        """Build TMDB query params, adding api_key if using v3 key style."""
        params = dict(extra) if extra else {}
        if self.tmdb_key and not self._tmdb_is_bearer:
            params["api_key"] = self.tmdb_key
        return params

    def _tmdb_get(self, url: str, extra_params: Optional[dict] = None) -> Optional[dict]:
        """Fetch from TMDB with proper auth (Bearer token or api_key param)."""
        params = self._tmdb_params(extra_params)

        # Build full URL for caching
        full_url = url
        if params:
            from urllib.parse import urlencode
            full_url = f"{url}?{urlencode(params, doseq=True)}"

        cached = self._get_cached(full_url)
        if cached is not None:
            return cached

        logger.info("Fetching TMDB: %s", full_url)
        headers = {}
        if self._tmdb_is_bearer:
            headers["Authorization"] = f"Bearer {self.tmdb_key}"

        try:
            resp = self.session.get(url, params=params, headers=headers, timeout=30)
            resp.raise_for_status()
            data = resp.json()
            self._save_cache(full_url, data)
            return data
        except requests.RequestException as e:
            logger.error("TMDB request failed: %s - %s", full_url, e)
            return None

    def fetch_tmdb_images(self, show_name: str) -> dict[str, str]:
        """
        Fetch higher-quality images from TMDB.
        Returns dict mapping keys like 'show', 's1', 's1e1' to image URLs.
        Requires tmdb_key to be set.
        """
        if not self.tmdb_key:
            logger.warning("No TMDB API key provided, skipping TMDB images")
            return {}

        # Search for the show
        url = f"{TMDB_BASE}/search/tv"
        data = self._tmdb_get(url, {"query": show_name})
        if not data or not data.get("results"):
            return {}

        tmdb_id = data["results"][0]["id"]
        images: dict[str, str] = {}

        # Show poster
        poster_path = data["results"][0].get("poster_path")
        if poster_path:
            images["show"] = f"{TMDB_IMAGE_BASE}/w300{poster_path}"

        # Season images
        show_url = f"{TMDB_BASE}/tv/{tmdb_id}"
        show_data = self._tmdb_get(show_url)
        if show_data:
            for season in show_data.get("seasons", []):
                s_num = season.get("season_number", 0)
                if s_num == 0:
                    continue
                s_poster = season.get("poster_path")
                if s_poster:
                    images[f"s{s_num}"] = f"{TMDB_IMAGE_BASE}/w300{s_poster}"

                # Fetch episode stills
                s_url = f"{TMDB_BASE}/tv/{tmdb_id}/season/{s_num}"
                s_data = self._tmdb_get(s_url)
                if s_data:
                    for ep in s_data.get("episodes", []):
                        e_num = ep.get("episode_number", 0)
                        still = ep.get("still_path")
                        if still:
                            images[f"s{s_num}e{e_num}"] = f"{TMDB_IMAGE_BASE}/w300{still}"

        logger.info("Fetched %d TMDB images for %s", len(images), show_name)
        return images

    def download_image(self, url: str, output_path: Path) -> bool:
        """Download an image to a local path."""
        cache_key = f"img_{hashlib.md5(url.encode()).hexdigest()}"
        cached_path = self.cache_dir / cache_key

        if cached_path.exists():
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_bytes(cached_path.read_bytes())
            return True

        try:
            logger.info("Downloading image: %s", url)
            resp = self.session.get(url, timeout=30)
            resp.raise_for_status()

            cached_path.write_bytes(resp.content)
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_bytes(resp.content)
            return True
        except requests.RequestException as e:
            logger.error("Failed to download image: %s - %s", url, e)
            return False
