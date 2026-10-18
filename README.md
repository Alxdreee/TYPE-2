# TYPE-2 Autonomous Polyphonic Synthesizer

<img width="3592" height="2021" alt="17102026-DSCF561100_01_15_16" src="https://github.com/user-attachments/assets/a070da4b-b622-49cc-8b56-b6c869f34880" />


**Version:** v1.6.0

**Author:** Alexandre Esnard


The TYPE-2 is a microcontroller-based hybrid polyphonic synthesizer. Engineered entirely around a highly optimized DSP core, it leverages fast polynomial saturation, a dual-rate processing architecture, and analog-modeled filters to deliver a heavy, studio-quality audio signal flow.

Designed with accessibility in mind, it is highly affordable (around €35 in total components) and features a near-zero difficulty build process requiring minimal wiring.



## Hardware BOM



* ESP32-S3 Microcontroller (16MB Flash, PSRAM strictly required)

* PCM5102A I2S DAC Module

* SSD1306 128x64 OLED Display

* KY-040 Rotary Encoder

* 1x Tactile Navigation Button

* 2x MPR121 I2C Capacitive Touch Sensors

* Copper tape (for creating capacitive touch keys)



## Core Features

* **Hybrid DSP Engine:** Fast polynomial saturation and dual-rate architecture.

* **Signal Chain:** Factorized 24dB/Oct Trapezoidal SVF Filter, Tube Overdrive, and Studio Digital Delay.

* **User Interface:** Cylindrical rotary menu interface and responsive topological grid visualization.

* **Connectivity:** Composite USB Device (Native USB MIDI Out + USB PCM Audio Stream) for direct DAW integration.


## Repository Structure



* `enclosure/` : 3D models and CAD files for the synthesizer casing.

* `firmware/` : Contains the main `TYPE2.ino` firmware file and the `type2_audio_bridge.py` script for USB Audio streaming.

* `hardware/` : Contains the KiCad project files (`.kicad_pro`, `.kicad_sch`, `.kicad_pcb`), the schematic/wiring diagram, and a ready-to-manufacture `Type2_v2_Gerber.zip` file.



## Setup & Installation



1. **IDE Setup:** Install the Arduino IDE or PlatformIO with ESP32 board support.

2. **Compiler Settings:** To ensure the DSP engine, display tasks, and USB native features run properly, configure your IDE with the following exact parameters:

   * **USB Mode:** USB-OTG (TinyUSB) or Hardware CDC and JTAG

   * **USB CDC On Boot:** Enabled

   * **CPU Frequency:** 240MHz (WiFi)

   * **Flash Mode:** QIO 80MHz

   * **Flash Size:** 16MB (128Mb)

   * **Partition Scheme:** 16M Flash (3MB APP/9.9MB FATFS)

   * **PSRAM:** OPI PSRAM

   * **Upload Speed:** 115200

3. **Libraries:** Install `Adafruit GFX`, `Adafruit SSD1306`, and `Adafruit MPR121`.

4. **Wiring:** Refer to the `wiring_diagram.md` in the `hardware/` folder to route the I2S, I2C, and GPIO connections correctly.

5. **USB Audio Bridge (Optional):** To stream audio directly over USB, run the `type2_audio_bridge.py` script located in the `firmware/` folder on your host machine.



## PCB Design



This project is proudly supported by **PCBWay**, who generously provided the manufacturing for the TYPE-2 printed circuit boards. Their service offers exceptional quality, which is crucial for audio applications where proper ground planes, trace routing, and impedance control directly impact the final sound quality.



The silkscreen precision on the touch pads and the fast turnaround times have been instrumental in the prototyping and finalization of this synthesizer.

If you want to build your own TYPE-2, you can use the production-ready `Type2_v2_Gerber.zip` file located in the `hardware/` folder. I highly recommend ordering your boards from PCBWay for guaranteed compatibility and professional finish.

### Hardware BOM
* **ESP32-S3 Microcontroller Board** (ESP32-S3-DevKitC-1 form factor, 16MB Flash, OPI PSRAM required)
* PCM5102A I2S DAC Module
* SSD1306 128x64 I2C OLED Display
* KY-040 Rotary Encoder
* 1x Tactile Navigation Button
* 2x MPR121 I2C Capacitive Touch Sensors
* Custom TYPE-2 Motherboard PCB (or breadboard + copper tape)

> **CRITICAL ESP32-S3 PINOUT & FORM FACTOR NOTICE:**  
> The custom PCB footprint is specifically routed for the standard **ESP32-S3-DevKitC-1** form factor featuring **44 pins (2x22 pin headers, 2.54mm pitch)** and an onboard **16MB Flash / 8MB OPI PSRAM (e.g., ESP32-S3-WROOM-1 / N16R8 module)**.  
> Do **not** use 38-pin or compact 30-pin ESP32-S3 boards, as their power and GPIO pinouts will not align with the PCB socket and will cause short circuits. Before soldering, always verify your board's pinout against [`hardware/wiring_diagram.md`](./hardware/wiring_diagram.md) and the KiCad schematic [`hardware/Type2_v2.kicad_sch`](./hardware/Type2_v2.kicad_sch).



## Roadmap



- [x] Core 7-voice polyphonic DSP engine implementation

- [x] Tube overdrive, analog slop, and delay effects

- [x] Real-time topological grid UI and hierarchical menu

- [x] Advanced firmware optimization

- [x] Improved menu navigation

- [x] Manage the creation of presets from a dedicated menu

- [x] External MIDI communication (USB/Serial)

- [x] Custom PCB design

- [x] Direct USB PCM Audio streaming (Python Bridge)

- [ ] ESP32 Bluetooth communication integration

- [ ] Battery management and portable power components integration

- [ ] Enclosure design and manufacturing



## Links & Contact



* [GitHub Repository](https://github.com/Alxdreee)

* [YouTube Channel](https://www.youtube.com/@alexndreee)

* [LinkedIn Profile](https://www.linkedin.com/in/alexandreesnard/)

* [Reddit Development Log](https://www.reddit.com/user/Alxdreee/submitted/)

* [Instructables](https://www.instructables.com/DIY-35-Polyphonic-Synthesizer-the-TYPE-2/)

* alxdreee.esn@gmail.com



## Support the Project | Donation Page



This project is 100% open-source and developed entirely on my own in my spare time. If you find it useful or like what I'm doing, you can support its development here!



[Buy Me a Coffee](https://buymeacoffee.com/alexandre.esnard)



## License & Copyright



This project is an open-source initiative developed for the benefit of the maker and synthesizer community. 



* **Community Use:** You are entirely free to study, modify, and build upon this code for your personal projects.

* **Commercial Restriction:** The commercial resale of this project, its code, or any hardware derivatives based directly upon this architecture is strictly prohibited. 

* **Patent Restriction:** No patents, trademarks, or restrictive intellectual property rights may be filed on this work or its derivatives.
