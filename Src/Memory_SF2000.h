/*
 * Memory_SF2000.h - Optimized C64 memory system for SF2000
 *
 * Optimizations:
 * - Direct memory access for common cases (RAM)
 * - Cached memory configuration
 * - Fast zero page access
 * - Optimized bank switching
 * - MIPS-friendly memory alignment
 */

#ifndef _MEMORY_SF2000_H
#define _MEMORY_SF2000_H

#include "types.h"
#include <stdint.h>

// Memory configuration constants
#define C64_RAM_SIZE        0x10000
#define C64_BASIC_ROM_SIZE  0x2000
#define C64_KERNAL_ROM_SIZE 0x2000
#define C64_CHAR_ROM_SIZE   0x1000
#define C64_COLOR_RAM_SIZE  0x0400
#define C64_IO_SIZE         0x1000

// Memory regions
#define MEM_RAM_START       0x0000
#define MEM_RAM_END         0x9FFF
#define MEM_BASIC_START     0xA000
#define MEM_BASIC_END       0xBFFF
#define MEM_RAM2_START      0xC000
#define MEM_RAM2_END        0xCFFF
#define MEM_IO_START        0xD000
#define MEM_IO_END          0xDFFF
#define MEM_KERNAL_START    0xE000
#define MEM_KERNAL_END      0xFFFF

// Fast memory access macros
#define MEMORY_READ_FAST(addr) \
    (((addr) <= 0x9FFF) ? ram[addr] : memory_read_slow(addr))

#define MEMORY_WRITE_FAST(addr, val) \
    do { \
        if ((addr) <= 0x9FFF) { \
            ram[addr] = (val); \
        } else { \
            memory_write_slow(addr, val); \
        } \
    } while(0)

// Zero page access (most critical for 6502 performance)
#define ZP_READ(addr)       ram[addr & 0xFF]
#define ZP_WRITE(addr, val) ram[addr & 0xFF] = (val)

// Stack access (page 1)
#define STACK_READ(addr)    ram[0x100 | (addr & 0xFF)]
#define STACK_WRITE(addr, val) ram[0x100 | (addr & 0xFF)] = (val)

// Memory configuration cache
struct MemoryConfig {
    uint8 config_byte;      // Current $01 value
    bool basic_enabled;     // BASIC ROM at $A000-$BFFF
    bool kernal_enabled;    // KERNAL ROM at $E000-$FFFF
    bool char_enabled;      // CHAR ROM at $D000-$DFFF
    bool io_enabled;        // I/O at $D000-$DFFF
    
    // Fast lookup tables
    uint8* read_map[256];   // 256 page pointers for reads
    uint8* write_map[256];  // 256 page pointers for writes
};

// SF2000 optimized memory manager
class C64Memory_SF2000 {
public:
    C64Memory_SF2000();
    ~C64Memory_SF2000();
    
    // Memory initialization
    void Initialize(uint8* ram_ptr, uint8* basic_rom, uint8* kernal_rom, uint8* char_rom, uint8* color_ram);
    
    // Memory configuration
    void SetConfiguration(uint8 config_byte);
    void UpdateMemoryMap();
    
    // Fast memory access
    inline uint8 ReadByte(uint16 addr);
    inline void WriteByte(uint16 addr, uint8 value);
    inline uint16 ReadWord(uint16 addr);
    inline void WriteWord(uint16 addr, uint16 value);
    
    // I/O access (slower path)
    uint8 ReadIO(uint16 addr);
    void WriteIO(uint16 addr, uint8 value);
    
    // Memory statistics
    uint32 GetFastReads() const { return fast_reads; }
    uint32 GetSlowReads() const { return slow_reads; }
    uint32 GetFastWrites() const { return fast_writes; }
    uint32 GetSlowWrites() const { return slow_writes; }
    
private:
    // Memory pointers
    uint8* ram;
    uint8* basic_rom;
    uint8* kernal_rom;
    uint8* char_rom;
    uint8* color_ram;
    
    // Memory configuration
    MemoryConfig config;
    bool config_dirty;
    
    // Performance counters
    uint32 fast_reads;
    uint32 slow_reads;
    uint32 fast_writes;
    uint32 slow_writes;
    
    // I/O handlers (to be connected to VIC, SID, CIA, etc.)
    void* vic_ptr;
    void* sid_ptr;
    void* cia1_ptr;
    void* cia2_ptr;
    
    // Internal methods
    uint8 memory_read_slow(uint16 addr);
    void memory_write_slow(uint16 addr, uint8 value);
    void build_memory_map();
};

// Inline implementations for maximum performance
inline uint8 C64Memory_SF2000::ReadByte(uint16 addr) {
    uint8 page = addr >> 8;
    
    // Fast path: direct memory access
    if (config.read_map[page]) {
        fast_reads++;
        return config.read_map[page][addr & 0xFF];
    }
    
    // Slow path: I/O or special handling
    slow_reads++;
    return memory_read_slow(addr);
}

inline void C64Memory_SF2000::WriteByte(uint16 addr, uint8 value) {
    uint8 page = addr >> 8;
    
    // Check for configuration change
    if (addr <= 1) {
        if (addr == 1 && value != config.config_byte) {
            config.config_byte = value;
            config_dirty = true;
        }
        ram[addr] = value;
        return;
    }
    
    // Fast path: direct memory access
    if (config.write_map[page]) {
        fast_writes++;
        config.write_map[page][addr & 0xFF] = value;
        return;
    }
    
    // Slow path: I/O or special handling
    slow_writes++;
    memory_write_slow(addr, value);
}

inline uint16 C64Memory_SF2000::ReadWord(uint16 addr) {
    // Handle page boundary crossing
    if ((addr & 0xFF) == 0xFF) {
        return ReadByte(addr) | (ReadByte(addr + 1) << 8);
    } else {
        // Fast path for same page
        uint8 page = addr >> 8;
        if (config.read_map[page]) {
            uint8* ptr = &config.read_map[page][addr & 0xFF];
            return ptr[0] | (ptr[1] << 8);
        } else {
            return ReadByte(addr) | (ReadByte(addr + 1) << 8);
        }
    }
}

inline void C64Memory_SF2000::WriteWord(uint16 addr, uint16 value) {
    WriteByte(addr, value & 0xFF);
    WriteByte(addr + 1, value >> 8);
}

#endif // _MEMORY_SF2000_H