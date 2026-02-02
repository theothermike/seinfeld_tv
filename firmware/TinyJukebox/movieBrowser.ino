//-------------------------------------------------------------------------------
//  TinyJukebox - Movie Browser
//
//  Scans /Movies/ for subdirectories containing movie.sdb files.
//  Displays movie thumbnails and metadata. Vol knob navigates,
//  CH forward plays movie.avi, CH back returns to media selector.
//-------------------------------------------------------------------------------

#include "uiHelpers.h"

extern uint16_t frameBuf[];
extern GraphicsBuffer2 screenBuffer;

// ─── Movie Scanning ─────────────────────────────────────────────────────────
// Scan /Movies/ for subdirectories containing movie.sdb.
// Populates appCtx.availableItems[] and appCtx.availableItemCount.

void scanMovies() {
  appCtx.availableItemCount = 0;

  File32 root;
  if (!root.open(MOVIES_DIR, O_RDONLY)) {
    dbgPrint("Failed to open " MOVIES_DIR);
    return;
  }

  File32 entry;
  while (entry.openNext(&root, O_RDONLY)) {
    if (!entry.isDir()) {
      entry.close();
      continue;
    }

    char dirName[ITEM_DIR_LEN];
    entry.getName(dirName, sizeof(dirName));
    entry.close();

    // Skip hidden/system dirs
    if (dirName[0] == '.' || dirName[0] == '_') continue;

    // Check for movie.sdb
    char sdbPath[64];
    snprintf(sdbPath, sizeof(sdbPath), "%s/%s/movie.sdb", MOVIES_DIR, dirName);
    File32 sdbCheck;
    if (sdbCheck.open(sdbPath, O_RDONLY)) {
      sdbCheck.close();
      if (appCtx.availableItemCount < MAX_ITEMS) {
        strncpy(appCtx.availableItems[appCtx.availableItemCount], dirName, ITEM_DIR_LEN - 1);
        appCtx.availableItems[appCtx.availableItemCount][ITEM_DIR_LEN - 1] = '\0';
        appCtx.availableItemCount++;
        dbgPrint("Found movie: " + String(dirName));
      }
    }
  }
  root.close();

  // Sort alphabetically
  for (int i = 0; i < appCtx.availableItemCount - 1; i++) {
    for (int j = i + 1; j < appCtx.availableItemCount; j++) {
      if (strcmp(appCtx.availableItems[i], appCtx.availableItems[j]) > 0) {
        char tmp[ITEM_DIR_LEN];
        memcpy(tmp, appCtx.availableItems[i], ITEM_DIR_LEN);
        memcpy(appCtx.availableItems[i], appCtx.availableItems[j], ITEM_DIR_LEN);
        memcpy(appCtx.availableItems[j], tmp, ITEM_DIR_LEN);
      }
    }
  }

  appCtx.itemNavIndex = 0;
  if (appCtx.availableItemCount > 0) {
    strncpy(appCtx.currentItemDir, appCtx.availableItems[0], ITEM_DIR_LEN - 1);
    appCtx.currentItemDir[ITEM_DIR_LEN - 1] = '\0';
    loadMovieMetadata();
  }

  dbgPrint("Movies found: " + String(appCtx.availableItemCount));
}

// ─── Metadata Loading ───────────────────────────────────────────────────────

bool loadMovieMetadata() {
  char path[64];
  snprintf(path, sizeof(path), "%s/%s/movie.sdb", MOVIES_DIR, appCtx.currentItemDir);

  File32 f;
  if (!f.open(path, O_RDONLY)) {
    dbgPrint("Failed to open " + String(path));
    appCtx.metadataLoaded = false;
    return false;
  }

  if (f.fileSize() != sizeof(MovieMetadata)) {
    dbgPrint("movie.sdb wrong size: " + String(f.fileSize()));
    f.close();
    appCtx.metadataLoaded = false;
    return false;
  }

  f.read(&appCtx.meta1.movieMeta, sizeof(MovieMetadata));
  f.close();

  if (strncmp(appCtx.meta1.movieMeta.magic, MOVIE_MAGIC, 4) != 0) {
    dbgPrint("movie.sdb bad magic");
    appCtx.metadataLoaded = false;
    return false;
  }

  appCtx.metadataLoaded = true;
  dbgPrint("Loaded movie: " + String(appCtx.meta1.movieMeta.title));
  return true;
}

// ─── Draw Movie Browser ─────────────────────────────────────────────────────
//
// Layout (210x135):
//   Row 0-14:   "Movies" header (white, centered)
//   Row 16:     Divider
//   Row 20-86:  Thumbnail (108x67) centered
//   Row 90:     Movie title (yellow, centered)
//   Row 106:    Year + runtime (gray)
//   Row 122:    Hint text

