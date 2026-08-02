# TYPE-2 ESP32-S3 Synthesizer

Welcome to the TYPE 2 project. This synthesizer started as a physics and engineering school assignment (TIPE) and slowly evolved into a full-blown obsession with digital audio and hardware design.

My goal is to build a completely standalone, battery-powered polyphonic synthesizer driven by an ESP32-S3 microcontroller.



# The Concept

Instead of standard mechanical keys, I am building a custom capacitive touch interface using copper tape and MPR121 sensors. The layout features 12 notes for melodies and 9 dedicated touchpads to trigger chords and control their qualities (Major, Minor, 7ths, Diminished, etc.).

Under the hood, all the digital signal processing (DSP) is handled from scratch by the ESP32-S3. The current audio engine includes:
- An 8-voice polyphonic engine with four selectable waveforms (Sine, Square, Saw, Triangle)
- A State Variable Filter (SVF) with a dedicated decay envelope.
- A built-in arpeggiator with multiple dynamic modes (Up, Down, Bounce).
- A complete effects chain featuring Reverb, Delay, Chorus, Stereo Widening, Tube Drive, and an "Analog Slop" modifier to add some organic lo-fi warmth.
- A OLED display that acts as both a menu interface and a real-time oscilloscope.



# Current Status

Right now, the project is a mess of jumper wires on my desk. The core audio DSP and the capacitive touch logic are up and running smoothly.

Here is the roadmap for the upcoming months:
- Translating the breadboard circuit into a clean PCB using KiCad.
- Designing and 3D printing a portable, ergonomic enclosure.
- Releasing the full source code and hardware design files.

If you are into synth DIY, DSP coding, or just want to see how this instrument comes to life, please hit the Star button at the top of the page. It is a huge motivation booster, and it is the best way to get notified when I finally publish the code and schematics!
