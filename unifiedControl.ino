#include <FastLED.h>

/*
  MERGED Candle Controller
  - 4 strip candles + 4 ring candles
  - Physical pairing requested by user:
      Candle 1: strip pin 5  + ring pin 9
      Candle 2: strip pin 6  + ring pin 10
      Candle 3: strip pin 7  + ring pin 11
      Candle 4: strip pin 8  + ring pin 12

  Serial command families kept/merged:
  Original V4 strip commands:
    gN              global strip social state 1..5, also maps paired rings
    xCN             strip/candle C social state N
    yCN             strip/candle C talking state N; rings stay off except warning/listening states
    bCV             strip/candle C brightness V 0..255
    cCN             strip/candle C manual color preset N
    eCN             strip/candle C strip effect 0 static / 1 flicker
    kCrrggbbrrggbb  strip/candle C custom gradient
    tCseconds       tap animation for candle C
    sAB             share/copy candle A to candle B
    z1011           group tap/sync animation mask

  Merged ring/flow commands:
    rCN             ring/candle C state N (0 off, 1..5 flow states)
    qCV             ring/candle C brightness V 0..255
    wCN             ring/candle C effect 0 static / 1 flicker
    oC              blow out whole candle C (strip + ring)

  Backward compatibility for the second HTML:
    TCN or TN       strip talking state for candle C, or candle 1 if no C
    SCN or SN       ring state for candle C, or candle 1 if no C
    BCV or BV       ring brightness for candle C, or all rings if no C
    ECN or EN       ring effect for candle C, or all rings if no C
    X or XC         blow out all candles, or candle C
*/

#define STRIP_PIN_1 5
#define STRIP_PIN_2 6
#define STRIP_PIN_3 7
#define STRIP_PIN_4 8

#define RING_PIN_1 9
#define RING_PIN_2 10
#define RING_PIN_3 11
#define RING_PIN_4 12

#define NUM_STRIP_LEDS 10   // Set to 6 if your strip pieces have 6 LEDs each.
#define NUM_RING_LEDS 16
#define CANDLE_COUNT 4

CRGB stripLeds[CANDLE_COUNT][NUM_STRIP_LEDS];
CRGB ringLeds[CANDLE_COUNT][NUM_RING_LEDS];

int stripBrightness[CANDLE_COUNT]       = {200, 200, 200, 200};
int targetStripBrightness[CANDLE_COUNT] = {200, 200, 200, 200};
uint8_t ringBrightness[CANDLE_COUNT]    = {150, 150, 150, 150};

int colorMode[CANDLE_COUNT]    = {0, 0, 0, 0};
int stripEffect[CANDLE_COUNT]  = {1, 1, 1, 1};
int socialState[CANDLE_COUNT]  = {0, 0, 0, 0};
int talkingState[CANDLE_COUNT] = {0, 0, 0, 0};
uint8_t ringState[CANDLE_COUNT] = {0, 0, 0, 0};
uint8_t ringEffect[CANDLE_COUNT] = {1, 1, 1, 1};

CRGBPalette16 currentPalette[CANDLE_COUNT];
CRGBPalette16 targetPalette[CANDLE_COUNT];
uint8_t gHue = 0;

CRGB currentTalkA[CANDLE_COUNT];
CRGB currentTalkB[CANDLE_COUNT];

CRGB customColorA[CANDLE_COUNT] = {
  CRGB(255, 50, 0), CRGB(255, 50, 0), CRGB(255, 50, 0), CRGB(255, 50, 0)
};
CRGB customColorB[CANDLE_COUNT] = {
  CRGB(200, 0, 0), CRGB(200, 0, 0), CRGB(200, 0, 0), CRGB(200, 0, 0)
};

bool tapActive = false;
unsigned long tapTimer = 0;
int tappedCandle = -1;
uint8_t tapMix = 0;
uint8_t targetTapMix = 0;
unsigned long tapDurationMs = 2000;

bool isCelebrating[CANDLE_COUNT] = {false, false, false, false};
unsigned long celebrationStartTime[CANDLE_COUNT] = {0, 0, 0, 0};
unsigned long celebrationEndTime[CANDLE_COUNT] = {0, 0, 0, 0};

