//-------------------------------------------------------------------------------
//  TinyJukebox - Media Type Selector (Icon Browser)
//
//  Scans the SD card for available media type directories (TV, Movies,
//  MusicVideos, Music, Photos, YouTube) and presents a single-item-at-a-time
//  browser with centered icons and media type names.
//  Vol knob navigates left/right, CH forward selects, power button powers off.
//-------------------------------------------------------------------------------

#include "uiHelpers.h"
#include "mediaIcons.h"

extern uint16_t frameBuf[];
extern GraphicsBuffer2 screenBuffer;

// ─── Media Type Names (display labels) ──────────────────────────────────────

static const char* mediaTypeLabels[] = {
  "TV Shows",
  "Movies",
  "Music Videos",
  "Music",
  "Photos",
  "YouTube"
};

// Singular/plural count labels for each media type
static const char* mediaTypeUnits[][2] = {
  { "show",       "shows" },
  { "movie",      "movies" },
  { "collection", "collections" },
  { "artist",     "artists" },
  { "album",      "albums" },
  { "playlist",   "playlists" }
};

// Settings is an extra entry beyond the media types
#define SELECTOR_ENTRY_SETTINGS  MEDIA_TYPE_COUNT

// ─── Scan Media Types ───────────────────────────────────────────────────────
// Check which top-level media directories exist and have content.
// Also counts subdirectories for each available type.

void scanMediaTypes() {
  appCtx.mediaSelectorCount = 0;

  const char* mediaDirs[] = { TV_DIR, MOVIES_DIR, MUSIC_VIDEOS_DIR, MUSIC_DIR, PHOTOS_DIR, YOUTUBE_DIR };

  for (int i = 0; i < MEDIA_TYPE_COUNT; i++) {
    appCtx.mediaTypeAvailable[i] = false;
    appCtx.mediaTypeItemCount[i] = 0;

    File32 dir;
    if (!dir.open(mediaDirs[i], O_RDONLY)) {
      continue;
    }

    // Count all subdirectories
    File32 entry;
    int count = 0;
    while (entry.openNext(&dir, O_RDONLY)) {
      if (entry.isDir()) {
        count++;
      }
      entry.close();
    }
    dir.close();

    if (count > 0) {
      appCtx.mediaTypeAvailable[i] = true;
      appCtx.mediaTypeItemCount[i] = count;
      appCtx.mediaSelectorCount++;
      dbgPrint("Media type available: " + String(mediaTypeLabels[i]) + " (" + String(count) + ")");
    }
  }

  // Settings is always available (adds 1 to the count for display)
  dbgPrint("Total media types: " + String(appCtx.mediaSelectorCount));
}

// ─── Helper: Map display index to MediaType or Settings ─────────────────────
// Returns the MediaType enum value for the given display index, or
// MEDIA_TYPE_COUNT if it maps to Settings.

static int displayIndexToMediaType(int displayIdx) {
  int idx = 0;
  for (int i = 0; i < MEDIA_TYPE_COUNT; i++) {
    if (!appCtx.mediaTypeAvailable[i]) continue;
    if (idx == displayIdx) return i;
    idx++;
  }
  return MEDIA_TYPE_COUNT;  // Settings
}

// ─── Helper: Format item count string ───────────────────────────────────────

static void getMediaTypeInfoString(char* buf, int bufLen, int mediaType) {
  if (mediaType >= MEDIA_TYPE_COUNT) {
    // Settings has no count
    snprintf(buf, bufLen, "");
    return;
  }
  int count = appCtx.mediaTypeItemCount[mediaType];
  const char* unit = (count == 1) ? mediaTypeUnits[mediaType][0] : mediaTypeUnits[mediaType][1];
  snprintf(buf, bufLen, "%d %s", count, unit);
}

// ─── Draw Media Selector ────────────────────────────────────────────────────
//
// Layout (210x135):
//   Row 0-14:   "TinyJukebox" header (yellow, centered)
//   Row 18:     Divider line
//   Row 20-86:  48x48 icon centered in this area
//               "<" arrow at x=4, ">" arrow at x=198
//   Row 92:     Media type name (yellow, centered)
//   Row 106:    Item count e.g. "3 shows" (gray)
//   Row 122:    "VOL:nav  CH:select" hint (dark gray)

