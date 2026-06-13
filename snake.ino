// ============================================================
//  SNAKE for ESP32
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

#define BLOCK_SIZE 6
#define GRID_W 20
#define GRID_H 24
#define OFFSET_X 4
#define OFFSET_Y 12

struct Point { 
  int8_t x, y; 
};

#define MAX_SNAKE 480
Point snake[MAX_SNAKE];
int snakeLen = 3;
int dirX = 1, dirY = 0;
Point food;
bool gameOver = false;
int score = 0;
unsigned long lastMove = 0;
int moveInterval = 200;

void drawBlock(int x, int y, uint16_t color) {
  tft.fillRect(OFFSET_X + x * BLOCK_SIZE, OFFSET_Y + y * BLOCK_SIZE, BLOCK_SIZE - 1, BLOCK_SIZE - 1, color);
}

void spawnFood() {
  bool valid;
  do {
    valid = true;
    food.x = random(GRID_W);
    food.y = random(GRID_H);
    for (int i = 0; i < snakeLen; i++) {
      if (snake[i].x == food.x && snake[i].y == food.y) { valid = false; break; }
    }
  } while (!valid);
  drawBlock(food.x, food.y, ST77XX_RED);
}

void initGame() {
  tft.fillScreen(ST77XX_BLACK);
  tft.drawRect(OFFSET_X - 1, OFFSET_Y - 1, GRID_W * BLOCK_SIZE + 1, GRID_H * BLOCK_SIZE + 1, 0x4208); // Dark border
  
  snakeLen = 3;
  snake[0].x = 10; snake[0].y = 10;
  snake[1].x = 9;  snake[1].y = 10;
  snake[2].x = 8;  snake[2].y = 10;
  
  dirX = 1; dirY = 0;
  score = 0;
  moveInterval = 200;
  gameOver = false;
  
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(5, 2);
  tft.print("Score: 0");
  
  for (int i = 0; i < snakeLen; i++) {
    drawBlock(snake[i].x, snake[i].y, ST77XX_GREEN);
  }
  spawnFood();
  lastMove = millis();
}

void showGameOver() {
  tft.fillRect(10, 50, 108, 60, ST77XX_BLACK);
  tft.drawRect(10, 50, 108, 60, ST77XX_RED);
  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(1);
  tft.setCursor(38, 60);
  tft.print("GAME OVER");
  
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(25, 80);
  tft.print("Score: "); tft.print(score);
  
  tft.setTextColor(0x8C71);
  tft.setCursor(25, 95);
  tft.print("BTN to retry");
}

void showSplash() {
  tft.fillScreen(ST77XX_BLACK);
  
  // Draw simple snake graphic
  for(int i=0; i<6; i++) {
    tft.fillRect(40 + i*6, 30, 5, 5, ST77XX_GREEN);
  }
  tft.fillRect(70, 24, 5, 5, ST77XX_RED); // Tongue or food
  
  tft.setTextColor(0x07E0); // Green
  tft.setTextSize(2);
  tft.setCursor(35, 50);
  tft.print("SNAKE");
  
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(28, 90);
  tft.print("Use Joystick");
  
  tft.setTextColor(0x07FF); // Cyan
  tft.setCursor(20, 120);
  tft.print("[ BTN to Start ]");
}

void setup() {
  pinMode(JOY_BTN, INPUT_PULLUP);
  randomSeed(analogRead(0));
  
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  
  showSplash();
  
  // Wait for button press
  while (digitalRead(JOY_BTN) == HIGH) delay(30);
  delay(200); // debounce
  
  initGame();
}

void loop() {
  if (gameOver) {
    if (digitalRead(JOY_BTN) == LOW) { 
      delay(200); 
      initGame(); 
    }
    return;
  }
  
  int jx = analogRead(JOY_X);
  int jy = analogRead(JOY_Y);
  
  // Update direction, but prevent 180 degree turns to avoid instant death
  if (jx < 1000 && dirX != 1) { dirX = -1; dirY = 0; }
  else if (jx > 3000 && dirX != -1) { dirX = 1; dirY = 0; }
  else if (jy < 1000 && dirY != 1) { dirX = 0; dirY = -1; }
  else if (jy > 3000 && dirY != -1) { dirX = 0; dirY = 1; }
  
  unsigned long now = millis();
  if (now - lastMove >= moveInterval) {
    lastMove = now;
    
    int nx = snake[0].x + dirX;
    int ny = snake[0].y + dirY;
    
    // Check wall collision
    if (nx < 0 || nx >= GRID_W || ny < 0 || ny >= GRID_H) {
      gameOver = true;
      showGameOver();
      return;
    }
    
    // Check self collision
    for (int i = 0; i < snakeLen; i++) {
      if (nx == snake[i].x && ny == snake[i].y) {
        gameOver = true;
        showGameOver();
        return;
      }
    }
    
    // Record tail position
    Point tail = snake[snakeLen - 1];
    
    // Move snake body
    for (int i = snakeLen - 1; i > 0; i--) {
      snake[i] = snake[i - 1];
    }
    
    // Move head
    snake[0].x = nx;
    snake[0].y = ny;
    
    drawBlock(nx, ny, ST77XX_GREEN);
    
    // Check food collision
    if (nx == food.x && ny == food.y) {
      score++;
      if (snakeLen < MAX_SNAKE) {
        snake[snakeLen] = tail;
        snakeLen++;
      }
      
      // Speed up slightly
      if (moveInterval > 60) moveInterval -= 3;
      
      // Update score HUD
      tft.fillRect(40, 2, 50, 10, ST77XX_BLACK);
      tft.setCursor(5, 2);
      tft.print("Score: "); tft.print(score);
      
      spawnFood();
    } else {
      // Erase old tail
      drawBlock(tail.x, tail.y, ST77XX_BLACK);
    }
  }
}