const unsigned long SHARE_FEEDBACK_MS = 1200;  // Two soft pulses before the copied flame is applied.
bool sharePending = false;
unsigned long shareApplyTime = 0;
int pendingShareTarget = -1;
int pendingShareStripBrightness = 200;
int pendingShareColorMode = 0;
int pendingShareStripEffect = 1;
int pendingShareSocialState = 0;
int pendingShareTalkingState = 0;
CRGB pendingShareCustomColorA = CRGB(255, 50, 0);
CRGB pendingShareCustomColorB = CRGB(200, 0, 0);
uint8_t pendingShareRingState = 0;
uint8_t pendingShareRingBrightness = 150;
uint8_t pendingShareRingEffect = 1;

CRGBPalette16 candlePalette = CRGBPalette16(
  CRGB(200, 30, 0),
  CRGB(255, 80, 0),
  CRGB(255, 100, 0),
  CRGB(255, 120, 0)
);

CRGBPalette16 ragingFirePalette = CRGBPalette16(
  CRGB::DarkRed,
  CRGB::Red,
  CRGB(255, 50, 0),
  CRGB(255, 100, 0)
);

void setup() {
  Serial.begin(9600);

  FastLED.addLeds<WS2812B, STRIP_PIN_1, GRB>(stripLeds[0], NUM_STRIP_LEDS);
  FastLED.addLeds<WS2812B, STRIP_PIN_2, GRB>(stripLeds[1], NUM_STRIP_LEDS);
  FastLED.addLeds<WS2812B, STRIP_PIN_3, GRB>(stripLeds[2], NUM_STRIP_LEDS);
  FastLED.addLeds<WS2812B, STRIP_PIN_4, GRB>(stripLeds[3], NUM_STRIP_LEDS);

  FastLED.addLeds<WS2812B, RING_PIN_1, GRB>(ringLeds[0], NUM_RING_LEDS);
  FastLED.addLeds<WS2812B, RING_PIN_2, GRB>(ringLeds[1], NUM_RING_LEDS);
  FastLED.addLeds<WS2812B, RING_PIN_3, GRB>(ringLeds[2], NUM_RING_LEDS);
  FastLED.addLeds<WS2812B, RING_PIN_4, GRB>(ringLeds[3], NUM_RING_LEDS);

  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(255);

  for (int i = 0; i < CANDLE_COUNT; i++) {
    currentPalette[i] = CRGBPalette16(CRGB(255, 15, 0));
    targetPalette[i]  = CRGBPalette16(CRGB(255, 15, 0));
    currentTalkA[i]   = CRGB(0, 0, 0);
    currentTalkB[i]   = CRGB(0, 0, 0);
  }
}

void approachColor(CRGB& current, const CRGB& target, uint8_t speed) {
  if (current.r < target.r) current.r = (current.r + speed > target.r) ? target.r : current.r + speed;
  else if (current.r > target.r) current.r = (current.r - speed < target.r) ? target.r : current.r - speed;

  if (current.g < target.g) current.g = (current.g + speed > target.g) ? target.g : current.g + speed;
  else if (current.g > target.g) current.g = (current.g - speed < target.g) ? target.g : current.g - speed;

  if (current.b < target.b) current.b = (current.b + speed > target.b) ? target.b : current.b + speed;
  else if (current.b > target.b) current.b = (current.b - speed < target.b) ? target.b : current.b - speed;
}

CRGB getTalkingColor(uint8_t state) {
  switch (state) {
    case 1: return CRGB(255, 150, 30);  // No Talking
    case 2: return CRGB(255, 130, 15);  // Talking Less
    case 3: return CRGB(255, 100, 10);  // Talking Normal
    case 4: return CRGB(255, 30, 0);    // Talking Too Much
    case 5: return CRGB(255, 80, 0);    // Listening ring base
    default: return CRGB::Black;
  }
}

CRGB getListeningStripColor() {
  // Listening strip base now matches the ring Listening base color.
  return getTalkingColor(5);
}

void setPairedTalkingState(int candle, int val) {
  if (candle < 0 || candle >= CANDLE_COUNT) return;

  if (val >= 1 && val <= 4) {
    if (talkingState[candle] == 0) {
      currentTalkA[candle] = stripLeds[candle][0];
      currentTalkB[candle] = stripLeds[candle][NUM_STRIP_LEDS - 1];
    }
    talkingState[candle] = val;
    socialState[candle] = 0;

    // New behaviour: the ring is normally off. It only stays active for the
    // two edge states that need visible feedback: No Talking and Too Much.
    if (val == 1 || val == 4) ringState[candle] = val;
    else ringState[candle] = 0;
  } else if (val == 5) {
    // Listening: strip uses its listening color; ring can keep its listening spark.
    socialState[candle] = 5;
    talkingState[candle] = 0;
    ringState[candle] = 5;
  }
}

