#define _GNU_SOURCE
#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Delay one policy/credential read in a test subprocess without production hooks. */
ssize_t read(int fd, void *buffer, size_t size)
{
    static int delayed;
    ssize_t (*next_read)(int, void *, size_t) = dlsym(RTLD_NEXT, "read");
    const char *target = getenv("SWSS_TEST_DELAY_AUTH_READ");
    if (!delayed && target)
    {
        char descriptor[64], path[PATH_MAX];
        snprintf(descriptor, sizeof(descriptor), "/proc/self/fd/%d", fd);
        ssize_t length = readlink(descriptor, path, sizeof(path) - 1);
        if (length >= 0)
        {
            path[length] = '\0';
            if (strcmp(path, target) == 0)
            {
                delayed = 1;
                usleep(200000);
                const char marker[] = "delayed Redis authentication read\n";
                (void)write(STDERR_FILENO, marker, sizeof(marker) - 1);
            }
        }
    }
    return next_read(fd, buffer, size);
}
