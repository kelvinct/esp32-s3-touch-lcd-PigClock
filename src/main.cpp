#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <Wire.h>
#include <QMI8658.h>
#include <LovyanGFX.hpp>
#include <Preferences.h>

float LATITUDE  = 22.3193;
float LONGITUDE = 114.1694;
String TIMEZONE = "Asia/Hong_Kong";

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789  _panel_instance;
  lgfx::Bus_SPI       _bus_instance;
  lgfx::Light_PWM     _light_instance;
  lgfx::Touch_CST816S _touch_instance;
public:
  LGFX() {
    { auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST; cfg.spi_mode = 0;
      cfg.freq_write = 40000000; cfg.freq_read = 8000000;
      cfg.spi_3wire = false; cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = 39; cfg.pin_mosi = 38; cfg.pin_miso = -1; cfg.pin_dc = 42;
      _bus_instance.config(cfg); _panel_instance.setBus(&_bus_instance); }
    { auto cfg = _panel_instance.config();
      cfg.pin_cs = 45; cfg.pin_rst = -1; cfg.pin_busy = -1;
      cfg.memory_width = 240; cfg.memory_height = 320;
      cfg.panel_width = 240; cfg.panel_height = 320;
      cfg.invert = true; cfg.rgb_order = false;
      _panel_instance.config(cfg); }
    { auto cfg = _light_instance.config();
      cfg.pin_bl = 1; cfg.freq = 44100; cfg.pwm_channel = 7;
      _light_instance.config(cfg); _panel_instance.setLight(&_light_instance); }
    { auto cfg = _touch_instance.config();
      cfg.i2c_addr = 0x15; cfg.i2c_port = 0;
      cfg.pin_sda = 48; cfg.pin_scl = 47; cfg.pin_int = -1; cfg.pin_rst = -1;
      cfg.freq = 400000; cfg.x_max = 240; cfg.y_max = 320;
      _touch_instance.config(cfg); _panel_instance.setTouch(&_touch_instance); }
    setPanel(&_panel_instance);
  }
};

LGFX tft;
LGFX_Sprite bgSprite(&tft);
LGFX_Sprite canvas(&tft);
QMI8658 imu;
Preferences prefs;
WiFiMulti wifiMulti;

#define SKY_TOP 0x0000
#define SKY_BOTTOM 0x1082
#define HILL_COLOR 0x0841
#define HILL_HIGHLIGHT 0x10A2
#define CARD_BG 0x2124
#define CARD_SHADOW 0x0000
#define CARD_BORDER 0x39E7
#define TEXT_MAIN 0xFFFF
#define TEXT_DIM 0xBDF7
#define TEXT_ACCENT 0x07FF
#define TEXT_WARN 0xFBE0
#define TEXT_SHADOW 0x0000
#define STAR_COLOR 0xFFFF
#define SUN_CORE 0xFD20
#define SUN_GLOW 0xFEA0
#define SUN_RAY 0xFFE0
#define MOON_BODY 0xFFEC
#define MOON_DARK 0x1082
#define PIG_PINK 0xF9A8
#define PIG_PINK_DARK 0xD145
#define PIG_WHITE 0xD6D6
#define PIG_WHITE_DARK 0x8410
#define ICE_BLUE 0x3D7F
#define ICE_LIGHT 0x7DFF
#define ICE_WHITE 0xEFFF
#define BIRD_BODY 0xFEA0
#define BIRD_DARK 0xFB00
#define BIRD_BEAK 0xF800
#define BIRD_EYE 0x0000
#define BIRD_FEATHER 0xFFE0
#define FEAR_SWEAT 0x5D9B
#define FEAR_EXCL 0xF800
#define DIZZY_STAR1 0xFFE0
#define DIZZY_STAR2 0x07FF
#define DIZZY_STAR3 0xF81F
#define WOLF_BROWN 0x9B26
#define WOLF_DARK 0x5961
#define WOLF_EYE 0xFC00
#define BAT_BODY 0x2124
#define WASP_BODY 0xFE00
#define WASP_STRIPE 0x0000
#define CROW_BODY 0x1082
#define CROW_BEAK 0xFC00

struct WeatherData { float temperature=0; float humidity=0; int weatherCode=0; bool valid=false; };
WeatherData weather;
unsigned long lastWeatherUpdate = 0;
const unsigned long WEATHER_INTERVAL = 600000;
unsigned long lastClockUpdate = 0;

int pigX = -40, pig2X = 340;
int pigY = 224;
int pigSpeed = 2, pig2Speed = -2;
unsigned long lastPigUpdate = 0;
bool collisionActive = false;
unsigned long collisionStartTime = 0;

bool pinkFrozen = false, whiteFrozen = false;
unsigned long pinkFreezeEnd = 0, whiteFreezeEnd = 0;
const unsigned long FREEZE_DURATION = 2500;
int pinkFrozenSpeed = 0, whiteFrozenSpeed = 0;

bool pinkDizzy = false, whiteDizzy = false;
unsigned long pinkDizzyEnd = 0, whiteDizzyEnd = 0;
const unsigned long DIZZY_DURATION = 3000;

// 🆕 小猪睡觉系统
bool pigSleeping = false;
unsigned long lastActivityTime = 0;
const unsigned long SLEEP_DELAY = 15000;

int birdX = 160, birdY = -30, birdState = 0, birdTarget = 0;
unsigned long birdStateTime = 0;
float birdWingAngle = 0, birdDeathAngle = 0, birdDeathVY = 0;

int wolfX = -50, wolfY = 224, wolfState = 0, wolfDir = 1, wolfTarget = 0;
unsigned long wolfStateTime = 0, wolfNextSpawn = 0;
float wolfLegAngle = 0, wolfDeathAngle = 0, wolfDeathVY = 0;
unsigned long wolfHowlEnd = 0, wolfClawEnd = 0;
#define MAX_WOLF_DUST 15
struct WolfDust { float x, y, vx, vy; int life; bool active; };
WolfDust wolfDust[MAX_WOLF_DUST];

int batX = 0, batY = -30, batState = 0;
unsigned long batStateTime = 0, batNextSpawn = 0;
float batWingAngle = 0, batWavePhase = 0;

int waspX = -30, waspY = 150, waspState = 0, waspDir = 1;
unsigned long waspStateTime = 0, waspNextSpawn = 0;
float waspWingAngle = 0;

int crowX = 160, crowY = -40, crowState = 0, crowTarget = 0;
unsigned long crowStateTime = 0, crowNextSpawn = 0;
float crowWingAngle = 0;

//int trapX = -100, trapY = 230, trapState = 0;
int trapX = -100, trapY = 236, trapState = 0;
unsigned long trapStateTime = 0, trapNextSpawn = 0;
bool trapActive = false;

int fruitX = -100, fruitY = 236, fruitState = 0;
unsigned long fruitStateTime = 0, fruitNextSpawn = 0;
bool fruitActive = false;
bool pinkInvincible = false, whiteInvincible = false;
unsigned long pinkInvincibleEnd = 0, whiteInvincibleEnd = 0;
const unsigned long INVINCIBLE_DURATION = 8000;

unsigned long pigCelebrateEnd = 0;
bool pigsCelebrating = false;
float celebrationRotation = 0;
int shakeX = 0, shakeY = 0;
unsigned long shakeEndTime = 0;
unsigned long flashEndTime = 0;

#define MAX_FEATHERS 20
struct Feather { float x, y, vx, vy; int life; bool active; };
Feather feathers[MAX_FEATHERS];

int currentView = 0;
bool lastTouchState = false;
float ax = 0, ay = 0, az = 0;
float tiltX = 0, tiltY = 0;
float currentSunAlpha = 0.0;
bool isDaytime = false;

bool enemyBird = true, enemyWolf = true, enemyBat = true;
bool enemyWasp = true, enemyCrow = true, enemyTrap = true, enemyFruit = true;
bool pigFearMode = false;   // false = 勇敢衝鋒, true = 害怕逃跑
#define MAX_SNOW 40
struct Snowflake { float x, y; float size; int life; bool active; };
Snowflake snow[MAX_SNOW];

#define MAX_TWINKLE 8
struct Twinkle { int x, y; int phase; };
Twinkle twinkles[MAX_TWINKLE];

#define MAX_NOTIFS 3
struct Notification { String text; uint16_t color; unsigned long createdAt; bool active; };
Notification notifs[MAX_NOTIFS];

// 函數聲明
void connectWiFi();
void syncTime();
void fetchWeather();
String getWeatherDescription(int code);
void renderBackground();
void drawWeatherIcon(int code, int x, int y);
void drawWeatherPanel();
void drawDigitalClock();
void drawAnalogClock();
void drawPig(int x, int y, uint16_t body, uint16_t dark, bool facingRight, bool frozen, bool fear, bool dizzy);
void drawAnimeEffect(int x, int y);
void drawFreezeEffect(int x, int y);
void drawDizzyEffect(int x, int y);
void drawPigCelebration();
void handleTouch();
void spawnSnow(int tx, int ty);
void updateSnow();
void drawSnowflake(int x, int y, int size);
uint16_t blendRGB565(uint16_t c1, uint16_t c2, float t);
void updateCelestialState();
void drawCelestialBody();
void drawSun(int cx, int cy, float alpha);
void drawMoon(int cx, int cy, float alpha);
void updateBirdAttack(unsigned long now);
void drawBird(int x, int y, float wingAngle);
void drawDyingBird(int x, int y, float angle);
void spawnFeathers(int x, int y);
void updateFeathers();
bool checkBirdTouch(uint16_t tx, uint16_t ty);
void killBird();
void updateWolf(unsigned long now);
void drawWolf(int x, int y, int dir, float legAngle);
void drawDyingWolf(int x, int y, float angle);
bool checkWolfTouch(uint16_t tx, uint16_t ty);
void killWolf();
void spawnWolfDust(int x, int y);
void updateWolfDust();
void drawWolfDust();
void drawWolfClaw(int x, int y);
void updateBat(unsigned long now);
void drawBat(int x, int y, float wingAngle);
void drawDyingBat(int x, int y);
bool checkBatTouch(uint16_t tx, uint16_t ty);
void killBat();
void updateWasp(unsigned long now);
void drawWasp(int x, int y, int dir, float wingAngle);
void drawDyingWasp(int x, int y);
bool checkWaspTouch(uint16_t tx, uint16_t ty);
void killWasp();
void updateCrow(unsigned long now);
void drawCrow(int x, int y, float wingAngle);
void drawDyingCrow(int x, int y);
bool checkCrowTouch(uint16_t tx, uint16_t ty);
void killCrow();
void updateTrap(unsigned long now);
void drawTrap();
bool checkTrapTouch(uint16_t tx, uint16_t ty);
void triggerTrap();
void updateFruit(unsigned long now);
void drawFruit();
void giveInvincibleToPig(bool isPink);
void addNotification(String text, uint16_t color);
void updateNotifications();
void drawNotifications();
int getNextEnemyType();
unsigned long getNextEnemyTime();
void drawNextEnemyBox();
void showBootAnimation();
void showMainMenu();
void showEnemyMenu();
void showInfoScreen();
void showWifiScanner();
String showPasswordKeyboard(String ssid);
void saveWiFi(String ssid, String pass);

void setup() {
  Serial.begin(115200);
  delay(1000);
  randomSeed(esp_random());
  Wire.begin(48, 47, 400000UL);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  canvas.setColorDepth(16);   canvas.setPsram(true);  canvas.createSprite(320, 240);
  bgSprite.setColorDepth(16); bgSprite.setPsram(true); bgSprite.createSprite(320, 240);
  showBootAnimation();
  showMainMenu();
  prefs.begin("clock", true);
  String savedLat = prefs.getString("lat", "");
  String savedLon = prefs.getString("lon", "");
  String savedTz  = prefs.getString("tz",  "");
  enemyBird  = prefs.getBool("eBird", true);
  enemyWolf  = prefs.getBool("eWolf", true);
  enemyBat   = prefs.getBool("eBat",  true);
  enemyWasp  = prefs.getBool("eWasp", true);
  enemyCrow  = prefs.getBool("eCrow", true);
  enemyTrap  = prefs.getBool("eTrap", true);
  enemyFruit = prefs.getBool("eFruit", true);
  pigFearMode = prefs.getBool("pigFear", false);   // 🆕
  prefs.end();
  if (savedLat.length() > 0) {
    LATITUDE  = savedLat.toFloat();
    LONGITUDE = savedLon.toFloat();
    TIMEZONE  = savedTz;
  }
  if (psramInit()) Serial.printf("PSRAM OK! Free: %d\n", ESP.getFreePsram());
  if (!imu.begin()) Serial.println("QMI8658 init failed!");
  for (int i = 0; i < MAX_TWINKLE; i++) {
    twinkles[i].x = random(5, 315);
    twinkles[i].y = random(5, 150);
    twinkles[i].phase = random(0, 100);
  }
  for (int i = 0; i < MAX_NOTIFS; i++) notifs[i].active = false;
  for (int i = 0; i < MAX_WOLF_DUST; i++) wolfDust[i].active = false;
  renderBackground();
  updateCelestialState();
  connectWiFi();
  if (WiFi.status() == WL_CONNECTED) { syncTime(); fetchWeather(); }
  unsigned long now = millis();
  lastWeatherUpdate = now;
  lastClockUpdate = now;
  lastPigUpdate = now;
  lastActivityTime = now;    // 🆕
  birdStateTime = now;
  wolfNextSpawn = now + 20000;
  batNextSpawn = now + 35000;
  waspNextSpawn = now + 50000;
  crowNextSpawn = now + 65000;
  trapNextSpawn = now + 12000;
  fruitNextSpawn = now + 8000;
}

