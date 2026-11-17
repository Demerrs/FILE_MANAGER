/* fs.h - Modulul 2: "Core" */
#ifndef FS_H
#define FS_H

#include <sys/types.h>
#include <time.h>

typedef enum {
    FT_REGULAR,
    FT_DIRECTORY,
    FT_SYMLINK,
    FT_OTHER
} FileType;

typedef struct {
    char     *name;
    FileType  type;
    off_t     size;
    mode_t    mode;
    time_t    mtime;
    char     *link_target;
} FileEntry;

int  fs_read_dir(const char *path, FileEntry **out, size_t *count);
void fs_free_entries(FileEntry *entries, size_t count);

char *fs_join(const char *dir, const char *name);

#endif