void drawMediaSelector() {
  clearFrameBuf();

  // Count total entries for navigation bounds
  int totalEntries = appCtx.mediaSelectorCount + 1;  // +1 for Settings

  // Clamp selector index
  if (appCtx.mediaSelectorIndex < 0) appCtx.mediaSelectorIndex = 0;
  if (appCtx.mediaSelectorIndex >= totalEntries) appCtx.mediaSelectorIndex = totalEntries - 1;

  // Header
  const char* header = "TinyJukebox";
  int headerW = textWidth(header);
  drawText(header, (VIDEO_W - headerW) / 2, 4, COL_YELLOW);

  // Divider
  drawHLine(10, 18, VIDEO_W - 20, COL_GRAY_DK);

  // Determine which media type (or settings) is selected
  int mediaType = displayIndexToMediaType(appCtx.mediaSelectorIndex);

  // Draw icon centered in the thumbnail area (rows 20-86)
  int iconAreaY = 20;
  int iconAreaH = 66;
  int iconX = (VIDEO_W - ICON_W) / 2;
  int iconY = iconAreaY + (iconAreaH - ICON_H) / 2;

  // Select icon from lookup table (mediaType index, or MEDIA_TYPE_COUNT for settings)
  int iconIdx = (mediaType < MEDIA_TYPE_COUNT) ? mediaType : MEDIA_TYPE_COUNT;
  const uint16_t* iconPtr = (const uint16_t*)pgm_read_ptr(&mediaIcons[iconIdx]);
  drawIcon(iconPtr, iconX, iconY, ICON_W, ICON_H);

  // Navigation arrows
  int arrowY = iconAreaY + iconAreaH / 2 - 6;
  if (appCtx.mediaSelectorIndex > 0) {
    drawText("<", 4, arrowY, COL_GRAY_MED);
  }
  if (appCtx.mediaSelectorIndex < totalEntries - 1) {
    drawText(">", VIDEO_W - 12, arrowY, COL_GRAY_MED);
  }

  // Media type name (yellow, centered)
  const char* label;
  if (mediaType < MEDIA_TYPE_COUNT) {
    label = mediaTypeLabels[mediaType];
  } else {
    label = "Settings";
  }
  int labelW = textWidth(label);
  drawText(label, (VIDEO_W - labelW) / 2, 92, COL_YELLOW);

  // Item count info line (gray, centered)
  char infoStr[32];
  getMediaTypeInfoString(infoStr, sizeof(infoStr), mediaType);
  if (infoStr[0] != '\0') {
    int infoW = textWidth(infoStr);
    drawText(infoStr, (VIDEO_W - infoW) / 2, 106, COL_GRAY_MED);
  }

  // Hint text
  const char* hint = "VOL:nav  CH:select";
  int hintW = textWidth(hint);
  drawText(hint, (VIDEO_W - hintW) / 2, 122, COL_GRAY_DK);

  // Push to display
  waitForScreenDMA();
  setScreenAddressWindow(VIDEO_X, VIDEO_Y, VIDEO_W, VIDEO_H);
  writeToScreenDMA(frameBuf, VIDEO_W * VIDEO_H);
}

// ─── Input Handling ─────────────────────────────────────────────────────────

void handleMediaSelectorInput() {
  RawInputFlags& raw = appCtx.rawInput;

  int totalEntries = appCtx.mediaSelectorCount + 1;  // +1 for Settings

  // Navigate with volume knob
  if (raw.encoder2CW || raw.irVolUp) {
    raw.encoder2CW = false;
    raw.irVolUp = false;
    if (appCtx.mediaSelectorIndex < totalEntries - 1) {
      appCtx.mediaSelectorIndex++;
      resetScrollState(&appCtx.scrollState);
      drawMediaSelector();
    }
  }
  if (raw.encoder2CCW || raw.irVolDn) {
    raw.encoder2CCW = false;
    raw.irVolDn = false;
    if (appCtx.mediaSelectorIndex > 0) {
      appCtx.mediaSelectorIndex--;
      resetScrollState(&appCtx.scrollState);
      drawMediaSelector();
    }
  }

  // Select with channel forward
  if (raw.encoderCW || raw.irChannelUp) {
    raw.encoderCW = false;
    raw.irChannelUp = false;

    int mediaType = displayIndexToMediaType(appCtx.mediaSelectorIndex);

    if (mediaType < MEDIA_TYPE_COUNT) {
      // Selected a media type
      appCtx.currentMediaType = (MediaType)mediaType;
      AppState target = getFirstStateForMediaType(appCtx.currentMediaType);
      appCtx.nextState = target;
      appCtx.currentState = STATE_TRANSITION;
      appCtx.transitionStart = millis();
      appCtx.transitionDurationMS = TRANSITION_STATIC_MS;
      dbgPrint("Selected media type: " + String(mediaTypeLabels[mediaType]));
    } else {
      // Settings
      appCtx.nextState = STATE_SETTINGS;
      appCtx.currentState = STATE_TRANSITION;
      appCtx.transitionStart = millis();
      appCtx.transitionDurationMS = TRANSITION_STATIC_MS;
      dbgPrint("Selected Settings");
    }
    return;
  }

  // Consume channel back (no-op at media selector level)
  if (raw.encoderCCW || raw.irChannelDn) {
    raw.encoderCCW = false;
    raw.irChannelDn = false;
  }

  // Mute
  if (raw.irMute) {
    raw.irMute = false;
    setMute(!isMute());
  }
}