void loop() {
  unsigned long now = millis();
  imu.readAccel(ax, ay, az);
  static float sAx = 0, sAy = 0;
  sAx = sAx * 0.85 + ax * 0.15;
  sAy = sAy * 0.85 + ay * 0.15;
  tiltX = constrain(sAy * 1.5, -15, 15);
  tiltY = constrain(sAx * 1.5, -15, 15);
  if (now - lastClockUpdate >= 1000) { lastClockUpdate = now; updateCelestialState(); }
  if (now - lastWeatherUpdate >= WEATHER_INTERVAL) {
    lastWeatherUpdate = now;
    if (WiFi.status() == WL_CONNECTED) fetchWeather();
  }
  handleTouch();
  if (pinkFrozen && now > pinkFreezeEnd) { pinkFrozen = false; pigSpeed = pinkFrozenSpeed; }
  if (whiteFrozen && now > whiteFreezeEnd) { whiteFrozen = false; pig2Speed = whiteFrozenSpeed; }
  if (pinkDizzy && now > pinkDizzyEnd) pinkDizzy = false;
  if (whiteDizzy && now > whiteDizzyEnd) whiteDizzy = false;
  if (pinkInvincible && now > pinkInvincibleEnd) { pinkInvincible = false; addNotification("PINK NORMAL", PIG_PINK); }
  if (whiteInvincible && now > whiteInvincibleEnd) { whiteInvincible = false; addNotification("WHITE NORMAL", PIG_WHITE); }
  if (enemyBird) updateBirdAttack(now);
  if (enemyWolf) updateWolf(now);
  if (enemyBat)  updateBat(now);
  if (enemyWasp) updateWasp(now);
  if (enemyCrow) updateCrow(now);
  updateTrap(now);
  updateFruit(now);
  if (pigsCelebrating && now > pigCelebrateEnd) pigsCelebrating = false;
  if (pigsCelebrating) celebrationRotation += 15;
  bool birdVisible = (birdState == 1 || birdState == 2 || birdState == 4);
  bool pigsCharging = birdVisible && !pigFearMode;   // 🆕 勇敢：衝鋒
  bool pigsFear = birdVisible && pigFearMode;        // 🆕 害怕：逃跑
  bool wolfThreat = (wolfState == 1 || wolfState == 2);

  // 🆕 活動偵測 + 睡觉邏輯
  bool anyActivity = 
    (birdState != 0) || (wolfState != 0) || (batState != 0) ||
    (waspState != 0) || (crowState != 0) ||
    (trapState != 0) || (fruitState != 0) ||
    collisionActive || lastTouchState ||
    pinkDizzy || whiteDizzy || pinkFrozen || whiteFrozen;
  if (anyActivity) {
    lastActivityTime = now;
    if (pigSleeping) { pigSleeping = false; addNotification("WAKE UP!", 0xFFE0); }
  } else {
    if (!pigSleeping && now - lastActivityTime > SLEEP_DELAY) {
      pigSleeping = true;
      addNotification("ZZZ...", 0x5D9B);
    }
  }

  if (now < shakeEndTime) { shakeX = random(-3, 4); shakeY = random(-3, 4); }
  else { shakeX = 0; shakeY = 0; }

  canvas.pushImage(0, 0, 320, 240, (uint16_t*)bgSprite.getBuffer());
  drawCelestialBody();
  float starAlpha = 1.0 - currentSunAlpha;
  if (starAlpha > 0.3) {
    for (int i = 0; i < MAX_TWINKLE; i++) {
      if ((now / 300 + twinkles[i].phase) % 2 == 0) {
        canvas.drawPixel(twinkles[i].x, twinkles[i].y, STAR_COLOR);
        canvas.drawPixel(twinkles[i].x + 1, twinkles[i].y, STAR_COLOR);
        canvas.drawPixel(twinkles[i].x, twinkles[i].y + 1, STAR_COLOR);
        canvas.drawPixel(twinkles[i].x + 1, twinkles[i].y + 1, STAR_COLOR);
      }
    }
  }
  drawWeatherPanel();
  if (currentView == 0) drawDigitalClock();
  else drawAnalogClock();

  // 🆕 睡觉時唔動
  if (now - lastPigUpdate >= 40 && !pigSleeping) {
    lastPigUpdate = now;
    bool pinkCanMove = !pinkFrozen && !pinkDizzy;
    bool whiteCanMove = !whiteFrozen && !whiteDizzy;
    bool bothOnScreen = (pigX >= 20 && pigX <= 300 && pig2X >= 20 && pig2X <= 300);
    if (pigsCharging) {
      if (pinkCanMove) { if (pigX < pig2X) pigX += 6; else pigX -= 6; }
      if (whiteCanMove) { if (pig2X > pigX) pig2X -= 6; else pig2X += 6; }
      if (pinkCanMove && whiteCanMove && !collisionActive && bothOnScreen && abs(pigX - pig2X) < 35) {
        collisionActive = true;
        collisionStartTime = now;
        pigSpeed = (pigX < pig2X) ? -2 : 2;
        pig2Speed = (pig2X > pigX) ? 2 : -2;
        if (birdState != 0 && birdState != 5) {
          birdState = 5; birdStateTime = now;
          birdDeathAngle = 0; birdDeathVY = -4;
          spawnFeathers(birdX, birdY); spawnFeathers(birdX, birdY);
          shakeEndTime = now + 600; flashEndTime = now + 100;
          pigsCelebrating = true; pigCelebrateEnd = now + 2000;
          celebrationRotation = 0;
          addNotification("BIRD KO!", 0xFEA0);
        }
      }
    } else if (pigsFear || wolfThreat) {
      int threatX = pigsFear ? birdX : wolfX;   // 🆕 根據威脅源選擇
      if (pinkCanMove) { if (pigX < threatX) pigX -= 3; else pigX += 3; }
      if (whiteCanMove) { if (pig2X < threatX) pig2X -= 3; else pig2X += 3; }
      if (pigX < -40) pigX = -40;
      if (pigX > 340) pigX = 340;
      if (pig2X < -40) pig2X = -40;
      if (pig2X > 340) pig2X = 340;
    } else {
      if (pinkCanMove) pigX += pigSpeed;
      if (whiteCanMove) pig2X += pig2Speed;

      // 🆕 物理碰撞（推開 + 彈性交換速度）
      if (pinkCanMove && whiteCanMove && bothOnScreen) {
        float dx = pig2X - pigX;
        float dist = abs(dx);
        const float PIG_RADIUS = 18.0f;   // 豬半徑

        if (dist < PIG_RADIUS * 2 && dist > 0.1f) {
          // 1️⃣ 硬推開（防止重疊）
          float overlap = PIG_RADIUS * 2 - dist;
          int push = (int)(overlap * 0.5f) + 1;
          if (dx > 0) { pigX -= push; pig2X += push; }
          else { pigX += push; pig2X -= push; }

          // 2️⃣ 彈性碰撞（等質量 → 交換速度）
          if (!collisionActive) {
            int tmp = pigSpeed;
            pigSpeed = pig2Speed;
            pig2Speed = tmp;

            collisionActive = true;
            collisionStartTime = now;
            addNotification("DOKI!", 0xFC9F);
          }
        }
      }

      // 邊界重生（防重疊）
      if (pigX < -20) {
        pigX = 340;
        if (pinkCanMove) pigSpeed = -2;
        if (abs(pig2X - pigX) < 60) pigX = 260;
      }
      if (pigX > 340) {
        pigX = -20;
        if (pinkCanMove) pigSpeed = 2;
        if (abs(pig2X - pigX) < 60) pigX = 60;
      }
      if (pig2X < -20) {
        pig2X = 340;
        if (whiteCanMove) pig2Speed = -2;
        if (abs(pigX - pig2X) < 60) pig2X = 260;
      }
      if (pig2X > 340) {
        pig2X = -20;
        if (whiteCanMove) pig2Speed = 2;
        if (abs(pigX - pig2X) < 60) pig2X = 60;
      }
    
    }
    if (collisionActive && now - collisionStartTime > 800) collisionActive = false;
  }
  int bounce = (pigX / 4) % 2 == 0 ? 0 : -2;
  int bounce2 = (pig2X / 4) % 2 == 0 ? 0 : -2;
  bool pinkFR, whiteFR;
  if (pigsCharging) { pinkFR = (pig2X > pigX); whiteFR = (pigX < pig2X);  } 
  else if (pigsFear) {
    pinkFR = (pigX > birdX);     // 🆕 害怕時面向鳥的相反方向
    whiteFR = (pig2X > birdX);
  } else if (wolfThreat) {
    pinkFR = (pigX > wolfX);
    whiteFR = (pig2X > wolfX);
  } else if (collisionStartTime > 0 && now - collisionStartTime < 1500) { pinkFR = (pigSpeed > 0); whiteFR = (pig2Speed > 0); }
  else { pinkFR = (pigSpeed > 0); whiteFR = (pig2X < 160); }
  int pinkWobbleX = 0, pinkWobbleY = 0, whiteWobbleX = 0, whiteWobbleY = 0;
  if (pinkDizzy) { pinkWobbleX = sin(now / 100.0) * 3; pinkWobbleY = cos(now / 80.0) * 2; }
  if (whiteDizzy) { whiteWobbleX = sin(now / 100.0 + 1.5) * 3; whiteWobbleY = cos(now / 80.0 + 1.5) * 2; }
  bool bothOnScreenDraw = (pigX >= 20 && pigX <= 300 && pig2X >= 20 && pig2X <= 300);
  if (collisionActive && bothOnScreenDraw) {
    int midX = (pigX + pig2X) / 2;
    drawAnimeEffect(midX, pigY - 20);
    drawPig(pigX, pigY, PIG_PINK, PIG_PINK_DARK, pinkFR, pinkFrozen, false, false);
    drawPig(pig2X, pigY, PIG_WHITE, PIG_WHITE_DARK, whiteFR, whiteFrozen, false, false);
  } else {
    drawPig(pigX + pinkWobbleX, pigY + pinkWobbleY + (pinkFrozen ? 0 : bounce), PIG_PINK, PIG_PINK_DARK, pinkFR, pinkFrozen, false, pinkDizzy);
    drawPig(pig2X + whiteWobbleX, pigY + whiteWobbleY + (whiteFrozen ? 0 : bounce2), PIG_WHITE, PIG_WHITE_DARK, whiteFR, whiteFrozen, false, whiteDizzy);
  }
  if (pinkFrozen) drawFreezeEffect(pigX, pigY);
  if (whiteFrozen) drawFreezeEffect(pig2X, pigY);
  if (pinkDizzy) drawDizzyEffect(pigX + pinkWobbleX, pigY + pinkWobbleY);
  if (whiteDizzy) drawDizzyEffect(pig2X + whiteWobbleX, pigY + whiteWobbleY);
  if (enemyBird) { if (birdState == 5) drawDyingBird(birdX, birdY, birdDeathAngle); else if (birdState != 0) drawBird(birdX, birdY, birdWingAngle); }
  if (enemyWolf) { if (wolfState == 5) drawDyingWolf(wolfX, wolfY, wolfDeathAngle); else if (wolfState != 0) drawWolf(wolfX, wolfY, wolfDir, wolfLegAngle); }
  if (enemyBat) { if (batState == 5) drawDyingBat(batX, batY); else if (batState != 0) drawBat(batX, batY, batWingAngle); }
  if (enemyWasp) { if (waspState == 5) drawDyingWasp(waspX, waspY); else if (waspState != 0) drawWasp(waspX, waspY, waspDir, waspWingAngle); }
  if (enemyCrow) { if (crowState == 5) drawDyingCrow(crowX, crowY); else if (crowState != 0) drawCrow(crowX, crowY, crowWingAngle); }
  drawTrap();
  drawFruit();
  if (pigsCelebrating) drawPigCelebration();
  updateFeathers();
  updateSnow();
  updateNotifications();
  drawNotifications();
  drawNextEnemyBox();
  if (millis() < flashEndTime) canvas.fillRect(0, 0, 320, 240, 0xFFFF);
  canvas.pushSprite(shakeX, shakeY);
  delay(16);
}
// ============================================================
// drawPig — 含睡觉造型
// ============================================================
void drawPig(int x, int y, uint16_t body, uint16_t dark, bool facingRight, bool frozen, bool fear, bool dizzy) {
  if (frozen) { body = ICE_BLUE; dark = ICE_LIGHT; }
  bool isPinkPig = (body == PIG_PINK || body == 0xF9A8);
  bool inv = isPinkPig ? pinkInvincible : whiteInvincible;
  if (inv) {
    int gr = 18 + (int)(sin(millis()/100.0)*2);
    canvas.drawCircle(x, y, gr, 0xFFE0);
    canvas.drawCircle(x, y, gr-2, 0xFBE0);
    for (int i = 0; i < 3; i++) {
      float a = (i*120 + millis()/30.0)*DEG_TO_RAD;
      canvas.fillCircle(x+15*cos(a), y-20+8*sin(a), 2, 0xFFE0);
    }
  }
  uint16_t snout = frozen ? ICE_WHITE : ((body == PIG_PINK) ? 0xFC9F : 0xB5B6);
  uint16_t eyeColor = frozen ? ICE_WHITE : 0x0000;

  // 💤 睡觉造型
  if (pigSleeping) {
    canvas.fillEllipse(x, y + 2, 14, 6, body);
    canvas.fillEllipse(x - 3, y, 7, 3, dark);
    if (facingRight) {
      canvas.fillCircle(x + 12, y + 1, 7, body);
      canvas.fillTriangle(x + 7, y - 5, x + 9, y - 11, x + 14, y - 6, body);
      canvas.fillEllipse(x + 17, y + 2, 3, 3, snout);
      canvas.drawLine(x + 11, y - 1, x + 15, y - 1, 0x0000);
      canvas.drawLine(x + 15, y - 1, x + 15, y + 1, 0x0000);
    } else {
      canvas.fillCircle(x - 12, y + 1, 7, body);
      canvas.fillTriangle(x - 7, y - 5, x - 9, y - 11, x - 14, y - 6, body);
      canvas.fillEllipse(x - 17, y + 2, 3, 3, snout);
      canvas.drawLine(x - 11, y - 1, x - 15, y - 1, 0x0000);
      canvas.drawLine(x - 15, y - 1, x - 15, y + 1, 0x0000);
    }
    canvas.fillRect(x - 6, y + 6, 3, 4, body);
    canvas.fillRect(x - 1, y + 6, 3, 4, body);
    canvas.fillRect(x + 4, y + 6, 3, 4, body);
    canvas.drawCircle(x - (facingRight ? 13 : -13), y + 1, 3, dark);
    unsigned long now = millis();
    for (int i = 0; i < 3; i++) {
      int ph = (now / 500 + i) % 3;
      int zx = x + (facingRight ? 15 : -15) + ph * 4;
      int zy = y - 8 - ph * 5;
      canvas.setTextColor(0x5D9B - ph * 0x0820);
      canvas.setTextSize(1 + ph / 2);
      canvas.setCursor(zx, zy);
      canvas.println("z");
    }
    return;
  }

  // 正常造型
  int legPhase = (millis() / 80) % 2;
  int legOffset = legPhase ? 1 : -1;

  if (facingRight) {
    canvas.fillEllipse(x, y, 11, 8, body);
    canvas.fillEllipse(x-2, y-2, 6, 3, dark);
    canvas.fillCircle(x+12, y-2, 8, body);
    canvas.fillTriangle(x+7, y-8, x+9, y-15, x+14, y-9, body);
    canvas.fillTriangle(x+9, y-9, x+10, y-13, x+13, y-10, dark);
    canvas.fillEllipse(x+18, y-2, 4, 3, snout);
    canvas.fillCircle(x+16, y-2, 1, eyeColor);
    canvas.fillCircle(x+20, y-2, 1, eyeColor);
    if (dizzy) {
      canvas.drawCircle(x+13, y-5, 2, 0x0000);
      canvas.drawCircle(x+13, y-5, 3, 0x0000);
      canvas.drawCircle(x+13, y-5, 4, 0x0000);
    } else if (fear) {
      canvas.fillCircle(x+13, y-5, 4, 0xFFFF);
      canvas.fillCircle(x+14, y-6, 1, 0x0000);
    } else {
      canvas.fillCircle(x+13, y-5, 2, eyeColor);
      if (!frozen) canvas.fillCircle(x+12, y-6, 1, 0xFFFF);
    }
    if (!frozen) canvas.fillCircle(x+10, y+1, 2, 0xFBE0);
    canvas.fillRect(x-7, y+6+legOffset, 3, 6, body);
    canvas.fillRect(x-2, y+6-legOffset, 3, 6, body);
    canvas.fillRect(x+3, y+6+legOffset, 3, 6, body);
    canvas.fillRect(x+8, y+6-legOffset, 3, 6, body);
    canvas.drawCircle(x-13, y-1, 3, dark);
    canvas.drawCircle(x-13, y-1, 1, body);
  } else {
    canvas.fillEllipse(x, y, 11, 8, body);
    canvas.fillEllipse(x+2, y-2, 6, 3, dark);
    canvas.fillCircle(x-12, y-2, 8, body);
    canvas.fillTriangle(x-7, y-8, x-9, y-15, x-14, y-9, body);
    canvas.fillTriangle(x-9, y-9, x-10, y-13, x-13, y-10, dark);
    canvas.fillEllipse(x-18, y-2, 4, 3, snout);
    canvas.fillCircle(x-16, y-2, 1, eyeColor);
    canvas.fillCircle(x-20, y-2, 1, eyeColor);
    if (dizzy) {
      canvas.drawCircle(x-13, y-5, 2, 0x0000);
      canvas.drawCircle(x-13, y-5, 3, 0x0000);
      canvas.drawCircle(x-13, y-5, 4, 0x0000);
    } else if (fear) {
      canvas.fillCircle(x-13, y-5, 4, 0xFFFF);
      canvas.fillCircle(x-14, y-6, 1, 0x0000);
    } else {
      canvas.fillCircle(x-13, y-5, 2, eyeColor);
      if (!frozen) canvas.fillCircle(x-14, y-6, 1, 0xFFFF);
    }
    if (!frozen) canvas.fillCircle(x-10, y+1, 2, 0xFBE0);
    canvas.fillRect(x+7, y+6+legOffset, 3, 6, body);
    canvas.fillRect(x+2, y+6-legOffset, 3, 6, body);
    canvas.fillRect(x-3, y+6+legOffset, 3, 6, body);
    canvas.fillRect(x-8, y+6-legOffset, 3, 6, body);
    canvas.drawCircle(x+13, y-1, 3, dark);
    canvas.drawCircle(x+13, y-1, 1, body);
  }
}

