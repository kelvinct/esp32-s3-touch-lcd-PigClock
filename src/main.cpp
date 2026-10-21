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
#include <WebServer.h>

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
WebServer webLog(80);

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

int pigX = -40, pig2X = 340, pigY = 224;
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
bool pigFearMode = false;

#define MAX_SNOW 40
struct Snowflake { float x, y; float size; int life; bool active; };
Snowflake snow[MAX_SNOW];

#define MAX_TWINKLE 8
struct Twinkle { int x, y; int phase; };
Twinkle twinkles[MAX_TWINKLE];

#define MAX_NOTIFS 3
struct Notification { String text; uint16_t color; unsigned long createdAt; bool active; };
Notification notifs[MAX_NOTIFS];

const int WIFI_BTN_X = 250, WIFI_BTN_Y = 50, WIFI_BTN_W = 65, WIFI_BTN_H = 22;
const int MENU_BTN_X = 2, MENU_BTN_Y = 2, MENU_BTN_W = 24, MENU_BTN_H = 24;

#define MOCK_BATTERY false
const int   BATTERY_PIN   = 5;
const float BATTERY_RATIO = 3.0;   // 200K + 100K 分壓

float batteryVoltage = 0.0;
int batteryPercent = 100;
unsigned long lastBatteryUpdate = 0;

enum EcoState { ECO_NORMAL, ECO_LIGHT, ECO_DEEP };
EcoState ecoState = ECO_NORMAL;
bool ecoAuto = true;
int  ecoDeepMinutes = 3;
unsigned long ecoLightTimeout = 90000UL;
unsigned long ecoDeepTimeout  = 180000UL;
unsigned long lastUserActivity = 0;
bool ecoJustWokeFlag = false;
const uint8_t BRIGHT_NORMAL = 150;
const uint8_t BRIGHT_LIGHT  = 20;
const uint8_t BRIGHT_DEEP   = 5;

// ★ Wi-Fi 全局状态
bool apConfigMode = false;
volatile bool wifiNeedReconnect = false;
volatile unsigned long wifiDisconnectTime = 0;

// ==================== 函數原型 ====================
void syncTime(); void fetchWeather(); String getWeatherDescription(int code);
void renderBackground(); void drawWeatherIcon(int code, int x, int y); void drawWeatherPanel();
void drawDigitalClock(); void drawAnalogClock();
void drawPig(int x, int y, uint16_t body, uint16_t dark, bool facingRight, bool frozen, bool fear, bool dizzy);
void drawAnimeEffect(int x, int y); void drawFreezeEffect(int x, int y); void drawDizzyEffect(int x, int y); void drawPigCelebration();
void handleTouch(); void spawnSnow(int tx, int ty); void updateSnow(); void drawSnowflake(int x, int y, int size);
uint16_t blendRGB565(uint16_t c1, uint16_t c2, float t);
void updateCelestialState(); void drawCelestialBody(); void drawSun(int cx, int cy, float alpha); void drawMoon(int cx, int cy, float alpha);
void updateBirdAttack(unsigned long now); void drawBird(int x, int y, float wingAngle); void drawDyingBird(int x, int y, float angle);
void spawnFeathers(int x, int y); void updateFeathers(); bool checkBirdTouch(uint16_t tx, uint16_t ty); void killBird();
void updateWolf(unsigned long now); void drawWolf(int x, int y, int dir, float legAngle); void drawDyingWolf(int x, int y, float angle);
bool checkWolfTouch(uint16_t tx, uint16_t ty); void killWolf(); void spawnWolfDust(int x, int y); void updateWolfDust(); void drawWolfDust(); void drawWolfClaw(int x, int y);
void updateBat(unsigned long now); void drawBat(int x, int y, float wingAngle); void drawDyingBat(int x, int y); bool checkBatTouch(uint16_t tx, uint16_t ty); void killBat();
void updateWasp(unsigned long now); void drawWasp(int x, int y, int dir, float wingAngle); void drawDyingWasp(int x, int y); bool checkWaspTouch(uint16_t tx, uint16_t ty); void killWasp();
void updateCrow(unsigned long now); void drawCrow(int x, int y, float wingAngle); void drawDyingCrow(int x, int y); bool checkCrowTouch(uint16_t tx, uint16_t ty); void killCrow();
void updateTrap(unsigned long now); void drawTrap(); bool checkTrapTouch(uint16_t tx, uint16_t ty); void triggerTrap();
void updateFruit(unsigned long now); void drawFruit(); void giveInvincibleToPig(bool isPink);
void addNotification(String text, uint16_t color); void updateNotifications(); void drawNotifications();
int getNextEnemyType(); unsigned long getNextEnemyTime(); void drawNextEnemyBox();
void showBootAnimation(); void showMainMenu(); void showEnemyMenu(); void showInfoScreen();
void showWifiScanner(); String showPasswordKeyboard(String ssid);
void saveWiFi(String ssid, String pass); bool connectWiFi();
void drawEcoClock(bool deep); void setEcoState(EcoState s); void updateEcoTimeouts(); void cycleEcoSetting(); String ecoLabel();
void drawWifiSetupButton(bool animate); bool checkWifiSetupButton(uint16_t tx, uint16_t ty); void drawMenuButton(bool animate); bool checkMenuButton(uint16_t tx, uint16_t ty);
void updateBattery(); void drawBattery(int x, int y);

// ★ Wi-Fi 新函數
void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
void setupWiFiAP();
void showAPModeScreen();

// ================================================================
// Wi-Fi 事件回調
// ================================================================
void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    if (!apConfigMode) {
      wifiNeedReconnect = true;
      wifiDisconnectTime = millis();
      Serial.println("[WiFi] DISCONNECTED");
    }
  } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    wifiNeedReconnect = false;
    Serial.print("[WiFi] GOT IP: ");
    Serial.println(WiFi.localIP());
  }
}

// ================================================================
// setup()
// ================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  randomSeed(esp_random());

  pinMode(BATTERY_PIN, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(BATTERY_PIN, ADC_11db);

  Wire.begin(48, 47, 400000UL);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  canvas.setColorDepth(16);
  canvas.setPsram(true);
  canvas.createSprite(320, 240);

  bgSprite.setColorDepth(16);
  bgSprite.setPsram(true);
  bgSprite.createSprite(320, 240);

  showBootAnimation();

  prefs.begin("clock", true);
  String savedLat = prefs.getString("lat", "");
  String savedLon = prefs.getString("lon", "");
  String savedTz  = prefs.getString("tz", "");
  enemyBird = prefs.getBool("eBird", true);
  enemyWolf = prefs.getBool("eWolf", true);
  enemyBat  = prefs.getBool("eBat", true);
  enemyWasp = prefs.getBool("eWasp", true);
  enemyCrow = prefs.getBool("eCrow", true);
  enemyTrap = prefs.getBool("eTrap", true);
  enemyFruit = prefs.getBool("eFruit", true);
  pigFearMode = prefs.getBool("pigFear", false);
  ecoAuto = prefs.getBool("ecoAuto", true);
  ecoDeepMinutes = prefs.getInt("ecoDeepMin", 3);
  prefs.end();

  updateEcoTimeouts();
  if (savedLat.length() > 0) {
    LATITUDE = savedLat.toFloat();
    LONGITUDE = savedLon.toFloat();
    TIMEZONE = savedTz;
  }

  showMainMenu();

  if (!imu.begin()) {
    Serial.println("QMI8658 init failed!");
  }

  for (int i = 0; i < MAX_TWINKLE; i++) {
    twinkles[i].x = random(5, 315);
    twinkles[i].y = random(5, 150);
    twinkles[i].phase = random(0, 100);
  }
  for (int i = 0; i < MAX_NOTIFS; i++) notifs[i].active = false;
  for (int i = 0; i < MAX_WOLF_DUST; i++) wolfDust[i].active = false;

  renderBackground();
  updateCelestialState();

  // ★ Wi-Fi 初始化 —— 從 Preferences 讀取，失敗自動進 AP 配網
  WiFi.onEvent(onWiFiEvent);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  if (connectWiFi()) {
    Serial.println("[Setup] WiFi OK, syncing time/weather");
    syncTime();
    fetchWeather();
    apConfigMode = false;
  } else {
    Serial.println("[Setup] WiFi fail -> AP config mode");
    setupWiFiAP();
    apConfigMode = true;
    showAPModeScreen();
  }

  // 重置計時器
  unsigned long now = millis();
  lastBatteryUpdate = 0;
  lastWeatherUpdate = now;
  lastClockUpdate = now;
  lastPigUpdate = now;
  lastActivityTime = now;
  lastUserActivity = now;
  ecoState = ECO_NORMAL;
  birdStateTime = now;

  wolfNextSpawn  = now + 20000;
  batNextSpawn   = now + 35000;
  waspNextSpawn  = now + 50000;
  crowNextSpawn  = now + 65000;
  trapNextSpawn  = now + 12000;
  fruitNextSpawn = now + 8000;
}

