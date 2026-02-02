//-------------------------------------------------------------------------------
//  TinyJukebox - Show Browser UI
//
//  Show browser, season browser, and episode browser screens rendered into
//  frameBuf using GraphicsBuffer2 text rendering from upstream display.ino.
//-------------------------------------------------------------------------------

// Include shared UI helpers (colors, drawing primitives)
#include "uiHelpers.h"

extern uint16_t frameBuf[];
extern GraphicsBuffer2 screenBuffer;

// ─── Show Browser ───────────────────────────────────────────────────────────
//
// Layout (210x135):
//   Row 0-14:   "TV Shows" header (white, centered)
//   Row 16:     Divider
//   Row 20-86:  Thumbnail (108x67) centered
//   Row 88:     Nav arrows + show name (yellow, centered)
//   Row 104:    Info line (year + season count, gray)
//   Row 120:    Hint text

void drawShowBrowser() {
  clearFrameBuf();

  // Header
  const char* header = "TV Shows";
  int headerW = textWidth(header);
  drawText(header, (VIDEO_W - headerW) / 2, 4, COL_WHITE);

  // Divider
  drawHLine(10, 16, VIDEO_W - 20, COL_GRAY_DK);

  // Thumbnail (centered)
  int thumbX = (VIDEO_W - THUMB_W) / 2;
  int thumbY = 20;
  char thumbPath[64];
  getShowThumbPath(thumbPath, sizeof(thumbPath), appCtx.showNavIndex);
  if (!displayThumbnail(thumbPath, thumbX, thumbY)) {
    fillRect(thumbX, thumbY, THUMB_W, THUMB_H, COL_GRAY_DK);
  }

  // Navigation arrows
  if (appCtx.showNavIndex > 0) {
    drawText("<", 4, thumbY + THUMB_H / 2 - 6, COL_GRAY_MED);
  }
  if (appCtx.showNavIndex < appCtx.availableShowCount - 1) {
    drawText(">", VIDEO_W - 12, thumbY + THUMB_H / 2 - 6, COL_GRAY_MED);
  }

  // Show name (yellow, centered)
  if (appCtx.metadataLoaded) {
    const char* showName = appCtx.meta1.showMeta.name;
    int nameW = textWidth(showName);
    int nameX = (VIDEO_W - nameW) / 2;
    if (nameX < 2) nameX = 2;
    drawText(showName, nameX, 92, COL_YELLOW);

    // Info line: year + season count
    char infoStr[48];
    snprintf(infoStr, sizeof(infoStr), "%s - %d seasons",
             appCtx.meta1.showMeta.year, appCtx.meta1.showMeta.seasonCount);
    int infoW = textWidth(infoStr);
    drawText(infoStr, (VIDEO_W - infoW) / 2, 106, COL_GRAY_MED);
  } else {
    // Show directory name as fallback
    const char* dirName = appCtx.availableShows[appCtx.showNavIndex];
    int nameW = textWidth(dirName);
    drawText(dirName, (VIDEO_W - nameW) / 2, 92, COL_YELLOW);
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

// ─── Season Browser ──────────────────────────────────────────────────────────
//
// Layout (210x135):
//   Row 0-20:   Show name (centered, white)
//   Row 40-75:  "< Season N >" (large, yellow, centered)
//   Row 78-90:  "of M" (small, gray, centered)
//   Row 110-125: "CH to select" (tiny, gray, centered)

void drawSeasonBrowser() {
  clearFrameBuf();

  // Show name - centered at top
  const char* showName = appCtx.meta1.showMeta.name;
  int nameW = textWidth(showName);
  int nameX = (VIDEO_W - nameW) / 2;
  if (nameX < 2) nameX = 2;
  drawText(showName, nameX, 8, COL_WHITE);

  // Divider line
  drawHLine(10, 24, VIDEO_W - 20, COL_GRAY_DK);

  // Season number - large centered
  char seasonStr[32];
  snprintf(seasonStr, sizeof(seasonStr), "Season %d", appCtx.currentSeason);
  int seasonW = textWidth(seasonStr);
  int seasonX = (VIDEO_W - seasonW) / 2;

  // Draw navigation arrows if applicable
  if (appCtx.seasonNavIndex > 0) {
    drawText("<", seasonX - 16, 48, COL_GRAY_MED);
  }
  drawText(seasonStr, seasonX, 48, COL_YELLOW);
  if (appCtx.seasonNavIndex < appCtx.availableSeasonCount - 1) {
    drawText(">", seasonX + seasonW + 8, 48, COL_GRAY_MED);
  }

  // "of N" subtitle
  char ofStr[16];
  snprintf(ofStr, sizeof(ofStr), "of %d", appCtx.availableSeasonCount);
  int ofW = textWidth(ofStr);
  drawText(ofStr, (VIDEO_W - ofW) / 2, 68, COL_GRAY_MED);

  // Episode count for this season
  if (appCtx.seasonMetaLoaded) {
    char epStr[32];
    snprintf(epStr, sizeof(epStr), "%d episodes", appCtx.meta2.seasonMeta.episodeCount);
    int epW = textWidth(epStr);
    drawText(epStr, (VIDEO_W - epW) / 2, 86, COL_GRAY_LT);
  }

  // Hint text
  const char* hint = "VOL:nav  CH:select";
  int hintW = textWidth(hint);
  drawText(hint, (VIDEO_W - hintW) / 2, 120, COL_GRAY_DK);

  // Push to display
  waitForScreenDMA();
  setScreenAddressWindow(VIDEO_X, VIDEO_Y, VIDEO_W, VIDEO_H);
  writeToScreenDMA(frameBuf, VIDEO_W * VIDEO_H);
}

// ─── Episode Browser ─────────────────────────────────────────────────────────
//
// Layout (210x135):
//   Row 0-10:   "Season N" (tiny gray, left)
//   Row 12:     Divider
//   Row 14-80:  Thumbnail (108x67) left + Episode info right
//   Row 82:     Divider
//   Row 84-100: Hints
//   Row 102-130: Description (truncated)

void drawEpisodeBrowser() {
  clearFrameBuf();

  // Season label - top left
  char seasonLabel[24];
  snprintf(seasonLabel, sizeof(seasonLabel), "Season %d", appCtx.currentSeason);
  drawText(seasonLabel, 4, 4, COL_GRAY_MED);

  // Divider
  drawHLine(2, 16, VIDEO_W - 4, COL_GRAY_DK);

  // Thumbnail area (left side: x=2, y=19, 108x67)
  char thumbPath[64];
  getEpisodeThumbPath(thumbPath, sizeof(thumbPath),
                      appCtx.currentSeason, appCtx.currentEpisode);
  if (!displayThumbnail(thumbPath, 2, 19)) {
    // Placeholder gray box if thumbnail missing
    fillRect(2, 19, THUMB_W, THUMB_H, COL_GRAY_DK);
  }

  // Episode info (right side, x=114)
  int infoX = 114;

  // Episode number
  char epNumStr[16];
  if (appCtx.seasonMetaLoaded) {
    snprintf(epNumStr, sizeof(epNumStr), "Ep %d/%d",
             appCtx.currentEpisode, appCtx.meta2.seasonMeta.episodeCount);
  } else {
    snprintf(epNumStr, sizeof(epNumStr), "Ep %d", appCtx.currentEpisode);
  }
  drawText(epNumStr, infoX, 22, COL_WHITE);

  // Episode title (may need truncation for display)
  if (appCtx.episodeMetaLoaded) {
    // Title - possibly two lines
    char titleBuf[49];
    strncpy(titleBuf, appCtx.meta3.episodeMeta.title, 48);
    titleBuf[48] = '\0';

    // Truncate to fit ~12 chars per line in the info area
    char line1[13], line2[13];
    memset(line1, 0, sizeof(line1));
    memset(line2, 0, sizeof(line2));

    if (strlen(titleBuf) <= 12) {
      strncpy(line1, titleBuf, 12);
    } else {
      // Find word break near char 12
      int breakAt = 12;
      while (breakAt > 0 && titleBuf[breakAt] != ' ') breakAt--;
      if (breakAt == 0) breakAt = 12;
      strncpy(line1, titleBuf, breakAt);
      strncpy(line2, titleBuf + breakAt + (titleBuf[breakAt] == ' ' ? 1 : 0), 12);
    }

    drawText(line1, infoX, 38, COL_YELLOW);
    if (line2[0]) {
      drawText(line2, infoX, 52, COL_YELLOW);
    }

    // Air date
    drawText(appCtx.meta3.episodeMeta.airDate, infoX, 68, COL_GRAY_LT);
  }

  // Lower divider
  drawHLine(2, 88, VIDEO_W - 4, COL_GRAY_DK);

  // Hint text
  drawText("CH:play  VOL:nav", 4, 93, COL_GRAY_DK);

  // Description
  if (appCtx.episodeMetaLoaded && appCtx.meta3.episodeMeta.description[0]) {
    char descBuf[57];
    strncpy(descBuf, appCtx.meta3.episodeMeta.description, 56);
    descBuf[56] = '\0';

    // Split into two lines if needed (~26 chars per line)
    char dLine1[27], dLine2[27];
    memset(dLine1, 0, sizeof(dLine1));
    memset(dLine2, 0, sizeof(dLine2));

    if (strlen(descBuf) <= 26) {
      strncpy(dLine1, descBuf, 26);
    } else {
      int breakAt = 26;
      while (breakAt > 0 && descBuf[breakAt] != ' ') breakAt--;
      if (breakAt == 0) breakAt = 26;
      strncpy(dLine1, descBuf, breakAt);
      strncpy(dLine2, descBuf + breakAt + (descBuf[breakAt] == ' ' ? 1 : 0), 26);
    }

    drawText(dLine1, 4, 108, COL_GRAY_LT);
    if (dLine2[0]) {
      drawText(dLine2, 4, 122, COL_GRAY_LT);
    }
  }

  // Push to display
  waitForScreenDMA();
  setScreenAddressWindow(VIDEO_X, VIDEO_Y, VIDEO_W, VIDEO_H);
  writeToScreenDMA(frameBuf, VIDEO_W * VIDEO_H);
}

// ─── Input Handling for Browsers ─────────────────────────────────────────────

// Process input in SHOW_BROWSER state
// Volume knob: navigate shows
// Channel forward (encoderCW): select show -> enter season browser
// IR: vol=navigate, ch-up=select
void handleShowBrowserInput() {
  RawInputFlags& raw = appCtx.rawInput;

  // Navigate shows with volume knob
  if (raw.encoder2CW || raw.irVolUp) {
    raw.encoder2CW = false;
    raw.irVolUp = false;
    if (appCtx.showNavIndex < appCtx.availableShowCount - 1) {
      appCtx.showNavIndex++;
      strncpy(appCtx.currentShowDir, appCtx.availableShows[appCtx.showNavIndex], SHOW_DIR_LEN - 1);
      appCtx.currentShowDir[SHOW_DIR_LEN - 1] = '\0';
      appCtx.metadataLoaded = false;
      loadShowMetadata();
      drawShowBrowser();
    }
  }
  if (raw.encoder2CCW || raw.irVolDn) {
    raw.encoder2CCW = false;
    raw.irVolDn = false;
    if (appCtx.showNavIndex > 0) {
      appCtx.showNavIndex--;
      strncpy(appCtx.currentShowDir, appCtx.availableShows[appCtx.showNavIndex], SHOW_DIR_LEN - 1);
      appCtx.currentShowDir[SHOW_DIR_LEN - 1] = '\0';
      appCtx.metadataLoaded = false;
      loadShowMetadata();
      drawShowBrowser();
    }
  }

  // Select show -> scan seasons -> enter season browser
  if (raw.encoderCW || raw.irChannelUp) {
    raw.encoderCW = false;
    raw.irChannelUp = false;
    // Scan seasons for selected show
    scanAvailableSeasons();
    if (appCtx.availableSeasonCount > 0) {
      appCtx.currentSeason = appCtx.availableSeasons[0];
      appCtx.currentEpisode = 1;
      appCtx.seasonNavIndex = 0;
      appCtx.nextState = STATE_SEASON_BROWSER;
      appCtx.currentState = STATE_TRANSITION;
      appCtx.transitionStart = millis();
      appCtx.transitionDurationMS = TRANSITION_STATIC_MS;
    } else {
      dbgPrint("No seasons found for show: " + String(appCtx.currentShowDir));
    }
  }

  // Channel back -> media selector
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

// Process input in SEASON_BROWSER state
// Volume knob: navigate seasons
// Channel forward (encoderCW): select -> enter episode browser
// Channel back (encoderCCW): back to show browser
// IR: vol=navigate, ch-up=select, ch-dn=back
void handleSeasonBrowserInput() {
  RawInputFlags& raw = appCtx.rawInput;

  // Navigate seasons with volume knob
  if (raw.encoder2CW || raw.irVolUp) {
    raw.encoder2CW = false;
    raw.irVolUp = false;
    if (appCtx.seasonNavIndex < appCtx.availableSeasonCount - 1) {
      appCtx.seasonNavIndex++;
      appCtx.currentSeason = appCtx.availableSeasons[appCtx.seasonNavIndex];
      appCtx.seasonMetaLoaded = false;
      loadSeasonMetadata(appCtx.currentSeason);
      drawSeasonBrowser();
    }
  }
  if (raw.encoder2CCW || raw.irVolDn) {
    raw.encoder2CCW = false;
    raw.irVolDn = false;
    if (appCtx.seasonNavIndex > 0) {
      appCtx.seasonNavIndex--;
      appCtx.currentSeason = appCtx.availableSeasons[appCtx.seasonNavIndex];
      appCtx.seasonMetaLoaded = false;
      loadSeasonMetadata(appCtx.currentSeason);
      drawSeasonBrowser();
    }
  }

  // Select season -> enter episode browser (channel forward)
  if (raw.encoderCW || raw.irChannelUp) {
    raw.encoderCW = false;
    raw.irChannelUp = false;
    appCtx.currentEpisode = 1;
    appCtx.nextState = STATE_EPISODE_BROWSER;
    appCtx.currentState = STATE_TRANSITION;
    appCtx.transitionStart = millis();
    appCtx.transitionDurationMS = TRANSITION_STATIC_MS;
  }

  // Back to show browser (channel back)
  if (raw.encoderCCW || raw.irChannelDn) {
    raw.encoderCCW = false;
    raw.irChannelDn = false;
    appCtx.nextState = STATE_SHOW_BROWSER;
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

// Process input in EPISODE_BROWSER state
// Volume knob: navigate episodes (CW=next, CCW=prev)
// Channel forward (encoderCW): play episode
// Channel back (encoderCCW): back to season browser
// IR: vol=navigate, ch-up=play, ch-dn=back
void handleEpisodeBrowserInput() {
  RawInputFlags& raw = appCtx.rawInput;

  // Navigate episodes with volume knob
  // Volume CW = next episode
  if (raw.encoder2CW || raw.irVolUp) {
    raw.encoder2CW = false;
    raw.irVolUp = false;
    if (appCtx.seasonMetaLoaded &&
        appCtx.currentEpisode < appCtx.meta2.seasonMeta.episodeCount) {
      appCtx.currentEpisode++;
      appCtx.episodeMetaLoaded = false;
      loadEpisodeMetadata(appCtx.currentSeason, appCtx.currentEpisode);
      drawEpisodeBrowser();
    }
  }
  // Volume CCW = prev episode
  if (raw.encoder2CCW || raw.irVolDn) {
    raw.encoder2CCW = false;
    raw.irVolDn = false;
    if (appCtx.currentEpisode > 1) {
      appCtx.currentEpisode--;
      appCtx.episodeMetaLoaded = false;
      loadEpisodeMetadata(appCtx.currentSeason, appCtx.currentEpisode);
      drawEpisodeBrowser();
    }
  }

  // Play episode (channel forward)
  if (raw.encoderCW || raw.irChannelUp) {
    raw.encoderCW = false;
    raw.irChannelUp = false;
    appCtx.nextState = STATE_PLAYBACK;
    appCtx.currentState = STATE_TRANSITION;
    appCtx.transitionStart = millis();
    appCtx.transitionDurationMS = TRANSITION_STATIC_MS;
  }

  // Back to season browser (channel back)
  if (raw.encoderCCW || raw.irChannelDn) {
    raw.encoderCCW = false;
    raw.irChannelDn = false;
    appCtx.nextState = STATE_SEASON_BROWSER;
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
