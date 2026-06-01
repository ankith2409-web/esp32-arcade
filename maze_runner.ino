// ============================================================
//  MAZE RUNNER for ESP32
//  Display : ST7735 TFT 128x160 (SPI)
//  Input   : Analog joystick (X/Y axes + push button)
//
//  Controls:
//    Joystick        → move player
//    Button          → start / next level / restart
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

#define MAZE_COLS  12
#define MAZE_ROWS  14
#define UNIT       5
#define GRID_W     (2 * MAZE_COLS + 1)
#define GRID_H     (2 * MAZE_ROWS + 1)
#define MAZE_X     2
#define MAZE_Y     14

#define EXIT_COL  (MAZE_COLS - 1)
#define EXIT_ROW  (MAZE_ROWS - 1)

#define COL_WALL    0x4228
#define COL_PATH    ST77XX_BLACK
#define COL_PLAYER  0x07FF
#define COL_EXIT    0x07E0
#define COL_TRAIL   0x2945

uint8_t grid[GRID_H][GRID_W];
bool    visited[MAZE_ROWS][MAZE_COLS];
bool    trailMap[MAZE_ROWS][MAZE_COLS];

int playerCol, playerRow;
int level = 1;
unsigned long startTime;
bool gameWon = false;

struct Cell { int8_t r, c; };
Cell dfsStack[MAZE_ROWS * MAZE_COLS];
int  stackTop;

void generateMaze() {
  for (int r = 0; r < GRID_H; r++)
    for (int c = 0; c < GRID_W; c++)
      grid[r][c] = 1;

  for (int r = 0; r < MAZE_ROWS; r++)
    for (int c = 0; c < MAZE_COLS; c++) {
      grid[2*r+1][2*c+1] = 0;
      visited[r][c]  = false;
      trailMap[r][c] = false;
    }

  const int8_t dr[] = {-1, 0, 1,  0};
  const int8_t dc[] = { 0, 1, 0, -1};

  stackTop = 0;
  dfsStack[stackTop++] = {0, 0};
  visited[0][0] = true;

  while (stackTop > 0) {
    Cell cur = dfsStack[stackTop - 1];
    int8_t nb[4];
    int nCount = 0;
    for (int d = 0; d < 4; d++) {
      int nr = cur.r + dr[d];
      int nc = cur.c + dc[d];
      if (nr >= 0 && nr < MAZE_ROWS &&
          nc >= 0 && nc < MAZE_COLS &&
          !visited[nr][nc])
        nb[nCount++] = d;
    }
    if (nCount == 0) {
      stackTop--;
    } else {
      int d = nb[random(nCount)];
      int nr = cur.r + dr[d];
      int nc = cur.c + dc[d];
      grid[2*cur.r+1 + dr[d]][2*cur.c+1 + dc[d]] = 0;
      visited[nr][nc] = true;
      dfsStack[stackTop++] = {(int8_t)nr, (int8_t)nc};
    }
  }
}

bool canMove(int fromCol, int fromRow, int toCol, int toRow) {
  if (toCol < 0 || toCol >= MAZE_COLS) return false;
  if (toRow < 0 || toRow >= MAZE_ROWS) return false;
  int wallR = fromRow + toRow + 1;
  int wallC = fromCol + toCol + 1;
  return grid[wallR][wallC] == 0;
}

inline int cellPX(int col) { return MAZE_X + (2*col+1) * UNIT; }
inline int cellPY(int row) { return MAZE_Y + (2*row+1) * UNIT; }

void drawCell(int col, int row, uint16_t color) {
  tft.fillRect(cellPX(col) + 1, cellPY(row) + 1, UNIT - 2, UNIT - 2, color);
}

void drawMaze() {
  tft.fillRect(MAZE_X, MAZE_Y, GRID_W * UNIT, GRID_H * UNIT, COL_WALL);
  for (int gr = 0; gr < GRID_H; gr++)
    for (int gc = 0; gc < GRID_W; gc++)
      if (grid[gr][gc] == 0)
        tft.fillRect(MAZE_X + gc*UNIT, MAZE_Y + gr*UNIT, UNIT, UNIT, COL_PATH);
  drawCell(EXIT_COL, EXIT_ROW, COL_EXIT);
}