void blowOutCandle(int candle) {
  if (candle < 0 || candle >= CANDLE_COUNT) return;
  socialState[candle] = 5;
  talkingState[candle] = 0;
  targetStripBrightness[candle] = 0;
  ringState[candle] = 0;
}

void startShareFeedback(int from, int to) {
  unsigned long now = millis();
  unsigned long endTime = now + SHARE_FEEDBACK_MS;

  // Snapshot the sender now. The receiver keeps its own current color during
  // the two-pulse feedback, then this snapshot is applied at the end.
  pendingShareTarget = to;
  pendingShareStripBrightness = targetStripBrightness[from];
  pendingShareColorMode = colorMode[from];
  pendingShareStripEffect = stripEffect[from];
  pendingShareSocialState = socialState[from];
  pendingShareTalkingState = talkingState[from];
  pendingShareCustomColorA = customColorA[from];
  pendingShareCustomColorB = customColorB[from];
  pendingShareRingState = ringState[from];
  pendingShareRingBrightness = ringBrightness[from];
  pendingShareRingEffect = ringEffect[from];
  shareApplyTime = endTime;
  sharePending = true;

  celebrationStartTime[from] = now;
  celebrationStartTime[to] = now;
  celebrationEndTime[from] = endTime;
  celebrationEndTime[to] = endTime;
  isCelebrating[from] = true;
  isCelebrating[to] = true;
}

void applyPendingShareIfReady() {
  if (!sharePending || millis() < shareApplyTime) return;
  int targetCandle = pendingShareTarget;
  sharePending = false;
  pendingShareTarget = -1;

  if (targetCandle < 0 || targetCandle >= CANDLE_COUNT) return;
  targetStripBrightness[targetCandle] = pendingShareStripBrightness;
  colorMode[targetCandle] = pendingShareColorMode;
  stripEffect[targetCandle] = pendingShareStripEffect;
  socialState[targetCandle] = pendingShareSocialState;
  talkingState[targetCandle] = pendingShareTalkingState;
  customColorA[targetCandle] = pendingShareCustomColorA;
  customColorB[targetCandle] = pendingShareCustomColorB;
  ringState[targetCandle] = pendingShareRingState;
  ringBrightness[targetCandle] = pendingShareRingBrightness;
  ringEffect[targetCandle] = pendingShareRingEffect;
}

uint8_t getSharePulseScale(int candle) {
  if (!isCelebrating[candle]) return 255;

  unsigned long now = millis();
  if (now > celebrationEndTime[candle]) {
    isCelebrating[candle] = false;
    return 255;
  }

  unsigned long duration = celebrationEndTime[candle] - celebrationStartTime[candle];
  if (duration == 0) return 255;

  unsigned long elapsed = now - celebrationStartTime[candle];
  // 0..255 twice across the feedback window = two soft pulses.
  uint8_t phase = (uint8_t)((elapsed * 512UL / duration) & 0xFF);
  uint8_t wave = sin8(phase);
  return map(wave, 0, 255, 145, 255);
}

