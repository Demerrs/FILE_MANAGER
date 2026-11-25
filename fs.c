/* fs.c - Modulul 2: "Core" */
#define _DEFAULT_SOURCE
#include "fs.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

char *fs_join(const char *dir, const char *name)
{
    size_t ld = strlen(dir), ln = strlen(name);
    int need_slash = (ld > 0 && dir[ld - 1] != '/');
    char *p = malloc(ld + need_slash + ln + 1);
    if (!p) return NULL;
    memcpy(p, dir, ld);
    if (need_slash) p[ld] = '/';
    memcpy(p + ld + need_slash, name, ln + 1);
    return p;
}

static FileType mode_to_type(mode_t m)
{
    if (S_ISDIR(m)) return FT_DIRECTORY;
    if (S_ISLNK(m)) return FT_SYMLINK;
    if (S_ISREG(m)) return FT_REGULAR;
    return FT_OTHER;
}

static char *read_link_target(const char *path, off_t hint)
{
    size_t bufsz = (hint > 0 ? (size_t)hint : 255) + 1;
    char *buf = malloc(bufsz);
    if (!buf) return NULL;
    ssize_t n = readlink(path, buf, bufsz - 1);
    if (n < 0) { free(buf); return NULL; }
    buf[n] = '\0';
    return buf;
}

int fs_read_dir(const char *path, FileEntry **out, size_t *count)
{
    *out = NULL;
    *count = 0;

    DIR *d = opendir(path);
    if (!d) return -1;

    size_t cap = 64, n = 0;
    FileEntry *arr = malloc(cap * sizeof *arr);
    if (!arr) { closedir(d); return -1; }

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0) continue;

        char *full = fs_join(path, de->d_name);
        if (!full) continue;

        struct stat st;
        if (lstat(full, &st) < 0) { free(full); continue; }

        if (n == cap) {
            cap *= 2;
            FileEntry *tmp = realloc(arr, cap * sizeof *arr);
            if (!tmp) { free(full); break; }
            arr = tmp;
        }

        FileEntry *e = &arr[n++];
        e->name        = strdup(de->d_name);
        e->type        = mode_to_type(st.st_mode);
        e->size        = st.st_size;
        e->mode        = st.st_mode;
        e->mtime       = st.st_mtime;
        e->link_target = (e->type == FT_SYMLINK)
                         ? read_link_target(full, st.st_size) : NULL;
        free(full);
    }
    closedir(d);

    *out = arr;
    *count = n;
    return 0;
}

void fs_free_entries(FileEntry *entries, size_t count)
{
    if (!entries) return;
    for (size_t i = 0; i < count; i++) {
        free(entries[i].name);
        free(entries[i].link_target);
    }
    free(entries);
}

int fs_create_file(const char *path)
{

    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd < 0) return -1;
    return close(fd);
}

int fs_create_symlink(const char *target, const char *linkpath)
{
    return symlink(target, linkpath);
}

int fs_create_dir(const char *path)
{
    return mkdir(path, 0755);
}

int fs_delete(const char *path)
{
    struct stat st;
    if (lstat(path, &st) < 0) return -1;

    if (!S_ISDIR(st.st_mode)) return unlink(path);

    FileEntry *list;
    size_t n;
    if (fs_read_dir(path, &list, &n) < 0) return -1;

    int rc = 0;
    for (size_t i = 0; i < n && rc == 0; i++) {
        if (strcmp(list[i].name, "..") == 0) continue;
        char *child = fs_join(path, list[i].name);
        if (!child) { errno = ENOMEM; rc = -1; break; }
        rc = fs_delete(child);
        free(child);
    }

    int saved = errno;
    fs_free_entries(list, n);
    errno = saved;

    if (rc < 0) return -1;
    return rmdir(path);
}
