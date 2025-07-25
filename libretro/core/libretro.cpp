#include <stdarg.h>
#include <libretro.h>
#include <compat/strl.h>

#include "libretro-core.h"
#include "libretro_core_options.h"
#include "Version.h"

#ifdef NO_LIBCO
#include "main.h"
#include "C64.h"
#include "Display.h"
#include "Prefs.h"
#else
cothread_t mainThread;
cothread_t emuThread;
#endif

int CROP_WIDTH;
int CROP_HEIGHT;
int VIRTUAL_WIDTH;
int retrow=1024; 
int retroh=1024;

// Frameskip variables
int frameskip_type = 0;     // 0 = fixed, 1 = auto
int frameskip_value = 0;    // Number of frames to skip
int frameskip_counter = 0;  // Current frame counter

// Overscan variables - Initialize with default "auto" values
int overscan_crop_left = 24;
int overscan_crop_right = 24;
int overscan_crop_top = 12;
int overscan_crop_bottom = 12;

// Frodo_1541emul variable
bool frodo_1541emul = true;

// Shutdown flag
static bool shutdown_requested = false;

// Simple 8x8 font bitmap for splash screen text
static const unsigned char font_8x8[][8] = {
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Space
   {0x3C, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x42, 0x00}, // A
   {0x7C, 0x42, 0x42, 0x7C, 0x42, 0x42, 0x7C, 0x00}, // B
   {0x3C, 0x42, 0x40, 0x40, 0x40, 0x42, 0x3C, 0x00}, // C
   {0x7C, 0x42, 0x42, 0x42, 0x42, 0x42, 0x7C, 0x00}, // D
   {0x7E, 0x40, 0x40, 0x7C, 0x40, 0x40, 0x7E, 0x00}, // E
   {0x7E, 0x40, 0x40, 0x7C, 0x40, 0x40, 0x40, 0x00}, // F
   {0x3C, 0x42, 0x40, 0x4E, 0x42, 0x42, 0x3C, 0x00}, // G
   {0x42, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x42, 0x00}, // H
   {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00}, // I
   {0x0E, 0x04, 0x04, 0x04, 0x04, 0x44, 0x38, 0x00}, // J
   {0x42, 0x44, 0x48, 0x70, 0x48, 0x44, 0x42, 0x00}, // K
   {0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7E, 0x00}, // L
   {0x42, 0x66, 0x5A, 0x42, 0x42, 0x42, 0x42, 0x00}, // M
   {0x42, 0x62, 0x52, 0x4A, 0x46, 0x42, 0x42, 0x00}, // N
   {0x3C, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00}, // O
   {0x7C, 0x42, 0x42, 0x7C, 0x40, 0x40, 0x40, 0x00}, // P
   {0x38, 0x44, 0x44, 0x44, 0x4C, 0x44, 0x3A, 0x00}, // Q
   {0x7C, 0x42, 0x42, 0x7C, 0x48, 0x44, 0x42, 0x00}, // R
   {0x3C, 0x42, 0x40, 0x3C, 0x02, 0x42, 0x3C, 0x00}, // S
   {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00}, // T
   {0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00}, // U
   {0x42, 0x42, 0x42, 0x42, 0x24, 0x24, 0x18, 0x00}, // V
   {0x42, 0x42, 0x42, 0x42, 0x5A, 0x66, 0x42, 0x00}, // W
   {0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x42, 0x00}, // X
   {0x42, 0x42, 0x24, 0x18, 0x18, 0x18, 0x18, 0x00}, // Y
   {0x7E, 0x04, 0x08, 0x10, 0x20, 0x40, 0x7E, 0x00}, // Z
   {0x38, 0x44, 0x44, 0x44, 0x44, 0x44, 0x38, 0x00}, // 0
   {0x10, 0x30, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00}, // 1
   {0x3C, 0x42, 0x02, 0x0C, 0x30, 0x40, 0x7E, 0x00}, // 2
   {0x3C, 0x42, 0x02, 0x1C, 0x02, 0x42, 0x3C, 0x00}, // 3
   {0x04, 0x0C, 0x14, 0x24, 0x7E, 0x04, 0x04, 0x00}, // 4
   {0x7E, 0x40, 0x7C, 0x02, 0x02, 0x42, 0x3C, 0x00}, // 5
   {0x1C, 0x20, 0x40, 0x7C, 0x42, 0x42, 0x3C, 0x00}, // 6
   {0x7E, 0x02, 0x04, 0x08, 0x10, 0x20, 0x20, 0x00}, // 7
   {0x3C, 0x42, 0x42, 0x3C, 0x42, 0x42, 0x3C, 0x00}, // 8
   {0x3C, 0x42, 0x42, 0x3E, 0x02, 0x04, 0x38, 0x00}, // 9
   {0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00}, // .
   {0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x00}, // /
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00}, // :
   {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00}, // -
};

