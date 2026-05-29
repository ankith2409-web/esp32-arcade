// ============================================================
//  TETRIS for ESP32
//  Display : ST7735 TFT 128x160 (SPI)
//  Input   : Analog joystick (X/Y axes + push button)
//
//  Controls:
//    Joystick Left/Right  → move piece
//    Joystick Down        → soft drop
//    Button               → rotate piece
//    Button (game over)   → restart
//
//  Libraries needed (via Arduino Library Manager):
//    - Adafruit GFX Library
//    - Adafruit ST7735 and ST7789 Library
// ============================================================

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// ── Pin Definitions ──────────────────────────────────────────
#define TFT_CS    5
#define TFT_RST   4
#define TFT_DC    2
#define JOY_X     34
#define JOY_Y     35
#define JOY_BTN   32

// ── Grid & Display Config ────────────────────────────────────
#define CELL      7           // pixels per grid cell
#define COLS      10          // board width  (10 cells)
#define ROWS      20          // board height (20 cells)
#define BOARD_X   0           // board left edge on screen
#define BOARD_Y   0           // board top  edge on screen
#define UI_X      (COLS * CELL + 3)  // UI panel starts at x=73

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ── Tetromino Definitions ────────────────────────────────────
// 7 pieces × 4 rotations × 4 blocks × (x, y) offset
const int8_t PIECES[7][4][4][2] = {
  // I
  {{{0,1},{1,1},{2,1},{3,1}}, {{2,0},{2,1},{2,2},{2,3}},
   {{0,2},{1,2},{2,2},{3,2}}, {{1,0},{1,1},{1,2},{1,3}}},
  // O
  {{{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}},
   {{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}}},
  // T
  {{{1,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{2,1},{1,2}},
   {{0,1},{1,1},{2,1},{1,2}}, {{1,0},{0,1},{1,1},{1,2}}},
  // S
  {{{1,0},{2,0},{0,1},{1,1}}, {{1,0},{1,1},{2,1},{2,2}},
   {{1,1},{2,1},{0,2},{1,2}}, {{0,0},{0,1},{1,1},{1,2}}},
  // Z
  {{{0,0},{1,0},{1,1},{2,1}}, {{2,0},{1,1},{2,1},{1,2}},
   {{0,1},{1,1},{1,2},{2,2}}, {{1,0},{0,1},{1,1},{0,2}}},
  // J
  {{{0,0},{0,1},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{1,2}},
   {{0,1},{1,1},{2,1},{2,2}}, {{1,0},{1,1},{0,2},{1,2}}},
  // L
  {{{2,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{1,2},{2,2}},
   {{0,1},{1,1},{2,1},{0,2}}, {{0,0},{1,0},{1,1},{1,2}}},
};

// Piece colors (16-bit RGB565)
const uint16_t COLORS[7] = {
  0x07FF,  // I — Cyan
  0xFFE0,  // O — Yellow
  0xF81F,  // T — Magenta
  0x07E0,  // S — Green
  0xF800,  // Z — Red
  0x001F,  // J — Blue
  0xFC00,  // L — Orange
};

// ── Game State ────────────────────────────────────────────────
uint8_t board[ROWS][COLS];         // 0 = empty
uint8_t boardColors[ROWS][COLS];   // stored piece color index (1-7)

int      curX, curY;               // active piece origin
int      curPiece, curRot;         // active piece type & rotation
int      nextPiece;                // preview piece

unsigned long score       = 0;
int           level       = 1;
int           linesCleared= 0;
bool          gameOver    = false;

// Timing
unsigned long lastFall  = 0;
unsigned long fallInterval = 600;  // ms — decreases with level

// Joystick debounce timestamps
unsigned long lastMoveH = 0;
unsigned long lastMoveV = 0;
unsigned long lastBtn   = 0;
#define MOVE_DELAY  150   // ms between horizontal moves
#define BTN_DELAY   250   // ms between rotations

// ── Drawing Helpers ───────────────────────────────────────────
void drawCell(int col, int row, uint16_t color) {
  tft.fillRect(BOARD_X + col * CELL, BOARD_Y + row * CELL,
               CELL - 1, CELL - 1, color);
}

void eraseCell(int col, int row) {
  tft.fillRect(BOARD_X + col * CELL, BOARD_Y + row * CELL,
               CELL - 1, CELL - 1, ST77XX_BLACK);
}

void drawPiece(int px, int py, int piece, int rot, bool erase) {
  for (int i = 0; i < 4; i++) {
    int nx = px + PIECES[piece][rot][i][0];
    int ny = py + PIECES[piece][rot][i][1];
    if (ny >= 0) {
      if (erase) eraseCell(nx, ny);
      else       drawCell(nx, ny, COLORS[piece]);
    }
  }
}

// ── Board Validation ──────────────────────────────────────────
bool isValid(int px, int py, int piece, int rot) {
  for (int i = 0; i < 4; i++) {
    int nx = px + PIECES[piece][rot][i][0];
    int ny = py + PIECES[piece][rot][i][1];
    if (nx < 0 || nx >= COLS || ny >= ROWS) return false;
    if (ny >= 0 && board[ny][nx])           return false;
  }
  return true;
}

// ── Lock, Clear Lines, Redraw ─────────────────────────────────
void lockPiece() {
  for (int i = 0; i < 4; i++) {
    int nx = curX + PIECES[curPiece][curRot][i][0];
    int ny = curY + PIECES[curPiece][curRot][i][1];
    if (ny >= 0) {
      board[ny][nx]       = 1;
      boardColors[ny][nx] = curPiece + 1;
    }
  }
}

void redrawBoard() {
  tft.fillRect(BOARD_X, BOARD_Y, COLS * CELL, ROWS * CELL, ST77XX_BLACK);
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      if (board[r][c])
        drawCell(c, r, COLORS[boardColors[r][c] - 1]);
}

void clearLines() {
  int cleared = 0;
  for (int r = ROWS - 1; r >= 0; r--) {
    bool full = true;
    for (int c = 0; c < COLS; c++) if (!board[r][c]) { full = false; break; }
    if (full) {
      cleared++;
      for (int rr = r; rr > 0; rr--)
        for (int c = 0; c < COLS; c++) {
          board[rr][c]       = board[rr-1][c];
          boardColors[rr][c] = boardColors[rr-1][c];
        }
      for (int c = 0; c < COLS; c++) board[0][c] = 0;
      r++;  // re-check same row after shift
    }
  }
  if (cleared > 0) {
    static const unsigned int pts[] = {0, 100, 300, 500, 800};
    score        += (unsigned long)pts[min(cleared, 4)] * level;
    linesCleared += cleared;
    level         = linesCleared / 10 + 1;
    fallInterval  = max(80UL, 600UL - (unsigned long)(level - 1) * 55UL);
    redrawBoard();
    updateScoreUI();
  }
}

// ── UI Panel ──────────────────────────────────────────────────
void drawBorder() {
  tft.drawRect(BOARD_X - 1, BOARD_Y - 1,
               COLS * CELL + 2, ROWS * CELL + 2, 0x632C);
}

void drawNextPiece() {
  int startX = UI_X + 2;
  int startY = 82;
  tft.fillRect(startX - 1, startY - 1, 40, 34, ST77XX_BLACK);
  for (int i = 0; i < 4; i++) {
    int nx = PIECES[nextPiece][0][i][0];
    int ny = PIECES[nextPiece][0][i][1];
    tft.fillRect(startX + nx * 6, startY + ny * 6, 5, 5, COLORS[nextPiece]);
  }
}

void updateScoreUI() {
  tft.fillRect(UI_X, 0, 128 - UI_X, 160, ST77XX_BLACK);

  // Score
  tft.setTextSize(1);
  tft.setTextColor(0x8C71);
  tft.setCursor(UI_X, 4);
  tft.print("SCR");
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(UI_X, 14);
  tft.print(score);

  // Level
  tft.setTextColor(0x8C71);
  tft.setCursor(UI_X, 36);
  tft.print("LVL");
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(UI_X, 46);
  tft.print(level);

  // Lines
  tft.setTextColor(0x8C71);
  tft.setCursor(UI_X, 58);
  tft.print("LNS");
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(UI_X, 68);
  tft.print(linesCleared);

  // Next
  tft.setTextColor(0x8C71);
  tft.setCursor(UI_X, 80);
  tft.print("NXT");
  drawNextPiece();
}

// ── Spawn & Game Over ─────────────────────────────────────────
void spawnPiece() {
  curPiece  = nextPiece;
  nextPiece = random(7);
  curRot    = 0;
  curX      = COLS / 2 - 2;
  curY      = -1;

  if (!isValid(curX, curY, curPiece, curRot))
    gameOver = true;

  updateScoreUI();
}

void showGameOver() {
  int bx = BOARD_X + 2;
  int by = BOARD_Y + 55;
  tft.fillRect(bx, by, COLS * CELL - 4, 50, ST77XX_BLACK);
  tft.drawRect(bx, by, COLS * CELL - 4, 50, 0xF800);

  tft.setTextColor(0xF800);
  tft.setTextSize(1);
  tft.setCursor(bx + 6, by + 6);
  tft.print("GAME OVER");

  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(bx + 4, by + 20);
  tft.print("Score:");
  tft.print(score);

  tft.setTextColor(0x8C71);
  tft.setCursor(bx + 4, by + 34);
  tft.print("Btn to retry");
}

void showSplash() {
  tft.fillScreen(ST77XX_BLACK);

  // Title
  tft.setTextSize(2);
  tft.setTextColor(0x07FF);
  tft.setCursor(16, 40);
  tft.print("TETRIS");

  // Decorative mini blocks
  const uint16_t demoColors[] = {0xF800, 0x07E0, 0xFFE0, 0xF81F, 0x07FF};
  for (int i = 0; i < 5; i++)
    tft.fillRect(14 + i * 14, 65, 10, 10, demoColors[i]);

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(8, 90);
  tft.print("Press BTN to start");

  tft.setTextColor(0x632C);
  tft.setCursor(14, 108);
  tft.print("BTN = Rotate");
  tft.setCursor(14, 118);
  tft.print("Down = Soft Drop");
}

// ── Init ──────────────────────────────────────────────────────
void initGame() {
  memset(board,       0, sizeof(board));
  memset(boardColors, 0, sizeof(boardColors));
  score        = 0;
  level        = 1;
  linesCleared = 0;
  fallInterval = 600;
  gameOver     = false;
  nextPiece    = random(7);

  tft.fillScreen(ST77XX_BLACK);
  drawBorder();
  spawnPiece();
  updateScoreUI();
}

// ── Arduino Entry Points ──────────────────────────────────────
void setup() {
  pinMode(JOY_BTN, INPUT_PULLUP);
  randomSeed(analogRead(0));

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);

  showSplash();

  // Wait for button press to start
  while (digitalRead(JOY_BTN) == HIGH) delay(30);
  delay(200);

  initGame();
}

void loop() {
  // ── Game Over: wait for restart ──────────────────────────────
  if (gameOver) {
    if (digitalRead(JOY_BTN) == LOW) {
      delay(200);
      initGame();
    }
    return;
  }

  unsigned long now = millis();

  // ── Move Left / Right ─────────────────────────────────────────
  int jx = analogRead(JOY_X);
  if (now - lastMoveH > MOVE_DELAY) {
    if (jx < 1200) {        // joystick pushed left
      drawPiece(curX, curY, curPiece, curRot, true);
      if (isValid(curX - 1, curY, curPiece, curRot)) curX--;
      drawPiece(curX, curY, curPiece, curRot, false);
      lastMoveH = now;
    } else if (jx > 2900) { // joystick pushed right
      drawPiece(curX, curY, curPiece, curRot, true);
      if (isValid(curX + 1, curY, curPiece, curRot)) curX++;
      drawPiece(curX, curY, curPiece, curRot, false);
      lastMoveH = now;
    }
  }

  // ── Soft Drop ─────────────────────────────────────────────────
  int jy = analogRead(JOY_Y);
  unsigned long effectiveDrop = (jy > 2900) ? 60UL : fallInterval;

  // ── Rotate (Button) ───────────────────────────────────────────
  if (digitalRead(JOY_BTN) == LOW && now - lastBtn > BTN_DELAY) {
    int newRot = (curRot + 1) % 4;
    // Wall kick: try offset +1 if rotation clips the wall
    int kick = 0;
    if (!isValid(curX, curY, curPiece, newRot)) {
      if      (isValid(curX + 1, curY, curPiece, newRot)) kick =  1;
      else if (isValid(curX - 1, curY, curPiece, newRot)) kick = -1;
    }
    if (isValid(curX + kick, curY, curPiece, newRot)) {
      drawPiece(curX, curY, curPiece, curRot, true);
      curX  += kick;
      curRot = newRot;
      drawPiece(curX, curY, curPiece, curRot, false);
    }
    lastBtn = now;
  }

  // ── Gravity ───────────────────────────────────────────────────
  if (now - lastFall > effectiveDrop) {
    drawPiece(curX, curY, curPiece, curRot, true);

    if (isValid(curX, curY + 1, curPiece, curRot)) {
      curY++;
      drawPiece(curX, curY, curPiece, curRot, false);
    } else {
      // Piece has landed
      drawPiece(curX, curY, curPiece, curRot, false);
      lockPiece();
      clearLines();
      spawnPiece();
      if (gameOver) {
        showGameOver();
      } else {
        drawPiece(curX, curY, curPiece, curRot, false);
      }
    }

    lastFall = now;
  }
}
