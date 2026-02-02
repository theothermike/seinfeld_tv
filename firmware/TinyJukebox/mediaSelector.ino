//-------------------------------------------------------------------------------
//  TinyJukebox - Media Type Selector
//
//  Scans the SD card for available media type directories (TV, Movies,
//  MusicVideos, Music, Photos) and presents a menu for the user to choose.
//  Also provides a Settings entry at the end of the list.
//  Vol knob navigates, CH forward selects, power button powers off.
//-------------------------------------------------------------------------------

#include "uiHelpers.h"

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

// Settings is an extra entry beyond the media types
#define SELECTOR_ENTRY_SETTINGS  MEDIA_TYPE_COUNT

// ─── Scan Media Types ───────────────────────────────────────────────────────
// Check which top-level media directories exist and have content.

void scanMediaTypes() {
  appCtx.mediaSelectorCount = 0;

  // Check each media type directory
  const char* mediaDirs[] = { TV_DIR, MOVIES_DIR, MUSIC_VIDEOS_DIR, MUSIC_DIR, PHOTOS_DIR, YOUTUBE_DIR };

  for (int i = 0; i < MEDIA_TYPE_COUNT; i++) {
    appCtx.mediaTypeAvailable[i] = false;

    File32 dir;
    if (!dir.open(mediaDirs[i], O_RDONLY)) {
      continue;
    }

    // Check if directory has at least one subdirectory or relevant file
    File32 entry;
    bool hasContent = false;
    while (entry.openNext(&dir, O_RDONLY)) {
      if (entry.isDir()) {
        hasContent = true;
        entry.close();
        break;
      }
      entry.close();
    }
    dir.close();

    if (hasContent) {
      appCtx.mediaTypeAvailable[i] = true;
      appCtx.mediaSelectorCount++;
      dbgPrint("Media type available: " + String(mediaTypeLabels[i]));
    }
  }

  // Settings is always available (adds 1 to the count for display)
  dbgPrint("Total media types: " + String(appCtx.mediaSelectorCount));
}

// ─── Draw Media Selector ────────────────────────────────────────────────────
//
// Layout (210x135):
//   Row 0-14:   "TinyJukebox" header (yellow, centered)
//   Row 16:     Divider
//   Row 20+:    List of available media types (highlighted current)
//   Row 122:    Hint text

void drawMediaSelector() {
  clearFrameBuf();

  // Header
  const char* header = "TinyJukebox";
  int headerW = textWidth(header);
  drawText(header, (VIDEO_W - headerW) / 2, 4, COL_YELLOW);

  // Divider
  drawHLine(10, 18, VIDEO_W - 20, COL_GRAY_DK);

  // Build list of visible entries: available media types + settings
  // We track a mapping from display index -> entry type
  int displayIndex = 0;
  int selectorY = 24;
  int rowHeight = 16;

  // Count total entries for navigation bounds
  int totalEntries = appCtx.mediaSelectorCount + 1;  // +1 for Settings

  // Clamp selector index
  if (appCtx.mediaSelectorIndex < 0) appCtx.mediaSelectorIndex = 0;
  if (appCtx.mediaSelectorIndex >= totalEntries) appCtx.mediaSelectorIndex = totalEntries - 1;

  // Calculate scroll offset so selected item is visible
  // We can show about 6 items on screen
  int maxVisible = 6;
  int scrollOffset = 0;
  if (appCtx.mediaSelectorIndex >= maxVisible) {
    scrollOffset = appCtx.mediaSelectorIndex - maxVisible + 1;
  }

  // Draw each available media type
  displayIndex = 0;
  for (int i = 0; i < MEDIA_TYPE_COUNT; i++) {
    if (!appCtx.mediaTypeAvailable[i]) continue;

    int visIndex = displayIndex - scrollOffset;
    if (visIndex >= 0 && visIndex < maxVisible) {
      int y = selectorY + visIndex * rowHeight;
      bool selected = (displayIndex == appCtx.mediaSelectorIndex);

      if (selected) {
        // Highlight bar
        fillRect(4, y - 1, VIDEO_W - 8, rowHeight, COL_GRAY_DK);
      }

      // Draw label
      const char* label = mediaTypeLabels[i];
      int labelW = textWidth(label);
      int labelX = (VIDEO_W - labelW) / 2;
      drawText(label, labelX, y + 1, selected ? COL_YELLOW : COL_WHITE);
    }

    displayIndex++;
  }

  // Settings entry
  {
    int visIndex = displayIndex - scrollOffset;
    if (visIndex >= 0 && visIndex < maxVisible) {
      int y = selectorY + visIndex * rowHeight;
      bool selected = (displayIndex == appCtx.mediaSelectorIndex);

      if (selected) {
        fillRect(4, y - 1, VIDEO_W - 8, rowHeight, COL_GRAY_DK);
      }

      const char* label = "Settings";
      int labelW = textWidth(label);
      int labelX = (VIDEO_W - labelW) / 2;
      drawText(label, labelX, y + 1, selected ? COL_YELLOW : COL_GRAY_LT);
    }
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
      drawMediaSelector();
    }
  }
  if (raw.encoder2CCW || raw.irVolDn) {
    raw.encoder2CCW = false;
    raw.irVolDn = false;
    if (appCtx.mediaSelectorIndex > 0) {
      appCtx.mediaSelectorIndex--;
      drawMediaSelector();
    }
  }

  // Select with channel forward
  if (raw.encoderCW || raw.irChannelUp) {
    raw.encoderCW = false;
    raw.irChannelUp = false;

    // Map display index back to media type or settings
    int displayIndex = 0;
    for (int i = 0; i < MEDIA_TYPE_COUNT; i++) {
      if (!appCtx.mediaTypeAvailable[i]) continue;
      if (displayIndex == appCtx.mediaSelectorIndex) {
        // Selected a media type
        appCtx.currentMediaType = (MediaType)i;
        AppState target = getFirstStateForMediaType(appCtx.currentMediaType);
        appCtx.nextState = target;
        appCtx.currentState = STATE_TRANSITION;
        appCtx.transitionStart = millis();
        appCtx.transitionDurationMS = TRANSITION_STATIC_MS;
        dbgPrint("Selected media type: " + String(mediaTypeLabels[i]));
        return;
      }
      displayIndex++;
    }

    // If we got here, user selected Settings
    if (displayIndex == appCtx.mediaSelectorIndex) {
      appCtx.nextState = STATE_SETTINGS;
      appCtx.currentState = STATE_TRANSITION;
      appCtx.transitionStart = millis();
      appCtx.transitionDurationMS = TRANSITION_STATIC_MS;
      dbgPrint("Selected Settings");
      return;
    }
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