// Character to font index mapping
static int char_to_font_index(char c) {
   if (c >= 'A' && c <= 'Z') return c - 'A' + 1;
   if (c >= 'a' && c <= 'z') return c - 'a' + 1;
   if (c >= '0' && c <= '9') return c - '0' + 27;
   if (c == '.') return 37;
   if (c == '/') return 38;
   if (c == ':') return 39;
   if (c == '-') return 40;
   return 0; // Space for any unsupported character
}

// Draw a character at x, y
static void draw_char(int x, int y, char c, PIXEL_TYPE color) {
   int font_idx = char_to_font_index(c);
   
   // Bounds check for font array (0-40)
   if (font_idx < 0 || font_idx >= 41) {
      font_idx = 0; // Default to space
   }
   
   const unsigned char *font_data = font_8x8[font_idx];
   
   for (int row = 0; row < 8; row++) {
      unsigned char byte = font_data[row];
      for (int col = 0; col < 8; col++) {
         if (byte & (0x80 >> col)) {
            int px = x + col;
            int py = y + row;
            if (px >= 0 && px < retrow && py >= 0 && py < retroh && 
                px < 1024 && py < 1024 && (py * retrow + px) < (1024 * 1024)) {
               Retro_Screen[py * retrow + px] = color;
            }
         }
      }
   }
}

// Draw a string at x, y
static void draw_string(int x, int y, const char *str, PIXEL_TYPE color) {
   int curr_x = x;
   while (*str) {
      if (*str == ' ') {
         curr_x += 8;
      } else {
         draw_char(curr_x, y, *str, color);
         curr_x += 8;
      }
      str++;
   }
}

// Splash screen function for FRODO DASH V
static void draw_splash_screen(void)
{
   // Background color: Pink #e791bf (RGB 231, 145, 191)
   // Convert to RGB565: R=231>>3=28, G=145>>2=36, B=191>>3=23
#ifdef RENDER16B
   uint16_t bg_color = ((231 >> 3) << 11) | ((145 >> 2) << 5) | (191 >> 3); // RGB565: Pink #e791bf
   uint16_t text_color = 0xFFFF; // White
#else
   uint32_t bg_color = 0xFFE791BF; // ARGB: Pink #e791bf
   uint32_t text_color = 0xFFFFFFFF; // White
#endif

   // Fill entire screen buffer with pink background
   // Ensure we don't exceed buffer bounds (1024x1024)
   // Only fill if screen dimensions are safe
   if (retrow <= 1024 && retroh <= 1024) {
      for (int y = 0; y < retroh; y++) {
         for (int x = 0; x < retrow; x++) {
            int index = y * retrow + x;
            if (index >= 0 && index < (1024 * 1024)) {
               Retro_Screen[index] = bg_color;
            }
         }
      }
   } else {
      // Fallback: fill first 1024x1024 if dimensions are too large
      for (int i = 0; i < (1024 * 1024); i++) {
         Retro_Screen[i] = bg_color;
      }
   }

   // Calculate center position for text
   int center_x = retrow / 2;
   int center_y = retroh / 2;
   
   // Main title: "FRODO DASH V." - 50 pixels above center
   char title[] = "FRODO DASH V.";
   int title_width = strlen(title) * 8;
   draw_string(center_x - title_width / 2, center_y - 50, title, text_color);
   
   // Credits: "MOD BY PROSTY" - 30 pixels above center
   char credits[] = "MOD BY PROSTY";
   int credits_width = strlen(credits) * 8;
   draw_string(center_x - credits_width / 2, center_y - 30, credits, text_color);
   
   // Discord link: "discord.gg/bvfKkHvsXK" - 50 pixels from bottom
   char discord[] = "discord.gg/bvfKkHvsXK";
   int discord_width = strlen(discord) * 8;
   draw_string(center_x - discord_width / 2, retroh - 50, discord, text_color);
   
   // Version date: dynamic compilation date - 30 pixels from bottom
   char version[] = "ver " __DATE__;
   int version_width = strlen(version) * 8;
   draw_string(center_x - version_width / 2, retroh - 30, version, text_color);
}

