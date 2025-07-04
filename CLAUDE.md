# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

This is a libretro core for the Frodo C64 emulator using Make-based build system.

### Primary Build Commands
- `make` - Build the core for the current platform (creates `frodo_libretro.so` on Unix/Linux)
- `make clean` - Clean build artifacts
- `make DEBUG=1` - Build with debug symbols and no optimization
- `make EMUTYPE=frodosc` - Build the single-cycle accurate version (FrodoSC)

### SF2000 Optimized Build Commands
- `make platform=sf2000` - Build highly optimized C64 core for SF2000 handheld
- `make platform=sf2000 clean` - Clean SF2000 build artifacts
- `make platform=sf2000 DEBUG=1` - Build SF2000 core with debug symbols

### SF2000 Optimization Features
The SF2000 build automatically enables advanced optimizations:
- **SF2000_FAST_CPU**: High-performance 6502 CPU emulation with computed goto
- **SF2000_FAST_MEMORY**: Direct memory access with cached memory mapping
- **SF2000_FAST_VIC**: Optimized VIC-II graphics rendering for 320x240 display
- **SF2000_FAST_SID**: Streamlined SID audio synthesis for mono output
- **SF2000_MIPS_OPTIMIZED**: MIPS-specific compiler optimizations and register allocation
- **SF2000_COMPUTED_GOTO**: Zero-overhead instruction dispatch using GCC computed goto

### Platform-Specific Building
The build system automatically detects the platform but can be overridden:
- `make platform=unix` - Unix/Linux shared library (.so)
- `make platform=osx` - macOS dynamic library (.dylib)
- `make platform=win` - Windows DLL
- `make platform=android` - Android shared library
- `make platform=ios` - iOS dynamic library
- `make platform=psp1` - PSP static library
- `make platform=vita` - PS Vita static library
- `make platform=ctr` - Nintendo 3DS static library
- `make platform=libnx` - Nintendo Switch static library
- `make platform=sf2000` - SF2000 handheld static library

### Build Variables
- `DEBUG=1` - Enable debug build
- `EMUTYPE=frodosc` - Build FrodoSC (single-cycle accurate) instead of regular Frodo
- `HAVE_SAM=1` - Enable SAM voice synthesizer support
- `NOLIBCO=1` - Disable libco cooperative threading (required for some platforms)

## Architecture Overview

