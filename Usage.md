# Usage Guide

## Simple VCO

### Basic Operation

1. Add the Simple VCO module to your patch
2. Connect any output (SIN, TRI, SAW, SQR) to an audio output or mixer
3. Adjust the FREQ knob to set the base pitch
4. Use FINE for precise tuning

### Pitch Control

The oscillator follows the 1V/octave standard:
- 0V at V/OCT input = C4 (261.63 Hz)
- Each +1V raises pitch by one octave
- Each -1V lowers pitch by one octave

**Example:** Connect a keyboard or sequencer CV output to V/OCT for melodic control.

### Frequency Modulation (FM)

1. Connect a modulation source (LFO, envelope, another oscillator) to the FM input
2. Adjust the FM knob to control modulation depth:
   - Center (0%) = no modulation
   - Right (+100%) = full positive modulation
   - Left (-100%) = inverted modulation

**FM Techniques:**
- **Vibrato**: Use a slow LFO (1-6 Hz) with low FM amount (5-15%)
- **FM Synthesis**: Use an audio-rate oscillator for harmonic/inharmonic timbres
- **Envelope Modulation**: Connect an envelope for pitch sweeps

### Waveform Characteristics

| Output | Character | Harmonics |
|--------|-----------|-----------|
| SIN | Pure, smooth | Fundamental only |
| TRI | Soft, hollow | Odd harmonics (weak) |
| SAW | Bright, buzzy | All harmonics |
| SQR | Hollow, reedy | Odd harmonics (strong) |

### Patch Ideas

**Basic Subtractive Synth:**
```
Simple VCO (SAW) -> VCF -> VCA -> Audio Out
```

**Dual Oscillator Pad:**
```
Simple VCO 1 (SIN) -+
                    +-> Mixer -> VCF -> VCA -> Audio Out
Simple VCO 2 (TRI) -+
```
Detune one oscillator slightly with FINE for thickness.

**FM Bass:**
```
Simple VCO 1 (SIN output) -> Simple VCO 2 (FM input)
Simple VCO 2 (SIN output) -> VCA -> Audio Out
```
