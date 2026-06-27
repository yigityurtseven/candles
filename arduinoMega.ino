#include <FastLED.h>

#define STRIP_PIN 5
#define RING_PIN 6
#define NUM_STRIP 10
#define NUM_RING 16

CRGB strip[NUM_STRIP];
CRGB ring[NUM_RING];

// Independent Snapshot Buffers for flawless crossfading
CRGB oldStrip[NUM_STRIP];
CRGB newStrip[NUM_STRIP];
CRGB oldRing[NUM_RING];
CRGB newRing[NUM_RING];

// Independent States
uint8_t targetStripState = 2; // Default: Talking Less
uint8_t targetRingState = 2;  // Default: Talking Less

// Independent Transition Timers
uint16_t stripTransition = 255; 
uint16_t ringTransition = 255; 

uint8_t ringBrightness = 150; 
uint8_t currentEffect = 1; // 1 = Flicker, 0 = Static

// The Spark Palette (Listening)
CRGBPalette16 candlePalette = CRGBPalette16(
  CRGB(200, 30, 0),   // Deep red-orange (cooler edges)
  CRGB(255, 80, 0),   // Base orange
  CRGB(255, 100, 0), // Bright yellow-orange (middle flame)
  CRGB(255, 120, 0)  // Bright yellow (hot spot)
);

CRGBPalette16 ragingFirePalette = CRGBPalette16(
  CRGB::DarkRed, 
  CRGB::Red, 
  CRGB(255, 50, 0), // Your original "Talking Too Much" orange
  CRGB(255, 100, 0)
);

// Helper function to return exact colors from your previous project
CRGB getTalkingColor(uint8_t state) {
  switch(state) {
    case 1: return CRGB(255, 200, 50); // No Talking
    case 2: return CRGB(255, 130, 15); // Talking Less
    case 3: return CRGB(255, 100, 10); // Talking Normal
    case 4: return CRGB(255, 50, 0);   // Talking Too Much
    default: return CRGB::Black;
  }
}

void setup() {
  Serial.begin(9600); 

  FastLED.addLeds<WS2812B, STRIP_PIN, GRB>(strip, NUM_STRIP);
  FastLED.addLeds<WS2812B, RING_PIN, GRB>(ring, NUM_RING);
  FastLED.setBrightness(255); 
  
  fill_solid(oldStrip, NUM_STRIP, CRGB::Black);
  fill_solid(oldRing, NUM_RING, CRGB::Black);
}