#ifdef NO_LIBCO
extern C64 *TheC64;
extern void quit_frodo_emu(void);
#endif

extern int SHIFTON,pauseg,SND ,snd_sampler;
extern short signed int SNDBUF[1024*2];
extern char RPATH[512];

#include "cmdline.c"

extern void texture_init(void);
extern void texture_uninit(void);

const char *retro_save_directory;
const char *retro_system_directory;
const char *retro_content_directory;

static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;
static void RETRO_CALLCONV fallback_log(enum retro_log_level level, const char *fmt, ...);
retro_log_printf_t log_cb = fallback_log;

static void RETRO_CALLCONV fallback_log(
      enum retro_log_level level, const char *fmt, ...) { }

void retro_set_environment(retro_environment_t cb)
{
   struct retro_log_callback log;
   bool no_rom = true;

   environ_cb = cb;

   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
     log_cb = log.log;

   libretro_set_core_options(environ_cb);

   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_rom);
}

static void update_variables(void)
{
   struct retro_variable var;
   var.key   = "frodo_resolution";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      char *pch;
      char str[100];
      strlcpy(str, var.value, sizeof(str));

      pch = strtok(str, "x");
      if (pch)
         retrow = strtoul(pch, NULL, 0);
      pch = strtok(NULL, "x");
      if (pch)
         retroh = strtoul(pch, NULL, 0);

      //FIXME remove force 384x288
      retrow        = WINDOW_WIDTH;
      retroh        = WINDOW_HEIGHT;
      CROP_WIDTH    = retrow;
      CROP_HEIGHT   = (retroh-80);
      VIRTUAL_WIDTH = retrow;
      texture_init();
      //reset_screen();
   }

   // Handle Processor-level 1541 emulation
   var.key   = "frodo_1541emul";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "true") == 0)
      {
         frodo_1541emul = true;  // Enable processor-level 1541 emulation
      }
      else
      {
         frodo_1541emul = false;  // Disable processor-level 1541 emulation
      }
      ThePrefs.Emul1541Proc = frodo_1541emul;
      
      if (log_cb)
         log_cb(RETRO_LOG_INFO, "Emul1541Proc set to: %s\n", 
                var.value);
   }
   
   // Handle frameskip option
   var.key   = "frodo_frameskip";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "auto") == 0)
      {
         frameskip_type = 1;  // Auto frameskip
         frameskip_value = 0; // Will be determined dynamically
      }
      else
      {
         frameskip_type = 0;  // Fixed frameskip
         frameskip_value = strtoul(var.value, NULL, 0);
      }
      
      if (log_cb)
         log_cb(RETRO_LOG_INFO, "Frameskip set to: %s (type=%d, value=%d)\n", 
                var.value, frameskip_type, frameskip_value);
   }

   // Handle overscan option
   var.key   = "frodo_overscan";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "none") == 0)
      {
         overscan_crop_left = 0;
         overscan_crop_right = 0;
         overscan_crop_top = 0;
         overscan_crop_bottom = 0;
      }
      else if (strcmp(var.value, "small") == 0)
      {
         overscan_crop_left = 8;
         overscan_crop_right = 8;
         overscan_crop_top = 4;
         overscan_crop_bottom = 4;
      }
      else if (strcmp(var.value, "medium") == 0)
      {
         overscan_crop_left = 16;
         overscan_crop_right = 16;
         overscan_crop_top = 8;
         overscan_crop_bottom = 8;
      }
      else if (strcmp(var.value, "large") == 0)
      {
         overscan_crop_left = 32;
         overscan_crop_right = 32;
         overscan_crop_top = 16;
         overscan_crop_bottom = 16;
      }
      else // "auto" or any other value
      {
         overscan_crop_left = 24;
         overscan_crop_right = 24;
         overscan_crop_top = 12;
         overscan_crop_bottom = 12;
      }
      
      if (log_cb)
         log_cb(RETRO_LOG_INFO, "[FRODO] Overscan set to: %s (L=%d, R=%d, T=%d, B=%d)\n", 
                var.value, overscan_crop_left, overscan_crop_right, 
                overscan_crop_top, overscan_crop_bottom);
   }
}