// ============================================================
// 大鸟
// ============================================================
void updateBirdAttack(unsigned long now) {
  switch (birdState) {
    case 0: if (now - birdStateTime > 15000) { birdState = 1; birdStateTime = now; birdX = random(60, 260); birdY = -30; birdWingAngle = 0; birdTarget = random(1, 3); } break;
    case 1: birdY += 3.5; birdX += sin(now / 300.0) * 2; birdWingAngle = sin(now / 100.0) * 0.5; if (birdY > 50) { birdState = 2; birdStateTime = now; } break;
    case 2: {
      int tX = (birdTarget == 1) ? pigX : pig2X;
      int tY = pigY - 10;
      float dx = tX - birdX, dy = tY - birdY, d = sqrt(dx*dx + dy*dy);
      if (d < 5) {
        bool inv = (birdTarget == 1) ? pinkInvincible : whiteInvincible;
        if (!inv) { if (birdTarget == 1) { pinkDizzy = true; pinkDizzyEnd = now + DIZZY_DURATION; } else { whiteDizzy = true; whiteDizzyEnd = now + DIZZY_DURATION; } }
        else { addNotification("REFLECT!", 0xFFE0); flashEndTime = now + 100; }
        birdState = 3; birdStateTime = now; shakeEndTime = now + 300; spawnFeathers(tX, tY);
      } else { birdX += (dx/d)*6; birdY += (dy/d)*6; birdWingAngle = sin(now/50.0)*0.8; }
      break;
    }
    case 3: if (now - birdStateTime > 600) { birdState = 4; birdStateTime = now; } break;
    case 4: birdY -= 3; birdX += sin(now / 200.0) * 2; birdWingAngle = sin(now / 80.0) * 0.8; if (birdY < -50) { birdState = 0; birdStateTime = now; } break;
    case 5: birdDeathAngle += 15; birdDeathVY += 0.5; birdY += birdDeathVY; birdX += sin(now / 100.0) * 1.5; if (now - birdStateTime > 1500 || birdY > 260) { birdState = 0; birdStateTime = now; } break;
  }
}

void drawBird(int x, int y, float wingAngle) {
  int wo = sin(wingAngle) * 8;
  canvas.fillTriangle(x-5, y, x-22, y+wo-5, x-10, y+8, BIRD_DARK);
  canvas.fillTriangle(x+5, y, x+22, y+wo-5, x+10, y+8, BIRD_DARK);
  canvas.fillEllipse(x, y, 12, 10, BIRD_BODY);
  canvas.fillCircle(x, y-8, 8, BIRD_BODY);
  canvas.fillTriangle(x-5, y-8, x+5, y-8, x, y-2, BIRD_BEAK);
  canvas.fillCircle(x-3, y-10, 2, BIRD_EYE);
  canvas.fillCircle(x+3, y-10, 2, BIRD_EYE);
  canvas.fillCircle(x-3, y-11, 1, 0xFFFF);
  canvas.fillCircle(x+3, y-11, 1, 0xFFFF);
  canvas.fillTriangle(x-8, y+8, x+8, y+8, x, y+18, BIRD_DARK);
}

void drawDyingBird(int x, int y, float angle) {
  int w = sin(angle * DEG_TO_RAD) * 5;
  canvas.fillTriangle(x-5, y, x-20, y+w+5, x-10, y+8, BIRD_DARK);
  canvas.fillTriangle(x+5, y, x+20, y+w+5, x+10, y+8, BIRD_DARK);
  canvas.fillEllipse(x, y, 12, 10, BIRD_BODY);
  canvas.fillCircle(x, y-8, 8, BIRD_BODY);
  canvas.fillTriangle(x-5, y-8, x+5, y-8, x, y-2, BIRD_BEAK);
  canvas.drawLine(x-5, y-12, x-1, y-8, 0x0000);
  canvas.drawLine(x-1, y-12, x-5, y-8, 0x0000);
  canvas.drawLine(x+1, y-12, x+5, y-8, 0x0000);
  canvas.drawLine(x+5, y-12, x+1, y-8, 0x0000);
  canvas.fillRect(x-2, y-2, 4, 5, 0xF81F);
  canvas.fillTriangle(x-8, y+8, x+8, y+8, x, y+18, BIRD_DARK);
  canvas.setTextColor(0xF800); canvas.setTextSize(3);
  canvas.setCursor(x-20, y-40); canvas.println("KO!");
  canvas.drawRect(x-22, y-42, 55, 28, 0xFFFF);
}

void spawnFeathers(int x, int y) {
  for (int i = 0; i < MAX_FEATHERS; i++) {
    if (!feathers[i].active) {
      feathers[i].x = x; feathers[i].y = y;
      feathers[i].vx = random(-30, 31) / 10.0;
      feathers[i].vy = random(-40, 1) / 10.0;
      feathers[i].life = random(20, 50);
      feathers[i].active = true;
    }
  }
}

void updateFeathers() {
  for (int i = 0; i < MAX_FEATHERS; i++) {
    if (feathers[i].active) {
      feathers[i].life--;
      feathers[i].x += feathers[i].vx;
      feathers[i].y += feathers[i].vy;
      feathers[i].vy += 0.15;
      feathers[i].vx *= 0.98;
      if (feathers[i].life <= 0 || feathers[i].y > 240) feathers[i].active = false;
      else {
        canvas.drawLine(feathers[i].x, feathers[i].y, feathers[i].x+3, feathers[i].y+2, BIRD_FEATHER);
        canvas.drawLine(feathers[i].x, feathers[i].y, feathers[i].x-2, feathers[i].y+3, BIRD_FEATHER);
      }
    }
  }
}

bool checkBirdTouch(uint16_t tx, uint16_t ty) {
  if (birdState == 0 || birdState == 5) return false;
  return (abs((int)tx - birdX) < 25 && abs((int)ty - birdY) < 25);
}

void killBird() {
  canvas.fillCircle(birdX, birdY, 20, 0xFFFF);
  canvas.setTextColor(0xF800); canvas.setTextSize(2);
  canvas.setCursor(birdX-20, birdY-10); canvas.println("KILL!");
  canvas.pushSprite(0, 0);
  delay(200);
  birdState = 5; birdStateTime = millis();
  birdDeathAngle = 0; birdDeathVY = -3;
  flashEndTime = millis() + 100;
  addNotification("BAM!", 0xFEA0);
}

// ============================================================
// 狼
// ============================================================
void spawnWolfDust(int x, int y) {
  for (int i = 0; i < MAX_WOLF_DUST; i++) {
    if (!wolfDust[i].active) {
      wolfDust[i].x = x + random(-8, 8); wolfDust[i].y = y + random(-2, 4);
      wolfDust[i].vx = random(-15, 16) / 10.0; wolfDust[i].vy = random(-25, -5) / 10.0;
      wolfDust[i].life = random(15, 30); wolfDust[i].active = true;
      break;
    }
  }
}

void updateWolfDust() {
  for (int i = 0; i < MAX_WOLF_DUST; i++) {
    if (wolfDust[i].active) {
      wolfDust[i].life--;
      wolfDust[i].x += wolfDust[i].vx;
      wolfDust[i].y += wolfDust[i].vy;
      wolfDust[i].vy += 0.1;
      wolfDust[i].vx *= 0.95;
      if (wolfDust[i].life <= 0) wolfDust[i].active = false;
    }
  }
}

void drawWolfDust() {
  for (int i = 0; i < MAX_WOLF_DUST; i++) {
    if (wolfDust[i].active) {
      int r = 1 + (wolfDust[i].life / 10);
      canvas.fillCircle(wolfDust[i].x, wolfDust[i].y, r, 0x7BEF);
    }
  }
}

void drawWolfClaw(int x, int y) {
  canvas.drawLine(x-20, y-15, x-5, y+5, 0xF800);
  canvas.drawLine(x-15, y-18, x, y+2, 0xF800);
  canvas.drawLine(x-10, y-20, x+5, y-1, 0xF800);
  canvas.drawLine(x-19, y-15, x-6, y+3, 0xFBE0);
  canvas.drawLine(x-14, y-18, x-1, y, 0xFBE0);
  canvas.drawLine(x-9, y-20, x+4, y-2, 0xFBE0);
}

void updateWolf(unsigned long now) {
  switch (wolfState) {
    case 0:
      if (now > wolfNextSpawn) {
        wolfState = 1; wolfStateTime = now;
        if (random(0, 2) == 0) { wolfX = -50; wolfDir = 1; } else { wolfX = 370; wolfDir = -1; }
        wolfY = 224; wolfTarget = random(1, 3); wolfHowlEnd = now + 800;
      }
      break;
    case 1:
      wolfX += wolfDir * 3; wolfLegAngle += 0.3;
      if ((millis() / 50) % 2 == 0) spawnWolfDust(wolfX, wolfY + 8);
      if ((wolfDir == 1 && wolfX > 50) || (wolfDir == -1 && wolfX < 270)) { wolfState = 2; wolfStateTime = now; }
      break;
    case 2: {
      int tX = (wolfTarget == 1) ? pigX : pig2X;
      wolfDir = (tX > wolfX) ? 1 : -1;
      wolfX += wolfDir * 2; wolfLegAngle += 0.3;
      if (millis() % 40 == 0) spawnWolfDust(wolfX, wolfY + 8);
      bool hit = false;
      if (abs(wolfX - pigX) < 28) {
        if (pinkInvincible) { wolfState = 4; wolfStateTime = now; flashEndTime = now + 100; addNotification("REFLECT!", 0xFBE0); }
        else { pinkDizzy = true; pinkDizzyEnd = now + 2000; }
        hit = true;
      } else if (abs(wolfX - pig2X) < 28) {
        if (whiteInvincible) { wolfState = 4; wolfStateTime = now; flashEndTime = now + 100; addNotification("REFLECT!", 0xFBE0); }
        else { whiteDizzy = true; whiteDizzyEnd = now + 2000; }
        hit = true;
      }
      if (hit) { if (wolfState != 4) { wolfState = 3; wolfStateTime = now; } shakeEndTime = now + 300; wolfClawEnd = now + 300; }
      if (now - wolfStateTime > 8000 && wolfState == 2) { wolfState = 4; wolfStateTime = now; }
      break;
    }
    case 3: wolfLegAngle += 0.8; if (now - wolfStateTime > 600) { wolfState = 4; wolfStateTime = now; } break;
    case 4:
      wolfX += wolfDir * 4; wolfLegAngle += 0.5;
      if (millis() % 60 == 0) spawnWolfDust(wolfX, wolfY + 8);
      if (wolfX < -60 || wolfX > 380) { wolfState = 0; wolfNextSpawn = now + 15000 + random(10000); }
      break;
    case 5:
      wolfDeathAngle += 18; wolfDeathVY += 0.6; wolfY += wolfDeathVY;
      if (now - wolfStateTime > 1500 || wolfY > 260) { wolfState = 0; wolfNextSpawn = now + 15000 + random(10000); }
      break;
  }
  updateWolfDust();
}

void drawWolf(int x, int y, int dir, float legAngle) {
  unsigned long now = millis();
  bool howling = (now < wolfHowlEnd);
  bool chasing = (wolfState == 2);
  if (chasing) {
    int pulseR = 20 + (int)(sin(now / 80.0) * 3);
    canvas.drawCircle(x, y, pulseR, 0xF800);
    canvas.drawCircle(x, y, pulseR + 2, 0x7800);
  }
  canvas.fillEllipse(x, y, 14, 9, WOLF_BROWN);
  canvas.fillEllipse(x - 3, y - 3, 8, 4, WOLF_DARK);
  int headX = x + dir * 12;
  canvas.fillEllipse(headX, y - 2, 9, 8, WOLF_BROWN);
  canvas.fillTriangle(headX - 7, y - 8, headX - 10, y - 18, headX - 2, y - 10, WOLF_DARK);
  canvas.fillTriangle(headX + 7, y - 8, headX + 10, y - 18, headX + 2, y - 10, WOLF_DARK);
  canvas.fillEllipse(headX + dir * 7, y - 1, 5, 3, WOLF_DARK);
  if (howling) {
    canvas.fillTriangle(headX + dir*4, y+1, headX + dir*4, y+6, headX + dir*10, y+3, 0x0000);
    canvas.drawPixel(headX + dir*5, y+2, 0xFFFF);
    canvas.drawPixel(headX + dir*7, y+3, 0xFFFF);
    int waveR = 12 + ((now - (wolfHowlEnd - 800)) / 40);
    canvas.drawCircle(headX, y-2, waveR, 0xFFE0);
    canvas.drawCircle(headX, y-2, waveR+5, 0xFBE0);
  } else {
    canvas.drawLine(headX + dir*5, y+2, headX + dir*9, y+1, 0xFFFF);
  }
  int eyeGlow = chasing ? 3 : 2;
  uint16_t eyeColor = chasing ? 0xF800 : WOLF_EYE;
  canvas.fillCircle(headX - 2, y - 5, eyeGlow, eyeColor);
  canvas.fillCircle(headX + 4, y - 5, eyeGlow, eyeColor);
  canvas.fillCircle(headX - 2 + dir, y - 5, 1, 0x0000);
  canvas.fillCircle(headX + 4 + dir, y - 5, 1, 0x0000);
  if (chasing && (millis() / 100) % 2 == 0) {
    canvas.drawCircle(headX - 2, y - 5, eyeGlow + 2, 0xF800);
    canvas.drawCircle(headX + 4, y - 5, eyeGlow + 2, 0xF800);
  }
  int legOffset = sin(legAngle) * 2;
  canvas.fillRect(x - 8, y + 6 + legOffset, 3, 6, WOLF_BROWN);
  canvas.fillRect(x - 3, y + 6 - legOffset, 3, 6, WOLF_BROWN);
  canvas.fillRect(x + 2, y + 6 + legOffset, 3, 6, WOLF_BROWN);
  canvas.fillRect(x + 7, y + 6 - legOffset, 3, 6, WOLF_BROWN);
  canvas.fillTriangle(x - dir*12, y-3, x - dir*22, y-8, x - dir*20, y+2, WOLF_DARK);
  drawWolfDust();
  if (now < wolfClawEnd) { drawWolfClaw(pigX, pigY); drawWolfClaw(pig2X, pigY); }
}

void drawDyingWolf(int x, int y, float angle) {
  int w = sin(angle * DEG_TO_RAD) * 4;
  canvas.fillEllipse(x, y, 14, 9, WOLF_BROWN);
  int headX = x + 12;
  canvas.fillEllipse(headX, y - 2 + w, 9, 8, WOLF_BROWN);
  canvas.drawLine(headX-4, y-7, headX-1, y-4, 0x0000);
  canvas.drawLine(headX-1, y-7, headX-4, y-4, 0x0000);
  canvas.drawLine(headX+2, y-7, headX+5, y-4, 0x0000);
  canvas.drawLine(headX+5, y-7, headX+2, y-4, 0x0000);
  canvas.fillRect(headX+3, y+1, 3, 5, 0xF81F);
  canvas.setTextColor(0xF800); canvas.setTextSize(2);
  canvas.setCursor(x-18, y-38); canvas.println("KO!");
}

bool checkWolfTouch(uint16_t tx, uint16_t ty) {
  if (wolfState == 0 || wolfState == 5) return false;
  return (abs((int)tx - wolfX) < 35 && abs((int)ty - wolfY) < 30);
}

void killWolf() {
  canvas.fillCircle(wolfX, wolfY, 22, 0xFFFF);
  canvas.setTextColor(0xF800); canvas.setTextSize(2);
  canvas.setCursor(wolfX-22, wolfY-10); canvas.println("BOOM!");
  canvas.pushSprite(0, 0);
  delay(200);
  wolfState = 5; wolfStateTime = millis();
  wolfDeathAngle = 0; wolfDeathVY = -3;
  flashEndTime = millis() + 100;
  addNotification("BOOM!", WOLF_BROWN);
}

