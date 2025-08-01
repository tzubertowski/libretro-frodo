/*
 *  SID.cpp - 6581 emulation
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

/*
 * Incompatibilities:
 * ------------------
 *
 *  - Lots of empirically determined constants in the filter calculations
 */

#include <stdint.h>
#include <string.h>
#include <math.h>

#include "sysdeps.h"

#include "SID.h"
#include "Prefs.h"

#ifdef USE_FIXPOINT_MATHS
#include "FixPoint.h"
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


/*
 *  Resonance frequency polynomials
 */

#define CALC_RESONANCE_LP(f) (227.755\
				- 1.7635 * f\
				- 0.0176385 * f * f\
				+ 0.00333484 * f * f * f\
				- 9.05683E-6 * f * f * f * f)

#define CALC_RESONANCE_HP(f) (366.374\
				- 14.0052 * f\
				+ 0.603212 * f * f\
				- 0.000880196 * f * f * f)


/*
 *  Random number generator for noise waveform
 */

static uint8 sid_random(void)
{
	static uint32 seed = 1;
	seed = seed * 1103515245 + 12345;
	return seed >> 16;
}


/*
 *  Constructor
 */

MOS6581::MOS6581(C64 *c64) : the_c64(c64)
{
   unsigned i;
	the_renderer = NULL;
	for (i=0; i < 32; i++)
		regs[i] = 0;

	// Open the renderer
	open_close_renderer(SIDTYPE_NONE, ThePrefs.SIDType);
}


/*
 *  Destructor
 */

MOS6581::~MOS6581()
{
	// Close the renderer
	open_close_renderer(ThePrefs.SIDType, SIDTYPE_NONE);
}


/*
 *  Reset the SID
 */

void MOS6581::Reset(void)
{
   unsigned i;
	for (i = 0; i < 32; i++)
		regs[i] = 0;
	last_sid_byte = 0;

	// Reset the renderer
	if (the_renderer)
		the_renderer->Reset();
}


/*
 *  Preferences may have changed
 */

void MOS6581::NewPrefs(Prefs *prefs)
{
	open_close_renderer(ThePrefs.SIDType, prefs->SIDType);
	if (the_renderer)
		the_renderer->NewPrefs(prefs);
}


/*
 *  Pause sound output
 */

void MOS6581::PauseSound(void)
{
	if (the_renderer)
		the_renderer->Pause();
}


/*
 *  Resume sound output
 */

void MOS6581::ResumeSound(void)
{
	if (the_renderer)
		the_renderer->Resume();
}


/*
 *  Get SID state
 */

void MOS6581::GetState(MOS6581State *ss)
{
	ss->freq_lo_1 = regs[0];
	ss->freq_hi_1 = regs[1];
	ss->pw_lo_1 = regs[2];
	ss->pw_hi_1 = regs[3];
	ss->ctrl_1 = regs[4];
	ss->AD_1 = regs[5];
	ss->SR_1 = regs[6];

	ss->freq_lo_2 = regs[7];
	ss->freq_hi_2 = regs[8];
	ss->pw_lo_2 = regs[9];
	ss->pw_hi_2 = regs[10];
	ss->ctrl_2 = regs[11];
	ss->AD_2 = regs[12];
	ss->SR_2 = regs[13];

	ss->freq_lo_3 = regs[14];
	ss->freq_hi_3 = regs[15];
	ss->pw_lo_3 = regs[16];
	ss->pw_hi_3 = regs[17];
	ss->ctrl_3 = regs[18];
	ss->AD_3 = regs[19];
	ss->SR_3 = regs[20];

	ss->fc_lo = regs[21];
	ss->fc_hi = regs[22];
	ss->res_filt = regs[23];
	ss->mode_vol = regs[24];

	ss->pot_x = 0xff;
	ss->pot_y = 0xff;
	ss->osc_3 = 0;
	ss->env_3 = 0;
}


/*
 *  Restore SID state
 */