void updateHUD() {
  tft.fillRect(0, 0, SCR_W, 13, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(2, 3);
  tft.print("LVL:"); tft.print(level);
  unsigned long sec = (millis() - startTime) / 1000;
  tft.setCursor(50, 3);
  tft.print("T:");
  if (sec < 10) tft.print("0");
  tft.print(sec); tft.print("s");
  tft.setTextColor(0x632C);
  tft.setCursor(100, 3); tft.print("EXIT");
}

void initLevel() {
  gameWon = false; playerCol = 0; playerRow = 0;
  startTime = millis();
  generateMaze();
  tft.fillScreen(ST77XX_BLACK);
  drawMaze();
  drawCell(playerCol, playerRow, COL_PLAYER);
  updateHUD();
}

void initGame() { level = 1; initLevel(); }

void showWin() {
  unsigned long sec = (millis() - startTime) / 1000;
  bool finalLevel = (level >= 5);
  tft.fillRect(8, 48, SCR_W - 16, 78, ST77XX_BLACK);
  tft.drawRect(8, 48, SCR_W - 16, 78, finalLevel ? 0xFFE0 : COL_EXIT);
  tft.setTextSize(1);
  if (finalLevel) {
    tft.setTextColor(0xFFE0); tft.setCursor(20, 56); tft.print("YOU ESCAPED!");
  } else {
    tft.setTextColor(COL_EXIT); tft.setCursor(18, 56); tft.print("LEVEL CLEAR!");
  }
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(16, 72); tft.print("Time : "); tft.print(sec); tft.print("s");
  tft.setCursor(16, 84); tft.print("Level: "); tft.print(level); tft.print(" / 5");
  tft.setTextColor(0x8C71);
  tft.setCursor(16, 100);
  if (finalLevel) tft.print("BTN: play again");
  else            tft.print("BTN: next level");
}

void showSplash() {
  tft.fillScreen(ST77XX_BLACK);
  const uint8_t demo[5][9] = {
    {1,1,1,1,1,1,1,1,1},
    {1,0,0,0,1,0,0,0,1},
    {1,0,1,0,0,0,1,0,1},
    {1,0,1,1,1,0,1,0,1},
    {1,1,1,1,1,1,1,1,1},
  };
  for (int r = 0; r < 5; r++)
    for (int c = 0; c < 9; c++)
      tft.fillRect(30 + c*6, 12 + r*6, 5, 5, demo[r][c] ? COL_WALL : COL_PATH);
  tft.fillRect(36, 18, 4, 4, COL_PLAYER);
  tft.fillRect(78, 30, 4, 4, COL_EXIT);
  tft.setTextSize(2); tft.setTextColor(COL_PLAYER);
  tft.setCursor(10, 50); tft.print("MAZE RUN");
  tft.setTextSize(1); tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(12, 76); tft.print("Find the");
  tft.setTextColor(COL_EXIT); tft.print(" green");
  tft.setTextColor(ST77XX_WHITE); tft.print(" exit");
  tft.setCursor(12, 88); tft.print("5 levels - go fast!");
  tft.setTextColor(0x8C71); tft.setCursor(12, 104); tft.print("Joystick = move");
  tft.setTextColor(COL_PLAYER); tft.setCursor(22, 140); tft.print("Press BTN to play");
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

unsigned long lastMove    = 0;
unsigned long lastHUDTick = 0;
#define MOVE_DELAY  160

void loop() {
  if (gameWon) {
    if (digitalRead(JOY_BTN) == LOW) {
      delay(200);
      if (level >= 5) { level = 1; initLevel(); }
      else            { level++;   initLevel(); }
    }
    return;
  }

  unsigned long now = millis();

  if (now - lastHUDTick >= 1000) { updateHUD(); lastHUDTick = now; }
  if (now - lastMove < MOVE_DELAY) return;

  int jx = analogRead(JOY_X);
  int jy = analogRead(JOY_Y);
  int newCol = playerCol;
  int newRow = playerRow;

  if      (jx < 1200) newCol--;
  else if (jx > 2900) newCol++;
  else if (jy < 1200) newRow--;
  else if (jy > 2900) newRow++;
  else return;

  if (newCol == playerCol && newRow == playerRow) return;

  if (!canMove(playerCol, playerRow, newCol, newRow)) {
    lastMove = now; return;
  }

  drawCell(playerCol, playerRow, trailMap[playerRow][playerCol] ? COL_TRAIL : COL_PATH);
  trailMap[playerRow][playerCol] = true;
  playerCol = newCol;
  playerRow = newRow;
  lastMove  = now;

  if (playerCol == EXIT_COL && playerRow == EXIT_ROW) {
    drawCell(playerCol, playerRow, 0xFFE0);
    delay(150);
    gameWon = true;
    showWin();
    return;
  }

  drawCell(playerCol, playerRow, COL_PLAYER);
}
