/*
 * CPUC64_SF2000_flags.h - Pre-calculated flag lookup tables for SF2000 optimized 6502 CPU
 *
 * This file contains lookup tables for 6502 processor flags to eliminate
 * runtime flag calculations and improve performance on the SF2000 platform.
 */

#ifndef CPUC64_SF2000_FLAGS_H
#define CPUC64_SF2000_FLAGS_H

#include "types.h"

#ifdef SF2000_FAST_CPU

// Pre-calculated N (negative) and Z (zero) flag lookup table
// For 8-bit values, N flag is bit 7, Z flag is set if value is 0
extern const uint8 nz_flag_table[256];

// Pre-calculated flag results for ADC/SBC operations
// Index = (A << 16) | (operand << 8) | carry_in
// Result = new_A | (V_flag << 8) | (C_flag << 9) | (N_flag << 10) | (Z_flag << 11)
extern const uint16 adc_flag_table[65536];
extern const uint16 sbc_flag_table[65536];

// Pre-calculated flag results for compare operations (CMP, CPX, CPY)
// Index = (reg_value << 8) | operand
// Result = C_flag | (N_flag << 1) | (Z_flag << 2)
extern const uint8 cmp_flag_table[65536];

// Initialize flag lookup tables (called once at startup)
void init_sf2000_flag_tables(void);

#endif // SF2000_FAST_CPU

#endif // CPUC64_SF2000_FLAGS_H