#define _GNU_SOURCE
#include <link.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The selected runtime includes its loader, not just shared libraries. This
 * also detects host-loader fallback when host and target libc versions match. */
static int found;
static int inspect_loader(struct dl_phdr_info *info, size_t size, void *data)
{
    (void)size;
    (void)data;
    if (!strstr(info->dlpi_name, "ld-linux"))
        return 0;
    const char *root = getenv("SWSS_RUNTIME_ROOT");
    char *path = realpath(info->dlpi_name, NULL);
    if (root && path && strncmp(path, root, strlen(root)) == 0 && path[strlen(root)] == '/')
        found = 1;
    free(path);
    return 0;
}
int main(void)
{
    dl_iterate_phdr(inspect_loader, NULL);
    if (!found) {
        fputs("ELF did not use the declared runtime interpreter\n", stderr);
        return 1;
    }
    puts("ELF uses its declared runtime interpreter");
    return 0;
}
