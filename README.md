# Clatchift

A ratcheting, phase-shiftable momentary clock alt-firmware for the Music Thing Modular "Chord Organ/Radio Music" Eurorack module. See the [Chord Organ](https://www.musicthing.co.uk/Chord-Organ/) faceplate for reference.

## Clock / Ratchet / Shift

The ROOT input is the clock input.

The CHORD knob and CV control the clock rate. Noon passes through the input unaffected. Right of noon mulitiplies, left of noon divides.

The ROOT knob controls the phase of the output clock signal.

All clocks are converted to square waves.

Clock pulses are only allowed through when the button is held.

Parameter changes only take affect on incoming rising edges.

TRIG is the output clock signal.

OUT is a sawtooth wave that roughly corresponds to the phase of the sawtooth wave used to drive the clock output.

