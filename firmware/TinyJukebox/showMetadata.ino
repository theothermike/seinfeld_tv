//-------------------------------------------------------------------------------
//  TinyJukebox - Metadata Reader & Thumbnail Streamer
//
//  Reads .sdb binary metadata files from SD card and streams .raw
//  RGB565 thumbnails row-by-row to the display framebuffer.
//  TV Shows use /TV/{ShowDir}/S##/E##.avi layout.
//-------------------------------------------------------------------------------

// Forward declarations for display functions (defined in display.ino)
extern uint16_t frameBuf[];

// ─── Show Scanning ──────────────────────────────────────────────────────────
// Scan /TV/ directory for subdirectories containing show.sdb files.

void scanAvailableShows() {
  appCtx.availableShowCount = 0;

  File32 root;
  if (!root.open(TV_DIR, O_RDONLY)) {
    dbgPrint("No /TV directory found");
    return;
  }

  File32 entry;
  while (entry.openNext(&root, O_RDONLY)) {
    if (!entry.isDir()) {
      entry.close();
      continue;
    }

    char dirName[SHOW_DIR_LEN];
    entry.getName(dirName, sizeof(dirName));
    entry.close();

    if (dirName[0] == '.' || dirName[0] == '_') continue;

    char sdbPath[64];
    snprintf(sdbPath, sizeof(sdbPath), "%s/%s/show.sdb", TV_DIR, dirName);
    File32 sdbCheck;
    if (sdbCheck.open(sdbPath, O_RDONLY)) {
      sdbCheck.close();
      if (appCtx.availableShowCount < MAX_SHOWS) {
        strncpy(appCtx.availableShows[appCtx.availableShowCount], dirName, SHOW_DIR_LEN - 1);
        appCtx.availableShows[appCtx.availableShowCount][SHOW_DIR_LEN - 1] = '\0';
        appCtx.availableShowCount++;
        dbgPrint("Found show: " + String(dirName));
      }
    }
  }
  root.close();

  // Sort alphabetically
  for (int i = 0; i < appCtx.availableShowCount - 1; i++) {
    for (int j = i + 1; j < appCtx.availableShowCount; j++) {
      if (strcmp(appCtx.availableShows[i], appCtx.availableShows[j]) > 0) {
        char tmp[SHOW_DIR_LEN];
        memcpy(tmp, appCtx.availableShows[i], SHOW_DIR_LEN);
        memcpy(appCtx.availableShows[i], appCtx.availableShows[j], SHOW_DIR_LEN);
        memcpy(appCtx.availableShows[j], tmp, SHOW_DIR_LEN);
      }
    }
  }

  dbgPrint("Available shows: " + String(appCtx.availableShowCount));
}

// ─── Metadata Loading ────────────────────────────────────────────────────────

bool loadShowMetadata() {
  char path[64];
  snprintf(path, sizeof(path), "%s/%s/show.sdb", TV_DIR, appCtx.currentShowDir);

  File32 f;
  if (!f.open(path, O_RDONLY)) {
    dbgPrint("Failed to open " + String(path));
    return false;
  }

  if (f.fileSize() != sizeof(ShowMetadata)) {
    dbgPrint("show.sdb wrong size");
    f.close();
    return false;
  }

  f.read(&appCtx.meta1.showMeta, sizeof(ShowMetadata));
  f.close();

  if (strncmp(appCtx.meta1.showMeta.magic, SHOW_MAGIC, 4) != 0) {
    dbgPrint("show.sdb bad magic");
    return false;
  }

  appCtx.metadataLoaded = true;
  dbgPrint("Loaded show: " + String(appCtx.meta1.showMeta.name));
  dbgPrint("Seasons: " + String(appCtx.meta1.showMeta.seasonCount));
  dbgPrint("Episodes: " + String(appCtx.meta1.showMeta.totalEpisodes));
  return true;
}

bool loadSeasonMetadata(int seasonNum) {
  char path[64];
  snprintf(path, sizeof(path), "%s/%s/S%02d/season.sdb", TV_DIR, appCtx.currentShowDir, seasonNum);

  File32 f;
  if (!f.open(path, O_RDONLY)) {
    dbgPrint("Failed to open " + String(path));
    return false;
  }

  if (f.fileSize() != sizeof(SeasonMetadata)) {
    dbgPrint("season.sdb wrong size");
    f.close();
    return false;
  }

  f.read(&appCtx.meta2.seasonMeta, sizeof(SeasonMetadata));
  f.close();

  if (strncmp(appCtx.meta2.seasonMeta.magic, SEASON_MAGIC, 4) != 0) {
    dbgPrint("season.sdb bad magic");
    return false;
  }

  appCtx.seasonMetaLoaded = true;
  dbgPrint("Loaded season " + String(seasonNum) + ": " +
           String(appCtx.meta2.seasonMeta.episodeCount) + " episodes");
  return true;
}

