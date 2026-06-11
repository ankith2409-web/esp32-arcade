// ============================================================
//  SIMON SAYS for ESP32
//  Display : ST7735 TFT 128x160 (SPI)
//  Input   : Analog joystick (X/Y axes + push button)
//
//  Watch the color panels flash in sequence.
//  Repeat the pattern using the joystick.
//  Each round adds one more step. How far can you go?
//
//  Controls:
//    Joystick UP    → Red panel
//    Joystick RIGHT → Green panel
//    Joystick DOWN  → Yellow panel
//    Joystick LEFT  → Blue panel
//    Button         → Start / Restart
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
#define HUD_H  14
#define GAP    5

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ── Panel layout (2x2 grid) ───────────────────────────────────
#define PAN_W  ((SCR_W - GAP) / 2)
#define PAN_H  ((SCR_H - HUD_H - GAP) / 2)

// Panel index: 0=Red(UP), 1=Green(RIGHT), 2=Yellow(DOWN), 3=Blue(LEFT)
const int   PAN_X[4]    = { 0,        PAN_W+GAP, 0,        PAN_W+GAP };
const int   PAN_Y[4]    = { HUD_H,    HUD_H,     HUD_H+PAN_H+GAP, HUD_H+PAN_H+GAP };
const uint16_t DIM[4]   = { 0x4000,   0x0280,    0x4400,   0x0010   };
const uint16_t LIT[4]   = { 0xF800,   0x07E0,    0xFFE0,   0x001F   };
const char  ARROWS[4]   = { '^',      '>',       'v',      '<'      };

// ── Game state ────────────────────────────────────────────────
#define MAX_SEQ 99
int  seq[MAX_SEQ];
int  seqLen    = 0;
int  playerStep = 0;
int  hiScore   = 0;
bool gameOver  = false;
bool watching  = false;   // true while Simon is showing sequence

// ── Draw a single panel ───────────────────────────────────────
void drawPanel(int i, bool lit) {
  tft.fillRoundRect(PAN_X[i], PAN_Y[i], PAN_W, PAN_H, 8, lit ? LIT[i] : DIM[i]);
  tft.setTextSize(3);
  tft.setTextColor(lit ? ST77XX_WHITE : (uint16_t)(DIM[i] | 0x2104));
  tft.setCursor(PAN_X[i] + PAN_W/2 - 9, PAN_Y[i] + PAN_H/2 - 12);
  tft.print(ARROWS[i]);
}

void drawAllPanels(int litIdx = -1) {
  for (int i = 0; i < 4; i++) drawPanel(i, i == litIdx);
}

