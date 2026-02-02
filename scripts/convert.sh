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
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PYTHON="$PROJECT_DIR/.venv/bin/python"
DEFAULT_OUTPUT_DIR="$PROJECT_DIR/sdcard_test"

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

if [[ -z "$INPUT" ]]; then
    echo "Error: --input is required"
    echo ""
    echo "Usage:"
    echo "  $0 --type tv --show \"Seinfeld\" --input ~/tv/Seinfeld/"
    echo "  $0 --type movie --title \"The Matrix\" --input ~/movies/matrix.mkv"
    echo "  $0 --type music-video --collection \"80s\" --input ~/mv/80s/"
    echo "  $0 --type music --artist \"Pink Floyd\" --input ~/music/pinkfloyd/"
    echo "  $0 --type photo --album \"Vacation\" --input ~/photos/vacation/"
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
    *)
        echo "Error: Unknown type '$MEDIA_TYPE'"
        exit 1
        ;;
esac

echo "Input: $INPUT"
echo "Output: $OUTPUT_DIR"
echo ""

exec "$PYTHON" -m converter \
    "${CONVERTER_ARGS[@]}" \
    "${PASSTHROUGH_ARGS[@]}"
