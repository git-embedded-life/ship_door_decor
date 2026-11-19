#include <Adafruit_NeoPixel.h>
#include <driver/rtc_io.h>

#define RUN_TIME_S          10
#define SENSOR_BOOT_S       6
#define BATT_KEEPALIVE_S    25
#define KEEPALIVE_PULSE_MS  100

#define PIXEL_POWER_PIN   GPIO_NUM_0
#define PIXEL_DATA_PIN    GPIO_NUM_1
#define PIXEL_NUM_LEDS    28
#define SENSOR_POWER_PIN  GPIO_NUM_21
#define SENSOR_INPUT_PIN  GPIO_NUM_2

#define SENSOR_ON()       pinMode(SENSOR_POWER_PIN, OUTPUT); digitalWrite(SENSOR_POWER_PIN, 0); digitalWrite(LED_BUILTIN, 0);
#define SENSOR_OFF()      pinMode(SENSOR_POWER_PIN, INPUT); digitalWrite(LED_BUILTIN, 1);
#define PIXEL_ON()        pinMode(PIXEL_POWER_PIN, OUTPUT); digitalWrite(PIXEL_POWER_PIN, 0);
#define PIXEL_OFF()       pinMode(PIXEL_POWER_PIN, INPUT);

#define LED_BOTTOM_START  26
#define LED_BOTTOM_END    9
#define LED_TOP_START     10
#define LED_TOP_STOP      25

#define DEBUG

#define BRIGHTNESS        25

const uint8_t pixel_brightness[PIXEL_NUM_LEDS] = {
  BRIGHTNESS,
  BRIGHTNESS,
  BRIGHTNESS,
  BRIGHTNESS,
  BRIGHTNESS,
  BRIGHTNESS,
  BRIGHTNESS,
  BRIGHTNESS,
  BRIGHTNESS,
  BRIGHTNESS + 25,
  BRIGHTNESS + 50,
  BRIGHTNESS + 50,
  BRIGHTNESS + 25,
  BRIGHTNESS,
  BRIGHTNESS,
  BRIGHTNESS,
  BRIGHTNESS,
  BRIGHTNESS,
  BRIGHTNESS,
  BRIGHTNESS,
  BRIGHTNESS,
  BRIGHTNESS,
  BRIGHTNESS,
  BRIGHTNESS + 25,
  BRIGHTNESS + 50,
  BRIGHTNESS + 50,
  BRIGHTNESS + 25,
  BRIGHTNESS,
};

#define PIXEL_COLOR(n, r, g, b)       strip.Color((r * pixel_brightness[n]) >> 8, (g * pixel_brightness[n]) >> 8, (b * pixel_brightness[n]) >> 8)

#define COLOR_SKY(n)        PIXEL_COLOR(n, 252, 144, 8)
#define COLOR_SEA(n)        PIXEL_COLOR(n, 0, 88, 255)

typedef enum {
  STATE_Initial,
  STATE_StartAnimation,
  STATE_AnimationSensorOff,
  STATE_AnimationSensorOn,
  STATE_StartKeepAlive,
  STATE_RunKeepAlive,
  STATE_Sleep,
} state_e;

typedef struct {
  void (*start)(void);
  void (*run)(void);
} pattern_fns_t;


RTC_DATA_ATTR unsigned char _selectedPattern = 0;
Adafruit_NeoPixel strip(PIXEL_NUM_LEDS, PIXEL_DATA_PIN, NEO_GRB + NEO_KHZ800);
state_e state;



void setup() {
  state = STATE_Initial;
}


bool LED_keepalive_Run() {
  RTC_DATA_ATTR static unsigned char _random_led = 0;
  static unsigned long pulseStart = 0;

  if (pulseStart == 0) {
    pulseStart = millis();
    if (_random_led >= PIXEL_NUM_LEDS) _random_led = 0;

    // Pulse a pair of LEDs briefly to guarantee sufficient load (~80-100mA total with ESP32)
    // to reset the power bank auto-off timer while minimizing active awake time.
    strip.clear();
    strip.setPixelColor(_random_led, strip.Color(120, 120, 120));
    strip.setPixelColor((_random_led + 1) % PIXEL_NUM_LEDS, strip.Color(120, 120, 120));
    strip.show();
  }

  if (millis() - pulseStart >= KEEPALIVE_PULSE_MS) {
    strip.clear();
    strip.show();
    pulseStart = 0;
    _random_led = (_random_led + 2) % PIXEL_NUM_LEDS;
    return false; // Pulse complete, ready to return to sleep
  }

  return true;
} 


