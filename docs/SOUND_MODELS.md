# How the amp and pedals are modelled

The goal is a real John Mayer / SRV style clean to edge-of-breakup sound. This page explains what is simulated from
real circuits, what is still an approximation, and where every number comes from.

## Amp: Fender AB763 "blackface" (Deluxe Reverb or Twin Reverb)

Pick the amp with the **DELUXE / TWIN** switch under the input jack.

| Stage | What it is in the real amp | How it is modelled |
|---|---|---|
| V1A | 12AX7, 100k plate load, 1.5k cathode resistor (bypassed), about 270 V | Norman Koren's 12AX7 equations, solved on the load line for every input voltage |
| Tone stack | Fender treble / middle / bass network: 250k treble, 250k bass, 100k slope, 250pF, 0.1uF, 0.047uF (Deluxe); 10k mid pot and 120pF in the Twin | Yeh & Smith's exact transfer function of the network, turned into a digital filter with the bilinear transform |
| Volume | 1M audio-taper pot after the tone stack | about 10 % of the signal at 5, like the real pot |
| V1B | 12AX7, 100k plate, shared 820 ohm cathode | Same Koren model |
| Power amp | Long-tail-pair phase inverter and two 6V6 (Deluxe, 22 W) or four 6L6 (Twin, 85 W), negative feedback | Symmetric push-pull clip with supply sag. The Twin gets far more headroom |
| Speaker | Open-back 1x12 (Deluxe) or 2x12 (Twin) | Filters: low cut, cone resonance, presence peak, steep top-end roll-off |

Everything from V1A to the power tubes runs at 4x the sample rate, so the harmonics don't alias.

The knobs:

* **GAIN** is the amp's volume pot.
* **BASS / MID / TREBLE** are the real tone stack. MID at 5 is the stock Deluxe (6.8k).
* **WARMTH** is how hard the power tubes work. 3 is the stock amp. Higher means more compression and sag, at about the same loudness.
* **MASTER** is just the output level.

How distorted a firm single-coil note gets (0.25 V, from the test program):

| Volume | 3 | 4 | 5 | 6 | 7 | 8 | 10 |
|---|---|---|---|---|---|---|---|
| Deluxe THD % | 0.8 | 2.1 | 11 | 22 | 30 | 35 | 40 |
| Twin THD % | 0.6 | 0.6 | 1.3 | 5 | 13 | 21 | 33 |

So the Deluxe is clean up to about 4 and breaks up around 5 to 6, as players describe the real amp. The Twin stays
clean until about 6.

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
* **Speaker**: filters, not a measured impulse response of a real cabinet.
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
