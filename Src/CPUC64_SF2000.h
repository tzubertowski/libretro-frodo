/*
 * CPUC64_SF2000.h - High-performance 6502 CPU core optimized for SF2000 MIPS
 *
 * SF2000 Optimization Notes:
 * - 918MHz MIPS32 vs 1MHz 6502 = 918x performance advantage
 * - Direct instruction dispatch using computed goto (GCC extension)
 * - MIPS register allocation for 6502 registers
 * - Optimized memory access patterns
 * - Lookup tables for flag calculations
 * - Instruction-level parallelism where possible
 */

#ifndef _CPUC64_SF2000_H
#define _CPUC64_SF2000_H

#include "types.h"
#include "CPUC64.h"

// SF2000 performance optimizations enabled
#ifdef __GNUC__
#define SF2000_COMPUTED_GOTO 1
#define SF2000_FAST_DISPATCH 1
#define SF2000_MIPS_OPTIMIZED 1
#endif

// Fast 6502 instruction dispatch using computed goto
#ifdef SF2000_COMPUTED_GOTO
#define DISPATCH_NEXT() goto *dispatch_table[*pc++]
#else
#define DISPATCH_NEXT() continue
#endif

// MIPS register allocation hints for 6502 registers
#ifdef SF2000_MIPS_OPTIMIZED
#define REGISTER_A      register uint8 reg_a asm("$s0")
#define REGISTER_X      register uint8 reg_x asm("$s1")  
#define REGISTER_Y      register uint8 reg_y asm("$s2")
#define REGISTER_SP     register uint8 reg_sp asm("$s3")
#define REGISTER_PC     register uint8* reg_pc asm("$s4")
#define REGISTER_FLAGS  register uint32 reg_flags asm("$s5")
#else
#define REGISTER_A      uint8 reg_a
#define REGISTER_X      uint8 reg_x
#define REGISTER_Y      uint8 reg_y
#define REGISTER_SP     uint8 reg_sp
#define REGISTER_PC     uint8* reg_pc
#define REGISTER_FLAGS  uint32 reg_flags
#endif

// Fast flag calculations using lookup tables
struct FlagLookup {
    uint8 nz_table[256];    // Combined N and Z flags
    uint8 adc_table[512];   // ADC flag results
    uint8 sbc_table[512];   // SBC flag results
    uint8 cmp_table[512];   // CMP flag results
};

// SF2000 optimized 6502 core
class MOS6510_SF2000 : public MOS6510 {
public:
    MOS6510_SF2000(C64 *c64, uint8 *Ram, uint8 *Basic, uint8 *Kernal, uint8 *Char, uint8 *Color);
    
    // Override base class method to use optimized version
    int EmulateLine(int cycles_left) { 
        return EmulateLineFast(cycles_left);
    }
    
    // Fast emulation methods  
    int EmulateLineFast(int cycles_left);
    int EmulateLineComputedGoto(int cycles_left);
    void InitializeFastTables();
    
    // Memory access optimizations
    inline uint8 ReadMemoryFast(uint16 addr);
    inline void WriteMemoryFast(uint16 addr, uint8 value);
    
    // Override memory access methods for performance
    uint8 ExtReadByte(uint16 addr);
    void ExtWriteByte(uint16 addr, uint8 value);
    
    // Instruction implementations (optimized)
    void ExecuteInstructionFast();
    
private:
    // Fast lookup tables
    static FlagLookup flag_lookup;
    static bool tables_initialized;
    
    // Fast instruction dispatch table
    #ifdef SF2000_COMPUTED_GOTO
    static void* dispatch_table[256];
    #endif
    
    // Fast memory cache for most common addresses
    mutable uint16 last_read_addr;
    mutable uint8 last_read_value;
    mutable uint16 last_write_addr;
    
    // Simplified configuration tracking
    bool config_changed;
    
    // Fast access to base class memory for direct RAM optimization
    uint8* ram_ptr_cache;
    
    // Performance counters
    uint32 fast_instructions;
    uint32 slow_instructions;
    
    // Internal optimization methods
    void UpdateMemoryMap();
    void InitializeDispatchTable();
    
    // Fast instruction implementations
    void FastLDA(); void FastLDX(); void FastLDY();
    void FastSTA(); void FastSTX(); void FastSTY();
    void FastADC(); void FastSBC();
    void FastAND(); void FastORA(); void FastEOR();
    void FastCMP(); void FastCPX(); void FastCPY();
    void FastINC(); void FastDEC(); void FastINX(); void FastINY(); void FastDEX(); void FastDEY();
    void FastJMP(); void FastJSR(); void FastRTS(); void FastRTI();
    void FastBEQ(); void FastBNE(); void FastBPL(); void FastBMI();
    void FastBCC(); void FastBCS(); void FastBVC(); void FastBVS();
    void FastNOP();
};

// Ultra-fast memory access macros for maximum performance
// These provide the fastest possible memory access for SF2000

// Ultra-fast macros for direct RAM access (zero overhead)
#define SF2000_READ_RAM_FAST(addr) (ram_ptr_cache[addr])
#define SF2000_WRITE_RAM_FAST(addr, value) (ram_ptr_cache[addr] = (value))

// Optimized inline functions with minimal overhead
inline uint8 MOS6510_SF2000::ReadMemoryFast(uint16 addr) {
    // Optimized for MIPS branch prediction
    return (addr < 0xa000) ? ram_ptr_cache[addr] : MOS6510::ExtReadByte(addr);
}

inline void MOS6510_SF2000::WriteMemoryFast(uint16 addr, uint8 value) {
    // Optimized for MIPS branch prediction  
    if (addr < 0xa000) {
        ram_ptr_cache[addr] = value;
    } else {
        MOS6510::ExtWriteByte(addr, value);
    }
}

// Flag calculation macros using lookup tables
#define SET_NZ_FLAGS(val) reg_flags = (reg_flags & 0x7D) | flag_lookup.nz_table[val]
#define SET_CARRY(val) reg_flags = (reg_flags & 0xFE) | (val & 1)
#define GET_CARRY() (reg_flags & 1)
#define GET_ZERO() ((reg_flags & 2) == 0)
#define GET_NEGATIVE() (reg_flags & 0x80)

// Common addressing modes optimized for MIPS
#define ADDR_ZP()       (reg_pc[0])
#define ADDR_ZPX()      ((reg_pc[0] + reg_x) & 0xFF)
#define ADDR_ZPY()      ((reg_pc[0] + reg_y) & 0xFF)
#define ADDR_ABS()      (reg_pc[0] | (reg_pc[1] << 8))
#define ADDR_ABSX()     ((reg_pc[0] | (reg_pc[1] << 8)) + reg_x)
#define ADDR_ABSY()     ((reg_pc[0] | (reg_pc[1] << 8)) + reg_y)

// Common instruction patterns
#define LOAD_OP(reg, addr) \
    reg = ReadMemoryFast(addr); \
    SET_NZ_FLAGS(reg)

#define STORE_OP(reg, addr) \
    WriteMemoryFast(addr, reg)

#define COMPARE_OP(reg, addr) \
    { \
        uint8 val = ReadMemoryFast(addr); \
        uint16 result = reg - val; \
        SET_NZ_FLAGS(result); \
        SET_CARRY(result < 0x100); \
    }

#endif // _CPUC64_SF2000_H