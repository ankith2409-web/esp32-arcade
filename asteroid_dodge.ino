// ============================================================
//  ASTEROID DODGE for ESP32
//  Display : ST7735 TFT 128x160 (SPI)
//  Input   : Analog joystick (X/Y axes + push button)
// ============================================================

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <math.h>

#define TFT_CS   5
#define TFT_RST  4
#define TFT_DC   2
#define JOY_X    34
#define JOY_Y    35
#define JOY_BTN  32
#define SCR_W  128
#define SCR_H  160

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

#define HUD_H   14
#define PLAY_Y  HUD_H
#define PLAY_H  (SCR_H - HUD_H)
#define PLAYER_R    4
#define PLAYER_SPD  2.6f

float px, py, ppx, ppy;

#define MAX_ROCKS  16
struct Rock { float x, y, dx, dy; uint8_t r; uint16_t color; bool active; };
Rock rocks[MAX_ROCKS];

const uint16_t ROCK_COLS[] = { 0x8C71, 0xAD75, 0xC638, 0x9CF3, 0x7BCF, 0xBDD7 };

#define NUM_STARS  35
struct Star { uint8_t x, y; uint16_t col; };
Star stars[NUM_STARS];

bool gameOver = false;
int  score    = 0;
unsigned long gameStart, lastSpawn, lastScoreTick;

#define NEAR_MISS_DIST  10.0f

float dist2(float ax, float ay, float bx, float by) {
  float dx = ax-bx, dy = ay-by; return dx*dx+dy*dy;
}
bool collides(float ax, float ay, int ar, float bx, float by, int br) {
  float md = (float)(ar+br); return dist2(ax,ay,bx,by) < md*md;
}

void drawStar(int i) { tft.drawPixel(stars[i].x, stars[i].y, stars[i].col); }
void drawAllStars()  { for (int i = 0; i < NUM_STARS; i++) drawStar(i); }

void drawShip(float x, float y, uint16_t col) {
  int ix=(int)x, iy=(int)y;
  tft.fillCircle(ix, iy+PLAYER_R-1, 2, col != ST77XX_BLACK ? 0xFC00 : ST77XX_BLACK);
  tft.fillTriangle(ix, iy-PLAYER_R, ix-PLAYER_R, iy+PLAYER_R, ix+PLAYER_R, iy+PLAYER_R, col);
  if (col != ST77XX_BLACK) tft.fillCircle(ix, iy, 1, 0x07FF);
}

void eraseShip(float x, float y) {
  drawShip(x, y, ST77XX_BLACK);
  for (int i = 0; i < NUM_STARS; i++) {
    if (abs(stars[i].x-(int)x) <= PLAYER_R+1 && abs(stars[i].y-(int)y) <= PLAYER_R+2)
      drawStar(i);
  }
}

void drawRock(Rock& r, bool erase) {
  uint16_t col = erase ? ST77XX_BLACK : r.color;
  tft.fillCircle((int)r.x, (int)r.y, r.r, col);
  if (!erase && r.r >= 6) tft.drawCircle((int)r.x-2, (int)r.y-2, r.r/3, 0x6B4D);
  if (erase) {
    for (int i = 0; i < NUM_STARS; i++)
      if (abs(stars[i].x-(int)r.x) <= r.r+1 && abs(stars[i].y-(int)r.y) <= r.r+1)
        drawStar(i);
  }
}