// ================================================================
// loop()
// ================================================================
void loop() {
  unsigned long now = millis();
  updateBattery();

  // ★ AP 配網模式
  if (apConfigMode) {
    webLog.handleClient();
    uint16_t tx, ty;
    if (tft.getTouch(&tx, &ty) && ty > 220) {
      webLog.stop();
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      apConfigMode = false;
      Serial.println("[AP] Exit config mode");
      renderBackground();
      updateCelestialState();
    }
    delay(30);
    return;
  }

  if (ecoJustWokeFlag) {
    ecoJustWokeFlag = false;
    setCpuFrequencyMhz(240);
    if (WiFi.getMode() != WIFI_STA) { WiFi.mode(WIFI_STA); WiFi.begin(); }
    WiFi.setSleep(false);
  }

  imu.readAccel(ax, ay, az);
  static float sAx = 0, sAy = 0;
  sAx = sAx * 0.85 + ax * 0.15;
  sAy = sAy * 0.85 + ay * 0.15;
  tiltX = constrain(sAy * 1.5, -15, 15);
  tiltY = constrain(sAx * 1.5, -15, 15);

  // ★ 非阻塞重連
  if (wifiNeedReconnect && !apConfigMode && millis() - wifiDisconnectTime > 10000) {
    wifiNeedReconnect = false;
    Serial.println("[WiFi] Reconnecting...");
    WiFi.reconnect();
  }
  if (WiFi.getMode() & WIFI_AP) webLog.handleClient();

  if (ecoAuto) {
    unsigned long idle = now - lastUserActivity;
    if (idle > ecoDeepTimeout && ecoState != ECO_DEEP) setEcoState(ECO_DEEP);
    else if (idle > ecoLightTimeout && ecoState == ECO_NORMAL) setEcoState(ECO_LIGHT);
  }

  if (ecoState != ECO_NORMAL) {
    static int lastDrawSec = -1;
    if (now - lastClockUpdate >= 1000) { lastClockUpdate = now; updateCelestialState(); }
    struct tm ti; bool needRedraw = false;
    if (getLocalTime(&ti, 100)) {
      if (ecoState == ECO_DEEP) { if (ti.tm_min != lastDrawSec) { lastDrawSec = ti.tm_min; needRedraw = true; } }
      else { if (ti.tm_sec != lastDrawSec) { lastDrawSec = ti.tm_sec; needRedraw = true; } }
    }
    if (needRedraw) { drawEcoClock(ecoState == ECO_DEEP); canvas.pushSprite(0, 0); }
    handleTouch();
    delay(50);
    return;
  }

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
  if (pinkInvincible && now > pinkInvincibleEnd) pinkInvincible = false;
  if (whiteInvincible && now > whiteInvincibleEnd) whiteInvincible = false;

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
  bool pigsCharging = birdVisible && !pigFearMode;
  bool pigsFear = birdVisible && pigFearMode;
  bool wolfThreat = (wolfState == 1 || wolfState == 2);
  bool anyActivity = (birdState != 0) || (wolfState != 0) || (batState != 0) ||
                     (waspState != 0) || (crowState != 0) || (trapState != 0) ||
                     (fruitState != 0) || collisionActive || lastTouchState ||
                     pinkDizzy || whiteDizzy || pinkFrozen || whiteFrozen;

  if (anyActivity) { lastActivityTime = now; if (pigSleeping) pigSleeping = false; }
  else { if (!pigSleeping && now - lastActivityTime > SLEEP_DELAY) pigSleeping = true; }

  if (now < shakeEndTime) { shakeX = random(-3, 4); shakeY = random(-3, 4); }
  else { shakeX = 0; shakeY = 0; }

  canvas.pushImage(0, 0, 320, 240, (uint16_t*)bgSprite.getBuffer());
  drawCelestialBody();

  if (1.0 - currentSunAlpha > 0.3) {
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
  if (currentView == 0) drawDigitalClock(); else drawAnalogClock();

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
          birdState = 5; birdStateTime = now; birdDeathAngle = 0; birdDeathVY = -4;
          spawnFeathers(birdX, birdY); spawnFeathers(birdX, birdY);
          shakeEndTime = now + 600; flashEndTime = now + 100;
          pigsCelebrating = true; pigCelebrateEnd = now + 2000; celebrationRotation = 0;
          addNotification("BIRD KO!", 0xFEA0);
        }
      }
    } else if (pigsFear || wolfThreat) {
      int threatX = pigsFear ? birdX : wolfX;
      if (pinkCanMove) { if (pigX < threatX) pigX -= 3; else pigX += 3; }
      if (whiteCanMove) { if (pig2X < threatX) pig2X -= 3; else pig2X += 3; }
      if (pigX < -40) pigX = -40;
      if (pigX > 340) pigX = 340;
      if (pig2X < -40) pig2X = -40;
      if (pig2X > 340) pig2X = 340;
    } else {
      if (pinkCanMove) pigX += pigSpeed;
      if (whiteCanMove) pig2X += pig2Speed;
      if (pinkCanMove && whiteCanMove && bothOnScreen) {
        float dx = pig2X - pigX, dist = abs(dx);
        const float PIG_RADIUS = 18.0f;
        if (dist < PIG_RADIUS * 2 && dist > 0.1f) {
          float overlap = PIG_RADIUS * 2 - dist;
          int push = (int)(overlap * 0.5f) + 1;
          if (dx > 0) { pigX -= push; pig2X += push; }
          else { pigX += push; pig2X -= push; }
          if (!collisionActive) {
            int tmp = pigSpeed; pigSpeed = pig2Speed; pig2Speed = tmp;
            collisionActive = true; collisionStartTime = now;
          }
        }
      }
      if (pigX < -20) { pigX = 340; if (pinkCanMove) pigSpeed = -2; if (abs(pig2X - pigX) < 60) pigX = 260; }
      if (pigX > 340) { pigX = -20; if (pinkCanMove) pigSpeed = 2;  if (abs(pig2X - pigX) < 60) pigX = 60; }
      if (pig2X < -20) { pig2X = 340; if (whiteCanMove) pig2Speed = -2; if (abs(pigX - pig2X) < 60) pig2X = 260; }
      if (pig2X > 340) { pig2X = -20; if (whiteCanMove) pig2Speed = 2;  if (abs(pigX - pig2X) < 60) pig2X = 60; }
    }
    if (collisionActive && now - collisionStartTime > 800) collisionActive = false;
  }

  int bounce  = (pigX  / 4) % 2 == 0 ? 0 : -2;
  int bounce2 = (pig2X / 4) % 2 == 0 ? 0 : -2;
  bool pinkFR, whiteFR;
  if (pigsCharging)      { pinkFR = (pig2X > pigX); whiteFR = (pigX < pig2X); }
  else if (pigsFear)     { pinkFR = (pigX > birdX);  whiteFR = (pig2X > birdX); }
  else if (wolfThreat)   { pinkFR = (pigX > wolfX);  whiteFR = (pig2X > wolfX); }
  else if (collisionStartTime > 0 && now - collisionStartTime < 1500) { pinkFR = (pigSpeed > 0); whiteFR = (pig2Speed > 0); }
  else { pinkFR = (pigSpeed > 0); whiteFR = (pig2X < 160); }

  int pinkWobbleX = 0, pinkWobbleY = 0, whiteWobbleX = 0, whiteWobbleY = 0;
  if (pinkDizzy)  { pinkWobbleX  = sin(now / 100.0) * 3;       pinkWobbleY  = cos(now / 80.0) * 2; }
  if (whiteDizzy) { whiteWobbleX = sin(now / 100.0 + 1.5) * 3; whiteWobbleY = cos(now / 80.0 + 1.5) * 2; }

  if (collisionActive && (pigX >= 20 && pigX <= 300 && pig2X >= 20 && pig2X <= 300)) {
    int midX = (pigX + pig2X) / 2;
    drawAnimeEffect(midX, pigY - 20);
    drawPig(pigX, pigY, PIG_PINK, PIG_PINK_DARK, pinkFR, pinkFrozen, false, false);
    drawPig(pig2X, pigY, PIG_WHITE, PIG_WHITE_DARK, whiteFR, whiteFrozen, false, false);
  } else {
    drawPig(pigX + pinkWobbleX,  pigY + pinkWobbleY  + (pinkFrozen ? 0 : bounce),  PIG_PINK,  PIG_PINK_DARK,  pinkFR,  pinkFrozen,  false, pinkDizzy);
    drawPig(pig2X + whiteWobbleX, pigY + whiteWobbleY + (whiteFrozen ? 0 : bounce2), PIG_WHITE, PIG_WHITE_DARK, whiteFR, whiteFrozen, false, whiteDizzy);
  }

  if (pinkFrozen)  drawFreezeEffect(pigX, pigY);
  if (whiteFrozen) drawFreezeEffect(pig2X, pigY);
  if (pinkDizzy)   drawDizzyEffect(pigX + pinkWobbleX, pigY + pinkWobbleY);
  if (whiteDizzy)  drawDizzyEffect(pig2X + whiteWobbleX, pigY + whiteWobbleY);

  if (enemyBird) {
    if (birdState == 5) drawDyingBird(birdX, birdY, birdDeathAngle);
    else if (birdState != 0) drawBird(birdX, birdY, birdWingAngle);
  }
  if (enemyWolf) {
    if (wolfState == 5) drawDyingWolf(wolfX, wolfY, wolfDeathAngle);
    else if (wolfState != 0) drawWolf(wolfX, wolfY, wolfDir, wolfLegAngle);
  }
  if (enemyBat) {
    if (batState == 5) drawDyingBat(batX, batY);
    else if (batState != 0) drawBat(batX, batY, batWingAngle);
  }
  if (enemyWasp) {
    if (waspState == 5) drawDyingWasp(waspX, waspY);
    else if (waspState != 0) drawWasp(waspX, waspY, waspDir, waspWingAngle);
  }
  if (enemyCrow) {
    if (crowState == 5) drawDyingCrow(crowX, crowY);
    else if (crowState != 0) drawCrow(crowX, crowY, crowWingAngle);
  }

  drawTrap();
  drawFruit();
  if (pigsCelebrating) drawPigCelebration();

  updateFeathers();
  updateSnow();
  updateNotifications();
  drawNotifications();
  drawNextEnemyBox();

  drawWifiSetupButton(true);
  drawMenuButton(true);
  drawBattery(175, 22);

  if (millis() < flashEndTime) canvas.fillRect(0, 0, 320, 240, 0xFFFF);
  canvas.pushSprite(shakeX, shakeY);
  delay(33);
}

// ================================================================
// 電池
// ================================================================
void updateBattery() {
  unsigned long now = millis();
  if (now - lastBatteryUpdate < 5000) return;
  lastBatteryUpdate = now;

#if MOCK_BATTERY
  batteryPercent = 85;
  batteryVoltage = 3.85;
#else
  int mv = analogReadMilliVolts(BATTERY_PIN);
  batteryVoltage = (mv / 1000.0) * BATTERY_RATIO;
  if (batteryVoltage >= 4.15) batteryPercent = 100;
  else if (batteryVoltage <= 3.3) batteryPercent = 0;
  else batteryPercent = (int)(((batteryVoltage - 3.3) / (4.15 - 3.3)) * 100.0);
  batteryPercent = constrain(batteryPercent, 0, 100);
  Serial.printf("[BATTERY] MV: %d mv, Volt: %.2f V, Percent: %d %%\n", mv, batteryVoltage, batteryPercent);
#endif
}

void drawBattery(int x, int y) {
  uint16_t color = 0x07E0;
  if (batteryPercent < 20) color = 0xF800;
  else if (batteryPercent < 50) color = 0xFFE0;
  canvas.drawRect(x, y, 20, 10, 0xFFFF);
  canvas.fillRect(x + 20, y + 3, 2, 4, 0xFFFF);
  int fillWidth = (batteryPercent * 16) / 100;
  if (fillWidth > 0) canvas.fillRect(x + 2, y + 2, fillWidth, 6, color);
  canvas.setTextColor(0xFFFF);
  canvas.setTextSize(1);
  canvas.setCursor(x + 25, y + 2);
  canvas.printf("%d%%", batteryPercent);
}

// ================================================================
// 天氣面板 / 下一敵人框
// ================================================================
void drawWeatherPanel() {
  if (!weather.valid) {
    canvas.setTextColor(TEXT_DIM);
    canvas.setTextSize(1);
    canvas.setCursor(20, 25);
    canvas.println(WiFi.status() == WL_CONNECTED ? "Loading weather..." : "No WiFi - no weather");
    return;
  }
  drawWeatherIcon(weather.weatherCode, 18, 15);
  canvas.setTextColor(TEXT_WARN);
  canvas.setTextSize(2);
  canvas.setCursor(65, 18);
  canvas.printf("%.1f", weather.temperature);
  canvas.setTextColor(TEXT_DIM);
  canvas.setTextSize(1);
  canvas.setCursor(65, 42);
  canvas.printf("Humidity: %.0f%%   %s", weather.humidity, getWeatherDescription(weather.weatherCode).c_str());
}

void drawNextEnemyBox() {
  int nextType = getNextEnemyType();
  if (nextType == 0) return;
  unsigned long waitMs = getNextEnemyTime();
  const int W = 44, H = 44, X = 320 - W - 2, Y = 2;
  canvas.fillRoundRect(X + 2, Y + 2, W, H, 5, 0x0000);
  canvas.fillRoundRect(X, Y, W, H, 5, 0xFE60);
  canvas.fillRoundRect(X + 2, Y + 2, W - 4, H - 4, 4, 0x4208);
  canvas.setTextColor(0xFE60, 0x4208);
  canvas.setTextSize(1);
  canvas.setCursor(X + 8, Y + 6);
  canvas.println("NEXT");
  const char* nm[] = {"", "BIRD", "WOLF", "BAT", "WASP", "CROW", "TRAP", "FRUIT"};
  int nl = strlen(nm[nextType]);
  canvas.setTextColor(0xFFFF, 0x4208);
  canvas.setCursor(X + (W - nl * 6) / 2, Y + 20);
  canvas.println(nm[nextType]);
  float s = waitMs / 1000.0;
  char ts[16];
  if (s <= 0 || waitMs == 0) snprintf(ts, sizeof(ts), "READY");
  else snprintf(ts, sizeof(ts), "%.1fs", s);
  canvas.setTextColor(0xFE60, 0x4208);
  int tl = strlen(ts);
  canvas.setCursor(X + (W - tl * 6) / 2, Y + 32);
  canvas.println(ts);
}