void loop() {
  // 1. Check for incoming commands from the Flask Server
  if (Serial.available() > 0) {
    char commandPrefix = Serial.read(); 
    
    // T = Strip State
    if (commandPrefix == 'T') {
      uint8_t requestedState = Serial.parseInt(); 
      if (requestedState != targetStripState) {
        for(int i=0; i<NUM_STRIP; i++) oldStrip[i] = strip[i]; // Snapshot Strip
        targetStripState = requestedState;
        stripTransition = 0; // Trigger Strip Fade
      }
    } 
    // S = Ring State
    else if (commandPrefix == 'S') {
      uint8_t requestedState = Serial.parseInt(); 
      if (requestedState != targetRingState) {
        for(int i=0; i<NUM_RING; i++) oldRing[i] = ring[i]; // Snapshot Ring
        targetRingState = requestedState;
        ringTransition = 0; // Trigger Ring Fade
      }
    } 
    else if (commandPrefix == 'B') ringBrightness = Serial.parseInt(); 
    else if (commandPrefix == 'E') currentEffect = Serial.parseInt();
    else if (commandPrefix == 'X') {
      if (targetStripState != 0) {
        for(int i=0; i<NUM_STRIP; i++) oldStrip[i] = strip[i]; // Snapshot Strip
        targetStripState = 0; // State 0 defaults to Black
        stripTransition = 0;  // Trigger fade
      }
      if (targetRingState != 0) {
        for(int i=0; i<NUM_RING; i++) oldRing[i] = ring[i]; // Snapshot Ring
        targetRingState = 0; // State 0 fills with Black
        ringTransition = 0;  // Trigger fade
      }
    } 
  }

  // ---------------------------------------------------------
  // 2. RENDER THE TARGET ANIMATIONS
  // ---------------------------------------------------------
  
  // STRIP LOGIC: Solid Talking Colors (States 1-4)
  CRGB stripColor = getTalkingColor(targetStripState);
  fill_solid(newStrip, NUM_STRIP, stripColor);

  // RING LOGIC: Talking Colors (1-4) OR Listening Spark (5)
  // RING LOGIC: Talking Colors
  // States 1-3 get the gentle pulsing breath
  if (targetRingState >= 1 && targetRingState <= 3) { 
    CRGB ringColor = getTalkingColor(targetRingState);
    uint8_t breath = beatsin8(30, 10, ringBrightness); 
    fill_solid(newRing, NUM_RING, ringColor.nscale8(breath));
  } 
  // State 4 gets the chaotic, raging fire
  else if (targetRingState == 4) {
    for(int i = 0; i < NUM_RING; i++) {
      uint16_t noiseSpace = i * 857; 
      
      // We use millis() / 2 here (faster than the candle's / 3) for a more agitated flame
      uint8_t noiseVal = inoise8(noiseSpace, millis() / 2); 
      
      uint8_t colorIndex = map(noiseVal, 0, 255, 0, 255); 
      uint8_t pixelBrt = map(noiseVal, 0, 255, ringBrightness / 4, ringBrightness);
      
      newRing[i] = ColorFromPalette(ragingFirePalette, colorIndex, pixelBrt, LINEARBLEND);

      // Aggressive flickering: 20% chance to drop brightness sharply, simulating a roaring fire
      if (random8() < 50) { 
        newRing[i].fadeToBlackBy(random8(100, 200)); 
      }
    }
  }
  else if (targetRingState == 5) { 
    // Listening (Realistic Candle Flame - Independent Flicker)
    for(int i = 0; i < NUM_RING; i++) {
      
      // 1. Break the circular wave by scattering the LEDs across the noise field.
      // 857 is an arbitrary large number to ensure adjacent LEDs sample far-apart noise.
      uint16_t noiseSpace = i * 857; 
      
      // 2. Generate smooth, independent Perlin noise for this specific LED
      uint8_t noiseVal = inoise8(noiseSpace, millis() / 5); 
      
      // 3. Map the noise to our candle palette
      // High noise = hotter/yellower, Low noise = cooler/redder
      uint8_t colorIndex = map(noiseVal, 0, 255, 0, 255); 
      
      // 4. Modulate brightness organically based on the same noise
      uint8_t pixelBrt = map(noiseVal, 0, 255, ringBrightness / 2, ringBrightness);
      
      // Apply the color
      newRing[i] = ColorFromPalette(candlePalette, colorIndex, pixelBrt, LINEARBLEND);

      // 5. Draft/Wind effect: sudden, sharp dips in brightness
      if (currentEffect == 1) {
        if (random8() < 25) { // 10% chance per frame per LED to flicker heavily
          newRing[i].fadeToBlackBy(random8(80, 160)); 
        }
      }
    }
  }
  else {
    fill_solid(newRing, NUM_RING, CRGB::Black);
  }

  // ---------------------------------------------------------
  // 3. DUAL CROSSFADE ENGINES
  // ---------------------------------------------------------
  // Fade the Strip
  if (stripTransition < 255) {
    stripTransition += 4; 
    if (stripTransition > 255) stripTransition = 255;
    for(int i=0; i<NUM_STRIP; i++) strip[i] = blend(oldStrip[i], newStrip[i], stripTransition);
  } else {
    for(int i=0; i<NUM_STRIP; i++) strip[i] = newStrip[i];
  }

  // Fade the Ring
  if (ringTransition < 255) {
    ringTransition += 4; 
    if (ringTransition > 255) ringTransition = 255;
    for(int i=0; i<NUM_RING; i++) ring[i] = blend(oldRing[i], newRing[i], ringTransition);
  } else {
    for(int i=0; i<NUM_RING; i++) ring[i] = newRing[i];
  }

  FastLED.show();
  delay(15); 
}