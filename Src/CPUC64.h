/*
 *  CPUC64.h - 6510 (C64) emulation (line based)
 *
 *  Frodo Copyright (C) Christian Bauer
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _CPU_C64_H
#define _CPU_C64_H

#include "C64.h"


// Set this to 1 if the 6502 PC should be represented by a real pointer
#ifndef FRODO_SC
#ifndef PC_IS_POINTER
#define PC_IS_POINTER 1
#endif
#endif

// Set this to 1 for more precise CPU cycle calculation
#ifndef PRECISE_CPU_CYCLES
#define PRECISE_CPU_CYCLES 0
#endif

// Set this to 1 for instruction-aligned CIA emulation
#ifndef PRECISE_CIA_CYCLES
#define PRECISE_CIA_CYCLES 0
#endif


// Interrupt types
enum {
	INT_VICIRQ,
	INT_CIAIRQ,
	INT_NMI,
	INT_RESET
};


class MOS6569;
class MOS6581;
class MOS6526_1;
class MOS6526_2;
class REU;
class IEC;
struct MOS6510State;


// 6510 emulation (C64)
class MOS6510 {
public:
	MOS6510(C64 *c64, uint8 *Ram, uint8 *Basic, uint8 *Kernal, uint8 *Char, uint8 *Color);

#ifdef FRODO_SC
	void EmulateCycle(void);			// Emulate one clock cycle
#else
	int EmulateLine(int cycles_left);	// Emulate until cycles_left underflows
#endif
	void Reset(void);
	void AsyncReset(void);				// Reset the CPU asynchronously
	void AsyncNMI(void);				// Raise NMI asynchronously (NMI pulse)
	void GetState(MOS6510State *s);
	void SetState(MOS6510State *s);
	uint8 ExtReadByte(uint16 adr);
	void ExtWriteByte(uint16 adr, uint8 byte);
	uint8 REUReadByte(uint16 adr);
	void REUWriteByte(uint16 adr, uint8 byte);

	void TriggerVICIRQ(void);
	void ClearVICIRQ(void);
	void TriggerCIAIRQ(void);
	void ClearCIAIRQ(void);
	void TriggerNMI(void);
	void ClearNMI(void);

	int ExtConfig;	// Memory configuration for ExtRead/WriteByte (0..7)

	MOS6569 *TheVIC;	// Pointer to VIC
	MOS6581 *TheSID;	// Pointer to SID
	MOS6526_1 *TheCIA1;	// Pointer to CIA 1
	MOS6526_2 *TheCIA2;	// Pointer to CIA 2
	REU *TheREU;		// Pointer to REU
	IEC *TheIEC;		// Pointer to drive array

#ifdef FRODO_SC
	bool BALow;			// BA line for Frodo SC
#endif

protected: // Changed from private to protected for MOS6510_SF2000 access
	uint8 read_byte(uint16 adr);
	uint8 read_byte_io(uint16 adr);
	uint16 read_word(uint16 adr);
	void write_byte(uint16 adr, uint8 byte);
	void write_byte_io(uint16 adr, uint8 byte);

	uint8 read_zp(uint16 adr);
	uint16 read_zp_word(uint16 adr);
	void write_zp(uint16 adr, uint8 byte);

	void new_config(void);
	void illegal_op(uint8 op, uint16 at);
	void illegal_jump(uint16 at, uint16 to);

	void do_adc(uint8 byte);
	void do_sbc(uint8 byte);

	uint8 read_emulator_id(uint16 adr);

	C64 *the_c64;		// Pointer to C64 object

	uint8 *ram;			// Pointer to main RAM
	uint8 *basic_rom, *kernal_rom, *char_rom, *color_ram; // Pointers to ROMs and color RAM

	union {				// Pending interrupts
		uint8 intr[4];	// Index: See definitions above
		unsigned long intr_any;
	} interrupt;
	bool nmi_state;		// State of NMI line

	uint8 n_flag, z_flag;
	bool v_flag, d_flag, i_flag, c_flag;
	uint8 a, x, y, sp;

#if PC_IS_POINTER
	uint8 *pc, *pc_base;
#else
	uint16 pc;
#endif

#ifdef FRODO_SC
	uint32 first_irq_cycle, first_nmi_cycle;

	enum {
		OPFLAG_IRQ_DISABLED = 0x01,
		OPFLAG_IRQ_ENABLED = 0x02,
		OPFLAG_INT_DELAYED = 0x04
	};
	uint8 opflags;			// Interrupt operation flags

	uint8 state, op;		// Current state and opcode
	uint16 ar, ar2;			// Address registers
	uint8 rdbuf;			// Data buffer for RMW instructions
	uint8 ddr, pr, pr_out;	// Processor port
#else
	int	borrowed_cycles;	// Borrowed cycles from next line
#endif

	bool basic_in, kernal_in, char_in, io_in;
	uint8 dfff_byte;
};

// 6510 state
struct MOS6510State {
	uint8 a, x, y;
	uint8 p;			// Processor flags
	uint8 ddr, pr;		// Port
	uint16 pc, sp;
	uint8 intr[4];		// Interrupt state
	bool nmi_state;	
	uint8 dfff_byte;
	bool instruction_complete;
};


// Interrupt functions
#ifdef FRODO_SC
inline void MOS6510::TriggerVICIRQ(void)
{
	if (!(interrupt.intr[INT_VICIRQ] || interrupt.intr[INT_CIAIRQ]))
		first_irq_cycle = the_c64->CycleCounter;
	interrupt.intr[INT_VICIRQ] = true;
}

inline void MOS6510::TriggerCIAIRQ(void)
{
	if (!(interrupt.intr[INT_VICIRQ] || interrupt.intr[INT_CIAIRQ]))
		first_irq_cycle = the_c64->CycleCounter;
	interrupt.intr[INT_CIAIRQ] = true;
}

inline void MOS6510::TriggerNMI(void)
{
	if (!nmi_state) {
		nmi_state = true;
		interrupt.intr[INT_NMI] = true;
		first_nmi_cycle = the_c64->CycleCounter;
	}
}
#else
inline void MOS6510::TriggerVICIRQ(void)
{
	interrupt.intr[INT_VICIRQ] = true;
}

inline void MOS6510::TriggerCIAIRQ(void)
{
	interrupt.intr[INT_CIAIRQ] = true;
}

inline void MOS6510::TriggerNMI(void)
{
	if (!nmi_state) {
		nmi_state = true;
		interrupt.intr[INT_NMI] = true;
	}
}
#endif
inline void MOS6510::ClearVICIRQ(void)
{
	interrupt.intr[INT_VICIRQ] = false;
}

inline void MOS6510::ClearCIAIRQ(void)
{
	interrupt.intr[INT_CIAIRQ] = false;
}

inline void MOS6510::ClearNMI(void)
{
	nmi_state = false;
}

#endif
