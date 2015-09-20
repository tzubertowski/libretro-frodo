/*
 *  SID_linux.i - 6581 emulation, Linux specific stuff
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 *  Linux sound stuff by Bernd Schmidt
 */


#include "VIC.h"

#warning RETRO
/*
 *  Initialization
 */

void DigitalRenderer::init_sound(void)
{
	sndbufsize=882;
	sound_buffer = new int16[sndbufsize*2];
    ready = true;
}


/*
 *  Destructor
 */

DigitalRenderer::~DigitalRenderer()
{
}


/*
 *  Pause sound output
 */

void DigitalRenderer::Pause(void)
{
}


/*
 * Resume sound output
 */

void DigitalRenderer::Resume(void)
{
}


/*
 * Fill buffer, sample volume (for sampled voice)
 */

extern void retro_audiocb(short int *sound_buffer,int sndbufsize);

void DigitalRenderer::EmulateLine(void)
{
    static int divisor = 0;
    static int to_output = 0;
    static int buffer_pos = 0;

    if (!ready)
	return;

	sample_buf[sample_in_ptr] = volume;
	sample_in_ptr = (sample_in_ptr + 1) % SAMPLE_BUF_SIZE;

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
    if ((buffer_pos + to_output) >= sndbufsize) {

	int datalen = sndbufsize - buffer_pos;
	to_output -= datalen;
	calc_buffer(sound_buffer + buffer_pos, datalen*2);
	retro_audiocb(sound_buffer, sndbufsize);
//	write(devfd, sound_buffer, sndbufsize*2);
	buffer_pos = 0;
    }    
}
