/*
 *  main_x.i - Main program, X specific stuff
 *
 *  Frodo (C) 1994-1997,2002 Christian Bauer
 */

#include "Version.h"

#include "core-log.h"
#ifndef NO_LIBCO
#include "libco.h"
extern cothread_t mainThread;
extern cothread_t emuThread;
#endif

extern int init_graphics(void);

Frodo *the_app;

// Global variables
char Frodo::prefs_path[256] = "";

char Frodo::device_path[256] = "";

/*
 *  Create application object and start it
 */

int skel_main(int argc, char **argv)
{

	timeval tv;
	gettimeofday(&tv, NULL);
	srand(tv.tv_usec);

	LOGI("%s by Christian Bauer\n", VERSION_STRING);
	if (!init_graphics())
		return 0;
	fflush(stdout);

	the_app = new Frodo();
	the_app->ArgvReceived(argc, argv);
	the_app->ReadyToRun();
#ifndef NO_LIBCO
	delete the_app;
#endif
	return 0;
}

#ifdef NO_LIBCO
void quit_frodo_emu(){
	LOGI("delete c64&app\n");
	delete TheC64;
	delete the_app;
}
#endif

/*
 *  Constructor: Initialize member variables
 */

Frodo::Frodo()
{
	TheC64 = NULL;
}


/*
 *  Process command line arguments
 */

void Frodo::ArgvReceived(int argc, char **argv)
{
/*
	if (argc == 2)
		strncpy(prefs_path, argv[1], 255);
*/

	if (argc == 2){
			strncpy(device_path, argv[1], 255);
	}	

}


/*
 *  Arguments processed, run emulation
 */

void Frodo::ReadyToRun(void)
{
	getcwd(AppDirPath, 256);

	// Load preferences
	if (!prefs_path[0]) {

#if  defined(__ANDROID__) || defined(ANDROID)
		char *home = "/mnt/sdcard";
#else
 		char *home = getenv("HOME");
#endif

		if (home != NULL && strlen(home) < 240) {
			strncpy(prefs_path, home, 200);
			strcat(prefs_path, "/");
		}
		strcat(prefs_path, ".frodorc");
	}

	LOGI("pref:(%s) dev:(%s)\n",prefs_path,device_path);

	ThePrefs.Load(prefs_path);

	ThePrefs.set_drive8(device_path,0);

	// Create and start C64
	TheC64 = new C64;

	load_rom_files();

#ifndef NO_LIBCO
	co_switch(mainThread); //return mainthread before enter C64thread
#endif

	TheC64->Run();

#ifndef NO_LIBCO
	delete TheC64;
#endif
}



/*
 *  Run preferences editor
 */

bool Frodo::RunPrefsEditor(void)
{
	Prefs *prefs = new Prefs(ThePrefs);
	bool result = prefs->ShowEditor(false, prefs_path);
	if (result) {
		TheC64->NewPrefs(prefs);
		ThePrefs = *prefs;
	}
	delete prefs;
	return result;
}


/*
 *  Determine whether path name refers to a directory
 */

bool IsDirectory(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);

}