void spawnRock() {
  for (int i = 0; i < MAX_ROCKS; i++) {
    if (rocks[i].active) continue;
    rocks[i].active = true;
    rocks[i].r      = random(3, 9);
    rocks[i].color  = ROCK_COLS[random(6)];
    int edge = random(4);
    switch (edge) {
      case 0: rocks[i].x=random(SCR_W);       rocks[i].y=PLAY_Y+rocks[i].r+1; break;
      case 1: rocks[i].x=random(SCR_W);       rocks[i].y=SCR_H-rocks[i].r-1;  break;
      case 2: rocks[i].x=rocks[i].r+1;        rocks[i].y=PLAY_Y+random(PLAY_H); break;
      case 3: rocks[i].x=SCR_W-rocks[i].r-1; rocks[i].y=PLAY_Y+random(PLAY_H); break;
    }
    float spd = fmin(0.7f + (float)score * 0.06f, 3.2f);
    if (random(10) < 7) {
      float ddx=px-rocks[i].x, ddy=py-rocks[i].y;
      float len=sqrtf(ddx*ddx+ddy*ddy); if(len<1.0f) len=1.0f;
      float scatter=(random(-30,30))/100.0f;
      rocks[i].dx=(ddx/len+scatter)*spd;
      rocks[i].dy=(ddy/len+scatter)*spd;
    } else {
      float angle=(float)random(0,628)/100.0f;
      rocks[i].dx=cosf(angle)*spd;
      rocks[i].dy=sinf(angle)*spd;
    }
    drawRock(rocks[i], false);
    break;
  }
}

void updateHUD() {
  tft.fillRect(0, 0, SCR_W, HUD_H, ST77XX_BLACK);
  int rockCount=0;
  for (int i=0;i<MAX_ROCKS;i++) if(rocks[i].active) rockCount++;
  int danger=map(rockCount,0,MAX_ROCKS,0,40);
  tft.fillRect(2,4,40,6,0x2000);
  tft.fillRect(2,4,danger,6,danger>28?0xF800:0x07E0);
  tft.drawRect(2,4,40,6,0x4208);
  tft.setTextSize(1); tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(50,3); tft.print("T:"); tft.print(score); tft.print("s");
  tft.setTextColor(0xFFE0);
  tft.setCursor(96,3); tft.print("L"); tft.print(score/10+1);
}

void explode(float x, float y) {
  for (int r=2;r<=20;r+=3) { tft.drawCircle((int)x,(int)y,r,r<10?0xFFE0:0xFC00); delay(25); }
  delay(100);
  for (int r=20;r>=0;r-=4) tft.fillCircle((int)x,(int)y,r,ST77XX_BLACK);
}

void showGameOver() {
  tft.fillRect(8,48,SCR_W-16,78,ST77XX_BLACK);
  tft.drawRect(8,48,SCR_W-16,78,0xF800);
  tft.setTextColor(0xF800); tft.setTextSize(1);
  tft.setCursor(18,56); tft.print("SHIP DESTROYED");
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(16,70); tft.print("Survived: "); tft.print(score); tft.print("s");
  tft.setCursor(16,82); tft.print("Level hit: "); tft.print(score/10+1);
  tft.setTextColor(0x07FF); tft.setCursor(16,96);
  if      (score>=90) tft.print("Rank: LEGENDARY");
  else if (score>=60) tft.print("Rank: ACE PILOT");
  else if (score>=30) tft.print("Rank: VETERAN");
  else if (score>=15) tft.print("Rank: CADET");
  else                tft.print("Rank: ROOKIE");
  tft.setTextColor(0x8C71); tft.setCursor(22,112); tft.print("BTN to retry");
}

void showSplash() {
  tft.fillScreen(ST77XX_BLACK);
  for (int i=0;i<50;i++) tft.drawPixel(random(SCR_W),random(SCR_H),random(2)?0xFFFF:0x8C71);
  tft.fillCircle(22,35,9,0x8C71);  tft.drawCircle(20,32,3,0x6B4D);
  tft.fillCircle(105,25,6,0xAD75); tft.fillCircle(14,130,7,0x9CF3);
  tft.fillCircle(110,120,5,0xC638);
  tft.fillTriangle(64,72,58,84,70,84,0x07FF);
  tft.fillCircle(64,79,1,ST77XX_WHITE); tft.fillCircle(64,86,2,0xFC00);
  tft.setTextSize(2); tft.setTextColor(0xF800);
  tft.setCursor(10,96); tft.print("ASTEROID");
  tft.setTextColor(0xFFE0); tft.setCursor(18,114); tft.print("D O D G E");
  tft.setTextSize(1); tft.setTextColor(0x8C71);
  tft.setCursor(14,135); tft.print("Joystick = fly");
  tft.setCursor(14,147); tft.print("Survive the field!");
  tft.fillRect(0,150,SCR_W,10,ST77XX_BLACK);
  tft.setTextColor(0x07FF); tft.setCursor(14,151); tft.print("[ BTN to launch ]");
}

