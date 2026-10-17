# TYPE-2 Autonomous Polyphonic Synthesizer



<img width="3840" height="2160" alt="DSCF561100_01_15_16" src="https://github.com/user-attachments/assets/325f2c29-7a08-4ffa-91c3-2aea92f83564" />



**Version:** v1.6.0  

**Author:** Alexandre Esnard  



The TYPE-2 is a microcontroller-based hybrid polyphonic synthesizer. Engineered entirely around a highly optimized DSP core, it leverages fast polynomial saturation, a dual-rate processing architecture, and analog-modeled filters to deliver a heavy, studio-quality audio signal flow. 



With the latest updates, it now acts as a standalone hardware instrument, a Native USB MIDI controller, and a direct USB Audio streaming device. Designed with accessibility in mind, it is highly affordable (around €35 in total components) and features a near-zero difficulty build process requiring minimal wiring.



## Hardware BOM

* ESP32-S3 Microcontroller (16MB Flash, PSRAM strictly required)

* PCM5102A I2S DAC Module

* SSD1306 128x64 OLED Display

* KY-040 Rotary Encoder

* 1x Tactile Navigation Button

* 2x MPR121 I2C Capacitive Touch Sensors

* Copper tape (for creating capacitive touch keys)



## Core Features

* **Hybrid DSP Engine:** Fast polynomial saturation, dual-rate architecture, and anti-aliased oscillators.

* **Signal Chain:** Trapezoidal 24dB/Oct State Variable Filter (SVF), Asymmetrical Tube Overdrive, and Studio Digital Delay.

* **USB Connectivity:** Composite USB device featuring Native MIDI Out (with Pitch Bend and CC mapping) and a custom PCM Audio Stream Bridge via Python.

* **User Interface:** Cylindrical rotary menu interface and responsive topological grid visualization.



## Setup & Installation

1. **IDE Setup:** Install the Arduino IDE or PlatformIO with ESP32 board support.

2. **Compiler Settings:** To ensure the DSP engine, USB Composite features, and display tasks run properly, configure your IDE with the following exact parameters:

   * **USB Mode:** USB-OTG (TinyUSB) / Hardware CDC and JTAG

   * **USB CDC On Boot:** Enabled

   * **CPU Frequency:** 240MHz (WiFi)

   * **Flash Mode:** QIO 80MHz

   * **Flash Size:** 16MB (128Mb)

   * **Partition Scheme:** 16M Flash (3MB APP/9.9MB FATFS)

   * **PSRAM:** OPI PSRAM

   * **Upload Speed:** 115200

3. **Libraries:** Install `Adafruit GFX`, `Adafruit SSD1306`, and `Adafruit MPR121`.

4. **USB Audio Bridge (Optional):** To stream digital audio directly to your PC without using the hardware DAC, install Python and run `pip install pyserial sounddevice`. Then, execute the provided `type2_audio_bridge.py` script.



## PCB Design



This project is proudly supported by **[PCBWay](https://www.pcbway.com/)**. 



Turning a complex breadboard prototype into a robust, noise-free audio synthesizer requires high-quality printed circuit boards. PCBWay generously sponsored the development of the TYPE-2 by providing exceptional manufacturing services. The resulting boards feature flawless traces, perfect grounding planes (crucial for audio clarity), and crisp silkscreens. 



Their rapid prototyping service is incredibly reliable and highly recommended for hardware makers, DIY synth builders, and audio engineers. If you are planning to build your own TYPE-2 or design custom electronics, I highly encourage checking out [PCBWay.com](https://www.pcbway.com/) for fast, professional, and affordable PCB fabrication.



## Roadmap

- [x] Core 7-voice polyphonic DSP engine implementation

- [x] Tube overdrive, analog slop, and delay effects

- [x] Real-time topological grid UI and hierarchical menu

- [x] Advanced firmware optimization

- [x] Improved menu navigation

- [x] Manage the creation of presets from a dedicated menu

- [x] External MIDI communication (USB/Serial)

- [x] Direct USB PCM Audio streaming (Python Bridge)

- [x] Custom PCB design

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