void playSyncAnimation(bool participating[CANDLE_COUNT], int peopleCount) {
  if (peopleCount < 2) return;

  unsigned long duration = (peopleCount >= 4) ? 5000 : 4000;
  int numColors = (peopleCount == 2) ? 1 : (peopleCount == 3 ? 2 : 3);

  CRGB palette[3] = {
    CRGB(255, 140, 0),
    CRGB(255, 170, 10),
    CRGB(255, 60, 0)
  };

  // Group tap/sync is now a strip-only feedback animation.
  // Ring LEDs are forced off during the entire simulation so the visual
  // feedback stays only on the candle bodies/strip LEDs.
  CRGB currentColors[NUM_STRIP_LEDS];
  CRGB targetColors[NUM_STRIP_LEDS];

  for (int j = 0; j < NUM_STRIP_LEDS; j++) {
    CRGB c = palette[random8(numColors)];
    currentColors[j] = c;
    targetColors[j] = c;
  }

  unsigned long startTime = millis();
  unsigned long lastColorChange = millis();

  while (millis() - startTime < duration) {
    if (millis() - lastColorChange > 30) {
      lastColorChange = millis();
      for (int changes = 0; changes < 2; changes++) {
        int randomLed = random8(NUM_STRIP_LEDS);
        targetColors[randomLed] = palette[random8(numColors)];
      }
    }

    for (int j = 0; j < NUM_STRIP_LEDS; j++) {
      nblend(currentColors[j], targetColors[j], 55);
    }

    uint8_t globalPulse = beatsin8(30, 90, 255);

    for (int i = 0; i < CANDLE_COUNT; i++) {
      if (participating[i]) {
        for (int j = 0; j < NUM_STRIP_LEDS; j++) {
          CRGB c = currentColors[j];
          c.nscale8(globalPulse);
          stripLeds[i][j] = c;
        }
      } else {
        fill_solid(stripLeds[i], NUM_STRIP_LEDS, CRGB(15, 5, 0));
      }

      fill_solid(ringLeds[i], NUM_RING_LEDS, CRGB::Black);
    }

    FastLED.show();
    delay(15);
  }

  for (int f = 0; f < 30; f++) {
    for (int i = 0; i < CANDLE_COUNT; i++) {
      if (participating[i]) {
        for (int j = 0; j < NUM_STRIP_LEDS; j++) stripLeds[i][j].fadeToBlackBy(8);
      }
      fill_solid(ringLeds[i], NUM_RING_LEDS, CRGB::Black);
    }
    FastLED.show();
    delay(15);
  }

  for (int i = 0; i < CANDLE_COUNT; i++) {
    fill_solid(ringLeds[i], NUM_RING_LEDS, CRGB::Black);
  }
}

void handleLegacyUppercase(String cmd) {
  char type = cmd.charAt(0);

  if (type == 'X') {
    if (cmd.length() >= 2 && isDigit(cmd.charAt(1))) {
      blowOutCandle(cmd.substring(1, 2).toInt() - 1);
    } else {
      for (int i = 0; i < CANDLE_COUNT; i++) blowOutCandle(i);
    }
    return;
  }

  // Old second-HTML commands used T3, S3, B150, E1.
  // B/E stay global for that old page. New per-candle ring commands use lowercase q/w.
  if (type == 'B') {
    int val = cmd.substring(1).toInt();
    for (int i = 0; i < CANDLE_COUNT; i++) ringBrightness[i] = constrain(val, 0, 255);
    return;
  }
  if (type == 'E') {
    int val = cmd.substring(1).toInt();
    for (int i = 0; i < CANDLE_COUNT; i++) ringEffect[i] = constrain(val, 0, 1);
    return;
  }

  int candle = 0;
  int valueStart = 1;

  // Supports both old format: S3 and merged format: S13 (ring state 3 for candle 1).
  if (cmd.length() >= 3 && isDigit(cmd.charAt(1))) {
    int possibleCandle = cmd.substring(1, 2).toInt() - 1;
    if (possibleCandle >= 0 && possibleCandle < CANDLE_COUNT) {
      candle = possibleCandle;
      valueStart = 2;
    }
  }

  int val = cmd.substring(valueStart).toInt();

  if (type == 'T') setPairedTalkingState(candle, val);
  else if (type == 'S') {
    if (candle >= 0 && candle < CANDLE_COUNT) ringState[candle] = constrain(val, 0, 5);
  }
}