void initGame() {
  gameOver=false; score=0;
  px=SCR_W/2.0f; py=PLAY_Y+PLAY_H/2.0f; ppx=px; ppy=py;
  for (int i=0;i<MAX_ROCKS;i++) rocks[i].active=false;
  for (int i=0;i<NUM_STARS;i++) {
    stars[i].x=random(SCR_W); stars[i].y=PLAY_Y+random(PLAY_H);
    stars[i].col=random(3)==0?0xFFFF:0x39E7;
  }
  gameStart=lastSpawn=lastScoreTick=millis();
  tft.fillScreen(ST77XX_BLACK);
  drawAllStars(); drawShip(px,py,0x07FF); updateHUD();
}

void setup() {
  pinMode(JOY_BTN, INPUT_PULLUP);
  randomSeed(analogRead(0));
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);
  showSplash();
  while (digitalRead(JOY_BTN)==HIGH) delay(30);
  delay(200);
  initGame();
}

void loop() {
  if (gameOver) {
    if (digitalRead(JOY_BTN)==LOW) { delay(200); initGame(); }
    return;
  }

  unsigned long now = millis();

  if (now-lastScoreTick>=1000) { score++; updateHUD(); lastScoreTick=now; }

  int jx=analogRead(JOY_X), jy=analogRead(JOY_Y);
  float vx=0, vy=0;
  if      (jx<1000) vx=-PLAYER_SPD*(1.0f-jx/1000.0f);
  else if (jx>3095) vx= PLAYER_SPD*(jx-3095.0f)/1000.0f;
  if      (jy<1000) vy=-PLAYER_SPD*(1.0f-jy/1000.0f);
  else if (jy>3095) vy= PLAYER_SPD*(jy-3095.0f)/1000.0f;

  ppx=px; ppy=py;
  px=constrain(px+vx, PLAYER_R+1, SCR_W-PLAYER_R-1);
  py=constrain(py+vy, PLAY_Y+PLAYER_R+2, SCR_H-PLAYER_R-1);
  if ((int)ppx!=(int)px || (int)ppy!=(int)py) eraseShip(ppx,ppy);

  unsigned long spawnInterval=max(350UL, 1400UL-(unsigned long)score*18UL);
  if (now-lastSpawn>spawnInterval) { spawnRock(); lastSpawn=now; }

  bool nearMiss=false;
  for (int i=0;i<MAX_ROCKS;i++) {
    if (!rocks[i].active) continue;
    drawRock(rocks[i], true);
    rocks[i].x+=rocks[i].dx; rocks[i].y+=rocks[i].dy;
    if (rocks[i].x+rocks[i].r<0 || rocks[i].x-rocks[i].r>SCR_W ||
        rocks[i].y+rocks[i].r<PLAY_Y || rocks[i].y-rocks[i].r>SCR_H) {
      rocks[i].active=false; continue;
    }
    if (collides(rocks[i].x,rocks[i].y,rocks[i].r,px,py,PLAYER_R)) {
      gameOver=true; explode(px,py); showGameOver(); return;
    }
    float d2=dist2(rocks[i].x,rocks[i].y,px,py);
    float md=rocks[i].r+PLAYER_R+NEAR_MISS_DIST;
    if (d2<md*md) nearMiss=true;
    drawRock(rocks[i], false);
  }

  drawShip(px, py, nearMiss ? 0xFC00 : 0x07FF);
  delay(14);
}
