#include <Arduino.h>
// Tiny Gaming Console - ESP32 + KY-023 Joystick + ST7789 2" TFT
// Games: Snake (select with joystick button)
// Navigation: joystick X/Y for direction, press to start/restart

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

// ── Pin Definitions ──────────────────────────────────────────────
#define TFT_SCK   18
#define TFT_MOSI  23
#define TFT_CS    5
#define TFT_DC    4
#define TFT_RST   13
#define TFT_BL    14

#define JOY_VRX   25
#define JOY_VRY   26
#define JOY_SW    16

// ── Display ───────────────────────────────────────────────────────

// Hoisted type definitions
enum GameState { STATE_MENU, STATE_PLAYING, STATE_GAMEOVER };

struct Point { int8_t x; int8_t y; };

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCK, TFT_RST);

// ── Display dimensions ────────────────────────────────────────────
#define SCREEN_W  320
#define SCREEN_H  240

// ── Colors ────────────────────────────────────────────────────────
#define C_BLACK   ST77XX_BLACK
#define C_WHITE   ST77XX_WHITE
#define C_GREEN   ST77XX_GREEN
#define C_RED     ST77XX_RED
#define C_YELLOW  ST77XX_YELLOW
#define C_BLUE    ST77XX_BLUE
#define C_CYAN    ST77XX_CYAN
#define C_MAGENTA ST77XX_MAGENTA
#define C_ORANGE  0xFD20   // 16-bit orange

// ── Snake Game Config ─────────────────────────────────────────────
#define CELL      10                      // cell size in pixels
#define COLS      (SCREEN_W / CELL)       // 32
#define ROWS      ((SCREEN_H - 20) / CELL)// 22 (top 20 px = scorebar)
#define TOP_OFFSET 20
#define MAX_LEN   (COLS * ROWS)

// ── Joystick thresholds ───────────────────────────────────────────
#define JOY_CENTER_LOW  1500
#define JOY_CENTER_HIGH 2600
#define JOY_DEAD_LOW    1700
#define JOY_DEAD_HIGH   2400

// ── Game state ────────────────────────────────────────────────────

GameState gState = STATE_MENU;

// ── Snake data ────────────────────────────────────────────────────


Point snake[MAX_LEN];
int   snakeLen = 0;
Point food;
int8_t dirX = 1, dirY = 0;   // current direction
int8_t nextDirX = 1, nextDirY = 0;
uint32_t lastMove   = 0;
uint16_t moveDelay  = 140;    // ms between moves
uint32_t score      = 0;
uint32_t highScore  = 0;

// ── Button debounce ───────────────────────────────────────────────
bool      btnLastState  = HIGH;
uint32_t  btnDebounce   = 0;
bool      btnPressed    = false; // single-event flag

// ── Forward helpers (bodies below) ───────────────────────────────
void drawMenu();
void startGame();
void drawScorebar();
void drawCell(int8_t x, int8_t y, uint16_t color);
void placeFood();
void gameUpdate();
void drawGameOver();
void readJoystick();

// ═════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);

  // Backlight ON
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // Joystick button
  pinMode(JOY_SW, INPUT_PULLUP);

  // ADC resolution
  analogReadResolution(12); // 0-4095

  // Init display (320x240, using init with width/height)
  tft.init(240, 320);
  tft.setRotation(1); // landscape
  tft.fillScreen(C_BLACK);

  randomSeed(analogRead(35)); // floating pin for seed

  drawMenu();
}

// ═════════════════════════════════════════════════════════════════
void loop() {
  readJoystick(); // updates btnPressed and direction

  switch (gState) {
    case STATE_MENU:
      if (btnPressed) {
        btnPressed = false;
        startGame();
      }
      break;

    case STATE_PLAYING:
      gameUpdate();
      break;

    case STATE_GAMEOVER:
      if (btnPressed) {
        btnPressed = false;
        drawMenu();
        gState = STATE_MENU;
      }
      break;
  }
}

// ── Read joystick + button ─────────────────────────────────────────
void readJoystick() {
  // ── Direction (only while playing) ─────────────────────────────
  int vx = analogRead(JOY_VRX); // 0-4095
  int vy = analogRead(JOY_VRY);

  if (gState == STATE_PLAYING) {
    if (vx < JOY_DEAD_LOW  && !(dirX == 1)) { nextDirX = -1; nextDirY =  0; }
    else if (vx > JOY_DEAD_HIGH && !(dirX == -1)) { nextDirX =  1; nextDirY =  0; }
    else if (vy < JOY_DEAD_LOW  && !(dirY == 1)) { nextDirX =  0; nextDirY = -1; }
    else if (vy > JOY_DEAD_HIGH && !(dirY == -1)) { nextDirX =  0; nextDirY =  1; }
  }

  // ── Button debounce ─────────────────────────────────────────────
  bool curState = digitalRead(JOY_SW);
  if (curState == LOW && btnLastState == HIGH) {
    uint32_t now = millis();
    if (now - btnDebounce > 200) {
      btnDebounce = now;
      btnPressed  = true;
    }
  }
  btnLastState = curState;
}