void handleCommand(String cmd) {
  cmd.trim();
  if (cmd.length() < 1) return;

  char type = cmd.charAt(0);

  if (type == 'T' || type == 'S' || type == 'B' || type == 'E' || type == 'X') {
    handleLegacyUppercase(cmd);
    return;
  }

  if (type == 'g') {
    int val = cmd.substring(1).toInt();
    for (int i = 0; i < CANDLE_COUNT; i++) {
      socialState[i] = val;
      talkingState[i] = 0;
      ringState[i] = (val == 1 || val == 4) ? val : 0;
    }
    return;
  }

  if (type == 'z' && cmd.length() >= 5) {
    bool tapped[CANDLE_COUNT];
    int count = 0;
    for (int i = 0; i < CANDLE_COUNT; i++) {
      tapped[i] = (cmd.charAt(i + 1) == '1');
      if (tapped[i]) count++;
    }
    if (count > 1) playSyncAnimation(tapped, count);
    return;
  }

  if (type == 't') {
    tappedCandle = cmd.substring(1, 2).toInt() - 1;
    if (tappedCandle >= 0 && tappedCandle < CANDLE_COUNT) {
      tapActive = true;
      tapTimer = millis();
      tapDurationMs = (cmd.length() > 2) ? cmd.substring(2).toInt() * 1000UL : 2000UL;
      if (tapDurationMs < 250) tapDurationMs = 2000UL;
    }
    return;
  }

  if (type == 'o') {
    int candle = cmd.substring(1, 2).toInt() - 1;
    blowOutCandle(candle);
    return;
  }

  if (cmd.length() < 3) return;
  int candle = cmd.substring(1, 2).toInt() - 1;
  if (candle < 0 || candle >= CANDLE_COUNT) return;

  if (type == 'x') {
    int val = cmd.substring(2).toInt();
    socialState[candle] = val;
    talkingState[candle] = 0;
    ringState[candle] = (val == 1 || val == 4) ? val : 0;
  }
  else if (type == 'y') {
    int val = cmd.substring(2).toInt();
    setPairedTalkingState(candle, val);
  }
  else if (type == 'b') {
    targetStripBrightness[candle] = constrain(cmd.substring(2).toInt(), 0, 255);
  }
  else if (type == 'c') {
    colorMode[candle] = cmd.substring(2).toInt();
    socialState[candle] = 0;
    talkingState[candle] = 0;
  }
  else if (type == 'e') {
    stripEffect[candle] = constrain(cmd.substring(2).toInt(), 0, 1);
  }
  else if (type == 'k' && cmd.length() >= 14) {
    long rgb1 = strtol(cmd.substring(2, 8).c_str(), NULL, 16);
    long rgb2 = strtol(cmd.substring(8, 14).c_str(), NULL, 16);

    customColorA[candle] = CRGB((rgb1 >> 16) & 0xFF, (rgb1 >> 8) & 0xFF, rgb1 & 0xFF);
    customColorB[candle] = CRGB((rgb2 >> 16) & 0xFF, (rgb2 >> 8) & 0xFF, rgb2 & 0xFF);

    colorMode[candle] = 10;
    socialState[candle] = 0;
    talkingState[candle] = 0;
  }
  else if (type == 's') {
    int targetCandle = cmd.substring(2).toInt() - 1;
    if (targetCandle >= 0 && targetCandle < CANDLE_COUNT && targetCandle != candle) {
      startShareFeedback(candle, targetCandle);
    }
  }
  else if (type == 'r') {
    ringState[candle] = constrain(cmd.substring(2).toInt(), 0, 5);
  }
  else if (type == 'q') {
    ringBrightness[candle] = constrain(cmd.substring(2).toInt(), 0, 255);
  }
  else if (type == 'w') {
    ringEffect[candle] = constrain(cmd.substring(2).toInt(), 0, 1);
  }
}

void updateTargetPalettes() {
  for (int i = 0; i < CANDLE_COUNT; i++) {
    if (socialState[i] > 0) {
      switch (socialState[i]) {
        case 1: targetPalette[i] = CRGBPalette16(CRGB(220, 40, 0), CRGB(120, 10, 0), CRGB(200, 30, 0), CRGB(80, 5, 0)); break;
        case 2: targetPalette[i] = CRGBPalette16(CRGB(255, 120, 10), CRGB(200, 80, 5), CRGB(255, 100, 0), CRGB(180, 60, 0)); break;
        case 3: targetPalette[i] = CRGBPalette16(CRGB(255, 180, 40), CRGB(255, 100, 0), CRGB(255, 200, 50), CRGB(200, 80, 0)); break;
        case 4: targetPalette[i] = CRGBPalette16(CRGB(255, 20, 0), CRGB(100, 0, 0), CRGB(200, 10, 0), CRGB(50, 0, 0)); break;
        case 5: targetPalette[i] = CRGBPalette16(candlePalette); break;
      }
    } else {
      switch (colorMode[i]) {
        case 0: targetPalette[i] = CRGBPalette16(CRGB(255, 130, 15)); break;
        case 1: targetPalette[i] = CRGBPalette16(CRGB(255, 130, 15), CRGB(255, 100, 10), CRGB(255, 130, 15), CRGB(255, 100, 10)); break;
        case 2: targetPalette[i] = CRGBPalette16(CRGB(255, 100, 10), CRGB(255, 80, 5), CRGB(255, 100, 10), CRGB(255, 80, 5)); break;
        case 3: targetPalette[i] = CRGBPalette16(CRGB(255, 80, 5), CRGB(255, 60, 0), CRGB(255, 80, 5), CRGB(255, 60, 0)); break;
        case 4: targetPalette[i] = CRGBPalette16(CRGB(255, 60, 0), CRGB(255, 40, 0), CRGB(255, 60, 0), CRGB(255, 40, 0)); break;
        case 5: targetPalette[i] = CRGBPalette16(CRGB(255, 40, 0), CRGB(255, 25, 0), CRGB(255, 40, 0), CRGB(255, 25, 0)); break;
        case 6: targetPalette[i] = CRGBPalette16(CRGB(255, 25, 0), CRGB(255, 15, 0), CRGB(255, 25, 0), CRGB(255, 15, 0)); break;
        case 7: targetPalette[i] = CRGBPalette16(CRGB(255, 45, 0), CRGB(255, 90, 0), CRGB(80, 10, 0), CRGB(255, 60, 0)); break;
        case 8: targetPalette[i] = CRGBPalette16(CRGB(150, 20, 0), CRGB(220, 40, 0), CRGB(50, 5, 0), CRGB(180, 25, 0)); break;
        case 9: targetPalette[i] = CRGBPalette16(CRGB(255, 100, 0), CRGB(10, 0, 0), CRGB(255, 40, 0), CRGB(10, 0, 0)); break;
        case 10: targetPalette[i] = CRGBPalette16(customColorA[i], customColorB[i], customColorA[i], customColorB[i]); break;
      }
    }
  }
}

