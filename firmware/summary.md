# Disney Door Decor Firmware Analysis

This Arduino sketch ([Disney_Door_Decor.ino](file:///C:/Users/jeram/Git%20Repos/ship_door_decor/firmware/Disney_Door_Decor.ino)) implements firmware for a battery-powered, motion-activated illuminated Disney cruise cabin door decoration running on an **ESP32** microcontroller driving a 28-LED WS2812 NeoPixel strip.

---

## 1. Hardware Pinout & Configuration

* **Microcontroller**: ESP32 (utilizes ESP32 RTC slow memory, RTC GPIOs, and deep sleep APIs).
* **LED Power Control**: `GPIO_NUM_0` (`PIXEL_POWER_PIN`) controls a power switch/transistor to cut standby power to the LED strip completely during sleep.
* **LED Data**: `GPIO_NUM_1` (`PIXEL_DATA_PIN`), driving 28 NeoPixels.
* **Sensor Power & Input**: 
  * `GPIO_NUM_21` (`SENSOR_POWER_PIN`) powers the motion/presence sensor.
  * `GPIO_NUM_2` (`SENSOR_INPUT_PIN`) receives active-HIGH sensor trigger events.
* **Custom Brightness Mapping**: Uses a per-LED brightness table (`pixel_brightness[28]`) with elevated brightness on LEDs 9–12 and 23–26 (+25 to +50 above the baseline `25`), compensating for physical diffusion or highlighting specific areas of the decoration.

---

## 2. Power Management & Power-Bank Keep-Alive

The device leverages deep sleep to maximize battery life, waking up via two distinct triggers:

1. **Motion / Sensor Trigger (`ESP_SLEEP_WAKEUP_EXT1` on GPIO 2)**:
   * Wakes the system when a guest approaches.
   * Cycles to the next animation pattern and plays the lighting effect.
2. **Periodic Keep-Alive Timer (`ESP_SLEEP_WAKEUP_TIMER` every 25s)**:
   * **Problem solved**: Portable USB power banks shut down automatically if current draw drops too low for extended periods.
   * **Solution**: Every 25 seconds, the ESP32 briefly wakes up, powers on the LED rail, pulses a pair of LEDs for 100 ms (`KEEPALIVE_PULSE_MS`) to draw an ~80–100 mA pulse of current (`LED_keepalive_Run()`), checks the sensor, and returns to deep sleep immediately. This avoids the high energy cost of long awake periods while reliably keeping the power bank active.
3. **GPIO State Holding (`gpio_hold_en` / `gpio_hold_dis`)**:
   * Freezes pin states before sleeping to prevent floating gates or power glitches.
4. **RTC Persistent Storage**:
   * `RTC_DATA_ATTR unsigned char _selectedPattern`: Preserves the active pattern index across deep sleep cycles.
   * `RTC_DATA_ATTR static unsigned char _random_led`: Preserves the active LED index for the keep-alive pulse across sleep cycles.

---

## 3. State Machine & Execution Flow

The `loop()` function implements a non-blocking state machine with seven states:

```
[STATE_Initial]
       │
       ├─ Wakeup: Timer ──────────> [STATE_StartKeepAlive] ──> [STATE_RunKeepAlive]
       │                                                              │ (Done pulse)
       │                                                              ├── Motion detected? ──> [STATE_StartAnimation]
       │                                                              └── No Motion ─────────> [STATE_Sleep]
       │
       └─ Wakeup: Sensor / Ext1 ──> [STATE_StartAnimation] (Powers on LEDs, turns OFF sensor)
                                           │
                                           ▼
                                    [STATE_AnimationSensorOff] (Runs pattern for 4 seconds)
                                           │ (Warm up sensor 6s before end)
                                           ▼
                                    [STATE_AnimationSensorOn]  (Runs pattern remaining 6 seconds)
                                           │ (After 10s runtime)
                                           ├── Motion still HIGH? ──> [STATE_StartAnimation] (Loops/cycles)
                                           └── No Motion ────────────> [STATE_Sleep] (Deep sleep)
```

* **Sensor Power Cycling**: During the initial animation phase, the sensor power is cut (`SENSOR_OFF`) to reduce current and prevent re-triggering, then powered back on 6 seconds before the 10-second animation completes (`RUN_TIME_S - SENSOR_BOOT_S`) to allow the sensor to stabilize before checking for continuous occupancy.

---

## 4. Lighting Patterns

The animation cycles sequentially on each wake-up, tracked in RTC memory:

1. **`LED_allWhite`**:
   * Ambient breathing glow (`sin(ms / 1000.0)`) layered with sparkling/twinkling stars via spatial phase-shifted sinusoidal shimmers (`sin(ms / 300.0 + i * 2.3)`).
2. **`LED_allBlue`**:
   * Traveling wave along the strip transitioning between deep ocean blue and bright aqua/cyan wave crests.
3. **`LED_splitOceanSky`**:
   * Splits the 28 LEDs into two distinct physical zones:
     * **Sky Zone (LEDs 10–25)**: Sunset gradient slowly shifting between warm orange and magenta/rose with twinkling star overlays.
     * **Ocean Zone (LEDs 26–28 & 0–9)**: Dual-frequency wave motion simulating sloshing water between ocean blue and seafoam aqua.