// ── Menu screen ───────────────────────────────────────────────────
void drawMenu() {
  tft.fillScreen(C_BLACK);
  tft.setTextColor(C_GREEN);
  tft.setTextSize(3);
  tft.setCursor(80, 40);
  tft.print("SNAKE");

  tft.setTextColor(C_WHITE);
  tft.setTextSize(1);
  tft.setCursor(70, 100);
  tft.print("Use joystick to steer");
  tft.setCursor(70, 120);
  tft.print("Press button to START");

  if (highScore > 0) {
    tft.setTextColor(C_YELLOW);
    tft.setTextSize(1);
    tft.setCursor(100, 160);
    tft.print("High Score: ");
    tft.print(highScore);
  }

  // Simple snake logo
  uint16_t colors[] = {C_GREEN, C_CYAN, C_GREEN, C_CYAN, C_GREEN};
  for (int i = 0; i < 5; i++) {
    tft.fillRect(60 + i * 12, 200, 10, 10, colors[i]);
  }
  tft.fillRect(60 + 5 * 12, 200, 10, 10, C_RED); // food

  gState = STATE_MENU;
}

// ── Start/restart game ────────────────────────────────────────────
void startGame() {
  tft.fillScreen(C_BLACK);

  // Init snake at center
  snakeLen = 4;
  for (int i = 0; i < snakeLen; i++) {
    snake[i].x = (COLS / 2) - i;
    snake[i].y = ROWS / 2;
  }
  dirX = 1; dirY = 0;
  nextDirX = 1; nextDirY = 0;
  score     = 0;
  moveDelay = 140;

  // Draw grid border
  tft.drawRect(0, TOP_OFFSET, SCREEN_W, SCREEN_H - TOP_OFFSET, C_WHITE);

  // Draw initial snake
  for (int i = 0; i < snakeLen; i++) {
    drawCell(snake[i].x, snake[i].y, C_GREEN);
  }

  placeFood();
  drawScorebar();
  lastMove = millis();
  gState = STATE_PLAYING;
}

// ── Draw a single grid cell ────────────────────────────────────────
void drawCell(int8_t x, int8_t y, uint16_t color) {
  tft.fillRect(x * CELL + 1, y * CELL + TOP_OFFSET + 1, CELL - 1, CELL - 1, color);
}

// ── Place food at random empty cell ───────────────────────────────
void placeFood() {
  bool ok = false;
  while (!ok) {
    food.x = random(0, COLS);
    food.y = random(0, ROWS);
    ok = true;
    for (int i = 0; i < snakeLen; i++) {
      if (snake[i].x == food.x && snake[i].y == food.y) { ok = false; break; }
    }
  }
  drawCell(food.x, food.y, C_RED);
}

// ── Scorebar ──────────────────────────────────────────────────────
void drawScorebar() {
  tft.fillRect(0, 0, SCREEN_W, TOP_OFFSET - 1, C_BLACK);
  tft.setTextColor(C_WHITE);
  tft.setTextSize(1);
  tft.setCursor(2, 6);
  tft.print("SCORE:");
  tft.print(score);
  tft.setCursor(160, 6);
  tft.print("BEST:");
  tft.print(highScore);
  tft.setCursor(260, 6);
  tft.print("LEN:");
  tft.print(snakeLen);
}

// ── Main game update ──────────────────────────────────────────────
void gameUpdate() {
  uint32_t now = millis();
  if (now - lastMove < moveDelay) return;
  lastMove = now;

  // Commit direction
  dirX = nextDirX;
  dirY = nextDirY;

  // New head position
  int8_t nx = snake[0].x + dirX;
  int8_t ny = snake[0].y + dirY;

  // Wall collision
  if (nx < 0 || nx >= COLS || ny < 0 || ny >= ROWS) {
    drawGameOver();
    return;
  }

  // Self collision (skip tail tip — it will move)
  for (int i = 0; i < snakeLen - 1; i++) {
    if (snake[i].x == nx && snake[i].y == ny) {
      drawGameOver();
      return;
    }
  }

  bool ate = (nx == food.x && ny == food.y);

  if (ate) {
    // Grow: shift body, no tail erase
    if (snakeLen < MAX_LEN) snakeLen++;
    for (int i = snakeLen - 1; i > 0; i--) {
      snake[i] = snake[i - 1];
    }
    snake[0].x = nx;
    snake[0].y = ny;

    score += 10;
    if (score > highScore) highScore = score;
    if (moveDelay > 60) moveDelay -= 2; // speed up

    // Redraw entire snake to avoid ghost pixels
    for (int i = 0; i < snakeLen; i++) {
      drawCell(snake[i].x, snake[i].y, (i == 0) ? C_CYAN : C_GREEN);
    }
    placeFood();
    drawScorebar();
  } else {
    // Erase tail
    drawCell(snake[snakeLen - 1].x, snake[snakeLen - 1].y, C_BLACK);

    // Shift body
    for (int i = snakeLen - 1; i > 0; i--) {
      snake[i] = snake[i - 1];
    }
    snake[0].x = nx;
    snake[0].y = ny;

    // Draw new head (highlight) and neck (normal)
    drawCell(snake[0].x, snake[0].y, C_CYAN);
    if (snakeLen > 1) drawCell(snake[1].x, snake[1].y, C_GREEN);
  }
}

// ── Game over screen ──────────────────────────────────────────────
void drawGameOver() {
  gState = STATE_GAMEOVER;

  // Dim the play field
  tft.fillRect(1, TOP_OFFSET + 1, SCREEN_W - 2, SCREEN_H - TOP_OFFSET - 2, 0x18C3);

  tft.setTextColor(C_RED);
  tft.setTextSize(3);
  tft.setCursor(75, 90);
  tft.print("GAME OVER");

  tft.setTextColor(C_WHITE);
  tft.setTextSize(2);
  tft.setCursor(90, 135);
  tft.print("Score: ");
  tft.print(score);

  tft.setTextColor(C_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(85, 170);
  tft.print("Press button to continue");
}