### Core Structure
- **Src/**: Original Frodo emulator source code
  - `C64.cpp/h` - Main C64 system emulation
  - `CPUC64.cpp/h` - 6510 CPU emulation
  - `VIC.cpp/h` - VIC-II video chip emulation
  - `SID.cpp/h` - SID sound chip emulation
  - `CIA.cpp/h` - CIA timer/I/O chip emulation
  - `Display.cpp/h` - Display/video output handling
  - `1541*.cpp/h` - 1541 floppy drive emulation
  - `IEC.cpp/h` - IEC bus emulation
  - `Prefs.cpp/h` - Preferences/configuration

- **libretro/**: Libretro core integration layer
  - `core/libretro.cpp` - Main libretro API implementation
  - `core/core-mapper.cpp` - Input mapping
  - `core/graph.cpp` - Graphics/video processing
  - `gui/` - Configuration GUI dialogs
  - `include/` - Libretro-specific headers

### Emulation Modes
The core supports two emulation modes controlled by `EMUTYPE`:
- **frodo** (default): Fast emulation with line-based timing
- **frodosc**: Cycle-accurate emulation with single-cycle precision

### Threading Model
- Uses libco cooperative threading by default for better performance
- Can be disabled with `NOLIBCO=1` for platforms that don't support it
- Main emulation runs in separate thread from libretro callbacks

### Memory Layout
- C64 RAM: 64KB main memory
- Color RAM: 1KB video color memory
- ROM areas: BASIC, KERNAL, Character ROM
- Drive RAM/ROM: 1541 floppy drive memory

### Key Integration Points
- `libretro.cpp:retro_run()` - Main emulation loop entry point
- `Display.cpp` - Video output to libretro video callback
- `SID.cpp` - Audio output to libretro audio callback
- `core-mapper.cpp` - Input handling from libretro input system
- `Prefs.cpp` - Configuration via libretro core options

### File Format Support
- `.d64` - Disk images (1541 format)
- `.t64` - Tape images
- `.prg` - Program files
- `.p00` - PC64 program files
- `.x64` - Extended disk images

### Configuration
Core options are defined in `libretro_core_options.h` and include:
- Video resolution and cropping
- Audio settings
- Input mapping
- Drive emulation options
- System region (PAL/NTSC)

### Development Notes
- The codebase maintains compatibility with the original Frodo emulator
- Platform-specific code is isolated in the build system
- GUI dialogs are available for runtime configuration
- Video output uses a virtual framebuffer that's converted for libretro
- Audio uses a sample buffer that's pushed to libretro callbacks

## SF2000 Optimizations

### Performance Advantage
The SF2000's MIPS CPU at 918MHz provides a massive 918x performance advantage over the original C64's 1MHz 6502 processor. This allows for:
- Perfect C64 emulation with significant headroom
- Advanced optimization techniques not possible on weaker hardware
- Enhanced features while maintaining full compatibility

### SF2000-Specific Optimized Components

#### CPUC64_SF2000.cpp/h - High-Performance 6502 Emulation
- **Computed Goto Dispatch**: Zero-overhead instruction dispatch using GCC's computed goto
- **MIPS Register Allocation**: 6502 registers mapped to MIPS hardware registers
- **Lookup Tables**: Pre-calculated flag operations and instruction timings
- **Direct Memory Access**: Bypasses function calls for RAM access
- **Instruction-Level Optimizations**: Unrolled common instruction sequences

#### Memory_SF2000.cpp/h - Optimized Memory System
- **Direct RAM Access**: Fast path for 75% of memory space (RAM areas)
- **Cached Memory Mapping**: Pre-calculated page pointers for instant access
- **Bank Switch Optimization**: Minimal overhead for C64 memory configuration changes
- **Zero Page Optimization**: Special handling for 6502's most critical memory area

#### VIC_SF2000.cpp/h - Graphics Optimizations
- **Perfect Resolution Match**: C64's 320x200 scales perfectly to SF2000's 320x240
- **Direct Framebuffer Access**: No intermediate buffering
- **RGB565 Color Palette**: Pre-converted C64 colors for direct pixel writing
- **Sprite Caching**: Pre-rendered sprites for instant blitting
- **Character Set Caching**: Cached character rendering for text modes

#### SID_SF2000.cpp/h - Audio Optimizations  
- **Lookup Table Synthesis**: Pre-calculated waveforms for instant generation
- **Mono Output Optimization**: Simplified mixing for SF2000's mono audio
- **22050Hz Target Rate**: Optimized for SF2000's audio hardware
- **Simplified Filter**: Fast approximation of SID's analog filter

### Expected Performance Results
With these optimizations, the SF2000 should achieve:
- **100% speed**: Full 50fps PAL / 60fps NTSC emulation
- **Low CPU usage**: ~10-20% CPU utilization, leaving headroom for enhancements
- **Perfect audio**: No dropouts or timing issues
- **High compatibility**: Runs vast majority of C64 software flawlessly
- **Enhanced features**: Fast save states, disk switching, multiple C64 models

### Optimization Build Flags
All optimizations are automatically enabled for `platform=sf2000`:
```
-DSF2000_C64_OPTIMIZED -DSF2000_FAST_CPU -DSF2000_FAST_VIC 
-DSF2000_FAST_SID -DSF2000_FAST_MEMORY -DSF2000_COMPUTED_GOTO 
-DSF2000_MIPS_OPTIMIZED -O3 -funroll-loops [many MIPS-specific flags]
```