int getNextEnemyType() {
  int enabled[7]; int count = 0;
  if (enemyBird) enabled[count++] = 1;
  if (enemyWolf) enabled[count++] = 2;
  if (enemyBat)  enabled[count++] = 3;
  if (enemyWasp) enabled[count++] = 4;
  if (enemyCrow) enabled[count++] = 5;
  if (enemyTrap) enabled[count++] = 6;
  if (enemyFruit) enabled[count++] = 7;
  if (count == 0) return 0;
  return enabled[millis() / 2500 % count];
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

// ================================================================
// 豬
// ================================================================
void drawPig(int x, int y, uint16_t body, uint16_t dark, bool facingRight, bool frozen, bool fear, bool dizzy) {
  if (frozen) { body = ICE_BLUE; dark = ICE_LIGHT; }
  uint16_t snout = frozen ? ICE_WHITE : ((body == PIG_PINK) ? 0xFC9F : 0xB5B6);
  if (pigSleeping) {
    canvas.fillEllipse(x, y + 2, 14, 6, body);
    canvas.fillEllipse(x - 3, y, 7, 3, dark);
    if (facingRight) {
      canvas.fillCircle(x + 12, y + 1, 7, body);
      canvas.fillTriangle(x + 7, y - 5, x + 9, y - 11, x + 14, y - 6, body);
      canvas.fillEllipse(x + 17, y + 2, 3, 3, snout);
      canvas.drawLine(x + 11, y - 1, x + 15, y - 1, 0x0000);
    } else {
      canvas.fillCircle(x - 12, y + 1, 7, body);
      canvas.fillTriangle(x - 7, y - 5, x - 9, y - 11, x - 14, y - 6, body);
      canvas.fillEllipse(x - 17, y + 2, 3, 3, snout);
      canvas.drawLine(x - 11, y - 1, x - 15, y - 1, 0x0000);
    }
    canvas.fillRect(x - 6, y + 6, 3, 4, body);
    canvas.fillRect(x - 1, y + 6, 3, 4, body);
    canvas.fillRect(x + 4, y + 6, 3, 4, body);
    unsigned long now = millis();
    for (int i = 0; i < 3; i++) {
      int ph = (now / 500 + i) % 3;
      int zx = x + (facingRight ? 15 : -15) + ph * 4;
      int zy = y - 8 - ph * 5;
      canvas.setTextColor(0x5D9B);
      canvas.setTextSize(1 + ph / 2);
      canvas.setCursor(zx, zy);
      canvas.println("z");
    }
    return;
  }
  int legPhase = (millis() / 80) % 2;
  int legOffset = legPhase ? 1 : -1;
  uint16_t eyeColor = frozen ? ICE_WHITE : 0x0000;
  if (facingRight) {
    canvas.fillEllipse(x, y, 11, 8, body);
    canvas.fillEllipse(x - 2, y - 2, 6, 3, dark);
    canvas.fillCircle(x + 12, y - 2, 8, body);
    canvas.fillTriangle(x + 7, y - 8, x + 9, y - 15, x + 14, y - 9, body);
    canvas.fillTriangle(x + 9, y - 9, x + 10, y - 13, x + 13, y - 10, dark);
    canvas.fillEllipse(x + 18, y - 2, 4, 3, snout);
    canvas.fillCircle(x + 16, y - 2, 1, eyeColor);
    canvas.fillCircle(x + 20, y - 2, 1, eyeColor);
    if (dizzy) { canvas.drawCircle(x + 13, y - 5, 2, 0x0000); canvas.drawCircle(x + 13, y - 5, 4, 0x0000); }
    else if (fear) { canvas.fillCircle(x + 13, y - 5, 4, 0xFFFF); canvas.fillCircle(x + 14, y - 6, 1, 0x0000); }
    else { canvas.fillCircle(x + 13, y - 5, 2, eyeColor); if (!frozen) canvas.fillCircle(x + 12, y - 6, 1, 0xFFFF); }
    if (!frozen) canvas.fillCircle(x + 10, y + 1, 2, 0xFBE0);
    canvas.fillRect(x - 7, y + 6 + legOffset, 3, 6, body);
    canvas.fillRect(x - 2, y + 6 - legOffset, 3, 6, body);
    canvas.fillRect(x + 3, y + 6 + legOffset, 3, 6, body);
    canvas.fillRect(x + 8, y + 6 - legOffset, 3, 6, body);
    canvas.drawCircle(x - 13, y - 1, 3, dark);
    canvas.drawCircle(x - 13, y - 1, 1, body);
  } else {
    canvas.fillEllipse(x, y, 11, 8, body);
    canvas.fillEllipse(x + 2, y - 2, 6, 3, dark);
    canvas.fillCircle(x - 12, y - 2, 8, body);
    canvas.fillTriangle(x - 7, y - 8, x - 9, y - 15, x - 14, y - 9, body);
    canvas.fillTriangle(x - 9, y - 9, x - 10, y - 13, x - 13, y - 10, dark);
    canvas.fillEllipse(x - 18, y - 2, 4, 3, snout);
    canvas.fillCircle(x - 16, y - 2, 1, eyeColor);
    canvas.fillCircle(x - 20, y - 2, 1, eyeColor);
    if (dizzy) { canvas.drawCircle(x - 13, y - 5, 2, 0x0000); canvas.drawCircle(x - 13, y - 5, 4, 0x0000); }
    else if (fear) { canvas.fillCircle(x - 13, y - 5, 4, 0xFFFF); canvas.fillCircle(x - 14, y - 6, 1, 0x0000); }
    else { canvas.fillCircle(x - 13, y - 5, 2, eyeColor); if (!frozen) canvas.fillCircle(x - 14, y - 6, 1, 0xFFFF); }
    if (!frozen) canvas.fillCircle(x - 10, y + 1, 2, 0xFBE0);
    canvas.fillRect(x + 7, y + 6 + legOffset, 3, 6, body);
    canvas.fillRect(x + 2, y + 6 - legOffset, 3, 6, body);
    canvas.fillRect(x - 3, y + 6 + legOffset, 3, 6, body);
    canvas.fillRect(x - 8, y + 6 - legOffset, 3, 6, body);
    canvas.drawCircle(x + 13, y - 1, 3, dark);
    canvas.drawCircle(x + 13, y - 1, 1, body);
  }
}

// ================================================================
// 鳥
// ================================================================
void updateBirdAttack(unsigned long now) {
  switch (birdState) {
    case 0: if (now - birdStateTime > 15000) { birdState = 1; birdStateTime = now; birdX = random(60, 260); birdY = -30; birdTarget = random(1, 3); } break;
    case 1: birdY += 3.5; birdX += sin(now / 300.0) * 2; birdWingAngle = sin(now / 100.0) * 0.5; if (birdY > 50) { birdState = 2; birdStateTime = now; } break;
    case 2: {
      int tX = (birdTarget == 1) ? pigX : pig2X;
      int tY = pigY - 10;
      float dx = tX - birdX, dy = tY - birdY, d = sqrt(dx * dx + dy * dy);
      if (d < 5) {
        if (birdTarget == 1) { pinkDizzy = true; pinkDizzyEnd = now + DIZZY_DURATION; }
        else { whiteDizzy = true; whiteDizzyEnd = now + DIZZY_DURATION; }
        birdState = 3; birdStateTime = now;
        shakeEndTime = now + 300;
        spawnFeathers(tX, tY);
      } else { birdX += (dx / d) * 6; birdY += (dy / d) * 6; birdWingAngle = sin(now / 50.0) * 0.8; }
      break;
    }
    case 3: if (now - birdStateTime > 600) { birdState = 4; birdStateTime = now; } break;
    case 4: birdY -= 3; birdX += sin(now / 200.0) * 2; birdWingAngle = sin(now / 80.0) * 0.8; if (birdY < -50) { birdState = 0; birdStateTime = now; } break;
    case 5: birdDeathAngle += 15; birdDeathVY += 0.5; birdY += birdDeathVY; birdX += sin(now / 100.0) * 1.5; if (now - birdStateTime > 1500 || birdY > 260) { birdState = 0; birdStateTime = now; } break;
  }
}

void drawBird(int x, int y, float wingAngle) {
  int wo = sin(wingAngle) * 8;
  canvas.fillTriangle(x - 5, y, x - 22, y + wo - 5, x - 10, y + 8, BIRD_DARK);
  canvas.fillTriangle(x + 5, y, x + 22, y + wo - 5, x + 10, y + 8, BIRD_DARK);
  canvas.fillEllipse(x, y, 12, 10, BIRD_BODY);
  canvas.fillCircle(x, y - 8, 8, BIRD_BODY);
  canvas.fillTriangle(x - 5, y - 8, x + 5, y - 8, x, y - 2, BIRD_BEAK);
  canvas.fillCircle(x - 3, y - 10, 2, BIRD_EYE);
  canvas.fillCircle(x + 3, y - 10, 2, BIRD_EYE);
  canvas.fillCircle(x - 3, y - 11, 1, 0xFFFF);
  canvas.fillCircle(x + 3, y - 11, 1, 0xFFFF);
  canvas.fillTriangle(x - 8, y + 8, x + 8, y + 8, x, y + 18, BIRD_DARK);
}

void drawDyingBird(int x, int y, float angle) {
  int w = sin(angle * DEG_TO_RAD) * 5;
  canvas.fillTriangle(x - 5, y, x - 20, y + w + 5, x - 10, y + 8, BIRD_DARK);
  canvas.fillTriangle(x + 5, y, x + 20, y + w + 5, x + 10, y + 8, BIRD_DARK);
  canvas.fillEllipse(x, y, 12, 10, BIRD_BODY);
  canvas.fillCircle(x, y - 8, 8, BIRD_BODY);
  canvas.fillTriangle(x - 5, y - 8, x + 5, y - 8, x, y - 2, BIRD_BEAK);
  canvas.drawLine(x - 5, y - 12, x - 1, y - 8, 0x0000);
  canvas.drawLine(x - 1, y - 12, x - 5, y - 8, 0x0000);
  canvas.drawLine(x + 1, y - 12, x + 5, y - 8, 0x0000);
  canvas.drawLine(x + 5, y - 12, x + 1, y - 8, 0x0000);
  canvas.fillRect(x - 2, y - 2, 4, 5, 0xF81F);
  canvas.fillTriangle(x - 8, y + 8, x + 8, y + 8, x, y + 18, BIRD_DARK);
  canvas.setTextColor(0xF800);
  canvas.setTextSize(3);
  canvas.setCursor(x - 20, y - 40);
  canvas.println("KO!");
  canvas.drawRect(x - 22, y - 42, 55, 28, 0xFFFF);
}

bool checkBirdTouch(uint16_t tx, uint16_t ty) {
  return (birdState != 0 && birdState != 5 && abs((int)tx - birdX) < 25 && abs((int)ty - birdY) < 25);
}

void killBird() {
  canvas.fillCircle(birdX, birdY, 20, 0xFFFF);
  canvas.setTextColor(0xF800);
  canvas.setTextSize(2);
  canvas.setCursor(birdX - 20, birdY - 10);
  canvas.println("KILL!");
  canvas.pushSprite(0, 0);
  delay(200);
  birdState = 5; birdStateTime = millis();
  birdDeathAngle = 0; birdDeathVY = -3;
  flashEndTime = millis() + 100;
}

// ================================================================
// 狼
// ================================================================
void updateWolf(unsigned long now) {
  switch (wolfState) {
    case 0:
      if (now > wolfNextSpawn) {
        wolfState = 1; wolfStateTime = now;
        if (random(0, 2) == 0) { wolfX = -50; wolfDir = 1; }
        else { wolfX = 370; wolfDir = -1; }
        wolfY = 224;
        wolfTarget = random(1, 3);
        wolfHowlEnd = now + 800;
      }
      break;
    case 1:
      wolfX += wolfDir * 3;
      wolfLegAngle += 0.3;
      if ((millis() / 50) % 2 == 0) spawnWolfDust(wolfX, wolfY + 8);
      if ((wolfDir == 1 && wolfX > 50) || (wolfDir == -1 && wolfX < 270)) { wolfState = 2; wolfStateTime = now; }
      break;
    case 2: {
      int tX = (wolfTarget == 1) ? pigX : pig2X;
      wolfDir = (tX > wolfX) ? 1 : -1;
      wolfX += wolfDir * 2;
      wolfLegAngle += 0.3;
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
      wolfX += wolfDir * 4;
      wolfLegAngle += 0.5;
      if (millis() % 60 == 0) spawnWolfDust(wolfX, wolfY + 8);
      if (wolfX < -60 || wolfX > 380) { wolfState = 0; wolfNextSpawn = now + 15000 + random(10000); }
      break;
    case 5:
      wolfDeathAngle += 18;
      wolfDeathVY += 0.6;
      wolfY += wolfDeathVY;
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
    canvas.fillTriangle(headX + dir * 4, y + 1, headX + dir * 4, y + 6, headX + dir * 10, y + 3, 0x0000);
    canvas.drawPixel(headX + dir * 5, y + 2, 0xFFFF);
    canvas.drawPixel(headX + dir * 7, y + 3, 0xFFFF);
    int waveR = 12 + ((now - (wolfHowlEnd - 800)) / 40);
    canvas.drawCircle(headX, y - 2, waveR, 0xFFE0);
    canvas.drawCircle(headX, y - 2, waveR + 5, 0xFBE0);
  } else {
    canvas.drawLine(headX + dir * 5, y + 2, headX + dir * 9, y + 1, 0xFFFF);
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
  canvas.fillTriangle(x - dir * 12, y - 3, x - dir * 22, y - 8, x - dir * 20, y + 2, WOLF_DARK);
  drawWolfDust();
  if (now < wolfClawEnd) { drawWolfClaw(pigX, pigY); drawWolfClaw(pig2X, pigY); }
}

void drawDyingWolf(int x, int y, float angle) {
  int w = sin(angle * DEG_TO_RAD) * 4;
  canvas.fillEllipse(x, y, 14, 9, WOLF_BROWN);
  int headX = x + 12;
  canvas.fillEllipse(headX, y - 2 + w, 9, 8, WOLF_BROWN);
  canvas.drawLine(headX - 4, y - 7, headX - 1, y - 4, 0x0000);
  canvas.drawLine(headX - 1, y - 7, headX - 4, y - 4, 0x0000);
  canvas.drawLine(headX + 2, y - 7, headX + 5, y - 4, 0x0000);
  canvas.drawLine(headX + 5, y - 7, headX + 2, y - 4, 0x0000);
  canvas.fillRect(headX + 3, y + 1, 3, 5, 0xF81F);
  canvas.setTextColor(0xF800);
  canvas.setTextSize(2);
  canvas.setCursor(x - 18, y - 38);
  canvas.println("KO!");
}

void spawnWolfDust(int x, int y) {
  for (int i = 0; i < MAX_WOLF_DUST; i++) {
    if (!wolfDust[i].active) {
      wolfDust[i].x = x + random(-8, 8);
      wolfDust[i].y = y + random(-2, 4);
      wolfDust[i].vx = random(-15, 16) / 10.0;
      wolfDust[i].vy = random(-25, -5) / 10.0;
      wolfDust[i].life = random(15, 30);
      wolfDust[i].active = true;
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
  canvas.drawLine(x - 20, y - 15, x - 5, y + 5, 0xF800);
  canvas.drawLine(x - 15, y - 18, x, y + 2, 0xF800);
  canvas.drawLine(x - 10, y - 20, x + 5, y - 1, 0xF800);
  canvas.drawLine(x - 19, y - 15, x - 6, y + 3, 0xFBE0);
  canvas.drawLine(x - 14, y - 18, x - 1, y, 0xFBE0);
  canvas.drawLine(x - 9, y - 20, x + 4, y - 2, 0xFBE0);
}

bool checkWolfTouch(uint16_t tx, uint16_t ty) {
  return (wolfState != 0 && wolfState != 5 && abs((int)tx - wolfX) < 35 && abs((int)ty - wolfY) < 30);
}

void killWolf() {
  canvas.fillCircle(wolfX, wolfY, 22, 0xFFFF);
  canvas.setTextColor(0xF800);
  canvas.setTextSize(2);
  canvas.setCursor(wolfX - 22, wolfY - 10);
  canvas.println("BOOM!");
  canvas.pushSprite(0, 0);
  delay(200);
  wolfState = 5; wolfStateTime = millis();
  wolfDeathAngle = 0; wolfDeathVY = -3;
  flashEndTime = millis() + 100;
  addNotification("BOOM!", WOLF_BROWN);
}

// ================================================================
// 蝙蝠
// ================================================================
void updateBat(unsigned long now) {
  switch (batState) {
    case 0: if (now > batNextSpawn) { batState = 1; batStateTime = now; batX = random(60, 260); batY = -30; batWavePhase = 0; } break;
    case 1: batY += 3; batWavePhase += 0.15; batWingAngle = sin(now / 60.0) * 0.8; if (batY > 40) { batState = 2; batStateTime = now; } break;
    case 2:
      batY += sin(now / 200.0) * 1.5;
      batX += sin(now / 150.0) * 2;
      batWingAngle = sin(now / 50.0) * 0.9;
      if (batY < 20) batY = 20;
      if (batY > 150) batY = 150;
      if (batX < 20) batX = 20;
      if (batX > 300) batX = 300;
      if (now - batStateTime > 5000 && !pinkDizzy && !whiteDizzy) {
        int targetX = (abs(batX - pigX) < abs(batX - pig2X)) ? pigX : pig2X;
        batX += (targetX - batX) * 0.1;
        batY += (pigY - 50 - batY) * 0.1;
        if (abs(batX - targetX) < 30 && batY > 150) {
          bool inv = (targetX == pigX) ? pinkInvincible : whiteInvincible;
          if (inv) { addNotification("REFLECT!", 0xFFE0); flashEndTime = now + 100; }
          else if (targetX == pigX) { pinkDizzy = true; pinkDizzyEnd = now + 2500; }
          else { whiteDizzy = true; whiteDizzyEnd = now + 2500; }
          batState = 3; batStateTime = now;
          shakeEndTime = now + 300;
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
  canvas.fillTriangle(x - 3, y, x - 22, y - 6 + wo, x - 10, y + 6, BAT_BODY);
  canvas.fillTriangle(x + 3, y, x + 22, y - 6 + wo, x + 10, y + 6, BAT_BODY);
  canvas.fillEllipse(x, y, 6, 8, BAT_BODY);
  canvas.fillCircle(x, y - 8, 5, BAT_BODY);
  canvas.fillTriangle(x - 5, y - 8, x - 8, y - 16, x - 2, y - 10, BAT_BODY);
  canvas.fillTriangle(x + 5, y - 8, x + 8, y - 16, x + 2, y - 10, BAT_BODY);
  canvas.fillCircle(x - 2, y - 9, 1, 0xF800);
  canvas.fillCircle(x + 2, y - 9, 1, 0xF800);
  canvas.fillCircle(x, y - 14, 1, 0xFFFF);
}

void drawDyingBat(int x, int y) {
  canvas.fillEllipse(x, y, 6, 8, BAT_BODY);
  canvas.fillCircle(x, y + 8, 5, BAT_BODY);
  canvas.drawLine(x - 3, y + 7, x - 1, y + 9, 0xF800);
  canvas.drawLine(x - 1, y + 7, x - 3, y + 9, 0xF800);
  canvas.drawLine(x + 1, y + 7, x + 3, y + 9, 0xF800);
  canvas.drawLine(x + 3, y + 7, x + 1, y + 9, 0xF800);
  canvas.setTextColor(0xF800);
  canvas.setTextSize(2);
  canvas.setCursor(x - 20, y - 25);
  canvas.println("KO!");
}

bool checkBatTouch(uint16_t tx, uint16_t ty) {
  return (batState != 0 && batState != 5 && abs((int)tx - batX) < 25 && abs((int)ty - batY) < 25);
}

void killBat() {
  canvas.fillCircle(batX, batY, 20, 0xFFFF);
  canvas.pushSprite(0, 0);
  delay(200);
  batState = 5; batStateTime = millis();
  flashEndTime = millis() + 100;
}

// ================================================================
// 黃蜂
// ================================================================
void updateWasp(unsigned long now) {
  switch (waspState) {
    case 0:
      if (now > waspNextSpawn) {
        waspState = 1; waspStateTime = now;
        if (random(0, 2) == 0) { waspX = -30; waspDir = 1; }
        else { waspX = 350; waspDir = -1; }
        waspY = 150 + random(-50, 50);
      }
      break;
    case 1:
      waspX += waspDir * 6;
      waspY += sin(now / 100.0) * 1;
      waspWingAngle = sin(now / 20.0) * 0.9;
      if ((waspDir == 1 && waspX > 60) || (waspDir == -1 && waspX < 260)) { waspState = 2; waspStateTime = now; }
      break;
    case 2:
      waspX += waspDir * 4;
      waspY += sin(now / 80.0) * 3;
      waspWingAngle = sin(now / 15.0) * 0.9;
      if (waspY < 30) waspY = 30;
      if (waspY > 210) waspY = 210;
      if (waspX < 10) { waspX = 10; waspDir = 1; }
      if (waspX > 310) { waspX = 310; waspDir = -1; }
      if (abs(waspX - pigX) < 20 && abs(waspY - pigY) < 20) {
        if (pinkInvincible) { addNotification("REFLECT!", 0xFFE0); flashEndTime = now + 100; }
        else { pinkDizzy = true; pinkDizzyEnd = now + 2500; }
        waspState = 3; waspStateTime = now;
        shakeEndTime = now + 200;
      } else if (abs(waspX - pig2X) < 20 && abs(waspY - pigY) < 20) {
        if (whiteInvincible) { addNotification("REFLECT!", 0xFFE0); flashEndTime = now + 100; }
        else { whiteDizzy = true; whiteDizzyEnd = now + 2500; }
        waspState = 3; waspStateTime = now;
        shakeEndTime = now + 200;
      }
      if (now - waspStateTime > 6000) { waspState = 4; waspStateTime = now; }
      break;
    case 3: if (now - waspStateTime > 400) { waspState = 4; waspStateTime = now; } break;
    case 4: waspX += waspDir * 8; waspWingAngle = sin(now / 15.0) * 0.9; if (waspX < -50 || waspX > 380) { waspState = 0; waspNextSpawn = now + 20000 + random(15000); } break;
  }
}

void drawWasp(int x, int y, int dir, float wingAngle) {
  int wo = sin(wingAngle) * 3;
  canvas.fillEllipse(x - 4, y - 6 + wo, 5, 3, 0xBDFF);
  canvas.fillEllipse(x + 4, y - 6 + wo, 5, 3, 0xBDFF);
  canvas.fillEllipse(x, y, 5, 8, WASP_BODY);
  canvas.drawLine(x - 4, y - 3, x + 4, y - 3, WASP_STRIPE);
  canvas.drawLine(x - 5, y, x + 5, y, WASP_STRIPE);
  canvas.drawLine(x - 4, y + 3, x + 4, y + 3, WASP_STRIPE);
  canvas.fillCircle(x + dir * 2, y - 8, 4, WASP_STRIPE);
  canvas.fillCircle(x + dir * 1, y - 8, 1, 0xFFFF);
  canvas.fillCircle(x + dir * 4, y - 8, 1, 0xFFFF);
  canvas.fillTriangle(x - 2, y + 8, x + 2, y + 8, x, y + 13, 0xF800);
}

void drawDyingWasp(int x, int y) {
  canvas.fillEllipse(x, y, 5, 8, WASP_BODY);
  canvas.fillCircle(x, y - 8, 4, 0x0000);
  canvas.drawLine(x - 3, y - 9, x - 1, y - 7, 0xF800);
  canvas.drawLine(x - 1, y - 9, x - 3, y - 7, 0xF800);
  canvas.drawLine(x + 1, y - 9, x + 3, y - 7, 0xF800);
  canvas.drawLine(x + 3, y - 9, x + 1, y - 7, 0xF800);
  canvas.setTextColor(0xF800);
  canvas.setTextSize(2);
  canvas.setCursor(x - 18, y - 25);
  canvas.println("ZAP!");
}

bool checkWaspTouch(uint16_t tx, uint16_t ty) {
  return (waspState != 0 && waspState != 5 && abs((int)tx - waspX) < 20 && abs((int)ty - waspY) < 20);
}

void killWasp() {
  canvas.fillCircle(waspX, waspY, 18, 0xFFFF);
  canvas.pushSprite(0, 0);
  delay(200);
  waspState = 5; waspStateTime = millis();
  flashEndTime = millis() + 100;
  addNotification("ZAP!", 0xFE00);
}

// ================================================================
// 烏鴉
// ================================================================
void updateCrow(unsigned long now) {
  switch (crowState) {
    case 0: if (now > crowNextSpawn) { crowState = 1; crowStateTime = now; crowX = random(60, 260); crowY = -40; crowTarget = random(1, 3); } break;
    case 1: crowY += 2; crowX += sin(now / 250.0) * 2; crowWingAngle = sin(now / 80.0) * 0.7; if (crowY > 50) { crowState = 2; crowStateTime = now; } break;
    case 2: {
      int tX = (crowTarget == 1) ? pigX : pig2X;
      int tY = pigY - 10;
      float dx = tX - crowX, dy = tY - crowY, d = sqrt(dx * dx + dy * dy);
      if (d < 6) {
        bool inv = (crowTarget == 1) ? pinkInvincible : whiteInvincible;
        if (inv) { addNotification("REFLECT!", 0xFFE0); flashEndTime = now + 100; }
        else if (crowTarget == 1) { pinkDizzy = true; pinkDizzyEnd = now + 2500; }
        else { whiteDizzy = true; whiteDizzyEnd = now + 2500; }
        crowState = 3; crowStateTime = now;
        shakeEndTime = now + 400;
      } else { crowX += (dx / d) * 5; crowY += (dy / d) * 5; crowWingAngle = sin(now / 40.0) * 0.9; }
      break;
    }
    case 3: if (now - crowStateTime > 600) { crowState = 4; crowStateTime = now; } break;
    case 4: crowY -= 4; crowX += sin(now / 150.0) * 2; crowWingAngle = sin(now / 60.0) * 0.8; if (crowY < -50) { crowState = 0; crowNextSpawn = now + 30000 + random(20000); } break;
  }
}

void drawCrow(int x, int y, float wingAngle) {
  canvas.drawCircle(x, y, 15, 0xF81F);
  canvas.drawCircle(x, y, 16, 0x7BEF);
  int wo = sin(wingAngle) * 10;
  canvas.fillTriangle(x - 5, y, x - 25, y - 8 + wo, x - 10, y + 8, CROW_BODY);
  canvas.fillTriangle(x + 5, y, x + 25, y - 8 + wo, x + 10, y + 8, CROW_BODY);
  canvas.fillEllipse(x, y, 12, 9, CROW_BODY);
  canvas.fillCircle(x, y - 8, 7, CROW_BODY);
  canvas.fillTriangle(x - 4, y - 8, x + 4, y - 8, x, y - 1, CROW_BEAK);
  canvas.fillCircle(x - 3, y - 9, 2, 0xF800);
  canvas.fillCircle(x + 3, y - 9, 2, 0xF800);
  canvas.fillCircle(x - 3, y - 9, 1, 0x0000);
  canvas.fillCircle(x + 3, y - 9, 1, 0x0000);
  canvas.fillTriangle(x - 8, y + 8, x + 8, y + 8, x, y + 20, 0x0000);
  canvas.fillCircle(x, y - 16, 1, 0xFFFF);
}

void drawDyingCrow(int x, int y) {
  canvas.fillEllipse(x, y, 12, 9, CROW_BODY);
  canvas.fillCircle(x, y - 8, 7, CROW_BODY);
  canvas.fillTriangle(x - 4, y - 8, x + 4, y - 8, x, y - 1, CROW_BEAK);
  canvas.drawLine(x - 5, y - 11, x - 1, y - 7, 0xF800);
  canvas.drawLine(x - 1, y - 11, x - 5, y - 7, 0xF800);
  canvas.drawLine(x + 1, y - 11, x + 5, y - 7, 0xF800);
  canvas.drawLine(x + 5, y - 11, x + 1, y - 7, 0xF800);
  canvas.fillRect(x - 1, y - 2, 3, 5, 0xF81F);
  canvas.fillTriangle(x - 8, y + 8, x + 8, y + 8, x, y + 20, 0x0000);
  canvas.setTextColor(0xF800);
  canvas.setTextSize(2);
  canvas.setCursor(x - 20, y - 35);
  canvas.println("KO!");
}

bool checkCrowTouch(uint16_t tx, uint16_t ty) {
  return (crowState != 0 && crowState != 5 && abs((int)tx - crowX) < 25 && abs((int)ty - crowY) < 25);
}

void killCrow() {
  canvas.fillCircle(crowX, crowY, 20, 0xFFFF);
  canvas.pushSprite(0, 0);
  delay(200);
  crowState = 5; crowStateTime = millis();
  flashEndTime = millis() + 100;
  addNotification("CROW KO!", 0x1082);
}

// ================================================================
// 陷阱
// ================================================================
void updateTrap(unsigned long now) {
  if (!enemyTrap) { trapState = 0; trapActive = false; return; }
  switch (trapState) {
    case 0: if (now > trapNextSpawn) { trapState = 1; trapStateTime = now; trapX = random(50, 270); trapY = 236; } break;
    case 1: if (now - trapStateTime > 800) { trapState = 2; trapStateTime = now; trapActive = true; } break;
    case 2:
      if (!pinkFrozen && !pinkDizzy && abs(pigX - trapX) < 15) {
        if (pinkInvincible) addNotification("IMMUNE!", 0xFFE0);
        else { pinkDizzy = true; pinkDizzyEnd = now + 2000; shakeEndTime = now + 300; flashEndTime = now + 80; addNotification("TRAP!", 0xF800); }
        trapState = 3; trapStateTime = now; trapActive = false;
      } else if (!whiteFrozen && !whiteDizzy && abs(pig2X - trapX) < 15) {
        if (whiteInvincible) addNotification("IMMUNE!", 0xFFE0);
        else { whiteDizzy = true; whiteDizzyEnd = now + 2000; shakeEndTime = now + 300; flashEndTime = now + 80; addNotification("TRAP!", 0xF800); }
        trapState = 3; trapStateTime = now; trapActive = false;
      }
      if (now - trapStateTime > 10000) { trapState = 3; trapStateTime = now; trapActive = false; }
      break;
    case 3: if (now - trapStateTime > 400) { trapState = 0; trapNextSpawn = now + 15000 + random(15000); } break;
  }
}

void drawTrap() {
  if (trapState == 0 || !enemyTrap) return;
  unsigned long now = millis();
  if (trapState == 1) {
    if ((now / 150) % 2 == 0) {
      canvas.fillTriangle(trapX - 12, trapY, trapX + 12, trapY, trapX, trapY - 10, 0xF800);
      canvas.drawTriangle(trapX - 12, trapY, trapX + 12, trapY, trapX, trapY - 10, 0xFBE0);
      canvas.setTextColor(0xFFFF); canvas.setTextSize(1);
      canvas.setCursor(trapX - 1, trapY - 8);
      canvas.println("!");
    }
  } else if (trapState == 2) {
    canvas.fillRect(trapX - 14, trapY - 2, 28, 4, 0x0000);
    canvas.fillRect(trapX - 13, trapY - 1, 26, 3, 0x4208);
    for (int i = 0; i < 3; i++) {
      int sx = trapX - 8 + i * 8;
      canvas.fillTriangle(sx - 3, trapY - 1, sx + 3, trapY - 1, sx, trapY - 14, 0xC618);
      canvas.drawTriangle(sx - 3, trapY - 1, sx + 3, trapY - 1, sx, trapY - 14, 0xFFFF);
      canvas.drawLine(sx, trapY - 12, sx - 1, trapY - 3, 0xFFFF);
    }
    if ((now / 300) % 2 == 0) canvas.drawLine(trapX - 13, trapY, trapX + 13, trapY, 0xF800);
  } else if (trapState == 3) {
    unsigned long e = now - trapStateTime;
    int offset = e / 50; if (offset > 14) offset = 14;
    canvas.fillRect(trapX - 13, trapY - 1, 26, 3, 0x4208);
    for (int i = 0; i < 3; i++) {
      int sx = trapX - 8 + i * 8;
      int spH = 14 - offset;
      if (spH > 0) canvas.fillTriangle(sx - 3, trapY - 1, sx + 3, trapY - 1, sx, trapY - 1 - spH, 0xC618);
    }
    if (e < 300 && (e / 80) % 2 == 0) {
      canvas.drawLine(trapX - 8, trapY - 15, trapX + 8, trapY - 3, 0xF800);
      canvas.drawLine(trapX + 8, trapY - 15, trapX - 8, trapY - 3, 0xF800);
    }
  }
}

bool checkTrapTouch(uint16_t tx, uint16_t ty) {
  if (trapState != 2) return false;
  if (abs((int)tx - trapX) < 20 && abs((int)ty - trapY) < 20) {
    trapState = 3;
    trapStateTime = millis();
    trapActive = false;
    addNotification("DEFUSED!", 0x07E0);
    return true;
  }
  return false;
}

void triggerTrap() {}

// ================================================================
// 水果
// ================================================================
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

void drawFruit() {
  if (fruitState == 0 || !enemyFruit) return;
  unsigned long now = millis();
  if (fruitState == 1) {
    int gr = 10 + (int)(sin(now / 150.0) * 3);
    canvas.fillCircle(fruitX, fruitY - 4, gr, 0x07E0);
    canvas.fillCircle(fruitX, fruitY - 4, gr - 2, 0x0FE0);
    canvas.fillCircle(fruitX, fruitY - 4, 7, 0xF800);
    canvas.fillCircle(fruitX, fruitY - 4, 6, 0xFC00);
    canvas.fillCircle(fruitX - 2, fruitY - 6, 2, 0xFFFF);
    canvas.fillTriangle(fruitX, fruitY - 11, fruitX - 5, fruitY - 14, fruitX + 3, fruitY - 13, 0x07E0);
    canvas.drawLine(fruitX, fruitY - 11, fruitX - 1, fruitY - 15, 0x4208);
    for (int i = 0; i < 4; i++) {
      float a = (i * 90 + now / 20.0) * DEG_TO_RAD;
      int sx = fruitX + 14 * cos(a), sy = fruitY - 4 + 14 * sin(a);
      uint16_t sc = (i % 2 == 0) ? 0xFFE0 : 0x07FF;
      canvas.drawPixel(sx, sy, sc);
      canvas.drawPixel(sx - 1, sy, sc);
      canvas.drawPixel(sx + 1, sy, sc);
      canvas.drawPixel(sx, sy - 1, sc);
      canvas.drawPixel(sx, sy + 1, sc);
    }
    canvas.drawLine(fruitX - 8, fruitY + 3, fruitX + 8, fruitY + 3, 0x07E0);
  } else if (fruitState == 2) {
    unsigned long e = now - fruitStateTime;
    int offset = e / 20; if (offset > 30) offset = 30;
    int al = 255 - (offset * 8); if (al < 0) al = 0;
    uint16_t fc = blendRGB565(0x0000, 0xF800, al / 255.0);
    canvas.fillCircle(fruitX, fruitY - 4 - offset, 7, fc);
    if (e < 300) for (int i = 0; i < 8; i++) {
      float a = i * 45 * DEG_TO_RAD;
      int sx = fruitX + (offset + 5) * cos(a);
      int sy = fruitY - 4 + (offset + 5) * sin(a);
      if ((e / 60) % 2 == 0) canvas.fillCircle(sx, sy, 2, 0xFFFF);
    }
  }
}

void giveInvincibleToPig(bool isPink) {
  unsigned long now = millis();
  if (isPink) { pinkInvincible = true; pinkInvincibleEnd = now + INVINCIBLE_DURATION; addNotification("PINK INVINCIBLE!", 0x07E0); }
  else { whiteInvincible = true; whiteInvincibleEnd = now + INVINCIBLE_DURATION; addNotification("WHITE INVINCIBLE!", 0x07FF); }
  flashEndTime = now + 100;
}

// ================================================================
// 特效
// ================================================================
void drawAnimeEffect(int x, int y) {
  canvas.fillCircle(x, y, 32, 0xFFFF);
  canvas.drawCircle(x, y, 34, 0xFC9F);
  for (int i = 0; i < 360; i += 30) {
    float r = i * DEG_TO_RAD;
    canvas.drawLine(x + 15 * cos(r), y + 15 * sin(r), x + 38 * cos(r), y + 38 * sin(r), 0xFF2C);
  }
  canvas.setTextColor(0xF800);
  canvas.setTextSize(2);
  canvas.setCursor(x - 22, y - 8);
  canvas.println("DOKI!");
  canvas.fillCircle(x - 15, y - 30, 4, 0xF81F);
  canvas.fillCircle(x - 9, y - 30, 4, 0xF81F);
  canvas.fillTriangle(x - 19, y - 30, x - 5, y - 30, x - 12, y - 20, 0xF81F);
  canvas.fillCircle(x + 9, y - 30, 4, 0xF81F);
  canvas.fillCircle(x + 15, y - 30, 4, 0xF81F);
  canvas.fillTriangle(x + 5, y - 30, x + 19, y - 30, x + 12, y - 20, 0xF81F);
}

void drawFreezeEffect(int x, int y) {
  unsigned long now = millis();
  for (int i = 0; i < 8; i++) {
    float angle = (i * 45 + (now / 50)) * DEG_TO_RAD;
    int r = 22 + (sin(now / 200.0 + i) * 3);
    int ix = x + r * cos(angle), iy = y + r * sin(angle) * 0.7;
    canvas.drawPixel(ix, iy, ICE_WHITE);
    canvas.drawPixel(ix - 1, iy, ICE_LIGHT);
    canvas.drawPixel(ix + 1, iy, ICE_LIGHT);
    canvas.drawPixel(ix, iy - 1, ICE_LIGHT);
    canvas.drawPixel(ix, iy + 1, ICE_LIGHT);
  }
  canvas.drawCircle(x, y + 10, 15, ICE_BLUE);
  canvas.drawCircle(x, y + 10, 16, ICE_LIGHT);
}

void drawDizzyEffect(int x, int y) {
  unsigned long now = millis();
  for (int i = 0; i < 3; i++) {
    float angle = (now / 200.0 + i * 120) * DEG_TO_RAD;
    int r = 18;
    int sx = x + r * cos(angle), sy = y - 22 + r * sin(angle) * 0.4;
    uint16_t starColor = (i == 0) ? DIZZY_STAR1 : ((i == 1) ? DIZZY_STAR2 : DIZZY_STAR3);
    canvas.drawPixel(sx, sy, starColor);
    canvas.drawPixel(sx - 1, sy, starColor);
    canvas.drawPixel(sx + 1, sy, starColor);
    canvas.drawPixel(sx, sy - 1, starColor);
    canvas.drawPixel(sx, sy + 1, starColor);
  }
  canvas.setTextColor(0xFFFF);
  canvas.setTextSize(1);
  canvas.setCursor(x - 10, y + 18);
  canvas.println("Zzz...");
}

void drawPigCelebration() {
  unsigned long now = millis();
  int midX = (pigX + pig2X) / 2;
  int midY = pigY - 40;
  const char* msg = "PIG POWER!!";
  int msgX = midX - 60, msgY = midY - 30;
  uint16_t rainbow[] = {0xF800, 0xFD20, 0xFFE0, 0x07E0, 0x07FF, 0x001F, 0xF81F};
  for (int i = 0; i < 11; i++) {
    canvas.setTextColor(rainbow[i % 7]);
    canvas.setTextSize(2);
    canvas.setCursor(msgX + i * 11, msgY + sin(now / 80.0 + i) * 3);
    canvas.printf("%c", msg[i]);
  }
}

void spawnFeathers(int x, int y) {
  for (int i = 0; i < MAX_FEATHERS; i++) {
    if (!feathers[i].active) {
      feathers[i].x = x;
      feathers[i].y = y;
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
        canvas.drawLine(feathers[i].x, feathers[i].y, feathers[i].x + 3, feathers[i].y + 2, BIRD_FEATHER);
        canvas.drawLine(feathers[i].x, feathers[i].y, feathers[i].x - 2, feathers[i].y + 3, BIRD_FEATHER);
      }
    }
  }
}

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
        snow[i].active = false; addNotification("FROZEN!", 0x3D7F);
        continue;
      }
      if (!whiteFrozen && abs((int)snow[i].x - pig2X) < 20 && abs((int)snow[i].y - pigY) < 15) {
        whiteFrozen = true; whiteFreezeEnd = now + FREEZE_DURATION;
        whiteFrozenSpeed = pig2Speed; pig2Speed = 0;
        snow[i].active = false; addNotification("FROZEN!", 0x3D7F);
        continue;
      }
      if (snow[i].life <= 0 || snow[i].y > 240 || snow[i].size <= 1) snow[i].active = false;
      else drawSnowflake(snow[i].x, snow[i].y, snow[i].size);
    }
  }
}

void drawSnowflake(int x, int y, int size) {
  canvas.drawPixel(x, y, 0xFFFF);
  canvas.drawLine(x, y - size, x, y + size, 0xFFFF);
  canvas.drawLine(x - size * 0.8, y - size * 0.5, x + size * 0.8, y + size * 0.5, 0xFFFF);
  canvas.drawLine(x - size * 0.8, y + size * 0.5, x + size * 0.8, y - size * 0.5, 0xFFFF);
}

// ================================================================
// 通知
// ================================================================
void addNotification(String text, uint16_t color) {
  for (int i = MAX_NOTIFS - 1; i > 0; i--) notifs[i] = notifs[i - 1];
  notifs[0].text = text;
  notifs[0].color = color;
  notifs[0].createdAt = millis();
  notifs[0].active = true;
}

void updateNotifications() {
  unsigned long now = millis();
  for (int i = 0; i < MAX_NOTIFS; i++)
    if (notifs[i].active && now - notifs[i].createdAt > 3000) notifs[i].active = false;
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
    canvas.fillRoundRect(drawX + 2, y + 2, BOX_W, BOX_H, 6, 0x0000);
    canvas.fillRoundRect(drawX, y, BOX_W, BOX_H, 6, 0x2124);
    canvas.drawRoundRect(drawX, y, BOX_W, BOX_H, 6, notifs[i].color);
    canvas.fillRect(drawX + 3, y + 4, 4, BOX_H - 8, notifs[i].color);
    canvas.setTextColor(notifs[i].color, 0x2124);
    canvas.setTextSize(1);
    canvas.setCursor(drawX + 12, y + 10);
    canvas.println(notifs[i].text);
  }
}

// ================================================================
// 日月星辰 / 背景
// ================================================================
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
    canvas.drawLine(cx + 17 * cos(ang), cy + 17 * sin(ang), cx + 26 * cos(ang), cy + 26 * sin(ang), (i % 2 == 0) ? rc : rc2);
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

uint16_t blendRGB565(uint16_t c1, uint16_t c2, float t) {
  if (t <= 0) return c1;
  if (t >= 1) return c2;
  int r1 = (c1 >> 11) & 0x1F, g1 = (c1 >> 5) & 0x3F, b1 = c1 & 0x1F;
  int r2 = (c2 >> 11) & 0x1F, g2 = (c2 >> 5) & 0x3F, b2 = c2 & 0x1F;
  return ((int)(r1 + (r2 - r1) * t) << 11) | ((int)(g1 + (g2 - g1) * t) << 5) | (int)(b1 + (b2 - b1) * t);
}

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
      bgSprite.drawPixel(x + 1, y, STAR_COLOR);
      bgSprite.drawPixel(x, y + 1, STAR_COLOR);
      bgSprite.drawPixel(x + 1, y + 1, STAR_COLOR);
    }
  }
  bgSprite.fillEllipse(160, 290, 220, 75, HILL_COLOR);
  bgSprite.fillEllipse(160, 295, 220, 70, HILL_HIGHLIGHT);
  for (int i = 0; i < 15; i++) bgSprite.drawPixel(random(20, 300), random(215, 235), 0xFD20);
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