// ============================================================
// 蝙蝠
// ============================================================
void updateBat(unsigned long now) {
  switch (batState) {
    case 0: if (now > batNextSpawn) { batState = 1; batStateTime = now; batX = random(60, 260); batY = -30; batWavePhase = 0; } break;
    case 1: batY += 3; batWavePhase += 0.15; batWingAngle = sin(now / 60.0) * 0.8; if (batY > 40) { batState = 2; batStateTime = now; } break;
    case 2:
      batY += sin(now / 200.0) * 1.5;
      batX += sin(now / 150.0) * 2;
      batWingAngle = sin(now / 50.0) * 0.9;
      if (batY < 20) batY = 20; if (batY > 150) batY = 150;
      if (batX < 20) batX = 20; if (batX > 300) batX = 300;
      if (now - batStateTime > 5000 && !pinkDizzy && !whiteDizzy) {
        int targetX = (abs(batX - pigX) < abs(batX - pig2X)) ? pigX : pig2X;
        batX += (targetX - batX) * 0.1;
        batY += (pigY - 50 - batY) * 0.1;
        if (abs(batX - targetX) < 30 && batY > 150) {
          bool inv = (targetX == pigX) ? pinkInvincible : whiteInvincible;
          if (inv) { addNotification("REFLECT!", 0xFFE0); flashEndTime = now + 100; }
          else if (targetX == pigX) { pinkDizzy = true; pinkDizzyEnd = now + 2500; }
          else { whiteDizzy = true; whiteDizzyEnd = now + 2500; }
          batState = 3; batStateTime = now; shakeEndTime = now + 300;
        }
      }
      if (now - batStateTime > 10000) { batState = 4; batStateTime = now; }
      break;
    case 3: if (now - batStateTime > 500) { batState = 4; batStateTime = now; } break;
    case 4: batY -= 4; batWingAngle = sin(now / 40.0) * 0.9; if (batY < -50) { batState = 0; batNextSpawn = now + 25000 + random(15000); } break;
    case 5: if (now - batStateTime > 1500) { batState = 0; batNextSpawn = now + 25000 + random(15000); } break;
  }
}

void drawBat(int x, int y, float wingAngle) {
  canvas.drawCircle(x, y, 14, 0x5D9B);
  canvas.drawCircle(x, y, 15, 0x3186);
  int wo = sin(wingAngle) * 6;
  canvas.fillTriangle(x-3, y, x-22, y-6+wo, x-10, y+6, BAT_BODY);
  canvas.fillTriangle(x+3, y, x+22, y-6+wo, x+10, y+6, BAT_BODY);
  canvas.fillEllipse(x, y, 6, 8, BAT_BODY);
  canvas.fillCircle(x, y-8, 5, BAT_BODY);
  canvas.fillTriangle(x-5, y-8, x-8, y-16, x-2, y-10, BAT_BODY);
  canvas.fillTriangle(x+5, y-8, x+8, y-16, x+2, y-10, BAT_BODY);
  canvas.fillCircle(x-2, y-9, 1, 0xF800);
  canvas.fillCircle(x+2, y-9, 1, 0xF800);
  canvas.fillCircle(x, y-14, 1, 0xFFFF);
}

void drawDyingBat(int x, int y) {
  canvas.fillEllipse(x, y, 6, 8, BAT_BODY);
  canvas.fillCircle(x, y+8, 5, BAT_BODY);
  canvas.drawLine(x-3, y+7, x-1, y+9, 0xF800);
  canvas.drawLine(x-1, y+7, x-3, y+9, 0xF800);
  canvas.drawLine(x+1, y+7, x+3, y+9, 0xF800);
  canvas.drawLine(x+3, y+7, x+1, y+9, 0xF800);
  canvas.setTextColor(0xF800); canvas.setTextSize(2);
  canvas.setCursor(x-20, y-25); canvas.println("KO!");
}

bool checkBatTouch(uint16_t tx, uint16_t ty) {
  if (batState == 0 || batState == 5) return false;
  return (abs((int)tx - batX) < 25 && abs((int)ty - batY) < 25);
}

void killBat() {
  canvas.fillCircle(batX, batY, 20, 0xFFFF);
  canvas.setTextColor(0xF800); canvas.setTextSize(2);
  canvas.setCursor(batX-20, batY-10); canvas.println("BAM!");
  canvas.pushSprite(0, 0);
  delay(200);
  batState = 5; batStateTime = millis();
  flashEndTime = millis() + 100;
  addNotification("BAT DOWN!", 0x8410);
}

// ============================================================
// 黄蜂
// ============================================================
void updateWasp(unsigned long now) {
  switch (waspState) {
    case 0: if (now > waspNextSpawn) { waspState = 1; waspStateTime = now; if (random(0, 2) == 0) { waspX = -30; waspDir = 1; } else { waspX = 350; waspDir = -1; } waspY = 150 + random(-50, 50); } break;
    case 1: waspX += waspDir * 6; waspY += sin(now / 100.0) * 1; waspWingAngle = sin(now / 20.0) * 0.9; if ((waspDir == 1 && waspX > 60) || (waspDir == -1 && waspX < 260)) { waspState = 2; waspStateTime = now; } break;
    case 2:
      waspX += waspDir * 4; waspY += sin(now / 80.0) * 3; waspWingAngle = sin(now / 15.0) * 0.9;
      if (waspY < 30) waspY = 30; if (waspY > 210) waspY = 210;
      if (waspX < 10) { waspX = 10; waspDir = 1; } if (waspX > 310) { waspX = 310; waspDir = -1; }
      if (abs(waspX - pigX) < 20 && abs(waspY - pigY) < 20) {
        if (pinkInvincible) { addNotification("REFLECT!", 0xFFE0); flashEndTime = now + 100; }
        else { pinkDizzy = true; pinkDizzyEnd = now + 2500; }
        waspState = 3; waspStateTime = now; shakeEndTime = now + 200;
      } else if (abs(waspX - pig2X) < 20 && abs(waspY - pigY) < 20) {
        if (whiteInvincible) { addNotification("REFLECT!", 0xFFE0); flashEndTime = now + 100; }
        else { whiteDizzy = true; whiteDizzyEnd = now + 2500; }
        waspState = 3; waspStateTime = now; shakeEndTime = now + 200;
      }
      if (now - waspStateTime > 6000) { waspState = 4; waspStateTime = now; }
      break;
    case 3: if (now - waspStateTime > 400) { waspState = 4; waspStateTime = now; } break;
    case 4: waspX += waspDir * 8; waspWingAngle = sin(now / 15.0) * 0.9; if (waspX < -50 || waspX > 380) { waspState = 0; waspNextSpawn = now + 20000 + random(15000); } break;
    case 5: if (now - waspStateTime > 1200) { waspState = 0; waspNextSpawn = now + 20000 + random(15000); } break;
  }
}

void drawWasp(int x, int y, int dir, float wingAngle) {
  int wo = sin(wingAngle) * 3;
  canvas.fillEllipse(x-4, y-6+wo, 5, 3, 0xBDFF);
  canvas.fillEllipse(x+4, y-6+wo, 5, 3, 0xBDFF);
  canvas.fillEllipse(x, y, 5, 8, WASP_BODY);
  canvas.drawLine(x-4, y-3, x+4, y-3, WASP_STRIPE);
  canvas.drawLine(x-5, y, x+5, y, WASP_STRIPE);
  canvas.drawLine(x-4, y+3, x+4, y+3, WASP_STRIPE);
  canvas.fillCircle(x + dir*2, y-8, 4, WASP_STRIPE);
  canvas.fillCircle(x + dir*1, y-8, 1, 0xFFFF);
  canvas.fillCircle(x + dir*4, y-8, 1, 0xFFFF);
  canvas.fillTriangle(x-2, y+8, x+2, y+8, x, y+13, 0xF800);
}

void drawDyingWasp(int x, int y) {
  canvas.fillEllipse(x, y, 5, 8, WASP_BODY);
  canvas.fillCircle(x, y-8, 4, 0x0000);
  canvas.drawLine(x-3, y-9, x-1, y-7, 0xF800);
  canvas.drawLine(x-1, y-9, x-3, y-7, 0xF800);
  canvas.drawLine(x+1, y-9, x+3, y-7, 0xF800);
  canvas.drawLine(x+3, y-9, x+1, y-7, 0xF800);
  canvas.setTextColor(0xF800); canvas.setTextSize(2);
  canvas.setCursor(x-18, y-25); canvas.println("ZAP!");
}

bool checkWaspTouch(uint16_t tx, uint16_t ty) {
  if (waspState == 0 || waspState == 5) return false;
  return (abs((int)tx - waspX) < 20 && abs((int)ty - waspY) < 20);
}

void killWasp() {
  canvas.fillCircle(waspX, waspY, 18, 0xFFFF);
  canvas.setTextColor(0xF800); canvas.setTextSize(2);
  canvas.setCursor(waspX-20, waspY-10); canvas.println("ZAP!");
  canvas.pushSprite(0, 0);
  delay(200);
  waspState = 5; waspStateTime = millis();
  flashEndTime = millis() + 100;
  addNotification("ZAP!", 0xFE00);
}

// ============================================================
// 乌鸦
// ============================================================
void updateCrow(unsigned long now) {
  switch (crowState) {
    case 0: if (now > crowNextSpawn) { crowState = 1; crowStateTime = now; crowX = random(60, 260); crowY = -40; crowTarget = random(1, 3); } break;
    case 1: crowY += 2; crowX += sin(now / 250.0) * 2; crowWingAngle = sin(now / 80.0) * 0.7; if (crowY > 50) { crowState = 2; crowStateTime = now; } break;
    case 2: {
      int tX = (crowTarget == 1) ? pigX : pig2X;
      int tY = pigY - 10;
      float dx = tX - crowX, dy = tY - crowY, d = sqrt(dx*dx + dy*dy);
      if (d < 6) {
        bool inv = (crowTarget == 1) ? pinkInvincible : whiteInvincible;
        if (inv) { addNotification("REFLECT!", 0xFFE0); flashEndTime = now + 100; }
        else if (crowTarget == 1) { pinkDizzy = true; pinkDizzyEnd = now + 2500; }
        else { whiteDizzy = true; whiteDizzyEnd = now + 2500; }
        crowState = 3; crowStateTime = now; shakeEndTime = now + 400;
      } else { crowX += (dx/d)*5; crowY += (dy/d)*5; crowWingAngle = sin(now/40.0)*0.9; }
      break;
    }
    case 3: if (now - crowStateTime > 600) { crowState = 4; crowStateTime = now; } break;
    case 4: crowY -= 4; crowX += sin(now / 150.0) * 2; crowWingAngle = sin(now / 60.0) * 0.8; if (crowY < -50) { crowState = 0; crowNextSpawn = now + 30000 + random(20000); } break;
    case 5: if (now - crowStateTime > 1500) { crowState = 0; crowNextSpawn = now + 30000 + random(20000); } break;
  }
}

void drawCrow(int x, int y, float wingAngle) {
  canvas.drawCircle(x, y, 15, 0xF81F);
  canvas.drawCircle(x, y, 16, 0x7BEF);
  int wo = sin(wingAngle) * 10;
  canvas.fillTriangle(x-5, y, x-25, y-8+wo, x-10, y+8, CROW_BODY);
  canvas.fillTriangle(x+5, y, x+25, y-8+wo, x+10, y+8, CROW_BODY);
  canvas.fillEllipse(x, y, 12, 9, CROW_BODY);
  canvas.fillCircle(x, y-8, 7, CROW_BODY);
  canvas.fillTriangle(x-4, y-8, x+4, y-8, x, y-1, CROW_BEAK);
  canvas.fillCircle(x-3, y-9, 2, 0xF800);
  canvas.fillCircle(x+3, y-9, 2, 0xF800);
  canvas.fillCircle(x-3, y-9, 1, 0x0000);
  canvas.fillCircle(x+3, y-9, 1, 0x0000);
  canvas.fillTriangle(x-8, y+8, x+8, y+8, x, y+20, 0x0000);
  canvas.fillCircle(x, y-16, 1, 0xFFFF);
}

void drawDyingCrow(int x, int y) {
  canvas.fillEllipse(x, y, 12, 9, CROW_BODY);
  canvas.fillCircle(x, y-8, 7, CROW_BODY);
  canvas.fillTriangle(x-4, y-8, x+4, y-8, x, y-1, CROW_BEAK);
  canvas.drawLine(x-5, y-11, x-1, y-7, 0xF800);
  canvas.drawLine(x-1, y-11, x-5, y-7, 0xF800);
  canvas.drawLine(x+1, y-11, x+5, y-7, 0xF800);
  canvas.drawLine(x+5, y-11, x+1, y-7, 0xF800);
  canvas.fillRect(x-1, y-2, 3, 5, 0xF81F);
  canvas.fillTriangle(x-8, y+8, x+8, y+8, x, y+20, 0x0000);
  canvas.setTextColor(0xF800); canvas.setTextSize(2);
  canvas.setCursor(x-20, y-35); canvas.println("KO!");
}

bool checkCrowTouch(uint16_t tx, uint16_t ty) {
  if (crowState == 0 || crowState == 5) return false;
  return (abs((int)tx - crowX) < 25 && abs((int)ty - crowY) < 25);
}

void killCrow() {
  canvas.fillCircle(crowX, crowY, 20, 0xFFFF);
  canvas.setTextColor(0xF800); canvas.setTextSize(2);
  canvas.setCursor(crowX-20, crowY-10); canvas.println("BAM!");
  canvas.pushSprite(0, 0);
  delay(200);
  crowState = 5; crowStateTime = millis();
  flashEndTime = millis() + 100;
  addNotification("CROW KO!", 0x1082);
}

// ============================================================
// 陷阱
// ============================================================
void updateTrap(unsigned long now) {
  if (!enemyTrap) { trapState = 0; trapActive = false; return; }
  switch (trapState) {
    case 0: if (now > trapNextSpawn) { trapState = 1; trapStateTime = now; trapX = random(50, 270); trapY = 236; } break;
    case 1: if (now - trapStateTime > 800) { trapState = 2; trapStateTime = now; trapActive = true; } break;
    case 2:
      if (!pinkFrozen && !pinkDizzy && abs(pigX - trapX) < 15) {
        if (pinkInvincible) { addNotification("IMMUNE!", 0xFFE0); }
        else { pinkDizzy = true; pinkDizzyEnd = now + 2000; shakeEndTime = now + 300; flashEndTime = now + 80; addNotification("TRAP!", 0xF800); }
        trapState = 3; trapStateTime = now; trapActive = false;
      } else if (!whiteFrozen && !whiteDizzy && abs(pig2X - trapX) < 15) {
        if (whiteInvincible) { addNotification("IMMUNE!", 0xFFE0); }
        else { whiteDizzy = true; whiteDizzyEnd = now + 2000; shakeEndTime = now + 300; flashEndTime = now + 80; addNotification("TRAP!", 0xF800); }
        trapState = 3; trapStateTime = now; trapActive = false;
      }
      if (now - trapStateTime > 10000) { trapState = 3; trapStateTime = now; trapActive = false; }
      break;
    case 3: if (now - trapStateTime > 400) { trapState = 0; trapNextSpawn = now + 15000 + random(15000); } break;
  }
}

void drawTrap() {
  if (trapState == 0) return;
  if (!enemyTrap) return;
  unsigned long now = millis();
  if (trapState == 1) {
    if ((now / 150) % 2 == 0) {
      canvas.fillTriangle(trapX-12, trapY, trapX+12, trapY, trapX, trapY-10, 0xF800);
      canvas.drawTriangle(trapX-12, trapY, trapX+12, trapY, trapX, trapY-10, 0xFBE0);
      canvas.setTextColor(0xFFFF); canvas.setTextSize(1);
      canvas.setCursor(trapX-1, trapY-8); canvas.println("!");
    }
  } else if (trapState == 2) {
    canvas.fillRect(trapX-14, trapY-2, 28, 4, 0x0000);
    canvas.fillRect(trapX-13, trapY-1, 26, 3, 0x4208);
    for (int i = 0; i < 3; i++) {
      int sx = trapX - 8 + i * 8;
      canvas.fillTriangle(sx-3, trapY-1, sx+3, trapY-1, sx, trapY-14, 0xC618);
      canvas.drawTriangle(sx-3, trapY-1, sx+3, trapY-1, sx, trapY-14, 0xFFFF);
      canvas.drawLine(sx, trapY-12, sx-1, trapY-3, 0xFFFF);
    }
    if ((now / 300) % 2 == 0) canvas.drawLine(trapX-13, trapY, trapX+13, trapY, 0xF800);
  } else if (trapState == 3) {
    unsigned long e = now - trapStateTime;
    int offset = e / 50; if (offset > 14) offset = 14;
    canvas.fillRect(trapX-13, trapY-1, 26, 3, 0x4208);
    for (int i = 0; i < 3; i++) {
      int sx = trapX - 8 + i * 8, spH = 14 - offset;
      if (spH > 0) canvas.fillTriangle(sx-3, trapY-1, sx+3, trapY-1, sx, trapY-1-spH, 0xC618);
    }
    if (e < 300 && (e / 80) % 2 == 0) {
      canvas.drawLine(trapX-8, trapY-15, trapX+8, trapY-3, 0xF800);
      canvas.drawLine(trapX+8, trapY-15, trapX-8, trapY-3, 0xF800);
    }
  }
}

