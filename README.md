<div align="center">
  <h1>TYPE 2 SYNTH</h1>
  <p><i>A Standalone ESP32-S3 Polyphonic Synthesizer</i></p>

  <img src="https://img.shields.io/badge/Status-Prototyping-orange?style=flat-square" alt="Status">
  <img src="https://img.shields.io/badge/MCU-ESP32--S3-blue?style=flat-square" alt="MCU">
  <img src="https://img.shields.io/badge/Audio-I2S_44.1kHz-brightgreen?style=flat-square" alt="Audio">
</div>

<br>

> **Note:** This synthesizer started as a physics and engineering school assignment and slowly evolved into a full-blown obsession with digital audio and hardware design.

## The Concept

Instead of standard mechanical keys, I am building a custom capacitive touch interface using copper tape and MPR121 sensors. The layout features 12 notes for melodies and 9 dedicated touchpads to trigger chords and control their qualities (Major, Minor, 7ths, Diminished, etc.).

Under the hood, all the digital signal processing is handled from scratch by the ESP32. 

<details>
<summary><b>Click to expand the Audio Engine Specs</b></summary>
<br>
<ul>
  <li>An 8-voice polyphonic engine with four selectable waveforms.</li>
  <li>A State Variable Filter (SVF) with a dedicated decay envelope.</li>
  <li>A built-in arpeggiator with multiple dynamic modes (Up, Down, Bounce).</li>
  <li>A complete effects chain featuring Reverb, Delay, Chorus, Stereo Widening, Tube Drive, and an "Analog Slop" modifier.</li>
  <li>A OLED display that acts as both a menu interface and a real-time oscilloscope.</li>
</ul>
</details>

## Current Status: The Breadboard Phase

Right now, the project is a beautiful mess of jumper wires on my desk. The core audio DSP and the capacitive touch logic are up and running smoothly.

**Roadmap:**
- [ ] Translate the breadboard circuit into a clean PCB using KiCad.
- [ ] Design and 3D print a portable, ergonomic enclosure.
- [ ] Release the full source code and hardware design files.

---
*If you are into synth DIY, DSP coding, or just want to see how this instrument comes to life, please hit the **Star** button at the top of the page!*
