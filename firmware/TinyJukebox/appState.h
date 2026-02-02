//-------------------------------------------------------------------------------
//  TinyJukebox - Application State & Data Structures
//
//  State machine, context struct, and binary metadata structs that match
//  the Python converter's .sdb binary format.
//  Supports: TV Shows, Movies, Music Videos, Music, Photos, YouTube
//-------------------------------------------------------------------------------

#ifndef APPSTATE_H
#define APPSTATE_H

#include <stdint.h>

// ─── Media Types ─────────────────────────────────────────────────────────────

enum MediaType {
  MEDIA_TV = 0,
  MEDIA_MOVIES,
  MEDIA_MUSIC_VIDEOS,
  MEDIA_MUSIC,
  MEDIA_PHOTOS,
  MEDIA_YOUTUBE,
  MEDIA_TYPE_COUNT  // Always last - gives us the count
};

// ─── State Machine ───────────────────────────────────────────────────────────

enum AppState {
  STATE_BOOT,
  STATE_SPLASH,
  STATE_MEDIA_SELECTOR,
  // TV Shows
  STATE_SHOW_BROWSER,
  STATE_SEASON_BROWSER,
  STATE_EPISODE_BROWSER,
  // Movies
  STATE_MOVIE_BROWSER,
  // Music Videos
  STATE_MV_COLLECTION,
  STATE_MV_VIDEO_BROWSER,
  // Music
  STATE_MUSIC_ARTIST,
  STATE_MUSIC_ALBUM,
  STATE_MUSIC_TRACK,
  // Photos
  STATE_PHOTO_ALBUM,
  STATE_PHOTO_SLIDESHOW,
  // YouTube
  STATE_YOUTUBE_PLAYLIST,
  STATE_YOUTUBE_VIDEO_BROWSER,
  // Settings
  STATE_SETTINGS,
  // Shared
  STATE_PLAYBACK,
  STATE_TRANSITION,
  STATE_POWER_OFF
};

// ─── Binary Metadata Structs (match converter/binary_writer.py) ──────────────
// All multi-byte integers are little-endian (RP2040 native).
// All strings are null-padded to their fixed width.

// --- TV Shows ---

// show.sdb - 128 bytes
struct ShowMetadata {
  char     magic[4];        // "SFTV"
  uint8_t  version;         // FORMAT_VERSION (1)
  uint8_t  seasonCount;
  uint16_t totalEpisodes;   // LE
  char     name[48];        // null-padded
  char     year[8];         // null-padded
  uint8_t  reserved[64];
} __attribute__((packed));

// season.sdb - 64 bytes
struct SeasonMetadata {
  char    magic[4];          // "SFSN"
  uint8_t seasonNumber;
  uint8_t episodeCount;
  uint8_t reserved1[2];
  char    year[8];           // null-padded
  char    title[24];         // null-padded
  uint8_t reserved2[24];
} __attribute__((packed));

// E##.sdb - 128 bytes
struct EpisodeMetadata {
  char     magic[4];         // "SFEP"
  uint8_t  seasonNumber;
  uint8_t  episodeNumber;
  uint16_t runtimeMinutes;   // LE
  char     title[48];        // null-padded
  char     airDate[12];      // null-padded
  char     description[56];  // null-padded
  uint8_t  reserved[4];
} __attribute__((packed));

// --- Movies ---

// movie.sdb - 128 bytes
struct MovieMetadata {
  char     magic[4];         // "TJMV"
  uint8_t  version;
  uint8_t  reserved1;
  uint16_t runtimeMinutes;   // LE
  char     title[48];        // null-padded
  char     year[8];          // null-padded
  char     description[56];  // null-padded
  uint8_t  reserved2[8];
} __attribute__((packed));

// --- Music Videos ---

// collection.sdb - 128 bytes
struct CollectionMetadata {
  char    magic[4];          // "TJVC"
  uint8_t version;
  uint8_t videoCount;
  uint8_t reserved1[2];
  char    name[48];          // null-padded
  char    year[8];           // null-padded
  uint8_t reserved2[64];
} __attribute__((packed));

// V##.sdb - 128 bytes
struct VideoMetadata {
  char     magic[4];         // "TJVD"
  uint8_t  videoNumber;
  uint8_t  reserved1;
  uint16_t runtimeMinutes;   // LE
  char     title[48];        // null-padded
  char     artist[12];       // null-padded
  char     description[56];  // null-padded
  uint8_t  reserved2[4];
} __attribute__((packed));