// ── HUD ───────────────────────────────────────────────────────
void updateHUD() {
  tft.fillRect(0, 0, SCR_W, HUD_H, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(2, 3);   tft.print("SIMON SAYS");
  tft.setCursor(80, 3);  tft.print("R:"); tft.print(seqLen);
  tft.setCursor(104, 3); tft.print("B:"); tft.print(hiScore);
}

// ── Flash one panel ───────────────────────────────────────────
void flashPanel(int i, int ms = 420) {
  drawPanel(i, true);
  delay(ms);
  drawPanel(i, false);
  delay(180);
}

// ── Play the current sequence ─────────────────────────────────
void playSequence() {
  watching = true;
  updateHUD();

  // Flash "WATCH" hint
  tft.fillRect(34, 68, 60, 16, ST77XX_BLACK);
  tft.setTextColor(0xFFE0); tft.setTextSize(1);
  tft.setCursor(38, 72); tft.print("WATCH...");
  delay(700);
  tft.fillRect(34, 68, 60, 16, ST77XX_BLACK);

  // Speed scales with sequence length
  int flashMs = max(200, 450 - seqLen * 12);

  for (int i = 0; i < seqLen; i++) {
    flashPanel(seq[i], flashMs);
    delay(50);
  }

  // Flash "YOUR TURN" hint
  tft.fillRect(20, 68, 88, 16, ST77XX_BLACK);
  tft.setTextColor(0x07FF); tft.setTextSize(1);
  tft.setCursor(24, 72); tft.print("YOUR TURN!");
  delay(500);
  tft.fillRect(20, 68, 88, 16, ST77XX_BLACK);

  playerStep = 0;
  watching = false;
}

// ── Read joystick direction ───────────────────────────────────
// Returns 0=UP 1=RIGHT 2=DOWN 3=LEFT or -1=center
int readDir() {
  int jx = analogRead(JOY_X);
  int jy = analogRead(JOY_Y);
  if      (jy < 1000) return 0;
  else if (jx > 3000) return 1;
  else if (jy > 3000) return 2;
  else if (jx < 1000) return 3;
  return -1;
}

// ── All-panel flash (win / lose effect) ──────────────────────
void flashAll(uint16_t col, int times, int ms) {
  for (int t = 0; t < times; t++) {
    tft.fillRect(0, HUD_H, SCR_W, SCR_H - HUD_H, col);
    delay(ms);
    drawAllPanels();
    delay(ms);
  }
}

// ── Game over screen ─────────────────────────────────────────
void showGameOver() {
  if (seqLen - 1 > hiScore) hiScore = seqLen - 1;
  flashAll(0xF800, 3, 100);

  tft.fillRect(12, 52, SCR_W - 24, 72, ST77XX_BLACK);
  tft.drawRect(12, 52, SCR_W - 24, 72, 0xF800);

  tft.setTextColor(0xF800); tft.setTextSize(1);
  tft.setCursor(20, 60); tft.print("WRONG MOVE!");

  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(18, 74); tft.print("You got to round "); tft.print(seqLen);
  tft.setCursor(18, 86); tft.print("Best round : "); tft.print(hiScore);

  tft.setTextColor(0x07FF);
  tft.setCursor(18, 98);
  if      (hiScore >= 20) tft.print("Rank: MEMORY KING");
  else if (hiScore >= 12) tft.print("Rank: SHARP MIND");
  else if (hiScore >= 6)  tft.print("Rank: GETTING THERE");
  else                    tft.print("Rank: KEEP TRYING");

  tft.setTextColor(0x8C71);
  tft.setCursor(22, 114); tft.print("BTN to replay");
}

// ── Splash screen ─────────────────────────────────────────────
void showSplash() {
  tft.fillScreen(ST77XX_BLACK);

  // Mini 2x2 preview
  tft.fillRoundRect(8,  18, 50, 30, 5, 0x6000);
  tft.fillRoundRect(70, 18, 50, 30, 5, 0x0380);
  tft.fillRoundRect(8,  56, 50, 30, 5, 0x6600);
  tft.fillRoundRect(70, 56, 50, 30, 5, 0x0018);

  tft.setTextSize(1); tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(20, 26); tft.print("^ UP");
  tft.setCursor(78, 26); tft.print("> RIGHT");
  tft.setCursor(16, 64); tft.print("v DOWN");
  tft.setCursor(76, 64); tft.print("< LEFT");

  tft.setTextSize(2); tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(16, 98);  tft.print("SIMON");
  tft.setCursor(24, 116); tft.print("SAYS");

  tft.setTextSize(1); tft.setTextColor(0x8C71);
  tft.setCursor(8, 140); tft.print("Watch the flash pattern");
  tft.setCursor(8, 152); tft.print("then repeat it back!");
}

// ── Init new game ─────────────────────────────────────────────
void initGame() {
  seqLen = 0; playerStep = 0; gameOver = false;
  tft.fillScreen(ST77XX_BLACK);
  drawAllPanels();
  updateHUD();

  // Brief countdown
  for (int i = 3; i >= 1; i--) {
    tft.fillRect(50, 68, 28, 16, ST77XX_BLACK);
    tft.setTextColor(0xFFE0); tft.setTextSize(2);
    tft.setCursor(56, 68); tft.print(i);
    delay(600);
  }
  tft.fillRect(50, 68, 28, 16, ST77XX_BLACK);

  seq[seqLen++] = random(4);
  playSequence();
}

// ── Setup ─────────────────────────────────────────────────────
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

// ── Main loop ─────────────────────────────────────────────────
void loop() {
  if (gameOver) {
    if (digitalRead(JOY_BTN) == LOW) { delay(200); initGame(); }
    return;
  }
  if (watching) return;

  int dir = readDir();
  if (dir == -1) return;

  // Show the panel the player pressed
  flashPanel(dir, 300);

  if (dir == seq[playerStep]) {
    playerStep++;

    if (playerStep == seqLen) {
      // Sequence complete!
      if (seqLen > hiScore) hiScore = seqLen;
      flashAll(0x07E0, 2, 120);   // green win flash
      delay(300);
      seq[seqLen++] = random(4);
      updateHUD();
      playSequence();
    }
  } else {
    gameOver = true;
    showGameOver();
    return;
  }

  // Wait for joystick to return to center
  while (readDir() != -1) delay(20);
  delay(80);
}