void drawMovieBrowser() {
  clearFrameBuf();

  // Header
  const char* header = "Movies";
  int headerW = textWidth(header);
  drawText(header, (VIDEO_W - headerW) / 2, 4, COL_WHITE);

  // Divider
  drawHLine(10, 16, VIDEO_W - 20, COL_GRAY_DK);

  // Thumbnail (centered)
  int thumbX = (VIDEO_W - THUMB_W) / 2;
  int thumbY = 20;
  char thumbPath[64];
  snprintf(thumbPath, sizeof(thumbPath), "%s/%s/movie.raw",
           MOVIES_DIR, appCtx.availableItems[appCtx.itemNavIndex]);
  if (!displayThumbnail(thumbPath, thumbX, thumbY)) {
    fillRect(thumbX, thumbY, THUMB_W, THUMB_H, COL_GRAY_DK);
  }

  // Navigation arrows
  if (appCtx.itemNavIndex > 0) {
    drawText("<", 4, thumbY + THUMB_H / 2 - 6, COL_GRAY_MED);
  }
  if (appCtx.itemNavIndex < appCtx.availableItemCount - 1) {
    drawText(">", VIDEO_W - 12, thumbY + THUMB_H / 2 - 6, COL_GRAY_MED);
  }

  // Movie title (scrolls if too long)
  if (appCtx.metadataLoaded) {
    const char* title = appCtx.meta1.movieMeta.title;
    drawScrollText(title, 2, 92, VIDEO_W - 4, COL_YELLOW,
                   &appCtx.scrollState.slots[0]);

    // Year + runtime
    char infoStr[48];
    if (appCtx.meta1.movieMeta.runtimeMinutes > 0) {
      snprintf(infoStr, sizeof(infoStr), "%s - %dmin",
               appCtx.meta1.movieMeta.year,
               appCtx.meta1.movieMeta.runtimeMinutes);
    } else {
      snprintf(infoStr, sizeof(infoStr), "%s", appCtx.meta1.movieMeta.year);
    }
    int infoW = textWidth(infoStr);
    drawText(infoStr, (VIDEO_W - infoW) / 2, 106, COL_GRAY_MED);
  } else {
    // Fallback: directory name
    const char* dirName = appCtx.availableItems[appCtx.itemNavIndex];
    int nameW = textWidth(dirName);
    drawText(dirName, (VIDEO_W - nameW) / 2, 92, COL_YELLOW);
  }

  // Push to display
  waitForScreenDMA();
  setScreenAddressWindow(VIDEO_X, VIDEO_Y, VIDEO_W, VIDEO_H);
  writeToScreenDMA(frameBuf, VIDEO_W * VIDEO_H);
}

// ─── Input Handling ─────────────────────────────────────────────────────────

void handleMovieBrowserInput() {
  RawInputFlags& raw = appCtx.rawInput;

  // Navigate with volume knob
  if (raw.encoder2CW || raw.irVolUp) {
    raw.encoder2CW = false;
    raw.irVolUp = false;
    if (appCtx.itemNavIndex < appCtx.availableItemCount - 1) {
      appCtx.itemNavIndex++;
      strncpy(appCtx.currentItemDir, appCtx.availableItems[appCtx.itemNavIndex], ITEM_DIR_LEN - 1);
      appCtx.currentItemDir[ITEM_DIR_LEN - 1] = '\0';
      appCtx.metadataLoaded = false;
      loadMovieMetadata();
      resetScrollState(&appCtx.scrollState);
      drawMovieBrowser();
    }
  }
  if (raw.encoder2CCW || raw.irVolDn) {
    raw.encoder2CCW = false;
    raw.irVolDn = false;
    if (appCtx.itemNavIndex > 0) {
      appCtx.itemNavIndex--;
      strncpy(appCtx.currentItemDir, appCtx.availableItems[appCtx.itemNavIndex], ITEM_DIR_LEN - 1);
      appCtx.currentItemDir[ITEM_DIR_LEN - 1] = '\0';
      appCtx.metadataLoaded = false;
      loadMovieMetadata();
      resetScrollState(&appCtx.scrollState);
      drawMovieBrowser();
    }
  }

  // Play movie (channel forward)
  if (raw.encoderCW || raw.irChannelUp) {
    raw.encoderCW = false;
    raw.irChannelUp = false;

    char aviPath[64];
    snprintf(aviPath, sizeof(aviPath), "%s/%s/movie.avi",
             MOVIES_DIR, appCtx.currentItemDir);
    dbgPrint("Playing movie: " + String(aviPath));
    startGenericPlayback(aviPath);
  }

  // Back to media selector (channel back)
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
