/*
	modded for libretro-uae
*/

#ifndef HATARI_UTYPE_H
#define HATARI_UTYPE_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <ctype.h>

typedef uint8_t	 Uint8;
typedef int8_t	 Sint8;
typedef uint16_t Uint16;
typedef int16_t	 Sint16;
typedef uint32_t Uint32;
typedef int32_t	 Sint32;

typedef int8_t     int8;
typedef int16_t    int16;
typedef int32_t   int32;
typedef uint8_t   uint8;
typedef uint16_t  uint16;
typedef uint32_t  uint32;


#if defined (__vita__) || defined(__psp__)
#define	getcwd(a,b)	"/"
#define chdir(a) 0
#endif

#ifdef PS3PORT
#include <sdk_version.h>
#include <cell.h>
#include <string.h>
#define	getcwd(a,b)	"/"
#define S_ISDIR(x) (x & CELL_FS_S_IFDIR)
#define F_OK 0
#define ftello ftell
#define chdir(a) 0
#define getenv(a)	"/dev_hdd0/HOMEBREW/ST/"
#define tmpfile() NULL
#endif

#ifdef _WIN32
#define PATHSEP '\\'
#else
#define PATHSEP '/'
#endif

#endif
