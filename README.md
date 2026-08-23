# TYPE-2 Autonomous Polyphonic Synthesizer

**Version:** v0.0.0  
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
* Hybrid DSP Engine: Fast polynomial saturation and dual-rate architecture.
* Signal Chain: Factorized 24dB/Oct Moog Ladder Filter, Tube Overdrive, and Studio Digital Delay.
* User Interface: Cylindrical rotary menu interface and responsive topological grid visualization.

## Setup & Installation
1. **IDE Setup:** Install the Arduino IDE or PlatformIO with ESP32 board support.
2. **Compiler Settings:** To ensure the DSP engine and display tasks run properly, configure your IDE with the following exact parameters:
   * **USB CDC On Boot:** Enabled
   * **CPU Frequency:** 240MHz (WiFi)
   * **Flash Mode:** QIO 80MHz
   * **Flash Size:** 16MB (128Mb)
   * **Partition Scheme:** 16M Flash (3MB APP/9.9MB FATFS)
   * **PSRAM:** OPI PSRAM
   * **Upload Speed:** 115200
3. **Libraries:** Install `Adafruit GFX`, `Adafruit SSD1306`, and `Adafruit MPR121`.
4. **Wiring:** Refer to the provided wiring diagram schematic to route the I2S, I2C, and GPIO connections correctly.

## Roadmap
- [x] Core 7-voice polyphonic DSP engine implementation
- [x] Tube overdrive, analog slop, and delay effects
- [x] Real-time topological grid UI and hierarchical menu
- [ ] Advanced firmware optimization
- [ ] Manage the creation of presets from a dedicated menu
- [ ] External MIDI communication (USB/Serial)
- [ ] ESP32 Bluetooth communication integration
- [ ] Battery management and portable power components integration
- [ ] Custom PCB design
- [ ] Enclosure design and manufacturing

## License & Copyright
This project is an open-source initiative developed for the benefit of the maker and synthesizer community. 
* **Community Use:** You are entirely free to study, modify, and build upon this code for your personal projects.
* **Commercial Restriction:** The commercial resale of this project, its code, or any hardware derivatives based directly upon this architecture is strictly prohibited. 
* **Patent Restriction:** No patents, trademarks, or restrictive intellectual property rights may be filed on this work or its derivatives. 

## Links & Contact
* [GitHub Repository](https://github.com/Alxdreee)
* [YouTube Channel](https://www.youtube.com/@alexndreee)
* [LinkedIn Profile](https://www.linkedin.com/in/alexandreesnard/)
* [Reddit Development Log](https://www.reddit.com/user/Alxdreee/submitted/)
* alexandre.esnard006@gmail.com