void MOS6581::SetState(MOS6581State *ss)
{
   unsigned i;
	regs[0]  = ss->freq_lo_1;
	regs[1]  = ss->freq_hi_1;
	regs[2]  = ss->pw_lo_1;
	regs[3]  = ss->pw_hi_1;
	regs[4]  = ss->ctrl_1;
	regs[5]  = ss->AD_1;
	regs[6]  = ss->SR_1;

	regs[7]  = ss->freq_lo_2;
	regs[8]  = ss->freq_hi_2;
	regs[9]  = ss->pw_lo_2;
	regs[10] = ss->pw_hi_2;
	regs[11] = ss->ctrl_2;
	regs[12] = ss->AD_2;
	regs[13] = ss->SR_2;

	regs[14] = ss->freq_lo_3;
	regs[15] = ss->freq_hi_3;
	regs[16] = ss->pw_lo_3;
	regs[17] = ss->pw_hi_3;
	regs[18] = ss->ctrl_3;
	regs[19] = ss->AD_3;
	regs[20] = ss->SR_3;

	regs[21] = ss->fc_lo;
	regs[22] = ss->fc_hi;
	regs[23] = ss->res_filt;
	regs[24] = ss->mode_vol;

	// Stuff the new register values into the renderer
	if (the_renderer)
		for (i = 0; i < 25; i++)
			the_renderer->WriteRegister(i, regs[i]);
}


/**
 **  Renderer for digital SID emulation (SIDTYPE_DIGITAL)
 **/

#if !defined(SF2000)
const uint32 SAMPLE_FREQ = 44100;	// Sample output frequency in Hz
#else
const uint32 SAMPLE_FREQ = 22050;	// Sample output frequency in Hz
#endif
const uint32 SID_FREQ = 985248;		// SID frequency in Hz
const uint32 CALC_FREQ = 50;			// Frequency at which calc_buffer is called in Hz (should be 50Hz)
const uint32 SID_CYCLES = SID_FREQ/SAMPLE_FREQ;	// # of SID clocks per sample frame
const int SAMPLE_BUF_SIZE = 0x138*2;// Size of buffer for sampled voice (double buffered)

// SID waveforms (some of them :-)
enum {
	WAVE_NONE,
	WAVE_TRI,
	WAVE_SAW,
	WAVE_TRISAW,
	WAVE_RECT,
	WAVE_TRIRECT,
	WAVE_SAWRECT,
	WAVE_TRISAWRECT,
	WAVE_NOISE
};

// EG states
enum {
	EG_IDLE,
	EG_ATTACK,
	EG_DECAY,
	EG_RELEASE
};

// Filter types
enum
{
   FILT_NONE,
   FILT_LP,
   FILT_BP,
   FILT_LPBP,
   FILT_HP,
   FILT_NOTCH,
   FILT_HPBP,
   FILT_ALL
};

// Structure for one voice
struct DRVoice
{
   int wave;		// Selected waveform
   int eg_state;	// Current state of EG
   DRVoice *mod_by;	// Voice that modulates this one
   DRVoice *mod_to;	// Voice that is modulated by this one

   uint32 count;	// Counter for waveform generator, 8.16 fixed
   uint32 add;		// Added to counter in every frame

   uint16 freq;		// SID frequency value
   uint16 pw;		// SID pulse-width value

   uint32 a_add;	// EG parameters
   uint32 d_sub;
   uint32 s_level;
   uint32 r_sub;
   uint32 eg_level;	// Current EG level, 8.16 fixed

   uint32 noise;	// Last noise generator output value

   bool gate;		// EG gate bit
   bool ring;		// Ring modulation bit
   bool test;		// Test bit
   bool filter;	// Flag: Voice filtered

   // The following bit is set for the modulating
   // voice, not for the modulated one (as the SID bits)
   bool sync;		// Sync modulation bit
   bool mute;		// Voice muted (voice 3 only)
};

// Renderer class
class DigitalRenderer : public SIDRenderer
{
   public:
      DigitalRenderer(C64 *c64);
      virtual ~DigitalRenderer();

      virtual void Reset(void);
      virtual void EmulateLine(void);
      virtual void WriteRegister(uint16 adr, uint8 byte);
      virtual void NewPrefs(Prefs *prefs);
      virtual void Pause(void);
      virtual void Resume(void);

   private:
      void init_sound(void);
      void calc_filter(void);
      void calc_buffer(int16 *buf, long count);

      C64 *the_c64;					// Pointer to C64 object

      bool ready;						// Flag: Renderer has initialized and is ready
      uint8 volume;					// Master volume

      static uint16 TriTable[0x1000*2];	// Tables for certain waveforms
      static const uint16 TriSawTable[0x100];
      static const uint16 TriRectTable[0x100];
      static const uint16 SawRectTable[0x100];
      static const uint16 TriSawRectTable[0x100];
      static const uint32 EGTable[16];	// Increment/decrement values for all A/D/R settings
      static const uint8 EGDRShift[256]; // For exponential approximation of D/R
      static const int16 SampleTab[16]; // Table for sampled voice

