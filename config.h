/**
 * @file    config.h
 * @brief   SC68 build configuration for replay_frontend integration
 */

#ifndef SC68_CONFIG_H
#define SC68_CONFIG_H

/* Package information */
#ifndef PACKAGE
#define PACKAGE "sc68"
#endif
#ifndef PACKAGE_BUGREPORT
#define PACKAGE_BUGREPORT "https://sourceforge.net/projects/sc68/"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "libsc68"
#endif
#ifndef PACKAGE_STRING
#define PACKAGE_STRING "libsc68 3.0.0"
#endif
#ifndef PACKAGE_TARNAME
#define PACKAGE_TARNAME "sc68"
#endif
#ifndef PACKAGE_URL
#define PACKAGE_URL "http://sc68.atari.org"
#endif
#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "3.0.0"
#endif
#ifndef VERSION
#define VERSION "3.0.0"
#endif

/* Version components */
#define LIB68_MAJOR 3
#define LIB68_MINOR 0
#define LIB68_PATCH 0
#define LIB68_TWEAK 0

/* Enable file68 features */
#define FILE68_UNICE68 1

/* Disable optional features */
#undef FILE68_Z    /* No zlib compression */
#undef FILE68_CURL /* No network access */
#undef FILE68_AO   /* No libao audio output */

/* Disable VFS implementations we don't need (we use our own VFS) */
#define ISTREAM68_NO_FD 1   /* No file descriptor VFS */
#define ISTREAM68_NO_FILE 1 /* No stdio FILE VFS */

/* YM emulation engine: 1=PULS (pulse), 2=BLEP (band-limited), 3=DUMP */
#define YM_ENGINE 1

/* Disable dialogs */
#undef USE_DIALOG

/* Standard C functions available */
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDINT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_LIMITS_H 1
#define HAVE_ASSERT_H 1
#define HAVE_ERRNO_H 1

/* Memory functions */
#define HAVE_MEMCPY 1
#define HAVE_MEMMOVE 1
#define HAVE_MEMSET 1
#define HAVE_MEMCMP 1

/* String functions */
#define HAVE_STRCPY 1
#define HAVE_STRNCPY 1
#define HAVE_STRCMP 1
#define HAVE_STRNCMP 1
#define HAVE_STRCASECMP 1
#define HAVE_STRRCHR 1
#define HAVE_STRSTR 1
#define HAVE_STRLEN 1
#define HAVE_STRTOL 1
#define HAVE_STRTOUL 1
#define HAVE_SNPRINTF 1

/* Other functions */
#define HAVE_GETENV 1

/* Platform specifics */
#ifdef _WIN32
#define HAVE_IO_H 1
#undef HAVE_UNISTD_H
#undef HAVE_FCNTL_H
#undef HAVE_LIBGEN_H
/* Provide basename implementation for Windows */
#define HAVE_BASENAME 1
static inline char* basename(char* path) {
    char* p = path;
    char* last = path;
    if (!path || !*path)
        return path;
    while (*p) {
        if (*p == '/' || *p == '\\')
            last = p + 1;
        p++;
    }
    return last;
}
#else
#define HAVE_UNISTD_H 1
#define HAVE_FCNTL_H 1
#define HAVE_LIBGEN_H 1
#define HAVE_BASENAME 1
#undef HAVE_IO_H
#endif

/* Use standard int types from stdint.h */
#include <stdint.h>

#endif /* SC68_CONFIG_H */
