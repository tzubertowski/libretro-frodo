/*
 *  sysdeps.h - Try to include the right system headers and get other
 *              system-specific stuff right
 *
 *  Frodo (C) 1994-1997,2002-2005 Christian Bauer
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

#ifndef _SYSDEPS_H
#define _SYSDEPS_H

#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <boolean.h>

#include "types.h"

/* Define if you have the <dirent.h> header file, and it defines `DIR'. */
#define HAVE_DIRENT_H 1

/* Define if you have the `mkdir' function. */
#define HAVE_MKDIR 1

/* Define if you have the `rmdir' function. */
#define HAVE_RMDIR 1

/* Define if you have the `sigaction' function. */
#if !defined (__vita__) && !defined(__psp__) && !defined(_3DS) && !defined(_WIN32) && !defined(__SWITCH__)
#define HAVE_SIGACTION 1
#endif

/* Define if you have the `statfs' function. */
#define HAVE_STATFS 1

/* Define if `st_blocks' is member of `struct stat'. */
#define HAVE_STRUCT_STAT_ST_BLOCKS 1

#if HAVE_DIRENT_H
# include <dirent.h>
#else
# define dirent direct
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#ifdef _WIN32
#if !defined(M_PI)
#define M_PI 3.14159265358979323846
#endif
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define LITTLE_ENDIAN_UNALIGNED 1
#endif

#endif
