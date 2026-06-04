// ============================================================
//  WHACK-A-MOLE for ESP32
//  Display : ST7735 TFT 128x160 (SPI)
//  Input   : Analog joystick (X/Y axes + push button)
//
//  Controls:
//    Joystick   → move cursor between holes
//    Button     → WHACK the selected hole
//
//  Libraries needed (via Arduino Library Manager):
//    - Adafruit GFX Library
//    - Adafruit ST7735 and ST7789 Library
// ============================================================

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

#define TFT_CS   5
#define TFT_RST  4
#define TFT_DC   2
#define JOY_X    34
#define JOY_Y    35
#define JOY_BTN  32

#define SCR_W  128
#define SCR_H  160

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

#define GRID_COLS  3
#define GRID_ROWS  3
#define NUM_HOLES  9
#define HUD_H      14
#define CELL_W     (SCR_W / GRID_COLS)
#define CELL_H     ((SCR_H - HUD_H) / GRID_ROWS)

#define HOLE_W  28
#define HOLE_H  14
#define MOLE_R  13

#define COL_GRASS   0x2D05
#define COL_HOLE    0x1800
#define COL_MOLE    0xC460
#define COL_NOSE    0xF0A0
#define COL_HIT     0xFC00
#define COL_CURSOR  0xFFE0

int  cursorCol = 1, cursorRow = 1;
bool          moleUp[NUM_HOLES];
unsigned long moleExpiry[NUM_HOLES];
int  score = 0, misses = 0;
bool gameOver = false;

#define GAME_DURATION  60000UL
unsigned long gameStart;
unsigned long lastSpawn = 0, lastJoyMove = 0, lastBtn = 0;
#define JOY_DELAY  190
#define BTN_DELAY  160

int holeCX(int idx) { return (idx % GRID_COLS) * CELL_W + CELL_W / 2; }
int holeCY(int idx) { return HUD_H + (idx / GRID_COLS) * CELL_H + CELL_H / 2; }

void drawHole(int idx, bool hasMole, bool selected, bool hitFlash) {
  int cx = holeCX(idx), cy = holeCY(idx);
  int cellX = (idx % GRID_COLS) * CELL_W;
  int cellY = HUD_H + (idx / GRID_COLS) * CELL_H;

  tft.fillRect(cellX, cellY, CELL_W, CELL_H, COL_GRASS);
  tft.fillRoundRect(cx - HOLE_W/2 - 2, cy + 4, HOLE_W + 4, HOLE_H + 4, 6, 0x3000);
  tft.fillRoundRect(cx - HOLE_W/2, cy + 6, HOLE_W, HOLE_H, 7, COL_HOLE);

  if (hasMole) {
    uint16_t bodyCol = hitFlash ? COL_HIT : COL_MOLE;
    tft.fillCircle(cx, cy - 2, MOLE_R, bodyCol);
    tft.fillCircle(cx - 10, cy - 12, 5, bodyCol);
    tft.fillCircle(cx + 10, cy - 12, 5, bodyCol);
    tft.fillCircle(cx - 10, cy - 12, 3, 0xFAAD);
    tft.fillCircle(cx + 10, cy - 12, 3, 0xFAAD);
    tft.fillCircle(cx - 5, cy - 5, 3, ST77XX_WHITE);
    tft.fillCircle(cx + 5, cy - 5, 3, ST77XX_WHITE);
    tft.fillCircle(cx - 5, cy - 5, 2, ST77XX_BLACK);
    tft.fillCircle(cx + 5, cy - 5, 2, ST77XX_BLACK);
    tft.fillCircle(cx - 4, cy - 6, 1, ST77XX_WHITE);
    tft.fillCircle(cx + 6, cy - 6, 1, ST77XX_WHITE);
    tft.fillRoundRect(cx - 3, cy, 7, 5, 2, COL_NOSE);
    tft.drawLine(cx - 3, cy + 2, cx - 12, cy,     0x8C71);
    tft.drawLine(cx - 3, cy + 3, cx - 12, cy + 4, 0x8C71);
    tft.drawLine(cx + 4, cy + 2, cx + 13, cy,     0x8C71);
    tft.drawLine(cx + 4, cy + 3, cx + 13, cy + 4, 0x8C71);
    tft.fillRect(cx - 3, cy + 5, 3, 4, ST77XX_WHITE);
    tft.fillRect(cx + 1, cy + 5, 3, 4, ST77XX_WHITE);
  }

  if (selected) {
    tft.drawRoundRect(cellX + 1, cellY + 1, CELL_W - 2, CELL_H - 2, 4, COL_CURSOR);
    tft.drawRoundRect(cellX + 2, cellY + 2, CELL_W - 4, CELL_H - 4, 4, COL_CURSOR);
  }
}