void LED_allWhite_Start(void) {
  for (int i = 0; i < PIXEL_NUM_LEDS; i++) {
    strip.setPixelColor(i, PIXEL_COLOR(i, 255, 255, 255));
  }
  strip.show();
}


void LED_allWhite_Run(void) {
  static unsigned long lastUpdate = 0;

  if (millis() - lastUpdate < 20) return; // ~50 fps
  lastUpdate = millis();

  unsigned long ms = millis();
  for (int i = 0; i < PIXEL_NUM_LEDS; i++) {
    // Breathing base white glow (sine wave common to all LEDs)
    float breath = 0.5 + 0.5 * sin(ms / 1000.0);
    
    // Twinkling stars effect (spatial phase shift)
    float stars = 0.5 + 0.5 * sin(ms / 300.0 + i * 2.3);
    float shimmer = (stars > 0.85) ? (stars - 0.85) * 1.5 : 0.0;

    // Scale color values
    uint8_t baseVal = 180 * breath;
    uint8_t r = min(255, baseVal + (int)(shimmer * 75));
    uint8_t g = min(255, baseVal + (int)(shimmer * 75));
    uint8_t b = min(255, baseVal + (int)(shimmer * 75));

    strip.setPixelColor(i, PIXEL_COLOR(i, r, g, b));
  }
  strip.show();
}


void LED_allBlue_Start(void) {
  for (int i = 0; i < PIXEL_NUM_LEDS; i++) {
    strip.setPixelColor(i, PIXEL_COLOR(i, 0, 0, 255));
  }
  strip.show();
}


void LED_allBlue_Run(void) {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < 30) return; // ~33 fps
  lastUpdate = millis();

  unsigned long ms = millis();
  for (int i = 0; i < PIXEL_NUM_LEDS; i++) {
    // Traveling wave moving down the strip
    float wave = 0.5 + 0.5 * sin((ms / 500.0) - (i * 0.45));
    
    // Interpolate between deep ocean blue (0, 0, 255) and bright cyan/aqua (0, 160, 230)
    uint8_t r = 0;
    uint8_t g = 160 * wave;
    uint8_t b = 255 - (25 * wave); // wave crests are slightly lighter cyan/teal

    strip.setPixelColor(i, PIXEL_COLOR(i, r, g, b));
  }
  strip.show();
}


void LED_splitOceanSky_Start(void) {
  for (int i = 0; i < PIXEL_NUM_LEDS; i++) {
    if (i >= LED_TOP_START && i <= LED_TOP_STOP) {
      strip.setPixelColor(i, COLOR_SKY(i));
    }
    else {
      strip.setPixelColor(i, COLOR_SEA(i));
    }
  }
  strip.show();
}


void LED_splitOceanSky_Run(void) {
  static unsigned long lastUpdate = 0;

  if (millis() - lastUpdate < 30) return; // ~33 fps
  lastUpdate = millis();

  unsigned long ms = millis();
  for (int i = 0; i < PIXEL_NUM_LEDS; i++) {
    if (i >= LED_TOP_START && i <= LED_TOP_STOP) {
      // --- SKY PATTERN (Sunset shift with star twinkle) ---
      // Shift from warm orange (252, 120, 8) to magenta/rose (210, 30, 120) over time
      float colorShift = 0.5 + 0.5 * sin(ms / 1500.0);
      
      // Twinkling stars overlay
      float stars = 0.5 + 0.5 * sin(ms / 250.0 + i * 1.8);
      float shimmer = (stars > 0.88) ? (stars - 0.88) * 1.5 : 0.0;

      uint8_t r = 252 * (1.0 - colorShift) + 210 * colorShift;
      uint8_t g = 120 * (1.0 - colorShift) + 30 * colorShift;
      uint8_t b = 8 * (1.0 - colorShift) + 120 * colorShift;

      r = min(255, r + (int)(shimmer * 70));
      g = min(255, g + (int)(shimmer * 70));
      b = min(255, b + (int)(shimmer * 110));

      strip.setPixelColor(i, PIXEL_COLOR(i, r, g, b));
    }
    else {
      // --- OCEAN PATTERN (Sloshing wave motion) ---
      // Traveling wave modulated by a secondary slow frequency for a realistic sloshing effect
      float wave = 0.5 + 0.5 * sin((ms / 600.0) + (i * 0.6) + 0.4 * sin(ms / 1300.0));
      
      // Interpolate between deep ocean blue (0, 30, 180) and seafoam green/aqua (0, 150, 240)
      uint8_t r = 0;
      uint8_t g = 30 + (120 * wave);
      uint8_t b = 180 + (60 * wave);

      strip.setPixelColor(i, PIXEL_COLOR(i, r, g, b));
    }
  }
  strip.show();
}