// ================================================================
// 時鐘
// ================================================================
void drawDigitalClock() {
  struct tm ti;
  if (!getLocalTime(&ti, 100)) return;
  char timeStr[6]; strftime(timeStr, sizeof(timeStr), "%H:%M", &ti);
  char secStr[3];  strftime(secStr, sizeof(secStr), "%S", &ti);
  char dateStr[32]; strftime(dateStr, sizeof(dateStr), "%A, %d %b", &ti);
  canvas.setTextColor(TEXT_SHADOW); canvas.setTextSize(7); canvas.setCursor(24, 84); canvas.println(timeStr);
  canvas.setTextColor(TEXT_MAIN);   canvas.setCursor(22, 82); canvas.println(timeStr);
  canvas.setTextColor(TEXT_ACCENT); canvas.setTextSize(3); canvas.setCursor(268, 138); canvas.println(secStr);
  canvas.setTextColor(TEXT_DIM);    canvas.setTextSize(2); canvas.setCursor(28, 184); canvas.println(dateStr);
  canvas.fillCircle(24, 78, 3, 0x07FF);
}

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
    int x1 = cx + (R - 12) * sin(a), y1 = cy - (R - 12) * cos(a);
    int x2 = cx + (R - 6) * sin(a),  y2 = cy - (R - 6) * cos(a);
    uint16_t c = (i % 3 == 0) ? 0xFFFF : 0x5D9B;
    canvas.drawLine(x1, y1, x2, y2, c);
  }
  float hA = (ti.tm_hour % 12 + ti.tm_min / 60.0) * 30 * DEG_TO_RAD;
  float mA = (ti.tm_min + ti.tm_sec / 60.0) * 6 * DEG_TO_RAD;
  float sA = ti.tm_sec * 6 * DEG_TO_RAD;
  int hL = R * 0.5, mL = R * 0.72, sL = R * 0.82;
  int hx = cx + hL * sin(hA), hy = cy - hL * cos(hA);
  int mx = cx + mL * sin(mA), my = cy - mL * cos(mA);
  int sx = cx + sL * sin(sA), sy = cy - sL * cos(sA);
  int sox = tiltX * 0.8, soy = tiltY * 0.8;
  canvas.drawLine(cx + sox, cy + soy, hx + sox, hy + soy, 0x0000);
  canvas.drawLine(cx + sox, cy + soy, mx + sox, my + soy, 0x0000);
  canvas.drawLine(cx + sox, cy + soy, sx + sox, sy + soy, 0x0000);
  canvas.drawLine(cx, cy, hx, hy, 0xFFFF);
  canvas.drawLine(cx, cy, mx, my, 0x07FF);
  canvas.drawLine(cx, cy, sx, sy, 0xF800);
  canvas.fillCircle(cx, cy, 5, 0x39E7);
  canvas.fillCircle(cx - 1, cy - 1, 3, 0xFFFF);
  canvas.fillCircle(cx, cy, 2, 0xF800);
}

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

