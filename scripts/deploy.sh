#!/usr/bin/env bash
#
# deploy.sh - Deploy converted files to TinyTV 2 via USB MSC
#
# Usage:
#   ./scripts/deploy.sh                              # Convert + deploy
#   ./scripts/deploy.sh --skip-convert                # Deploy existing files only
#   ./scripts/deploy.sh --show-only ShowName          # Deploy single show + settings
#   ./scripts/deploy.sh --show "Seinfeld" --input ~/tv/Seinfeld/  # Convert then deploy
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DEVICE_MOUNT="/media/mike/disk"
SOURCE_DIR="$PROJECT_DIR/sdcard_test"

SKIP_CONVERT=false
SHOW_ONLY=""
CONVERT_ARGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --skip-convert)
            SKIP_CONVERT=true
            shift
            ;;
        --show-only)
            SHOW_ONLY="$2"
            shift 2
            ;;
        *)
            CONVERT_ARGS+=("$1")
            shift
            ;;
    esac
done

# Step 1: Convert (unless skipped)
if [[ "$SKIP_CONVERT" == false && ${#CONVERT_ARGS[@]} -gt 0 ]]; then
    echo "=== Running conversion ==="
    "$SCRIPT_DIR/convert.sh" "${CONVERT_ARGS[@]}"
    echo ""
fi

# Step 2: Check device is mounted
if ! mountpoint -q "$DEVICE_MOUNT" 2>/dev/null; then
    echo "Error: TinyTV not found at $DEVICE_MOUNT"
    echo ""
    echo "To put the TinyTV in USB Mass Storage mode:"
    echo "  1. Power on the TinyTV"
    echo "  2. Use the IR remote or button to enter Settings"
    echo "  3. Select 'USB Mode' -> 'Mass Storage'"
    echo "  4. Connect USB cable to computer"
    echo "  5. Wait for $DEVICE_MOUNT to appear"
    echo ""
    echo "Then re-run: $0 --skip-convert"
    exit 1
fi

# Step 3: Sync files
if [[ -n "$SHOW_ONLY" ]]; then
    echo "=== Deploying show: $SHOW_ONLY ==="
    # Check under TV/ directory (new layout)
    if [[ -d "$SOURCE_DIR/TV/$SHOW_ONLY" ]]; then
        rsync -av --progress "$SOURCE_DIR/TV/$SHOW_ONLY/" "$DEVICE_MOUNT/TV/$SHOW_ONLY/"
    elif [[ -d "$SOURCE_DIR/$SHOW_ONLY" ]]; then
        rsync -av --progress "$SOURCE_DIR/$SHOW_ONLY/" "$DEVICE_MOUNT/$SHOW_ONLY/"
    else
        echo "Error: Show directory not found: $SOURCE_DIR/TV/$SHOW_ONLY"
        exit 1
    fi
    # Also sync settings and boot.avi if they exist
    [[ -f "$SOURCE_DIR/settings.txt" ]] && cp -v "$SOURCE_DIR/settings.txt" "$DEVICE_MOUNT/"
    [[ -f "$SOURCE_DIR/boot.avi" ]] && rsync -av --progress "$SOURCE_DIR/boot.avi" "$DEVICE_MOUNT/"
else
    echo "=== Deploying all files to TinyJukebox ==="
    rsync -av --progress --delete "$SOURCE_DIR/" "$DEVICE_MOUNT/"
fi

# Step 4: Sync filesystem
echo ""
echo "Syncing filesystem..."
sync

echo ""
echo "Deploy complete! Safe to disconnect the TinyTV."