void updateStripAnimation() {
  updateTargetPalettes();

  if (tapActive && (millis() - tapTimer > tapDurationMs)) tapActive = false;
  targetTapMix = tapActive ? 255 : 0;

  EVERY_N_MILLISECONDS(15) {
    if (tapMix < targetTapMix) tapMix = qadd8(tapMix, 15);
    else if (tapMix > targetTapMix) tapMix = qsub8(tapMix, 10);

    for (int i = 0; i < CANDLE_COUNT; i++) {
      nblendPaletteTowardPalette(currentPalette[i], targetPalette[i], 16);

      int step = (abs(targetStripBrightness[i] - stripBrightness[i]) > 100) ? 35 : 2;
      if (stripBrightness[i] < targetStripBrightness[i]) {
        stripBrightness[i] += step;
        if (stripBrightness[i] > targetStripBrightness[i]) stripBrightness[i] = targetStripBrightness[i];
      } else if (stripBrightness[i] > targetStripBrightness[i]) {
        stripBrightness[i] -= step;
        if (stripBrightness[i] < targetStripBrightness[i]) stripBrightness[i] = targetStripBrightness[i];
      }

      if (talkingState[i] > 0) {
        CRGB targetA, targetB;
        switch (talkingState[i]) {
          case 1: {
            CRGB noTalkingColor = getTalkingColor(1);
            targetA = noTalkingColor;
            targetB = noTalkingColor;
            break;
          }
          case 2: targetA = CRGB(255, 170, 0);  targetB = CRGB(255, 150, 0);  break;
          case 3: targetA = CRGB(255, 70, 0);  targetB = CRGB(255, 50, 0);  break;
          case 4: targetA = CRGB(255, 20, 0);   targetB = CRGB(255, 20, 0);   break;
          default: targetA = CRGB::Black; targetB = CRGB::Black; break;
        }
        approachColor(currentTalkA[i], targetA, 2);
        approachColor(currentTalkB[i], targetB, 2);
      }
    }
  }

  EVERY_N_MILLISECONDS(30) { gHue++; }

  for (int i = 0; i < CANDLE_COUNT; i++) {
    int finalBrt = stripBrightness[i];
    uint8_t speed = 10;

    if (socialState[i] > 0) {
      switch (socialState[i]) {
        case 1: finalBrt = scale8(stripBrightness[i], beatsin8(15, 128, 255)); speed = 5; break;
        case 2: finalBrt = scale8(stripBrightness[i], beatsin8(20, 180, 255, 0, i * 64)); speed = 10; break;
        case 3: finalBrt = scale8(stripBrightness[i], beatsin8(45, 180, 255)); speed = 25; break;
        case 4: finalBrt = (random8() > 180) ? random8(64, max(stripBrightness[i], 65)) : stripBrightness[i]; speed = 40; break;
        case 5: finalBrt = stripBrightness[i]; speed = 0; break;
      }
    }

    if (talkingState[i] > 0) {
      for (int j = 0; j < NUM_STRIP_LEDS; j++) {
        stripLeds[i][j] = (j < NUM_STRIP_LEDS / 2) ? currentTalkA[i] : currentTalkB[i];
      }
    } else {
      for (int j = 0; j < NUM_STRIP_LEDS; j++) {
        uint8_t colorIndex = (j * 50);
        if (socialState[i] > 0) {
          if (socialState[i] != 5) colorIndex += (millis() * speed / 100);
        } else {
          if (colorMode[i] == 7 || colorMode[i] == 8 || colorMode[i] == 10) colorIndex += gHue;
          else if (colorMode[i] == 9) colorIndex += (gHue * 5);
        }
        stripLeds[i][j] = ColorFromPalette(currentPalette[i], colorIndex, 255, LINEARBLEND);
      }
    }

    if (tapMix > 0) {
      uint8_t tapTarget = (i == tappedCandle) ? 255 : 25;
      finalBrt = blend8(finalBrt, tapTarget, tapMix);
    }

    if (stripEffect[i] == 1 && !tapActive && socialState[i] != 4) {
      for (int j = 0; j < NUM_STRIP_LEDS; j++) stripLeds[i][j].fadeLightBy(random8(75));
    }

    uint8_t sharePulse = getSharePulseScale(i);
    finalBrt = scale8(finalBrt, sharePulse);

    for (int j = 0; j < NUM_STRIP_LEDS; j++) stripLeds[i][j].nscale8(finalBrt);
  }
}