bool checkTrapTouch(uint16_t tx, uint16_t ty) {
  if (trapState != 2) return false;
  if (abs((int)tx - trapX) < 20 && abs((int)ty - trapY) < 20) {
    trapState = 3; trapStateTime = millis(); trapActive = false;
    addNotification("DEFUSED!", 0x07E0);
    return true;
  }
  return false;
}

void triggerTrap() {}

// ============================================================
// 🍎 无敌果实
// ============================================================
void updateFruit(unsigned long now) {
  if (!enemyFruit) { fruitState = 0; fruitActive = false; return; }
  switch (fruitState) {
    case 0: if (now > fruitNextSpawn) { fruitState = 1; fruitStateTime = now; fruitX = random(40, 280); fruitY = 236; fruitActive = true; } break;
    case 1:
      if (!pinkFrozen && abs(pigX - fruitX) < 18) { giveInvincibleToPig(true); fruitState = 2; fruitStateTime = now; fruitActive = false; }
      else if (!whiteFrozen && abs(pig2X - fruitX) < 18) { giveInvincibleToPig(false); fruitState = 2; fruitStateTime = now; fruitActive = false; }
      if (now - fruitStateTime > 15000) { fruitState = 2; fruitStateTime = now; fruitActive = false; }
      break;
    case 2: if (now - fruitStateTime > 500) { fruitState = 0; fruitNextSpawn = now + 12000 + random(15000); } break;
  }
}

void giveInvincibleToPig(bool isPink) {
  unsigned long now = millis();
  if (isPink) { pinkInvincible = true; pinkInvincibleEnd = now + INVINCIBLE_DURATION; addNotification("PINK INVINCIBLE!", 0x07E0); }
  else { whiteInvincible = true; whiteInvincibleEnd = now + INVINCIBLE_DURATION; addNotification("WHITE INVINCIBLE!", 0x07FF); }
  flashEndTime = now + 100;
}

void drawFruit() {
  if (fruitState == 0) return;
  if (!enemyFruit) return;
  unsigned long now = millis();
  if (fruitState == 1) {
    int gr = 10 + (int)(sin(now / 150.0) * 3);
    canvas.fillCircle(fruitX, fruitY-4, gr, 0x07E0);
    canvas.fillCircle(fruitX, fruitY-4, gr-2, 0x0FE0);
    canvas.fillCircle(fruitX, fruitY-4, 7, 0xF800);
    canvas.fillCircle(fruitX, fruitY-4, 6, 0xFC00);
    canvas.fillCircle(fruitX-2, fruitY-6, 2, 0xFFFF);
    canvas.fillTriangle(fruitX, fruitY-11, fruitX-5, fruitY-14, fruitX+3, fruitY-13, 0x07E0);
    canvas.drawLine(fruitX, fruitY-11, fruitX-1, fruitY-15, 0x4208);
    for (int i = 0; i < 4; i++) {
      float a = (i * 90 + now / 20.0) * DEG_TO_RAD;
      int sx = fruitX + 14 * cos(a), sy = fruitY - 4 + 14 * sin(a);
      uint16_t sc = (i % 2 == 0) ? 0xFFE0 : 0x07FF;
      canvas.drawPixel(sx, sy, sc); canvas.drawPixel(sx-1, sy, sc);
      canvas.drawPixel(sx+1, sy, sc); canvas.drawPixel(sx, sy-1, sc);
      canvas.drawPixel(sx, sy+1, sc);
    }
    canvas.drawLine(fruitX-8, fruitY+3, fruitX+8, fruitY+3, 0x07E0);
  } else if (fruitState == 2) {
    unsigned long e = now - fruitStateTime;
    int offset = e / 20; if (offset > 30) offset = 30;
    int al = 255 - (offset * 8); if (al < 0) al = 0;
    uint16_t fc = blendRGB565(0x0000, 0xF800, al / 255.0);
    canvas.fillCircle(fruitX, fruitY-4-offset, 7, fc);
    if (e < 300) for (int i = 0; i < 8; i++) {
      float a = i * 45 * DEG_TO_RAD;
      int sx = fruitX + (offset + 5) * cos(a);
      int sy = fruitY - 4 + (offset + 5) * sin(a);
      if ((e / 60) % 2 == 0) canvas.fillCircle(sx, sy, 2, 0xFFFF);
    }
  }
}

// ============================================================
// 通知系统
// ============================================================
void addNotification(String text, uint16_t color) {
  for (int i = MAX_NOTIFS - 1; i > 0; i--) notifs[i] = notifs[i - 1];
  notifs[0].text = text;
  notifs[0].color = color;
  notifs[0].createdAt = millis();
  notifs[0].active = true;
}

void updateNotifications() {
  unsigned long now = millis();
  for (int i = 0; i < MAX_NOTIFS; i++) {
    if (notifs[i].active && now - notifs[i].createdAt > 3000) notifs[i].active = false;
  }
}

void drawNotifications() {
  if (!notifs[0].active && !notifs[1].active && !notifs[2].active) return;
  const int BOX_W = 110, BOX_H = 28;
  const int BOX_X = 320 - BOX_W - 5;
  const int BASE_Y = 75;
  unsigned long now = millis();
  for (int i = 0; i < MAX_NOTIFS; i++) {
    if (!notifs[i].active) continue;
    int y = BASE_Y + i * (BOX_H + 4);
    unsigned long age = now - notifs[i].createdAt;
    int slideOffset = 0;
    if (age < 200) slideOffset = (200 - age) * BOX_W / 200;
    if (age > 2700) slideOffset = (age - 2700) * BOX_W / 300;
    int drawX = BOX_X + slideOffset;
    canvas.fillRoundRect(drawX+2, y+2, BOX_W, BOX_H, 6, 0x0000);
    canvas.fillRoundRect(drawX, y, BOX_W, BOX_H, 6, 0x2124);
    canvas.drawRoundRect(drawX, y, BOX_W, BOX_H, 6, notifs[i].color);
    canvas.fillRect(drawX+3, y+4, 4, BOX_H-8, notifs[i].color);
    canvas.setTextColor(notifs[i].color, 0x2124);
    canvas.setTextSize(1);
    canvas.setCursor(drawX+12, y+10);
    canvas.println(notifs[i].text);
  }
}

// ============================================================
// NEXT 敌人预览（塞尔达风格）
// ============================================================
int getNextEnemyType() {
  static int cycle = 0;
  static unsigned long lastCycle = 0;
  int enabled[7]; int count = 0;
  if (enemyBird) enabled[count++] = 1;
  if (enemyWolf) enabled[count++] = 2;
  if (enemyBat)  enabled[count++] = 3;
  if (enemyWasp) enabled[count++] = 4;
  if (enemyCrow) enabled[count++] = 5;
  if (enemyTrap) enabled[count++] = 6;
  if (enemyFruit) enabled[count++] = 7;
  if (count == 0) return 0;
  if (millis() - lastCycle > 2500) { lastCycle = millis(); cycle = (cycle + 1) % count; }
  return enabled[cycle % count];
}

unsigned long getNextEnemyTime() {
  unsigned long now = millis();
  int type = getNextEnemyType();
  unsigned long t = 0;
  switch (type) {
    case 1: t = (birdStateTime + 15000) - now; break;
    case 2: t = wolfNextSpawn - now; break;
    case 3: t = batNextSpawn - now; break;
    case 4: t = waspNextSpawn - now; break;
    case 5: t = crowNextSpawn - now; break;
    case 6: t = trapNextSpawn - now; break;
    case 7: t = fruitNextSpawn - now; break;
  }
  if (t > 999999 || (long)t < 0) t = 0;
  return t;
}

void drawNextEnemyBox() {
  int nextType = getNextEnemyType();
  if (nextType == 0) return;
  unsigned long waitMs = getNextEnemyTime();
  #define ZBG 0x4208
  #define ZGD 0xFE60
  #define ZGD2 0x9BC0
  #define ZIV 0xFFDE
  const int W = 44, H = 44, X = 320-W-2, Y = 2;
  canvas.fillRoundRect(X+2, Y+2, W, H, 5, 0x0000);
  canvas.fillRoundRect(X, Y, W, H, 5, ZGD);
  canvas.fillRoundRect(X+2, Y+2, W-4, H-4, 4, ZBG);
  canvas.drawRoundRect(X+4, Y+4, W-8, H-8, 3, ZGD2);
  canvas.fillTriangle(X+2, Y+2, X+7, Y+2, X+2, Y+7, ZGD);
  canvas.fillTriangle(X+W-2, Y+2, X+W-7, Y+2, X+W-2, Y+7, ZGD);
  canvas.fillTriangle(X+2, Y+H-2, X+7, Y+H-2, X+2, Y+H-7, ZGD);
  canvas.fillTriangle(X+W-2, Y+H-2, X+W-7, Y+H-2, X+W-2, Y+H-7, ZGD);
  int gx = X+W/2, gy = Y+1;
  canvas.fillTriangle(gx, gy-2, gx-3, gy+1, gx+3, gy+1, ZGD);
  canvas.fillTriangle(gx, gy+4, gx-3, gy+1, gx+3, gy+1, ZGD);
  canvas.setTextColor(ZGD, ZBG); canvas.setTextSize(1);
  canvas.setCursor(X+8, Y+6); canvas.println("NEXT");
  int ix = X+W/2, iy = Y+22;
  switch (nextType) {
    case 1: canvas.fillEllipse(ix, iy, 5, 4, 0xFEA0); canvas.fillCircle(ix, iy-4, 3, 0xFEA0); canvas.fillTriangle(ix-2, iy-4, ix+2, iy-4, ix, iy-1, 0xF800); canvas.fillCircle(ix-1, iy-4, 1, 0); canvas.fillCircle(ix+1, iy-4, 1, 0); break;
    case 2: canvas.fillEllipse(ix-2, iy, 5, 4, 0x9B26); canvas.fillEllipse(ix+3, iy-1, 4, 3, 0x9B26); canvas.fillTriangle(ix+2, iy-3, ix+3, iy-7, ix+4, iy-3, 0x5961); canvas.fillTriangle(ix+5, iy-3, ix+6, iy-7, ix+7, iy-3, 0x5961); canvas.fillCircle(ix+3, iy-2, 1, 0xF800); canvas.fillCircle(ix+5, iy-2, 1, 0xF800); break;
    case 3: canvas.fillTriangle(ix-2, iy, ix-8, iy-3, ix-3, iy+2, 0x2124); canvas.fillTriangle(ix+2, iy, ix+8, iy-3, ix+3, iy+2, 0x2124); canvas.fillEllipse(ix, iy, 2, 4, 0x2124); canvas.fillCircle(ix, iy-3, 3, 0x2124); canvas.fillCircle(ix-1, iy-3, 1, 0xF800); canvas.fillCircle(ix+1, iy-3, 1, 0xF800); break;
    case 4: canvas.fillEllipse(ix-2, iy-4, 2, 2, 0xBDFF); canvas.fillEllipse(ix+2, iy-4, 2, 2, 0xBDFF); canvas.fillEllipse(ix, iy, 2, 4, 0xFE00); canvas.drawLine(ix-2, iy-1, ix+2, iy-1, 0); canvas.drawLine(ix-2, iy+1, ix+2, iy+1, 0); canvas.fillCircle(ix, iy-4, 2, 0); break;
    case 5: canvas.fillTriangle(ix-2, iy, ix-9, iy-3, ix-4, iy+3, 0x1082); canvas.fillTriangle(ix+2, iy, ix+9, iy-3, ix+4, iy+3, 0x1082); canvas.fillEllipse(ix, iy, 4, 3, 0x1082); canvas.fillCircle(ix, iy-3, 3, 0x1082); canvas.fillTriangle(ix-2, iy-3, ix+2, iy-3, ix, iy-1, 0xFC00); canvas.fillCircle(ix-1, iy-4, 1, 0xF800); canvas.fillCircle(ix+1, iy-4, 1, 0xF800); break;
    case 6: canvas.fillRect(ix-6, iy+2, 12, 2, 0xC618); for (int i = 0; i < 3; i++) { int sx = ix-4+i*4; canvas.fillTriangle(sx-2, iy+2, sx+2, iy+2, sx, iy-5, 0xC618); } break;
    case 7: canvas.fillCircle(ix, iy, 5, 0x07E0); canvas.fillCircle(ix, iy, 4, 0xF800); canvas.fillCircle(ix-1, iy-1, 1, 0xFFFF); canvas.fillTriangle(ix, iy-5, ix-3, iy-7, ix+2, iy-6, 0x07E0); break;
  }
  const char* nm[] = {"", "BIRD", "WOLF", "BAT", "WASP", "CROW", "TRAP", "FRUIT"};
  canvas.setTextColor(ZIV, ZBG); canvas.setTextSize(1);
  int nl = strlen(nm[nextType]);
  canvas.setCursor(X+(W-nl*6)/2, Y+31); canvas.println(nm[nextType]);
  float s = waitMs / 1000.0;
  char ts[16];
  if (s <= 0 || waitMs == 0) snprintf(ts, sizeof(ts), "READY");
  else snprintf(ts, sizeof(ts), "%.1fs", s);
  canvas.setTextColor(ZGD, ZBG);
  int tl = strlen(ts);
  canvas.setCursor(X+(W-tl*6)/2, Y+38); canvas.println(ts);
  if (s < 3.0 && s > 0) {
    int pulse = (int)(sin(millis() / 80.0) * 1);
    uint16_t pc = (millis() / 150) % 2 ? 0xF800 : ZGD;
    canvas.drawRoundRect(X-1+pulse, Y-1+pulse, W+2-pulse*2, H+2-pulse*2, 6, pc);
    canvas.drawRoundRect(X+pulse, Y+pulse, W-pulse*2, H-pulse*2, 5, pc);
  }
}

// ============================================================
// 触摸
// ============================================================
void handleTouch() {
  uint16_t x, y;
  bool touched = tft.getTouch(&x, &y);
  static unsigned long lastSnow = 0;
  static unsigned long tStart = 0;
  static bool switched = false;
  if (touched) {
    unsigned long now = millis();
    if (enemyBird && checkBirdTouch(x, y)) { killBird(); return; }
    if (enemyWolf && checkWolfTouch(x, y)) { killWolf(); return; }
    if (enemyBat && checkBatTouch(x, y)) { killBat(); return; }
    if (enemyWasp && checkWaspTouch(x, y)) { killWasp(); return; }
    if (enemyCrow && checkCrowTouch(x, y)) { killCrow(); return; }
    if (checkTrapTouch(x, y)) { return; }
    if (now - lastSnow > 50) { lastSnow = now; spawnSnow(x, y); }
    if (!lastTouchState) { tStart = now; switched = false; }
    else if (!switched && (now - tStart > 1000)) { currentView = (currentView == 0) ? 1 : 0; switched = true; }
  }
  lastTouchState = touched;
}

// ============================================================
// 天体（太阳/月亮 240, 42）
// ============================================================
void updateCelestialState() {
  struct tm ti;
  if (!getLocalTime(&ti, 100)) { currentSunAlpha = 0.0; isDaytime = false; return; }
  float hour = ti.tm_hour + ti.tm_min / 60.0;
  if (hour >= 7.0 && hour <= 17.0) { currentSunAlpha = 1.0; isDaytime = true; }
  else if (hour > 5.0 && hour < 7.0) { currentSunAlpha = (hour - 5.0) / 2.0; isDaytime = currentSunAlpha > 0.5; }
  else if (hour > 17.0 && hour < 19.0) { currentSunAlpha = (19.0 - hour) / 2.0; isDaytime = currentSunAlpha > 0.5; }
  else { currentSunAlpha = 0.0; isDaytime = false; }
  if (currentSunAlpha < 0) currentSunAlpha = 0;
  if (currentSunAlpha > 1) currentSunAlpha = 1;
}