void drawWeatherIcon(int code, int x, int y) {
  if (code == 0) {
    if (isDaytime) {
      canvas.fillCircle(x + 18, y + 18, 9, 0xFF2C);
      canvas.drawCircle(x + 18, y + 18, 10, 0xFC67);
      for (int i = 0; i < 360; i += 45) {
        float r = i * DEG_TO_RAD;
        canvas.drawLine(x + 18 + 12 * cos(r), y + 18 + 12 * sin(r), x + 18 + 16 * cos(r), y + 18 + 16 * sin(r), 0xFF2C);
      }
    } else {
      canvas.fillCircle(x + 18, y + 18, 9, 0xFFFF);
      canvas.fillCircle(x + 23, y + 15, 9, CARD_BG);
      canvas.drawCircle(x + 18, y + 18, 10, 0x5D9B);
    }
  } else {
    canvas.fillCircle(x + 12, y + 20, 8, 0x4208);
    canvas.fillCircle(x + 22, y + 18, 10, 0x8410);
    canvas.fillCircle(x + 32, y + 20, 8, 0x4208);
    canvas.fillRect(x + 12, y + 20, 20, 8, 0x8410);
    if (code >= 51) {
      canvas.drawLine(x + 15, y + 30, x + 13, y + 36, 0x5D9B);
      canvas.drawLine(x + 23, y + 30, x + 21, y + 36, 0x5D9B);
      canvas.drawLine(x + 31, y + 30, x + 29, y + 36, 0x5D9B);
    }
    if (code >= 95) {
      canvas.drawLine(x + 22, y + 28, x + 18, y + 36, 0xFF2C);
      canvas.drawLine(x + 18, y + 36, x + 24, y + 36, 0xFF2C);
    }
  }
}

