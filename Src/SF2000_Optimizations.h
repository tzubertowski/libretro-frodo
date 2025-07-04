/*
 * SF2000_Optimizations.h - Integration header for SF2000-specific optimizations
 *
 * This header conditionally enables SF2000 optimizations and provides
 * compatibility layer for the existing Frodo codebase.
 */

#ifndef _SF2000_OPTIMIZATIONS_H
#define _SF2000_OPTIMIZATIONS_H

// Check if SF2000 optimizations are enabled
#ifdef SF2000_C64_OPTIMIZED

// Include SF2000 optimized components
#ifdef SF2000_FAST_CPU
#include "CPUC64_SF2000.h"
#define MOS6510_OPTIMIZED MOS6510_SF2000
#else
#include "CPUC64.h"
#define MOS6510_OPTIMIZED MOS6510
#endif

#ifdef SF2000_FAST_MEMORY
#include "Memory_SF2000.h"
#define C64Memory_OPTIMIZED C64Memory_SF2000
#endif

#ifdef SF2000_FAST_VIC
#include "VIC_SF2000.h"
#define MOS6569_OPTIMIZED MOS6569_SF2000
#else
#include "VIC.h"
#define MOS6569_OPTIMIZED MOS6569
#endif

#ifdef SF2000_FAST_SID
#include "SID_SF2000.h"
#define MOS6581_OPTIMIZED MOS6581_SF2000
#else
#include "SID.h"
#define MOS6581_OPTIMIZED MOS6581
#endif

// SF2000 performance tuning constants
#define SF2000_CPU_CYCLES_PER_FRAME    20000   // Reduced from ~65000 for fast emulation
#define SF2000_RASTER_LINES_PER_FRAME  312     // PAL standard
#define SF2000_AUDIO_SAMPLES_PER_FRAME 441     // 22050Hz / 50Hz

// SF2000 display mapping
#define SF2000_DISPLAY_WIDTH           320
#define SF2000_DISPLAY_HEIGHT          240
#define SF2000_C64_SCALE_X             1.0f    // Perfect 1:1 scaling
#define SF2000_C64_SCALE_Y             1.2f    // Scale 200 to 240 lines
#define SF2000_C64_OFFSET_Y            20      // Center vertically

// Performance monitoring macros (compiled out in release)
#ifdef DEBUG
#define SF2000_PERF_START(timer) timer = get_cpu_cycle_count()
#define SF2000_PERF_END(timer, counter) counter += get_cpu_cycle_count() - timer
#define SF2000_PERF_LOG(name, counter, frames) \
    if (frames % 300 == 0) printf("SF2000 %s: %u cycles/frame\n", name, counter / 300); \
    if (frames % 300 == 0) counter = 0
#else
#define SF2000_PERF_START(timer)
#define SF2000_PERF_END(timer, counter)
#define SF2000_PERF_LOG(name, counter, frames)
#endif

// Memory optimization macros
#ifdef SF2000_FAST_MEMORY
#define FAST_RAM_READ(addr)     ((addr) < 0xA000 ? ram[addr] : read_byte_slow(addr))
#define FAST_RAM_WRITE(addr, val) \
    do { \
        if ((addr) < 0xA000) ram[addr] = (val); \
        else write_byte_slow(addr, val); \
    } while(0)
#else
#define FAST_RAM_READ(addr)     read_byte(addr)
#define FAST_RAM_WRITE(addr, val) write_byte(addr, val)
#endif

// CPU optimization macros
#ifdef SF2000_FAST_CPU
#define FAST_CPU_EMULATE_LINE(cycles) EmulateLineFast(cycles)
#else
#define FAST_CPU_EMULATE_LINE(cycles) EmulateLine(cycles)
#endif

// VIC optimization macros
#ifdef SF2000_FAST_VIC
#define FAST_VIC_EMULATE_LINE() EmulateLine()
#define FAST_VIC_SET_FRAMEBUFFER(fb) SetFramebuffer((uint16*)fb)
#else
#define FAST_VIC_EMULATE_LINE() EmulateLine()
#define FAST_VIC_SET_FRAMEBUFFER(fb)
#endif

// SID optimization macros
#ifdef SF2000_FAST_SID
#define FAST_SID_GENERATE_SAMPLES(buf, num) GenerateSamples((int16*)buf, num)
#else
#define FAST_SID_GENERATE_SAMPLES(buf, num) EmulateLine()
#endif

// Compiler hints for SF2000 MIPS
#ifdef SF2000_MIPS_OPTIMIZED
#define SF2000_INLINE inline __attribute__((always_inline))
#define SF2000_HOT __attribute__((hot))
#define SF2000_LIKELY(x) __builtin_expect(!!(x), 1)
#define SF2000_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define SF2000_PREFETCH(addr) __builtin_prefetch(addr, 0, 3)
#else
#define SF2000_INLINE inline
#define SF2000_HOT
#define SF2000_LIKELY(x) (x)
#define SF2000_UNLIKELY(x) (x)
#define SF2000_PREFETCH(addr)
#endif

#else

// No SF2000 optimizations - use standard Frodo components
#include "CPUC64.h"
#include "VIC.h"
#include "SID.h"

#define MOS6510_OPTIMIZED MOS6510
#define MOS6569_OPTIMIZED MOS6569
#define MOS6581_OPTIMIZED MOS6581

#define FAST_RAM_READ(addr) read_byte(addr)
#define FAST_RAM_WRITE(addr, val) write_byte(addr, val)
#define FAST_CPU_EMULATE_LINE(cycles) EmulateLine(cycles)
#define FAST_VIC_EMULATE_LINE() EmulateLine()
#define FAST_VIC_SET_FRAMEBUFFER(fb)
#define FAST_SID_GENERATE_SAMPLES(buf, num) EmulateLine()

#define SF2000_INLINE inline
#define SF2000_HOT
#define SF2000_LIKELY(x) (x)
#define SF2000_UNLIKELY(x) (x)
#define SF2000_PREFETCH(addr)

#endif // SF2000_C64_OPTIMIZED

// Common utility macros for all platforms
#define CLAMP_16BIT(x) ((x) > 32767 ? 32767 : ((x) < -32768 ? -32768 : (x)))
#define SIGN_EXTEND_8BIT(x) ((int8)(x))
#define ZERO_PAGE_ADDR(x) ((x) & 0xFF)

#endif // _SF2000_OPTIMIZATIONS_H