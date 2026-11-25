/* state.h - Modulul 3: "I/O" */
#ifndef STATE_H
#define STATE_H

#include <limits.h>
#include "fs.h"

typedef enum { SORT_NAME, SORT_SIZE, SORT_DATE } SortMode;

typedef struct {
    char       path[PATH_MAX];
    FileEntry *entries;
    size_t     count;
    size_t     selected;
    size_t     scroll;
    SortMode   sort;
    char       error[256];
} Panel;

typedef struct {
    Panel panels[2];
    int   active;
} AppState;

void panel_init(Panel *p, const char *path);
int  panel_load(Panel *p, const char *path);
int  panel_reload(Panel *p);
void panel_sort(Panel *p);
void panel_cycle_sort(Panel *p);
void panel_enter(Panel *p);
void panel_free(Panel *p);

int  panel_delete_selected(Panel *p);
int  panel_create(Panel *p, const char *name, int is_dir);

const char *sort_name(SortMode m);

#endif
