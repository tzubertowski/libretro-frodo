/*
 * Memory_SF2000.cpp - Optimized C64 memory system implementation
 */

#include "Memory_SF2000.h"
#include <string.h>

C64Memory_SF2000::C64Memory_SF2000() {
    // Initialize all pointers to null
    ram = nullptr;
    basic_rom = nullptr;
    kernal_rom = nullptr;
    char_rom = nullptr;
    color_ram = nullptr;
    
    // Initialize configuration
    config.config_byte = 0x37;  // Default C64 configuration
    config_dirty = true;
    
    // Initialize performance counters
    fast_reads = 0;
    slow_reads = 0;
    fast_writes = 0;
    slow_writes = 0;
    
    // Initialize I/O handlers
    vic_ptr = nullptr;
    sid_ptr = nullptr;
    cia1_ptr = nullptr;
    cia2_ptr = nullptr;
}

C64Memory_SF2000::~C64Memory_SF2000() {
    // Nothing to clean up - we don't own the memory pointers
}

void C64Memory_SF2000::Initialize(uint8* ram_ptr, uint8* basic_rom_ptr, uint8* kernal_rom_ptr, uint8* char_rom_ptr, uint8* color_ram_ptr) {
    ram = ram_ptr;
    basic_rom = basic_rom_ptr;
    kernal_rom = kernal_rom_ptr;
    char_rom = char_rom_ptr;
    color_ram = color_ram_ptr;
    
    // Build initial memory map
    UpdateMemoryMap();
}

void C64Memory_SF2000::SetConfiguration(uint8 config_byte) {
    if (config.config_byte != config_byte) {
        config.config_byte = config_byte;
        config_dirty = true;
    }
}

void C64Memory_SF2000::UpdateMemoryMap() {
    if (!config_dirty) return;
    
    // Decode configuration bits
    config.basic_enabled  = (config.config_byte & 0x03) == 0x03;
    config.kernal_enabled = (config.config_byte & 0x02) != 0;
    config.char_enabled   = (config.config_byte & 0x04) == 0 && (config.config_byte & 0x03) != 0;
    config.io_enabled     = (config.config_byte & 0x04) != 0 && (config.config_byte & 0x03) != 0;
    
    build_memory_map();
    config_dirty = false;
}

void C64Memory_SF2000::build_memory_map() {
    // Clear all mappings
    memset(config.read_map, 0, sizeof(config.read_map));
    memset(config.write_map, 0, sizeof(config.write_map));
    
    // Always map RAM pages 0x00-0x9F and 0xC0-0xCF
    for (int page = 0x00; page <= 0x9F; page++) {
        config.read_map[page] = ram + (page << 8);
        config.write_map[page] = ram + (page << 8);
    }
    
    for (int page = 0xC0; page <= 0xCF; page++) {
        config.read_map[page] = ram + (page << 8);
        config.write_map[page] = ram + (page << 8);
    }
    
    // Map BASIC ROM area (0xA0-0xBF)
    if (config.basic_enabled && basic_rom) {
        for (int page = 0xA0; page <= 0xBF; page++) {
            config.read_map[page] = basic_rom + ((page - 0xA0) << 8);
            // BASIC ROM is read-only, write goes to RAM
            config.write_map[page] = ram + (page << 8);
        }
    } else {
        // BASIC ROM disabled, map RAM
        for (int page = 0xA0; page <= 0xBF; page++) {
            config.read_map[page] = ram + (page << 8);
            config.write_map[page] = ram + (page << 8);
        }
    }
    
    // Map I/O/CHAR ROM area (0xD0-0xDF)
    if (config.io_enabled) {
        // I/O area - handled by slow path
        for (int page = 0xD0; page <= 0xDF; page++) {
            config.read_map[page] = nullptr;   // Force slow path
            config.write_map[page] = nullptr;  // Force slow path
        }
    } else if (config.char_enabled && char_rom) {
        // CHAR ROM enabled
        for (int page = 0xD0; page <= 0xDF; page++) {
            if (page == 0xD8 || page == 0xD9 || page == 0xDA || page == 0xDB) {
                // Color RAM area
                config.read_map[page] = nullptr;   // Handle specially
                config.write_map[page] = nullptr;
            } else {
                config.read_map[page] = char_rom + ((page - 0xD0) << 8);
                config.write_map[page] = ram + (page << 8);  // Write to RAM underneath
            }
        }
    } else {
        // Both I/O and CHAR ROM disabled, map RAM
        for (int page = 0xD0; page <= 0xDF; page++) {
            config.read_map[page] = ram + (page << 8);
            config.write_map[page] = ram + (page << 8);
        }
    }
    
    // Map KERNAL ROM area (0xE0-0xFF)
    if (config.kernal_enabled && kernal_rom) {
        for (int page = 0xE0; page <= 0xFF; page++) {
            config.read_map[page] = kernal_rom + ((page - 0xE0) << 8);
            // KERNAL ROM is read-only, write goes to RAM
            config.write_map[page] = ram + (page << 8);
        }
    } else {
        // KERNAL ROM disabled, map RAM
        for (int page = 0xE0; page <= 0xFF; page++) {
            config.read_map[page] = ram + (page << 8);
            config.write_map[page] = ram + (page << 8);
        }
    }
}

uint8 C64Memory_SF2000::memory_read_slow(uint16 addr) {
    // Handle I/O area
    if (addr >= 0xD000 && addr <= 0xDFFF && config.io_enabled) {
        return ReadIO(addr);
    }
    
    // Handle color RAM
    if (addr >= 0xD800 && addr <= 0xDBFF) {
        return color_ram[addr - 0xD800] | 0xF0;  // High nibble always set
    }
    
    // Should not reach here if memory map is correct
    return 0xFF;
}

void C64Memory_SF2000::memory_write_slow(uint16 addr, uint8 value) {
    // Handle I/O area
    if (addr >= 0xD000 && addr <= 0xDFFF && config.io_enabled) {
        WriteIO(addr, value);
        return;
    }
    
    // Handle color RAM
    if (addr >= 0xD800 && addr <= 0xDBFF) {
        color_ram[addr - 0xD800] = value & 0x0F;  // Only low nibble writable
        return;
    }
    
    // Fallback to RAM
    ram[addr] = value;
}

uint8 C64Memory_SF2000::ReadIO(uint16 addr) {
    // This will be connected to actual I/O chips
    // For now, return empty bus
    switch (addr & 0xF000) {
        case 0xD000:  // VIC-II
            // TODO: Connect to VIC emulation
            return 0xFF;
            
        case 0xD400:  // SID
            // TODO: Connect to SID emulation
            return 0xFF;
            
        case 0xDC00:  // CIA1
            // TODO: Connect to CIA1 emulation
            return 0xFF;
            
        case 0xDD00:  // CIA2
            // TODO: Connect to CIA2 emulation
            return 0xFF;
            
        default:
            return 0xFF;
    }
}

void C64Memory_SF2000::WriteIO(uint16 addr, uint8 value) {
    // This will be connected to actual I/O chips
    switch (addr & 0xF000) {
        case 0xD000:  // VIC-II
            // TODO: Connect to VIC emulation
            break;
            
        case 0xD400:  // SID
            // TODO: Connect to SID emulation
            break;
            
        case 0xDC00:  // CIA1
            // TODO: Connect to CIA1 emulation
            break;
            
        case 0xDD00:  // CIA2
            // TODO: Connect to CIA2 emulation
            break;
    }
}