void drawCelestialBody() {
  int cx = 240, cy = 42;
  float moonAlpha = 1.0 - currentSunAlpha;
  if (moonAlpha > 0.01) drawMoon(cx, cy, moonAlpha);
  if (currentSunAlpha > 0.01) drawSun(cx, cy, currentSunAlpha);
}

void drawSun(int cx, int cy, float alpha) {
  uint16_t core = blendRGB565(SKY_TOP, SUN_CORE, alpha);
  uint16_t glow = blendRGB565(SKY_TOP, SUN_GLOW, alpha * 0.5);
  uint16_t rc = blendRGB565(SKY_TOP, SUN_RAY, alpha);
  uint16_t rc2 = blendRGB565(SKY_TOP, SUN_GLOW, alpha * 0.7);
  canvas.fillCircle(cx, cy, 24, glow);
  canvas.fillCircle(cx, cy, 14, core);
  unsigned long now = millis();
  float rot = now / 40.0;
  for (int i = 0; i < 12; i++) {
    float ang = (i * 30 + rot) * DEG_TO_RAD;
    canvas.drawLine(cx + 17*cos(ang), cy + 17*sin(ang), cx + 26*cos(ang), cy + 26*sin(ang), (i % 2 == 0) ? rc : rc2);
  }
  if (alpha > 0.7) canvas.fillCircle(cx - 4, cy - 4, 3, blendRGB565(SKY_TOP, 0xFFFF, alpha));
}

void drawMoon(int cx, int cy, float alpha) {
  uint16_t bc = blendRGB565(SKY_TOP, MOON_BODY, alpha);
  uint16_t dc = blendRGB565(SKY_TOP, MOON_DARK, alpha);
  canvas.fillCircle(cx, cy, 18, bc);
  canvas.fillCircle(cx + 8, cy - 3, 16, dc);
  if (alpha > 0.5) {
    uint16_t cc = blendRGB565(SKY_TOP, 0xCE79, alpha * 0.8);
    canvas.fillCircle(cx - 5, cy + 3, 2, cc);
    canvas.fillCircle(cx - 9, cy - 2, 1, cc);
    canvas.fillCircle(cx - 3, cy - 7, 2, cc);
  }
  if (alpha > 0.7) {
    uint16_t hc = blendRGB565(SKY_TOP, 0x3186, alpha * 0.4);
    canvas.drawCircle(cx, cy, 20, hc);
    canvas.drawCircle(cx, cy, 22, hc);
  }
}

// ============================================================
// 颜色混合
// ============================================================
uint16_t blendRGB565(uint16_t c1, uint16_t c2, float t) {
  if (t <= 0) return c1;
  if (t >= 1) return c2;
  int r1 = (c1 >> 11) & 0x1F, g1 = (c1 >> 5) & 0x3F, b1 = c1 & 0x1F;
  int r2 = (c2 >> 11) & 0x1F, g2 = (c2 >> 5) & 0x3F, b2 = c2 & 0x1F;
  return ((int)(r1 + (r2-r1)*t) << 11) | ((int)(g1 + (g2-g1)*t) << 5) | (int)(b1 + (b2-b1)*t);
}

// ============================================================
// 背景
// ============================================================
void renderBackground() {
  uint16_t topColor = blendRGB565(SKY_TOP, 0xFD20, currentSunAlpha * 0.15);
  uint16_t botColor = blendRGB565(SKY_BOTTOM, 0x5D9B, currentSunAlpha * 0.2);
  for (int y = 0; y < 240; y++) {
    uint16_t c = blendRGB565(topColor, botColor, (float)y / 240.0);
    bgSprite.drawFastHLine(0, y, 320, c);
  }
  for (int i = 0; i < 80; i++) {
    int x = random(0, 320), y = random(0, 200);
    bgSprite.drawPixel(x, y, STAR_COLOR);
    if (random(0, 4) == 0) {
      bgSprite.drawPixel(x+1, y, STAR_COLOR);
      bgSprite.drawPixel(x, y+1, STAR_COLOR);
      bgSprite.drawPixel(x+1, y+1, STAR_COLOR);
    }
  }
  bgSprite.fillEllipse(160, 290, 220, 75, HILL_COLOR);
  bgSprite.fillEllipse(160, 295, 220, 70, HILL_HIGHLIGHT);
  for (int i = 0; i < 15; i++) bgSprite.drawPixel(random(20,300), random(215,235), 0xFD20);
  bgSprite.fillRoundRect(13, 13, 300, 47, 15, CARD_SHADOW);
  bgSprite.fillRoundRect(13, 68, 300, 100, 15, CARD_SHADOW);
  bgSprite.fillRoundRect(13, 178, 300, 38, 15, CARD_SHADOW);
  bgSprite.fillRoundRect(10, 10, 300, 47, 15, CARD_BG);
  bgSprite.drawRoundRect(10, 10, 300, 47, 15, CARD_BORDER);
  bgSprite.fillRoundRect(10, 65, 300, 100, 15, CARD_BG);
  bgSprite.drawRoundRect(10, 65, 300, 100, 15, CARD_BORDER);
  bgSprite.fillRoundRect(10, 175, 300, 38, 15, CARD_BG);
  bgSprite.drawRoundRect(10, 175, 300, 38, 15, CARD_BORDER);
}

// ============================================================
// 天气面板（移除右上角小图标）
// ============================================================
void drawWeatherIcon(int code, int x, int y) {
  if (code == 0) {
    canvas.fillCircle(x + 18, y + 18, 9, 0xFF2C);
    canvas.drawCircle(x + 18, y + 18, 10, 0xFC67);
    for (int i = 0; i < 360; i += 45) {
      float r = i * DEG_TO_RAD;
      canvas.drawLine(x + 18 + 12*cos(r), y + 18 + 12*sin(r), x + 18 + 16*cos(r), y + 18 + 16*sin(r), 0xFF2C);
    }
  } else {
    canvas.fillCircle(x + 12, y + 20, 8, 0x4208);
    canvas.fillCircle(x + 22, y + 18, 10, 0x8410);
    canvas.fillCircle(x + 32, y + 20, 8, 0x4208);
    canvas.fillRect(x + 12, y + 20, 20, 8, 0x8410);
    if (code >= 51) {
      canvas.drawLine(x+15, y+30, x+13, y+36, 0x5D9B);
      canvas.drawLine(x+23, y+30, x+21, y+36, 0x5D9B);
      canvas.drawLine(x+31, y+30, x+29, y+36, 0x5D9B);
    }
    if (code >= 95) {
      canvas.drawLine(x+22, y+28, x+18, y+36, 0xFF2C);
      canvas.drawLine(x+18, y+36, x+24, y+36, 0xFF2C);
    }
  }
}

void drawWeatherPanel() {
  if (!weather.valid) {
    canvas.setTextColor(TEXT_DIM); canvas.setTextSize(1);
    canvas.setCursor(20, 25); canvas.println("Loading weather...");
    return;
  }
  drawWeatherIcon(weather.weatherCode, 18, 15);
  canvas.setTextColor(TEXT_WARN); canvas.setTextSize(2);
  canvas.setCursor(65, 18); canvas.printf("%.1f", weather.temperature);
  canvas.setTextColor(TEXT_DIM); canvas.setTextSize(1);
  canvas.setCursor(65, 42);
  canvas.printf("Humidity: %.0f%%   %s", weather.humidity, getWeatherDescription(weather.weatherCode).c_str());
}

// ============================================================
// 数字时钟
// ============================================================
void drawDigitalClock() {
  struct tm ti;
  if (!getLocalTime(&ti, 100)) return;
  char timeStr[6]; strftime(timeStr, sizeof(timeStr), "%H:%M", &ti);
  char secStr[3];  strftime(secStr, sizeof(secStr), "%S", &ti);
  char dateStr[32]; strftime(dateStr, sizeof(dateStr), "%A, %d %b", &ti);
  canvas.setTextColor(TEXT_SHADOW); canvas.setTextSize(7);
  canvas.setCursor(24, 84); canvas.println(timeStr);
  canvas.setTextColor(TEXT_MAIN);
  canvas.setCursor(22, 82); canvas.println(timeStr);
  canvas.setTextColor(TEXT_ACCENT); canvas.setTextSize(3);
  canvas.setCursor(268, 138); canvas.println(secStr);
  canvas.setTextColor(TEXT_DIM); canvas.setTextSize(2);
  canvas.setCursor(28, 184); canvas.println(dateStr);
  canvas.fillCircle(24, 78, 3, 0x07FF);
}

// ============================================================
// 模拟时钟
// ============================================================
void drawAnalogClock() {
  struct tm ti;
  if (!getLocalTime(&ti, 100)) return;
  int cx = 160 + tiltX, cy = 115 + tiltY, R = 68;
  canvas.fillCircle(cx + 4, cy + 4, R, 0x0000);
  canvas.fillCircle(cx, cy, R, 0x2124);
  canvas.drawCircle(cx, cy, R, 0x39E7);
  canvas.drawCircle(cx, cy, R - 3, 0x5D9B);
  for (int i = 0; i < 12; i++) {
    float a = i * 30 * DEG_TO_RAD;
    int x1 = cx + (R-12)*sin(a), y1 = cy - (R-12)*cos(a);
    int x2 = cx + (R-6)*sin(a),  y2 = cy - (R-6)*cos(a);
    uint16_t c = (i % 3 == 0) ? 0xFFFF : 0x5D9B;
    canvas.drawLine(x1, y1, x2, y2, c);
  }
  float hA = (ti.tm_hour % 12 + ti.tm_min/60.0) * 30 * DEG_TO_RAD;
  float mA = (ti.tm_min + ti.tm_sec/60.0) * 6 * DEG_TO_RAD;
  float sA = ti.tm_sec * 6 * DEG_TO_RAD;
  int hL = R*0.5, mL = R*0.72, sL = R*0.82;
  int hx = cx + hL*sin(hA), hy = cy - hL*cos(hA);
  int mx = cx + mL*sin(mA), my = cy - mL*cos(mA);
  int sx = cx + sL*sin(sA), sy = cy - sL*cos(sA);
  int sox = tiltX*0.8, soy = tiltY*0.8;
  canvas.drawLine(cx+sox, cy+soy, hx+sox, hy+soy, 0x0000);
  canvas.drawLine(cx+sox, cy+soy, mx+sox, my+soy, 0x0000);
  canvas.drawLine(cx+sox, cy+soy, sx+sox, sy+soy, 0x0000);
  canvas.drawLine(cx, cy, hx, hy, 0xFFFF);
  canvas.drawLine(cx, cy, mx, my, 0x07FF);
  canvas.drawLine(cx, cy, sx, sy, 0xF800);
  canvas.fillCircle(cx, cy, 5, 0x39E7);
  canvas.fillCircle(cx - 1, cy - 1, 3, 0xFFFF);
  canvas.fillCircle(cx, cy, 2, 0xF800);
  char ts[6]; strftime(ts, sizeof(ts), "%H:%M", &ti);
  canvas.setTextColor(0x07FF); canvas.setTextSize(1);
  canvas.setCursor(cx - 15, cy + R + 6); canvas.println(ts);
}

// ============================================================
// 特效
// ============================================================
void drawDizzyEffect(int x, int y) {
  unsigned long now = millis();
  for (int i = 0; i < 3; i++) {
    float angle = (now / 200.0 + i * 120) * DEG_TO_RAD;
    int r = 18;
    int sx = x + r * cos(angle);
    int sy = y - 22 + r * sin(angle) * 0.4;
    uint16_t starColor = (i == 0) ? DIZZY_STAR1 : ((i == 1) ? DIZZY_STAR2 : DIZZY_STAR3);
    canvas.drawPixel(sx, sy, starColor);
    canvas.drawPixel(sx-1, sy, starColor);
    canvas.drawPixel(sx+1, sy, starColor);
    canvas.drawPixel(sx, sy-1, starColor);
    canvas.drawPixel(sx, sy+1, starColor);
  }
  canvas.setTextColor(0xFFFF); canvas.setTextSize(1);
  canvas.setCursor(x - 10, y + 18); canvas.println("Zzz...");
}

void drawFreezeEffect(int x, int y) {
  unsigned long now = millis();
  for (int i = 0; i < 8; i++) {
    float angle = (i * 45 + (now / 50)) * DEG_TO_RAD;
    int r = 22 + (sin(now / 200.0 + i) * 3);
    int ix = x + r*cos(angle);
    int iy = y + r*sin(angle)*0.7;
    canvas.drawPixel(ix, iy, ICE_WHITE);
    canvas.drawPixel(ix-1, iy, ICE_LIGHT);
    canvas.drawPixel(ix+1, iy, ICE_LIGHT);
    canvas.drawPixel(ix, iy-1, ICE_LIGHT);
    canvas.drawPixel(ix, iy+1, ICE_LIGHT);
  }
  canvas.drawCircle(x, y+10, 15, ICE_BLUE);
  canvas.drawCircle(x, y+10, 16, ICE_LIGHT);
}

void drawAnimeEffect(int x, int y) {
  canvas.fillCircle(x, y, 32, 0xFFFF);
  canvas.drawCircle(x, y, 34, 0xFC9F);
  for (int i = 0; i < 360; i += 30) {
    float r = i * DEG_TO_RAD;
    canvas.drawLine(x+15*cos(r), y+15*sin(r), x+38*cos(r), y+38*sin(r), 0xFF2C);
  }
  canvas.setTextColor(0xF800); canvas.setTextSize(2);
  canvas.setCursor(x-22, y-8); canvas.println("DOKI!");
  canvas.fillCircle(x-15, y-30, 4, 0xF81F);
  canvas.fillCircle(x-9, y-30, 4, 0xF81F);
  canvas.fillTriangle(x-19, y-30, x-5, y-30, x-12, y-20, 0xF81F);
  canvas.fillCircle(x+9, y-30, 4, 0xF81F);
  canvas.fillCircle(x+15, y-30, 4, 0xF81F);
  canvas.fillTriangle(x+5, y-30, x+19, y-30, x+12, y-20, 0xF81F);
}

void drawPigCelebration() {
  unsigned long now = millis();
  int midX = (pigX + pig2X) / 2;
  int midY = pigY - 40;
  const char* msg = "PIG POWER!!";
  int msgX = midX - 60, msgY = midY - 30;
  uint16_t rainbow[] = {0xF800, 0xFD20, 0xFFE0, 0x07E0, 0x07FF, 0x001F, 0xF81F};
  for (int i = 0; i < 11; i++) {
    canvas.setTextColor(rainbow[i % 7]); canvas.setTextSize(2);
    canvas.setCursor(msgX + i * 11, msgY + sin(now / 80.0 + i) * 3);
    canvas.printf("%c", msg[i]);
  }
  const char* comics[3] = {"BAM!", "POW!", "OINK!"};
  uint16_t comicColors[3] = {0xF800, 0xFFE0, 0xF81F};
  for (int i = 0; i < 3; i++) {
    int cx = midX - 50 + i * 50;
    int cy = midY + 15 + sin(now / 100.0 + i * 2) * 5;
    canvas.setTextColor(comicColors[i]); canvas.setTextSize(2);
    canvas.setCursor(cx, cy); canvas.println(comics[i]);
    canvas.drawRect(cx - 3, cy - 3, 45, 20, 0xFFFF);
  }
  for (int i = 0; i < 5; i++) {
    float t = fmod(now / 500.0 + i * 0.3, 1.0);
    int hx = midX - 60 + i * 30;
    int hy = midY + 40 - (int)(t * 50);
    uint16_t colors[5] = {0xF81F, 0xFC9F, 0xFFE0, 0x07FF, 0xF800};
    canvas.fillCircle(hx - 3, hy, 3, colors[i]);
    canvas.fillCircle(hx + 3, hy, 3, colors[i]);
    canvas.fillTriangle(hx - 6, hy, hx + 6, hy, hx, hy + 7, colors[i]);
  }
  for (int i = 0; i < 8; i++) {
    float angle = (i * 45 + celebrationRotation * 2) * DEG_TO_RAD;
    int r = 40 + sin(now / 100.0 + i) * 8;
    int sx = midX + r * cos(angle);
    int sy = midY + r * sin(angle);
    uint16_t starColor = (i % 2 == 0) ? 0xFFE0 : 0x07FF;
    canvas.drawPixel(sx, sy, starColor);
    canvas.drawPixel(sx-1, sy, starColor);
    canvas.drawPixel(sx+1, sy, starColor);
    canvas.drawPixel(sx, sy-1, starColor);
    canvas.drawPixel(sx, sy+1, starColor);
  }
}

