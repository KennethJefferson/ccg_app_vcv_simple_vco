# Tony VCO

A VCV Rack plugin containing synthesizer modules.

## Modules

### Simple VCO
A basic voltage-controlled oscillator with multiple waveform outputs.

**Controls:**
- **FREQ** - Coarse frequency adjustment (-3 to +3 octaves)
- **FINE** - Fine tuning (-6 to +6 semitones)
- **FM** - Frequency modulation attenuverter (-100% to +100%)

**Inputs:**
- **V/OCT** - 1V/octave pitch CV input
- **FM** - Frequency modulation input

**Outputs:**
- **SIN** - Sine wave output
- **TRI** - Triangle wave output
- **SAW** - Sawtooth wave output
- **SQR** - Square wave output

All outputs are 10Vpp (-5V to +5V).

## Building

### Requirements
- Windows with MSYS2 MINGW64
- VCV Rack SDK 2.x
- Required packages: `mingw-w64-x86_64-toolchain`, `mingw-w64-x86_64-jq`

### Build Commands
```bash
# Build plugin
mingw32-make

# Clean build artifacts
mingw32-make clean

# Build and install to VCV Rack
mingw32-make install
```

### SDK Configuration
Set `RACK_DIR` in the Makefile or as an environment variable to point to your Rack SDK location.

## Installation

### From Source
Run `mingw32-make install` to build and install the plugin.

### Manual Installation
Copy the following to `%LOCALAPPDATA%/Rack2/plugins-win-x64/TonyVCO/`:
- `plugin.dll`
- `plugin.json`
- `res/` folder

## License

GPL-3.0-or-later