// ================================================================
// 按鈕
// ================================================================
void drawWifiSetupButton(bool animate) {
  if (ecoState == ECO_DEEP || WiFi.status() == WL_CONNECTED) return;
  unsigned long now = millis();
  uint16_t borderCol = animate ? ((now / 400) % 2 ? 0xFBE0 : 0xFFFF) : 0xFBE0;
  uint16_t txtCol = animate ? ((now / 400) % 2 ? 0xFFFF : 0xFBE0) : 0xFFFF;
  canvas.fillRoundRect(WIFI_BTN_X + 1, WIFI_BTN_Y + 1, WIFI_BTN_W, WIFI_BTN_H, 6, 0x0000);
  canvas.fillRoundRect(WIFI_BTN_X, WIFI_BTN_Y, WIFI_BTN_W, WIFI_BTN_H, 6, 0xC000);
  canvas.drawRoundRect(WIFI_BTN_X, WIFI_BTN_Y, WIFI_BTN_W, WIFI_BTN_H, 6, borderCol);
  int ix = WIFI_BTN_X + 10, iy = WIFI_BTN_Y + 14;
  canvas.fillCircle(ix, iy, 1, 0xFFFF);
  canvas.drawCircle(ix, iy, 4, 0xFFFF);
  canvas.drawCircle(ix, iy, 8, 0xFFFF);
  if (animate) {
    float a = (now / 200.0) * DEG_TO_RAD;
    canvas.drawLine(ix, iy, ix + 8 * cos(a), iy + 8 * sin(a), 0xFFFF);
  }
  canvas.setTextColor(txtCol);
  canvas.setTextSize(1);
  canvas.setCursor(WIFI_BTN_X + 22, WIFI_BTN_Y + 8);
  canvas.println("Scan WiFi");
}

bool checkWifiSetupButton(uint16_t tx, uint16_t ty) {
  if (ecoState == ECO_DEEP || WiFi.status() == WL_CONNECTED) return false;
  return (tx >= WIFI_BTN_X && tx <= WIFI_BTN_X + WIFI_BTN_W && ty >= WIFI_BTN_Y && ty <= WIFI_BTN_Y + WIFI_BTN_H);
}

void drawMenuButton(bool animate) {
  if (ecoState == ECO_DEEP) return;
  unsigned long now = millis();
  uint16_t borderCol = animate ? ((now / 400) % 2 ? 0x5D9B : 0xFFFF) : 0x5D9B;
  uint16_t txtCol = animate ? ((now / 400) % 2 ? 0xFFFF : 0x5D9B) : 0xFFFF;
  canvas.fillRoundRect(MENU_BTN_X, MENU_BTN_Y, MENU_BTN_W, MENU_BTN_H, 6, 0x2124);
  canvas.drawRoundRect(MENU_BTN_X, MENU_BTN_Y, MENU_BTN_W, MENU_BTN_H, 6, borderCol);
  int cx = MENU_BTN_X + 12, cy = MENU_BTN_Y + 12;
  canvas.drawLine(cx - 6, cy - 5, cx + 6, cy - 5, txtCol);
  canvas.drawLine(cx - 6, cy,     cx + 6, cy,     txtCol);
  canvas.drawLine(cx - 6, cy + 5, cx + 6, cy + 5, txtCol);
}

bool checkMenuButton(uint16_t tx, uint16_t ty) {
  if (ecoState == ECO_DEEP) return false;
  return (tx >= MENU_BTN_X && tx <= MENU_BTN_X + MENU_BTN_W && ty >= MENU_BTN_Y && ty <= MENU_BTN_Y + MENU_BTN_H);
}

// ================================================================
// 省電模式
// ================================================================
void drawEcoClock(bool deep) {
  canvas.fillScreen(0x0000);
  struct tm ti;
  if (!getLocalTime(&ti, 100)) return;
  char hhmm[6]; strftime(hhmm, sizeof(hhmm), "%H:%M", &ti);
  canvas.setTextColor(0xFFFF); canvas.setTextSize(7); canvas.setCursor(55, 85); canvas.println(hhmm);
  if (!deep) {
    char ss[3]; strftime(ss, sizeof(ss), "%S", &ti);
    canvas.setTextColor(0x07FF); canvas.setTextSize(3); canvas.setCursor(272, 138); canvas.println(ss);
  }
  canvas.setTextColor(0x07E0); canvas.setTextSize(1); canvas.setCursor(6, 6);
  canvas.println(deep ? "DEEP ECO" : "ECO");
  drawBattery(270, 6);
  canvas.setTextColor(0x5D9B); canvas.setTextSize(1); canvas.setCursor(100, 225);
  canvas.println("Tap to wake");
  drawWifiSetupButton(false);
  drawMenuButton(false);
}

static void applyBrightness(uint8_t b) { tft.setBrightness(b); }

void setEcoState(EcoState s) {
  if (s == ecoState) return;
  unsigned long now = millis(); (void)now;
  switch (s) {
    case ECO_NORMAL: applyBrightness(BRIGHT_NORMAL); ecoJustWokeFlag = true; break;
    case ECO_LIGHT:  applyBrightness(BRIGHT_LIGHT); WiFi.setSleep(true); break;
    case ECO_DEEP:   applyBrightness(BRIGHT_DEEP);  WiFi.mode(WIFI_OFF); break;
  }
  ecoState = s;
}

void updateEcoTimeouts() {
  ecoDeepTimeout  = (unsigned long)ecoDeepMinutes * 60000UL;
  ecoLightTimeout = ecoDeepTimeout / 2;
  if (ecoLightTimeout < 15000UL) ecoLightTimeout = 15000UL;
}

void cycleEcoSetting() {
  if (!ecoAuto) { ecoAuto = true; ecoDeepMinutes = 3; }
  else if (ecoDeepMinutes == 3)  ecoDeepMinutes = 5;
  else if (ecoDeepMinutes == 5)  ecoDeepMinutes = 10;
  else if (ecoDeepMinutes == 10) ecoDeepMinutes = 30;
  else ecoDeepMinutes = 3;
  updateEcoTimeouts();
  prefs.begin("clock", false);
  prefs.putBool("ecoAuto", ecoAuto);
  prefs.putInt("ecoDeepMin", ecoDeepMinutes);
  prefs.end();
  if (!ecoAuto) setEcoState(ECO_NORMAL);
}

String ecoLabel() {
  if (!ecoAuto) return "Eco: OFF";
  return String("Eco: ") + String(ecoDeepMinutes) + "min";
}