      DRVoice voice[3];				// Data for 3 voices

      uint8 f_type;					// Filter type
      uint8 f_freq;					// SID filter frequency (upper 8 bits)
      uint8 f_res;					// Filter resonance (0..15)
#ifdef USE_FIXPOINT_MATHS
      FixPoint f_ampl;
      FixPoint d1, d2, g1, g2;
      int32 xn1, xn2, yn1, yn2;		// can become very large
      FixPoint sidquot;
#ifdef PRECOMPUTE_RESONANCE
      FixPoint resonanceLP[256];
      FixPoint resonanceHP[256];
#endif
#else
      float f_ampl;					// IIR filter input attenuation
      float d1, d2, g1, g2;			// IIR filter coefficients
      float xn1, xn2, yn1, yn2;		// IIR filter previous input/output signal
#ifdef PRECOMPUTE_RESONANCE
      float resonanceLP[256];			// shortcut for calc_filter
      float resonanceHP[256];
#endif
#endif

      uint8 sample_buf[SAMPLE_BUF_SIZE]; // Buffer for sampled voice
      int sample_in_ptr;				// Index in sample_buf for writing

      int devfd, sndbufsize, buffer_rate;
      int16 *sound_buffer;
};

// Static data members
uint16 DigitalRenderer::TriTable[0x1000*2];

#ifndef EMUL_MOS8580
// Sampled from a 6581R4
const uint16 DigitalRenderer::TriSawTable[0x100] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0808,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1010, 0x3C3C,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0808,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1010, 0x3C3C
};

const uint16 DigitalRenderer::TriRectTable[0x100] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x8080,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x8080,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x8080, 0xC0C0,
	0x0000, 0x8080, 0x8080, 0xE0E0, 0x8080, 0xE0E0, 0xF0F0, 0xFCFC,
	0xFFFF, 0xFCFC, 0xFAFA, 0xF0F0, 0xF6F6, 0xE0E0, 0xE0E0, 0x8080,
	0xEEEE, 0xE0E0, 0xE0E0, 0x8080, 0xC0C0, 0x0000, 0x0000, 0x0000,
	0xDEDE, 0xC0C0, 0xC0C0, 0x0000, 0x8080, 0x0000, 0x0000, 0x0000,
	0x8080, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0xBEBE, 0x8080, 0x8080, 0x0000, 0x8080, 0x0000, 0x0000, 0x0000,
	0x8080, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x7E7E, 0x4040, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

const uint16 DigitalRenderer::SawRectTable[0x100] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x7878,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x7878
};

const uint16 DigitalRenderer::TriSawRectTable[0x100] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};
#else
// Sampled from an 8580R5
const uint16 DigitalRenderer::TriSawTable[0x100] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0808,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1818, 0x3C3C,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1C1C,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x8080, 0x0000, 0x8080, 0x8080,
	0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xE0E0,
	0xF0F0, 0xF0F0, 0xF0F0, 0xF0F0, 0xF8F8, 0xF8F8, 0xFCFC, 0xFEFE
};

const uint16 DigitalRenderer::TriRectTable[0x100] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0xFFFF, 0xFCFC, 0xF8F8, 0xF0F0, 0xF4F4, 0xF0F0, 0xF0F0, 0xE0E0,
	0xECEC, 0xE0E0, 0xE0E0, 0xC0C0, 0xE0E0, 0xC0C0, 0xC0C0, 0xC0C0,
	0xDCDC, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0x8080, 0x8080,
	0xC0C0, 0x8080, 0x8080, 0x8080, 0x8080, 0x8080, 0x0000, 0x0000,
	0xBEBE, 0xA0A0, 0x8080, 0x8080, 0x8080, 0x8080, 0x8080, 0x0000,
	0x8080, 0x8080, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x8080, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x7E7E, 0x7070, 0x6060, 0x0000, 0x4040, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

