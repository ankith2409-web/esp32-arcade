#include <Arduino.h>
// Tiny Gaming Console - ESP32 + KY-023 Joystick + ST7789 2" TFT
// Game: Flappy Bird
// Controls: Press joystick button or push joystick UP to flap/start

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

// ── Display Dimensions ────────────────────────────────────────────
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
#define C_SKY     0x4DDF   // Sky blue
#define C_BROWN   0x8200   // Ground brown
#define C_DARKGRN 0x03E0   // Pipe outline green

// ── Game Config ───────────────────────────────────────────────────
#define BIRD_X        80
#define BIRD_RADIUS   6
#define GRAVITY       0.35
#define FLAP_FORCE    -5.0
#define MAX_VELOCITY  8.0

#define PIPE_WIDTH    35
#define PIPE_GAP      70
#define MIN_PIPE_H    30
#define MAX_PIPE_H    (SCREEN_H - PIPE_GAP - MIN_PIPE_H - 15) // Leave 15px for ground
#define MAX_PIPES     2
#define PIPE_SPEED    3

#define GROUND_H      15

// ── Joystick thresholds ───────────────────────────────────────────
#define JOY_DEAD_LOW    1700
#define JOY_DEAD_HIGH   2400

// ── Game State ────────────────────────────────────────────────────
enum GameState { STATE_MENU, STATE_PLAYING, STATE_GAMEOVER };
GameState gState = STATE_MENU;

// ── Game Data ─────────────────────────────────────────────────────
float birdY = 100.0;
float birdV = 0.0; // velocity

struct Pipe {
  int x;
  int gapY;
  bool active;
  bool scored;
};

Pipe pipes[MAX_PIPES];
uint32_t lastUpdate = 0;
uint16_t frameDelay = 30; // ~33 FPS
uint32_t score = 0;
uint32_t highScore = 0;

// Button state
bool btnLastState = HIGH;
uint32_t btnDebounce = 0;
bool btnPressed = false;

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCK, TFT_RST);

// ── Forward Declarations ──────────────────────────────────────────
void drawMenu();
void startGame();
void gameUpdate();
void drawGameOver();
void readJoystick();
void placePipe(int index, int startX);
void drawBird(uint16_t color);
void drawPipes(bool erase);
void drawGround();

// ═════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);

  // Backlight ON
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // Joystick button
  pinMode(JOY_SW, INPUT_PULLUP);

  // Init display
  tft.init(240, 320);
  tft.setRotation(1); // landscape
  tft.fillScreen(C_SKY);

  randomSeed(analogRead(35)); // floating pin seed

  drawMenu();
}

// ═════════════════════════════════════════════════════════════════
void loop() {
  readJoystick();

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
      }
      break;
  }
}

// ── Read Input ───────────────────────────────────────────────────
void readJoystick() {
  int vy = analogRead(JOY_VRY);
  bool curSWState = digitalRead(JOY_SW);

  // Joystick push up counts as flap
  bool inputFlap = (vy < JOY_DEAD_LOW);

  // Button edge check with debounce
  if ((curSWState == LOW && btnLastState == HIGH) || (inputFlap && btnLastState == HIGH)) {
    uint32_t now = millis();
    if (now - btnDebounce > 150) {
      btnDebounce = now;
      btnPressed = true;
    }
  }
  
  if (curSWState == HIGH && !inputFlap) {
    btnLastState = HIGH;
  } else {
    btnLastState = LOW;
  }
}