static void retro_wrap_emulator(void)
{
   pre_main(RPATH);
#ifndef NO_LIBCO
   pauseg=-1;

   environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, 0); 

   // Were done here
   co_switch(mainThread);

   // Dead emulator, but libco says not to return
   // Check for shutdown to avoid infinite loop
   while(!shutdown_requested)
      co_switch(mainThread);
#endif
}

void Emu_init(void)
{
   update_variables();

   memset(Key_Sate,0,512);
   memset(Key_Sate2,0,512);

#ifndef NO_LIBCO
   if(!emuThread && !mainThread)
   {
      mainThread = co_active();
      emuThread = co_create(65536*sizeof(void*), retro_wrap_emulator);
   }
#else
   retro_wrap_emulator();
#endif

}

void Emu_uninit(void)
{
#ifdef NO_LIBCO
   quit_frodo_emu();
#else
   shutdown_requested = true;
#endif
   texture_uninit();
}

void retro_shutdown_core(void)
{
#ifdef NO_LIBCO
	quit_frodo_emu();
#else
   shutdown_requested = true;
#endif
   texture_uninit();
   environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
}

/* TODO/FIXME - implement this */
void retro_reset(void) { }

void retro_init(void)
{    	
   const char *save_dir        = NULL;
   const char *content_dir     = NULL;
   const char *system_dir      = NULL;
#ifndef RENDER16B
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
#else
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
#endif

   // if defined, use the system directory			
   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir) && system_dir)
      retro_system_directory = system_dir;		

   // if defined, use the system directory			
   if (environ_cb(RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY, &content_dir) && content_dir)
      retro_content_directory = content_dir;		

   // If save directory is defined use it, otherwise use system directory
   if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir) && save_dir)
      retro_save_directory = *save_dir ? save_dir : retro_system_directory;      
   else
   {
      // make retro_save_directory the same in case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY is not implemented by the frontend
      retro_save_directory=retro_system_directory;
   }

   environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);

	struct retro_input_descriptor inputDescriptors[] = {
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "R2" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "L2" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "R3" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "L3" },
		{ 0 },
	};
	environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, &inputDescriptors);
#ifndef NO_LIBCO
   Emu_init();
#endif
   texture_init();

}

void retro_deinit(void)
{	 
   Emu_uninit(); 

#ifndef NO_LIBCO
   if(emuThread)
   {	 
      co_delete(emuThread);
      emuThread = 0;
   }
#endif
   shutdown_requested = false; // Reset for next time
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   (void)port;
   (void)device;
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "Frodo";
   info->library_version  = "V4_2";
   info->valid_extensions = "d64|t64|x64|p00|lnx|zip";
   info->need_fullpath    = true;
   info->block_extract = false;

}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
  struct retro_game_geometry geom = {
				     (unsigned int) retrow,
				     (unsigned int) retroh,
				     1024, 1024,4.0 / 3.0 };
#if !defined(SF2000)
   struct retro_system_timing timing = { 50.0, 44100.0 };
#else
   struct retro_system_timing timing = { 50.0, 22050.0 };  // SF2000: PAL 50Hz, 22050Hz audio