void updateRingAnimation() {
  for (int c = 0; c < CANDLE_COUNT; c++) {
    uint8_t state = ringState[c];
    uint8_t brt = ringBrightness[c];

    if (state == 0) {
      // Fade out instead of snapping off.
      for (int i = 0; i < NUM_RING_LEDS; i++) ringLeds[c][i].fadeToBlackBy(28);
    }
    else if (state == 1) {
      CRGB ringColor = getTalkingColor(1);
      uint8_t minBrt = (brt * 30) / 100;
      uint8_t breath = beatsin8(20, minBrt, brt);
      ringColor.nscale8(breath);
      fill_solid(ringLeds[c], NUM_RING_LEDS, ringColor);
    }
    else if (state == 2 || state == 3) {
      CRGB ringColor = getTalkingColor(state);
      ringColor.nscale8(brt);
      fill_solid(ringLeds[c], NUM_RING_LEDS, ringColor);
    }
    else if (state == 4) {
      // Talking Too Much uses the same smooth breathing animation as No Talking,
      // but with the Talking Too Much color. This removes the aggressive/fire flicker
      // while keeping the 5-second warning flow handled from the HTML side.
      CRGB ringColor = getTalkingColor(4);
      uint8_t minBrt = (brt * 30) / 100;
      uint8_t breath = beatsin8(20, minBrt, brt);
      ringColor.nscale8(breath);
      fill_solid(ringLeds[c], NUM_RING_LEDS, ringColor);
    }
    else if (state == 5) {
      for (int i = 0; i < NUM_RING_LEDS; i++) {
        uint16_t noiseSpace = i * 857 + c * 311;
        uint8_t noiseVal = inoise8(noiseSpace, millis() / 5);
        uint8_t colorIndex = map(noiseVal, 0, 255, 0, 255);
        uint8_t pixelBrt = map(noiseVal, 0, 255, brt / 2, brt);
        ringLeds[c][i] = ColorFromPalette(candlePalette, colorIndex, pixelBrt, LINEARBLEND);
        if (ringEffect[c] == 1 && random8() < 25) ringLeds[c][i].fadeToBlackBy(random8(80, 160));
      }
    }

    if (tapMix > 0) {
      uint8_t tapTarget = (c == tappedCandle) ? 255 : 25;
      uint8_t mixedBrt = blend8(brt, tapTarget, tapMix);
      for (int i = 0; i < NUM_RING_LEDS; i++) ringLeds[c][i].nscale8(mixedBrt);
    }

    uint8_t sharePulse = getSharePulseScale(c);
    if (sharePulse < 255) {
      for (int i = 0; i < NUM_RING_LEDS; i++) ringLeds[c][i].nscale8(sharePulse);
    }
  }
}

void loop() {
  while (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    handleCommand(cmd);
  }

  applyPendingShareIfReady();

  updateStripAnimation();
  updateRingAnimation();

  FastLED.show();
  delay(20);
}
