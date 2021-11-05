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
#include "../../Src/types.h"

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

#endif
