#ifndef SF2000_COMPAT_H
#define SF2000_COMPAT_H

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// Missing functions for SF2000 build
static int strcasecmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) return c1 - c2;
        s1++; s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

static int strncasecmp(const char *s1, const char *s2, size_t n) {
    while (n > 0 && *s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) return c1 - c2;
        s1++; s2++; n--;
    }
    if (n == 0) return 0;
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

static char *strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *dup = (char*)malloc(len);
    if (dup) memcpy(dup, s, len);
    return dup;
}

#endif // SF2000_COMPAT_H