void drawAllHoles() {
  for (int i = 0; i < NUM_HOLES; i++)
    drawHole(i, moleUp[i], i == cursorRow * GRID_COLS + cursorCol, false);
}

void updateHUD() {
  tft.fillRect(0, 0, SCR_W, HUD_H, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(2, 3); tft.print("HIT:"); tft.print(score);
  tft.setTextColor(0xF800);
  tft.setCursor(52, 3); tft.print("X:"); tft.print(misses);
  unsigned long now = millis();
  long remaining = (long)((gameStart + GAME_DURATION) - now) / 1000;
  if (remaining < 0) remaining = 0;
  tft.setTextColor(remaining <= 10 ? 0xF800 : 0x8C71);
  tft.setCursor(90, 3);
  if (remaining < 10) tft.print("0");
  tft.print(remaining); tft.print("s");
}

void trySpawnMole() {
  int upCount = 0;
  for (int i = 0; i < NUM_HOLES; i++) if (moleUp[i]) upCount++;
  int maxUp = min(4, 1 + score / 4);
  if (upCount >= maxUp) return;
  for (int attempt = 0; attempt < 15; attempt++) {
    int idx = random(NUM_HOLES);
    if (!moleUp[idx]) {
      moleUp[idx] = true;
      int duration = max(700, 2400 - score * 35);
      moleExpiry[idx] = millis() + duration;
      bool sel = (idx == cursorRow * GRID_COLS + cursorCol);
      drawHole(idx, true, sel, false);
      return;
    }
  }
}

void showGameOver() {
  for (int i = 0; i < NUM_HOLES; i++) { moleUp[i] = false; drawHole(i, false, false, false); }
  tft.fillRect(8, 46, SCR_W - 16, 82, ST77XX_BLACK);
  tft.drawRect(8, 46, SCR_W - 16, 82, 0xFFE0);
  tft.setTextColor(0xFFE0); tft.setTextSize(1);
  tft.setCursor(22, 54); tft.print("TIME'S UP!");
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(16, 68); tft.print("Hits  : "); tft.print(score);
  tft.setCursor(16, 80); tft.print("Misses: "); tft.print(misses);
  tft.setTextColor(0x07FF);
  tft.setCursor(16, 94);
  if      (score >= 25) tft.print("Rating: LEGENDARY");
  else if (score >= 15) tft.print("Rating: PRO WHACKER");
  else if (score >= 8)  tft.print("Rating: NOT BAD!");
  else                  tft.print("Rating: Keep trying");
  tft.setTextColor(0x8C71);
  tft.setCursor(22, 112); tft.print("BTN to replay");
}

void showSplash() {
  tft.fillScreen(COL_GRASS);
  tft.fillRoundRect(16, 56, 28, 14, 7, COL_HOLE);
  tft.fillCircle(30, 44, 13, COL_MOLE);
  tft.fillCircle(25, 40, 3, ST77XX_WHITE); tft.fillCircle(25, 40, 2, ST77XX_BLACK);
  tft.fillCircle(35, 40, 3, ST77XX_WHITE); tft.fillCircle(35, 40, 2, ST77XX_BLACK);
  tft.fillRoundRect(27, 46, 7, 5, 2, COL_NOSE);
  tft.fillRect(27, 51, 3, 4, ST77XX_WHITE);
  tft.fillRect(31, 51, 3, 4, ST77XX_WHITE);
  tft.fillRoundRect(84, 60, 28, 14, 7, COL_HOLE);
  tft.fillCircle(98, 52, 10, COL_MOLE);
  tft.fillCircle(94, 49, 2, ST77XX_WHITE); tft.fillCircle(94, 49, 1, ST77XX_BLACK);
  tft.fillCircle(102, 49, 2, ST77XX_WHITE); tft.fillCircle(102, 49, 1, ST77XX_BLACK);
  tft.setTextSize(1); tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(6, 82); tft.print("WHACK-A-MOLE");
  tft.setTextColor(0xFFE0);
  tft.setCursor(6, 96); tft.print("60 secs, whack em!");
  tft.setTextColor(0x8C71);
  tft.setCursor(6, 110); tft.print("Joystick = aim");
  tft.setCursor(6, 122); tft.print("Button   = whack!");
  tft.setTextColor(COL_CURSOR);
  tft.setCursor(18, 145); tft.print("Press BTN to play");
}

void initGame() {
  score = 0; misses = 0; gameOver = false;
  cursorCol = 1; cursorRow = 1;
  for (int i = 0; i < NUM_HOLES; i++) moleUp[i] = false;
  gameStart = millis(); lastSpawn = millis();
  tft.fillScreen(COL_GRASS);
  drawAllHoles(); updateHUD();
}

void setup() {
  pinMode(JOY_BTN, INPUT_PULLUP);
  randomSeed(analogRead(0));
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);
  showSplash();
  while (digitalRead(JOY_BTN) == HIGH) delay(30);
  delay(200);
  initGame();
}

