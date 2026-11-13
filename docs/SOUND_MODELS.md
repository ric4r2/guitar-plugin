# How the amp and pedals are modelled

The goal is a real John Mayer / SRV style clean to edge-of-breakup sound. This page explains what is simulated from
real circuits, what is still an approximation, and where every number comes from.

## Amps: Fender or Two-Rock

Pick the amp with the **FENDER / TWO-ROCK** switch under the input jack. Each amp has its own speaker cabinet.

| Stage | Fender (AB763 Deluxe Reverb) | Two-Rock (clean channel) | How it is modelled |
|---|---|---|---|
| V1A | 12AX7, 100k plate, 1.5k cathode, about 270 V | same type of stage | Norman Koren's 12AX7 equations, solved on the load line |
| Tone stack | 250k treble, 250k bass, 100k slope, 250pF / 0.1uF / 0.047uF, 6.8k mid | Same network with a 25k mid pot and 47nF / 22nF caps: much less mid scoop, tighter bass | Yeh & Smith's exact transfer function, bilinear transform |
| Into V1B | volume pot straight into V1B | volume pot, then an interstage divider (gain structure) | the Two-Rock's second stage clips much later |
| Power amp | two 6V6, 22 W, soft supply | 6L6, big stiff supply | push-pull clip with sag; the Two-Rock has about 2.7x the headroom and a third of the sag |
| Cabinet | open-back 1x12, Jensen/Oxford-type alnico voice | open-back 2x12, 65 W Celestion-type (TR-65 family) | impulse response built from the speaker's response curve (see below) |

Two-Rock does not publish schematics. Its model uses the documented Dumble-lineage topology with values chosen for
its known voice, not traced component values.

The knobs:

* **GAIN** is the amp's volume pot.
* **BASS / MID / TREBLE** are the real tone stack.
* **WARMTH** is how hard the power tubes work. 3 is the stock amp.
* **MASTER** is the output level.

How distorted a firm single-coil note gets (0.25 V, from the test program):

| Volume | 3 | 4 | 5 | 6 | 7 | 8 | 10 |
|---|---|---|---|---|---|---|---|
| Fender THD % | 0.8 | 2.2 | 12 | 23 | 32 | 38 | 44 |
| Two-Rock THD % | 0.8 | 0.5 | 0.5 | 2.1 | 6.9 | 14 | 27 |

### Speaker cabinets

The built-in cabinets are impulse responses that the program builds itself:

1. It takes each speaker's close-miked response curve: the low-end fall, the cone resonance, the presence peak and the top-end roll-off.
2. It adds the narrow cone break-up peaks and dips that a microphone picks up above about 1 kHz.
3. It turns the result into a 46 ms minimum-phase impulse response and plays it through convolution.

They need no downloaded files and are part of the repository.

**Recorded cabinets (your own build only).** You can put a recorded IR at `Assets/private-ir/fender.wav` and/or
`Assets/private-ir/tworock.wav` and rebuild. It then replaces the built-in cabinet for that amp, at the same
loudness. That folder is git-ignored, because most free IRs may be used but not redistributed. Don't share a build
that contains them.

## Tube Screamer: Ibanez TS808

| Part | Values | Model |
|---|---|---|
| Clipping stage | op-amp with 4.7k + 47nF to ground; 51k + 500k DRIVE pot, 51pF and two 1N914 diodes in the feedback | The diode equation (Is 2.52 nA, n 1.752) solved every sample with Newton's method, at 4x oversampling |
| Gain | 1 + (51k + drive) / 4.7k | 21.5 dB to 41 dB; the test program checks this |
| Low-pass | R9 1k, C5 0.22uF | 723 Hz first-order |
| Tone | 20k pot, 1 + R10/R7 = 5.55 | Fitted to the published corners: 400 Hz low-pass at minimum, flat at the centre, a shelf from 725 Hz to 4 kHz at maximum |
| Level | 100k pot | audio taper |

The 720 Hz corner on the gain path is why only mids and highs get clipped. That makes the famous mid hump and keeps
the low end tight.

## Still approximations

* **Compressor, overdrive, wah, phaser, chorus, tremolo, delay, digital reverb**: generic designs, not copies of a particular pedal.
* **Built-in cabinets**: made from response curves, not microphone recordings. Recorded IRs can replace them (see above).
* **Spring and plate reverb**: algorithmic.
* **TS808 tone stage**: built from published corner frequencies, not a full component-level solve.

Next steps for more realism:

* Load a real cabinet impulse response.
* Model the Dyna Comp (CA3080 OTA), the Cry Baby wah (inductor resonant filter) and the Klon Centaur from their circuits.

## Sources

* D. T. Yeh, J. O. Smith, "Discretization of the '59 Fender Bassman Tone Stack", DAFx-06. https://dafx.de/paper-archive/details/bbvGpyU59H2nHvV3Iamezg and https://ccrma.stanford.edu/~dtyeh/tonestack
* Faust `tonestacks.lib` (Guitarix), which has the full coefficients and component sets. https://faustlibraries.grame.fr/libs/tonestacks/
* Rob Robinette, "How the AB763 Deluxe Reverb Works". https://robrobinette.com/How_The_AB763_Deluxe_Reverb_Works.htm
* Norman Koren, "Improved vacuum tube models for SPICE simulations". https://www.normankoren.com/Audio/Tube_params.html
* PedalPCB forum, "Tone stacks part 3: Tube Screamer". https://forum.pedalpcb.com/threads/tone-stacks-part-3-tube-screamer-hm-2-gyrators.8557/
* ElectroSmash, "Tube Screamer Analysis". https://electrosmash.com/tube-screamer-analysis
