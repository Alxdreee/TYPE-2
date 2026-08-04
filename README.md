<img width="4075" height="2707" alt="04082026-DSCF3199" src="https://github.com/user-attachments/assets/cd1b3b7e-aa40-4ded-a41d-b8b56810e9c8" />
<div align="center">
  <h1>TYPE 2 SYNTH</h1>
  <p><i>A Standalone ESP32-S3 Polyphonic Synthesizer</i></p>

  <img src="https://img.shields.io/badge/Status-Prototyping-orange?style=flat-square" alt="Status">
  <img src="https://img.shields.io/badge/MCU-ESP32--S3-blue?style=flat-square" alt="MCU">
  <img src="https://img.shields.io/badge/Audio-I2S_44.1kHz-brightgreen?style=flat-square" alt="Audio">
</div>

<br>
  
![Uploading 04082026-DSCF3199.png…]()


> **Note:** This synthesizer started out as a school project as part of my studies in physics and engineering, and gradually evolved into a true passion for digital audio and audio hardware design.

## The Concept

Instead of standard mechanical keys, I am building a custom capacitive touch interface using copper tape and MPR121 sensors. The layout features 12 notes for melodies and 9 dedicated touchpads to trigger chords and control their qualities (Major, Minor, 7ths, Diminished, etc.).

Under the hood, all the digital signal processing is handled from scratch by the ESP32. 

<details>
<summary><b>Click to expand the Audio Engine Specs</b></summary>
<br>
<ul>
  <li>An 8-voice polyphonic engine with four selectable waveforms.</li>
  <li>A State Variable Filter with a dedicated decay envelope.</li>
  <li>A built-in arpeggiator with multiple dynamic modes (Up, Down, Bounce).</li>
  <li>A complete effects chain featuring Reverb, Delay, Chorus, Stereo Widening, Tube Drive, and an "Analog Slop" modifier.</li>
  <li>An OLED display that acts as both a menu interface and a real-time oscilloscope.</li>
</ul>
</details>

<details>
<summary><b>Click to expand the Hardware & Components (BOM)</b></summary>
<br>
<p>The project is currently transitioning from a standard ESP32 development board to a more powerful, battery-operated ESP32-S3 architecture.</p>
<b>Core & Processing:</b>
<ul>
  <li><b>MCU:</b> Moving to ESP32-S3 N16R8 (16MB Flash, 8MB PSRAM) for heavy DSP handling.</li>
  <li><b>Audio:</b> PCM5102A I2S DAC Decoder Module for clean, high-fidelity stereo output.</li>
</ul>
<b>Interface & Controls:</b>
<ul>
  <li><b>Sensors:</b> 2x Adafruit MPR121 I2C Capacitive Touch controllers.</li>
  <li><b>Keyboard:</b> Custom-cut conductive copper adhesive tape.</li>
  <li><b>Display:</b> 0.96" 128x64 OLED Display (SSD1306 I2C).</li>
  <li><b>Navigation:</b> 1x Rotary Encoder (with push-button switch) and 1x discrete tactile button for menu navigation.</li>
  <li><b>Volume:</b> 1x Analog Potentiometer (read via ADC).</li>
</ul>
<b>Power Management (Upcoming):</b>
<ul>
  <li><b>Battery:</b> 3.7V LiPo / 18650 cell.</li>
  <li><b>Charger:</b> TP4056 Lithium Battery Charger Module (Type-C).</li>
  <li><b>Power Regulation:</b> ND0205MA DC-DC Step-Up Boost Converter (0.8V-5V to 3.3V/5V) for stable audio and logic power.</li>
  <li><b>Main Switch:</b> 12mm 1NO Self-locking waterproof metal push button.</li>
  <li><b>Voltage Divider:</b> 2x 100k Ohm resistors to safely monitor battery voltage via ESP32 ADC.</li>
</ul>
</details>

<details>
<summary><b>Click to expand the Prototyping Tools</b></summary>
<br>
<ul>
  <li><b>Breadboards:</b> Multiple standard breadboards joined together to accommodate the wide 44-pin ESP32-S3 format.</li>
  <li><b>Wiring:</b> Assorted Dupont jumper wires (Male-Male, Male-Female).</li>
  <li><b>Soldering:</b> Basic soldering iron (used to secure wires to the back of the copper tape and the main power switch).</li>
  <li><b>Software:</b> Arduino IDE / C++ environment.</li>
</ul>
</details>

## Current Status: The Breadboard Phase

Right now, the project is a beautiful mess of jumper wires on my desk. The core audio DSP and the capacitive touch logic are up and running smoothly on a V1 prototype. I am currently waiting for the delivery of the new power management modules and the S3 processor to build the V2 architecture.

**Roadmap:**
- [X] Create an initial stable prototype that demonstrates the general concept
- [ ] Replace the ESP32D board with an ESP32-S3 board.
- [ ] Add a battery for greater portability.
- [ ] Add MIDI communication support.
- [ ] Create a Bluetooth interface between a smartphone and the ESP32 board to adjust the instrument’s settings.
- [ ] Convert the wiring diagram into a clean PCB layout in KiCad.
- [ ] Design and 3D-print a portable, ergonomic enclosure.
- [ ] Publish the final source code along with the hardware design files.
- [ ] Find a partner to mass-produce the instrument and offer it for sale.

---
*If you are into synth DIY, DSP coding, or just want to see how this instrument comes to life, please hit the **Star** button at the top of the page!*
