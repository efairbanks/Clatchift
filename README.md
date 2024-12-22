# Clatchift

A ratcheting, phase-shiftable momentary clock alt-firmware for the Music Thing Modular "Chord Organ/Radio Music" Eurorack module. See the [Chord Organ](https://www.musicthing.co.uk/Chord-Organ/) faceplate for reference.

## Clock / Ratchet / Shift

The ROOT input is the clock input.

The CHORD knob and CV control the clock rate. Noon passes through the input unaffected. Right of noon mulitiplies, left of noon divides.

The ROOT knob controls the phase of the output clock signal.

All clocks are converted to square waves.

Clock pulses are only allowed through when the button is held.

Parameter changes only take affect on incoming rising edges.

TRIG and OUT are the output clock signal. The voltage on TRIG is rather low, and may not register as a logic high on all modules.

This firmware runs in a 48kHz control loop with no buffering and thus is capable of processing and producing audio-rate signals.
