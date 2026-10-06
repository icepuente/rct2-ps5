/*
 * Directory reading on the kernel's getdents.
 *
 * The console libc's opendir/readdir lists nothing for a native title (an
 * OpenRCT2 scan of 2,500 object files found none), so these replacements read
 * the kernel's dirent records directly. They are used by scandir in
 * ps5_libc_compat.c and by libc++'s std::filesystem.
 *
 * getdents fails with EINVAL on these filesystems unless the buffer is at
 * least 64 KiB (the morrowind-ps5 port's directory backend uses the same size).
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int getdents(int fd, char *buf, int nbytes);

struct ps5_dir {
    int fd;
    int pos;
    int len;
    struct dirent entry;
    char buf[64 * 1024] __attribute__((aligned(8)));
};

DIR *fdopendir(int fd)
{
    struct ps5_dir *d = calloc(1, sizeof(*d));
    if (!d) {
        errno = ENOMEM;
        return NULL;
    }
    d->fd = fd;
    return (DIR *)d;
}

DIR *opendir(const char *path)
{
    int fd = open(path, O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        return NULL;
    }
    DIR *d = fdopendir(fd);
    if (!d) {
        close(fd);
    }
    return d;
}

struct dirent *readdir(DIR *dir)
{
    struct ps5_dir *d = (struct ps5_dir *)dir;

    for (;;) {
        if (d->pos >= d->len) {
            int n = getdents(d->fd, d->buf, (int)sizeof(d->buf));
            if (n <= 0) {
                static int reported;
                if (n < 0 && !reported) {
                    reported = 1;
                    printf("ps5_dirent: getdents failed: %s\n", strerror(errno));
                }
                return NULL; /* end of directory, or errno set */
            }
            d->len = n;
            d->pos = 0;
        }

        struct dirent *rec = (struct dirent *)(d->buf + d->pos);
        if (rec->d_reclen == 0) {
            d->pos = d->len;
            continue;
        }
        d->pos += rec->d_reclen;
        if (rec->d_fileno == 0) {
            continue; /* deleted entry */
        }

        size_t namelen = rec->d_namlen; /* at most 255, so it fits d_name */
        d->entry.d_fileno = rec->d_fileno;
        d->entry.d_reclen = sizeof(d->entry);
        d->entry.d_type = rec->d_type;
        d->entry.d_namlen = (__uint8_t)namelen;
        memcpy(d->entry.d_name, rec->d_name, namelen);
        d->entry.d_name[namelen] = '\0';
        return &d->entry;
    }
}

int readdir_r(DIR *dir, struct dirent *entry, struct dirent **result)
{
    errno = 0;
    struct dirent *e = readdir(dir);
    if (!e) {
        *result = NULL;
        return errno;
    }
    memcpy(entry, e, sizeof(*entry));
    *result = entry;
    return 0;
}

void rewinddir(DIR *dir)
{
    struct ps5_dir *d = (struct ps5_dir *)dir;
    lseek(d->fd, 0, SEEK_SET);
    d->pos = d->len = 0;
}

int dirfd(DIR *dir)
{
    return ((struct ps5_dir *)dir)->fd;
}

int closedir(DIR *dir)
{
    struct ps5_dir *d = (struct ps5_dir *)dir;
    int rc = close(d->fd);
    free(d);
    return rc;
}
