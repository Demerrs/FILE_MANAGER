/* state.c - Modulul 3: "I/O" */
#define _DEFAULT_SOURCE
#include "state.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *sort_name(SortMode m)
{
    switch (m) {
    case SORT_NAME: return "nume";
    case SORT_SIZE: return "dimensiune";
    case SORT_DATE: return "data";
    }
    return "?";
}

static int group_rank(const FileEntry *e)
{
    if (strcmp(e->name, "..") == 0) return 0;
    if (e->type == FT_DIRECTORY)     return 1;
    return 2;
}

static int cmp_name(const void *a, const void *b)
{
    const FileEntry *x = a, *y = b;
    int g = group_rank(x) - group_rank(y);
    if (g) return g;
    return strcmp(x->name, y->name);
}

static int cmp_size(const void *a, const void *b)
{
    const FileEntry *x = a, *y = b;
    int g = group_rank(x) - group_rank(y);
    if (g) return g;
    if (x->size != y->size) return (x->size > y->size) ? -1 : 1;
    return strcmp(x->name, y->name);
}

static int cmp_date(const void *a, const void *b)
{
    const FileEntry *x = a, *y = b;
    int g = group_rank(x) - group_rank(y);
    if (g) return g;
    if (x->mtime != y->mtime) return (x->mtime > y->mtime) ? -1 : 1;
    return strcmp(x->name, y->name);
}

void panel_sort(Panel *p)
{
    int (*cmp)(const void *, const void *) = cmp_name;
    if (p->sort == SORT_SIZE) cmp = cmp_size;
    if (p->sort == SORT_DATE) cmp = cmp_date;
    qsort(p->entries, p->count, sizeof(FileEntry), cmp);
}

void panel_init(Panel *p, const char *path)
{
    memset(p, 0, sizeof *p);
    p->sort = SORT_NAME;
    panel_load(p, path);
}

int panel_load(Panel *p, const char *path)
{
    char resolved[PATH_MAX];
    if (!realpath(path, resolved)) {
        snprintf(p->error, sizeof p->error, "%s: %s", path, strerror(errno));
        return -1;
    }

    FileEntry *entries;
    size_t count;
    if (fs_read_dir(resolved, &entries, &count) < 0) {
        snprintf(p->error, sizeof p->error, "%.200s: %s", resolved, strerror(errno));
        return -1;
    }

    fs_free_entries(p->entries, p->count);
    p->entries  = entries;
    p->count    = count;
    p->selected = 0;
    p->scroll   = 0;
    p->error[0] = '\0';
    strncpy(p->path, resolved, sizeof p->path - 1);
    p->path[sizeof p->path - 1] = '\0';

    panel_sort(p);
    return 0;
}

void panel_enter(Panel *p)
{
    if (p->count == 0) return;
    FileEntry *e = &p->entries[p->selected];

    int is_dir = (e->type == FT_DIRECTORY);
    if (e->type == FT_SYMLINK) {
        char *full = fs_join(p->path, e->name);
        char tmp[PATH_MAX];
        if (full && realpath(full, tmp)) {
            FileEntry *probe; size_t n;
            if (fs_read_dir(tmp, &probe, &n) == 0) {
                fs_free_entries(probe, n);
                is_dir = 1;
            }
        }
        free(full);
    }
    if (!is_dir) return;

    char *target = fs_join(p->path, e->name);
    if (!target) return;
    panel_load(p, target);
    free(target);
}

void panel_free(Panel *p)
{
    fs_free_entries(p->entries, p->count);
    p->entries = NULL;
    p->count = 0;
}
