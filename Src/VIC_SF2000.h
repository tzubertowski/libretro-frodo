/*
 * VIC_SF2000.h - Optimized VIC-II graphics chip emulation for SF2000
 *
 * SF2000 Display: 320x240 (perfect for C64's 320x200)
 * Optimizations:
 * - Direct framebuffer rendering
 * - Cached sprite data
 * - Fast character mode rendering
 * - Optimized color palette conversion
 * - Minimal overhead raster interrupt handling
 */

#ifndef _VIC_SF2000_H
#define _VIC_SF2000_H

#include "types.h"
#include "VIC.h"

// SF2000 display constants
#define SF2000_SCREEN_WIDTH     320
#define SF2000_SCREEN_HEIGHT    240
#define SF2000_BYTES_PER_PIXEL  2    // 16-bit RGB565

// C64 VIC-II constants
#define VIC_SCREEN_WIDTH        320
#define VIC_SCREEN_HEIGHT       200
#define VIC_BORDER_WIDTH        24
#define VIC_BORDER_HEIGHT       20
#define VIC_DISPLAY_WIDTH       (VIC_SCREEN_WIDTH + 2 * VIC_BORDER_WIDTH)
#define VIC_DISPLAY_HEIGHT      (VIC_SCREEN_HEIGHT + 2 * VIC_BORDER_HEIGHT)

// VIC-II color palette in RGB565 format (pre-converted for SF2000)
extern const uint16 vic_palette_rgb565[16];

// Sprite cache for fast rendering
struct SpriteCache {
    bool dirty[8];              // Which sprites need updating
    uint16* sprite_data[8];     // Pre-rendered sprite data in RGB565
    uint8 sprite_x[8];          // X coordinates
    uint8 sprite_y[8];          // Y coordinates
    uint8 sprite_color[8];      // Colors
    bool sprite_enable[8];      // Enable flags
    bool sprite_multicolor[8];  // Multicolor flags
    bool sprite_xexpand[8];     // X expansion
    bool sprite_yexpand[8];     // Y expansion
    uint8 sprite_priority[8];   // Background priority
};

// Character set cache for fast text rendering
struct CharsetCache {
    bool dirty;                 // Character set changed
    uint16* char_data[256];     // Pre-rendered characters in RGB565
    uint8 current_charset;      // Current character set bank
};

// Fast VIC-II emulation for SF2000
class MOS6569_SF2000 : public MOS6569 {
public:
    MOS6569_SF2000(C64 *c64, C64Display *disp, MOS6510 *CPU, uint8 *RAM, uint8 *Char, uint8 *Color);
    ~MOS6569_SF2000();
    
    // Optimized VIC methods
    int EmulateLine();
    void WriteRegister(uint16 adr, uint8 byte);
    uint8 ReadRegister(uint16 adr);
    
    // SF2000 specific optimizations
    void InitializeFastRenderer();
    void RenderLineFast(int line);
    void UpdateSpriteCacheFast();
    void UpdateCharsetCacheFast();
    
    // Fast path optimization methods
    bool CanUseFastPath();
    int EmulateLineFast();
    
    // Direct framebuffer access
    void SetFramebuffer(uint16* framebuffer);
    
    // Performance statistics
    uint32 GetFastLines() const { return fast_lines; }
    uint32 GetSlowLines() const { return slow_lines; }
    
private:
    // SF2000 framebuffer
    uint16* framebuffer;        // Direct access to SF2000 framebuffer
    int framebuffer_width;
    int framebuffer_height;
    
    // Caching systems
    SpriteCache sprite_cache;
    CharsetCache charset_cache;
    
    // Fast rendering state
    bool fast_mode_enabled;
    uint8 last_register_writes[64];  // Cache last register values
    bool registers_dirty[64];        // Which registers changed
    
    // Border color cache
    uint16 border_color_rgb565;
    uint16 background_color_rgb565[4];
    
    // Performance counters
    uint32 fast_lines;
    uint32 slow_lines;
    
    // Fast rendering methods
    void RenderCharacterMode(int line);
    void RenderBitmapMode(int line);
    void RenderSprites(int line);
    void RenderBorder(int line);
    
    // Cache management
    void InvalidateCharsetCache();
    void InvalidateSpriteCache(int sprite_num);
    void UpdateColorCaches();
    
    // Utility methods
    inline uint16 GetPixelColor(uint8 char_data, uint8 color_data, int pixel);
    inline void DrawCharacterLine(int line, int char_x, int char_y, uint8 char_code, uint8 color);
    inline void DrawSpriteLine(int line, int sprite_num);
    
    // Memory access optimization
    inline uint8 ReadVideoMatrix(uint16 addr);
    inline uint8 ReadColorRAM(uint16 addr);
    inline uint8 ReadCharacterData(uint16 addr);
};

// Inline implementations for maximum performance
inline uint8 MOS6569_SF2000::ReadVideoMatrix(uint16 addr) {
    // TODO: Implement fast video matrix access when we can access base class members
    // For now, use a placeholder approach
    return 0;
}

inline uint8 MOS6569_SF2000::ReadColorRAM(uint16 addr) {
    // TODO: Implement fast color RAM access when we can access base class members
    // For now, use a placeholder approach
    return 0xF0;
}

inline uint8 MOS6569_SF2000::ReadCharacterData(uint16 addr) {
    // TODO: Implement fast character data access when we can access base class members
    // For now, use a placeholder approach
    return 0;
}

inline uint16 MOS6569_SF2000::GetPixelColor(uint8 char_data, uint8 color_data, int pixel) {
    // Fast pixel color calculation
    uint8 color_index;
    if (char_data & (0x80 >> pixel)) {
        color_index = color_data & 0x0F;
    } else {
        color_index = 0;  // TODO: Get background color from register when accessible
    }
    return vic_palette_rgb565[color_index];
}

// Pre-calculated VIC-II color palette in RGB565 format (defined in .cpp file)
extern const uint16 vic_palette_rgb565[16];

#endif // _VIC_SF2000_H