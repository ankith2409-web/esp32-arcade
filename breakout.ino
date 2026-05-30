// ============================================================
//  BREAKOUT for ESP32
//  Display : ST7735 TFT 128x160 (SPI)
//  Input   : Analog joystick (X axis + push button)
//
//  Controls:
//    Joystick Left/Right  → move paddle
//    Button               → start / next level / restart
//
//  Libraries needed (via Arduino Library Manager):
//    - Adafruit GFX Library
//    - Adafruit ST7735 and ST7789 Library
// ============================================================

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <math.h>

#define TFT_CS    5
#define TFT_RST   4
#define TFT_DC    2
#define JOY_X     34
#define JOY_BTN   32

#define SCR_W  128
#define SCR_H  160

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

#define BRICK_COLS   8
#define BRICK_ROWS   6
#define BRICK_W      14
#define BRICK_H      7
#define BRICK_GAP    1
#define BRICK_OFF_X  2
#define BRICK_OFF_Y  16

const uint16_t BRICK_COLORS[BRICK_ROWS] = {
  0xF800, 0xFC00, 0xFFE0, 0x07E0, 0x07FF, 0xF81F,
};

#define PAD_W    28
#define PAD_H    5
#define PAD_Y    (SCR_H - 12)
#define PAD_SPD  3
#define BALL_S   4

bool  bricks[BRICK_ROWS][BRICK_COLS];
int   bricksLeft;
float ballX, ballY, ballDX, ballDY;
int   padX;
int   lives = 3, score = 0, level = 1;
bool  gameOver = false, levelWin = false;

void drawBrick(int col, int row, bool filled) {
  int x = BRICK_OFF_X + col * (BRICK_W + BRICK_GAP);
  int y = BRICK_OFF_Y + row * (BRICK_H + BRICK_GAP);
  tft.fillRect(x, y, BRICK_W, BRICK_H, filled ? BRICK_COLORS[row] : ST77XX_BLACK);
}

void drawAllBricks() {
  for (int r = 0; r < BRICK_ROWS; r++)
    for (int c = 0; c < BRICK_COLS; c++)
      drawBrick(c, r, bricks[r][c]);
}

void drawPaddle(int x, uint16_t color) { tft.fillRect(x, PAD_Y, PAD_W, PAD_H, color); }
void drawBall(float x, float y, uint16_t color) { tft.fillRect((int)x, (int)y, BALL_S, BALL_S, color); }

void updateHUD() {
  tft.fillRect(0, 0, SCR_W, 13, ST77XX_BLACK);
  tft.setTextSize(1); tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(2, 3); tft.print("S:"); tft.print(score);
  tft.setCursor(58, 3); tft.print("L:"); tft.print(level);
  for (int i = 0; i < lives; i++)
    tft.fillRect(98 + i * 10, 3, 7, 7, 0xF800);
}

void launchBall() {
  ballX = (float)(SCR_W / 2 - BALL_S / 2);
  ballY = (float)(PAD_Y - BALL_S - 4);
  float spd = 2.2f + (level - 1) * 0.35f;
  float angle = -1.1f + ((float)random(0, 40)) / 100.0f;
  ballDX = spd * sinf(angle) * (random(2) ? 1 : -1);
  ballDY = -spd * cosf(angle);
}

void initBricks() {
  bricksLeft = BRICK_ROWS * BRICK_COLS;
  for (int r = 0; r < BRICK_ROWS; r++)
    for (int c = 0; c < BRICK_COLS; c++)
      bricks[r][c] = true;
}

void initGame() {
  score = 0; lives = 3; level = 1;
  gameOver = false; levelWin = false;
  padX = SCR_W / 2 - PAD_W / 2;
  tft.fillScreen(ST77XX_BLACK);
  initBricks(); drawAllBricks();
  drawPaddle(padX, ST77XX_WHITE);
  launchBall(); drawBall(ballX, ballY, ST77XX_WHITE);
  updateHUD();
}

void startNextLevel() {
  level++; levelWin = false;
  tft.fillRect(14, 62, SCR_W - 28, 36, ST77XX_BLACK);
  tft.drawRect(14, 62, SCR_W - 28, 36, 0x07E0);
  tft.setTextColor(0x07E0); tft.setTextSize(1);
  tft.setCursor(22, 70); tft.print("LEVEL "); tft.print(level);
  tft.setTextColor(0x8C71); tft.setCursor(22, 82); tft.print("Press BTN");
  while (digitalRead(JOY_BTN) == HIGH) delay(30);
  delay(250);
  tft.fillRect(0, 13, SCR_W, SCR_H - 13, ST77XX_BLACK);
  initBricks(); padX = SCR_W / 2 - PAD_W / 2;
  drawAllBricks(); drawPaddle(padX, ST77XX_WHITE);
  launchBall(); drawBall(ballX, ballY, ST77XX_WHITE);
  updateHUD();
}

