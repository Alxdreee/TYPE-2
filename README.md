# TYPE-2 — Open-Source Polyphonic Touch Synthesizer (ESP32-S3)

<img width="3592" height="2021" alt="TYPE-2 synthesizer" src="https://github.com/user-attachments/assets/a070da4b-b622-49cc-8b56-b6c869f34880" />

**Firmware:** v1.6.0 · **Hardware:** V2 (released) — V3 full-SMT (in development)
**Author:** Alexandre Esnard

The TYPE-2 is an open-source, touch-capacitive polyphonic synthesizer built around the ESP32-S3. Its DSP engine, written from scratch in C++, combines fast polynomial saturation, a dual-rate processing architecture and analog-modeled filters to deliver a rich, expressive sound from a ~€35 instrument.

> Featured on **XDA Developers** (twice) and documented in a full **Instructables** Featured build guide — see [Press & Community](#press--community).

## Core Features

* **Hybrid DSP Engine:** 7-voice polyphony, fast polynomial saturation and dual-rate architecture.
* **Signal Chain:** Factorized 24dB/Oct Trapezoidal SVF Filter, Tube Overdrive, analog slop and Studio Digital Delay.
* **User Interface:** Cylindrical rotary menu, responsive topological grid visualization, preset management.
* **Connectivity:** Composite USB Device (Native USB MIDI Out + USB PCM Audio Stream) for direct DAW integration.

## Hardware Versions

| Version | Description | Status |
|---|---|---|
| **V1** | Off-the-shelf modules, hand wiring, copper-tape touch keys | Released |
| **V2** | Custom motherboard PCB carrying the modules (ESP32-S3-DevKitC-1, DAC, OLED, MPR121) | Released, Gerbers in `hardware/` |
| **V3** | Fully SMT two-board design, factory-assembled, PCB touch pads |In development, prototype stage |

### V3 — Fully SMT, factory-assembled design

V3 replaces the module stack with two custom boards designed in KiCad for turnkey PCB assembly:

* **Backplate:** ESP32-S3-WROOM-1-N16R8, PCM5102A DAC, native USB-C (programming, MIDI, audio) with ESD protection (USBLC6-2SC6), separate low-noise LDO (LP5907) for the analog audio rail, 3.5mm line output.
* **Frontplate:** 2x MPR121 (QFN) on two independent I2C buses, 24 touch keys etched directly on the PCB, no more copper tape.
* **Board-to-board:** single 2x4 header, no wiring at all.
* **Remaining hand-soldered parts:** OLED display, rotary encoder, navigation button, headers.

**Goal:** production-ready Gerber, BOM and CPL files (with LCSC part numbers, Basic parts prioritized) so that anyone can order an assembled TYPE-2 in a few clicks. These files will be published once the prototype is validated.

### V2 Hardware BOM

* ESP32-S3-DevKitC-1 board, 44 pins (ESP32-S3-WROOM-1 N16R8: 16MB Flash, 8MB OPI PSRAM **required**)
* PCM5102A I2S DAC Module
* SSD1306 128x64 I2C OLED Display
* KY-040 Rotary Encoder
* 1x Tactile Navigation Button
* 2x MPR121 I2C Capacitive Touch Sensors
* TYPE-2 V2 motherboard PCB (or breadboard + copper tape for V1)

> **V2 ESP32-S3 FORM FACTOR NOTICE:**
> The V2 PCB is routed for the standard **ESP32-S3-DevKitC-1** form factor (**44 pins, 2x22 headers, 2.54mm pitch**).
> Do **not** use 38-pin or 30-pin ESP32-S3 boards: their power and GPIO pinouts will not match and will cause short circuits. Always check your board against [`hardware/wiring_diagram.md`](./hardware/wiring_diagram.md) and [`hardware/Type2_v2.kicad_sch`](./hardware/Type2_v2.kicad_sch) before soldering.

## Repository Structure

* `enclosure/` : 3D models and CAD files for the synthesizer casing.
* `firmware/` : Main `TYPE2.ino` firmware and the `type2_audio_bridge.py` USB audio streaming script.
* `hardware/` : V2 KiCad project, wiring diagram and production-ready `Type2_v2_Gerber.zip`.
* `[hardware/V3/]` : V3 KiCad projects (backplate + frontplate) — work in progress.

## Setup & Installation

1. **IDE Setup:** Install the Arduino IDE or PlatformIO with ESP32 board support.
2. **Compiler Settings:**
   * **USB Mode:** USB-OTG (TinyUSB) or Hardware CDC and JTAG
   * **USB CDC On Boot:** Enabled
   * **CPU Frequency:** 240MHz (WiFi)
   * **Flash Mode:** QIO 80MHz
   * **Flash Size:** 16MB (128Mb)
   * **Partition Scheme:** 16M Flash (3MB APP/9.9MB FATFS)
   * **PSRAM:** OPI PSRAM
   * **Upload Speed:** 115200
3. **Libraries:** Install `Adafruit GFX`, `Adafruit SSD1306` and `Adafruit MPR121`.
4. **Wiring (V1/V2):** Follow `hardware/wiring_diagram.md` for the I2S, I2C and GPIO connections.
5. **USB Audio Bridge (optional):** Run `firmware/type2_audio_bridge.py` on your computer to stream audio over USB.

## Press & Community

* **Instructables:** [DIY €35 Polyphonic Synthesizer — the TYPE-2](https://www.instructables.com/DIY-35-Polyphonic-Synthesizer-the-TYPE-2/)
* **Reddit:** [Development log](https://www.reddit.com/user/Alxdreee/submitted/)
* **XDA Developers:** [Someone turned their ESP32 into a touch-activated polyphonic synthesizer](https://www.xda-developers.com/someone-turned-their-esp32-into-a-touch-activated-polyphonic-synthesizer/)
* **XDA Developers:** [This open-source ESP32 touch synth just got a massive upgrade](https://www.xda-developers.com/this-open-source-esp32-touch-synth-just-got-a-massive-upgrade-and-its-only-35-to-make/)

## Manufacturing & Acknowledgements

The V2 Gerber files are standard and can be ordered from any PCB manufacturer.
Thanks to **PCBWay** for sponsoring the manufacturing of the TYPE-2 V2 boards.

## Roadmap

- [x] 7-voice polyphonic DSP engine
- [x] Tube overdrive, analog slop and delay effects
- [x] Real-time topological grid UI and hierarchical menu
- [x] Firmware optimization and improved menu navigation
- [x] Preset creation menu
- [x] External MIDI communication (USB/Serial)
- [x] Direct USB PCM audio streaming (Python bridge)
- [x] Custom PCB design (V2)
- [ ] **V3 full-SMT two-board design** (schematics done, layout & prototype in progress)
- [ ] V3 one-click assembly files (Gerber / BOM / CPL)
- [ ] Enclosure for V3
- [ ] ESP32 Bluetooth integration
- [ ] Battery management for portable use

## Links & Contact

* [GitHub](https://github.com/Alxdreee)
* [YouTube](https://www.youtube.com/@alexndreee)
* [LinkedIn](https://www.linkedin.com/in/alexandreesnard/)
* [Instructables](https://www.instructables.com/DIY-35-Polyphonic-Synthesizer-the-TYPE-2/)
* Contact / collaborations: alxdreee.esn@gmail.com

## Support the Project

TYPE-2 is developed entirely on my own, in my spare time, as a student. If you like the project, you can support it here:

[Buy Me a Coffee](https://buymeacoffee.com/alexandre.esnard)

## License & Copyright

This project is a community initiative for makers and synthesizer enthusiasts.

* **Community Use:** You are free to study, modify and build upon this project for personal use.
* **Commercial Restriction:** Commercial resale of this project, its code, or hardware derivatives based directly on this architecture is prohibited.
* **Patent Restriction:** No patents, trademarks or restrictive IP rights may be filed on this work or its derivatives.