// --- Music ---

// artist.sdb - 128 bytes
struct ArtistMetadata {
  char     magic[4];         // "TJMA"
  uint8_t  version;
  uint8_t  albumCount;
  uint16_t totalTracks;      // LE
  char     name[48];         // null-padded
  char     genre[8];         // null-padded
  uint8_t  reserved[64];
} __attribute__((packed));

// album.sdb for music - 64 bytes
struct MusicAlbumMetadata {
  char    magic[4];          // "TJAL"
  uint8_t albumNumber;
  uint8_t trackCount;
  uint8_t reserved1[2];
  char    year[8];           // null-padded
  char    title[24];         // null-padded
  uint8_t reserved2[24];
} __attribute__((packed));

// T##.sdb - 64 bytes
struct TrackMetadata {
  char     magic[4];         // "TJTK"
  uint8_t  trackNumber;
  uint8_t  reserved1;
  uint16_t runtimeSeconds;   // LE
  char     title[48];        // null-padded
  uint8_t  reserved2[8];
} __attribute__((packed));

// --- Photos ---

// album.sdb for photos - 64 bytes
struct PhotoAlbumMetadata {
  char    magic[4];          // "TJPA"
  uint8_t version;
  uint8_t photoCount;
  uint8_t reserved1[2];
  char    title[48];         // null-padded
  uint8_t reserved2[8];
} __attribute__((packed));

// P##.sdb - 64 bytes
struct PhotoMetadata {
  char    magic[4];          // "TJPH"
  uint8_t photoNumber;
  uint8_t reserved1[3];
  char    caption[48];       // null-padded
  char    dateTaken[8];      // null-padded
} __attribute__((packed));

// --- YouTube ---

// playlist.sdb - 128 bytes
struct YouTubePlaylistMetadata {
  char    magic[4];          // "TJYP"
  uint8_t version;
  uint8_t videoCount;
  uint8_t reserved1[2];
  char    name[48];          // null-padded
  char    year[8];           // null-padded
  char    uploader[24];      // null-padded
  uint8_t reserved2[40];
} __attribute__((packed));

// Y##.sdb - 128 bytes
struct YouTubeVideoMetadata {
  char     magic[4];         // "TJYV"
  uint8_t  videoNumber;
  uint8_t  reserved1;
  uint16_t runtimeMinutes;   // LE
  char     title[48];        // null-padded
  char     uploader[12];     // null-padded
  char     uploadDate[12];   // null-padded "YYYY-MM-DD"
  char     description[44];  // null-padded
  uint8_t  reserved2[4];
} __attribute__((packed));

// ─── Input Flags (extended from upstream) ────────────────────────────────────

struct RawInputFlags {
  bool encoderCW   = false;
  bool encoderCCW  = false;
  bool encoder2CW  = false;
  bool encoder2CCW = false;
  bool power       = false;
  bool irChannelUp = false;
  bool irChannelDn = false;
  bool irVolUp     = false;
  bool irVolDn     = false;
  bool irMute      = false;
  bool irPower     = false;
};

// ─── Multi-Item Constants ───────────────────────────────────────────────────

#define MAX_SHOWS    10
#define MAX_ITEMS    20
#define SHOW_DIR_LEN 32
#define ITEM_DIR_LEN 32

// ─── Scrolling Text State ───────────────────────────────────────────────────

#define MAX_SCROLL_SLOTS 4   // max simultaneous scrolling text fields per screen

struct ScrollSlot {
  int16_t  offsetPx;      // current pixel offset (0 = start)
  int16_t  maxOffset;     // total overflow in pixels (0 = text fits, no scroll)
  uint32_t lastStepMs;    // millis() of last scroll step
  uint8_t  phase;         // 0=initial pause, 1=scrolling, 2=end pause, 3=reset
  bool     active;        // needs animation
};

struct ScrollState {
  ScrollSlot slots[MAX_SCROLL_SLOTS];
};

// ─── Application Context ─────────────────────────────────────────────────────

struct AppContext {
  AppState currentState;
  AppState nextState;

