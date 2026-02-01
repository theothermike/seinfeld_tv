#!/usr/bin/env bash
#
# convert.sh - Wrapper around the converter package for TinyTV 2
#
# Usage:
#   ./scripts/convert.sh --show "Seinfeld" --input ~/tv/Seinfeld/ [options]
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
INPUT_DIR=""
SHOW=""
PASSTHROUGH_ARGS=()
OUTPUT_DIR="$DEFAULT_OUTPUT_DIR"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --input)
            INPUT_DIR="$2"
            shift 2
            ;;
        --show)
            SHOW="$2"
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

if [[ -z "$SHOW" ]]; then
    echo "Error: --show is required"
    echo "Usage: $0 --show \"Seinfeld\" --input ~/tv/Seinfeld/ [--season N] [--episodes 1-5] [--verbose]"
    exit 1
fi

if [[ -z "$INPUT_DIR" ]]; then
    echo "Error: --input is required"
    echo "Usage: $0 --show \"Seinfeld\" --input ~/tv/Seinfeld/ [--season N] [--episodes 1-5] [--verbose]"
    exit 1
fi

echo "Converting show: $SHOW"
echo "Input: $INPUT_DIR"
echo "Output: $OUTPUT_DIR"
echo ""

exec "$PYTHON" -m converter \
    --show "$SHOW" \
    --input-dir "$INPUT_DIR" \
    --output-dir "$OUTPUT_DIR" \
    "${PASSTHROUGH_ARGS[@]}"
