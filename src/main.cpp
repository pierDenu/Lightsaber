#define STRIP_PIN 6
#define NUMLEDS 87
#define SD_ChipSelectPin 10
#define BTN_PIN 3
#define SPEAKER_PIN 9

#include <microLED.h>
#include <SD.h>
#include <TMRpcm.h>
#include <SPI.h>
#include <Wire.h>
#include <MPU6050.h>
#include <I2Cdev.h>

MPU6050 mpu;
TMRpcm tmrpcm;
microLED<NUMLEDS, STRIP_PIN, MLED_NO_CLOCK, LED_WS2812, ORDER_GRB, CLI_HIGH> strip;

mData colours[5] = {mRed, mYellow, mBlue, mPurple, mGreen};

int swing_time_L[4]   = {486, 541, 622, 652};
int strike_time[8]    = {879, 663, 787, 802, 773, 761, 766, 735};
int strike_s_time[8]  = {355, 267, 286, 350, 352, 355, 350, 338};

struct Motion {
  int16_t ax, ay, az;
  int16_t gx, gy, gz;
  uint16_t ACC;
  uint16_t GYR;
};

byte colourIndex = 0;
byte humActive = 0;

unsigned long lastActionTime = 0;
unsigned long humTime = 0;
unsigned long btnTime = 0;

int timeoutMs = 0;

char BUFFER[12];

static inline bool elapsed(unsigned long since, unsigned long ms) {
  return (millis() - since) > ms;
}

void buildName(const char* prefix, int idx1based) {
  snprintf(BUFFER, sizeof(BUFFER), "%s%d.wav", prefix, idx1based);
}

void playFile(const char* name) {
  tmrpcm.play(name);
}

void stopAndPlayFile(const char* name) {
  tmrpcm.stopPlayback();
  tmrpcm.play(name);
}

void playIndexed(const char* prefix, int idx1based) {
  buildName(prefix, idx1based);
  tmrpcm.play(BUFFER);
}

void stopAndPlayIndexed(const char* prefix, int idx1based) {
  buildName(prefix, idx1based);
  tmrpcm.stopPlayback();
  tmrpcm.play(BUFFER);
}

void setBladeColor(byte idx) {
  strip.fill(colours[idx]);
  strip.show();
}

void flashWhite(uint16_t ms) {
  strip.fill(mWhite);
  strip.show();
  delay(ms);
  setBladeColor(colourIndex);
}

Motion readMotion() {
  Motion m{};
  mpu.getMotion6(&m.ax, &m.ay, &m.az, &m.gx, &m.gy, &m.gz);

  m.gx = abs(m.gx / 100);
  m.gy = abs(m.gy / 100);
  m.gz = abs(m.gz / 100);

  m.ax = abs(m.ax / 100);
  m.ay = abs(m.ay / 100);
  m.az = abs(m.az / 100);

  long accSq = (long)m.ax * m.ax + (long)m.ay * m.ay + (long)m.az * m.az;
  long gyrSq = (long)m.gx * m.gx + (long)m.gy * m.gy + (long)m.gz * m.gz;

  m.ACC = (uint16_t)sqrt((double)accSq);
  m.GYR = (uint16_t)sqrt((double)gyrSq);

  return m;
}

void printDebug(const Motion& m) {
  Serial.print(m.ACC);
  Serial.print("      ");
  Serial.println(m.GYR);
}

void handleSwing(const Motion& m) {
  if (!elapsed(lastActionTime, (unsigned long)timeoutMs)) return;

  if (m.GYR > 80 && m.GYR < 230) {
    int idx = random(1, 5);
    timeoutMs = swing_time_L[idx - 1];
    tmrpcm.stopPlayback();
    playIndexed("SWL", idx);
    lastActionTime = millis();
    humActive = 0;
    return;
  }

  if (m.GYR > 230) {
    int idx = random(1, 5);
    timeoutMs = swing_time_L[idx - 1];
    tmrpcm.stopPlayback();
    playIndexed("SWL", idx);
    lastActionTime = millis();
    humActive = 0;
    return;
  }
}

void handleStrike(const Motion& m) {
  if (m.ACC > 60 && m.ACC < 150) {
    int idx = random(1, 9);
    lastActionTime = millis();
    flashWhite(50);
    timeoutMs = strike_s_time[idx - 1];
    tmrpcm.stopPlayback();
    playIndexed("SKS", idx);
    humActive = 0;
    return;
  }

  if (m.ACC >= 150) {
    int idx = random(1, 9);
    lastActionTime = millis();
    flashWhite(100);
    timeoutMs = strike_time[idx - 1];
    tmrpcm.stopPlayback();
    playIndexed("SK", idx);
    humActive = 0;
    return;
  }
}

void startHumIfIdle() {
  if (humActive == 0 && elapsed(lastActionTime, (unsigned long)timeoutMs)) {
    tmrpcm.stopPlayback();
    humActive = 1;
    humTime = millis();
    tmrpcm.play("HUM.wav", 4);
  }
}

void keepHumAlive() {
  if (humActive == 1 && elapsed(humTime, 5000)) {
    tmrpcm.stopPlayback();
    humTime = millis();
    tmrpcm.play("HUM.wav", 4);
  }
}

void handleColorButton() {
  if (digitalRead(BTN_PIN) == LOW && elapsed(btnTime, 300)) {
    colourIndex++;
    if (colourIndex > 4) colourIndex = 0;
    setBladeColor(colourIndex);
    btnTime = millis();
  }
}

void bootAnimation() {
  strip.clear();
  strip.show();
  strip.setBrightness(255);

  for (int i = 0; i < 46; i++) {
    strip.leds[i] = colours[0];
    strip.leds[NUMLEDS - i - 1] = colours[0];
    strip.show();
    delay(20);
  }

  playFile("ON.wav");
  delay(1100);
  setBladeColor(colourIndex);
}

void initHardware() {
  pinMode(BTN_PIN, INPUT_PULLUP);

  Wire.begin();

  tmrpcm.speakerPin = SPEAKER_PIN;
  tmrpcm.setVolume(5);

  Serial.begin(9600);

  SD.begin(SD_ChipSelectPin);

  mpu.initialize();
  mpu.setFullScaleAccelRange(MPU6050_ACCEL_FS_8);
}

void setup() {
  initHardware();
  bootAnimation();
}

void loop() {
  Motion m = readMotion();
  printDebug(m);

  handleSwing(m);
  handleStrike(m);

  startHumIfIdle();
  keepHumAlive();

  handleColorButton();
}