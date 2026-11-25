/* ui.c - Module 1: "UI" */
#define _DEFAULT_SOURCE
#include "ui.h"

#include <ctype.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

enum { CP_DIR = 1, CP_LINK, CP_SEL, CP_ERR, CP_ACTIVE, CP_OK, CP_KEY };

void ui_init(void)
{
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    set_escdelay(25);
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(CP_DIR,    COLOR_BLUE,   -1);
        init_pair(CP_LINK,   COLOR_CYAN,   -1);
        init_pair(CP_SEL,    COLOR_BLACK,  COLOR_CYAN);
        init_pair(CP_ERR,    COLOR_RED,    -1);
        init_pair(CP_ACTIVE, COLOR_YELLOW, -1);
        init_pair(CP_OK,     COLOR_GREEN,  -1);
        init_pair(CP_KEY,    COLOR_BLACK,  COLOR_CYAN);
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
        mvprintw(h - 2, x + 1, "%zu items | sort: %s", p->count, sort_name(p->sort));
    }
}

static void draw_hints(int y, int W)
{
    static const char *hints[][2] = {
        { "Tab",  "Panel"  },
        { "Ent",  "Open"   },
        { "Bksp", "Up"     },
        { "S",    "Sort"   },
        { "N",    "File"   },
        { "M",    "Dir"    },
        { "L",    "Link"   },
        { "D",    "Delete" },
        { "R",    "Reload" },
        { "Q",    "Quit"   },
    };
    int n = sizeof hints / sizeof hints[0];

    int left  = 1;
    int avail = W - 2;

    int text = 0;
    for (int i = 0; i < n; i++)
        text += (int)strlen(hints[i][0]) + 1 + (int)strlen(hints[i][1]);

    int pad = (avail >= text + 2 * n + (n - 1)) ? 1 : 0;
    int used = text + 2 * n * pad;
    int gaps = n - 1;
    int gap  = (avail > used) ? (avail - used) / gaps : 1;
    int rest = (avail > used) ? (avail - used) % gaps : 0;
    if (gap < 1) gap = 1;

    move(y, 0);
    clrtoeol();

    int x = left;
    for (int i = 0; i < n; i++) {
        const char *key   = hints[i][0];
        const char *label = hints[i][1];
        int keyw  = (int)strlen(key) + 2 * pad;
        int itemw = keyw + 1 + (int)strlen(label);
        if (x + itemw > left + avail) break;

        attron(COLOR_PAIR(CP_KEY));
        mvprintw(y, x, "%*s%s%*s", pad, "", key, pad, "");
        attroff(COLOR_PAIR(CP_KEY));

        mvprintw(y, x + keyw, " %s", label);

        x += itemw + gap + (i < rest ? 1 : 0);
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

    draw_hints(H - 1, W);

    refresh();
}

static WINDOW *popup_open(int h, int w, const char *title, int color)
{
    int H, W;
    getmaxyx(stdscr, H, W);
    if (w > W - 2) w = W - 2;
    if (h > H - 2) h = H - 2;

    WINDOW *win = newwin(h, w, (H - h) / 2, (W - w) / 2);
    keypad(win, TRUE);

    wattron(win, COLOR_PAIR(color) | A_BOLD);
    box(win, 0, 0);
    mvwprintw(win, 0, 2, " %.*s ", w - 6, title);
    wattroff(win, COLOR_PAIR(color) | A_BOLD);
    return win;
}

static void popup_close(WINDOW *win)
{
    werase(win);
    wrefresh(win);
    delwin(win);
    touchwin(stdscr);
}

static int ui_confirm(const char *title, const char *line1, const char *line2, int color, int default_yes)
{
    int w = 56;
    WINDOW *win = popup_open(8, w, title, color);
    w = getmaxx(win);
    int choice = default_yes ? 1 : 0;

    for (;;) {
        mvwprintw(win, 2, 2, "%.*s", w - 4, line1);
        mvwprintw(win, 3, 2, "%.*s", w - 4, line2);

        int bx = w / 2 - 9;
        if (choice == 1) wattron(win, A_REVERSE);
        mvwprintw(win, 5, bx, "[ Yes ]");
        if (choice == 1) wattroff(win, A_REVERSE);
        if (choice == 0) wattron(win, A_REVERSE);
        mvwprintw(win, 5, bx + 11, "[ No ]");
        if (choice == 0) wattroff(win, A_REVERSE);
        wrefresh(win);

        int ch = wgetch(win);
        if (ch == 'y' || ch == 'Y') { choice = 1; break; }
        if (ch == 'n' || ch == 'N' || ch == 27) { choice = 0; break; }
        if (ch == KEY_LEFT || ch == KEY_RIGHT || ch == '\t') choice ^= 1;
        if (ch == '\n' || ch == KEY_ENTER) break;
    }
    popup_close(win);
    return choice;
}

static int ui_input(const char *title, const char *prompt, char *buf, size_t size)
{
    int w = 56;
    WINDOW *win = popup_open(7, w, title, CP_OK);
    w = getmaxx(win);
    int field_w = w - 4;
    size_t len = strlen(buf);
    int result = -1;

    curs_set(1);
    while (result < 0) {
        mvwprintw(win, 2, 2, "%.*s", field_w, prompt);

        const char *shown = buf;
        if ((int)len > field_w - 1) shown = buf + len - (field_w - 1);
        wattron(win, A_UNDERLINE);
        mvwprintw(win, 3, 2, "%-*s", field_w, shown);
        wattroff(win, A_UNDERLINE);

        mvwprintw(win, 5, 2, "Enter: OK   Esc: cancel");
        wmove(win, 3, 2 + (int)strlen(shown));
        wrefresh(win);

        int ch = wgetch(win);
        if (ch == 27) {
            result = 0;
        } else if (ch == '\n' || ch == KEY_ENTER) {
            result = 1;
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            if (len > 0) buf[--len] = '\0';
        } else if (ch >= 0 && ch < 256 && isprint(ch) && len + 1 < size) {
            buf[len++] = (char)ch;
            buf[len] = '\0';
        }
    }
    curs_set(0);
    popup_close(win);
    return result;
}

static void ui_message(const char *title, const char *text)
{
    int w = 56;
    WINDOW *win = popup_open(6, w, title, CP_ERR);
    w = getmaxx(win);
    mvwprintw(win, 2, 2, "%.*s", w - 4, text);
    mvwprintw(win, 4, 2, "Press any key");
    wrefresh(win);
    wgetch(win);
    popup_close(win);
}

static void do_delete(AppState *s)
{
    Panel *p = &s->panels[s->active];
    if (p->count == 0) return;
    FileEntry *e = &p->entries[p->selected];

    if (strcmp(e->name, "..") == 0) {
        ui_message("Delete", "\"..\" cannot be deleted.");
        return;
    }

    char line1[300];
    snprintf(line1, sizeof line1, "Delete %s \"%s\"?",
             e->type == FT_DIRECTORY ? "directory" : "file", e->name);
    const char *line2 = e->type == FT_DIRECTORY
                        ? "Everything inside it will be deleted too."
                        : "This cannot be undone.";

    if (!ui_confirm("Delete", line1, line2, CP_ERR, 0)) return;

    if (panel_delete_selected(p) < 0)
        ui_message("Error", p->error);
    panel_reload(&s->panels[!s->active]);
}

static void do_create(AppState *s, int is_dir)
{
    Panel *p = &s->panels[s->active];
    char name[NAME_MAX + 1] = "";

    const char *title = is_dir ? "New directory" : "New file";
    if (!ui_input(title, "Name:", name, sizeof name)) return;
    if (name[0] == '\0') return;

    char line1[300];
    snprintf(line1, sizeof line1, "Create %s \"%s\"?", is_dir ? "directory" : "file", name);
    if (!ui_confirm(title, line1, p->path, CP_OK, 1)) return;

    if (panel_create(p, name, is_dir) < 0)
        ui_message("Error", p->error);
    panel_reload(&s->panels[!s->active]);
}

static void do_symlink(AppState *s)
{
    Panel *p = &s->panels[s->active];
    char target[PATH_MAX] = "";
    char name[NAME_MAX + 1] = "";

    if (p->count && strcmp(p->entries[p->selected].name, "..") != 0) {
        char *full = fs_join(p->path, p->entries[p->selected].name);
        if (full) {
            snprintf(target, sizeof target, "%s", full);
            free(full);
        }
        snprintf(name, sizeof name, "%.240s_link", p->entries[p->selected].name);
    }

    if (!ui_input("New symlink", "Points to (target):", target, sizeof target)) return;
    if (target[0] == '\0') return;
    if (!ui_input("New symlink", "Link name:", name, sizeof name)) return;
    if (name[0] == '\0') return;

    char line1[300], line2[300];
    snprintf(line1, sizeof line1, "Create link \"%.200s\"?", name);
    snprintf(line2, sizeof line2, "-> %.250s", target);
    if (!ui_confirm("New symlink", line1, line2, CP_OK, 1)) return;

    if (panel_create_symlink(p, target, name) < 0)
        ui_message("Error", p->error);
    panel_reload(&s->panels[!s->active]);
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
    case 's': case 'S':
        panel_cycle_sort(p);
        break;
    case 'n': case 'N':
        do_create(s, 0);
        break;
    case 'm': case 'M': case KEY_F(7):
        do_create(s, 1);
        break;
    case 'l': case 'L':
        do_symlink(s);
        break;
    case 'd': case 'D': case KEY_DC: case KEY_F(8):
        do_delete(s);
        break;
    case 'r': case 'R':
        panel_reload(p);
        break;
    case KEY_RESIZE:
        break;
    }
    return 1;
}