  // Media type selection
  MediaType currentMediaType;
  bool      mediaTypeAvailable[MEDIA_TYPE_COUNT];
  int       mediaSelectorIndex;
  int       mediaSelectorCount;

  // Show navigation (TV Shows)
  char availableShows[MAX_SHOWS][SHOW_DIR_LEN];
  int availableShowCount;
  int showNavIndex;
  char currentShowDir[SHOW_DIR_LEN];

  // Season/episode navigation
  int currentSeason;
  int currentEpisode;
  int seasonNavIndex;
  int availableSeasons[30];
  int availableSeasonCount;

  // Generic item navigation (Movies, Collections, Artists, Photo Albums)
  char availableItems[MAX_ITEMS][ITEM_DIR_LEN];
  int  availableItemCount;
  int  itemNavIndex;
  char currentItemDir[ITEM_DIR_LEN];

  // Sub-item navigation (Videos, Albums, Tracks, Photos)
  int currentSubItem;       // 1-based
  int subItemCount;
  int subItemNavIndex;

  // Metadata unions - only one set loaded at a time to save RAM
  union {
    ShowMetadata       showMeta;
    MovieMetadata      movieMeta;
    CollectionMetadata         collectionMeta;
    ArtistMetadata             artistMeta;
    YouTubePlaylistMetadata    youtubePlaylistMeta;
  } meta1;

  union {
    SeasonMetadata     seasonMeta;
    MusicAlbumMetadata musicAlbumMeta;
    PhotoAlbumMetadata photoAlbumMeta;
  } meta2;

  union {
    EpisodeMetadata episodeMeta;
    VideoMetadata   videoMeta;
    TrackMetadata          trackMeta;
    PhotoMetadata          photoMeta;
    YouTubeVideoMetadata   youtubeVideoMeta;
  } meta3;

  // UI state
  bool metadataLoaded;
  bool seasonMetaLoaded;
  bool episodeMetaLoaded;

  // Transition
  uint32_t transitionStart;
  int transitionDurationMS;

  // Settings persistence
  int  savedSeason;
  int  savedEpisode;
  int  savedVolume;
  bool settingsDirty;
  uint32_t settingsLastChange;

  // Slideshow
  uint8_t  slideshowIntervalSec;
  uint32_t slideshowLastAdvance;
  int      slideshowCurrentPhoto;

  // Splash screen
  uint32_t splashStartTime;

  // Raw input
  RawInputFlags rawInput;

  // Scrolling text state
  ScrollState scrollState;
};

// ─── Magic Constants ────────────────────────────────────────────────────────

#define SHOW_MAGIC       "SFTV"
#define SEASON_MAGIC     "SFSN"
#define EPISODE_MAGIC    "SFEP"
#define MOVIE_MAGIC      "TJMV"
#define COLLECTION_MAGIC "TJVC"
#define VIDEO_MAGIC      "TJVD"
#define ARTIST_MAGIC     "TJMA"
#define MUSIC_ALBUM_MAGIC "TJAL"
#define TRACK_MAGIC      "TJTK"
#define PHOTO_ALBUM_MAGIC "TJPA"
#define PHOTO_MAGIC      "TJPH"
#define YOUTUBE_PLAYLIST_MAGIC "TJYP"
#define YOUTUBE_VIDEO_MAGIC    "TJYV"

// ─── Media Type Directory Names on SD Card ──────────────────────────────────

#define TV_DIR           "/TV"
#define MOVIES_DIR       "/Movies"
#define MUSIC_VIDEOS_DIR "/MusicVideos"
#define MUSIC_DIR        "/Music"
#define PHOTOS_DIR       "/Photos"
#define YOUTUBE_DIR      "/YouTube"

// ─── UI / Timing Constants ──────────────────────────────────────────────────

#define TRANSITION_STATIC_MS  300
#define SPLASH_DURATION_MS    1500
#define DEFAULT_SLIDESHOW_SEC 5

// Thumbnail dimensions (108x67 for browsers)
#define THUMB_W 108
#define THUMB_H  67
#define THUMB_ROW_BYTES (THUMB_W * 2)

// Fullscreen photo dimensions (210x135)
#define FULLSCREEN_W 210
#define FULLSCREEN_H 135
#define FULLSCREEN_RAW_SIZE (FULLSCREEN_W * FULLSCREEN_H * 2)  // 56,700 bytes

#endif // APPSTATE_H
