/*
 * ==============================================================================
 * TYPE 2 - AUTONOMOUS POLYPHONIC SYNTHESIZER
 * ==============================================================================
 * Firmware   : v0.0.0
 * Author     : Alexandre Esnard
 * License    : Custom Non-Commercial Open Source
 * 
 * Hardware   : ESP32-S3, PCM5102A DAC, SSD1306 OLED, KY-040, 2x MPR121
 * DSP Engine : Polynomial Saturation, Hybrid Dual-Rate Architecture
 * Filters    : Factorized 24dB/Oct Moog Ladder Filter
 * Memory     : Dynamic PSRAM allocation for audio buffers
 * ==============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPR121.h>
#include <driver/i2s.h>
#include <math.h>

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
volatile int currentVolume     = 15; // Volume par défaut réglé à 15%
volatile int currentWaveform   = 3;  // 3 correspond à la forme d'onde Dent de Scie (Sawtooth)
volatile int currentDrift      = 2;  
volatile int currentCutoff     = 80; 
volatile int currentReso       = 0;  
volatile int currentEnvAmt     = 0;  
volatile int currentVcfAttack  = 0;  
volatile int currentDecay      = 50; 
volatile int currentAmpAttack  = 25; 
volatile int currentAmpRelease = 40; 
volatile int currentPortamento = 30; 
volatile int currentStrum      = 0;  
volatile int currentDrive      = 0;  
volatile int currentSlop       = 0;  
volatile int currentDelayTime  = 30; 
volatile int currentDelayFdbk  = 30; 
volatile int currentDelayMix   = 0;  

volatile int dummyInfo         = 0;  

enum ParamFormat { FMT_PERCENT, FMT_WAVEFORM, FMT_RAW, FMT_CUTOFF, FMT_RESO, FMT_INFO };

struct MenuParam {
  const char* shortName; 
  const char* fullName;  
  volatile int* value;   
  int minVal;            
  int maxVal;            
  int step;              
  ParamFormat format;    
};

MenuParam oscParams[] = {
  {"Volume",   "#1 MASTER VOL",    &currentVolume,     0, 100, 5, FMT_PERCENT}, 
  {"Wave",     "#2 WAVEFORM",      &currentWaveform,   0, 3,   1, FMT_WAVEFORM},
  {"Drift",    "#3 NOTE DRIFT",    &currentDrift,      0, 10,  1, FMT_RAW}
};

MenuParam vcfParams[] = {
  {"Cut",      "#1 VCF CUTOFF",    &currentCutoff,     0, 100, 2, FMT_CUTOFF},
  {"Reso",     "#2 VCF RESO",      &currentReso,       0, 10,  1, FMT_RESO},
  {"Env",      "#3 VCF ENV AMT",   &currentEnvAmt,     0, 100, 2, FMT_PERCENT},
  {"VcfAtk",   "#4 VCF ATTACK",    &currentVcfAttack,  0, 100, 2, FMT_PERCENT},
  {"VcfDec",   "#5 VCF DECAY",     &currentDecay,      0, 100, 2, FMT_PERCENT}
};

MenuParam ampParams[] = {
  {"AmpAtk",   "#1 AMP ATTACK",    &currentAmpAttack,  0, 100, 2, FMT_PERCENT},
  {"AmpRel",   "#2 AMP RELEASE",   &currentAmpRelease, 0, 100, 2, FMT_PERCENT}
};

MenuParam playParams[] = {
  {"Glide",    "#1 GLIDE",         &currentPortamento, 0, 100, 2, FMT_PERCENT},
  {"Strum",    "#2 STRUM SPREAD",  &currentStrum,      0, 100, 2, FMT_PERCENT}
};

MenuParam fxParams[] = {
  {"Drive",    "#1 TUBE DRIVE",    &currentDrive,      0, 100, 2, FMT_PERCENT},
  {"Slop",     "#2 ANALOG SLOP",   &currentSlop,       0, 100, 2, FMT_PERCENT},
  {"Dly Time", "#3 DELAY TIME",    &currentDelayTime,  0, 100, 5, FMT_PERCENT},
  {"Dly Fdbk", "#4 DELAY FDBK",    &currentDelayFdbk,  0, 100, 5, FMT_PERCENT},
  {"Dly Mix",  "#5 DELAY MIX",     &currentDelayMix,   0, 100, 5, FMT_PERCENT}
};

MenuParam sysParams[] = {
  {"Infos",    "#1 SYSTEM INFOS",  &dummyInfo,         0,   0, 0, FMT_INFO}
};

struct MenuCategory {
  const char* name;
  MenuParam* params; 
  int count;      
};

MenuCategory categories[] = {
  {"#1 MAIN & OSC", oscParams,  sizeof(oscParams)  / sizeof(oscParams[0])}, 
  {"#2 VCF FILTER", vcfParams,  sizeof(vcfParams)  / sizeof(vcfParams[0])},
  {"#3 AMPLIFIER",  ampParams,  sizeof(ampParams)  / sizeof(ampParams[0])},
  {"#4 PLAY MODE",  playParams, sizeof(playParams) / sizeof(playParams[0])},
  {"#5 EFFECTS",    fxParams,   sizeof(fxParams)   / sizeof(fxParams[0])},
  {"#6 SYSTEM",     sysParams,  sizeof(sysParams)  / sizeof(sysParams[0])}
};

const int totalCategories = sizeof(categories) / sizeof(categories[0]);

enum MenuState { STATE_HOME, STATE_CAT_SELECT, STATE_PARAM_SELECT, STATE_EDIT };
MenuState currentMenuState = STATE_HOME;

volatile int selectedCategoryIndex = 0;
volatile int selectedParamRelativeIndex = 0; 

const unsigned long UI_TIMEOUT = 10000; 
unsigned long lastUserInteraction = 0;

// ==============================================================================
// 3. DISPLAY SETUP
// ==============================================================================
#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64
#define OLED_RESET          -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool displayInitialized = false; 

// ==============================================================================
// 4. POLYPHONY & CHORDS DEFINITIONS
// ==============================================================================
Adafruit_MPR121 capRight = Adafruit_MPR121(); 
Adafruit_MPR121 capLeft  = Adafruit_MPR121();  

#define MAX_VOICES 6
#define NUM_OSC    7

const uint8_t leftPadMidiNotes[12] = {66, 61, 68, 63, 70, 65, 60, 67, 62, 69, 64, 71};
const char* noteNames[12] = {"Gb/F#", "Db/C#", "Ab/G#", "Eb/D#", "Bb/A#", "F", "C", "G", "D", "A", "E", "B"};
const uint8_t rightPadPins[9] = {0, 1, 2, 3, 4, 5, 6, 7, 11};

const int chordIntervals[9][MAX_VOICES] = {
  {0, 4, 7,  0, 12, 19}, // 0: Maj
  {0, 4, 7, 11, 12, 16}, // 1: Maj7
  {0, 4, 7, 10, 12, 16}, // 2: Dom7
  {0, 5, 7,  0, 12, 17}, // 3: Sus4
  {0, 3, 7,  0, 12, 15}, // 4: Min
  {0, 3, 7, 10, 12, 15}, // 5: Min7
  {0, 3, 6,  0, 12, 15}, // 6: Dim
  {0, 4, 8,  0, 12, 16}, // 7: Aug
  {0, 0, 0,  0,  0,  0}  // 8: Unison
};
const char* chordNames[9] = {"Maj", "Maj7", "Dom7", "Sus4", "Min", "Min7", "Dim", "Aug", "Unison"};

const float driftOffsets[NUM_OSC]   = {-0.003f, 0.003f, -0.0015f, 0.0015f, -0.004f, 0.004f, 0.0f};
const float panLeft[NUM_OSC]        = {0.50f, 0.85f, 0.15f, 0.70f, 0.30f, 0.50f, 0.50f};
const float panRight[NUM_OSC]       = {0.50f, 0.15f, 0.85f, 0.30f, 0.70f, 0.50f, 0.50f};
const float voiceGainTaper[NUM_OSC] = {1.00f, 0.90f, 0.85f, 0.75f, 0.70f, 0.60f, 1.00f};

// ==============================================================================
// 5. INTER-TASK SHARED MEMORY (Core 0 <-> Core 1)
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
volatile float* delayBuffer = nullptr; 

// ==============================================================================
// 6. HARDWARE INTERRUPT ROUTINES
// ==============================================================================
volatile int encoderDelta = 0; 

void IRAM_ATTR encoderISR() {
  static uint8_t old_AB = 3; 
  static int8_t encval = 0;   
  static const int8_t enc_states[] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0}; 
  
  old_AB <<= 2; 
  if (digitalRead(ENCODER_CLK_PIN)) old_AB |= 0x02; 
  if (digitalRead(ENCODER_DT_PIN)) old_AB |= 0x01; 
  
  encval += enc_states[(old_AB & 0x0f)];
  
  if (encval > 3) {  
    encoderDelta = 1;  
    encval = 0; 
  }
  else if (encval < -3) {  
    encoderDelta = -1;  
    encval = 0; 
  }
}

// ==============================================================================
// 7. DSP MATH CONSTANTS 
// ==============================================================================
const float DAC_MAX_AMPLITUDE = 28000.0f; 
const float MICRO_FADE_MULTIPLIER = 0.985f; 
const float MIX_GAIN_COMPENSATION = 2.5f; 
const float MAX_DELAY_SAMPLES = (float)(DELAY_BUFFER_SIZE - 768); 

// ==============================================================================
// 8. INITIALIZATION HELPERS
// ==============================================================================

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

const float PHASE_INC_MULT = (float)WAVETABLE_SIZE / (float)SAMPLE_RATE;
const float CUTOFF_MULT    = 2.0f / (float)SAMPLE_RATE;

enum WaveformType { WAVE_SINE = 0, WAVE_SQUARE, WAVE_TRIANGLE, WAVE_SAW, WAVE_MAX_COUNT };
const char* waveformNames[WAVE_MAX_COUNT] = {"Sine", "Square", "Triangle", "Sawtooth"};

float wavetable[WAVE_MAX_COUNT][WAVETABLE_SIZE];

inline float fastTanh(float x) {
  if (x < -2.0f) return -1.0f;
  if (x > 2.0f) return 1.0f;
  return x - 0.25f * x * fabsf(x);
}

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
      
      int16_t outSample = (int16_t)(rawSample * DAC_MAX_AMPLITUDE);
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
      
      int16_t outSample = (int16_t)(mixedSample * DAC_MAX_AMPLITUDE);
      sampleBuffer[i * 2] = outSample;
      sampleBuffer[i * 2 + 1] = outSample;
    }
    i2s_write(I2S_PORT, sampleBuffer, sizeof(sampleBuffer), &bytesWritten, portMAX_DELAY);
  }
}

void playBootChime() {
  const float amp = 0.04f; // Volume du Boot réglé mathématiquement à ~15%
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

    // PHASE 1: Grid Rendering
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
    // PHASE 2: Data Validation
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

      const char* topText = " ///  CONNEXION ESTABLISHED   ";
      int topWidth = 180; 
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
// 11. FREERTOS TASKS (AUDIO & UI)
// ==============================================================================

void IRAM_ATTR audioTask(void *pvParameters) {
  FastPRNG audioRng(micros()); 

  size_t bytesWritten;
  int16_t sampleBuffer[512 * 2]; 
  
  float voicePhases[NUM_OSC]   = {0.0f};
  float voicePhaseInc[NUM_OSC] = {0.0f};

  float moogOut1[NUM_OSC] = {0.0f}, moogOut2[NUM_OSC] = {0.0f};
  float moogOut3[NUM_OSC] = {0.0f}, moogOut4[NUM_OSC] = {0.0f};
  float moogInOld[NUM_OSC] = {0.0f}, moogOut1Old[NUM_OSC] = {0.0f};
  float moogOut2Old[NUM_OSC] = {0.0f}, moogOut3Old[NUM_OSC] = {0.0f};

  float filter_p[NUM_OSC] = {0.0f};
  float filter_k[NUM_OSC] = {0.0f};

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

  while (true) {
    float vcfAttackSecs = 0.005f + (currentVcfAttack * 0.025f); 
    float vcfAttackCoef = 1.0f - expf(-5.0f * controlSampleRateMult / vcfAttackSecs); 
    float decaySecs     = 0.005f + (currentDecay * 0.025f);
    float vcfDecayCoef  = 1.0f - expf(-5.0f * controlSampleRateMult / decaySecs);

    float ampAttackSecs  = 0.005f + (currentAmpAttack * 0.025f); 
    float ampAttackCoef  = 1.0f - expf(-5.0f * audioSampleRateMult / ampAttackSecs);
    float ampReleaseSecs = 0.005f + (currentAmpRelease * 0.030f);
    float ampReleaseCoef = 1.0f - expf(-5.0f * audioSampleRateMult / ampReleaseSecs);

    float baseCutoff   = 50.0f + (currentCutoff * 79.50f); 
    float envModAmount = currentEnvAmt * 80.0f; 
    float reso         = currentReso * 0.38f; 

    float tubeDriveGain = 1.0f + (currentDrive * 0.07f); 
    float tubeAsymmetry = currentDrive * 0.0015f; 
    float dcBlock = fastTanh(tubeAsymmetry);
    
    float masterVolumeMult = currentVolume / 100.0f;

    float delaySamples = (currentDelayTime / 100.0f) * MAX_DELAY_SAMPLES;
    if (delaySamples < 1.0f) delaySamples = 1.0f;
    
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
      float activeCount = 0.0f; 

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
          int index = (int)voicePhases[v];
          int waveToUse = (v == MAX_VOICES) ? WAVE_SINE : currentWaveform;
          float rawSample = wavetable[waveToUse][index];

          float input = rawSample - reso * moogOut4[v]; 
          float p = filter_p[v];
          float k = filter_k[v];

          moogOut1[v] = p * (input + moogInOld[v]) - k * moogOut1[v];
          moogOut2[v] = p * (moogOut1[v] + moogOut1Old[v]) - k * moogOut2[v];
          moogOut3[v] = p * (moogOut2[v] + moogOut2Old[v]) - k * moogOut3[v];
          moogOut4[v] = p * (moogOut3[v] + moogOut3Old[v]) - k * moogOut4[v];
          moogInOld[v] = input;
          moogOut1Old[v] = moogOut1[v];
          moogOut2Old[v] = moogOut2[v];
          moogOut3Old[v] = moogOut3[v];

          float voiceOutput = moogOut4[v] * voiceAmpEnv[v] * voiceGainTaper[v];

          voiceOutput = (voiceOutput * tubeDriveGain) + tubeAsymmetry;
          voiceOutput = fastTanh(voiceOutput) - dcBlock;

          mixedLeft += voiceOutput * panLeft[v];
          mixedRight += voiceOutput * panRight[v];
          activeCount += 1.0f;
        }
      }

      float normLeft = 0.0f;
      float normRight = 0.0f;
      
      if (activeCount > 0.0f) {
        float normalization = MIX_GAIN_COMPENSATION / activeCount;
        normLeft = mixedLeft * normalization; 
        normRight = mixedRight * normalization; 
      }

      if (delayBuffer != nullptr) {
        int readIndex = (delayWriteIndex - (int)delaySamples) & DELAY_MASK;
        float delayOut = delayBuffer[readIndex];

        float monoIn = (normLeft + normRight) * 0.5f;
        float delayNext = monoIn + (delayOut * delayFdbkAmount);
        
        if (delayNext > 1.5f) delayNext = 1.5f;
        else if (delayNext < -1.5f) delayNext = -1.5f;
        
        delayBuffer[delayWriteIndex] = delayNext;
        delayWriteIndex = (delayWriteIndex + 1) & DELAY_MASK;

        normLeft += delayOut * delayMixAmount;
        normRight += delayOut * delayMixAmount;
      }

      int16_t outSampleLeft = 0;
      int16_t outSampleRight = 0;

      outSampleLeft = (int16_t)(fastTanh(normLeft) * DAC_MAX_AMPLITUDE * masterVolumeMult); 
      outSampleRight = (int16_t)(fastTanh(normRight) * DAC_MAX_AMPLITUDE * masterVolumeMult); 

      sampleBuffer[i * 2] = outSampleLeft;     
      sampleBuffer[i * 2 + 1] = outSampleRight; 

      if (controlRateCounter == 0) {
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
          float actualFreq = voiceFreqs[v] > 0.0f ? (voiceFreqs[v] * driftFactor) : 0.0f;
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

          float finalCutoff = baseCutoff + (envModAmount * voiceEnv[v]);
          if (finalCutoff > 12000.0f) finalCutoff = 12000.0f; 
          float f = finalCutoff * CUTOFF_MULT; 
          filter_p[v] = f * (1.8f - 0.8f * f);
          float fastSin = f * (1.5708f - 0.5708f * f * f); 
          filter_k[v] = 2.0f * fastSin - 1.0f;
        }
      }

      controlRateCounter++;
      if (controlRateCounter >= CONTROL_RATE_DIVIDER) controlRateCounter = 0;
    }

    i2s_write(I2S_PORT, sampleBuffer, sizeof(sampleBuffer), &bytesWritten, portMAX_DELAY);
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
  
  uint16_t lastTouchedLeft = 0;
  uint16_t lastTouchedRight = 0;
  unsigned long lastOledUpdate = 0; 
  
  int lastChordIndex = -1; 
  int previousNoteIndex = -1;

  while (true) {
    
    if (digitalRead(MPR_LEFT_IRQ_PIN) == LOW || digitalRead(MPR_RIGHT_IRQ_PIN) == LOW) {
      
      uint16_t touchedLeft = capLeft.touched();
      uint16_t touchedRight = capRight.touched();
      
      if (touchedLeft != lastTouchedLeft || touchedRight != lastTouchedRight) {
        
        uint16_t newlyTouchedRight = touchedRight & ~lastTouchedRight;
        
        if (touchedRight != 0) {
          if (newlyTouchedRight != 0) {
            for (int i = 0; i < 9; i++) { 
              if (newlyTouchedRight & (1 << rightPadPins[i])) {
                currentChordIndex = i;
                break;
              }
            }
          } else if ((touchedRight & (1 << rightPadPins[currentChordIndex])) == 0) {
            for (int i = 0; i < 9; i++) { 
              if (touchedRight & (1 << rightPadPins[i])) {
                currentChordIndex = i;
                break;
              }
            }
          }
        } else {
          currentChordIndex = 0; 
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
            
            for(int v = 0; v < MAX_VOICES; v++) {
              int noteMidi = rootMidi + chordIntervals[currentChordIndex][v];
              targetVoiceFreqs[v] = 440.0f * powf(2.0f, (noteMidi - 69) / 12.0f);
            }
            targetVoiceFreqs[MAX_VOICES] = 440.0f * powf(2.0f, ((rootMidi - 12) - 69) / 12.0f);
            
            bool isSilent = true;
            for(int v=0; v<NUM_OSC; v++) {
              if (voiceAmpEnv[v] > 0.001f) isSilent = false;
            }

            isLegato = (wasHolding && masterGate); 
            reqNewChord = true;    
            masterGate = true;     
          }
        } else {
          masterGate = false; 
        }
        
        previousNoteIndex = currentNoteIndex;
        lastChordIndex = currentChordIndex;
        lastTouchedLeft = touchedLeft;
        lastTouchedRight = touchedRight;
      }
    } 

    if (encoderDelta != 0) { 
      int direction = encoderDelta;
      encoderDelta = 0; 
      lastUserInteraction = millis(); 

      if (currentMenuState == STATE_HOME) {
        currentMenuState = STATE_CAT_SELECT; 
      } 
      else if (currentMenuState == STATE_CAT_SELECT) {
        selectedCategoryIndex += direction;
        if (selectedCategoryIndex < 0) selectedCategoryIndex = totalCategories - 1;
        if (selectedCategoryIndex >= totalCategories) selectedCategoryIndex = 0;
      } 
      else if (currentMenuState == STATE_PARAM_SELECT) {
        MenuCategory& cat = categories[selectedCategoryIndex];
        selectedParamRelativeIndex += direction;
        if (selectedParamRelativeIndex < 0) selectedParamRelativeIndex = cat.count - 1;
        if (selectedParamRelativeIndex >= cat.count) selectedParamRelativeIndex = 0;
      }
      else if (currentMenuState == STATE_EDIT) {
        MenuCategory& cat = categories[selectedCategoryIndex];
        MenuParam& p = cat.params[selectedParamRelativeIndex];
        *(p.value) = constrain(*(p.value) + direction * p.step, p.minVal, p.maxVal);
      }
    }

    bool buttonIsPressed = (digitalRead(ENCODER_SW_PIN) == LOW);
    if (buttonIsPressed && !buttonWasPressed) {
      lastUserInteraction = millis(); 
      if (currentMenuState == STATE_HOME) {
        currentMenuState = STATE_CAT_SELECT;
      }
      else if (currentMenuState == STATE_CAT_SELECT) {
        currentMenuState = STATE_PARAM_SELECT; 
        selectedParamRelativeIndex = 0; 
      }
      else if (currentMenuState == STATE_PARAM_SELECT) {
        currentMenuState = STATE_EDIT;
      }
      else if (currentMenuState == STATE_EDIT) {
        currentMenuState = STATE_PARAM_SELECT; 
      }
      vTaskDelay(200 / portTICK_PERIOD_MS); 
    }
    buttonWasPressed = buttonIsPressed;

    bool backButtonIsPressed = (digitalRead(BTN_MENU_PIN) == LOW);
    if (backButtonIsPressed && !backButtonWasPressed) {
      lastUserInteraction = millis(); 
      
      if (currentMenuState == STATE_EDIT) {
        currentMenuState = STATE_PARAM_SELECT; 
      }
      else if (currentMenuState == STATE_PARAM_SELECT) {
        currentMenuState = STATE_CAT_SELECT; 
      }
      else if (currentMenuState == STATE_CAT_SELECT) {
        currentMenuState = STATE_HOME; 
      }
      
      vTaskDelay(200 / portTICK_PERIOD_MS); 
    }
    backButtonWasPressed = backButtonIsPressed;

    if (currentMenuState != STATE_HOME && (millis() - lastUserInteraction > UI_TIMEOUT)) {
      currentMenuState = STATE_HOME;
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
        
        // Grid deformation calculation
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
        
        // Status counters
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

        // Chord overlay
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
          
          display.print(String(noteNames[currentNoteIndex]) + " " + String(chordNames[currentChordIndex]));
          
          display.setTextSize(1);
        }
      } 
      // Category list rendering
      else if (currentMenuState == STATE_CAT_SELECT) {
        int16_t x1, y1; uint16_t w, h;
        
        display.setTextSize(1, 2); 
        display.getTextBounds("MENU", 0, 0, &x1, &y1, &w, &h);
        display.setCursor((128 - w) / 2, 0);
        display.print("MENU"); 
        
        display.drawLine(0, 17, 128, 17, SSD1306_WHITE); 

        display.setTextSize(1); 
        if (totalCategories > 2) {
          int prevIdx = (selectedCategoryIndex - 1 + totalCategories) % totalCategories;
          display.getTextBounds(categories[prevIdx].name, 0, 0, &x1, &y1, &w, &h);
          display.setCursor((128 - w) / 2, 19);
          display.print(categories[prevIdx].name);
        }

        display.fillRect(0, 28, 128, 20, SSD1306_WHITE); 
        display.setTextColor(SSD1306_BLACK);
        display.setTextSize(1, 2);
        display.getTextBounds(categories[selectedCategoryIndex].name, 0, 0, &x1, &y1, &w, &h);
        display.setCursor((128 - w) / 2, 30);
        display.print(categories[selectedCategoryIndex].name);

        display.setTextColor(SSD1306_WHITE);
        display.setTextSize(1);

        if (totalCategories > 1) {
          int nextIdx = (selectedCategoryIndex + 1) % totalCategories;
          display.getTextBounds(categories[nextIdx].name, 0, 0, &x1, &y1, &w, &h);
          display.setCursor((128 - w) / 2, 50);
          display.print(categories[nextIdx].name);
        }
      }
      // Parameter list rendering
      else if (currentMenuState == STATE_PARAM_SELECT) {
        MenuCategory& cat = categories[selectedCategoryIndex];
        int count = cat.count;
        int16_t x1, y1; uint16_t w, h;
        
        const char* cleanCatName = getCleanTitle(cat.name);
        display.setTextSize(1, 2); 
        display.getTextBounds(cleanCatName, 0, 0, &x1, &y1, &w, &h);
        display.setCursor((128 - w) / 2, 0);
        display.print(cleanCatName); 
        
        display.drawLine(0, 17, 128, 17, SSD1306_WHITE);

        display.setTextSize(1);
        if (count > 2) {
          int prevIdx = (selectedParamRelativeIndex - 1 + count) % count;
          display.getTextBounds(cat.params[prevIdx].fullName, 0, 0, &x1, &y1, &w, &h);
          display.setCursor((128 - w) / 2, 19);
          display.print(cat.params[prevIdx].fullName);
        }

        display.fillRect(0, 28, 128, 20, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        display.setTextSize(1, 2);
        
        display.getTextBounds(cat.params[selectedParamRelativeIndex].fullName, 0, 0, &x1, &y1, &w, &h);
        display.setCursor((128 - w) / 2, 30);
        display.print(cat.params[selectedParamRelativeIndex].fullName);

        display.setTextColor(SSD1306_WHITE);
        display.setTextSize(1);

        if (count > 1) {
          int nextIdx = (selectedParamRelativeIndex + 1) % count;
          display.getTextBounds(cat.params[nextIdx].fullName, 0, 0, &x1, &y1, &w, &h);
          display.setCursor((128 - w) / 2, 50);
          display.print(cat.params[nextIdx].fullName);
        }
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
          display.println("FW: v0.0.0");
          display.println("BY: Alexandre Esnard");
          display.println("YT: @alexndreee"); 
          display.println("GH: /Alxdreee"); 
          display.println("RD: /u/Alxdreee");
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

          String valStr = "";
          String minStr = "";
          String maxStr = "";

          if (p.format == FMT_WAVEFORM) {
            valStr = String(waveformNames[*(p.value)]).substring(0, 3); 
            minStr = "SIN";
            maxStr = "SAW";
          } 
          else if (p.format == FMT_CUTOFF) {
            valStr = String(50 + (int)((*(p.value) / 100.0f) * 7950.0f));
            minStr = "50Hz";
            maxStr = "8000Hz";
          }
          else if (p.format == FMT_PERCENT) {
            valStr = String(*(p.value));
            minStr = "0%";
            maxStr = "100%";
          }
          else if (p.format == FMT_RESO) {
            valStr = String(*(p.value) * 10);
            minStr = "0%";
            maxStr = "100%";
          }
          else {
            valStr = String(*(p.value));
            minStr = String(p.minVal);
            maxStr = String(p.maxVal);
          }

          display.setTextSize(1);
          display.setCursor(36, 22);
          display.print(minStr);
          
          display.getTextBounds(maxStr, 0, 0, &x1, &y1, &w, &h);
          display.setCursor(128 - w, 22); 
          display.print(maxStr);

          display.setCursor(0, 22);
          display.print("VAL");
          
          if (valStr.length() > 3 || p.format == FMT_WAVEFORM) {
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
// 12. FATAL ERROR HANDLING
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
// 13. SYSTEM SETUP 
// ==============================================================================
void setup() {
  #ifdef LED_BUILTIN
  pinMode(LED_BUILTIN, OUTPUT);
  neopixelWrite(LED_BUILTIN, 0, 0, 0); 
  #endif

  display.setTextWrap(false);
  
  Serial.begin(115200);
  delay(500); 

  delayBuffer = (volatile float*)ps_calloc(DELAY_BUFFER_SIZE, sizeof(float));
  if (delayBuffer == nullptr) {
    delayBuffer = (volatile float*)calloc(DELAY_BUFFER_SIZE, sizeof(float));
    if (delayBuffer == nullptr) {
      fatalError("Memory Allocation Failed (PSRAM/SRAM full)");
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
  Wire.setClock(100000);                    
  Wire.setTimeOut(20);                      

  Wire1.begin(I2C1_SDA_PIN, I2C1_SCL_PIN);  
  Wire1.setClock(100000);                   
  Wire1.setTimeOut(20);                     

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    fatalError("OLED Display (SSD1306) not found");
  }
  displayInitialized = true; 
  display.setTextWrap(false); 

  const uint8_t TOUCH_THRESH = 100;  
  const uint8_t RELEASE_THRESH = 50; 

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

  delay(20);
  digitalWrite(I2S_XSMT_PIN, HIGH); 

  xTaskCreatePinnedToCore(bootAnimationTask, "BootAnim", 4096, NULL, 1, NULL, 0);
  playBootChime();

  xTaskCreatePinnedToCore(uiTask, "UITask", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(audioTask, "AudioTask", 8192, NULL, configMAX_PRIORITIES - 1, NULL, 1);
}

void loop() {
  vTaskDelete(NULL); 
}