bool loadEpisodeMetadata(int seasonNum, int episodeNum) {
  char path[64];
  snprintf(path, sizeof(path), "%s/%s/S%02d/E%02d.sdb", TV_DIR, appCtx.currentShowDir, seasonNum, episodeNum);

  File32 f;
  if (!f.open(path, O_RDONLY)) {
    dbgPrint("Failed to open " + String(path));
    return false;
  }

  if (f.fileSize() != sizeof(EpisodeMetadata)) {
    dbgPrint("episode.sdb wrong size");
    f.close();
    return false;
  }

  f.read(&appCtx.meta3.episodeMeta, sizeof(EpisodeMetadata));
  f.close();

  if (strncmp(appCtx.meta3.episodeMeta.magic, EPISODE_MAGIC, 4) != 0) {
    dbgPrint("episode.sdb bad magic");
    return false;
  }

  appCtx.episodeMetaLoaded = true;
  dbgPrint("Loaded S" + String(seasonNum) + "E" + String(episodeNum) +
           ": " + String(appCtx.meta3.episodeMeta.title));
  return true;
}

// ─── Season Scanning ─────────────────────────────────────────────────────

void scanAvailableSeasons() {
  appCtx.availableSeasonCount = 0;

  for (int s = 1; s <= 30; s++) {
    char path[64];
    snprintf(path, sizeof(path), "%s/%s/S%02d/season.sdb", TV_DIR, appCtx.currentShowDir, s);
    File32 f;
    if (f.open(path, O_RDONLY)) {
      f.close();
      appCtx.availableSeasons[appCtx.availableSeasonCount] = s;
      appCtx.availableSeasonCount++;
      dbgPrint("Found season " + String(s));
    }
  }

  dbgPrint("Available seasons: " + String(appCtx.availableSeasonCount));

  if (appCtx.availableSeasonCount > 0) {
    appCtx.seasonNavIndex = 0;
    for (int i = 0; i < appCtx.availableSeasonCount; i++) {
      if (appCtx.availableSeasons[i] == appCtx.currentSeason) {
        appCtx.seasonNavIndex = i;
        break;
      }
    }
    appCtx.currentSeason = appCtx.availableSeasons[appCtx.seasonNavIndex];
  }
}

// ─── Thumbnail Streaming ─────────────────────────────────────────────────────

bool displayThumbnail(const char* path, int destX, int destY) {
  File32 f;
  if (!f.open(path, O_RDONLY)) {
    dbgPrint("Failed to open thumbnail: " + String(path));
    return false;
  }

  uint16_t rowBuf[THUMB_W];

  for (int row = 0; row < THUMB_H; row++) {
    int bytesRead = f.read(rowBuf, THUMB_ROW_BYTES);
    if (bytesRead != THUMB_ROW_BYTES) {
      dbgPrint("Thumbnail read error at row " + String(row));
      f.close();
      return false;
    }

    int fbY = destY + row;
    if (fbY < 0 || fbY >= VIDEO_H) continue;

    for (int col = 0; col < THUMB_W; col++) {
      int fbX = destX + col;
      if (fbX < 0 || fbX >= VIDEO_W) continue;
      frameBuf[fbY * VIDEO_W + fbX] = rowBuf[col];
    }
  }

  f.close();
  return true;
}

// ─── Fullscreen Raw Display ──────────────────────────────────────────────────
// Streams a 210x135 RGB565 .raw file directly into frameBuf for photo slideshow.

bool displayFullScreenRaw(const char* path) {
  File32 f;
  if (!f.open(path, O_RDONLY)) {
    dbgPrint("Failed to open fullscreen raw: " + String(path));
    return false;
  }

  uint16_t rowBuf[FULLSCREEN_W];

  for (int row = 0; row < FULLSCREEN_H; row++) {
    int bytesRead = f.read(rowBuf, FULLSCREEN_W * 2);
    if (bytesRead != FULLSCREEN_W * 2) {
      dbgPrint("Fullscreen read error at row " + String(row));
      f.close();
      return false;
    }

    for (int col = 0; col < FULLSCREEN_W; col++) {
      frameBuf[row * VIDEO_W + col] = rowBuf[col];
    }
  }

  f.close();

  waitForScreenDMA();
  setScreenAddressWindow(VIDEO_X, VIDEO_Y, VIDEO_W, VIDEO_H);
  writeToScreenDMA(frameBuf, VIDEO_W * VIDEO_H);

  return true;
}

