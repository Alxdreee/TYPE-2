/*
 * ==============================================================================
 * TYPE 2 - AUTONOMOUS POLYPHONIC SYNTHESIZER
 * ==============================================================================
 * Firmware   : v1.6.0 - MIDI Out FX + USB Audio Stream Bridge
 * Author     : Alexandre Esnard
 * License    : MIT License
 * 
 * Hardware   : ESP32-S3, PCM5102A DAC, SSD1306 OLED, KY-040, 2x MPR121
 * DSP Engine : Trapezoidal SVF 24dB, Hardware Performance FX (Tremolo, Sustain, Bend)
 * Comms      : Composite USB (CDC PCM Audio Stream + Native MIDI Out with CC/PB)
 * ==============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPR121.h>
#include <driver/i2s.h>
#include <math.h>
#include <Preferences.h>

// ==============================================================================
// 0. NATIVE USB MIDI LIBRARIES
// ==============================================================================
#include "USB.h"
#include "USBMIDI.h"

USBMIDI MIDI; 

// ==============================================================================
// 1. HARDWARE PINS CONFIGURATION
// ==============================================================================
#define I2S_LRC_PIN         4   
#define I2S_DOUT_PIN        5   
#define I2S_BCLK_PIN        6   
#define I2S_XSMT_PIN        7   

#define I2C0_SDA_PIN        8   
#define I2C0_SCL_PIN        9   
#define I2C1_SDA_PIN        1   
#define I2C1_SCL_PIN        2   

#define ENCODER_CLK_PIN     42  
#define ENCODER_DT_PIN      41  
#define ENCODER_SW_PIN      40  
#define BTN_MENU_PIN        38  
#define MPR_LEFT_IRQ_PIN    39  
#define MPR_RIGHT_IRQ_PIN   18  

// ==============================================================================
// 2. GLOBAL PARAMETERS & MENU ARCHITECTURE
// ==============================================================================
volatile int currentVolume     = 80; 
volatile int currentWaveform   = 2;  
volatile int currentOctave     = 0;  
volatile int currentDrift      = 2;  
volatile int currentCutoff     = 80; 
volatile int currentReso       = 0;  
volatile int currentEnvAmt     = 0;  
volatile int currentVcfAttack  = 0;  
volatile int currentDecay      = 50; 
volatile int currentAmpAttack  = 25; 
volatile int currentAmpRelease = 40; 

volatile int currentTremRate   = 50; 
volatile int currentTremDepth  = 80; 
volatile int currentSustain    = 60; 
volatile int currentBendAmt    = 7;  

volatile int currentDrive      = 0;  
volatile int currentDelayMix   = 0;  
volatile int currentDelayTime  = 30; 
volatile int currentDelayFdbk  = 30; 
volatile int currentPortamento = 30; 
volatile int currentStrum      = 0;  
volatile int currentSlop       = 0;  

volatile int dummyPreset       = 0;  
volatile int dummyInfo         = 0;  

volatile bool modTremolo       = false;
volatile bool modSustain       = false;
volatile bool modPitchBend     = false;

// Activated automatically when the PC Python Audio Bridge connects
volatile bool usbAudioStreamActive = false;

enum ParamFormat { FMT_PERCENT, FMT_WAVEFORM, FMT_RAW, FMT_CUTOFF, FMT_RESO, FMT_INFO, FMT_PRESET };

struct MenuParam {
  const char* shortName; 
  const char* fullName;  
  volatile int* value;   
  int minVal;            
  int maxVal;            
  int step;              
  ParamFormat format;    
  const char* nvsKey;    
};

MenuParam volParams[] = {
  {"Volume",   "MASTER VOL",       &currentVolume,     0, 100, 5, FMT_PERCENT,  "volume"}
};

MenuParam presetParams[] = {
  {"Preset",   "LOAD/SAVE",        &dummyPreset,       0,   0, 0, FMT_PRESET,   ""}
};

MenuParam fxParams[] = {
  {"Drive",    "#1 TUBE DRIVE",    &currentDrive,      0, 100, 2, FMT_PERCENT,  "drive"},
  {"TremRate", "#2 TREMOLO RATE",  &currentTremRate,   0, 100, 5, FMT_PERCENT,  "tremRate"},
  {"TremDep",  "#3 TREMOLO DEPTH", &currentTremDepth,  0, 100, 5, FMT_PERCENT,  "tremDep"},
  {"Dly Mix",  "#4 DELAY MIX",     &currentDelayMix,   0, 100, 5, FMT_PERCENT,  "dlyMix"},
  {"Dly Time", "#5 DELAY TIME",    &currentDelayTime,  0, 100, 5, FMT_PERCENT,  "dlyTime"},
  {"Dly Fdbk", "#6 DELAY FDBK",    &currentDelayFdbk,  0, 100, 5, FMT_PERCENT,  "dlyFdbk"},
  {"Sustain",  "#7 SUSTAIN FORCE", &currentSustain,    0, 100, 5, FMT_PERCENT,  "sustain"},
  {"Bend Amt", "#8 PITCH BEND",    &currentBendAmt,   -12, 12, 1, FMT_RAW,      "bendAmt"},
  {"Glide",    "#9 GLIDE",         &currentPortamento, 0, 100, 2, FMT_PERCENT,  "glide"},
  {"Strum",    "#10 STRUM SPREAD", &currentStrum,      0, 100, 2, FMT_PERCENT,  "strum"},
  {"Slop",     "#11 ANALOG SLOP",  &currentSlop,       0, 100, 2, FMT_PERCENT,  "slop"}
};

MenuParam oscParams[] = {
  {"Wave",     "#1 WAVEFORM",      &currentWaveform,   0, 3,   1, FMT_WAVEFORM, "wave"},
  {"Octave",   "#2 OCTAVE SHIFT",  &currentOctave,    -2, 2,   1, FMT_RAW,      "octave"},
  {"Drift",    "#3 NOTE DRIFT",    &currentDrift,      0, 10,  1, FMT_RAW,      "drift"}
};

MenuParam vcfParams[] = {
  {"Cut",      "#1 VCF CUTOFF",    &currentCutoff,     0, 100, 2, FMT_CUTOFF,   "cutoff"},
  {"Reso",     "#2 VCF RESO",      &currentReso,       0, 10,  1, FMT_RESO,     "reso"},
  {"Env",      "#3 VCF ENV AMT",   &currentEnvAmt,     0, 100, 2, FMT_PERCENT,  "envAmt"},
  {"VcfAtk",   "#4 VCF ATTACK",    &currentVcfAttack,  0, 100, 2, FMT_PERCENT,  "vcfAtk"},
  {"VcfDec",   "#5 VCF DECAY",     &currentDecay,      0, 100, 2, FMT_PERCENT,  "vcfDec"}
};

MenuParam ampParams[] = {
  {"AmpAtk",   "#1 AMP ATTACK",    &currentAmpAttack,  0, 100, 2, FMT_PERCENT,  "ampAtk"},
  {"AmpRel",   "#2 AMP RELEASE",   &currentAmpRelease, 0, 100, 2, FMT_PERCENT,  "ampRel"}
};

MenuParam sysParams[] = {
  {"Infos",    "SYSTEM INFOS",     &dummyInfo,         0,   0, 0, FMT_INFO,     ""}
};

struct MenuCategory {
  const char* name;
  MenuParam* params; 
  int count;      
};

MenuCategory categories[] = {
  {"#1 MASTER VOL", volParams,    sizeof(volParams)    / sizeof(volParams[0])},
  {"#2 PRESETS",    presetParams, sizeof(presetParams) / sizeof(presetParams[0])},
  {"#3 EFFECTS",    fxParams,     sizeof(fxParams)     / sizeof(fxParams[0])},
  {"#4 OSCILLATOR", oscParams,    sizeof(oscParams)    / sizeof(oscParams[0])},
  {"#5 VCF FILTER", vcfParams,    sizeof(vcfParams)    / sizeof(vcfParams[0])},
  {"#6 AMPLIFIER",  ampParams,    sizeof(ampParams)    / sizeof(ampParams[0])},
  {"#7 SYSTEM",     sysParams,    sizeof(sysParams)    / sizeof(sysParams[0])}
};

const int totalCategories = sizeof(categories) / sizeof(categories[0]);

enum MenuState { STATE_HOME, STATE_CAT_SELECT, STATE_PARAM_SELECT, STATE_EDIT };
MenuState currentMenuState = STATE_HOME;

volatile int selectedCategoryIndex = 0;
volatile int selectedParamRelativeIndex = 0; 

const unsigned long UI_TIMEOUT = 10000; 
unsigned long lastUserInteraction = 0;

// ==============================================================================
// 3. PRESET SYSTEM (NVS via Preferences)
// ==============================================================================
Preferences prefs;

const int MAX_PRESETS = 16;
int presetSlotMap[MAX_PRESETS + 1];   
int presetListCount = 0;              
int presetCarouselPos = 0;

enum PresetClickState { PC_IDLE, PC_PRESSED, PC_WAIT_DOUBLE };
PresetClickState presetClickState = PC_IDLE;
unsigned long presetPressStart = 0;
unsigned long presetReleaseTime = 0;
bool presetLongPressFired = false;
const unsigned long PRESET_DOUBLE_CLICK_MS = 350;
const unsigned long PRESET_LONG_PRESS_MS   = 3000;

char presetFeedbackMsg[16] = "";
unsigned long presetFeedbackUntil = 0;

const unsigned long AUTOSAVE_DELAY_MS = 2500;
bool autosavePending = false;

void getPresetNamespace(int slot, char* out) {
  sprintf(out, "preset%d", slot);
}

bool presetSlotValid(int slot) {
  char ns[16]; getPresetNamespace(slot, ns);
  prefs.begin(ns, true); 
  bool valid = prefs.getBool("valid", false);
  prefs.end();
  return valid;
}

void rebuildPresetList() {
  presetListCount = 0;
  for (int i = 0; i < MAX_PRESETS; i++) {
    if (presetSlotValid(i)) presetSlotMap[presetListCount++] = i;
  }
  presetSlotMap[presetListCount++] = -1; 
  if (presetCarouselPos >= presetListCount) presetCarouselPos = 0;
  if (presetCarouselPos < 0) presetCarouselPos = 0;
}

int findFirstFreeSlot() {
  for (int i = 0; i < MAX_PRESETS; i++) if (!presetSlotValid(i)) return i;
  return -1; 
}

void presetCarouselLabel(int pos, char* out) {
  int slot = presetSlotMap[pos];
  if (slot == -1) { strcpy(out, "+ NEW PRESET"); return; }
  if (slot == 0)  { strcpy(out, "CLEAN"); return; } 
  sprintf(out, "PRESET %02d", slot + 1);
}

void setPresetFeedback(const char* msg) {
  strncpy(presetFeedbackMsg, msg, sizeof(presetFeedbackMsg) - 1);
  presetFeedbackMsg[sizeof(presetFeedbackMsg) - 1] = '\0';
  presetFeedbackUntil = millis() + 700;
}

void savePreset(int slot) {
  digitalWrite(I2S_XSMT_PIN, LOW);
  char ns[16]; getPresetNamespace(slot, ns);
  prefs.begin(ns, false); 
  prefs.putBool("valid", true);
  for (int c = 0; c < totalCategories; c++) {
    MenuCategory& cat = categories[c];
    for (int p = 0; p < cat.count; p++) {
      MenuParam& mp = cat.params[p];
      if (mp.format == FMT_INFO || mp.format == FMT_PRESET) continue;
      prefs.putInt(mp.nvsKey, *(mp.value));
    }
  }
  prefs.end();
  digitalWrite(I2S_XSMT_PIN, HIGH);
}

void loadPreset(int slot) {
  char ns[16]; getPresetNamespace(slot, ns);
  prefs.begin(ns, true); 
  for (int c = 0; c < totalCategories; c++) {
    MenuCategory& cat = categories[c];
    for (int p = 0; p < cat.count; p++) {
      MenuParam& mp = cat.params[p];
      if (mp.format == FMT_INFO || mp.format == FMT_PRESET) continue;
      *(mp.value) = prefs.getInt(mp.nvsKey, *(mp.value));
    }
  }
  prefs.end();
}

void deletePreset(int slot) {
  digitalWrite(I2S_XSMT_PIN, LOW);
  char ns[16]; getPresetNamespace(slot, ns);
  prefs.begin(ns, false);
  prefs.clear(); 
  prefs.end();
  digitalWrite(I2S_XSMT_PIN, HIGH);
}

void autosaveCurrentState() {
  prefs.begin("autosave", false);
  if (!prefs.getBool("valid", false)) {
    prefs.putBool("valid", true);
  }
  for (int c = 0; c < totalCategories; c++) {
    MenuCategory& cat = categories[c];
    for (int p = 0; p < cat.count; p++) {
      MenuParam& mp = cat.params[p];
      if (mp.format == FMT_INFO || mp.format == FMT_PRESET) continue;
      int currentVal = *(mp.value);
      if (prefs.getInt(mp.nvsKey, currentVal + 1) != currentVal) {
        prefs.putInt(mp.nvsKey, currentVal);
      }
    }
  }
  prefs.end();
}

void autoloadLastState() {
  prefs.begin("autosave", true);
  bool valid = prefs.getBool("valid", false);
  if (valid) {
    for (int c = 0; c < totalCategories; c++) {
      MenuCategory& cat = categories[c];
      for (int p = 0; p < cat.count; p++) {
        MenuParam& mp = cat.params[p];
        if (mp.format == FMT_INFO || mp.format == FMT_PRESET) continue;
        *(mp.value) = prefs.getInt(mp.nvsKey, *(mp.value));
      }
    }
  }
  prefs.end();
}

int pollPresetClickGesture(bool rawPressed) {
  unsigned long now = millis();
  switch (presetClickState) {
    case PC_IDLE:
      if (rawPressed) { presetClickState = PC_PRESSED; presetPressStart = now; presetLongPressFired = false; }
      break;
    case PC_PRESSED:
      if (rawPressed) {
        if (!presetLongPressFired && (now - presetPressStart) >= PRESET_LONG_PRESS_MS) {
          presetLongPressFired = true;
          return 3; 
        }
      } else {
        if (presetLongPressFired) { presetClickState = PC_IDLE; }
        else { presetReleaseTime = now; presetClickState = PC_WAIT_DOUBLE; }
      }
      break;
    case PC_WAIT_DOUBLE:
      if (rawPressed) { presetClickState = PC_IDLE; return 2; } 
      else if (now - presetReleaseTime > PRESET_DOUBLE_CLICK_MS) { presetClickState = PC_IDLE; return 1; }
      break;
  }
  return 0;
}

// ==============================================================================
// 4. DISPLAY SETUP
// ==============================================================================
#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64
#define OLED_RESET          -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool displayInitialized = false; 

// ==============================================================================
// 5. POLYPHONY, CHORDS & HARDWARE FX DEFINITIONS
// ==============================================================================
Adafruit_MPR121 capRight = Adafruit_MPR121(); 
Adafruit_MPR121 capLeft  = Adafruit_MPR121();  

#define MAX_VOICES 6
#define NUM_OSC    7

const uint8_t leftPadMidiNotes[12] = {66, 61, 68, 63, 70, 65, 60, 67, 62, 69, 64, 71};
const char* noteNames[12] = {"Gb/F#", "Db/C#", "Ab/G#", "Eb/D#", "Bb/A#", "F", "C", "G", "D", "A", "E", "B"};
const uint8_t rightPadPins[12] = {0, 1, 2, 3, 4, 5, 6, 7, 11, 8, 9, 10};

const int chordIntervals[9][MAX_VOICES] = {
  {0, 4, 7,  0, 12, 19}, 
  {0, 4, 7, 11, 12, 16}, 
  {0, 4, 7, 10, 12, 16}, 
  {0, 5, 7,  0, 12, 17}, 
  {0, 3, 7,  0, 12, 15}, 
  {0, 3, 7, 10, 12, 15}, 
  {0, 3, 6,  0, 12, 15}, 
  {0, 4, 8,  0, 12, 16}, 
  {0, 0, 0,  0,  0,  0}  
};
const char* chordNames[9] = {"Maj", "Maj7", "Dom7", "Sus4", "Min", "Min7", "Dim", "Aug", "Unison"};

const float driftOffsets[NUM_OSC]   = {-0.003f, 0.003f, -0.0015f, 0.0015f, -0.004f, 0.004f, 0.0f};
const float panLeft[NUM_OSC]        = {0.50f, 0.85f, 0.15f, 0.70f, 0.30f, 0.50f, 0.50f};
const float panRight[NUM_OSC]       = {0.50f, 0.15f, 0.85f, 0.30f, 0.70f, 0.50f, 0.50f};
const float voiceGainTaper[NUM_OSC] = {1.00f, 0.90f, 0.85f, 0.75f, 0.70f, 0.60f, 1.00f};

// ==============================================================================
// 6. INTER-TASK SHARED MEMORY (Core 0 <-> Core 1)
// ==============================================================================
enum EnvPhase { ENV_IDLE, ENV_ATTACK, ENV_RELEASE }; 

volatile float targetVoiceFreqs[NUM_OSC] = {0.0f}; 
volatile float voiceFreqs[NUM_OSC]       = {0.0f};
volatile float voiceEnv[NUM_OSC]         = {0.0f}; 
volatile float voiceAmpEnv[NUM_OSC]      = {0.0f}; 

volatile EnvPhase vcfPhase[NUM_OSC] = {ENV_IDLE};
volatile EnvPhase vcaPhase[NUM_OSC] = {ENV_IDLE};

volatile bool reqNewChord = false;
volatile bool isLegato    = false;
volatile bool masterGate  = false;

volatile int currentChordIndex = 0; 
volatile int currentNoteIndex  = -1; 

#define DELAY_BUFFER_SIZE 32768 
#define DELAY_MASK 32767 
float* delayBuffer = nullptr; 

// ==============================================================================
// 7. HARDWARE INTERRUPT ROUTINES
// ==============================================================================
volatile int encoderDelta = 0; 
int infoPage = 0; 

void IRAM_ATTR encoderISR() {
  static uint8_t old_AB = 3; 
  static int8_t encval = 0;   
  static const int8_t enc_states[] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0}; 
  
  old_AB <<= 2; 
  if (digitalRead(ENCODER_CLK_PIN)) old_AB |= 0x02; 
  if (digitalRead(ENCODER_DT_PIN)) old_AB |= 0x01; 
  
  encval += enc_states[(old_AB & 0x0f)];
  
  if (encval > 3) {  
    encoderDelta += 1;  
    encval = 0; 
  }
  else if (encval < -3) {  
    encoderDelta -= 1;  
    encval = 0; 
  }
}

// ==============================================================================
// 8. DSP MATH CONSTANTS & UTILITIES
// ==============================================================================
const float DAC_MAX_AMPLITUDE     = 28000.0f; 
const float MICRO_FADE_MULTIPLIER = 0.985f; 
const float MIX_GAIN_COMPENSATION = 2.5f; 
const float FIXED_MIX_NORM        = MIX_GAIN_COMPENSATION / (float)NUM_OSC;
const float MAX_DELAY_SAMPLES     = (float)(DELAY_BUFFER_SIZE - 768); 

inline float fastTanh(float x) {
  return x / (1.0f + fabsf(x)); 
}

class FastPRNG {
  private:
    uint32_t seed;
  public:
    FastPRNG(uint32_t s = 12345) : seed(s) {}
    void reseed(uint32_t s) { seed = s == 0 ? 1 : s; }
    int random(int min_val, int max_val) {
        if (min_val >= max_val) return min_val;
        seed = (1103515245 * seed + 12345);
        return min_val + (int)((seed >> 16) % (max_val - min_val));
    }
    int random(int max_val) {
        return random(0, max_val);
    }
};

#define SAMPLE_RATE          44100
#define CONTROL_RATE_DIVIDER 8       
#define I2S_PORT             I2S_NUM_0
#define WAVETABLE_SIZE       2048
#define WAVETABLE_MASK       (WAVETABLE_SIZE - 1)

const float PHASE_INC_MULT = (float)WAVETABLE_SIZE / (float)SAMPLE_RATE;

enum WaveformType { WAVE_SINE = 0, WAVE_SQUARE, WAVE_TRIANGLE, WAVE_SAW, WAVE_MAX_COUNT };
const char* waveformNames[WAVE_MAX_COUNT] = {"Sine", "Square", "Triangle", "Sawtooth"};

float wavetable[WAVE_MAX_COUNT][WAVETABLE_SIZE];

void generateWavetables() {
  for (int i = 0; i < WAVETABLE_SIZE; i++) {
    wavetable[WAVE_SINE][i] = sinf(2.0f * PI * ((float)i / WAVETABLE_SIZE));
    wavetable[WAVE_SQUARE][i] = (i < WAVETABLE_SIZE / 2) ? 1.0f : -1.0f;
    float norm = (float)i / WAVETABLE_SIZE;
    if (norm < 0.25f) wavetable[WAVE_TRIANGLE][i] = 4.0f * norm;
    else if (norm < 0.75f) wavetable[WAVE_TRIANGLE][i] = 2.0f - 4.0f * norm;
    else wavetable[WAVE_TRIANGLE][i] = -4.0f + 4.0f * norm;
    wavetable[WAVE_SAW][i] = 2.0f * ((float)i / WAVETABLE_SIZE) - 1.0f;
  }
  for (int pass = 0; pass < 3; pass++) { 
    for (int i = 0; i < WAVETABLE_SIZE; i++) {
      int prev = (i - 1 + WAVETABLE_SIZE) % WAVETABLE_SIZE;
      int next = (i + 1) % WAVETABLE_SIZE;
      wavetable[WAVE_SQUARE][i] = (wavetable[WAVE_SQUARE][prev] + wavetable[WAVE_SQUARE][i] + wavetable[WAVE_SQUARE][next]) / 3.0f;
      wavetable[WAVE_SAW][i] = (wavetable[WAVE_SAW][prev] + wavetable[WAVE_SAW][i] + wavetable[WAVE_SAW][next]) / 3.0f;
    }
  }
}

void setupI2S() {
  i2s_config_t i2s_config = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate          = SAMPLE_RATE,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count        = 4,
    .dma_buf_len          = 512,
    .use_apll             = true,
    .tx_desc_auto_clear   = true
  };
  i2s_pin_config_t pin_config = {
    .bck_io_num   = I2S_BCLK_PIN,
    .ws_io_num    = I2S_LRC_PIN,
    .data_out_num = I2S_DOUT_PIN,
    .data_in_num  = I2S_PIN_NO_CHANGE
  };
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_zero_dma_buffer(I2S_PORT);
}

void tuneMPR121(Adafruit_MPR121 &cap) {
  cap.writeRegister(MPR121_ECR, 0x00); 
  cap.writeRegister(MPR121_AUTOCONFIG0, 0x00); 
  cap.writeRegister(MPR121_AUTOCONFIG1, 0x00);
  cap.writeRegister(MPR121_CONFIG1, 0x3F); 
  cap.writeRegister(MPR121_CONFIG2, 0x20); 
  cap.writeRegister(MPR121_ECR, 0x8F); 
}

// ==============================================================================
// 9. EXTENDED AUDIO BOOT SEQUENCE
// ==============================================================================
float midiToFreq(int midiNote) {
  return 440.0f * powf(2.0f, (midiNote - 69) / 12.0f);
}

void playSweep(float startFreq, float endFreq, int durationMs, float amp) {
  unsigned long startT = millis();
  float phase = 0.0f;
  size_t bytesWritten;
  int16_t sampleBuffer[512 * 2]; 
  
  while (millis() - startT < durationMs) {
    float progress = (float)(millis() - startT) / (float)durationMs;
    float currentFreq = startFreq + (endFreq - startFreq) * progress;
    float phaseInc = currentFreq * PHASE_INC_MULT; 
    
    for (int i = 0; i < 512; i++) {
      int index = (int)phase;
      float rawSample = wavetable[WAVE_TRIANGLE][index] * amp; 
      phase += phaseInc;
      if (phase >= WAVETABLE_SIZE) phase -= WAVETABLE_SIZE;
      
      int16_t outSample = (int16_t)(fastTanh(rawSample) * DAC_MAX_AMPLITUDE);
      sampleBuffer[i * 2] = outSample;
      sampleBuffer[i * 2 + 1] = outSample;
    }
    i2s_write(I2S_PORT, sampleBuffer, sizeof(sampleBuffer), &bytesWritten, portMAX_DELAY);
  }
}

void playChord(float freqs[], int numVoices, int durationMs, float amp) {
  unsigned long startT = millis();
  float phases[MAX_VOICES] = {0.0f};
  size_t bytesWritten;
  int16_t sampleBuffer[512 * 2];
  
  int fadeMs = 1500;
  if (fadeMs > durationMs) fadeMs = durationMs; 
  
  while (millis() - startT < durationMs) {
    float fade = 1.0f;
    int remaining = durationMs - (millis() - startT);
    
    if (remaining < fadeMs) {
      fade = (float)remaining / (float)fadeMs; 
    }
    
    for (int i = 0; i < 512; i++) {
      float mixedSample = 0.0f;
      for (int v = 0; v < numVoices; v++) {
        float phaseInc = freqs[v] * PHASE_INC_MULT;
        int index = (int)phases[v];
        mixedSample += wavetable[WAVE_TRIANGLE][index];
        phases[v] += phaseInc;
        if (phases[v] >= WAVETABLE_SIZE) phases[v] -= WAVETABLE_SIZE;
      }
      mixedSample = (mixedSample / numVoices) * amp * fade;
      
      int16_t outSample = (int16_t)(fastTanh(mixedSample) * DAC_MAX_AMPLITUDE);
      sampleBuffer[i * 2] = outSample;
      sampleBuffer[i * 2 + 1] = outSample;
    }
    i2s_write(I2S_PORT, sampleBuffer, sizeof(sampleBuffer), &bytesWritten, portMAX_DELAY);
  }
}

void playBootChime() {
  const float amp = 0.28f; 
  playSweep(45.0f, 720.0f, 1200, amp);        
  float chord[4] = { midiToFreq(48), midiToFreq(60), midiToFreq(72), midiToFreq(76) }; 
  playChord(chord, 4, 3500, amp * 0.8f);       
}

// ==============================================================================
// 10. SYSTEM BOOT ANIMATION
// ==============================================================================
void bootAnimationTask(void *pvParameters) {
  FastPRNG animRng(12345); 
  unsigned long startT = millis();
  unsigned long elapsed = 0;
  bool ledState = false; 

  while ((elapsed = millis() - startT) < 4700) {
    display.clearDisplay();

    if (elapsed < 1200) {
      display.invertDisplay(false);

      #ifdef LED_BUILTIN
      ledState = !ledState;
      if (ledState) {
        neopixelWrite(LED_BUILTIN, 255, 0, 0); 
      } else {
        neopixelWrite(LED_BUILTIN, 0, 0, 0);   
      }
      #endif

      int originX = 4;
      int originY = 52;
      
      display.drawLine(originX, 0, originX, 63, SSD1306_WHITE);  
      display.drawLine(0, originY, 127, originY, SSD1306_WHITE); 
      
      for (int y = originY; y >= 0; y -= 2) {
        int tickWidth = ((originY - y) % 10 == 0) ? 4 : 2;
        display.drawLine(originX - tickWidth, y, originX, y, SSD1306_WHITE);
      }
      
      for (int x = originX; x <= 128; x += 2) {
        int tickHeight = ((x - originX) % 10 == 0) ? 4 : 2;
        display.drawLine(x, originY - tickHeight, x, originY, SSD1306_WHITE);
      }

      int blinkFrame = elapsed / 30; 

      if (blinkFrame % 2 == 0) {
        animRng.reseed(blinkFrame); 
        int waveShape = animRng.random(4); 
        
        float freq = 0.03f + animRng.random(2, 10) * 0.01f;
        int prevX = originX;
        int prevY = originY;

        for (int x = originX; x < 128; x++) {
          float t = (float)(x - originX);
          float val = 0.0f;

          switch(waveShape) {
            case 0: val = sin(t * freq); break; 
            case 1: val = sin(t * freq) > 0 ? 1.0f : -1.0f; break; 
            case 2: val = 2.0f * fabs(2.0f * (t * freq / (2*PI) - floor(t * freq / (2*PI) + 0.5f))) - 1.0f; break; 
            case 3: val = 2.0f * (t * freq / (2*PI) - floor(t * freq / (2*PI) + 0.5f)); break; 
          }

          float amp = (20.0f + animRng.random(-8, 8) + 5.0f * sin(t * 0.05f)) * 1.5f; 
          int y = (originY - 22) - (int)(val * amp); 

          if (x > 30 && x < 65) {
            int yTop = animRng.random(0, 15);        
            int yBot = animRng.random(45, originY);  
            display.drawLine(prevX, prevY, x, yTop, SSD1306_WHITE);
            display.drawLine(x, yTop, x, yBot, SSD1306_WHITE);
            y = yBot;
          } else {
            y += animRng.random(-3, 4);
            if (y < 0) y = 0;
            if (y > originY) y = originY;
            display.drawLine(prevX, prevY, x, y, SSD1306_WHITE);
          }
          prevX = x;
          prevY = y;
        }
        animRng.reseed(micros()); 
      } else {
        display.fillRect(84, 40, 44, 12, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        display.setCursor(86, 42);
        display.print("WARNING");
      }

      display.fillRoundRect(12, 0, 54, 13, 2, SSD1306_BLACK); 
      display.drawRoundRect(12, 0, 54, 13, 2, SSD1306_WHITE); 
      
      display.setTextWrap(false); 
      display.setTextSize(1); 
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(16, 3);
      display.print("TYPE-2"); 
    } 
    else {
      display.invertDisplay(false);
      
      #ifdef LED_BUILTIN
      neopixelWrite(LED_BUILTIN, 0, 255, 0); 
      #endif

      display.fillRect(0, 0, 128, 12, SSD1306_WHITE);
      display.fillRect(0, 52, 128, 12, SSD1306_WHITE);
      
      display.setTextColor(SSD1306_BLACK);
      display.setTextWrap(false);
      display.setTextSize(1);

      long scrollTime = elapsed - 1200;

      const char* topText = " ///  CONNECTION ESTABLISHED   ";
      int topWidth = 186; 
      int topX = (scrollTime / 4) % topWidth; 
      
      display.setCursor(topX, 2);
      display.print(topText);
      display.setCursor(topX - topWidth, 2);
      display.print(topText);

      const char* botText = " ///  THANKS FOR USING   ";
      int botWidth = 150; 
      int botX = -((scrollTime / 4) % botWidth); 
      
      display.setCursor(botX, 54);
      display.print(botText);
      display.setCursor(botX + botWidth, 54);
      display.print(botText);

      display.fillRect(0, 12, 128, 40, SSD1306_BLACK);

      display.setTextSize(2, 4); 
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(28, 16);
      
      display.print("TYPE-2");
    }

    display.display();
    vTaskDelay(15 / portTICK_PERIOD_MS); 
  }

  #ifdef LED_BUILTIN
  neopixelWrite(LED_BUILTIN, 0, 0, 0);
  #endif

  display.invertDisplay(false);
  display.clearDisplay();
  display.display();
  vTaskDelete(NULL); 
}

// ==============================================================================
// 11. UI TASK & MIDI OUT TRANSMISSION
// ==============================================================================
void drawCarouselList(const char* title, const char** items, int count, int selectedIndex) {
  int16_t x1, y1; uint16_t w, h;

  display.setTextSize(1, 2);
  display.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((128 - w) / 2, 0);
  display.print(title);

  display.drawLine(0, 17, 128, 17, SSD1306_WHITE);

  display.setTextSize(1);
  if (count > 2) {
    int prevIdx = (selectedIndex - 1 + count) % count;
    display.getTextBounds(items[prevIdx], 0, 0, &x1, &y1, &w, &h);
    display.setCursor((128 - w) / 2, 19);
    display.print(items[prevIdx]);
  }

  display.fillRect(0, 28, 128, 20, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1, 2);
  display.getTextBounds(items[selectedIndex], 0, 0, &x1, &y1, &w, &h);
  display.setCursor((128 - w) / 2, 30);
  display.print(items[selectedIndex]);

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  if (count > 1) {
    int nextIdx = (selectedIndex + 1) % count;
    display.getTextBounds(items[nextIdx], 0, 0, &x1, &y1, &w, &h);
    display.setCursor((128 - w) / 2, 50);
    display.print(items[nextIdx]);
  }
}

const char* getCleanTitle(const char* text) {
  if (text[0] == '#') {
    const char* space = strchr(text, ' ');
    if (space != nullptr) return space + 1;
  }
  return text;
}

void uiTask(void *pvParameters) {
  FastPRNG uiRng(millis()); 

  bool buttonWasPressed = false;
  bool backButtonWasPressed = false;
  unsigned long lastSwDebounce = 0;
  unsigned long lastBackDebounce = 0;
  const unsigned long DEBOUNCE_MS = 180;
  
  uint16_t lastTouchedLeft = 0;
  uint16_t lastTouchedRight = 0;
  unsigned long lastOledUpdate = 0; 
  
  int lastChordIndex = -1; 
  int previousNoteIndex = -1;

  int currentlyPlayingMidi[NUM_OSC] = {-1, -1, -1, -1, -1, -1, -1};

  // ====================================================================
  // MIDI CONTROL TRACKING VARIABLES (Prevents USB spam)
  // ====================================================================
  bool lastSustainState = false;
  bool lastTremoloState = false;
  bool lastBendState    = false;

  while (true) {
    // Check if the PC Python Audio Bridge requested to start ('A') or stop ('S') streaming
    while (Serial.available() > 0) {
      char cmd = (char)Serial.read();
      if (cmd == 'A') usbAudioStreamActive = true;
      else if (cmd == 'S') usbAudioStreamActive = false;
    }

    if (digitalRead(MPR_LEFT_IRQ_PIN) == LOW || digitalRead(MPR_RIGHT_IRQ_PIN) == LOW) {
      
      uint16_t touchedLeft = capLeft.touched();
      uint16_t touchedRight = capRight.touched();
      
      if (touchedLeft != lastTouchedLeft || touchedRight != lastTouchedRight) {
        
        uint16_t newlyTouchedRight = touchedRight & ~lastTouchedRight;
        
        modTremolo   = (touchedRight & (1 << rightPadPins[9]))  != 0;
        modSustain   = (touchedRight & (1 << rightPadPins[10])) != 0;
        modPitchBend = (touchedRight & (1 << rightPadPins[11])) != 0;

        bool anyChordPressed = false;
        for(int i = 0; i < 9; i++) {
            if (touchedRight & (1 << rightPadPins[i])) {
                anyChordPressed = true;
                break;
            }
        }
        
        if (!anyChordPressed) {
            currentChordIndex = 0; 
        } else {
            if (newlyTouchedRight != 0) {
                for (int i = 0; i < 9; i++) { 
                    if (newlyTouchedRight & (1 << rightPadPins[i])) {
                        currentChordIndex = i;
                        break;
                    }
                }
            }
        }

        uint16_t newlyTouchedLeft = touchedLeft & ~lastTouchedLeft;
        bool noteFound = false;
        
        if (touchedLeft != 0) {
          noteFound = true;
          if (newlyTouchedLeft != 0) {
            for (int i = 0; i < 12; i++) {
              if (newlyTouchedLeft & (1 << i)) {
                currentNoteIndex = i;
                break;
              }
            }
          } else if ((touchedLeft & (1 << currentNoteIndex)) == 0) {
            for (int i = 0; i < 12; i++) {
              if (touchedLeft & (1 << i)) {
                currentNoteIndex = i;
                break;
              }
            }
          }
        } else {
           noteFound = false;
           currentNoteIndex = -1;
        }

        bool wasHolding = (lastTouchedLeft != 0);

        if (noteFound) {
          if (currentNoteIndex != previousNoteIndex || currentChordIndex != lastChordIndex || !masterGate) {
            int rootMidi = leftPadMidiNotes[currentNoteIndex];
            int octaveShift = currentOctave * 12;
            
            for(int v = 0; v < NUM_OSC; v++) {
              if(currentlyPlayingMidi[v] != -1) {
                 MIDI.noteOff(currentlyPlayingMidi[v], 0, 1);
                 currentlyPlayingMidi[v] = -1;
              }
            }

            for(int v = 0; v < MAX_VOICES; v++) {
              int noteMidi = rootMidi + chordIntervals[currentChordIndex][v];
              targetVoiceFreqs[v] = 440.0f * powf(2.0f, (noteMidi - 69) / 12.0f);
              
              int midiOutNote = constrain(noteMidi + octaveShift, 0, 127);
              bool alreadySent = false;
              for(int prev = 0; prev < v; prev++) {
                  if(currentlyPlayingMidi[prev] == midiOutNote) alreadySent = true;
              }
              if(!alreadySent) {
                  MIDI.noteOn(midiOutNote, 100, 1);
                  currentlyPlayingMidi[v] = midiOutNote;
              } else {
                  currentlyPlayingMidi[v] = -1; 
              }
            }
            
            int bassMidi = rootMidi - 12;
            targetVoiceFreqs[MAX_VOICES] = 440.0f * powf(2.0f, (bassMidi - 69) / 12.0f);
            
            int midiOutBass = constrain(bassMidi + octaveShift, 0, 127);
            bool bassAlreadySent = false;
            for(int prev = 0; prev < MAX_VOICES; prev++) {
                if(currentlyPlayingMidi[prev] == midiOutBass) bassAlreadySent = true;
            }
            if(!bassAlreadySent) {
                MIDI.noteOn(midiOutBass, 100, 1);
                currentlyPlayingMidi[MAX_VOICES] = midiOutBass;
            } else {
                currentlyPlayingMidi[MAX_VOICES] = -1;
            }

            isLegato = (wasHolding && masterGate); 
            reqNewChord = true;    
            masterGate = true;     
          }
        } else {
          if (masterGate) {
            for(int v = 0; v < NUM_OSC; v++) {
              if(currentlyPlayingMidi[v] != -1) {
                 MIDI.noteOff(currentlyPlayingMidi[v], 0, 1);
                 currentlyPlayingMidi[v] = -1;
              }
            }
          }
          masterGate = false; 
        }
        
        previousNoteIndex = currentNoteIndex;
        lastChordIndex = currentChordIndex;
        lastTouchedLeft = touchedLeft;
        lastTouchedRight = touchedRight;
      }
    } 

    // ====================================================================
    // MIDI FX CONTROLS TRANSMISSION
    // ====================================================================
    if (modSustain != lastSustainState) {
        MIDI.controlChange(64, modSustain ? 127 : 0, 1);
        lastSustainState = modSustain;
    }

    if (modTremolo != lastTremoloState) {
        MIDI.controlChange(1, modTremolo ? 127 : 0, 1);
        lastTremoloState = modTremolo;
    }

    if (modPitchBend != lastBendState) {
        int16_t pbValue = 0; 
        if (modPitchBend) {
            if (currentBendAmt > 0) pbValue = 8191;
            if (currentBendAmt < 0) pbValue = -8192;
        }
        MIDI.pitchBend(pbValue, 1);
        lastBendState = modPitchBend;
    }

    if (encoderDelta != 0) { 
      int direction = encoderDelta;
      encoderDelta = 0; 
      lastUserInteraction = millis(); 

      if (currentMenuState == STATE_HOME) {
        currentMenuState = STATE_CAT_SELECT; 
      } 
      else if (currentMenuState == STATE_CAT_SELECT) {
        selectedCategoryIndex = (selectedCategoryIndex + direction + totalCategories) % totalCategories;
      } 
      else if (currentMenuState == STATE_PARAM_SELECT) {
        MenuCategory& cat = categories[selectedCategoryIndex];
        selectedParamRelativeIndex = (selectedParamRelativeIndex + direction + cat.count) % cat.count;
      }
      else if (currentMenuState == STATE_EDIT) {
        MenuCategory& cat = categories[selectedCategoryIndex];
        MenuParam& p = cat.params[selectedParamRelativeIndex];
        if (p.format == FMT_INFO) {
          infoPage = (infoPage + direction + 3) % 3; 
        } else if (p.format == FMT_PRESET) {
          presetCarouselPos = (presetCarouselPos + direction + presetListCount) % presetListCount;
        } else {
          int oldVal = *(p.value);
          int newVal = constrain(oldVal + direction * p.step, p.minVal, p.maxVal);
          if (newVal != oldVal) {
            *(p.value) = newVal;
            autosavePending = true;
          }
        }
      }
    }

    bool buttonIsPressed = (digitalRead(ENCODER_SW_PIN) == LOW);

    bool onPresetScreen = (currentMenuState == STATE_EDIT &&
                            categories[selectedCategoryIndex].params[selectedParamRelativeIndex].format == FMT_PRESET);

    if (onPresetScreen) {
      int gesture = pollPresetClickGesture(buttonIsPressed);
      if (gesture != 0) {
        lastUserInteraction = millis();
        autosavePending = true;
        int slot = presetSlotMap[presetCarouselPos];
        
        if (gesture == 1) { 
          if (slot == -1) {
            int freeSlot = findFirstFreeSlot();
            if (freeSlot != -1) {
              savePreset(freeSlot);
              rebuildPresetList();
              for (int i = 0; i < presetListCount; i++) if (presetSlotMap[i] == freeSlot) { presetCarouselPos = i; break; }
              setPresetFeedback("CREATED");
            } else {
              setPresetFeedback("16 MAX");
            }
          } else {
            loadPreset(slot);
            setPresetFeedback("LOADED");
          }
        } 
        else if (gesture == 2) { 
          if (slot == 0) {
            setPresetFeedback("LOCKED"); 
          } else if (slot != -1) { 
            savePreset(slot); 
            setPresetFeedback("UPDATED"); 
          }
        } 
        else if (gesture == 3) { 
          if (slot == 0) {
            setPresetFeedback("LOCKED"); 
          } else if (slot != -1) {
            deletePreset(slot);
            rebuildPresetList();
            setPresetFeedback("DELETED");
          }
        }
      } 
    }
    else if (buttonIsPressed && !buttonWasPressed && (millis() - lastSwDebounce > DEBOUNCE_MS)) {
      lastSwDebounce = millis();
      lastUserInteraction = millis(); 
      if (currentMenuState == STATE_HOME) {
        currentMenuState = STATE_CAT_SELECT;
      }
      else if (currentMenuState == STATE_CAT_SELECT) {
        MenuCategory& cat = categories[selectedCategoryIndex];
        if (cat.count == 1) {
          currentMenuState = STATE_EDIT; 
          selectedParamRelativeIndex = 0; 
          MenuParam& enteredParam = cat.params[0];
          if (enteredParam.format == FMT_PRESET) {
            presetCarouselPos = 0;
            presetClickState = PC_PRESSED; 
            presetLongPressFired = true;   
            rebuildPresetList();
          }
        } else {
          currentMenuState = STATE_PARAM_SELECT; 
          selectedParamRelativeIndex = 0; 
        }
      }
      else if (currentMenuState == STATE_PARAM_SELECT) {
        currentMenuState = STATE_EDIT;
        MenuParam& enteredParam = categories[selectedCategoryIndex].params[selectedParamRelativeIndex];
        if (enteredParam.format == FMT_PRESET) {
          presetCarouselPos = 0;
          presetClickState = PC_IDLE;
          rebuildPresetList();
        }
      }
      else if (currentMenuState == STATE_EDIT) {
        MenuCategory& cat = categories[selectedCategoryIndex];
        if (cat.count == 1) {
           currentMenuState = STATE_CAT_SELECT;
           infoPage = 0;
           presetClickState = PC_IDLE;
        } else {
           currentMenuState = STATE_PARAM_SELECT; 
           infoPage = 0;
        }
      }
    }
    buttonWasPressed = buttonIsPressed;

    bool backButtonIsPressed = (digitalRead(BTN_MENU_PIN) == LOW);
    if (backButtonIsPressed && !backButtonWasPressed && (millis() - lastBackDebounce > DEBOUNCE_MS)) {
      lastBackDebounce = millis();
      lastUserInteraction = millis(); 
      
      if (currentMenuState == STATE_EDIT) {
        MenuCategory& cat = categories[selectedCategoryIndex];
        if (cat.count == 1) {
          currentMenuState = STATE_CAT_SELECT; 
        } else {
          currentMenuState = STATE_PARAM_SELECT; 
        }
        infoPage = 0;
        presetClickState = PC_IDLE;
      }
      else if (currentMenuState == STATE_PARAM_SELECT) {
        currentMenuState = STATE_CAT_SELECT; 
      }
      else if (currentMenuState == STATE_CAT_SELECT) {
        currentMenuState = STATE_HOME; 
      }
    }
    backButtonWasPressed = backButtonIsPressed;

    if (currentMenuState != STATE_HOME && (millis() - lastUserInteraction > UI_TIMEOUT)) {
      currentMenuState = STATE_HOME;
    }

    if (autosavePending && !masterGate && (millis() - lastUserInteraction > AUTOSAVE_DELAY_MS)) {
      autosaveCurrentState();
      autosavePending = false;
    }

    if (millis() - lastOledUpdate > 50) { 
      lastOledUpdate = millis();

      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE);

      if (currentMenuState == STATE_HOME) {
        int originX = 4;
        int originY = 32; 

        display.drawLine(originX, 0, originX, 63, SSD1306_WHITE);  
        display.drawLine(0, originY, 127, originY, SSD1306_WHITE); 
        
        for (int y = 0; y < 64; y += 2) {
          int tickWidth = (abs(y - originY) % 10 == 0) ? 4 : 2;
          display.drawLine(originX - tickWidth, y, originX, y, SSD1306_WHITE);
        }
        
        for (int x = originX; x <= 128; x += 2) {
          int tickHeight = ((x - originX) % 10 == 0) ? 4 : 2; 
          display.drawLine(x, originY, x, originY + tickHeight, SSD1306_WHITE);
        }
        
        for (int v = 0; v < NUM_OSC; v++) {
            int prevX = originX;
            int prevY = 0;
            
            for (int cx = originX; cx <= 128; cx += 4) {
                float bx = cx;
                float by = 8 + v * 8; 
                
                float fx = bx;
                float fy = by;

                for (int attr = 0; attr < NUM_OSC; attr++) {
                    float amp = voiceAmpEnv[attr];
                    if (amp < 0.001f) continue;
                    
                    float freq = voiceFreqs[attr] > 0 ? voiceFreqs[attr] : 440.0f;
                    float visualFreq = fmod(freq, 200.0f) / 50.0f + 0.5f; 
                    float wavePhase = (millis() * 0.001f * visualFreq) + attr;
                    
                    float attrX = 64.0f + 50.0f * sinf(wavePhase);
                    float attrY = 8.0f + attr * 8.0f;

                    float dx = fx - attrX;
                    float dy = fy - attrY;
                    float distSq = dx*dx + dy*dy;

                    float force = amp * 40.0f / (1.0f + distSq * 0.005f);
                    
                    if (distSq > 0.1f) {
                        float dist = sqrtf(distSq);
                        fx += (dx / dist) * force;
                        fy += (dy / dist) * force;
                    }
                }

                int px = constrain((int)fx, originX, 127);
                int py = constrain((int)fy, 0, 63);

                if (cx > originX) {
                    display.drawLine(prevX, prevY, px, py, SSD1306_WHITE);
                } 
                prevX = px;
                prevY = py;
            }
        }
        
        char topStr[12];
        char botStr[12];

        if (masterGate) {
          sprintf(topStr, "+ %05d", uiRng.random(10000, 99999));
          sprintf(botStr, "- %05d", uiRng.random(10000, 99999));
        } else {
          sprintf(topStr, "+ 00000");
          sprintf(botStr, "- 00000");
        }

        display.setTextSize(1, 2);
        display.setTextColor(SSD1306_WHITE);
        int16_t tx, ty; uint16_t tw, th;
        
        display.getTextBounds(topStr, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(128 - tw - 2, 0); 
        display.print(topStr);

        display.getTextBounds(botStr, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor(128 - tw - 2, 48); 
        display.print(botStr);

        if (currentNoteIndex != -1) {
          int cartelX = originX + 3; 
          int cartelWidth = 85; 
          int cartelHeight = 20; 
          
          display.fillRoundRect(cartelX, 0, cartelWidth, cartelHeight, 2, SSD1306_BLACK); 
          display.drawRoundRect(cartelX, 0, cartelWidth, cartelHeight, 2, SSD1306_WHITE); 
          
          display.setTextWrap(false);
          display.setTextSize(1, 2); 
          display.setTextColor(SSD1306_WHITE);
          display.setCursor(cartelX + 4, 2);
          
          char chordBuf[20];
          snprintf(chordBuf, sizeof(chordBuf), "%s %s", noteNames[currentNoteIndex], chordNames[currentChordIndex]);
          display.print(chordBuf);
          
          display.setTextSize(1);
        }
      } 
      else if (currentMenuState == STATE_CAT_SELECT) {
        const char* names[16]; 
        for (int i = 0; i < totalCategories; i++) names[i] = categories[i].name;
        drawCarouselList("MENU", names, totalCategories, selectedCategoryIndex);
      }
      else if (currentMenuState == STATE_PARAM_SELECT) {
        MenuCategory& cat = categories[selectedCategoryIndex];
        const char* names[16]; 
        for (int i = 0; i < cat.count; i++) names[i] = cat.params[i].fullName;
        drawCarouselList(getCleanTitle(cat.name), names, cat.count, selectedParamRelativeIndex);
      }
      else if (currentMenuState == STATE_EDIT) {
        MenuCategory& cat = categories[selectedCategoryIndex];
        MenuParam& p = cat.params[selectedParamRelativeIndex];
        
        if (p.format == FMT_INFO) {
          display.setCursor(0, 0);
          display.setTextSize(1, 2);
          display.print("SYSTEM DATA");
          display.drawLine(0, 18, 128, 18, SSD1306_WHITE);

          display.setTextSize(1);
          display.setCursor(0, 22);
          if (infoPage == 0) {
            display.println("FW VERSION : v1.6.0");
            display.println("AUTHOR : A. Esnard");
            display.println("LICENSE : MIT");
            display.println("-> TURN FOR LINKS");
          } else if (infoPage == 1) {
            display.println("GITHUB : Alxdreee");
            display.println("YOUTUBE: @alexndreee");
            display.println("REDDIT : u/Alxdreee");
            display.println("-> MORE");
          } else {
            display.println("IN : alexandreesnard");
            display.println("GUIDE: Instructables");
            display.println("");
            display.println("-> BACK TO START");
          }
        } 
        else if (p.format == FMT_PRESET) {
          if (millis() < presetFeedbackUntil) {
            int16_t x1, y1; uint16_t w, h;
            display.setTextSize(1, 2);
            display.getTextBounds(presetFeedbackMsg, 0, 0, &x1, &y1, &w, &h);
            display.setCursor((128 - w) / 2, 24);
            display.print(presetFeedbackMsg);
          } else {
            char labelBuf[MAX_PRESETS + 1][17];
            const char* labels[MAX_PRESETS + 1];
            for (int i = 0; i < presetListCount; i++) {
              presetCarouselLabel(i, labelBuf[i]);
              labels[i] = labelBuf[i];
            }
            drawCarouselList("PRESETS", labels, presetListCount, presetCarouselPos);
          }
        }
        else {
          int16_t x1, y1; uint16_t w, h;

          const char* cleanParamName = getCleanTitle(p.fullName);
          display.setTextSize(1, 2); 
          display.setTextColor(SSD1306_WHITE);
          display.getTextBounds(cleanParamName, 0, 0, &x1, &y1, &w, &h);
          display.setCursor((128 - w) / 2, 0);
          display.print(cleanParamName); 

          display.drawLine(0, 18, 128, 18, SSD1306_WHITE);

          char valStr[12];
          char minStr[10];
          char maxStr[10];

          if (p.format == FMT_WAVEFORM) {
            snprintf(valStr, sizeof(valStr), "%.3s", waveformNames[*(p.value)]);
            strcpy(minStr, "SIN");
            strcpy(maxStr, "SAW");
          } 
          else if (p.format == FMT_CUTOFF) {
            snprintf(valStr, sizeof(valStr), "%d", 50 + (int)((*(p.value) / 100.0f) * 7950.0f));
            strcpy(minStr, "50Hz");
            strcpy(maxStr, "8000Hz");
          }
          else if (p.format == FMT_PERCENT) {
            snprintf(valStr, sizeof(valStr), "%d", *(p.value));
            strcpy(minStr, "0%");
            strcpy(maxStr, "100%");
          }
          else if (p.format == FMT_RESO) {
            snprintf(valStr, sizeof(valStr), "%d", *(p.value) * 10);
            strcpy(minStr, "0%");
            strcpy(maxStr, "100%");
          }
          else {
            snprintf(valStr, sizeof(valStr), "%d", *(p.value));
            snprintf(minStr, sizeof(minStr), "%d", p.minVal);
            snprintf(maxStr, sizeof(maxStr), "%d", p.maxVal);
          }

          display.setTextSize(1);
          display.setCursor(36, 22);
          display.print(minStr);
          
          display.getTextBounds(maxStr, 0, 0, &x1, &y1, &w, &h);
          display.setCursor(128 - w, 22); 
          display.print(maxStr);

          display.setCursor(0, 22);
          display.print("VAL");
          
          if (strlen(valStr) > 3 || p.format == FMT_WAVEFORM) {
              display.setTextSize(1, 2); 
              display.setCursor(0, 36);
          } else {
              display.setTextSize(2); 
              display.setCursor(0, 36);
          }
          display.print(valStr);

          float progress = (float)(*(p.value) - p.minVal) / (float)(p.maxVal - p.minVal);
          int numBars = round(progress * 30.0f);
          
          for (int b = 0; b < 30; b++) {
              int barX = 36 + (b * 3); 
              if (b < numBars) {
                  display.fillRect(barX, 36, 2, 24, SSD1306_WHITE); 
              } else {
                  display.drawFastHLine(barX, 60, 2, SSD1306_WHITE); 
              }
          }
        }
      }

      display.display();
    }
    vTaskDelay(10 / portTICK_PERIOD_MS); 
  }
}

// ==============================================================================
// 12. AUDIO TASK (DSP Sound Engine + USB PCM Streamer)
// ==============================================================================
void audioTask(void *pvParameters) {
  FastPRNG audioRng(micros()); 

  size_t bytesWritten;
  int16_t sampleBuffer[512 * 2]; 
  const uint8_t usbSyncHeader[4] = {'T', 'Y', 'P', '2'};
  
  float voicePhases[NUM_OSC]   = {0.0f};
  float voicePhaseInc[NUM_OSC] = {0.0f};

  float ic1eq1[NUM_OSC] = {0.0f}, ic2eq1[NUM_OSC] = {0.0f};
  float ic1eq2[NUM_OSC] = {0.0f}, ic2eq2[NUM_OSC] = {0.0f};
  
  float filter_a1[NUM_OSC] = {0.0f}, filter_a2[NUM_OSC] = {0.0f}, filter_a3[NUM_OSC] = {0.0f};
  float filter_a1_s[NUM_OSC] = {0.0f}, filter_a2_s[NUM_OSC] = {0.0f}, filter_a3_s[NUM_OSC] = {0.0f};

  int strumCounters[NUM_OSC]   = {0};
  bool strumPending[NUM_OSC]   = {false};
  float activeTargetFreqs[NUM_OSC] = {0.0f}; 
  bool doMicroFade = false;

  int controlRateCounter = 0;
  const float audioSampleRateMult = 1.0f / (float)SAMPLE_RATE;
  const float controlSampleRateMult = 1.0f / ((float)SAMPLE_RATE / CONTROL_RATE_DIVIDER); 

  float slopCurrent[NUM_OSC]  = {0.0f};
  float slopTarget[NUM_OSC]   = {0.0f};
  int slopTimer[NUM_OSC]      = {0, 100, 200, 300, 400, 500, 600}; 
  int slopMaxTimer[NUM_OSC]   = {1500, 2000, 1200, 2500, 1800, 2200, 2000}; 

  int delayWriteIndex = 0;
  float smoothedDelaySamples = (currentDelayTime / 100.0f) * MAX_DELAY_SAMPLES;

  float tremoloPhase = 0.0f;
  float tremMultiplier = 1.0f;
  float currentBendSemi = 0.0f;

  while (true) {
    float vcfAttackSecs = 0.005f + (currentVcfAttack * 0.025f); 
    float vcfAttackCoef = 1.0f - expf(-5.0f * controlSampleRateMult / vcfAttackSecs); 
    float decaySecs     = 0.005f + (currentDecay * 0.025f);
    float vcfDecayCoef  = 1.0f - expf(-5.0f * controlSampleRateMult / decaySecs);

    float ampAttackSecs  = 0.005f + (currentAmpAttack * 0.025f); 
    float ampAttackCoef  = 1.0f - expf(-5.0f * audioSampleRateMult / ampAttackSecs);
    
    float actualReleaseSecs = 0.005f + (currentAmpRelease * 0.030f); 
    if (modSustain) {
        actualReleaseSecs = 1.0f + (currentSustain / 100.0f) * 9.0f; 
    }
    float ampReleaseCoef = 1.0f - expf(-5.0f * audioSampleRateMult / actualReleaseSecs);

    float baseCutoff   = 50.0f + (currentCutoff * 79.50f); 
    float envModAmount = currentEnvAmt * 80.0f; 
    
    float userReso = currentReso / 10.0f; 
    float comp = 1.0f - (userReso * 0.6f); 

    float tubeDriveGain = 1.0f + (currentDrive * 0.07f); 
    float tubeAsymmetry = currentDrive * 0.0015f; 
    float dcBlock = fastTanh(tubeAsymmetry);
    
    float tremRateHz = 1.0f + (currentTremRate / 100.0f) * 19.0f; 
    float tremIncControl = tremRateHz * controlSampleRateMult; 
    float tremDepthNormalized = currentTremDepth / 100.0f;
    float baseVolume = currentVolume / 100.0f;

    float targetDelaySamples = (currentDelayTime / 100.0f) * MAX_DELAY_SAMPLES;
    if (targetDelaySamples < 2.0f) targetDelaySamples = 2.0f;
    
    float delayFdbkAmount = (currentDelayFdbk / 100.0f) * 0.85f; 
    float delayMixAmount = currentDelayMix / 100.0f;

    float glideFactor = 1.0f; 
    if (currentPortamento > 0) {
      glideFactor = powf(0.92f, currentPortamento) * 0.15f + 0.0004f;
      if (glideFactor > 1.0f) glideFactor = 1.0f;
    } else {
      glideFactor = 0.8f; 
    }

    int strumSteps = currentStrum * 40; 

    for (int i = 0; i < 512; i++) {
      
      if (reqNewChord) {
        reqNewChord = false;
        if (!isLegato) {
          doMicroFade = true; 
        } else {
          for (int v = 0; v < NUM_OSC; v++) {
            strumCounters[v] = (v == MAX_VOICES) ? 0 : v * strumSteps;
            strumPending[v] = true;
          }
        }
      }

      if (doMicroFade) {
        bool allQuiet = true;
        for (int v = 0; v < NUM_OSC; v++) {
          vcaPhase[v] = ENV_IDLE; 
          voiceAmpEnv[v] *= MICRO_FADE_MULTIPLIER; 
          if (voiceAmpEnv[v] > 0.001f) allQuiet = false; 
        }

        if (allQuiet) {
          for (int v = 0; v < NUM_OSC; v++) voiceAmpEnv[v] = 0.0f;
          doMicroFade = false;
          for (int v = 0; v < NUM_OSC; v++) {
            strumCounters[v] = (v == MAX_VOICES) ? 0 : v * strumSteps;
            strumPending[v] = true;
          }
        }
      }

      if (!masterGate && !doMicroFade) {
        for (int v = 0; v < NUM_OSC; v++) {
          if (vcaPhase[v] == ENV_ATTACK) vcaPhase[v] = ENV_RELEASE;
        }
      }

      float mixedLeft = 0.0f;
      float mixedRight = 0.0f;

      for (int v = 0; v < NUM_OSC; v++) {
        if (strumPending[v] && !doMicroFade) {
          if (strumCounters[v] > 0) {
            strumCounters[v]--;
          } else {
            strumPending[v] = false;
            if (masterGate) {
              activeTargetFreqs[v] = targetVoiceFreqs[v];
              if (!isLegato) {
                voiceFreqs[v] = targetVoiceFreqs[v];
                voiceEnv[v] = 0.0f;    
                vcfPhase[v] = ENV_ATTACK;
                voiceAmpEnv[v] = 0.0f;
                vcaPhase[v] = ENV_ATTACK;
              } else {
                vcaPhase[v] = ENV_ATTACK; 
              }
            }
          }
        }

        if (vcaPhase[v] == ENV_ATTACK && !strumPending[v]) {
          voiceAmpEnv[v] += (1.0f - voiceAmpEnv[v]) * ampAttackCoef;
          if (voiceAmpEnv[v] >= 0.99f) voiceAmpEnv[v] = 1.0f;
        } else if (vcaPhase[v] == ENV_RELEASE) {
          voiceAmpEnv[v] += (0.0f - voiceAmpEnv[v]) * ampReleaseCoef;
          if (voiceAmpEnv[v] <= 0.001f) {
            voiceAmpEnv[v] = 0.0f;
            vcaPhase[v] = ENV_IDLE;
          }
        }

        voicePhases[v] += voicePhaseInc[v]; 
        if (voicePhases[v] >= WAVETABLE_SIZE) voicePhases[v] -= WAVETABLE_SIZE;

        if (voiceAmpEnv[v] > 0.0001f) {
          int idx0 = (int)voicePhases[v];
          int idx1 = (idx0 + 1) & WAVETABLE_MASK;
          float frac = voicePhases[v] - (float)idx0;
          int waveToUse = (v == MAX_VOICES) ? WAVE_SINE : currentWaveform;
          
          float rawWave = wavetable[waveToUse][idx0] + frac * (wavetable[waveToUse][idx1] - wavetable[waveToUse][idx0]);
          float input = rawWave * comp; 

          float a1 = filter_a1[v];
          float a2 = filter_a2[v];
          float a3 = filter_a3[v];

          float v3_1 = input - ic2eq1[v];
          float v1_1 = a1 * ic1eq1[v] + a2 * v3_1;
          float v2_1 = ic2eq1[v] + a2 * ic1eq1[v] + a3 * v3_1;
          
          ic1eq1[v] = 2.0f * v1_1 - ic1eq1[v];
          ic2eq1[v] = 2.0f * v2_1 - ic2eq1[v];
          float out1 = v2_1; 

          float a1_s = filter_a1_s[v];
          float a2_s = filter_a2_s[v];
          float a3_s = filter_a3_s[v];

          float v3_2 = out1 - ic2eq2[v];
          float v1_2 = a1_s * ic1eq2[v] + a2_s * v3_2;
          float v2_2 = ic2eq2[v] + a2_s * ic1eq2[v] + a3_s * v3_2;
          
          ic1eq2[v] = 2.0f * v1_2 - ic1eq2[v];
          ic2eq2[v] = 2.0f * v2_2 - ic2eq2[v];

          float voiceOutput = v2_2 * voiceAmpEnv[v] * voiceGainTaper[v];

          voiceOutput = (voiceOutput * tubeDriveGain) + tubeAsymmetry;
          voiceOutput = fastTanh(voiceOutput) - dcBlock;

          mixedLeft += voiceOutput * panLeft[v];
          mixedRight += voiceOutput * panRight[v];
        }
      }

      float normLeft  = mixedLeft * FIXED_MIX_NORM;
      float normRight = mixedRight * FIXED_MIX_NORM;

      if (delayBuffer != nullptr) {
        smoothedDelaySamples += (targetDelaySamples - smoothedDelaySamples) * 0.0005f;
        float readPos = (float)delayWriteIndex - smoothedDelaySamples;
        if (readPos < 0.0f) readPos += (float)DELAY_BUFFER_SIZE;
        
        int rIdx0 = ((int)readPos) & DELAY_MASK;
        int rIdx1 = (rIdx0 + 1) & DELAY_MASK;
        float dFrac = readPos - floorf(readPos);
        float delayOut = delayBuffer[rIdx0] + dFrac * (delayBuffer[rIdx1] - delayBuffer[rIdx0]);

        float monoIn = (normLeft + normRight) * 0.5f;
        float delayNext = monoIn + (delayOut * delayFdbkAmount);
        
        if (delayNext > 1.5f) delayNext = 1.5f;
        else if (delayNext < -1.5f) delayNext = -1.5f;
        
        delayBuffer[delayWriteIndex] = delayNext;
        delayWriteIndex = (delayWriteIndex + 1) & DELAY_MASK;

        normLeft += delayOut * delayMixAmount;
        normRight += delayOut * delayMixAmount;
      }

      float masterVolumeMult = baseVolume * tremMultiplier;
      int16_t outSampleLeft  = (int16_t)(fastTanh(normLeft) * DAC_MAX_AMPLITUDE * masterVolumeMult); 
      int16_t outSampleRight = (int16_t)(fastTanh(normRight) * DAC_MAX_AMPLITUDE * masterVolumeMult); 

      sampleBuffer[i * 2] = outSampleLeft;     
      sampleBuffer[i * 2 + 1] = outSampleRight; 

      if (controlRateCounter == 0) {
        tremoloPhase += tremIncControl;
        if (tremoloPhase > 1.0f) tremoloPhase -= 1.0f;
        if (modTremolo) {
          float lfo = 0.5f - 0.5f * cosf(2.0f * PI * tremoloPhase);
          tremMultiplier = 1.0f - (tremDepthNormalized * lfo);
        } else {
          tremMultiplier = 1.0f;
        }

        float targetBend = modPitchBend ? (float)currentBendAmt : 0.0f;
        currentBendSemi += (targetBend - currentBendSemi) * 0.002f; 
        float octaveAndBendMult = powf(2.0f, (float)currentOctave + (currentBendSemi / 12.0f));
        
        float expoReso = userReso * userReso * userReso; 
        float R = 1.0f - (expoReso * 0.995f); 
        const float T = audioSampleRateMult;

        for (int v = 0; v < NUM_OSC; v++) {
          
          slopTimer[v]++;
          if (slopTimer[v] > slopMaxTimer[v]) { 
            slopTimer[v] = 0;
            slopTarget[v] = ((float)audioRng.random(-1000, 1001) / 1000.0f); 
            slopMaxTimer[v] = audioRng.random(1000, 4000); 
          }
          slopCurrent[v] += (slopTarget[v] - slopCurrent[v]) * 0.001f;

          float dynamicSlop = slopCurrent[v] * (currentSlop * 0.0006f);

          if (!strumPending[v]) {
             voiceFreqs[v] += (activeTargetFreqs[v] - voiceFreqs[v]) * glideFactor;
          }
          
          float driftFactor = 1.0f + ((float)currentDrift * driftOffsets[v]) + dynamicSlop;
          float actualFreq = voiceFreqs[v] > 0.0f ? (voiceFreqs[v] * driftFactor * octaveAndBendMult) : 0.0f;
          voicePhaseInc[v] = actualFreq * PHASE_INC_MULT;

          if (vcfPhase[v] == ENV_ATTACK && !strumPending[v]) {
            voiceEnv[v] += (1.0f - voiceEnv[v]) * vcfAttackCoef;
            if (voiceEnv[v] >= 0.99f) {
              voiceEnv[v] = 1.0f;
              vcfPhase[v] = ENV_RELEASE;
            }
          } else if (vcfPhase[v] == ENV_RELEASE) {
            voiceEnv[v] += (0.0f - voiceEnv[v]) * vcfDecayCoef;
          }

          if (v == 0 || envModAmount > 0.0f) {
            float finalCutoff = constrain(baseCutoff + (envModAmount * voiceEnv[v]), 20.0f, 14000.0f);
            float g = tanf(PI * finalCutoff * T);
            
            filter_a1[v] = 1.0f / (1.0f + g * (g + 2.0f * R));
            filter_a2[v] = g * filter_a1[v];
            filter_a3[v] = g * filter_a2[v];

            filter_a1_s[v] = 1.0f / (1.0f + g * (g + 2.0f));
            filter_a2_s[v] = g * filter_a1_s[v];
            filter_a3_s[v] = g * filter_a2_s[v];
          } else {
            filter_a1[v]   = filter_a1[0];   filter_a2[v]   = filter_a2[0];   filter_a3[v]   = filter_a3[0];
            filter_a1_s[v] = filter_a1_s[0]; filter_a2_s[v] = filter_a2_s[0]; filter_a3_s[v] = filter_a3_s[0];
          }
        }
      }

      controlRateCounter++;
      if (controlRateCounter >= CONTROL_RATE_DIVIDER) controlRateCounter = 0;
    }

    // 1. Hardware I2S output (clocks the loop at an exact 44.1 kHz crystal rate)
    i2s_write(I2S_PORT, sampleBuffer, sizeof(sampleBuffer), &bytesWritten, portMAX_DELAY);

    // 2. USB PCM Audio Stream output (when PC Python receiver is active)
    if (usbAudioStreamActive && Serial) {
      Serial.write(usbSyncHeader, 4);
      Serial.write((const uint8_t*)sampleBuffer, sizeof(sampleBuffer));
    }
  }
}

// ==============================================================================
// 13. FATAL ERROR HANDLING
// ==============================================================================
void fatalError(const char* message) {
  Serial.println();
  Serial.print("FATAL BOOT ERROR: ");
  Serial.println(message);

  if (displayInitialized) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("SYSTEM HALTED");
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    display.setCursor(0, 20);
    display.println(message);
    display.display();
  }

  #ifdef LED_BUILTIN
  pinMode(LED_BUILTIN, OUTPUT);
  #endif

  while (true) {
    #ifdef LED_BUILTIN
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100); 
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
    #else
    delay(1000); 
    #endif
  }
}

// ==============================================================================
// 14. SYSTEM SETUP 
// ==============================================================================
void initCleanPreset();

void setup() {
  #ifdef LED_BUILTIN
  pinMode(LED_BUILTIN, OUTPUT);
  neopixelWrite(LED_BUILTIN, 0, 0, 0); 
  #endif

  display.setTextWrap(false);
  
  Serial.setTxTimeoutMs(15);
  Serial.begin(115200);
  
  // SAFETY DELAY: Allows host PC to enumerate Serial port
  delay(3000); 

  // Initialize Composite USB (CDC Serial + MIDI)
  MIDI.begin();
  USB.begin();

  delayBuffer = (float*)calloc(DELAY_BUFFER_SIZE, sizeof(float));
  if (delayBuffer == nullptr) {
    delayBuffer = (float*)ps_calloc(DELAY_BUFFER_SIZE, sizeof(float));
    if (delayBuffer == nullptr) {
      fatalError("Memory Allocation Failed (SRAM/PSRAM full)");
    }
  }

  pinMode(I2S_XSMT_PIN, OUTPUT);
  digitalWrite(I2S_XSMT_PIN, LOW); 

  pinMode(ENCODER_CLK_PIN, INPUT_PULLUP);
  pinMode(ENCODER_DT_PIN, INPUT_PULLUP);
  pinMode(ENCODER_SW_PIN, INPUT_PULLUP);
  pinMode(BTN_MENU_PIN, INPUT_PULLUP);
  pinMode(MPR_LEFT_IRQ_PIN, INPUT_PULLUP); 
  pinMode(MPR_RIGHT_IRQ_PIN, INPUT_PULLUP); 

  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK_PIN), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_DT_PIN), encoderISR, CHANGE);

  Wire.begin(I2C0_SDA_PIN, I2C0_SCL_PIN);   
  Wire.setClock(400000);                    
  Wire.setTimeOut(20);                      

  Wire1.begin(I2C1_SDA_PIN, I2C1_SCL_PIN);  
  Wire1.setClock(400000);                   
  Wire1.setTimeOut(20);                     

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    fatalError("OLED Display (SSD1306) not found");
  }
  displayInitialized = true; 
  display.setTextWrap(false); 

  const uint8_t TOUCH_THRESH = 80;  
  const uint8_t RELEASE_THRESH = 40; 

  if (!capLeft.begin(0x5A, &Wire1)) {
    fatalError("Left Touch Sensor (MPR121) not found");
  }
  tuneMPR121(capLeft); 
  capLeft.setThresholds(255, 255); 
  delay(500); 
  capLeft.setThresholds(TOUCH_THRESH, RELEASE_THRESH); 

  if (!capRight.begin(0x5A, &Wire)) {
    fatalError("Right Touch Sensor (MPR121) not found");
  }
  tuneMPR121(capRight); 
  capRight.setThresholds(255, 255); 
  delay(500); 
  capRight.setThresholds(TOUCH_THRESH, RELEASE_THRESH); 

  generateWavetables();
  setupI2S();
  initCleanPreset();
  autoloadLastState(); 

  delay(20);
  digitalWrite(I2S_XSMT_PIN, HIGH); 

  xTaskCreatePinnedToCore(bootAnimationTask, "BootAnim", 4096, NULL, 1, NULL, 0);
  playBootChime();

  xTaskCreatePinnedToCore(uiTask, "UITask", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(audioTask, "AudioTask", 8192, NULL, configMAX_PRIORITIES - 1, NULL, 1);
}

void initCleanPreset() {
  prefs.begin("preset0", true); 
  bool valid = prefs.getBool("valid", false);
  prefs.end();

  if (!valid) {
    prefs.begin("preset0", false);
    prefs.putBool("valid", true);
    
    prefs.putInt("wave", 3);      
    prefs.putInt("drift", 0);     
    prefs.putInt("octave", 0);    
    prefs.putInt("cutoff", 100);  
    prefs.putInt("reso", 0);      
    prefs.putInt("envAmt", 0);    
    prefs.putInt("vcfAtk", 0);
    prefs.putInt("vcfDec", 50);
    prefs.putInt("ampAtk", 0);    
    prefs.putInt("ampRel", 15);   
    prefs.putInt("volume", 80);   
    
    prefs.putInt("tremRate", 50);
    prefs.putInt("tremDep", 80);
    prefs.putInt("sustain", 60);
    prefs.putInt("bendAmt", 7);

    prefs.putInt("glide", 0);     
    prefs.putInt("strum", 0);     
    prefs.putInt("drive", 0);     
    prefs.putInt("slop", 0);      
    prefs.putInt("dlyTime", 30);
    prefs.putInt("dlyFdbk", 0);   
    prefs.putInt("dlyMix", 0);    
    
    prefs.end();
  }
}

void loop() {
  vTaskDelete(NULL); 
}