// ================================================================
// 觸控
// ================================================================
void handleTouch() {
  uint16_t x, y;
  bool touched = tft.getTouch(&x, &y);
  static unsigned long lastSnow = 0, tStart = 0;
  static bool switched = false;

  if (touched) {
    unsigned long now = millis();
    lastUserActivity = now;

    if (checkMenuButton(x, y)) {
      if (ecoState != ECO_NORMAL) { setEcoState(ECO_NORMAL); applyBrightness(BRIGHT_NORMAL); }
      showMainMenu();
      renderBackground();
      updateCelestialState();
      lastTouchState = true;
      return;
    }
    if (checkWifiSetupButton(x, y)) {
      if (ecoState != ECO_NORMAL) { setEcoState(ECO_NORMAL); applyBrightness(BRIGHT_NORMAL); delay(100); }
      showWifiScanner();
      renderBackground();
      updateCelestialState();
      return;
    }

    if (ecoState != ECO_NORMAL) {
      setEcoState(ECO_NORMAL);
      canvas.fillScreen(0xFFFF);
      canvas.pushSprite(0, 0);
      delay(30);
      canvas.pushImage(0, 0, 320, 240, (uint16_t*)bgSprite.getBuffer());
      drawCelestialBody();
      drawWeatherPanel();
      if (currentView == 0) drawDigitalClock(); else drawAnalogClock();
      int b1 = (pigX / 4) % 2 == 0 ? 0 : -2;
      int b2 = (pig2X / 4) % 2 == 0 ? 0 : -2;
      drawPig(pigX, pigY + b1, PIG_PINK, PIG_PINK_DARK, pigSpeed > 0, pinkFrozen, false, pinkDizzy);
      drawPig(pig2X, pigY + b2, PIG_WHITE, PIG_WHITE_DARK, pig2Speed > 0, whiteFrozen, false, whiteDizzy);
      drawWifiSetupButton(true);
      drawMenuButton(true);
      drawBattery(175, 22);
      canvas.pushSprite(0, 0);
      lastTouchState = true;
      return;
    }

    if (enemyBird && checkBirdTouch(x, y)) { killBird(); return; }
    if (enemyWolf && checkWolfTouch(x, y)) { killWolf(); return; }
    if (enemyBat  && checkBatTouch(x, y))  { killBat();  return; }
    if (enemyWasp && checkWaspTouch(x, y)) { killWasp(); return; }
    if (enemyCrow && checkCrowTouch(x, y)) { killCrow(); return; }
    if (checkTrapTouch(x, y)) { return; }

    if (now - lastSnow > 50) { lastSnow = now; spawnSnow(x, y); }

    if (!lastTouchState) { tStart = now; switched = false; }
    else if (!switched && (now - tStart > 1000)) {
      currentView = (currentView == 0) ? 1 : 0;
      switched = true;
    }
  }
  lastTouchState = touched;
}

// ================================================================
// 開機動畫 / 主菜單 / 敵人菜單 / Info
// ================================================================
void showBootAnimation() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(PIG_PINK);
  tft.setTextSize(3);
  tft.setCursor(50, 95);
  tft.println("Smart Clock");
  delay(1000);
  tft.fillScreen(TFT_BLACK);
}

void showEnemyMenu() { /* 省略 */ }

void showInfoScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(20, 55);
  tft.printf("IP: %s\n", WiFi.localIP().toString().c_str());
  delay(2000);
}

// ================================================================
// ★★★ 完整替換：可互動的 WiFi 掃描器 ★★★
// ================================================================
void showWifiScanner() {
  Serial.println("[Scanner] Scanning...");
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 8);
  tft.println("Wi-Fi Scanner");

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 32);
  tft.println("Scanning... please wait");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();

  tft.fillRect(0, 32, 320, 208, TFT_BLACK);

  if (n <= 0) {
    tft.setTextColor(TFT_RED);
    tft.setCursor(10, 50);
    tft.println(n == 0 ? "No networks found." : "Scan failed!");
    delay(1500);
    WiFi.scanDelete();
    return;
  }

  int displayLimit = (n > 8) ? 8 : n;
  int boxW = 148, boxH = 38;
  int startX = 8, startY = 36;
  int spacingX = 8, spacingY = 6;

  int boxX[8], boxY[8];

  for (int i = 0; i < displayLimit; i++) {
    int row = i / 2, col = i % 2;
    int x = startX + col * (boxW + spacingX);
    int y = startY + row * (boxH + spacingY);
    boxX[i] = x; boxY[i] = y;

    String ssid = WiFi.SSID(i);
    int32_t rssi = WiFi.RSSI(i);
    uint16_t bc = TFT_GREEN;
    if (rssi < -80) bc = TFT_RED;
    else if (rssi < -65) bc = TFT_YELLOW;

    String disp = ssid;
    if (disp.length() > 13) disp = disp.substring(0, 10) + "...";
    if (disp.length() == 0) disp = "<hidden>";

    tft.drawRoundRect(x, y, boxW, boxH, 4, bc);
    tft.fillRoundRect(x + 2, y + 2, boxW - 4, boxH - 4, 3, 0x141414);
    tft.fillCircle(x + 12, y + boxH / 2, 3, bc);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.setCursor(x + 22, y + 8);
    tft.print(disp);
    tft.setTextColor(TFT_DARKGREY);
    tft.setCursor(x + 22, y + 22);
    tft.printf("%d dBm", rssi);
  }

  const int retX = 10, retY = 220, retW = 90, retH = 18;
  tft.drawRoundRect(retX, retY, retW, retH, 4, 0xF800);
  tft.setTextColor(0xF800);
  tft.setCursor(retX + 20, retY + 5);
  tft.print("< Back");

  bool lastTouch = false;
  unsigned long lastHit = 0;
  uint16_t tx, ty;
  int selected = -1;

  while (true) {
    bool touched = tft.getTouch(&tx, &ty);
    if (touched && !lastTouch && millis() - lastHit > 250) {
      lastHit = millis();
      if (tx >= retX && tx <= retX + retW && ty >= retY && ty <= retY + retH) {
        WiFi.scanDelete();
        return;
      }
      for (int i = 0; i < displayLimit; i++) {
        if (tx >= boxX[i] && tx <= boxX[i] + boxW &&
            ty >= boxY[i] && ty <= boxY[i] + boxH) {
          selected = i;
          break;
        }
      }
      if (selected >= 0) break;
    }
    lastTouch = touched;
    delay(20);
  }

  String chosenSSID = WiFi.SSID(selected);
  WiFi.scanDelete();
  Serial.printf("[Scanner] Selected: %s\n", chosenSSID.c_str());

  String pass = showPasswordKeyboard(chosenSSID);
  if (pass == "__CANCEL__") { showWifiScanner(); return; }

  saveWiFi(chosenSSID, pass);

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE); tft.setTextSize(2);
  tft.setCursor(20, 100); tft.println("Connecting...");
  tft.setTextSize(1); tft.setCursor(20, 130); tft.println(chosenSSID);

  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(chosenSSID.c_str(), pass.c_str());
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) delay(200);

  tft.fillScreen(TFT_BLACK);
  if (WiFi.status() == WL_CONNECTED) {
    tft.setTextColor(TFT_GREEN); tft.setTextSize(2);
    tft.setCursor(20, 90); tft.println("Connected!");
    tft.setTextColor(TFT_WHITE); tft.setTextSize(1);
    tft.setCursor(20, 130); tft.println(WiFi.localIP().toString());
    syncTime();
    fetchWeather();
  } else {
    tft.setTextColor(TFT_RED); tft.setTextSize(2);
    tft.setCursor(20, 90); tft.println("Failed");
    tft.setTextColor(TFT_YELLOW); tft.setTextSize(1);
    tft.setCursor(20, 130); tft.println("Check password / signal");
  }
  delay(2500);
}

// ================================================================
// ★★★ 完整替換：真正的密碼鍵盤 ★★★
// 返回 "__CANCEL__" 表示取消
// ================================================================
String showPasswordKeyboard(String ssid) {
  String pass = "";
  bool shift = false;
  bool lastTouch = false;
  unsigned long lastHit = 0;
  uint16_t tx, ty;

  const char* rowNum = "1234567890";
  const char* rowQ   = "qwertyuiop";
  const char* rowA   = "asdfghjkl-";
  const char* rowZ   = "zxcvbnm._";
  const int KY = 52, KH = 34, KW = 32;

  auto drawUI = [&]() {
    tft.fillScreen(0x0000);
    tft.setTextColor(0x07FF, 0x0000);
    tft.setTextSize(1);
    tft.setCursor(6, 4);
    String shortSSID = ssid.length() > 20 ? ssid.substring(0, 20) : ssid;
    tft.printf("WiFi: %s", shortSSID.c_str());

    tft.drawRect(6, 18, 308, 28, 0xFFFF);
    tft.setTextColor(0xFFFF, 0x0000);
    tft.setTextSize(2);
    tft.setCursor(12, 23);
    String shown = pass.length() > 18 ? "..." + pass.substring(pass.length() - 15) : pass;
    tft.printf("%s_", shown.c_str());

    for (int r = 0; r < 4; r++) {
      const char* row = (r == 0) ? rowNum : (r == 1) ? rowQ : (r == 2) ? rowA : rowZ;
      for (int c = 0; c < 10; c++) {
        int x = c * KW;
        int y = KY + r * KH;
        tft.fillRoundRect(x + 1, y + 1, KW - 2, KH - 2, 3, 0x2124);
        tft.drawRoundRect(x + 1, y + 1, KW - 2, KH - 2, 3, 0x5D9B);
        tft.setTextColor(0xFFFF, 0x2124);
        tft.setTextSize(2);
        char ch = row[c];
        if (shift && ch >= 'a' && ch <= 'z') ch -= 32;
        tft.setCursor(x + 11, y + 8);
        tft.printf("%c", ch);
      }
    }

    int fy = KY + 4 * KH;
    int fw = 320 / 4;
    struct { const char* label; uint16_t bg; } fkeys[4] = {
      { shift ? "SHIFT*" : "SHIFT", shift ? (uint16_t)0x07E0 : (uint16_t)0x4208 },
      { "DEL",    0xF800 },
      { "CANCEL", 0xFFE0 },
      { "OK",     0x07E0 }
    };
    for (int i = 0; i < 4; i++) {
      int x = i * fw;
      tft.fillRoundRect(x + 1, fy + 1, fw - 2, KH - 2, 3, fkeys[i].bg);
      tft.setTextColor(0x0000, fkeys[i].bg);
      tft.setTextSize(1);
      int tl = strlen(fkeys[i].label);
      tft.setCursor(x + (fw - tl * 6) / 2, fy + 12);
      tft.print(fkeys[i].label);
    }
  };

  drawUI();

  while (true) {
    bool touched = tft.getTouch(&tx, &ty);
    if (touched && !lastTouch && millis() - lastHit > 120) {
      lastHit = millis();
      if (ty >= KY) {
        int row = (ty - KY) / KH;
        int col = tx / KW;
        if (row >= 0 && row < 4 && col >= 0 && col < 10) {
          const char* rowStr = (row == 0) ? rowNum : (row == 1) ? rowQ : (row == 2) ? rowA : rowZ;
          char ch = rowStr[col];
          if (shift && ch >= 'a' && ch <= 'z') ch -= 32;
          if (pass.length() < 63) pass += ch;
          drawUI();
        } else if (row == 4) {
          int fw = 320 / 4;
          int fkey = tx / fw;
          if (fkey == 0) { shift = !shift; drawUI(); }
          else if (fkey == 1) { if (pass.length() > 0) pass.remove(pass.length() - 1); drawUI(); }
          else if (fkey == 2) return "__CANCEL__";
          else if (fkey == 3) return pass;
        }
      }
    }
    lastTouch = touched;
    delay(20);
  }
}

// ================================================================
// 保存 Wi-Fi (多 AP 去重)
// ================================================================
void saveWiFi(String ssid, String pass) {
  if (ssid.length() == 0) return;
  prefs.begin("wifi", false);
  int count = prefs.getInt("count", 0);

  bool found = false;
  for (int i = 0; i < count; i++) {
    char ks[15], kp[15];
    snprintf(ks, sizeof(ks), "s_%d", i);
    snprintf(kp, sizeof(kp), "p_%d", i);
    if (prefs.getString(ks, "") == ssid) {
      prefs.putString(kp, pass);
      found = true;
      break;
    }
  }
  if (!found && count < 8) {
    char ks[15], kp[15];
    snprintf(ks, sizeof(ks), "s_%d", count);
    snprintf(kp, sizeof(kp), "p_%d", count);
    prefs.putString(ks, ssid);
    prefs.putString(kp, pass);
    prefs.putInt("count", count + 1);
    Serial.printf("[WiFi] Saved new AP: %s (Total: %d)\n", ssid.c_str(), count + 1);
  } else if (found) {
    Serial.printf("[WiFi] Updated password for: %s\n", ssid.c_str());
  } else {
    Serial.println("[WiFi] AP list full, cannot save more");
  }
  prefs.end();
}

