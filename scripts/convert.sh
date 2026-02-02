#!/usr/bin/env bash
#
# convert.sh - Wrapper around the TinyJukebox converter
#
# Usage:
#   ./scripts/convert.sh --type tv --show "Seinfeld" --input ~/tv/Seinfeld/
#   ./scripts/convert.sh --type movie --title "The Matrix" --input ~/movies/matrix.mkv
#   ./scripts/convert.sh --type music-video --collection "80s Hits" --input ~/mv/80s/
#   ./scripts/convert.sh --type music --artist "Pink Floyd" --input ~/music/pinkfloyd/
#   ./scripts/convert.sh --type photo --album "Vacation" --input ~/photos/vacation/
#   ./scripts/convert.sh --type youtube --url "https://youtube.com/watch?v=..." --playlist "My Videos"
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PYTHON="$PROJECT_DIR/.venv/bin/python"
DEFAULT_OUTPUT_DIR="$PROJECT_DIR/sdcard_test"

# Source .env if present (for TMDB_API_KEY, etc.)
if [[ -f "$PROJECT_DIR/.env" ]]; then
    set -a
    source "$PROJECT_DIR/.env"
    set +a
fi

if [[ ! -x "$PYTHON" ]]; then
    echo "Error: Python venv not found at $PYTHON"
    echo "Set up the venv first: python3.14 -m venv .venv && .venv/bin/pip install -e ."
    exit 1
fi

# Parse our wrapper args, collect passthrough args
INPUT=""
MEDIA_TYPE="tv"
SHOW=""
TITLE=""
COLLECTION=""
ARTIST=""
ALBUM=""
URL=""
PLAYLIST=""
COOKIES_FROM_BROWSER=""
PASSTHROUGH_ARGS=()
OUTPUT_DIR="$DEFAULT_OUTPUT_DIR"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --type)
            MEDIA_TYPE="$2"
            shift 2
            ;;
        --input)
            INPUT="$2"
            shift 2
            ;;
        --show)
            SHOW="$2"
            shift 2
            ;;
        --title)
            TITLE="$2"
            shift 2
            ;;
        --collection)
            COLLECTION="$2"
            shift 2
            ;;
        --artist)
            ARTIST="$2"
            shift 2
            ;;
        --album)
            ALBUM="$2"
            shift 2
            ;;
        --url)
            URL="$2"
            shift 2
            ;;
        --playlist)
            PLAYLIST="$2"
            shift 2
            ;;
        --cookies-from-browser)
            COOKIES_FROM_BROWSER="$2"
            shift 2
            ;;
        --output-dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        *)
            PASSTHROUGH_ARGS+=("$1")
            shift
            ;;
    esac
done

if [[ -z "$INPUT" && "$MEDIA_TYPE" != "youtube" ]]; then
    echo "Error: --input is required (except for youtube type)"
    echo ""
    echo "Usage:"
    echo "  $0 --type tv --show \"Seinfeld\" --input ~/tv/Seinfeld/"
    echo "  $0 --type movie --title \"The Matrix\" --input ~/movies/matrix.mkv"
    echo "  $0 --type music-video --collection \"80s\" --input ~/mv/80s/"
    echo "  $0 --type music --artist \"Pink Floyd\" --input ~/music/pinkfloyd/"
    echo "  $0 --type photo --album \"Vacation\" --input ~/photos/vacation/"
    echo "  $0 --type youtube --url \"https://youtube.com/watch?v=...\" --playlist \"My Videos\""
    exit 1
fi

# Build converter arguments based on type
CONVERTER_ARGS=(--type "$MEDIA_TYPE" --output-dir "$OUTPUT_DIR")

case "$MEDIA_TYPE" in
    tv)
        [[ -z "$SHOW" ]] && { echo "Error: --show required for TV"; exit 1; }
        CONVERTER_ARGS+=(--input-dir "$INPUT" --show "$SHOW")
        echo "Converting TV show: $SHOW"
        ;;
    movie)
        [[ -z "$TITLE" ]] && { echo "Error: --title required for movie"; exit 1; }
        CONVERTER_ARGS+=(--input-file "$INPUT" --title "$TITLE")
        echo "Converting movie: $TITLE"
        ;;
    music-video)
        [[ -z "$COLLECTION" ]] && { echo "Error: --collection required for music-video"; exit 1; }
        CONVERTER_ARGS+=(--input-dir "$INPUT" --collection "$COLLECTION")
        echo "Converting music video collection: $COLLECTION"
        ;;
    music)
        [[ -z "$ARTIST" ]] && { echo "Error: --artist required for music"; exit 1; }
        CONVERTER_ARGS+=(--input-dir "$INPUT" --artist "$ARTIST")
        echo "Converting music: $ARTIST"
        ;;
    photo)
        [[ -z "$ALBUM" ]] && { echo "Error: --album required for photo"; exit 1; }
        CONVERTER_ARGS+=(--input-dir "$INPUT" --album "$ALBUM")
        echo "Converting photo album: $ALBUM"
        ;;
    youtube)
        [[ -z "$URL" ]] && { echo "Error: --url required for youtube"; exit 1; }
        CONVERTER_ARGS+=(--url "$URL")
        [[ -n "$PLAYLIST" ]] && CONVERTER_ARGS+=(--playlist "$PLAYLIST")
        [[ -n "$COOKIES_FROM_BROWSER" ]] && CONVERTER_ARGS+=(--cookies-from-browser "$COOKIES_FROM_BROWSER")
        echo "Converting YouTube: $URL"
        [[ -n "$PLAYLIST" ]] && echo "Playlist name: $PLAYLIST"
        ;;
    *)
        echo "Error: Unknown type '$MEDIA_TYPE'"
        exit 1
        ;;
esac

echo "Input: $INPUT"
echo "Output: $OUTPUT_DIR"
echo ""

# Pass TMDB key if available
if [[ -n "${TMDB_API_KEY:-}" ]]; then
    CONVERTER_ARGS+=(--tmdb-key "$TMDB_API_KEY")
fi

exec "$PYTHON" -m converter \
    "${CONVERTER_ARGS[@]}" \
    "${PASSTHROUGH_ARGS[@]}"
