//-------------------------------------------------------------------------------
//  TinyJukebox - UI Drawing Helpers & Color Constants
//
//  Shared color definitions and framebuffer drawing primitives used by
//  all browser and menu screens. Included by showBrowser.ino and other
//  UI .ino files.
//-------------------------------------------------------------------------------

#ifndef UI_HELPERS_H
#define UI_HELPERS_H

#include <stdint.h>
#include <string.h>

// ─── Color Constants (RGB565 for display) ───────────────────────────────────

#define COL_BLACK     0x0000
#define COL_WHITE     0xFFFF
#define COL_YELLOW    0xFFE0
#define COL_GRAY_DK   0x4208
#define COL_GRAY_MED  0x8410
#define COL_GRAY_LT   0xC618
#define COL_BG        0x0000   // Black background

// ─── Framebuffer Drawing Helpers ────────────────────────────────────────────
// These are defined as inline so they can live in a header included by
// multiple .ino files (which Arduino concatenates into one translation unit).

static inline uint16_t rgb565BE(uint8_t r, uint8_t g, uint8_t b) {
  uint16_t val = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  return val;
}

static inline void clearFrameBuf() {
  extern uint16_t frameBuf[];
  memset(frameBuf, 0, VIDEO_W * VIDEO_H * 2);
}

static inline void fillRect(int x, int y, int w, int h, uint16_t color) {
  extern uint16_t frameBuf[];
  for (int row = y; row < y + h && row < VIDEO_H; row++) {
    if (row < 0) continue;
    for (int col = x; col < x + w && col < VIDEO_W; col++) {
      if (col < 0) continue;
      frameBuf[row * VIDEO_W + col] = color;
    }
  }
}

static inline void drawHLine(int x, int y, int w, uint16_t color) {
  extern uint16_t frameBuf[];
  if (y < 0 || y >= VIDEO_H) return;
  for (int col = x; col < x + w && col < VIDEO_W; col++) {
    if (col >= 0) frameBuf[y * VIDEO_W + col] = color;
  }
}

// Draw text using GraphicsBuffer2 into frameBuf
static inline void drawText(const char* text, int x, int y, uint16_t color) {
  extern uint16_t frameBuf[];
  extern GraphicsBuffer2 screenBuffer;
  screenBuffer.setBuffer((uint8_t*)frameBuf);
  screenBuffer.setWidth(VIDEO_W);
  screenBuffer.fontColor(color, COL_BG);
  screenBuffer.setCursor(x, y);
  screenBuffer.print(text);
}

// Measure text width (approximate - 8px per char for liberationSansNarrow_14pt)
static inline int textWidth(const char* text) {
  return strlen(text) * 8;
}

// ─── Icon Drawing ───────────────────────────────────────────────────────────
// Draw a PROGMEM RGB565 icon to frameBuf. Black (0x0000) pixels are transparent.

static inline void drawIcon(const uint16_t* iconData, int x, int y, int w, int h) {
  extern uint16_t frameBuf[];
  for (int row = 0; row < h; row++) {
    int fy = y + row;
    if (fy < 0 || fy >= VIDEO_H) continue;
    for (int col = 0; col < w; col++) {
      int fx = x + col;
      if (fx < 0 || fx >= VIDEO_W) continue;
      uint16_t pixel = pgm_read_word(&iconData[row * w + col]);
      if (pixel != 0x0000) {
        frameBuf[fy * VIDEO_W + fx] = pixel;
      }
    }
  }
}

// ─── Scrolling Text Support ─────────────────────────────────────────────────
// ScrollSlot and ScrollState structs are defined in appState.h.
// resetScrollState() and updateScrollState() are defined in TinyJukebox.ino
// (they don't depend on GraphicsBuffer2 so can live outside this header).

// Forward declarations for functions defined in TinyJukebox.ino
void resetScrollState(ScrollState* ss);
bool updateScrollState(ScrollState* ss, uint32_t nowMs);

// Draw text clipped to maxWidth pixels, scrolled by slot->offsetPx.
// If text fits in maxWidth, draws normally. Otherwise enables scrolling.
static inline void drawScrollText(const char* text, int x, int y, int maxWidth,
                                   uint16_t color, ScrollSlot* slot) {
  extern uint16_t frameBuf[];
  extern GraphicsBuffer2 screenBuffer;

  int tw = textWidth(text);

  if (tw <= maxWidth) {
    // Text fits — draw normally, no scrolling needed
    drawText(text, x, y, color);
    slot->maxOffset = 0;
    slot->active = false;
    return;
  }

  // Text overflows — set up scroll parameters
  int overflow = tw - maxWidth;
  if (slot->maxOffset != overflow) {
    // First call or text changed — initialize
    slot->maxOffset = overflow;
    if (slot->phase == 0 && slot->offsetPx == 0 && slot->lastStepMs == 0) {
      slot->lastStepMs = millis();
    }
    slot->active = true;
  }

  // Draw text shifted left by offsetPx, then black out only where text leaked
  int drawX = x - slot->offsetPx;
  drawText(text, drawX, y, color);

  // Only black out the narrow strips where text actually overflowed the
  // clip region [x, x+maxWidth]. Don't touch anything outside the text area
  // (that would destroy thumbnails and other UI elements on the same row).
  int textH = 18;  // liberationSansNarrow_14pt character height
  int textLeft = (drawX > 0) ? drawX : 0;
  int textRight = drawX + tw;
  if (textRight > VIDEO_W) textRight = VIDEO_W;

  // Left overflow: text pixels drawn between textLeft and x
  if (textLeft < x) {
    fillRect(textLeft, y, x - textLeft, textH, COL_BG);
  }
  // Right overflow: text pixels drawn between x+maxWidth and textRight
  int rightEdge = x + maxWidth;
  if (textRight > rightEdge) {
    fillRect(rightEdge, y, textRight - rightEdge, textH, COL_BG);
  }
}

#endif // UI_HELPERS_H