const uint16 DigitalRenderer::SawRectTable[0x100] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x8080,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x8080, 0x8080,
	0x0000, 0x8080, 0x8080, 0x8080, 0x8080, 0x8080, 0xB0B0, 0xBEBE,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x8080,
	0x0000, 0x0000, 0x0000, 0x8080, 0x8080, 0x8080, 0x8080, 0xC0C0,
	0x0000, 0x8080, 0x8080, 0x8080, 0x8080, 0x8080, 0x8080, 0xC0C0,
	0x8080, 0x8080, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xDCDC,
	0x8080, 0x8080, 0x8080, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0, 0xC0C0,
	0xC0C0, 0xC0C0, 0xC0C0, 0xE0E0, 0xE0E0, 0xE0E0, 0xE0E0, 0xECEC,
	0xC0C0, 0xE0E0, 0xE0E0, 0xE0E0, 0xE0E0, 0xF0F0, 0xF0F0, 0xF4F4,
	0xF0F0, 0xF0F0, 0xF8F8, 0xF8F8, 0xF8F8, 0xFCFC, 0xFEFE, 0xFFFF
};

const uint16 DigitalRenderer::TriSawRectTable[0x100] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x8080, 0x8080,
	0x8080, 0x8080, 0x8080, 0x8080, 0x8080, 0x8080, 0xC0C0, 0xC0C0,
	0xC0C0, 0xC0C0, 0xE0E0, 0xE0E0, 0xE0E0, 0xF0F0, 0xF8F8, 0xFCFC
};
#endif

const uint32 DigitalRenderer::EGTable[16] = {
	(SID_CYCLES << 16) / 9,     (SID_CYCLES << 16) / 32,
	(SID_CYCLES << 16) / 63,    (SID_CYCLES << 16) / 95,
	(SID_CYCLES << 16) / 149,   (SID_CYCLES << 16) / 220,
	(SID_CYCLES << 16) / 267,   (SID_CYCLES << 16) / 313,
	(SID_CYCLES << 16) / 392,   (SID_CYCLES << 16) / 977,
	(SID_CYCLES << 16) / 1954,  (SID_CYCLES << 16) / 3126,
	(SID_CYCLES << 16) / 3906,  (SID_CYCLES << 16) / 11720,
	(SID_CYCLES << 16) / 19531, (SID_CYCLES << 16) / 31251
};

