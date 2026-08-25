# TYPE-2 Hardware Wiring Diagram

This document details the pinout and wiring connections between the ESP32-S3 microcontroller and the peripheral components for the synthesizer. 

All logic levels are 3.3V. Ensure your power supply can provide sufficient current for the ESP32-S3 and the I2S DAC.

## 1. I2S Audio DAC (PCM5102A)
The audio engine outputs a 16-bit, 44.1kHz I2S signal.

| ESP32-S3 Pin | PCM5102A Pin | Description / Notes |
| :--- | :--- | :--- |
| `GPIO 4` | `LRCK / WS` | Word Select (Left/Right Clock) |
| `GPIO 5` | `DIN` | Data Input |
| `GPIO 6` | `BCK` | Bit Clock |
| `GPIO 7` | `XSMT` | Mute Control (High = Active, Low = Mute) |
| `GND` | `SCK` | System Clock (Tie to GND on PCM5102A) |
| `3.3V / 5V` | `VIN` | Power supply |
| `GND` | `GND` | Common Ground |

## 2. OLED Display 128x64 (I2C0)
The visual interface uses the primary I2C bus.

| ESP32-S3 Pin | SSD1306 Pin | Description / Notes |
| :--- | :--- | :--- |
| `GPIO 8` | `SDA` | I2C0 Data |
| `GPIO 9` | `SCL` | I2C0 Clock |
| `3.3V` | `VCC` | Power supply |
| `GND` | `GND` | Common Ground |

## 3. Right Touch Sensor - Chords (MPR121 on I2C0)
Shares the I2C0 bus with the OLED display. Address: `0x5A`.

| ESP32-S3 Pin | MPR121 Pin | Description / Notes |
| :--- | :--- | :--- |
| `GPIO 8` | `SDA` | I2C0 Data |
| `GPIO 9` | `SCL` | I2C0 Clock |
| `GPIO 18` | `IRQ` | Hardware Interrupt (Active Low) |
| `GND` | `ADD` | I2C Address selector (Tie to GND for 0x5A) |
| `3.3V` | `VIN` | Power supply |
| `GND` | `GND` | Common Ground |

## 4. Left Touch Sensor - Notes (MPR121 on I2C1)
Uses the secondary I2C bus to avoid address conflicts. Address: `0x5A`.

| ESP32-S3 Pin | MPR121 Pin | Description / Notes |
| :--- | :--- | :--- |
| `GPIO 1` | `SDA` | I2C1 Data |
| `GPIO 2` | `SCL` | I2C1 Clock |
| `GPIO 39` | `IRQ` | Hardware Interrupt (Active Low) |
| `GND` | `ADD` | I2C Address selector (Tie to GND for 0x5A) |
| `3.3V` | `VIN` | Power supply |
| `GND` | `GND` | Common Ground |

## 5. Main Controls (Rotary Encoder & Button)
The UI relies on internal pull-up resistors configured in the firmware (`INPUT_PULLUP`).

| ESP32-S3 Pin | Component Pin | Description / Notes |
| :--- | :--- | :--- |
| `GPIO 42` | KY-040 `CLK` | Encoder Clock (Phase A) |
| `GPIO 41` | KY-040 `DT` | Encoder Data (Phase B) |
| `GPIO 40` | KY-040 `SW` | Encoder Push Button |
| `GPIO 38` | Menu Btn `Leg 1`| Back/Menu Push Button |
| `GND` | Both `GND` | Connect Encoder GND and Btn Leg 2 to Ground |