// ============================================================
// 雪花
// ============================================================
void spawnSnow(int tx, int ty) {
  int n = 0;
  for (int i = 0; i < MAX_SNOW; i++) {
    if (!snow[i].active) {
      snow[i].x = tx + random(-20, 20);
      snow[i].y = ty + random(-20, 20);
      snow[i].size = random(3, 7);
      snow[i].life = random(20, 40);
      snow[i].active = true;
      if (++n >= 5) break;
    }
  }
}

void updateSnow() {
  unsigned long now = millis();
  for (int i = 0; i < MAX_SNOW; i++) {
    if (snow[i].active) {
      snow[i].life--;
      snow[i].y += 1.5;
      snow[i].x += random(-10, 11) / 10.0;
      if (snow[i].life % 8 == 0 && snow[i].size > 1) snow[i].size--;
      if (!pinkFrozen && abs((int)snow[i].x - pigX) < 20 && abs((int)snow[i].y - pigY) < 15) {
        pinkFrozen = true; pinkFreezeEnd = now + FREEZE_DURATION;
        pinkFrozenSpeed = pigSpeed; pigSpeed = 0;
        snow[i].active = false;
        addNotification("FROZEN!", 0x3D7F);
        continue;
      }
      if (!whiteFrozen && abs((int)snow[i].x - pig2X) < 20 && abs((int)snow[i].y - pigY) < 15) {
        whiteFrozen = true; whiteFreezeEnd = now + FREEZE_DURATION;
        whiteFrozenSpeed = pig2Speed; pig2Speed = 0;
        snow[i].active = false;
        addNotification("FROZEN!", 0x3D7F);
        continue;
      }
      if (snow[i].life <= 0 || snow[i].y > 240 || snow[i].size <= 1) snow[i].active = false;
      else drawSnowflake(snow[i].x, snow[i].y, snow[i].size);
    }
  }
}

void drawSnowflake(int x, int y, int size) {
  canvas.drawPixel(x, y, 0xFFFF);
  canvas.drawLine(x, y-size, x, y+size, 0xFFFF);
  canvas.drawLine(x-size*0.8, y-size*0.5, x+size*0.8, y+size*0.5, 0xFFFF);
  canvas.drawLine(x-size*0.8, y+size*0.5, x+size*0.8, y-size*0.5, 0xFFFF);
  if (size >= 5) {
    canvas.drawPixel(x-1, y-size+1, 0xFFFF);
    canvas.drawPixel(x+1, y-size+1, 0xFFFF);
    canvas.drawPixel(x-1, y+size-1, 0xFFFF);
    canvas.drawPixel(x+1, y+size-1, 0xFFFF);
  }
}
// ============================================================
// 小猪启动动画
// ============================================================
void showBootAnimation() {
  canvas.fillSprite(TFT_BLACK);
  int pinkY = 100 + sin(millis() / 300.0) * 3;
  drawPig(120, pinkY, PIG_PINK, PIG_PINK_DARK, true, false, false, false);
  int whiteY = 100 + sin(millis() / 300.0 + 1.5) * 3;
  drawPig(200, whiteY, PIG_WHITE, PIG_WHITE_DARK, false, false, false, false);
  canvas.fillCircle(158, 92, 4, 0xF81F);
  canvas.fillCircle(164, 92, 4, 0xF81F);
  canvas.fillTriangle(153, 92, 169, 92, 161, 103, 0xF81F);
  canvas.pushSprite(0, 0);

  unsigned long startTime = millis();
  while (millis() - startTime < 2200) {
    tft.fillRect(80, 160, 160, 70, TFT_BLACK);
    int dotX = 120, dotY = 175;
    int activeDot = (millis() / 250) % 4;
    for (int i = 0; i < 4; i++) {
      uint16_t color = (i == activeDot) ? PIG_PINK : ((i == (activeDot + 3) % 4) ? PIG_PINK_DARK : 0x18E3);
      tft.fillCircle(dotX + i * 20, dotY, 5, color);
    }
    tft.setTextColor(TEXT_DIM, TFT_BLACK); tft.setTextSize(1);
    tft.setCursor(128, 195); tft.println("PIG POWER");
    delay(30);
  }

  tft.fillRect(60, 160, 200, 70, TFT_BLACK);
  tft.drawRoundRect(60, 180, 200, 12, 6, PIG_PINK);
  for (int i = 0; i <= 196; i += 4) {
    tft.fillRoundRect(62, 182, i, 8, 4, PIG_PINK);
    delay(4);
  }
  delay(200);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(PIG_PINK, TFT_BLACK); tft.setTextSize(3);
  tft.setCursor(50, 95); tft.println("Smart Clock");
  tft.setTextColor(TEXT_DIM, TFT_BLACK); tft.setTextSize(1);
  tft.setCursor(90, 140); tft.println("ESP32-S3 PigDateClock");
  int hx = 150, hy = 175;
  tft.fillCircle(hx - 4, hy, 4, 0xF81F);
  tft.fillCircle(hx + 4, hy, 4, 0xF81F);
  tft.fillTriangle(hx - 8, hy, hx + 8, hy, hx, hy + 10, 0xF81F);
  delay(900);
  tft.fillScreen(TFT_BLACK);
}

// ============================================================
// GUI 主菜单（6 项）
// ============================================================
void showMainMenu() {
  const int MENU_ITEMS = 7;
  const char* labels[MENU_ITEMS] = {"Clock", "Scan WiFi", "WiFi Setup", "Enemy", "Fear", "Info", "Restart"};
  const uint16_t colors[MENU_ITEMS] = {0x07E0, 0x07FF, 0xFC9F, 0xF81F, 0x7DFF, 0xFFE0, 0xF800};
  int selected = 0;
  bool launched = false;

  auto drawMenuBase = [&]() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setTextSize(2);
    tft.setCursor(70, 4); tft.println("Smart Clock");
    tft.setTextColor(TEXT_DIM, TFT_BLACK); tft.setTextSize(1);
    tft.setCursor(60, 26); tft.println("Touch to select, tap again");
    tft.setCursor(10, 228); tft.println("v3.2  ESP32-S3 PigDateClock");
  };

  auto drawMenuItems = [&](int sel) {
    const int ITEM_Y[7] = {40, 66, 92, 118, 144, 170, 196};
    const int ITEM_H = 24;
    for (int i = 0; i < MENU_ITEMS; i++) {
      int y = ITEM_Y[i];
      String label = (i == 4) ? (pigFearMode ? "Fear: ON" : "Fear: OFF") : String(labels[i]);
      if (i == sel) {
        tft.fillRoundRect(15, y, 290, ITEM_H, 6, colors[i]);
        tft.setTextColor(TFT_BLACK, colors[i]);
      } else {
        tft.fillRoundRect(15, y, 290, ITEM_H, 6, 0x2124);
        tft.setTextColor(colors[i], 0x2124);
      }
      tft.drawRoundRect(15, y, 290, ITEM_H, 6, colors[i]);
      int iconX = 30, iconY = y + ITEM_H / 2;
      if (i == 0) { tft.drawCircle(iconX, iconY, 8, colors[i]); tft.drawLine(iconX, iconY, iconX, iconY-4, colors[i]); tft.drawLine(iconX, iconY, iconX+3, iconY, colors[i]); }
      else if (i == 1) { tft.drawCircle(iconX, iconY+3, 2, colors[i]); tft.drawCircle(iconX, iconY+3, 4, colors[i]); tft.drawCircle(iconX, iconY+3, 6, colors[i]); }
      else if (i == 2) { tft.drawCircle(iconX, iconY, 4, colors[i]); tft.drawCircle(iconX, iconY, 7, colors[i]); }
      else if (i == 3) { tft.fillCircle(iconX, iconY-2, 4, colors[i]); tft.fillRect(iconX-3, iconY+2, 6, 4, colors[i]); }
      else if (i == 4) { tft.drawCircle(iconX, iconY, 6, colors[i]); tft.drawCircle(iconX, iconY, 3, colors[i]); tft.fillCircle(iconX, iconY, 1, colors[i]); }
      else if (i == 5) { tft.fillCircle(iconX, iconY-5, 2, colors[i]); tft.fillRect(iconX-1, iconY-2, 3, 8, colors[i]); }
      else if (i == 6) { tft.drawCircle(iconX, iconY, 7, colors[i]); tft.fillTriangle(iconX+4, iconY-7, iconX+10, iconY-4, iconX+4, iconY-1, colors[i]); }
      tft.setTextSize(1);
      tft.setCursor(55, y + 8);
      tft.println(label);
    }
  };

  drawMenuBase();
  drawMenuItems(selected);

  unsigned long lastTouch = 0;
  bool lastTouched = false;
  uint16_t tx, ty;

  while (!launched) {
    bool touched = tft.getTouch(&tx, &ty);
    if (touched && !lastTouched && millis() - lastTouch > 300) {
      lastTouch = millis();
      const int ITEM_Y[7] = {40, 66, 92, 118, 144, 170, 196};
      for (int i = 0; i < MENU_ITEMS; i++) {
        if (ty >= ITEM_Y[i] && ty <= ITEM_Y[i] + 24 && tx >= 15 && tx <= 305) {
          if (i == selected) {
            tft.fillRoundRect(15, ITEM_Y[i], 290, 24, 6, TFT_WHITE);
            tft.setTextColor(TFT_BLACK, TFT_WHITE);
            tft.setTextSize(1);
            tft.setCursor(55, ITEM_Y[i] + 8);
            tft.println((i == 4) ? (pigFearMode ? "Fear: ON" : "Fear: OFF") : String(labels[i]));
            delay(200);
            switch (i) {
              case 0: launched = true; break;
              case 1: showWifiScanner(); drawMenuBase(); drawMenuItems(selected); break;
              case 2: launched = true; break;
              case 3: showEnemyMenu(); drawMenuBase(); drawMenuItems(selected); break;
              case 4:   // 🆕 Fear toggle
                pigFearMode = !pigFearMode;
                prefs.begin("clock", false);
                prefs.putBool("pigFear", pigFearMode);
                prefs.end();
                drawMenuBase();
                drawMenuItems(selected);
                break;
              case 5: showInfoScreen(); drawMenuBase(); drawMenuItems(selected); break;
              case 6:
                tft.fillScreen(TFT_BLACK);
                tft.setTextColor(TFT_RED, TFT_BLACK); tft.setTextSize(2);
                tft.setCursor(80, 100); tft.println("Restarting...");
                delay(1000); ESP.restart();
                break;
            }
          } else {
            selected = i;
            drawMenuItems(selected);
          }
          break;
        }
      }
    }
    lastTouched = touched;
    delay(30);
  }
  tft.fillScreen(TFT_BLACK);
}

// ============================================================
// 敌人子菜单（7 项，含开关）
// ============================================================
void showEnemyMenu() {
  const int ITEMS = 7;
  const char* names[ITEMS] = {"Bird", "Wolf", "Bat", "Wasp", "Crow", "Trap", "Fruit"};
  const uint16_t colors[ITEMS] = {0xFEA0, 0x9B26, 0x5D9B, 0xFE00, 0xF81F, 0xC618, 0xF800};
  int selected = 0;
  bool back = false;

  auto drawBase = [&]() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setTextSize(2);
    tft.setCursor(30, 4); tft.println("Enemy Settings");
    tft.setTextColor(TEXT_DIM, TFT_BLACK); tft.setTextSize(1);
    tft.setCursor(50, 24); tft.println("Tap item twice to toggle");
  };

  auto drawItems = [&](int sel) {
    bool states[ITEMS] = {enemyBird, enemyWolf, enemyBat, enemyWasp, enemyCrow, enemyTrap, enemyFruit};
    const int ITEM_Y[7] = {40, 66, 92, 118, 144, 170, 196};
    const int ITEM_H = 24;

    for (int i = 0; i < ITEMS; i++) {
      int y = ITEM_Y[i];
      bool on = states[i];
      if (i == sel) {
        tft.fillRoundRect(15, y, 290, ITEM_H, 6, colors[i]);
        tft.setTextColor(TFT_BLACK, colors[i]);
      } else {
        tft.fillRoundRect(15, y, 290, ITEM_H, 6, on ? 0x2124 : 0x1082);
        tft.setTextColor(on ? colors[i] : 0x5D9B, on ? 0x2124 : 0x1082);
      }
      tft.drawRoundRect(15, y, 290, ITEM_H, 6, on ? colors[i] : 0x5D9B);

      int iconX = 28, iconY = y + ITEM_H / 2;
      if (i == 0) { tft.fillEllipse(iconX, iconY, 4, 3, colors[i]); tft.fillCircle(iconX, iconY - 3, 3, colors[i]); }
      else if (i == 1) { tft.fillEllipse(iconX - 1, iconY, 5, 3, colors[i]); tft.fillCircle(iconX + 3, iconY - 2, 3, colors[i]); }
      else if (i == 2) { tft.fillTriangle(iconX - 2, iconY, iconX - 6, iconY - 3, iconX - 3, iconY + 2, colors[i]); tft.fillTriangle(iconX + 2, iconY, iconX + 6, iconY - 3, iconX + 3, iconY + 2, colors[i]); tft.fillCircle(iconX, iconY - 2, 2, colors[i]); }
      else if (i == 3) { tft.fillEllipse(iconX, iconY, 2, 3, colors[i]); tft.drawLine(iconX - 2, iconY, iconX + 2, iconY, 0x0000); }
      else if (i == 4) { tft.fillEllipse(iconX, iconY, 4, 2, colors[i]); tft.fillCircle(iconX, iconY - 2, 2, colors[i]); }
      else if (i == 5) { tft.fillRect(iconX - 5, iconY + 2, 10, 2, colors[i]); tft.fillTriangle(iconX - 4, iconY + 2, iconX - 2, iconY + 2, iconX - 3, iconY - 3, colors[i]); tft.fillTriangle(iconX + 2, iconY + 2, iconX + 4, iconY + 2, iconX + 3, iconY - 3, colors[i]); }
      else if (i == 6) { tft.fillCircle(iconX, iconY, 4, colors[i]); tft.fillTriangle(iconX, iconY - 5, iconX - 3, iconY - 7, iconX + 2, iconY - 6, 0x07E0); }

      tft.setTextSize(1);
      tft.setCursor(45, y + 8);
      tft.println(names[i]);

      if (on) {
        tft.setTextColor(0x07E0, i == sel ? colors[i] : 0x2124);
        tft.setCursor(240, y + 8); tft.println("[ON]");
      } else {
        tft.setTextColor(0xF800, i == sel ? colors[i] : 0x1082);
        tft.setCursor(235, y + 8); tft.println("[OFF]");
      }
    }

    tft.fillRoundRect(15, 222, 290, 16, 6, 0x5D9B);
    tft.setTextColor(TFT_WHITE, 0x5D9B); tft.setTextSize(1);
    tft.setCursor(130, 226); tft.println("Back to Menu");
  };

  drawBase();
  drawItems(selected);

  unsigned long lastTouch = 0;
  bool lastTouched = false;
  uint16_t tx, ty;

  while (!back) {
    bool touched = tft.getTouch(&tx, &ty);
    if (touched && !lastTouched && millis() - lastTouch > 300) {
      lastTouch = millis();
      if (ty >= 222 && ty <= 238 && tx >= 15 && tx <= 305) { back = true; break; }
      const int ITEM_Y[7] = {40, 66, 92, 118, 144, 170, 196};
      for (int i = 0; i < ITEMS; i++) {
        if (ty >= ITEM_Y[i] && ty <= ITEM_Y[i] + 24 && tx >= 15 && tx <= 305) {
          if (i == selected) {
            bool* ptr = nullptr;
            const char* key = "";
            switch (i) {
              case 0: ptr = &enemyBird; key = "eBird"; break;
              case 1: ptr = &enemyWolf; key = "eWolf"; break;
              case 2: ptr = &enemyBat;  key = "eBat";  break;
              case 3: ptr = &enemyWasp; key = "eWasp"; break;
              case 4: ptr = &enemyCrow; key = "eCrow"; break;
              case 5: ptr = &enemyTrap; key = "eTrap"; break;
              case 6: ptr = &enemyFruit; key = "eFruit"; break;
            }
            *ptr = !(*ptr);
            prefs.begin("clock", false);
            prefs.putBool(key, *ptr);
            prefs.end();
            if (!(*ptr)) {
              switch (i) {
                case 0: birdState = 0; break;
                case 1: wolfState = 0; break;
                case 2: batState = 0;  break;
                case 3: waspState = 0; break;
                case 4: crowState = 0; break;
                case 5: trapState = 0; trapActive = false; break;
                case 6: fruitState = 0; fruitActive = false; break;
              }
            }
            drawItems(selected);
          } else {
            selected = i;
            drawItems(selected);
          }
          break;
        }
      }
    }
    lastTouched = touched;
    delay(30);
  }
}