const pattern_fns_t pattern_fns[] = {
  {LED_allWhite_Start, LED_allWhite_Run},
  {LED_allBlue_Start, LED_allBlue_Run},
  {LED_splitOceanSky_Start, LED_splitOceanSky_Run}
};


void led_start(void) {
  if (pattern_fns[_selectedPattern].start) pattern_fns[_selectedPattern].start();
}


void led_run(void) {
  if (pattern_fns[_selectedPattern].run) pattern_fns[_selectedPattern].run();
}


void led_stop(void) {
  strip.clear();
  strip.show();
}

void led_cycle_pattern(void) {
  _selectedPattern++;
  if (_selectedPattern >= (sizeof(pattern_fns)/sizeof(pattern_fns_t))) {_selectedPattern = 0;}
}


void loop() {
  static unsigned long runStartTime;
  static esp_sleep_wakeup_cause_t cause;
  
  switch (state) {
    case STATE_Initial:
      cause = esp_sleep_get_wakeup_cause();

      pinMode(LED_BUILTIN, OUTPUT);
      pinMode(SENSOR_INPUT_PIN, INPUT);
      
      switch (cause) {
        case ESP_SLEEP_WAKEUP_TIMER:
          state = STATE_StartKeepAlive;
          break;

        default:
        case ESP_SLEEP_WAKEUP_EXT1:
          state = STATE_StartAnimation;
          break;
      }

      strip.begin();
      break;

    case STATE_StartKeepAlive:
      state = STATE_RunKeepAlive;
      gpio_hold_dis(PIXEL_POWER_PIN);
      PIXEL_ON();
      break;

    case STATE_RunKeepAlive:
      if (!LED_keepalive_Run()) {
        if (digitalRead(SENSOR_INPUT_PIN)) {
          state = STATE_StartAnimation;
        }
        else {
          state = STATE_Sleep;
        }
      }
      break;

    case STATE_StartAnimation:
      state = STATE_AnimationSensorOff;
      runStartTime = millis();

      gpio_hold_dis(SENSOR_POWER_PIN);
      gpio_hold_dis(SENSOR_INPUT_PIN);
      gpio_hold_dis(PIXEL_POWER_PIN);
      SENSOR_OFF();
      PIXEL_ON();

      led_cycle_pattern();
      led_start();
      break;

    case STATE_AnimationSensorOff:
      led_run();

      if ((millis() - runStartTime) >= ((RUN_TIME_S - SENSOR_BOOT_S) * 1000)) {
        state = STATE_AnimationSensorOn;
        SENSOR_ON();
        strip.show();
      }
      break;

    case STATE_AnimationSensorOn:
      led_run();

      if ((millis() - runStartTime) >= (RUN_TIME_S * 1000)) {
        led_stop();
        
        if (digitalRead(SENSOR_INPUT_PIN)) {
          state = STATE_StartAnimation;
        }
        else {
          state = STATE_Sleep;
        }
      }
      break;

    case STATE_Sleep:
      SENSOR_ON();
      PIXEL_OFF();
      gpio_hold_en(SENSOR_POWER_PIN);
      gpio_hold_en(SENSOR_INPUT_PIN);
      gpio_hold_en(PIXEL_POWER_PIN);
      esp_sleep_enable_ext1_wakeup((1ULL << SENSOR_INPUT_PIN), ESP_EXT1_WAKEUP_ANY_HIGH);
      esp_sleep_enable_timer_wakeup(BATT_KEEPALIVE_S * 1000000ULL);

      esp_deep_sleep_start();
      break;
  }
}