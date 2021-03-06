/* system.c - an elf and spin binary loader for the Parallax Propeller microcontroller

Copyright (c) 2011 David Michael Betz

Permission is hereby granted, free of charge, to any person obtaining a copy of this software
and associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "system.h"

#if defined(WIN32)
#include <windows.h>
#include <psapi.h>
#endif

#if defined(MACOSX)
#include <mach-o/dyld.h>
#endif

#if defined(LINUX)
#include <unistd.h>
#endif

typedef struct PathEntry PathEntry;
struct PathEntry {
    PathEntry *next;
    char path[1];
};

static PathEntry *path = NULL;
static PathEntry **pNextPathEntry = &path;

static const char *MakePath(PathEntry *entry, const char *name);

FILE *xbOpenFileInPath(const char *name, const char *mode)
{
    PathEntry *entry;
    FILE *fp;
    
    if (!(fp = fopen(name, mode))) {
        for (entry = path; entry != NULL; entry = entry->next)
            if ((fp = fopen(MakePath(entry, name), mode)) != NULL)
                break;
    }
    return fp;
}

int xbAddPath(const char *path)
{
    PathEntry *entry = malloc(sizeof(PathEntry) + strlen(path));
    if (!(entry))
        return FALSE;
    strcpy(entry->path, path);
    *pNextPathEntry = entry;
    pNextPathEntry = &entry->next;
    entry->next = NULL;
    return TRUE;
}

int xbAddFilePath(const char *name)
{
    PathEntry *entry;
    char *end;
    int len;
    if (!(end = strrchr(name, DIR_SEP)))
        return FALSE;
    len = (int)(end - name);
    if (!(entry  = malloc(sizeof(PathEntry) + len)))
        return FALSE;
    strncpy(entry->path, name, len);
    entry->path[len] = '\0';
    *pNextPathEntry = entry;
    pNextPathEntry = &entry->next;
    entry->next = NULL;
    return TRUE;
}

/* xbAddProgramPath - add the path relative the application directory */
int xbAddProgramPath(char *argv[])
{
    static char fullpath[1024];
    char *p;
#if defined(WIN32)

#if defined(Q_OS_WIN32) || defined(MINGW)
    /* get the full path to the executable */
    if (!GetModuleFileNameA(NULL, fullpath, sizeof(fullpath)))
        return FALSE;
#else
    /* get the full path to the executable */
    if (!GetModuleFileNameEx(GetCurrentProcess(), NULL, fullpath, sizeof(fullpath)))
        return FALSE;
#endif  /* Q_OS_WIN32 */

#elif defined(LINUX)
    int r;
    r = readlink("/proc/self/exe", fullpath, sizeof(fullpath)-1);
    if (r >= 0)
      fullpath[r] = 0;
    else
      return FALSE;
#elif defined(MACOSX)
    uint32_t bufsize = sizeof(fullpath)-1;
    int r = _NSGetExecutablePath(fullpath, &bufsize);
    if (r < 0)
      return FALSE;
#else
    /* fall back on argv[0]... probably not the best bet, since
       shells might not put the full path in, but it's the most portable
    */
    strcpy(fullpath, argv[0]);
#endif

    /* remove the executable filename */
    if ((p = strrchr(fullpath, DIR_SEP)) != NULL)
        *p = '\0';

    /* remove the immediate directory containing the executable (usually 'bin') */
    if ((p = strrchr(fullpath, DIR_SEP)) != NULL) {
        *p = '\0';
        
        /* check for the 'Release' or 'Debug' build directories used by Visual C++ */
        if (strcmp(&p[1], "Release") == 0 || strcmp(&p[1], "Debug") == 0) {
            if ((p = strrchr(fullpath, DIR_SEP)) != NULL)
                *p = '\0';
        }
    }

    /* add propeller-load for propeller-gcc */
    strcat(fullpath, DIR_SEP_STR "propeller-load");

    /* add the executable directory */
    xbAddPath(fullpath);

    return TRUE;
}

int xbAddEnvironmentPath(const char *name)
{
    char *p, *end;
    
    /* add path entries from the environment */
    if ((p = getenv(name)) != NULL) {
        while ((end = strchr(p, PATH_SEP)) != NULL) {
            *end = '\0';
            if (!xbAddPath(p))
                return FALSE;
            p = end + 1;
        }
        if (!xbAddPath(p))
            return FALSE;
    }
    
    return TRUE;
}

static const char *MakePath(PathEntry *entry, const char *name)
{
    static char fullpath[PATH_MAX];
    sprintf(fullpath, "%s%c%s", entry->path, DIR_SEP, name);
    return fullpath;
}