// ============================================================
// 设备信息
// ============================================================
void showInfoScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK); tft.setTextSize(2);
  tft.setCursor(80, 15); tft.println("Device Info");
  tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setTextSize(1);
  int y = 55;
  tft.setCursor(20, y); tft.printf("Chip: ESP32-S3"); y += 20;
  tft.setCursor(20, y); tft.printf("Flash: %d MB", ESP.getFlashChipSize() / (1024 * 1024)); y += 20;
  tft.setCursor(20, y); tft.printf("PSRAM: %d KB free", ESP.getFreePsram() / 1024); y += 20;
  tft.setCursor(20, y); tft.printf("RAM:   %d KB free", ESP.getFreeHeap() / 1024); y += 20;
  if (WiFi.status() == WL_CONNECTED) {
    tft.setTextColor(0x07E0, TFT_BLACK);
    tft.setCursor(20, y); tft.printf("WiFi:  Connected"); y += 20;
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(20, y); tft.printf("SSID:  %s", WiFi.SSID().c_str()); y += 20;
    tft.setCursor(20, y); tft.printf("IP:    %s", WiFi.localIP().toString().c_str());
  } else {
    tft.setTextColor(0xF800, TFT_BLACK);
    tft.setCursor(20, y); tft.printf("WiFi:  Not connected");
  }
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(60, 220); tft.println("Touch to go back");
  uint16_t tx, ty;
  while (true) { if (tft.getTouch(&tx, &ty)) { delay(300); break; } delay(50); }
}

// ============================================================
// 保存 WiFi
// ============================================================
void saveWiFi(String ssid, String pass) {
  prefs.begin("wifi", false);
  int count = prefs.getInt("count", 0);
  for (int i = 0; i < count; i++) {
    String key = "ssid" + String(i);
    if (prefs.getString(key.c_str(), "") == ssid) {
      String key2 = "pass" + String(i);
      prefs.putString(key2.c_str(), pass);
      prefs.end(); return;
    }
  }
  if (count < 10) {
    String k1 = "ssid" + String(count);
    String k2 = "pass" + String(count);
    prefs.putString(k1.c_str(), ssid);
    prefs.putString(k2.c_str(), pass);
    prefs.putInt("count", count + 1);
  }
  prefs.end();
}

// ============================================================
// WiFi 扫描器
// ============================================================
void showWifiScanner() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setTextSize(2);
  tft.setCursor(60, 100); tft.println("Scanning...");
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  if (n == 0) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setCursor(50, 100); tft.println("No WiFi found");
    delay(2000); return;
  }
  int scrollOffset = 0, selected = -1;
  const int MAX_SHOW = 4, ROW_H = 38;
  auto drawList = [&]() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setTextSize(2);
    tft.setCursor(10, 5); tft.printf("WiFi (%d)", n);
    tft.fillRoundRect(255, 3, 60, 28, 8, 0xF800);
    tft.setTextColor(TFT_WHITE, 0xF800); tft.setTextSize(1);
    tft.setCursor(272, 12); tft.println("Back");
    for (int i = 0; i < MAX_SHOW && (i + scrollOffset) < n; i++) {
      int idx = i + scrollOffset;
      int y = 40 + i * ROW_H;
      uint16_t bg = (idx == selected) ? 0x2124 : TFT_BLACK;
      tft.fillRoundRect(5, y, 310, ROW_H - 3, 5, bg);
      tft.drawRoundRect(5, y, 310, ROW_H - 3, 5, 0x5D9B);
      tft.setTextColor(TFT_WHITE, bg); tft.setTextSize(1);
      tft.setCursor(15, y + 6);
      String ssid = WiFi.SSID(idx);
      if (ssid.length() > 20) ssid = ssid.substring(0, 20) + "...";
      tft.println(ssid);
      tft.setCursor(15, y + 20);
      tft.setTextColor(TEXT_DIM, bg);
      tft.printf("%d dBm", WiFi.RSSI(idx));
      if (WiFi.encryptionType(idx) != WIFI_AUTH_OPEN) {
        tft.setTextColor(TFT_YELLOW, bg); tft.setCursor(260, y + 12); tft.println("[Lock]");
      } else {
        tft.setTextColor(0x07E0, bg); tft.setCursor(260, y + 12); tft.println("[Open]");
      }
    }
  };
  drawList();
  uint16_t tx, ty;
  bool lastTouched = false;
  unsigned long lastTouch = 0;
  bool done = false;
  while (!done) {
    bool touched = tft.getTouch(&tx, &ty);
    if (touched && !lastTouched && millis() - lastTouch > 300) {
      lastTouch = millis();
      if (tx >= 255 && ty <= 35) { done = true; break; }
      if (ty < 40 && scrollOffset > 0) { scrollOffset--; drawList(); continue; }
      if (ty > 210 && scrollOffset + MAX_SHOW < n) { scrollOffset++; drawList(); continue; }
      if (ty >= 40 && ty < 40 + MAX_SHOW * ROW_H) {
        int row = (ty - 40) / ROW_H;
        int idx = row + scrollOffset;
        if (idx < n) {
          selected = idx; drawList();
          String ssid = WiFi.SSID(idx);
          bool encrypted = WiFi.encryptionType(idx) != WIFI_AUTH_OPEN;
          String password = "";
          if (encrypted) password = showPasswordKeyboard(ssid);
          if (password == "__CANCEL__") { selected = -1; drawList(); continue; }
          tft.fillScreen(TFT_BLACK);
          tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setTextSize(2);
          tft.setCursor(50, 90); tft.println("Connecting");
          tft.setTextSize(1); tft.setCursor(50, 120);
          tft.setTextColor(TEXT_DIM, TFT_BLACK);
          tft.println(ssid);
          WiFi.begin(ssid.c_str(), password.c_str());
          int tries = 0;
          while (WiFi.status() != WL_CONNECTED && tries < 20) { delay(500); tries++; }
          if (WiFi.status() == WL_CONNECTED) {
            saveWiFi(ssid, password);
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(0x07E0, TFT_BLACK); tft.setTextSize(2);
            tft.setCursor(60, 100); tft.println("Connected!");
            delay(1500); done = true;
          } else {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.setCursor(50, 100); tft.println("Failed!");
            delay(1500);
            selected = -1; drawList();
          }
        }
      }
    }
    lastTouched = touched;
    delay(30);
  }
}

// ============================================================
// 密码键盘（大按键版）
// ============================================================
String showPasswordKeyboard(String ssid) {
  String password = "";
  int page = 0;
  const int NUM_PAGES = 4;
  const char* pages[4][20] = {
    {"q","w","e","r","t","y","u","i","o","p","1","2","3","4","5","6","7","8","9","0"},
    {"a","s","d","f","g","h","j","k","l",".","z","x","c","v","b","n","m","_","-","@"},
    {"Q","W","E","R","T","Y","U","I","O","P","1","2","3","4","5","6","7","8","9","0"},
    {"A","S","D","F","G","H","J","K","L",".","Z","X","C","V","B","N","M","_","-","@"}
  };
  const int KEY_W = 60, KEY_H = 30, KEY_GAP = 3;
  const int START_X = 8, START_Y = 70;
  const int BTN_Y = 205, BTN_H = 30, BTN_W = 60, BTN_GAP = 3;

  auto drawKeyboard = [&]() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TEXT_DIM, TFT_BLACK); tft.setTextSize(1);
    tft.setCursor(5, 5);
    String s = ssid; if (s.length() > 32) s = s.substring(0, 32);
    tft.printf("WiFi: %s", s.c_str());
    tft.fillRoundRect(5, 22, 310, 40, 8, 0x2124);
    tft.drawRoundRect(5, 22, 310, 40, 8, 0x5D9B);
    tft.setTextColor(TFT_WHITE, 0x2124); tft.setTextSize(2);
    tft.setCursor(12, 32);
    String display = password;
    if (display.length() > 22) display = display.substring(display.length() - 22);
    tft.println(display);
    int cursorX = 12 + display.length() * 12;
    if (cursorX > 300) cursorX = 300;
    if ((millis() / 500) % 2 == 0) tft.fillRect(cursorX, 32, 2, 18, TFT_WHITE);
    const char** keys = pages[page];
    for (int i = 0; i < 20; i++) {
      int row = i / 5, col = i % 5;
      int x = START_X + col * (KEY_W + KEY_GAP);
      int y = START_Y + row * (KEY_H + KEY_GAP);
      tft.fillRoundRect(x, y, KEY_W, KEY_H, 6, 0x2124);
      tft.drawRoundRect(x, y, KEY_W, KEY_H, 6, 0x5D9B);
      tft.setTextColor(TFT_WHITE, 0x2124); tft.setTextSize(2);
      tft.setCursor(x + KEY_W / 2 - 6, y + KEY_H / 2 - 8);
      tft.printf("%s", keys[i]);
    }
    int x = 4;
    tft.fillRoundRect(x, BTN_Y, BTN_W, BTN_H, 6, 0xFC9F);
    tft.setTextColor(TFT_BLACK, 0xFC9F); tft.setTextSize(1);
    tft.setCursor(x + 10, BTN_Y + 11); tft.printf("%d/%d", page + 1, NUM_PAGES);
    x += BTN_W + BTN_GAP;
    tft.fillRoundRect(x, BTN_Y, BTN_W, BTN_H, 6, 0x5D9B);
    tft.setTextColor(TFT_WHITE, 0x5D9B);
    tft.setCursor(x + 14, BTN_Y + 11); tft.println("Space");
    x += BTN_W + BTN_GAP;
    tft.fillRoundRect(x, BTN_Y, BTN_W, BTN_H, 6, 0xFBE0);
    tft.setTextColor(TFT_BLACK, 0xFBE0);
    tft.setCursor(x + 20, BTN_Y + 11); tft.println("Del");
    x += BTN_W + BTN_GAP;
    tft.fillRoundRect(x, BTN_Y, BTN_W, BTN_H, 6, 0xF800);
    tft.setTextColor(TFT_WHITE, 0xF800);
    tft.setCursor(x + 24, BTN_Y + 11); tft.println("X");
    x += BTN_W + BTN_GAP;
    tft.fillRoundRect(x, BTN_Y, BTN_W + 8, BTN_H, 6, 0x07E0);
    tft.setTextColor(TFT_BLACK, 0x07E0);
    tft.setCursor(x + 24, BTN_Y + 11); tft.println("OK");
  };
  drawKeyboard();
  uint16_t tx, ty;
  bool lastTouched = false;
  unsigned long lastTouch = 0, lastBlink = 0;
  while (true) {
    if (millis() - lastBlink > 500) { lastBlink = millis(); drawKeyboard(); }
    bool touched = tft.getTouch(&tx, &ty);
    if (touched && !lastTouched && millis() - lastTouch > 250) {
      lastTouch = millis();
      if (ty >= START_Y && ty < START_Y + 4 * (KEY_H + KEY_GAP)) {
        int row = (ty - START_Y) / (KEY_H + KEY_GAP);
        int col = (tx - START_X) / (KEY_W + KEY_GAP);
        if (row >= 0 && row < 4 && col >= 0 && col < 5) {
          password += pages[page][row * 5 + col];
          drawKeyboard();
        }
        continue;
      }
      if (ty >= BTN_Y && ty <= BTN_Y + BTN_H) {
        int bx = 4;
        if (tx >= bx && tx <= bx + BTN_W) { page = (page + 1) % NUM_PAGES; drawKeyboard(); continue; }
        bx += BTN_W + BTN_GAP;
        if (tx >= bx && tx <= bx + BTN_W) { password += ' '; drawKeyboard(); continue; }
        bx += BTN_W + BTN_GAP;
        if (tx >= bx && tx <= bx + BTN_W) { if (password.length() > 0) { password.remove(password.length()-1); drawKeyboard(); } continue; }
        bx += BTN_W + BTN_GAP;
        if (tx >= bx && tx <= bx + BTN_W) return "__CANCEL__";
        bx += BTN_W + BTN_GAP;
        if (tx >= bx) return password;
      }
    }
    lastTouched = touched;
    delay(30);
  }
}

// ============================================================
// WiFi 连接
// ============================================================
void connectWiFi() {
  tft.fillScreen(SKY_TOP);
  tft.setTextColor(TEXT_MAIN, SKY_TOP); tft.setTextSize(2);
  tft.setCursor(60, 40); tft.println("Connecting...");
  tft.setTextSize(1); tft.setTextColor(TEXT_DIM, SKY_TOP);
  tft.setCursor(20, 80); tft.println("Looking for saved WiFi...");
  prefs.begin("wifi", true);
  int count = prefs.getInt("count", 0);
  tft.setCursor(20, 100); tft.setTextColor(TEXT_ACCENT, SKY_TOP);
  tft.printf("Found %d saved network(s)", count);
  wifiMulti = WiFiMulti();
  for (int i = 0; i < count; i++) {
    String ssid = prefs.getString(("ssid" + String(i)).c_str(), "");
    String pass = prefs.getString(("pass" + String(i)).c_str(), "");
    if (ssid.length() > 0) wifiMulti.addAP(ssid.c_str(), pass.c_str());
  }
  prefs.end();
  if (count == 0) {
    tft.setCursor(20, 130); tft.setTextColor(TFT_RED, SKY_TOP);
    tft.println("No WiFi saved. Running offline.");
    delay(1500); return;
  }
  tft.setCursor(20, 130); tft.setTextColor(TEXT_DIM, SKY_TOP);
  tft.println("Connecting to strongest...");
  if (wifiMulti.run(10000) == WL_CONNECTED) {
    tft.setCursor(20, 160); tft.setTextColor(0x07E0, SKY_TOP);
    tft.printf("Connected: %s", WiFi.SSID().c_str());
    delay(1000);
  } else {
    tft.setCursor(20, 160); tft.setTextColor(TFT_RED, SKY_TOP);
    tft.println("Failed. Running offline.");
    delay(1500);
  }
}

// ============================================================
// NTP 时间同步
// ============================================================
void syncTime() {
  configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  struct tm ti; int r = 0;
  while (!getLocalTime(&ti) && r < 20) { delay(500); r++; }
}

// ============================================================
// 天气获取
// ============================================================
void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return;
  String url = "http://api.open-meteo.com/v1/forecast?latitude=" + String(LATITUDE, 4) +
               "&longitude=" + String(LONGITUDE, 4) +
               "&current=temperature_2m,relative_humidity_2m,weather_code&timezone=" + TIMEZONE;
  HTTPClient http; http.begin(url); http.setTimeout(10000);
  if (http.GET() == HTTP_CODE_OK) {
    String p = http.getString();
    JsonDocument doc;
    if (!deserializeJson(doc, p)) {
      JsonObject c = doc["current"];
      weather.temperature = c["temperature_2m"] | 0.0f;
      weather.humidity    = c["relative_humidity_2m"] | 0.0f;
      weather.weatherCode = c["weather_code"] | 0;
      weather.valid       = true;
    }
  }
  http.end();
}

// ============================================================
// 天气代码转文字
// ============================================================
String getWeatherDescription(int code) {
  if (code == 0) return "Clear";
  if (code <= 3) return "Cloudy";
  if (code <= 48) return "Foggy";
  if (code <= 67) return "Rain";
  if (code <= 77) return "Snow";
  if (code <= 99) return "Storm";
  return "N/A";
}