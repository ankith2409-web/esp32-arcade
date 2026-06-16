// ============================================================
//  PONG for ESP32
//  Display : ST7735 TFT 128x160 (SPI)
//  Input   : Analog joystick (X/Y axes + push button)
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

#define PADDLE_W 4
#define PADDLE_H 24
#define BALL_SIZE 4

int playerY = SCR_H / 2 - PADDLE_H / 2;
int aiY = SCR_H / 2 - PADDLE_H / 2;
float ballX = SCR_W / 2;
float ballY = SCR_H / 2;
float ballDX = 2.5;
float ballDY = 2.0;

int scorePlayer = 0;
int scoreAI = 0;
int maxScore = 5;
bool gameOver = false;

void setup() {
  pinMode(JOY_BTN, INPUT_PULLUP);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  
  showSplash();
  while (digitalRead(JOY_BTN) == HIGH) delay(30);
  delay(200); // debounce
  initGame();
}

void showSplash() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(35, 50);
  tft.print("PONG");
  tft.setTextSize(1);
  tft.setCursor(28, 90);
  tft.print("Use Joystick");
  tft.setTextColor(0x07FF);
  tft.setCursor(20, 120);
  tft.print("[ BTN to Start ]");
}

void initGame() {
  tft.fillScreen(ST77XX_BLACK);
  scorePlayer = 0;
  scoreAI = 0;
  resetRound();
  gameOver = false;
  drawNet();
}

void resetRound() {
  tft.fillScreen(ST77XX_BLACK);
  playerY = SCR_H / 2 - PADDLE_H / 2;
  aiY = SCR_H / 2 - PADDLE_H / 2;
  ballX = SCR_W / 2;
  ballY = SCR_H / 2;
  ballDX = (random(0, 2) == 0 ? 2.5 : -2.5);
  ballDY = random(-2, 3);
  if (ballDY == 0) ballDY = 1;
  drawNet();
  drawScores();
}

void drawNet() {
  for (int i = 0; i < SCR_H; i += 8) {
    tft.drawFastVLine(SCR_W / 2, i, 4, 0x4208);
  }
}

void drawScores() {
  tft.fillRect(0, 0, SCR_W, 30, ST77XX_BLACK); // clear score area
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(SCR_W / 4, 10);
  tft.print(scorePlayer);
  tft.setCursor(SCR_W * 3 / 4, 10);
  tft.print(scoreAI);
}

void showGameOver() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(2);
  tft.setCursor(10, 50);
  if (scorePlayer >= maxScore) {
    tft.setTextColor(ST77XX_GREEN);
    tft.print("YOU WIN!");
  } else {
    tft.print("GAME OVER");
  }
  tft.setTextColor(0x8C71);
  tft.setTextSize(1);
  tft.setCursor(25, 95);
  tft.print("BTN to retry");
}

void loop() {
  if (gameOver) {
    if (digitalRead(JOY_BTN) == LOW) { 
      delay(200); 
      initGame(); 
    }
    return;
  }

  // Clear previous ball and paddles
  tft.fillRect((int)ballX, (int)ballY, BALL_SIZE, BALL_SIZE, ST77XX_BLACK);
  tft.fillRect(2, playerY, PADDLE_W, PADDLE_H, ST77XX_BLACK);
  tft.fillRect(SCR_W - 2 - PADDLE_W, aiY, PADDLE_W, PADDLE_H, ST77XX_BLACK);

  // Player movement
  int jy = analogRead(JOY_Y);
  if (jy < 1000) playerY -= 3;
  else if (jy > 3000) playerY += 3;
  
  if (playerY < 0) playerY = 0;
  if (playerY > SCR_H - PADDLE_H) playerY = SCR_H - PADDLE_H;

  // AI movement
  if (ballX > SCR_W / 2 && ballDX > 0) {
    int aiCenter = aiY + PADDLE_H / 2;
    if (aiCenter < ballY - 4) aiY += 2;
    else if (aiCenter > ballY + 4) aiY -= 2;
  }
  if (aiY < 0) aiY = 0;
  if (aiY > SCR_H - PADDLE_H) aiY = SCR_H - PADDLE_H;

  // Ball movement
  ballX += ballDX;
  ballY += ballDY;

  // Wall collisions
  if (ballY <= 0 || ballY >= SCR_H - BALL_SIZE) {
    ballDY = -ballDY;
  }

  // Paddle collisions
  if (ballX <= 2 + PADDLE_W && ballY + BALL_SIZE >= playerY && ballY <= playerY + PADDLE_H) {
    ballX = 2 + PADDLE_W;
    ballDX = -ballDX;
    ballDY += (ballY - (playerY + PADDLE_H / 2)) * 0.1;
  }
  if (ballX >= SCR_W - 2 - PADDLE_W - BALL_SIZE && ballY + BALL_SIZE >= aiY && ballY <= aiY + PADDLE_H) {
    ballX = SCR_W - 2 - PADDLE_W - BALL_SIZE;
    ballDX = -ballDX;
    ballDY += (ballY - (aiY + PADDLE_H / 2)) * 0.1;
  }

  // Scoring
  if (ballX < 0) {
    scoreAI++;
    if (scoreAI >= maxScore) gameOver = true;
    else { resetRound(); return; }
  } else if (ballX > SCR_W) {
    scorePlayer++;
    if (scorePlayer >= maxScore) gameOver = true;
    else { resetRound(); return; }
  }

  if (gameOver) {
    showGameOver();
    return;
  }

  // Draw updated objects
  tft.fillRect((int)ballX, (int)ballY, BALL_SIZE, BALL_SIZE, ST77XX_WHITE);
  tft.fillRect(2, playerY, PADDLE_W, PADDLE_H, ST77XX_GREEN);
  tft.fillRect(SCR_W - 2 - PADDLE_W, aiY, PADDLE_W, PADDLE_H, ST77XX_RED);
  drawNet();
  
  delay(15);
}