// ─── Settings Persistence ────────────────────────────────────────────────────

void loadShowSettings() {
  File32 f;
  if (!f.open("/settings.txt", O_RDONLY)) {
    dbgPrint("No settings.txt, using defaults");
    appCtx.currentSeason = 1;
    appCtx.currentEpisode = 1;
    appCtx.currentShowDir[0] = '\0';
    appCtx.slideshowIntervalSec = DEFAULT_SLIDESHOW_SEC;
    return;
  }

  char line[64];
  while (f.available()) {
    int len = 0;
    char c;
    while (f.available() && len < 63) {
      c = f.read();
      if (c == '\n' || c == '\r') break;
      line[len++] = c;
    }
    line[len] = '\0';

    if (line[0] == '#' || len == 0) continue;

    char* eq = strchr(line, '=');
    if (!eq) continue;

    *eq = '\0';
    char* key = line;
    char* val = eq + 1;

    if (strcmp(key, "last_show") == 0) {
      strncpy(appCtx.currentShowDir, val, SHOW_DIR_LEN - 1);
      appCtx.currentShowDir[SHOW_DIR_LEN - 1] = '\0';
    } else if (strcmp(key, "last_season") == 0) {
      appCtx.savedSeason = atoi(val);
      appCtx.currentSeason = appCtx.savedSeason;
    } else if (strcmp(key, "last_episode") == 0) {
      appCtx.savedEpisode = atoi(val);
      appCtx.currentEpisode = appCtx.savedEpisode;
    } else if (strcmp(key, "volume") == 0) {
      appCtx.savedVolume = atoi(val);
      volumeSetting = appCtx.savedVolume;
    } else if (strcmp(key, "slideshow_interval") == 0) {
      int interval = atoi(val);
      if (interval >= 3 && interval <= 30) {
        appCtx.slideshowIntervalSec = interval;
      }
    } else if (strcmp(key, "media_type") == 0) {
      int mt = atoi(val);
      if (mt >= 0 && mt < MEDIA_TYPE_COUNT) {
        appCtx.currentMediaType = (MediaType)mt;
      }
    }
  }
  f.close();
  dbgPrint("Loaded settings: show=" + String(appCtx.currentShowDir) +
           " S" + String(appCtx.currentSeason) +
           "E" + String(appCtx.currentEpisode) +
           " vol=" + String(volumeSetting));
}

void saveShowSettings() {
  File32 f;
  if (!f.open("/settings.txt", O_WRONLY | O_CREAT | O_TRUNC)) {
    dbgPrint("Failed to save settings");
    return;
  }

  char buf[256];
  snprintf(buf, sizeof(buf),
           "# TinyJukebox Settings\n"
           "last_show=%s\n"
           "last_season=%d\n"
           "last_episode=%d\n"
           "volume=%d\n"
           "slideshow_interval=%d\n"
           "media_type=%d\n",
           appCtx.currentShowDir, appCtx.currentSeason,
           appCtx.currentEpisode, volumeSetting,
           appCtx.slideshowIntervalSec, (int)appCtx.currentMediaType);
  f.write(buf, strlen(buf));
  f.close();
  appCtx.settingsDirty = false;
  dbgPrint("Saved settings");
}

// ─── Path Helpers ────────────────────────────────────────────────────────────

void getEpisodeAVIPath(char* buf, int bufLen, int season, int episode) {
  snprintf(buf, bufLen, "%s/%s/S%02d/E%02d.avi", TV_DIR, appCtx.currentShowDir, season, episode);
}

void getEpisodeSDBPath(char* buf, int bufLen, int season, int episode) {
  snprintf(buf, bufLen, "%s/%s/S%02d/E%02d.sdb", TV_DIR, appCtx.currentShowDir, season, episode);
}

void getEpisodeThumbPath(char* buf, int bufLen, int season, int episode) {
  snprintf(buf, bufLen, "%s/%s/S%02d/E%02d.raw", TV_DIR, appCtx.currentShowDir, season, episode);
}

void getSeasonThumbPath(char* buf, int bufLen, int season) {
  snprintf(buf, bufLen, "%s/%s/S%02d/thumb.raw", TV_DIR, appCtx.currentShowDir, season);
}

void getShowThumbPath(char* buf, int bufLen, int showIdx) {
  snprintf(buf, bufLen, "%s/%s/show.raw", TV_DIR, appCtx.availableShows[showIdx]);
}
