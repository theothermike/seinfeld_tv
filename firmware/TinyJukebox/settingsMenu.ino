//-------------------------------------------------------------------------------
//  TinyJukebox - Settings Menu
//
//  Provides user-configurable settings:
//    - Slideshow interval (3/5/10/15/30 seconds)
//  Vol knob navigates between settings, CH forward cycles the value,
//  CH back returns to media selector.
//-------------------------------------------------------------------------------

#include "uiHelpers.h"

extern uint16_t frameBuf[];
extern GraphicsBuffer2 screenBuffer;

// ─── Settings Definitions ───────────────────────────────────────────────────

// Slideshow interval options (in seconds)
static const uint8_t slideshowOptions[] = { 3, 5, 10, 15, 30 };
#define SLIDESHOW_OPTION_COUNT 5

// Settings menu items
#define SETTING_SLIDESHOW_INTERVAL 0
#define SETTINGS_ITEM_COUNT        1

// Local state for the settings menu
static int settingsNavIndex = 0;

// ─── Helper: find current slideshow option index ────────────────────────────

static int findSlideshowOptionIndex() {
  for (int i = 0; i < SLIDESHOW_OPTION_COUNT; i++) {
    if (slideshowOptions[i] == appCtx.slideshowIntervalSec) {
      return i;
    }
  }
  return 1;  // Default to 5s if not found
}

// ─── Draw Settings Menu ─────────────────────────────────────────────────────
//
// Layout (210x135):
//   Row 0-14:   "Settings" header (white, centered)
//   Row 16:     Divider
//   Row 30+:    Setting name + value rows
//   Row 122:    Hint text

void drawSettingsMenu() {
  clearFrameBuf();

  // Header
  const char* header = "Settings";
  int headerW = textWidth(header);
  drawText(header, (VIDEO_W - headerW) / 2, 4, COL_WHITE);

  // Divider
  drawHLine(10, 18, VIDEO_W - 20, COL_GRAY_DK);

  // Clamp nav index
  if (settingsNavIndex < 0) settingsNavIndex = 0;
  if (settingsNavIndex >= SETTINGS_ITEM_COUNT) settingsNavIndex = SETTINGS_ITEM_COUNT - 1;

  int rowY = 30;
  int rowHeight = 20;

  // ─── Slideshow Interval ─────────────────────────────────────────────
  {
    bool selected = (settingsNavIndex == SETTING_SLIDESHOW_INTERVAL);
    int y = rowY + SETTING_SLIDESHOW_INTERVAL * rowHeight;

    if (selected) {
      fillRect(4, y - 2, VIDEO_W - 8, rowHeight, COL_GRAY_DK);
    }

    // Label
    const char* label = "Slideshow:";
    drawText(label, 8, y + 1, selected ? COL_WHITE : COL_GRAY_LT);

    // Value
    char valStr[16];
    snprintf(valStr, sizeof(valStr), "%ds", appCtx.slideshowIntervalSec);
    int valW = textWidth(valStr);
    drawText(valStr, VIDEO_W - valW - 8, y + 1, selected ? COL_YELLOW : COL_GRAY_MED);

    // Show arrows on selected item
    if (selected) {
      drawText("<", VIDEO_W - valW - 20, y + 1, COL_GRAY_MED);
      drawText(">", VIDEO_W - 6, y + 1, COL_GRAY_MED);
    }
  }

  // ─── Divider below settings ─────────────────────────────────────────
  int bottomDivY = rowY + SETTINGS_ITEM_COUNT * rowHeight + 10;
  drawHLine(10, bottomDivY, VIDEO_W - 20, COL_GRAY_DK);

  // ─── Current values summary ─────────────────────────────────────────
  int summaryY = bottomDivY + 8;
  char summaryStr[48];
  snprintf(summaryStr, sizeof(summaryStr), "Slideshow: %ds interval",
           appCtx.slideshowIntervalSec);
  int sumW = textWidth(summaryStr);
  drawText(summaryStr, (VIDEO_W - sumW) / 2, summaryY, COL_GRAY_MED);

  // Hint text
  const char* hint = "VOL:nav  CH:change  <:back";
  int hintW = textWidth(hint);
  drawText(hint, (VIDEO_W - hintW) / 2, 122, COL_GRAY_DK);

  // Push to display
  waitForScreenDMA();
  setScreenAddressWindow(VIDEO_X, VIDEO_Y, VIDEO_W, VIDEO_H);
  writeToScreenDMA(frameBuf, VIDEO_W * VIDEO_H);
}

// ─── Input Handling ─────────────────────────────────────────────────────────

void handleSettingsInput() {
  RawInputFlags& raw = appCtx.rawInput;

  // Navigate settings with volume knob
  if (raw.encoder2CW || raw.irVolUp) {
    raw.encoder2CW = false;
    raw.irVolUp = false;
    if (settingsNavIndex < SETTINGS_ITEM_COUNT - 1) {
      settingsNavIndex++;
      drawSettingsMenu();
    }
  }
  if (raw.encoder2CCW || raw.irVolDn) {
    raw.encoder2CCW = false;
    raw.irVolDn = false;
    if (settingsNavIndex > 0) {
      settingsNavIndex--;
      drawSettingsMenu();
    }
  }

  // Change value with channel forward
  if (raw.encoderCW || raw.irChannelUp) {
    raw.encoderCW = false;
    raw.irChannelUp = false;

    switch (settingsNavIndex) {
      case SETTING_SLIDESHOW_INTERVAL: {
        int optIdx = findSlideshowOptionIndex();
        optIdx = (optIdx + 1) % SLIDESHOW_OPTION_COUNT;
        appCtx.slideshowIntervalSec = slideshowOptions[optIdx];
        dbgPrint("Slideshow interval: " + String(appCtx.slideshowIntervalSec) + "s");
        appCtx.settingsDirty = true;
        appCtx.settingsLastChange = millis();
        drawSettingsMenu();
        break;
      }
    }
  }

  // Back to media selector
  if (raw.encoderCCW || raw.irChannelDn) {
    raw.encoderCCW = false;
    raw.irChannelDn = false;
    appCtx.nextState = STATE_MEDIA_SELECTOR;
    appCtx.currentState = STATE_TRANSITION;
    appCtx.transitionStart = millis();
    appCtx.transitionDurationMS = TRANSITION_STATIC_MS;
  }

  // Mute
  if (raw.irMute) {
    raw.irMute = false;
    setMute(!isMute());
  }
}