// ── Menu Screen ───────────────────────────────────────────────────
void drawMenu() {
  tft.fillScreen(C_SKY);
  drawGround();

  tft.setTextColor(C_YELLOW);
  tft.setTextSize(3);
  tft.setCursor(50, 45);
  tft.print("FLAPPY BIRD");

  tft.setTextColor(C_WHITE);
  tft.setTextSize(1);
  tft.setCursor(75, 105);
  tft.print("Press button/Push UP to Flap");
  tft.setCursor(70, 125);
  tft.print("Avoid the green obstacle pipes");

  if (highScore > 0) {
    tft.setTextColor(C_YELLOW);
    tft.setTextSize(1);
    tft.setCursor(110, 160);
    tft.print("High Score: ");
    tft.print(highScore);
  }

  // Draw static bird graphic in menu
  tft.fillCircle(BIRD_X, 100, BIRD_RADIUS, C_YELLOW);
  tft.fillCircle(BIRD_X + 3, 98, 1, C_BLACK); // eye
  tft.fillRect(BIRD_X + 5, 100, 3, 2, C_RED); // beak

  gState = STATE_MENU;
}

// ── Start Game ────────────────────────────────────────────────────
void startGame() {
  tft.fillScreen(C_SKY);
  drawGround();

  birdY = 100.0;
  birdV = 0.0;
  score = 0;

  // Initialize pipes
  placePipe(0, SCREEN_W);
  placePipe(1, SCREEN_W + (SCREEN_W + PIPE_WIDTH) / 2);

  drawBird(C_YELLOW);
  drawPipes(false);

  lastUpdate = millis();
  gState = STATE_PLAYING;
}

// ── Place Procedural Pipe ────────────────────────────────────────
void placePipe(int index, int startX) {
  pipes[index].x = startX;
  pipes[index].gapY = random(MIN_PIPE_H, MAX_PIPE_H);
  pipes[index].active = true;
  pipes[index].scored = false;
}

// ── Draw Bird helper ──────────────────────────────────────────────
void drawBird(uint16_t color) {
  // Body
  tft.fillCircle(BIRD_X, (int)birdY, BIRD_RADIUS, color);
  
  if (color != C_SKY) {
    // Eye
    tft.fillCircle(BIRD_X + 2, (int)birdY - 2, 1, C_BLACK);
    // Beak
    tft.fillRect(BIRD_X + 4, (int)birdY, 3, 2, C_RED);
  }
}

// ── Draw Pipes helper ─────────────────────────────────────────────
void drawPipes(bool erase) {
  uint16_t color = erase ? C_SKY : C_GREEN;

  for (int i = 0; i < MAX_PIPES; i++) {
    if (!pipes[i].active) continue;

    int x = pipes[i].x;
    int gapY = pipes[i].gapY;

    // We only draw elements within screen boundaries
    if (x + PIPE_WIDTH < 0 || x >= SCREEN_W) continue;

    // Top pipe rect
    tft.fillRect(x, 0, PIPE_WIDTH, gapY, color);
    
    // Bottom pipe rect
    tft.fillRect(x, gapY + PIPE_GAP, PIPE_WIDTH, SCREEN_H - gapY - PIPE_GAP - GROUND_H, color);

    // Decorative pipe lips (darker outline & highlight details)
    if (!erase) {
      tft.drawRect(x, 0, PIPE_WIDTH, gapY, C_DARKGRN);
      tft.drawRect(x, gapY + PIPE_GAP, PIPE_WIDTH, SCREEN_H - gapY - PIPE_GAP - GROUND_H, C_DARKGRN);
      // Pipe lips near gap
      tft.fillRect(x - 2, gapY - 12, PIPE_WIDTH + 4, 12, C_GREEN);
      tft.drawRect(x - 2, gapY - 12, PIPE_WIDTH + 4, 12, C_DARKGRN);
      
      tft.fillRect(x - 2, gapY + PIPE_GAP, PIPE_WIDTH + 4, 12, C_GREEN);
      tft.drawRect(x - 2, gapY + PIPE_GAP, PIPE_WIDTH + 4, 12, C_DARKGRN);
    } else {
      // Clean lip overhangs when erasing
      tft.fillRect(x - 2, gapY - 12, 2, 12, C_SKY);
      tft.fillRect(x + PIPE_WIDTH, gapY - 12, 2, 12, C_SKY);
      tft.fillRect(x - 2, gapY + PIPE_GAP, 2, 12, C_SKY);
      tft.fillRect(x + PIPE_WIDTH, gapY + PIPE_GAP, 2, 12, C_SKY);
    }
  }
}