// ================================================================
// 讀取 Wi-Fi 並嘗試連線
// ================================================================
bool connectWiFi() {
  prefs.begin("wifi", true);
  int count = prefs.getInt("count", 0);
  if (count == 0) {
    Serial.println("[WiFi] No saved APs.");
    prefs.end();
    return false;
  }
  WiFi.mode(WIFI_STA);
  delay(50);

  int loaded = 0;
  for (int i = 0; i < count; i++) {
    char ks[15], kp[15];
    snprintf(ks, sizeof(ks), "s_%d", i);
    snprintf(kp, sizeof(kp), "p_%d", i);
    String s = prefs.getString(ks, "");
    String p = prefs.getString(kp, "");
    if (s.length() > 0) {
      wifiMulti.addAP(s.c_str(), p.c_str());
      Serial.printf("[WiFi] Loaded AP[%d]: %s\n", i, s.c_str());
      loaded++;
    }
  }
  prefs.end();
  if (loaded == 0) return false;

  Serial.println("[WiFi] Connecting to strongest saved AP...");
  if (wifiMulti.run(6000) == WL_CONNECTED) {
    Serial.print("[WiFi] Connected! IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }
  Serial.println("[WiFi] Connect timeout.");
  return false;
}

// ================================================================
// ★ AP 配網模式
// ================================================================
void setupWiFiAP() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("SmartClock-Config", "smartclock");
  delay(100);

  Serial.print("[AP] SSID: SmartClock-Config  IP: ");
  Serial.println(WiFi.softAPIP());

  webLog.on("/", HTTP_GET, []() {
    String html = R"HTML(<!DOCTYPE html>
<html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Smart Clock WiFi Setup</title>
<style>
body{font-family:-apple-system,Roboto,sans-serif;max-width:420px;margin:0 auto;padding:16px;background:#141428;color:#eee;}
h2{color:#07e0ff;text-align:center;margin-top:0;}
input,button{width:100%;padding:12px;margin:8px 0;border-radius:8px;border:1px solid #333;background:#242438;color:#fff;font-size:16px;box-sizing:border-box;}
button{background:#07e0ff;color:#000;font-weight:bold;cursor:pointer;border:none;}
button:hover{background:#05b8cc;}
.ssid-item{padding:12px;margin:6px 0;background:#242438;border-radius:6px;cursor:pointer;border:1px solid #333;font-size:14px;}
.ssid-item:hover{background:#343448;}
.hint{color:#889;font-size:12px;text-align:center;}
#status{text-align:center;color:#889;padding:10px;}
</style></head><body>
<h2>&#128295; Smart Clock WiFi</h2>
<div id="status">Scanning networks...</div>
<div id="list"></div>
<input type="text" id="ssid" placeholder="SSID (or pick above)">
<input type="password" id="pass" placeholder="Password">
<button onclick="save()">Save &amp; Connect</button>
<p class="hint">Device will reboot after save.</p>
<script>
fetch('/scan').then(r=>r.json()).then(d=>{
  document.getElementById('status').innerHTML='Available networks:';
  if(!d.length){document.getElementById('list').innerHTML='<div class="hint">No networks found</div>';return;}
  let h='';
  d.forEach(n=>{h+='<div class="ssid-item" onclick="document.getElementById(\'ssid\').value=\''+n.s.replace(/'/g,"")+'\'"><b>'+n.s+'</b> &nbsp;<span style="color:#889">'+n.r+' dBm</span></div>';});
  document.getElementById('list').innerHTML=h;
}).catch(e=>{document.getElementById('status').innerHTML='Scan failed';});
function save(){
  var s=document.getElementById('ssid').value.trim();
  var p=document.getElementById('pass').value;
  if(!s){alert('Please enter SSID');return;}
  document.getElementById('status').innerHTML='Saving...';
  fetch('/save?ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(p))
    .then(r=>r.text()).then(t=>{alert(t);document.getElementById('status').innerHTML='Saved. Rebooting...';})
    .catch(e=>alert('Error: '+e));
}
</script></body></html>)HTML";
    webLog.send(200, "text/html; charset=utf-8", html);
  });

  webLog.on("/scan", HTTP_GET, []() {
    int n = WiFi.scanNetworks(false, false, false, 300);
    String json = "[";
    for (int i = 0; i < n && i < 20; i++) {
      if (i > 0) json += ",";
      String s = WiFi.SSID(i);
      s.replace("\\", "\\\\");
      s.replace("\"", "\\\"");
      json += "{\"s\":\"" + s + "\",\"r\":" + String(WiFi.RSSI(i)) + "}";
    }
    json += "]";
    WiFi.scanDelete();
    webLog.send(200, "application/json", json);
  });

  webLog.on("/save", HTTP_GET, []() {
    String ssid = webLog.arg("ssid");
    String pass = webLog.arg("pass");
    if (ssid.length() == 0) { webLog.send(400, "text/plain", "SSID required"); return; }
    saveWiFi(ssid, pass);
    webLog.send(200, "text/plain", "Saved: " + ssid + ". Rebooting...");
    delay(800);
    ESP.restart();
  });

  webLog.begin();
  Serial.println("[AP] WebServer started on port 80");
}

void showAPModeScreen() {
  tft.fillScreen(0x0000);
  tft.setTextColor(0x07FF, 0x0000);
  tft.setTextSize(2);
  tft.setCursor(20, 30);
  tft.println("WiFi Config Mode");

  tft.setTextColor(0xFFFF, 0x0000);
  tft.setTextSize(1);
  tft.setCursor(15, 70);
  tft.println("1. Connect phone to WiFi:");
  tft.setTextColor(0xFBE0, 0x0000);
  tft.setTextSize(2);
  tft.setCursor(20, 90);
  tft.println("SmartClock-Config");
  tft.setTextColor(0xBDF7, 0x0000);
  tft.setTextSize(1);
  tft.setCursor(15, 115);
  tft.println("   (password: smartclock)");

  tft.setTextColor(0xFFFF);
  tft.setCursor(15, 140);
  tft.println("2. Open browser to:");
  tft.setTextColor(0xFBE0);
  tft.setTextSize(2);
  tft.setCursor(20, 155);
  tft.println("192.168.4.1");

  tft.setTextColor(0x5D9B);
  tft.setTextSize(1);
  tft.setCursor(15, 185);
  tft.println("3. Pick WiFi, enter pass, Save");

  tft.setTextColor(0xF800);
  tft.setCursor(60, 215);
  tft.println(">>> Tap here to skip <<<");
  tft.drawRect(10, 210, 300, 25, 0xF800);
}

// ================================================================
// 時間 / 天氣 同步
// ================================================================
void syncTime() {
  configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  struct tm ti;
  int r = 0;
  while (!getLocalTime(&ti) && r < 20) { delay(500); r++; }
}

void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return;
  String url = "http://api.open-meteo.com/v1/forecast?latitude=" + String(LATITUDE, 4) +
               "&longitude=" + String(LONGITUDE, 4) +
               "&current=temperature_2m,relative_humidity_2m,weather_code&timezone=" + TIMEZONE;
  HTTPClient http;
  http.begin(url);
  if (http.GET() == HTTP_CODE_OK) {
    String p = http.getString();
    JsonDocument doc;
    if (!deserializeJson(doc, p)) {
      JsonObject c = doc["current"];
      weather.temperature = c["temperature_2m"] | 0.0f;
      weather.humidity    = c["relative_humidity_2m"] | 0.0f;
      weather.weatherCode = c["weather_code"] | 0;
      weather.valid = true;
    }
  }
  http.end();
}

String getWeatherDescription(int code) {
  if (code == 0) return "Clear";
  if (code <= 3) return "Cloudy";
  if (code <= 48) return "Foggy";
  if (code <= 67) return "Rain";
  if (code <= 77) return "Snow";
  if (code <= 99) return "Storm";
  return "N/A";
}

// ================================================================
// 主菜單
// ================================================================
void showMainMenu() {
  const int COLS = 4, ITEM_W = 68, ITEM_H = 80, GAP_X = 8, GAP_Y = 15, START_X = 12, START_Y = 50;
  const char* labels[8] = {"Clock", "Scan WiFi", "WiFi Setup", "Enemy", "Fear", "Eco", "Info", "Restart"};
  const uint16_t colors[8] = {0x07E0, 0x07FF, 0xFC9F, 0xF81F, 0x7DFF, 0x07E0, 0xFFE0, 0xF800};
  int selected = 0;
  bool launched = false;

  auto getLabel = [&](int i) -> String {
    if (i == 4) return pigFearMode ? "Fear: ON" : "Fear: OFF";
    if (i == 5) return ecoLabel();
    return String(labels[i]);
  };

  auto drawTile = [&](int i, int x, int y, bool isSelected) {
    uint16_t color = colors[i];
    uint16_t bgCol = isSelected ? color : 0x2124;
    uint16_t fgCol = isSelected ? 0x0000 : color;
    tft.fillRoundRect(x, y, ITEM_W, ITEM_H, 8, bgCol);
    tft.drawRoundRect(x, y, ITEM_W, ITEM_H, 8, isSelected ? 0xFFFF : color);
    if (isSelected) tft.drawRoundRect(x - 1, y - 1, ITEM_W + 2, ITEM_H + 2, 9, 0xFFFF);
    int cx = x + ITEM_W / 2, cy = y + 30;
    if (i == 0) { tft.drawCircle(cx, cy, 14, fgCol); tft.drawLine(cx, cy, cx, cy - 8, fgCol); tft.drawLine(cx, cy, cx + 6, cy + 2, fgCol); tft.fillCircle(cx, cy, 2, fgCol); }
    else if (i == 1) { tft.drawCircle(cx, cy + 8, 4, fgCol); tft.drawCircle(cx, cy + 8, 9, fgCol); tft.drawCircle(cx, cy + 8, 14, fgCol); tft.fillCircle(cx, cy + 8, 2, fgCol); }
    else if (i == 2) { tft.drawCircle(cx, cy, 8, fgCol); tft.fillCircle(cx, cy, 3, fgCol); for (int a = 0; a < 360; a += 45) { float r = a * DEG_TO_RAD; tft.drawLine(cx + 8 * cos(r), cy + 8 * sin(r), cx + 12 * cos(r), cy + 12 * sin(r), fgCol); } }
    else if (i == 3) { tft.fillEllipse(cx, cy + 4, 12, 10, fgCol); tft.fillTriangle(cx - 10, cy - 6, cx - 14, cy - 16, cx - 6, cy - 10, fgCol); tft.fillTriangle(cx + 10, cy - 6, cx + 14, cy - 16, cx + 6, cy - 10, fgCol); tft.fillCircle(cx - 5, cy - 2, 3, isSelected ? 0xFFFF : 0x0000); tft.fillCircle(cx + 5, cy - 2, 3, isSelected ? 0xFFFF : 0x0000); }
    else if (i == 4) { tft.drawCircle(cx, cy, 14, fgCol); tft.fillCircle(cx - 5, cy - 4, 3, fgCol); tft.fillCircle(cx + 5, cy - 4, 3, fgCol); tft.drawCircle(cx, cy + 6, 4, fgCol); }
    else if (i == 5) { tft.fillCircle(cx, cy, 12, fgCol); tft.fillCircle(cx + 6, cy - 6, 10, bgCol); tft.drawLine(cx, cy - 12, cx + 8, cy - 4, fgCol); }
    else if (i == 6) { tft.fillCircle(cx, cy, 14, fgCol); tft.setTextColor(bgCol); tft.setTextSize(3); tft.setCursor(cx - 4, cy - 12); tft.print("i"); }
    else if (i == 7) { tft.drawCircle(cx, cy, 12, fgCol); tft.fillRect(cx - 12, cy - 14, 24, 12, bgCol); tft.drawLine(cx, cy - 14, cx, cy - 6, fgCol); tft.drawLine(cx, cy - 10, cx + 4, cy - 6, fgCol); tft.drawLine(cx, cy - 10, cx - 4, cy - 6, fgCol); }
    String label = getLabel(i);
    tft.setTextColor(fgCol);
    tft.setTextSize(1);
    int textWidth = label.length() * 6;
    tft.setCursor(cx - textWidth / 2, y + ITEM_H - 18);
    tft.print(label);
  };

  auto drawAll = [&]() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(90, 10);
    tft.println("Smart Clock");
    tft.setTextColor(TEXT_DIM, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(90, 30);
    tft.println("Tap to select, tap again");
    for (int i = 0; i < 8; i++) {
      int r = i / COLS, c = i % COLS;
      drawTile(i, START_X + c * (ITEM_W + GAP_X), START_Y + r * (ITEM_H + GAP_Y), i == selected);
    }
    if (WiFi.status() == WL_CONNECTED) {
      String ip = WiFi.localIP().toString();
      tft.setTextColor(0x5D9B); tft.setTextSize(1);
      tft.setCursor((320 - ip.length() * 6) / 2, 228);
      tft.println(ip);
    } else {
      tft.setTextColor(0xF800); tft.setTextSize(1);
      tft.setCursor((320 - 9 * 6) / 2, 228);
      tft.println("No WiFi");
    }
  };

  drawAll();
  unsigned long lastTouch = 0;
  bool lastTouched = false;
  uint16_t tx, ty;

  while (!launched) {
    bool touched = tft.getTouch(&tx, &ty);
    if (!touched && lastTouched && millis() - lastTouch > 200) {
      lastTouch = millis();
      for (int i = 0; i < 8; i++) {
        int x = START_X + (i % COLS) * (ITEM_W + GAP_X);
        int y = START_Y + (i / COLS) * (ITEM_H + GAP_Y);
        if (tx >= x && tx <= x + ITEM_W && ty >= y && ty <= y + ITEM_H) {
          if (i == selected) {
            drawTile(i, x, y, true);
            delay(150);
            switch (i) {
              case 0: launched = true; break;
              case 1: showWifiScanner(); drawAll(); break;
              case 2: showWifiScanner(); drawAll(); break;  // ★ WiFi Setup 也進掃描器
              case 3: showEnemyMenu(); drawAll(); break;
              case 4:
                pigFearMode = !pigFearMode;
                prefs.begin("clock", false);
                prefs.putBool("pigFear", pigFearMode);
                prefs.end();
                drawAll();
                break;
              case 5: cycleEcoSetting(); drawAll(); break;
              case 6: showInfoScreen(); drawAll(); break;
              case 7:
                tft.fillScreen(TFT_BLACK);
                tft.setTextColor(TFT_RED);
                tft.setCursor(80, 100);
                tft.println("Restarting...");
                delay(1000);
                ESP.restart();
                break;
            }
          } else {
            selected = i;
            drawAll();
          }
          break;
        }
      }
    }
    if (touched) lastTouched = true; else lastTouched = false;
    delay(20);
  }
  tft.fillScreen(TFT_BLACK);
}