void showEndScreen(bool won) {
  tft.fillRect(10, 58, SCR_W - 20, 58, ST77XX_BLACK);
  tft.drawRect(10, 58, SCR_W - 20, 58, won ? 0x07E0 : 0xF800);
  tft.setTextSize(1);
  if (won) { tft.setTextColor(0x07E0); tft.setCursor(20, 66); tft.print("YOU WIN!"); }
  else     { tft.setTextColor(0xF800); tft.setCursor(16, 66); tft.print("GAME OVER"); }
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(18, 80); tft.print("Score: "); tft.print(score);
  tft.setCursor(18, 92); tft.print("Level: "); tft.print(level);
  tft.setTextColor(0x8C71); tft.setCursor(18, 106); tft.print("BTN to replay");
}

void onBallLost() {
  lives--; updateHUD();
  if (lives <= 0) { gameOver = true; showEndScreen(false); return; }
  tft.fillRect(0, PAD_Y - 20, SCR_W, 30, ST77XX_BLACK);
  drawPaddle(padX, ST77XX_BLACK);
  delay(350);
  padX = SCR_W / 2 - PAD_W / 2;
  launchBall(); drawPaddle(padX, ST77XX_WHITE); drawBall(ballX, ballY, ST77XX_WHITE);
}

void showSplash() {
  tft.fillScreen(ST77XX_BLACK);
  for (int c = 0; c < BRICK_COLS; c++) {
    tft.fillRect(BRICK_OFF_X + c*(BRICK_W+BRICK_GAP), 20, BRICK_W, BRICK_H, BRICK_COLORS[0]);
    tft.fillRect(BRICK_OFF_X + c*(BRICK_W+BRICK_GAP), 30, BRICK_W, BRICK_H, BRICK_COLORS[1]);
    tft.fillRect(BRICK_OFF_X + c*(BRICK_W+BRICK_GAP), 40, BRICK_W, BRICK_H, BRICK_COLORS[2]);
  }
  tft.setTextSize(2); tft.setTextColor(0xFFE0);
  tft.setCursor(8, 58); tft.print("BREAKOUT");
  tft.setTextSize(1); tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(16, 84); tft.print("Joy = Move Pad");
  tft.setCursor(16, 96); tft.print("BTN = Launch");
  tft.setTextColor(0x07FF);
  tft.setCursor(16, 145); tft.print("Press BTN to play");
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

  int jx = analogRead(JOY_X);
  int oldPad = padX;
  if (jx < 1200) padX = max(0, padX - PAD_SPD);
  else if (jx > 2900) padX = min(SCR_W - PAD_W, padX + PAD_SPD);
  if (padX != oldPad) { drawPaddle(oldPad, ST77XX_BLACK); drawPaddle(padX, ST77XX_WHITE); }

  drawBall(ballX, ballY, ST77XX_BLACK);
  float nx = ballX + ballDX;
  float ny = ballY + ballDY;

  if (nx <= 0)              { nx = 0;              ballDX =  fabs(ballDX); }
  if (nx + BALL_S >= SCR_W) { nx = SCR_W - BALL_S; ballDX = -fabs(ballDX); }
  if (ny <= 13)             { ny = 13;              ballDY =  fabs(ballDY); }

  if (ny + BALL_S >= SCR_H) { ballX = nx; ballY = ny; onBallLost(); return; }

  if (ny + BALL_S >= PAD_Y && ny + BALL_S <= PAD_Y + PAD_H + 2 &&
      nx + BALL_S > padX && nx < padX + PAD_W && ballDY > 0) {
    ny = (float)(PAD_Y - BALL_S);
    ballDY = -fabs(ballDY);
    float rel = (nx + BALL_S / 2.0f) - (padX + PAD_W / 2.0f);
    float targetSpd = fmax(sqrtf(ballDX*ballDX + ballDY*ballDY), 2.2f + (level-1)*0.35f);
    ballDX = rel * 0.18f;
    float mag = sqrtf(ballDX * ballDX + 1.0f);
    ballDX = (ballDX / mag) * targetSpd;
    ballDY = (-1.0f / mag) * targetSpd;
  }

  bool hitBrick = false;
  for (int r = 0; r < BRICK_ROWS && !hitBrick; r++) {
    for (int c = 0; c < BRICK_COLS && !hitBrick; c++) {
      if (!bricks[r][c]) continue;
      int bx = BRICK_OFF_X + c * (BRICK_W + BRICK_GAP);
      int by = BRICK_OFF_Y + r * (BRICK_H + BRICK_GAP);
      if (nx + BALL_S > bx && nx < bx + BRICK_W &&
          ny + BALL_S > by && ny < by + BRICK_H) {
        bricks[r][c] = false; bricksLeft--;
        score += 10 * level;
        drawBrick(c, r, false); updateHUD();
        hitBrick = true;
        float minH = fmin((ballX+BALL_S)-bx, (bx+BRICK_W)-ballX);
        float minV = fmin((ballY+BALL_S)-by, (by+BRICK_H)-ballY);
        if (minH < minV) ballDX = -ballDX;
        else             ballDY = -ballDY;
        if (bricksLeft == 0) {
          ballX = nx; ballY = ny;
          drawBall(ballX, ballY, ST77XX_WHITE);
          if (level < 5) startNextLevel();
          else { showEndScreen(true); gameOver = true; }
          return;
        }
      }
    }
  }

  ballX = nx; ballY = ny;
  drawBall(ballX, ballY, ST77XX_WHITE);
  delay(11);
}