// ── Ground Rendering ──────────────────────────────────────────────
void drawGround() {
  tft.fillRect(0, SCREEN_H - GROUND_H, SCREEN_W, 3, C_GREEN);
  tft.fillRect(0, SCREEN_H - GROUND_H + 3, SCREEN_W, GROUND_H - 3, C_BROWN);
}

// ── Game Engine Frame Update ──────────────────────────────────────
void gameUpdate() {
  uint32_t now = millis();
  if (now - lastUpdate < frameDelay) return;
  lastUpdate = now;

  // 1. Erase old frame items
  drawBird(C_SKY);
  drawPipes(true);

  // 2. Handle Inputs
  if (btnPressed) {
    btnPressed = false;
    birdV = FLAP_FORCE;
  }

  // 3. Physics update
  birdV += GRAVITY;
  if (birdV > MAX_VELOCITY) birdV = MAX_VELOCITY;
  birdY += birdV;

  // Collision with ground or ceiling
  if (birdY - BIRD_RADIUS <= 0 || birdY + BIRD_RADIUS >= SCREEN_H - GROUND_H) {
    drawGameOver();
    return;
  }

  // 4. Update and scroll pipes
  for (int i = 0; i < MAX_PIPES; i++) {
    if (!pipes[i].active) continue;

    pipes[i].x -= PIPE_SPEED;

    // Score point check
    if (!pipes[i].scored && pipes[i].x + PIPE_WIDTH < BIRD_X) {
      pipes[i].scored = true;
      score++;
      if (score > highScore) highScore = score;
    }

    // Reuse pipe once offscreen
    if (pipes[i].x + PIPE_WIDTH < -5) {
      // Find the other pipe's index to spawn relative to it
      int otherIdx = (i == 0) ? 1 : 0;
      placePipe(i, pipes[otherIdx].x + (SCREEN_W + PIPE_WIDTH) / 2);
    }

    // Collision Check: standard circle-to-AABB bounding box check
    int px = pipes[i].x;
    int py = pipes[i].gapY;
    
    // Check if bird overlaps horizontally with pipe
    if (BIRD_X + BIRD_RADIUS > px && BIRD_X - BIRD_RADIUS < px + PIPE_WIDTH) {
      // Check if bird is inside top pipe or bottom pipe vertically
      if (birdY - BIRD_RADIUS < py || birdY + BIRD_RADIUS > py + PIPE_GAP) {
        drawGameOver();
        return;
      }
    }
  }

  // 5. Render new frame items
  drawPipes(false);
  drawBird(C_YELLOW);
  drawGround(); // Repaint ground to prevent pipe overlap glitch

  // Draw Score overlay
  tft.setTextColor(C_WHITE);
  tft.setTextSize(2);
  tft.setCursor(145, 10);
  tft.print(score);
}

// ── Game Over Screen ──────────────────────────────────────────────
void drawGameOver() {
  gState = STATE_GAMEOVER;

  // Dim background block
  tft.fillRect(30, 30, SCREEN_W - 60, SCREEN_H - 60 - GROUND_H, 0x18C3);

  tft.setTextColor(C_RED);
  tft.setTextSize(3);
  tft.setCursor(75, 55);
  tft.print("GAME OVER");

  tft.setTextColor(C_WHITE);
  tft.setTextSize(2);
  tft.setCursor(95, 105);
  tft.print("Score: ");
  tft.print(score);
  tft.setCursor(95, 130);
  tft.print("Best:  ");
  tft.print(highScore);

  tft.setTextColor(C_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(75, 175);
  tft.print("Press button to continue");
}
