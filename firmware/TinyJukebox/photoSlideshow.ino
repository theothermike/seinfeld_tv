//-------------------------------------------------------------------------------
//  TinyJukebox - Photo Slideshow
//
//  Scans /Photos/ for album directories containing album.sdb.
//  Each album has P##.raw (210x135 RGB565) photo files and P##.sdb metadata.
//  Album browser: Vol knob navigates albums, CH forward enters slideshow.
//  Slideshow: auto-advances on timer, vol knob manual advance, CH back exits.
//  Photos are read directly into frameBuf (fullscreen 210x135 RGB565).
//-------------------------------------------------------------------------------

#include "uiHelpers.h"

extern uint16_t frameBuf[];
extern GraphicsBuffer2 screenBuffer;

// ─── Photo Album Scanning ───────────────────────────────────────────────────
// Scan /Photos/ for subdirectories containing album.sdb.

void scanPhotoAlbums() {
  appCtx.availableItemCount = 0;

  File32 root;
  if (!root.open(PHOTOS_DIR, O_RDONLY)) {
    dbgPrint("Failed to open " PHOTOS_DIR);
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

    if (dirName[0] == '.' || dirName[0] == '_') continue;

    char sdbPath[64];
    snprintf(sdbPath, sizeof(sdbPath), "%s/%s/album.sdb", PHOTOS_DIR, dirName);
    File32 sdbCheck;
    if (sdbCheck.open(sdbPath, O_RDONLY)) {
      sdbCheck.close();
      if (appCtx.availableItemCount < MAX_ITEMS) {
        strncpy(appCtx.availableItems[appCtx.availableItemCount], dirName, ITEM_DIR_LEN - 1);
        appCtx.availableItems[appCtx.availableItemCount][ITEM_DIR_LEN - 1] = '\0';
        appCtx.availableItemCount++;
        dbgPrint("Found photo album: " + String(dirName));
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
    // Load album metadata
    loadPhotoAlbumMetadata();
  }

  dbgPrint("Photo albums found: " + String(appCtx.availableItemCount));
}

// ─── Metadata Loading ───────────────────────────────────────────────────────

static bool loadPhotoAlbumMetadata() {
  char path[64];
  snprintf(path, sizeof(path), "%s/%s/album.sdb", PHOTOS_DIR, appCtx.currentItemDir);

  File32 f;
  if (!f.open(path, O_RDONLY)) {
    dbgPrint("Failed to open " + String(path));
    appCtx.metadataLoaded = false;
    return false;
  }

  if (f.fileSize() != sizeof(PhotoAlbumMetadata)) {
    dbgPrint("album.sdb wrong size: " + String(f.fileSize()));
    f.close();
    appCtx.metadataLoaded = false;
    return false;
  }

  f.read(&appCtx.meta2.photoAlbumMeta, sizeof(PhotoAlbumMetadata));
  f.close();

  if (strncmp(appCtx.meta2.photoAlbumMeta.magic, PHOTO_ALBUM_MAGIC, 4) != 0) {
    dbgPrint("album.sdb bad magic");
    appCtx.metadataLoaded = false;
    return false;
  }

  appCtx.metadataLoaded = true;
  appCtx.subItemCount = appCtx.meta2.photoAlbumMeta.photoCount;
  dbgPrint("Loaded photo album: " + String(appCtx.meta2.photoAlbumMeta.title) +
           " (" + String(appCtx.subItemCount) + " photos)");
  return true;
}

static bool loadPhotoMetadata(int photoNum) {
  char path[64];
  snprintf(path, sizeof(path), "%s/%s/P%02d.sdb",
           PHOTOS_DIR, appCtx.currentItemDir, photoNum);

  File32 f;
  if (!f.open(path, O_RDONLY)) {
    dbgPrint("Failed to open " + String(path));
    appCtx.episodeMetaLoaded = false;
    return false;
  }

  if (f.fileSize() != sizeof(PhotoMetadata)) {
    dbgPrint("P##.sdb wrong size: " + String(f.fileSize()));
    f.close();
    appCtx.episodeMetaLoaded = false;
    return false;
  }

  f.read(&appCtx.meta3.photoMeta, sizeof(PhotoMetadata));
  f.close();

  if (strncmp(appCtx.meta3.photoMeta.magic, PHOTO_MAGIC, 4) != 0) {
    dbgPrint("P##.sdb bad magic");
    appCtx.episodeMetaLoaded = false;
    return false;
  }

  appCtx.episodeMetaLoaded = true;
  dbgPrint("Loaded photo P" + String(photoNum) + ": " +
           String(appCtx.meta3.photoMeta.caption));
  return true;
}

// ─── Draw Photo Album Browser ───────────────────────────────────────────────
//
// Layout (210x135):
//   Row 0-14:   "Photos" header (white, centered)
//   Row 16:     Divider
//   Row 20-86:  Thumbnail (108x67) centered
//   Row 90:     Album title (yellow, centered)
//   Row 106:    Photo count (gray)
//   Row 122:    Hint text

void drawPhotoAlbumBrowser() {
  clearFrameBuf();

  // Header
  const char* header = "Photos";
  int headerW = textWidth(header);
  drawText(header, (VIDEO_W - headerW) / 2, 4, COL_WHITE);

  // Divider
  drawHLine(10, 16, VIDEO_W - 20, COL_GRAY_DK);

  // Thumbnail
  int thumbX = (VIDEO_W - THUMB_W) / 2;
  int thumbY = 20;
  char thumbPath[64];
  snprintf(thumbPath, sizeof(thumbPath), "%s/%s/album.raw",
           PHOTOS_DIR, appCtx.availableItems[appCtx.itemNavIndex]);
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

  // Album title (scrolls if too long)
  if (appCtx.metadataLoaded) {
    const char* title = appCtx.meta2.photoAlbumMeta.title;
    drawScrollText(title, 2, 92, VIDEO_W - 4, COL_YELLOW,
                   &appCtx.scrollState.slots[0]);

    // Photo count
    char infoStr[32];
    snprintf(infoStr, sizeof(infoStr), "%d photos",
             appCtx.meta2.photoAlbumMeta.photoCount);
    int infoW = textWidth(infoStr);
    drawText(infoStr, (VIDEO_W - infoW) / 2, 106, COL_GRAY_MED);
  } else {
    const char* dirName = appCtx.availableItems[appCtx.itemNavIndex];
    int nameW = textWidth(dirName);
    drawText(dirName, (VIDEO_W - nameW) / 2, 92, COL_YELLOW);
  }

  // Push to display
  waitForScreenDMA();
  setScreenAddressWindow(VIDEO_X, VIDEO_Y, VIDEO_W, VIDEO_H);
  writeToScreenDMA(frameBuf, VIDEO_W * VIDEO_H);
}

// ─── Display Fullscreen Photo ───────────────────────────────────────────────
// Reads P##.raw (210x135 RGB565) directly into frameBuf and pushes to display.

void displayPhoto(int photoNum) {
  char path[64];
  snprintf(path, sizeof(path), "%s/%s/P%02d.raw",
           PHOTOS_DIR, appCtx.currentItemDir, photoNum);

  File32 f;
  if (!f.open(path, O_RDONLY)) {
    dbgPrint("Failed to open photo: " + String(path));
    // Show error screen
    clearFrameBuf();
    const char* errMsg = "Photo not found";
    int errW = textWidth(errMsg);
    drawText(errMsg, (VIDEO_W - errW) / 2, 60, COL_GRAY_MED);
    waitForScreenDMA();
    setScreenAddressWindow(VIDEO_X, VIDEO_Y, VIDEO_W, VIDEO_H);
    writeToScreenDMA(frameBuf, VIDEO_W * VIDEO_H);
    return;
  }

  // Verify file size matches fullscreen dimensions
  uint32_t expectedSize = FULLSCREEN_RAW_SIZE;
  if (f.fileSize() != expectedSize) {
    dbgPrint("Photo wrong size: " + String(f.fileSize()) +
             " expected " + String(expectedSize));
    f.close();
    return;
  }

  // Read directly into frameBuf (row by row to be safe)
  uint16_t rowBuf[FULLSCREEN_W];
  for (int row = 0; row < FULLSCREEN_H; row++) {
    int bytesRead = f.read(rowBuf, FULLSCREEN_W * 2);
    if (bytesRead != FULLSCREEN_W * 2) {
      dbgPrint("Photo read error at row " + String(row));
      break;
    }
    memcpy(&frameBuf[row * VIDEO_W], rowBuf, FULLSCREEN_W * 2);
  }
  f.close();

  // Load photo metadata for caption overlay (optional)
  loadPhotoMetadata(photoNum);

  // Push to display
  waitForScreenDMA();
  setScreenAddressWindow(VIDEO_X, VIDEO_Y, VIDEO_W, VIDEO_H);
  writeToScreenDMA(frameBuf, VIDEO_W * VIDEO_H);

  dbgPrint("Displayed photo P" + String(photoNum));
}

// ─── Slideshow Advance ──────────────────────────────────────────────────────

void advanceSlideshow() {
  if (appCtx.slideshowCurrentPhoto < appCtx.subItemCount) {
    appCtx.slideshowCurrentPhoto++;
  } else {
    // Wrap to first photo
    appCtx.slideshowCurrentPhoto = 1;
  }

  displayPhoto(appCtx.slideshowCurrentPhoto);
  appCtx.slideshowLastAdvance = millis();
}

// ─── Album Browser Input Handling ───────────────────────────────────────────

void handlePhotoAlbumInput() {
  RawInputFlags& raw = appCtx.rawInput;

  // Navigate albums
  if (raw.encoder2CW || raw.irVolUp) {
    raw.encoder2CW = false;
    raw.irVolUp = false;
    if (appCtx.itemNavIndex < appCtx.availableItemCount - 1) {
      appCtx.itemNavIndex++;
      strncpy(appCtx.currentItemDir, appCtx.availableItems[appCtx.itemNavIndex], ITEM_DIR_LEN - 1);
      appCtx.currentItemDir[ITEM_DIR_LEN - 1] = '\0';
      appCtx.metadataLoaded = false;
      loadPhotoAlbumMetadata();
      resetScrollState(&appCtx.scrollState);
      drawPhotoAlbumBrowser();
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
      loadPhotoAlbumMetadata();
      resetScrollState(&appCtx.scrollState);
      drawPhotoAlbumBrowser();
    }
  }

  // Select album -> enter slideshow
  if (raw.encoderCW || raw.irChannelUp) {
    raw.encoderCW = false;
    raw.irChannelUp = false;
    if (appCtx.subItemCount > 0) {
      appCtx.nextState = STATE_PHOTO_SLIDESHOW;
      appCtx.currentState = STATE_TRANSITION;
      appCtx.transitionStart = millis();
      appCtx.transitionDurationMS = TRANSITION_STATIC_MS;
    } else {
      dbgPrint("No photos in album: " + String(appCtx.currentItemDir));
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

// ─── Slideshow Input Handling ───────────────────────────────────────────────

void handlePhotoSlideshowInput() {
  RawInputFlags& raw = appCtx.rawInput;

  // Manual advance forward
  if (raw.encoder2CW || raw.irVolUp || raw.encoderCW || raw.irChannelUp) {
    raw.encoder2CW = false;
    raw.irVolUp = false;
    raw.encoderCW = false;
    raw.irChannelUp = false;
    advanceSlideshow();
  }

  // Manual advance backward
  if (raw.encoder2CCW || raw.irVolDn) {
    raw.encoder2CCW = false;
    raw.irVolDn = false;
    if (appCtx.slideshowCurrentPhoto > 1) {
      appCtx.slideshowCurrentPhoto--;
    } else {
      appCtx.slideshowCurrentPhoto = appCtx.subItemCount;
    }
    displayPhoto(appCtx.slideshowCurrentPhoto);
    appCtx.slideshowLastAdvance = millis();
  }

  // Back to album browser
  if (raw.encoderCCW || raw.irChannelDn) {
    raw.encoderCCW = false;
    raw.irChannelDn = false;
    appCtx.nextState = STATE_PHOTO_ALBUM;
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
