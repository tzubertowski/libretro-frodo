#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int mkstemp2(char *path)
{
	int fd;   
	char *start, *trv;
    struct stat sbuf;

	pid_t pid;
    pid = getpid();

	/* To guarantee multiple calls generate unique names even if
	   the file is not created. 676 different possibilities with 7
	   or more X's, 26 with 6 or less. */
	static char xtra[2] = {'a','a'};
	int xcnt = 0;

	/* Move to end of path and count trailing X's. */
	for (trv = path; *trv; ++trv)
		if (*trv == 'X')
			xcnt++;
		else
			xcnt = 0;	

	/* Use at least one from xtra.  Use 2 if more than 6 X's. */
	if (*(trv - 1) == 'X')
		*--trv = xtra[0];
	if (xcnt > 6 && *(trv - 1) == 'X')
		*--trv = xtra[1];

	/* Set remaining X's to pid digits with 0's to the left. */
	while (*--trv == 'X') {
		*trv = (pid % 10) + '0';
		pid /= 10;
	}

	/* update xtra for next call. */
	if (xtra[0] != 'z')
		xtra[0]++;
	else {
		xtra[0] = 'a';
		if (xtra[1] != 'z')
			xtra[1]++;
		else
			xtra[1] = 'a';
	}

	/*
	 * check the target directory; if you have six X's and it
	 * doesn't exist this runs for a *very* long time.
	 */
	for (start = trv + 1;; --trv) {
		if (trv <= path)
			break;
		if (*trv == '/') {
			*trv = '\0';
			if (stat(path, &sbuf))
				return (0);
			if ((sbuf.st_mode & S_IFDIR) != S_IFDIR) {
				errno = ENOTDIR;
				return (0);
			}
			*trv = '/';
			break;
		}
	}

	for (;;) {
		if ((fd=open(path, O_CREAT|O_EXCL|O_RDWR|O_BINARY, 0600)) >= 0){
			return (fd);
		}
		if (errno != EEXIST){
			return (0);
		}
		/* tricky little algorithm for backward compatibility */
		for (trv = start;;) {
			if (!*trv)
				return (0);
			if (*trv == 'z')
				*trv++ = 'a';
			else {
				if (isdigit((unsigned char)*trv))
					*trv = 'a';
				else
					++*trv;
				break;
			}
		}
	}
	/*NOTREACHED*/
}

class ScopedSignalBlocker {
 public:
  ScopedSignalBlocker() {
    sigset_t set;
    sigfillset(&set);
    sigprocmask(SIG_BLOCK, &set, &old_set_);
  }

  ~ScopedSignalBlocker() {
    sigprocmask(SIG_SETMASK, &old_set_, NULL);
  }

 private:
  sigset_t old_set_;
};

static FILE* __tmpfile_dir(const char* tmp_dir) {
  char buf[PATH_MAX];
  int path_length = snprintf(buf, sizeof(buf), "%s/tmp.XXXXXXXXXX", tmp_dir);
  if (path_length >= sizeof(buf)) {
    return NULL;
  }

  int fd;
  {
    ScopedSignalBlocker ssb;
    fd = mkstemp2(buf);
    if (fd == -1) {
      return NULL;
    }

    // Unlink the file now so that it's removed when closed.
    unlink(buf);

    // Can we still use the file now it's unlinked?
    // File systems without hard link support won't have the usual Unix semantics.
    struct stat sb;
    int rc = fstat(fd, &sb);
    if (rc == -1) {
      int old_errno = errno;
      close(fd);
      errno = old_errno;
      return NULL;
    }
  }

  // Turn the file descriptor into a FILE*.
  FILE* fp = fdopen(fd, "w+");
  if (fp != NULL) {
    return fp;
  }

  // Failure. Clean up. We already unlinked, so we just need to close.
  int old_errno = errno;
  close(fd);
  errno = old_errno;
  return NULL;
}

FILE* tmpfile2() {

  FILE* fp = __tmpfile_dir("/data/data/com.retroarch/tmp");
  if (fp == NULL) {
     fp = __tmpfile_dir(P_tmpdir);
  }
  return fp;
}