void loop() {
  if (gameOver) {
    if (digitalRead(JOY_BTN) == LOW) { delay(200); initGame(); }
    return;
  }

  unsigned long now = millis();

  if (now >= gameStart + GAME_DURATION) { gameOver = true; showGameOver(); return; }

  static unsigned long lastHUD = 0;
  if (now - lastHUD >= 500) { updateHUD(); lastHUD = now; }

  unsigned long spawnInterval = max(500UL, 1100UL - (unsigned long)score * 18UL);
  if (now - lastSpawn > spawnInterval) { trySpawnMole(); lastSpawn = now; }

  for (int i = 0; i < NUM_HOLES; i++) {
    if (moleUp[i] && now > moleExpiry[i]) {
      moleUp[i] = false; misses++;
      bool sel = (i == cursorRow * GRID_COLS + cursorCol);
      drawHole(i, false, sel, false);
    }
  }

  if (now - lastJoyMove > JOY_DELAY) {
    int jx = analogRead(JOY_X), jy = analogRead(JOY_Y);
    int oc = cursorCol, or_ = cursorRow;
    if      (jx < 1200 && cursorCol > 0)             cursorCol--;
    else if (jx > 2900 && cursorCol < GRID_COLS - 1) cursorCol++;
    else if (jy < 1200 && cursorRow > 0)             cursorRow--;
    else if (jy > 2900 && cursorRow < GRID_ROWS - 1) cursorRow++;
    if (cursorCol != oc || cursorRow != or_) {
      drawHole(or_ * GRID_COLS + oc,          moleUp[or_ * GRID_COLS + oc],         false, false);
      drawHole(cursorRow * GRID_COLS + cursorCol, moleUp[cursorRow * GRID_COLS + cursorCol], true, false);
      lastJoyMove = now;
    }
  }

  if (digitalRead(JOY_BTN) == LOW && now - lastBtn > BTN_DELAY) {
    int idx = cursorRow * GRID_COLS + cursorCol;
    if (moleUp[idx]) {
      score++; moleUp[idx] = false;
      drawHole(idx, true,  true, true);
      delay(100);
      drawHole(idx, false, true, false);
      updateHUD();
    } else {
      int cx = (idx % GRID_COLS) * CELL_W;
      int cy = HUD_H + (idx / GRID_COLS) * CELL_H;
      tft.drawRoundRect(cx+1, cy+1, CELL_W-2, CELL_H-2, 4, 0xF800);
      delay(80);
      drawHole(idx, false, true, false);
    }
    lastBtn = now;
  }
}
