/* main.c */
#include <locale.h>
#include <ncurses.h>
#include <stdio.h>
#include <string.h>

#include "fs.h"
#include "state.h"
#include "ui.h"

static int dump(const char *path)
{
    Panel p;
    panel_init(&p, path);
    if (p.error[0]) { fprintf(stderr, "Eroare: %s\n", p.error); return 1; }

    printf("Director: %s (%zu elemente)\n", p.path, p.count);
    for (size_t i = 0; i < p.count; i++) {
        FileEntry *e = &p.entries[i];
        const char *t = e->type == FT_DIRECTORY ? "DIR " :
                        e->type == FT_SYMLINK   ? "LINK" :
                        e->type == FT_REGULAR   ? "FILE" : "OTH ";
        printf("%s %04o %10lld  %s", t, e->mode & 07777,
               (long long)e->size, e->name);
        if (e->link_target) printf(" -> %s", e->link_target);
        printf("\n");
    }
    panel_free(&p);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "--dump") == 0)
        return dump(argc >= 3 ? argv[2] : ".");

    setlocale(LC_ALL, "");

    AppState s;
    memset(&s, 0, sizeof s);
    panel_init(&s.panels[0], argc >= 2 ? argv[1] : ".");
    panel_init(&s.panels[1], argc >= 3 ? argv[2] : ".");

    ui_init();
    int running = 1;
    while (running) {
        ui_draw(&s);
        running = ui_handle_key(&s, getch());
    }
    ui_shutdown();

    panel_free(&s.panels[0]);
    panel_free(&s.panels[1]);
    return 0;
}
