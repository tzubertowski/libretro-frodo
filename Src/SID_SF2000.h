/*
 * SID_SF2000.h - Optimized SID sound chip emulation for SF2000
 *
 * SF2000 Audio: Mono output, simplified for performance
 * Optimizations:
 * - Fast 3-voice synthesis
 * - Lookup tables for waveforms
 * - Simplified filter emulation
 * - Direct audio buffer output
 * - MIPS-optimized calculations
 */

#ifndef _SID_SF2000_H
#define _SID_SF2000_H

#include "types.h"
#include "SID.h"

// SF2000 audio constants
#define SF2000_SAMPLE_RATE      22050   // SF2000's audio sample rate
#define SF2000_BUFFER_SIZE      1024    // Audio buffer size
#define SF2000_MONO_OUTPUT      1       // SF2000 only supports mono

// SID constants
#define SID_VOICES              3
#define SID_CYCLES_PER_SAMPLE   40      // Approximate for 22050Hz at 1MHz
#define SID_FREQUENCY_MAX       0xFFFF

// Waveform types
enum SIDWaveform {
    WAVE_NONE = 0,
    WAVE_TRIANGLE = 1,
    WAVE_SAWTOOTH = 2,
    WAVE_PULSE = 4,
    WAVE_NOISE = 8
};

// Voice state for fast emulation
struct SIDVoice {
    // Frequency and phase
    uint16 frequency;           // Current frequency value
    uint32 phase_accumulator;   // Phase accumulator (24.8 fixed point)
    uint32 phase_increment;     // Phase increment per sample
    
    // Waveform
    uint8 waveform;             // Current waveform
    uint16 pulse_width;         // Pulse width (12-bit)
    
    // Envelope
    uint8 attack;               // Attack rate
    uint8 decay;                // Decay rate  
    uint8 sustain;              // Sustain level
    uint8 release;              // Release rate
    uint8 envelope_state;       // Current envelope state
    uint16 envelope_level;      // Current envelope level
    uint32 envelope_counter;    // Envelope timing counter
    
    // Control
    bool gate;                  // Gate bit
    bool ring_mod;              // Ring modulation
    bool sync;                  // Oscillator sync
    bool test;                  // Test bit
    
    // Output
    int16 output;               // Current voice output
    bool active;                // Voice is producing sound
};

// Fast lookup tables
struct SIDTables {
    int16 triangle_table[4096];     // Triangle waveform lookup
    int16 sawtooth_table[4096];     // Sawtooth waveform lookup  
    int16 pulse_table[4096];        // Pulse waveform lookup
    uint16 noise_table[1024];       // Noise values
    uint32 frequency_table[0x10000]; // Frequency to phase increment
    uint16 envelope_table[256];     // Envelope rate table
};

// Optimized SID emulation for SF2000
class MOS6581_SF2000 : public MOS6581 {
public:
    MOS6581_SF2000(C64 *c64);
    ~MOS6581_SF2000();
    
    // Optimized SID methods  
    void WriteRegister(uint16 adr, uint8 byte);
    uint8 ReadRegister(uint16 adr);
    void EmulateLine();
    
    // SF2000 specific optimizations
    void InitializeFastSID();
    void GenerateSamples(int16* buffer, int num_samples);
    void SetSampleRate(int sample_rate);
    
    // Performance statistics
    uint32 GetFastSamples() const { return fast_samples; }
    uint32 GetSlowSamples() const { return slow_samples; }
    
private:
    // Fast emulation state
    SIDVoice voices[SID_VOICES];
    static SIDTables tables;
    static bool tables_initialized;
    
    // Audio output
    int sample_rate;
    int cycles_per_sample;
    int cycle_counter;
    
    // Filter state (simplified)
    bool filter_enabled;
    uint8 filter_cutoff;
    uint8 filter_resonance;
    uint8 filter_mode;
    int16 filter_output;
    
    // Performance counters
    uint32 fast_samples;
    uint32 slow_samples;
    
    // Master volume
    uint8 master_volume;
    
    // Internal methods
    void InitializeTables();
    void UpdateVoiceFrequency(int voice);
    void UpdateVoiceWaveform(int voice);
    void UpdateVoiceEnvelope(int voice);
    void UpdateFilter();
    
    // Fast synthesis methods
    int16 GenerateTriangle(SIDVoice& voice);
    int16 GenerateSawtooth(SIDVoice& voice);
    int16 GeneratePulse(SIDVoice& voice);
    int16 GenerateNoise(SIDVoice& voice);
    int16 ProcessEnvelope(SIDVoice& voice, int16 sample);
    int16 ProcessFilter(int16 sample);
    
    // Utility methods
    inline uint32 CalculatePhaseIncrement(uint16 frequency);
    inline uint16 GetEnvelopeRate(uint8 rate);
    inline int16 ClampSample(int32 sample);
};

// Inline implementations for maximum performance
inline uint32 MOS6581_SF2000::CalculatePhaseIncrement(uint16 frequency) {
    // Use lookup table for fast frequency conversion
    return tables.frequency_table[frequency];
}

inline uint16 MOS6581_SF2000::GetEnvelopeRate(uint8 rate) {
    // Use lookup table for envelope rates
    return tables.envelope_table[rate & 0x0F];
}

inline int16 MOS6581_SF2000::ClampSample(int32 sample) {
    // Fast clamping to 16-bit range
    if (sample > 32767) return 32767;
    if (sample < -32768) return -32768;
    return (int16)sample;
}

#endif // _SID_SF2000_H