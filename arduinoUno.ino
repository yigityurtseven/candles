#include <Adafruit_NeoPixel.h>

#define PIN1 8
#define PIN2 7
#define PIN3 6
#define PIN4 5
#define NUM_LEDS 5

// Array of strips for easier management
Adafruit_NeoPixel strips[4] = {
  Adafruit_NeoPixel(NUM_LEDS, PIN1, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, PIN2, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, PIN3, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(NUM_LEDS, PIN4, NEO_GRB + NEO_KHZ800)
};

// --- SMOOTHING STATE VARIABLES ---
float currentR[4] = {0, 0, 0, 0};
float currentG[4] = {0, 0, 0, 0};
float currentB[4] = {0, 0, 0, 0};

int targetR[4] = {255, 0, 0, 0}; 
int targetG[4] = {0, 0, 0, 0};
int targetB[4] = {0, 0, 0, 0};

int brightness[4] = {200, 200, 200, 200}; //[cite: 1]
int mode = 0; // 0 = normal, 1 = stepper[cite: 1]

// --- NEW: STRIP MODE VARIABLES ---
int stripMode[4] = {0, 0, 0, 0}; // 0 = normal, 1 = pulse

// --- LERP SETTINGS ---
float lerpSpeed = 0.05; 

void setup() {
  Serial.begin(9600); //[cite: 1]
  for (int i = 0; i < 4; i++) {
    strips[i].begin(); //[cite: 1]
    strips[i].show();  //[cite: 1]
  }
}

void setTargetColor(int stripIdx, int colorIdx) {
  if (stripIdx < 0 || stripIdx > 3) return; //[cite: 1]

  switch (colorIdx) {
    case 0: targetR[stripIdx] = 255; targetG[stripIdx] = 0;   targetB[stripIdx] = 0;   break; 
    case 1: targetR[stripIdx] = 255; targetG[stripIdx] = 100; targetB[stripIdx] = 0;   break; 
    case 2: targetR[stripIdx] = 255; targetG[stripIdx] = 210; targetB[stripIdx] = 0;   break; 
    case 3: targetR[stripIdx] = 255; targetG[stripIdx] = 255; targetB[stripIdx] = 150; break; 
    case 4: targetR[stripIdx] = 0;   targetG[stripIdx] = 255; targetB[stripIdx] = 0;   break; 
  }
}

void handleCommand(String cmd) {
  if (cmd.length() < 2) return; //[cite: 1]

  char type = cmd.charAt(0); //[cite: 1]

  if (type == 'b') {
    int stripNum = cmd.substring(1, 2).toInt() - 1; //[cite: 1]
    int val = cmd.substring(2).toInt(); //[cite: 1]
    if (stripNum >= 0 && stripNum < 4) brightness[stripNum] = val;
  } 
  else if (type == 'c') {
    int stripNum = cmd.substring(1, 2).toInt() - 1; //[cite: 1]
    int colorIdx = cmd.substring(2).toInt(); //[cite: 1]
    setTargetColor(stripNum, colorIdx);
  } 
  else if (type == 'p') {
    // NEW: Handle Pulse Command (e.g., 'p11' = strip 1 pulse on, 'p10' = strip 1 pulse off)
    int stripNum = cmd.substring(1, 2).toInt() - 1;
    int val = cmd.substring(2).toInt();
    if (stripNum >= 0 && stripNum < 4) stripMode[stripNum] = val;
  }
  else if (type == 'm') {
    mode = cmd.substring(1).toInt(); //[cite: 1]
  }
}

void loop() {
  // 1. Check for Serial Commands
  if (Serial.available()) { //[cite: 1]
    String cmd = Serial.readStringUntil('\n'); //[cite: 1]
    cmd.trim(); //[cite: 1]
    handleCommand(cmd);
  }

  // 2. Handle Global Stepper Animation 
  if (mode == 1) { //[cite: 1]
    static unsigned long lastStep = 0;
    static int stepIdx = 0;
    if (millis() - lastStep > 600) { 
      lastStep = millis();
      stepIdx = (stepIdx + 1) % 5;
      for (int i = 0; i < 4; i++) {
        setTargetColor(i, (stepIdx + i) % 5);
      }
    }
  }

  // 3. APPLY SMOOTHING (Lerp) & PULSING
  for (int i = 0; i < 4; i++) {
    currentR[i] += (targetR[i] - currentR[i]) * lerpSpeed;
    currentG[i] += (targetG[i] - currentG[i]) * lerpSpeed;
    currentB[i] += (targetB[i] - currentB[i]) * lerpSpeed;

    int currentBrt = brightness[i];

    // Apply Pulse Math if this strip is in pulse mode
    if (stripMode[i] == 1) {
      // Calculate a multiplier between roughly 0.4 and 1.0 using a sine wave
      float pulseMultiplier = 0.7 + 0.3 * sin(millis() / 300.0);
      currentBrt = (int)(brightness[i] * pulseMultiplier);
    }

    strips[i].setBrightness(currentBrt);
    for (int led = 0; led < NUM_LEDS; led++) {
      strips[i].setPixelColor(led, (int)currentR[i], (int)currentG[i], (int)currentB[i]);
    }
    strips[i].show();
  }

  delay(10); 
}