#endif

   info->geometry = geom;
   info->timing   = timing;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
   audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

#ifdef NO_LIBCO
/* TODO/FIXME - nolibco Gui endless loop -> no retro_run() call */
void retro_run_gui(void)
{
   bool updated = false;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      update_variables();

   video_cb(Retro_Screen,retrow,retroh,retrow<<PIXEL_BYTES);
}
#endif

static void (*pulse_handler)(int);

void libretro_pulse_handler(void (*handler)(int))
{
   pulse_handler = handler;
}

void retro_run(void)
{
   static int pulse_counter = 0;
   int x;

   bool updated = false;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE,
            &updated) && updated)
      update_variables();

   if (pulse_counter > 20 && pulse_handler)
      pulse_handler(0);

   if(pauseg==0)
   {
      if(SND==1)
         for(x=0;x<snd_sampler;x++)
            audio_cb(SNDBUF[x],SNDBUF[x]);
#ifdef NO_LIBCO
#ifndef FRODO_SC
      for(x=0;x<312;x++)
#else
         for(x=0;x<63*312;x++) 
#endif
            TheC64->thread_func();
#endif
   }   

   // Simple frame delay approach: Only apply frameskip after initial boot time
   static int frame_count = 0;
   frame_count++;
   
   // Show splash screen for first 180 frames (3 seconds at 60fps)
   if (frame_count <= 180)
   {
      draw_splash_screen();
      video_cb(Retro_Screen,retrow,retroh,retrow<<PIXEL_BYTES);
      
      // Upload silent audio samples during splash display
      if(SND==1)
         for(int x=0;x<snd_sampler;x++)
            audio_cb(0,0);
      
      // Don't switch to emulator thread during splash - wait for core boot
      return;
   }
   
   // Give the C64 300 frames (~6 seconds at 50fps) to complete boot sequence
   bool allow_frameskip = (frame_count > 300);

   // Frameskip logic - only apply when emulator is running AND after boot delay
   if(pauseg==0 && frameskip_value > 0 && allow_frameskip)
   {
      if (frameskip_counter < frameskip_value)
      {
         // Skip this frame - don't call video_cb at all
         frameskip_counter++;
      }
      else
      {
         // Render this frame and reset counter
         frameskip_counter = 0;
         video_cb(Retro_Screen,retrow,retroh,retrow<<PIXEL_BYTES);
      }
   }
   else
   {
      // Always render when paused/booting or frameskip disabled
      video_cb(Retro_Screen,retrow,retroh,retrow<<PIXEL_BYTES);
   }

#ifndef NO_LIBCO   
   co_switch(emuThread);
#endif

}

bool retro_load_game(const struct retro_game_info *info)
{
   const char *full_path = NULL;

#ifndef NO_LIBCO
   if (!mainThread || !emuThread)
   {
      log_cb(RETRO_LOG_ERROR, "libco init failed\n", __LINE__);
      return false;
   }
#endif

   if (info)
      full_path = info->path;

   if (full_path)
     strcpy(RPATH,full_path);
   else
     memset(RPATH, 0, sizeof(RPATH));

   update_variables();

#ifdef RENDER16B
	memset(Retro_Screen,0,1024*1024*2);
#else
	memset(Retro_Screen,0,1024*1024*2*2);
#endif
	memset(SNDBUF,0,1024*2*2);

#ifndef NO_LIBCO
	co_switch(emuThread);
#else
	Emu_init();
#endif
   return true;
}

void retro_unload_game(void)
{
   pauseg=0;
#ifndef NO_LIBCO
   shutdown_requested = true;
#endif
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

bool retro_load_game_special(
      unsigned type, const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

size_t retro_serialize_size(void)
{
   return 0;
}

bool retro_serialize(void *data_, size_t size)
{
   return false;
}

bool retro_unserialize(const void *data_, size_t size)
{
   return false;
}

void *retro_get_memory_data(unsigned id)
{
   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   return 0;
}

void retro_cheat_reset(void) { }

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}