const uint8 DigitalRenderer::EGDRShift[256] = {
	5,5,5,5,5,5,5,5,4,4,4,4,4,4,4,4,
	3,3,3,3,3,3,3,3,3,3,3,3,2,2,2,2,
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
	2,2,2,2,2,2,2,2,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

const int16 DigitalRenderer::SampleTab[16] = {
	(int16) 0x8000, (int16) 0x9111, (int16) 0xa222, (int16) 0xb333, (int16) 0xc444, (int16) 0xd555, (int16) 0xe666, (int16) 0xf777,
	0x0888, 0x1999, 0x2aaa, 0x3bbb, 0x4ccc, 0x5ddd, 0x6eee, 0x7fff,
};


/*
 *  Constructor
 */

DigitalRenderer::DigitalRenderer(C64 *c64) : the_c64(c64)
{
   unsigned i;

	// Link voices together
	voice[0].mod_by = &voice[2];
	voice[1].mod_by = &voice[0];
	voice[2].mod_by = &voice[1];
	voice[0].mod_to = &voice[1];
	voice[1].mod_to = &voice[2];
	voice[2].mod_to = &voice[0];

	// Calculate triangle table
	for (i=0; i<0x1000; i++)
   {
      TriTable[i]        = (i << 4) | (i >> 8);
      TriTable[0x1fff-i] = (i << 4) | (i >> 8);
   }

#ifdef PRECOMPUTE_RESONANCE
#ifdef USE_FIXPOINT_MATHS
	// slow floating point doesn't matter much on startup!
	for (i=0; i<256; i++)
   {
      resonanceLP[i] = FixNo(CALC_RESONANCE_LP(i));
      resonanceHP[i] = FixNo(CALC_RESONANCE_HP(i));
   }
	// Pre-compute the quotient. No problem since int-part is small enough
	sidquot = (int32)((((double)SID_FREQ)*65536) / SAMPLE_FREQ);
	// compute lookup table for sin and cos
	InitFixSinTab();
#else
	for (i=0; i<256; i++)
   {
      resonanceLP[i] = CALC_RESONANCE_LP(i);
      resonanceHP[i] = CALC_RESONANCE_HP(i);
   }
#endif
#endif

	Reset();

	// System specific initialization
	init_sound();
}


/*
 *  Reset emulation
 */

void DigitalRenderer::Reset(void)
{
   unsigned v;
   volume = 0;

   for (v=0; v<3; v++)
   {
      voice[v].wave     = WAVE_NONE;
      voice[v].eg_state = EG_IDLE;
      voice[v].count    = 0;
      voice[v].add      = 0;
      voice[v].freq     = 0;
      voice[v].pw       = 0;
      voice[v].eg_level = 0;
      voice[v].s_level  = 0;
      voice[v].a_add    = EGTable[0];
      voice[v].d_sub    = EGTable[0];
      voice[v].r_sub    = EGTable[0];
      voice[v].gate     = false;
      voice[v].ring     = false;
      voice[v].test     = false;
      voice[v].filter   = false;
      voice[v].sync     = false;
      voice[v].mute     = false;
   }

   f_type = FILT_NONE;
   f_freq = f_res = 0;
#ifdef USE_FIXPOINT_MATHS
   f_ampl = FixNo(1);
   d1 = d2 = g1 = g2 = 0;
   xn1 = xn2 = yn1 = yn2 = 0;
#else
   f_ampl = 1.0;
   d1 = d2 = g1 = g2 = 0.0;
   xn1 = xn2 = yn1 = yn2 = 0.0;
#endif
   sample_in_ptr = 0;
   memset(sample_buf, 0, SAMPLE_BUF_SIZE);
}


/*
 *  Write to register
 */

void DigitalRenderer::WriteRegister(uint16 adr, uint8 byte)
{
   int v;
   if (!ready)
      return;

   v = adr / 7;	// Voice number

   switch (adr)
   {
      case 0:
      case 7:
      case 14:
         voice[v].freq = (voice[v].freq & 0xff00) | byte;
#ifdef USE_FIXPOINT_MATHS
         voice[v].add = sidquot.imul((int)voice[v].freq);
#else
         voice[v].add = (uint32)((float)voice[v].freq * SID_FREQ / SAMPLE_FREQ);
#endif
         break;

      case 1:
      case 8:
      case 15:
         voice[v].freq = (voice[v].freq & 0xff) | (byte << 8);
#ifdef USE_FIXPOINT_MATHS
         voice[v].add = sidquot.imul((int)voice[v].freq);
#else
         voice[v].add = (uint32)((float)voice[v].freq * SID_FREQ / SAMPLE_FREQ);
#endif
         break;

      case 2:
      case 9:
      case 16:
         voice[v].pw = (voice[v].pw & 0x0f00) | byte;
         break;

      case 3:
      case 10:
      case 17:
         voice[v].pw = (voice[v].pw & 0xff) | ((byte & 0xf) << 8);
         break;

      case 4:
      case 11:
      case 18:
         voice[v].wave = (byte >> 4) & 0xf;
         if ((byte & 1) != voice[v].gate)
         {
            if (byte & 1)	// Gate turned on
               voice[v].eg_state = EG_ATTACK;
            else			// Gate turned off
            {
               if (voice[v].eg_state != EG_IDLE)
                  voice[v].eg_state = EG_RELEASE;
            }
         }
         voice[v].gate = byte & 1;
         voice[v].mod_by->sync = byte & 2;
         voice[v].ring = byte & 4;
         if ((voice[v].test = byte & 8) != 0)
            voice[v].count = 0;
         break;

      case 5:
      case 12:
      case 19:
         voice[v].a_add = EGTable[byte >> 4];
         voice[v].d_sub = EGTable[byte & 0xf];
         break;

      case 6:
      case 13:
      case 20:
         voice[v].s_level = (byte >> 4) * 0x111111;
         voice[v].r_sub = EGTable[byte & 0xf];
         break;

      case 22:
         if (byte != f_freq)
         {
            f_freq = byte;
            if (ThePrefs.SIDFilters)
               calc_filter();
         }
         break;

      case 23:
         voice[0].filter = byte & 1;
         voice[1].filter = byte & 2;
         voice[2].filter = byte & 4;
         if ((byte >> 4) != f_res) {
            f_res = byte >> 4;
            if (ThePrefs.SIDFilters)
               calc_filter();
         }
         break;

      case 24:
         volume = byte & 0xf;
         voice[2].mute = byte & 0x80;
         if (((byte >> 4) & 7) != f_type)
         {
            f_type = (byte >> 4) & 7;
#ifdef USE_FIXPOINT_MATHS
            xn1    = xn2 = yn1 = yn2 = 0;
#else
            xn1    = xn2 = yn1 = yn2 = 0.0;
#endif
            if (ThePrefs.SIDFilters)
               calc_filter();
         }
         break;
   }
}


/*
 *  Preferences may have changed
 */

void DigitalRenderer::NewPrefs(Prefs *prefs)
{
	calc_filter();
}


/*
 *  Calculate IIR filter coefficients
 */

void DigitalRenderer::calc_filter(void)
{
#ifdef USE_FIXPOINT_MATHS
	FixPoint fr, arg;

	if (f_type == FILT_ALL)
	{
		d1 = 0;
      d2 = 0;
      g1 = 0;
      g2 = 0;
      f_ampl = FixNo(1);
      return;
	}
	else if (f_type == FILT_NONE)
   {
      d1 = 0;
      d2 = 0;
      g1 = 0;
      g2 = 0;
      f_ampl = 0;
      return;
   }
#else
	float fr, arg;

	// Check for some trivial cases
	if (f_type == FILT_ALL)
   {
      d1 = 0.0; d2 = 0.0;
      g1 = 0.0; g2 = 0.0;
      f_ampl = 1.0;
      return;
   }
   else if (f_type == FILT_NONE)
   {
      d1 = 0.0; d2 = 0.0;
      g1 = 0.0; g2 = 0.0;
      f_ampl = 0.0;
      return;
   }
#endif

	// Calculate resonance frequency
	if (f_type == FILT_LP || f_type == FILT_LPBP)
#ifdef PRECOMPUTE_RESONANCE
		fr = resonanceLP[f_freq];
#else
		fr = CALC_RESONANCE_LP(f_freq);
#endif
	else
#ifdef PRECOMPUTE_RESONANCE
		fr = resonanceHP[f_freq];
#else
		fr = CALC_RESONANCE_HP(f_freq);
#endif

#ifdef USE_FIXPOINT_MATHS
	// explanations see below.
	arg = fr / (SAMPLE_FREQ >> 1);
	if (arg > FixNo(0.99)) {arg = FixNo(0.99);}
	if (arg < FixNo(0.01)) {arg = FixNo(0.01);}

	g2 = FixNo(0.55) + FixNo(1.2) * arg * (arg - 1) + FixNo(0.0133333333) * f_res;
	g1 = FixNo(-2) * g2.sqrt() * fixcos(arg);

	if (f_type == FILT_LPBP || f_type == FILT_HPBP) {g2 += FixNo(0.1);}

	if (g1.abs() >= g2 + 1)
	{
	  if (g1 > 0) {g1 = g2 + FixNo(0.99);}
	  else {g1 = -(g2 + FixNo(0.99));}
	}

	switch (f_type)
	{
	  case FILT_LPBP:
	  case FILT_LP:
		d1 = FixNo(2); d2 = FixNo(1); f_ampl = FixNo(0.25) * (1 + g1 + g2); break;
	  case FILT_HPBP:
	  case FILT_HP:
		d1 = FixNo(-2); d2 = FixNo(1); f_ampl = FixNo(0.25) * (1 - g1 + g2); break;
	  case FILT_BP:
		d1 = 0; d2 = FixNo(-1);
		f_ampl = FixNo(0.25) * (1 + g1 + g2) * (1 + fixcos(arg)) / fixsin(arg);
		break;
	  case FILT_NOTCH:
		d1 = FixNo(-2) * fixcos(arg); d2 = FixNo(1);
		f_ampl = FixNo(0.25) * (1 + g1 + g2) * (1 + fixcos(arg)) / fixsin(arg);
		break;
	  default: break;
	}

#else

	// Limit to <1/2 sample frequency, avoid div by 0 in case FILT_BP below
	arg = fr / (float)(SAMPLE_FREQ >> 1);
	if (arg > 0.99)
		arg = 0.99;
	if (arg < 0.01)
		arg = 0.01;

	// Calculate poles (resonance frequency and resonance)
	g2 = 0.55 + 1.2 * arg * arg - 1.2 * arg + (float)f_res * 0.0133333333;
	g1 = -2.0 * sqrt(g2) * cos(M_PI * arg);

	// Increase resonance if LP/HP combined with BP
	if (f_type == FILT_LPBP || f_type == FILT_HPBP)
		g2 += 0.1;

	// Stabilize filter
	if (fabs(g1) >= g2 + 1.0)
        {
		if (g1 > 0.0)
			g1 = g2 + 0.99;
		else
			g1 = -(g2 + 0.99);
        }

	// Calculate roots (filter characteristic) and input attenuation
	switch (f_type) {

		case FILT_LPBP:
		case FILT_LP:
			d1 = 2.0; d2 = 1.0;
			f_ampl = 0.25 * (1.0 + g1 + g2);
			break;

		case FILT_HPBP:
		case FILT_HP:
			d1 = -2.0; d2 = 1.0;
			f_ampl = 0.25 * (1.0 - g1 + g2);
			break;

		case FILT_BP:
			d1 = 0.0; d2 = -1.0;
			f_ampl = 0.25 * (1.0 + g1 + g2) * (1 + cos(M_PI * arg)) / sin(M_PI * arg);
			break;

		case FILT_NOTCH:
			d1 = -2.0 * cos(M_PI * arg); d2 = 1.0;
			f_ampl = 0.25 * (1.0 + g1 + g2) * (1 + cos(M_PI * arg)) / (sin(M_PI * arg));
			break;

		default:
			break;
	}
#endif
}

/*
 *  Fill one audio buffer with calculated SID sound
 */

void DigitalRenderer::calc_buffer(int16 *buf, long count)
{
   // Get filter coefficients, so the emulator won't change
   // them in the middle of our calculations
#ifdef USE_FIXPOINT_MATHS
   FixPoint cf_ampl = f_ampl;
   FixPoint cd1     = d1, cd2 = d2, cg1 = g1, cg2 = g2;
#else
   float cf_ampl    = f_ampl;
   float cd1        = d1, cd2 = d2, cg1 = g1, cg2 = g2;
#endif
   // Index in sample_buf for reading, 16.16 fixed
   uint32 sample_count = (sample_in_ptr + SAMPLE_BUF_SIZE/2) << 16;

   count >>= 1;	// 16 bit mono output, count is in bytes
   while (count--)
   {
      // Get current master volume from sample buffer,
      // calculate sampled voice
      uint8 master_volume     = sample_buf[(sample_count >> 16) 
         % SAMPLE_BUF_SIZE];
      sample_count           += ((0x138 * 50) << 16) / SAMPLE_FREQ;
      int32 sum_output        = SampleTab[master_volume] << 8;
      int32 sum_output_filter = 0;

      // Loop for all three voices
      for (int j=0; j<3; j++) {
         DRVoice *v = &voice[j];

         // Envelope generators
         uint16 envelope;

         switch (v->eg_state)
         {
            case EG_ATTACK:
               v->eg_level += v->a_add;
               if (v->eg_level > 0xffffff)
               {
                  v->eg_level = 0xffffff;
                  v->eg_state = EG_DECAY;
               }
               break;
            case EG_DECAY:
               if (v->eg_level <= v->s_level || v->eg_level > 0xffffff)
                  v->eg_level = v->s_level;
               else
               {
                  v->eg_level -= v->d_sub >> EGDRShift[v->eg_level >> 16];
                  if (v->eg_level <= v->s_level || v->eg_level > 0xffffff)
                     v->eg_level = v->s_level;
               }
               break;
            case EG_RELEASE:
               v->eg_level -= v->r_sub >> EGDRShift[v->eg_level >> 16];
               if (v->eg_level > 0xffffff)
               {
                  v->eg_level = 0;
                  v->eg_state = EG_IDLE;
               }
               break;
            case EG_IDLE:
               v->eg_level = 0;
               break;
         }
         envelope = (v->eg_level * master_volume) >> 20;

         // Waveform generator
         if (v->mute)
            continue;
         uint16 output;

         if (!v->test)
            v->count += v->add;

         if (v->sync && (v->count > 0x1000000))
            v->mod_to->count = 0;

         v->count &= 0xffffff;

         switch (v->wave)
         {
            case WAVE_TRI:
               if (v->ring)
                  output = TriTable[(v->count 
                        ^ (v->mod_by->count & 0x800000)) >> 11];
               else
                  output = TriTable[v->count >> 11];
               break;
            case WAVE_SAW:
               output = v->count >> 8;
               break;
            case WAVE_RECT:
               if (v->count > (uint32)(v->pw << 12))
                  output = 0xffff;
               else
                  output = 0;
               break;
            case WAVE_TRISAW:
               output = TriSawTable[v->count >> 16];
               break;
            case WAVE_TRIRECT:
               if (v->count > (uint32)(v->pw << 12))
                  output = TriRectTable[v->count >> 16];
               else
                  output = 0;
               break;
            case WAVE_SAWRECT:
               if (v->count > (uint32)(v->pw << 12))
                  output = SawRectTable[v->count >> 16];
               else
                  output = 0;
               break;
            case WAVE_TRISAWRECT:
               if (v->count > (uint32)(v->pw << 12))
                  output = TriSawRectTable[v->count >> 16];
               else
                  output = 0;
               break;
            case WAVE_NOISE:
               if (v->count > 0x100000)
               {
                  output = v->noise = sid_random() << 8;
                  v->count &= 0xfffff;
               }
               else
                  output = v->noise;
               break;
            default:
               output = 0x8000;
               break;
         }
         if (v->filter)
            sum_output_filter += (int16)(output ^ 0x8000) * envelope;
         else
            sum_output        += (int16)(output ^ 0x8000) * envelope;
      }

      // Filter
      if (ThePrefs.SIDFilters)
      {
#ifdef USE_FIXPOINT_MATHS
         int32 xn          = cf_ampl.imul(sum_output_filter);
         int32 yn          = xn + cd1.imul(xn1) + cd2.imul(xn2) - cg1.imul(yn1) - cg2.imul(yn2);
         yn2               = yn1;
         yn1               = yn;
         xn2               = xn1;
         xn1               = xn;
         sum_output_filter = yn;
#else
         float xn          = (float)sum_output_filter * cf_ampl;
         float yn          = xn + cd1 * xn1 + cd2 * xn2 - cg1 * yn1 - cg2 * yn2;
         yn2               = yn1;
         yn1               = yn;
         xn2               = xn1;
         xn1               = xn;
         sum_output_filter = (int32)yn;
#endif
      }

      // Write to buffer
      *buf++ = (sum_output + sum_output_filter) >> 10;
   }
}

/*
 *  SID_linux.i - 6581 emulation, Linux specific stuff
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 *  Linux sound stuff by Bernd Schmidt
 */


#include "VIC.h"

/* Initialization */
void DigitalRenderer::init_sound(void)
{
#if !defined(SF2000)
   sndbufsize   = 882;   // 44100 / 50 = 882 samples per frame
#else
   sndbufsize   = 441;   // 22050 / 50 = 441 samples per frame  
#endif
   sound_buffer = new int16[sndbufsize*2];
   ready        = true;
}


/* Destructor */
DigitalRenderer::~DigitalRenderer()
{
}

/* Pause sound output */
void DigitalRenderer::Pause(void)
{
}


/* Resume sound output */
void DigitalRenderer::Resume(void)
{
}


/*
 * Fill buffer, sample volume (for sampled voice)
 */

extern short signed int SNDBUF[1024*2];

void DigitalRenderer::EmulateLine(void)
{
   static int divisor = 0;
   static int to_output = 0;
   static int buffer_pos = 0;

   if (!ready)
      return;

   sample_buf[sample_in_ptr] = volume;
   sample_in_ptr             = (sample_in_ptr + 1) % SAMPLE_BUF_SIZE;

   /*
    * Now see how many samples have to be added for this line
    */
   divisor += SAMPLE_FREQ;
   while (divisor >= 0)
      divisor -= TOTAL_RASTERS*SCREEN_FREQ, to_output++;

   /*
    * Calculate the sound data only when we have enough to fill
    * the buffer entirely.
    */
   if ((buffer_pos + to_output) >= sndbufsize)
   {
      int datalen  = sndbufsize - buffer_pos;
      to_output   -= datalen;
      calc_buffer(sound_buffer + buffer_pos, datalen*2);
      memcpy(SNDBUF, sound_buffer , sndbufsize*2);
      //	retro_audiocb(sound_buffer, sndbufsize);
      //	write(devfd, sound_buffer, sndbufsize*2);
      buffer_pos = 0;
   }    
}

/*
 *  Open/close the renderer, according to old and new prefs
 */

void MOS6581::open_close_renderer(int old_type, int new_type)
{
   unsigned i;
   if (old_type == new_type)
      return;

   // Delete the old renderer
   delete the_renderer;

   // Create new renderer
   if (new_type == SIDTYPE_DIGITAL)
      the_renderer = new DigitalRenderer(the_c64);
   else
      the_renderer = NULL;

   // Stuff the current register values into the new renderer
   if (the_renderer)
      for (i = 0; i < 25; i++)
         the_renderer->WriteRegister(i, regs[i]);
}
