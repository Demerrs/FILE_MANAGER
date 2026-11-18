/* ui.c - Module 1: "UI" */
#define _DEFAULT_SOURCE
#include "ui.h"

#include <ncurses.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

enum { CP_DIR = 1, CP_LINK, CP_SEL, CP_HEADER, CP_ERR, CP_ACTIVE };

void ui_init(void)
{
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(CP_DIR,    COLOR_BLUE,  -1);
        init_pair(CP_LINK,   COLOR_CYAN,  -1);
        init_pair(CP_SEL,    COLOR_BLACK, COLOR_CYAN);
        init_pair(CP_HEADER, COLOR_BLACK, COLOR_WHITE);
        init_pair(CP_ERR,    COLOR_RED,   -1);
        init_pair(CP_ACTIVE, COLOR_YELLOW, -1);
    }
}

void ui_shutdown(void)
{
    endwin();
}

static void perm_string(mode_t m, char out[10])
{
    const char *c = "rwxrwxrwx";
    for (int i = 0; i < 9; i++)
        out[i] = (m & (1 << (8 - i))) ? c[i] : '-';
    out[9] = '\0';
}

static void human_size(off_t sz, char *buf, size_t n)
{
    const char *u[] = { "B", "K", "M", "G", "T" };
    double v = (double)sz;
    int i = 0;
    while (v >= 1024 && i < 4) { v /= 1024; i++; }
    if (i == 0) snprintf(buf, n, "%lld%s", (long long)sz, u[i]);
    else        snprintf(buf, n, "%.1f%s", v, u[i]);
}

static void draw_panel(Panel *p, int x, int w, int h, int active)
{

    if (active) attron(A_BOLD);
    mvaddch(0, x, ACS_ULCORNER);
    mvhline(0, x + 1, ACS_HLINE, w - 2);
    mvaddch(0, x + w - 1, ACS_URCORNER);
    mvvline(1, x, ACS_VLINE, h - 2);
    mvvline(1, x + w - 1, ACS_VLINE, h - 2);
    mvaddch(h - 1, x, ACS_LLCORNER);
    mvhline(h - 1, x + 1, ACS_HLINE, w - 2);
    mvaddch(h - 1, x + w - 1, ACS_LRCORNER);
    if (active) attroff(A_BOLD);

    int maxt = w - 6;
    const char *t = p->path;
    if ((int)strlen(t) > maxt) t += strlen(t) - maxt;

    int title_attr = active ? (COLOR_PAIR(CP_ACTIVE) | A_REVERSE | A_BOLD) : 0;
    attron(title_attr);
    mvprintw(0, x + 2, " %.*s ", maxt, t);
    attroff(title_attr);

    int inner_w = w - 2;
    int rows    = h - 3; 

    if (p->selected < p->scroll) p->scroll = p->selected;
    if (p->selected >= p->scroll + (size_t)rows) p->scroll = p->selected - rows + 1;

    for (int r = 0; r < rows; r++) {
        size_t i = p->scroll + r;
        move(1 + r, x + 1);
        for (int k = 0; k < inner_w; k++) addch(' ');
        if (i >= p->count) continue;

        FileEntry *e = &p->entries[i];
        char size_s[16], perm[10];
        if (e->type == FT_DIRECTORY) strcpy(size_s, "<DIR>");
        else human_size(e->size, size_s, sizeof size_s);
        perm_string(e->mode, perm);

        char prefix = e->type == FT_DIRECTORY ? '/' :
                      e->type == FT_SYMLINK   ? '@' : ' ';

        int attr = 0;
        if (e->type == FT_DIRECTORY) attr = COLOR_PAIR(CP_DIR) | A_BOLD;
        if (e->type == FT_SYMLINK)   attr = COLOR_PAIR(CP_LINK);
        if (i == p->selected && active) attr = COLOR_PAIR(CP_SEL);

        int name_w = inner_w - 19;
        if (name_w < 5) name_w = 5;

        attron(attr);
        mvprintw(1 + r, x + 1, "%c%-*.*s %7s %s",
                 prefix, name_w - 1, name_w - 1, e->name, size_s, perm);
        attroff(attr);
    }

    move(h - 2, x + 1);
    for (int k = 0; k < inner_w; k++) addch(' ');
    if (p->error[0]) {
        attron(COLOR_PAIR(CP_ERR));
        mvprintw(h - 2, x + 1, "%.*s", inner_w, p->error);
        attroff(COLOR_PAIR(CP_ERR));
    } else if (p->count && p->entries[p->selected].type == FT_SYMLINK) {
        const char *tg = p->entries[p->selected].link_target;
        mvprintw(h - 2, x + 1, "-> %.*s", inner_w - 3, tg ? tg : "(?)");
    } else {
        mvprintw(h - 2, x + 1, "%zu items", p->count);
    }
}

void ui_draw(AppState *s)
{
    int H, W;
    getmaxyx(stdscr, H, W);
    erase();

    int ph = H - 1;
    int lw = W / 2;
    draw_panel(&s->panels[0], 0,  lw,     ph, s->active == 0);
    draw_panel(&s->panels[1], lw, W - lw, ph, s->active == 1);

    attron(COLOR_PAIR(CP_HEADER));
    mvhline(H - 1, 0, ' ', W);
    mvprintw(H - 1, 0, " Tab:switch panel  Up/Down:move  Enter:open  Backspace:parent  r:reload  q:quit");
    attroff(COLOR_PAIR(CP_HEADER));

    refresh();
}

int ui_handle_key(AppState *s, int ch)
{
    Panel *p = &s->panels[s->active];
    int H, W;
    getmaxyx(stdscr, H, W);
    (void)W;
    size_t page = (H > 5) ? (size_t)(H - 5) : 1;

    switch (ch) {
    case 'q': case 'Q':
        return 0;
    case '\t':
        s->active ^= 1;
        break;
    case KEY_UP:
        if (p->selected > 0) p->selected--;
        break;
    case KEY_DOWN:
        if (p->selected + 1 < p->count) p->selected++;
        break;
    case KEY_PPAGE:
        p->selected = (p->selected > page) ? p->selected - page : 0;
        break;
    case KEY_NPAGE:
        p->selected += page;
        if (p->selected >= p->count) p->selected = p->count ? p->count - 1 : 0;
        break;
    case KEY_HOME:
        p->selected = 0;
        break;
    case KEY_END:
        p->selected = p->count ? p->count - 1 : 0;
        break;
    case '\n': case KEY_ENTER:
        panel_enter(p);
        break;
    case KEY_BACKSPACE: case 127: case 8: {
        char up[PATH_MAX + 4];
        snprintf(up, sizeof up, "%s/..", p->path);
        panel_load(p, up);
        break;
    }
    case 'r': case 'R':
        panel_load(p, p->path);
        break;
    case KEY_RESIZE:
        break;
    }
    return 1;
}
