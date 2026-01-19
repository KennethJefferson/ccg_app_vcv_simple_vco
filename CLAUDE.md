# CLAUDE.md

## Project Overview
VCV Rack plugin containing synthesizer modules. Built with C++ using the VCV Rack SDK.

## Build System
- **Platform**: Windows (MSYS2 MINGW64)
- **SDK Location**: `C:/dev-vcvrack/Rack-SDK`
- **Build Command**: `mingw32-make` (from MINGW64 environment)
- **Clean Command**: `mingw32-make clean`
- **Install Command**: `mingw32-make install`

## Project Structure
```
├── Makefile           # Build configuration, SOURCES defined here
├── plugin.json        # Plugin metadata (slug, version, modules)
├── src/
│   ├── plugin.hpp     # Plugin header, declares extern Model pointers
│   ├── plugin.cpp     # Plugin init, registers models
│   └── [Module].cpp   # Individual module implementations
└── res/
    └── [Module].svg   # Panel graphics (30.48mm x 128.5mm for 6HP)
```

## Adding New Modules
1. Create `src/NewModule.cpp` with Module struct and ModuleWidget
2. Add `extern Model* modelNewModule;` to `src/plugin.hpp`
3. Add `p->addModel(modelNewModule);` to `src/plugin.cpp`
4. Add `SOURCES += src/NewModule.cpp` to `Makefile`
5. Create `res/NewModule.svg` panel graphic
6. Add module entry to `plugin.json` modules array

## Code Conventions
- Use `configParam()`, `configInput()`, `configOutput()` in constructor
- Check `isConnected()` before computing unused outputs
- Voltage range: -10V to +10V (audio typically 10Vpp, CV typically 0-10V or -5V to +5V)
- V/Oct standard: 0V = C4 (261.63 Hz), 1V/octave scaling

## Dependencies
- VCV Rack SDK 2.x
- MSYS2 MINGW64 with: mingw-w64-x86_64-toolchain, mingw-w64-x86_64-jq
