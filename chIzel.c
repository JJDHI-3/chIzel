// Emacs style mode select -*- C -*-
// --------------------------------

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>
#include "gruvecolors.h"   // Gruvbox header

#define MAX_LINES 1000
#define MAX_COLS  512

typedef enum {
    MODE_NORMAL,
    MODE_INSERT,
    MODE_COMMAND,
    MODE_VISUAL 
} EditorMode;

static char buffer[MAX_LINES][MAX_COLS];
static int line_count = 1;
static int cur_row = 0, cur_col = 0;
static int top_row = 0; 
static bool modified = false;
static char filename[256] = {0};
static char cmd_feedback[128] = {0};
static bool syntax_c = false; 

// Forward declarations
static void draw(EditorMode mode, const char *cmdline, const GruvboxHardColors *t);
static void insert_char(int ch);
static void insert_newline();

extern char buffer[/* MAX_LINES */][MAX_COLS];
extern int line_count;

static const char *EMACS_HEADER = "// Emacs style mode select -*- C -*-";

bool is_emacs_header_present() {
    if (line_count == 0) return false;
    if (strcmp(buffer[0], EMACS_HEADER) == 0) return true;
    return false;
}

// Helper: Check if the current line starts with a comment (//)
static bool is_comment_line(int row) {
    if (row < 0 || row >= line_count) return false;
    const char *p = buffer[row];
    while (*p == ' ' || *p == '\t') p++;
    return (strncmp(p, "//", 2) == 0);
}

// Helper: Create a new line and automatically add the comment prefix
static void auto_comment_newline() {
    char leading_ws[MAX_COLS] = {0};
    int ws_len = 0;
    while ((buffer[cur_row][ws_len] == ' ' || buffer[cur_row][ws_len] == '\t') && ws_len < MAX_COLS - 1) {
        leading_ws[ws_len] = buffer[cur_row][ws_len];
        ws_len++;
    }
    
    insert_newline();
    
    for (int i = 0; i < ws_len; i++) insert_char(leading_ws[i]);
    insert_char('/');
    insert_char('/');
    insert_char(' ');
}

enum {
    CP_NORMAL = 1,
    CP_STATUS,
    CP_COMMAND,
    CP_CURSORLINE,
    CP_TILDE,
    CP_C_COMMENT,      
    CP_C_STRING,       
    CP_C_PREPROC,      
    CP_C_TYPE,         
    CP_C_BRACKET,      
    CP_C_NUMBER,       
    CP_C_SPECIAL       
};

static short scale255(int x) { return (short)((x * 1000) / 255); }

static void define_rgb(short idx, uint32_t hex) {
    short r = scale255((hex >> 16) & 0xFF);
    short g = scale255((hex >> 8) & 0xFF);
    short b = scale255(hex & 0xFF);
    init_color(idx, r, g, b);
}

static void init_colors(const GruvboxHardColors *t) {
    start_color();
    use_default_colors();
    bool can_custom = can_change_color() && COLORS >= 64;

    short IDX_BG = 20, IDX_FG = 21, IDX_STATUS_BG = 22, IDX_STATUS_FG = 23;
    short IDX_CMD_FG = 24, IDX_LINEHL_BG = 25, IDX_TILDE = 26;
    short IDX_COMMENT = 27;                   
    short IDX_STRING = 28;                    
    short IDX_PREPROC = 29;                   
    short IDX_TYPE = 30;                      
    short IDX_BRACKET = 31;                   
    short IDX_NUMBER = 32;                    
    short IDX_SPECIAL = 33;                   

    if (can_custom) {
        define_rgb(IDX_BG, t->bg);
        define_rgb(IDX_FG, t->fg);
        define_rgb(IDX_STATUS_BG, t->status_bg);
        define_rgb(IDX_STATUS_FG, t->status_fg);
        define_rgb(IDX_CMD_FG, t->mode_normal);
        define_rgb(IDX_LINEHL_BG, t->line_highlight);
        define_rgb(IDX_TILDE, t->line_number);
        define_rgb(IDX_COMMENT, t->comment);
        define_rgb(IDX_STRING, t->string);
        define_rgb(IDX_PREPROC, 0x8ec07b);
        define_rgb(IDX_TYPE, 0xfab92e);
        define_rgb(IDX_BRACKET, 0xd65a0e);
        define_rgb(IDX_NUMBER, 0xd3869b);
        define_rgb(IDX_SPECIAL, 0xd3869b);

        init_pair(CP_NORMAL, IDX_FG, IDX_BG);
        init_pair(CP_STATUS, IDX_STATUS_FG, IDX_STATUS_BG);
        init_pair(CP_COMMAND, IDX_CMD_FG, IDX_BG);
        init_pair(CP_CURSORLINE, IDX_FG, IDX_LINEHL_BG);
        init_pair(CP_TILDE, IDX_TILDE, IDX_BG);
        init_pair(CP_C_COMMENT, IDX_COMMENT, IDX_BG);
        init_pair(CP_C_STRING, IDX_STRING, IDX_BG);
        init_pair(CP_C_PREPROC, IDX_PREPROC, IDX_BG);
        init_pair(CP_C_TYPE, IDX_TYPE, IDX_BG);
        init_pair(CP_C_BRACKET, IDX_BRACKET, IDX_BG);
        init_pair(CP_C_NUMBER, IDX_NUMBER, IDX_BG);
        init_pair(CP_C_SPECIAL, IDX_SPECIAL, IDX_BG);
    } else {
        init_pair(CP_NORMAL, COLOR_WHITE, COLOR_BLACK);
        init_pair(CP_STATUS, COLOR_WHITE, COLOR_BLACK);
        init_pair(CP_COMMAND, COLOR_GREEN, COLOR_BLACK);
        init_pair(CP_CURSORLINE, COLOR_WHITE, COLOR_BLUE);
        init_pair(CP_TILDE, COLOR_CYAN, COLOR_BLACK);
        init_pair(CP_C_COMMENT, COLOR_CYAN, COLOR_BLACK);
        init_pair(CP_C_STRING, COLOR_GREEN, COLOR_BLACK);
        init_pair(CP_C_PREPROC, COLOR_GREEN, COLOR_BLACK);
        init_pair(CP_C_TYPE, COLOR_YELLOW, COLOR_BLACK);
        init_pair(CP_C_BRACKET, COLOR_YELLOW, COLOR_BLACK);
        init_pair(CP_C_NUMBER, COLOR_MAGENTA, COLOR_BLACK);
        init_pair(CP_C_SPECIAL, COLOR_MAGENTA, COLOR_BLACK);
    }
    bkgd(COLOR_PAIR(CP_NORMAL));
}

static bool is_word_boundary(char c) {
    return !(isalnum((unsigned char)c) || c == '_');
}

static void draw_line_with_syntax(int screen_row, int buffer_row, int gutter, int max_x) {
    const char *line = buffer[buffer_row];
    const char *p = line;
    int x = gutter;
    bool in_comment = false, in_preproc = false;

    const char *q = line;
    while (*q == ' ' || *q == '\t') q++;
    if (*q == '#') {
        in_preproc = true;
        attron(COLOR_PAIR(CP_C_PREPROC));
    }

    while (*p && x < max_x - 1) {
        if (!in_comment && strncmp(p, "//", 2) == 0) {
            if (in_preproc) { attroff(COLOR_PAIR(CP_C_PREPROC)); in_preproc = false; }
            attron(COLOR_PAIR(CP_C_COMMENT));
            mvaddstr(screen_row, x, p);
            attroff(COLOR_PAIR(CP_C_COMMENT));
            break;
        }
        if (!in_comment && strncmp(p, "/*", 2) == 0) {
            if (in_preproc) { attroff(COLOR_PAIR(CP_C_PREPROC)); in_preproc = false; }
            in_comment = true;
            attron(COLOR_PAIR(CP_C_COMMENT));
            mvaddch(screen_row, x, *p); p++; x++;
            if (*p && x < max_x - 1) { mvaddch(screen_row, x, *p); p++; x++; }
            continue;
        }
        if (in_comment) {
            if (strncmp(p, "*/", 2) == 0) {
                mvaddch(screen_row, x, *p); p++; x++;
                if (*p && x < max_x - 1) { mvaddch(screen_row, x, *p); p++; x++; }
                attroff(COLOR_PAIR(CP_C_COMMENT));
                in_comment = false;
                continue;
            }
            mvaddch(screen_row, x, *p); p++; x++;
            continue;
        }
        if (*p=='(' || *p==')' || *p=='{' || *p=='}' || *p=='[' || *p==']') {
            attron(COLOR_PAIR(CP_C_BRACKET));
            mvaddch(screen_row, x, *p); p++; x++;
            attroff(COLOR_PAIR(CP_C_BRACKET));
            continue;
        }
        if (*p == '"') {
            attron(COLOR_PAIR(CP_C_STRING));
            mvaddch(screen_row, x, *p); p++; x++;
            while (*p && x < max_x - 1) {
                mvaddch(screen_row, x, *p);
                if (*p == '"' && (*(p - 1) != '\\')) { p++; x++; break; }
                p++; x++;
            }
            attroff(COLOR_PAIR(CP_C_STRING));
            continue;
        }
        if (*p == '\'') {
            attron(COLOR_PAIR(CP_C_STRING));
            mvaddch(screen_row, x, *p); p++; x++;
            while (*p && x < max_x - 1) {
                mvaddch(screen_row, x, *p);
                if (*p == '\'' && (*(p - 1) != '\\')) { p++; x++; break; }
                p++; x++;
            }
            attroff(COLOR_PAIR(CP_C_STRING));
            continue;
        }
        if (*p == '<' && in_preproc) {
            attron(COLOR_PAIR(CP_C_STRING));
            mvaddch(screen_row, x, *p); p++; x++;
            while (*p && x < max_x - 1) {
                mvaddch(screen_row, x, *p);
                if (*p == '>') { p++; x++; break; }
                p++; x++;
            }
            attroff(COLOR_PAIR(CP_C_STRING));
            continue;
        }
        if (isdigit((unsigned char)*p)) {
            attron(COLOR_PAIR(CP_C_NUMBER));
            while (*p && x < max_x - 1 && (isalnum((unsigned char)*p) || *p=='.' || *p=='x' || *p=='X')) {
                mvaddch(screen_row, x, *p); p++; x++;
            }
            attroff(COLOR_PAIR(CP_C_NUMBER));
            continue;
        }
        const char *types[] = {
            "int","char","float","double","void","struct","enum","short","long",
            "signed","unsigned","const","volatile","extern","static","union",
            "size_t","ptrdiff_t","bool","_Bool","typedef","typename", NULL
        };
        bool matched_type = false;
        for (int k=0; types[k]; k++) {
            size_t len = strlen(types[k]);
            char prev = (p==line) ? ' ' : *(p-1);
            char next = p[len];
            if (strncmp(p, types[k], len)==0 && is_word_boundary(prev) && is_word_boundary(next)) {
                attron(COLOR_PAIR(CP_C_TYPE));
                mvaddnstr(screen_row, x, types[k], len);
                attroff(COLOR_PAIR(CP_C_TYPE));
                p += len; x += len;
                matched_type = true;
                break;
            }
        }
        if (matched_type) continue;
        if (in_preproc) {
            if (strncmp(p, "once", 4) == 0 && is_word_boundary((p==line)?' ':*(p-1)) && is_word_boundary(p[4])) {
                attron(COLOR_PAIR(CP_C_SPECIAL));
                mvaddnstr(screen_row, x, "once", 4);
                attroff(COLOR_PAIR(CP_C_SPECIAL));
                p += 4; x += 4;
                continue;
            }
            attron(COLOR_PAIR(CP_C_PREPROC));
            mvaddch(screen_row, x, *p); p++; x++;
            attroff(COLOR_PAIR(CP_C_PREPROC));
            continue;
        }
        mvaddch(screen_row, x, *p); p++; x++;
    }
}

static void scroll_view_to_cursor() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    int view_height = max_y - 2; 
    if (cur_row < top_row) top_row = cur_row;
    if (cur_row >= top_row + view_height) top_row = cur_row - view_height + 1;
    if (top_row < 0) top_row = 0;
}

static void draw(EditorMode mode, const char *cmdline, const GruvboxHardColors *t) {
    clear();
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    // Gutter set to 6: 4 digits for number + 2 spaces for a clean gap
    int gutter = 6; 

    if (mode != MODE_VISUAL) {
        scroll_view_to_cursor();
    }

    for (int i = 0; i < max_y - 2; i++) {
        int buffer_row = i + top_row; 
        if (buffer_row < line_count) {
            if (buffer_row == cur_row) attron(COLOR_PAIR(CP_CURSORLINE));
            else attron(COLOR_PAIR(CP_NORMAL));
            
            // Print number in first 4 columns
            mvprintw(i, 0, "%4d", buffer_row + 1);
            
            if (syntax_c) draw_line_with_syntax(i, buffer_row, gutter, max_x);
            else mvaddnstr(i, gutter, buffer[buffer_row], max_x - gutter - 1);
            
            if (buffer_row == cur_row) attroff(COLOR_PAIR(CP_CURSORLINE));
            else attroff(COLOR_PAIR(CP_NORMAL));
        } else {
            attron(COLOR_PAIR(CP_TILDE));
            mvaddch(i, 0, '~');
            attroff(COLOR_PAIR(CP_TILDE));
        }
    }

    attron(COLOR_PAIR(CP_STATUS));
    mvhline(max_y - 2, 0, ' ', max_x);
    const char *m =
        (mode == MODE_NORMAL) ? "NORMAL" :
        (mode == MODE_INSERT) ? "INSERT" :
        (mode == MODE_VISUAL) ? "VISUAL VIEW" : "COMMAND";
    mvprintw(max_y - 2, 0, "-- %s -- %s %s   row:%d col:%d   lines:%d   %s",
             m, filename[0] ? filename : "[No Name]", modified ? "[+]" : "",
             cur_row + 1, cur_col + 1, line_count, cmd_feedback);
    attroff(COLOR_PAIR(CP_STATUS));

    if (mode == MODE_COMMAND) {
        attron(COLOR_PAIR(CP_COMMAND));
        mvhline(max_y - 1, 0, ' ', max_x);
        mvprintw(max_y - 1, 0, ":%s", cmdline);
        attroff(COLOR_PAIR(CP_COMMAND));
    } else {
        attron(COLOR_PAIR(CP_NORMAL));
        mvhline(max_y - 1, 0, ' ', max_x);
        attroff(COLOR_PAIR(CP_NORMAL));
    }

    int vis_row = cur_row - top_row; 
    int vis_col = cur_col + gutter;
    if (vis_row >= max_y - 2) vis_row = max_y - 3;
    if (vis_col >= max_x) vis_col = max_x - 1;
    move(vis_row, vis_col);
    refresh();
}

static void insert_newline() {
    if (line_count >= MAX_LINES) return;
    char tail[MAX_COLS];
    strncpy(tail, &buffer[cur_row][cur_col], MAX_COLS - 1);
    tail[MAX_COLS - 1] = '\0';
    buffer[cur_row][cur_col] = '\0';
    memmove(&buffer[cur_row + 1], &buffer[cur_row], (size_t)(line_count - cur_row) * MAX_COLS);
    strncpy(buffer[cur_row + 1], tail, MAX_COLS - 1);
    buffer[cur_row + 1][MAX_COLS - 1] = '\0';
    line_count++; cur_row++; cur_col = 0; modified = true;
}

static void insert_char(int ch) {
    int len = (int)strlen(buffer[cur_row]);
    if (len >= MAX_COLS - 1) return;
    memmove(&buffer[cur_row][cur_col + 1], &buffer[cur_row][cur_col], (size_t)(len - cur_col + 1));
    buffer[cur_row][cur_col] = (char)ch;
    cur_col++; modified = true;
}

static void backspace() {
    if (cur_col > 0) {
        int len = (int)strlen(buffer[cur_row]);
        memmove(&buffer[cur_row][cur_col - 1], &buffer[cur_row][cur_col], (size_t)(len - cur_col + 1));
        cur_col--; modified = true;
    } else if (cur_row > 0) {
        int prev_len = (int)strlen(buffer[cur_row - 1]);
        int cur_len = (int)strlen(buffer[cur_row]);
        if (prev_len + cur_len < MAX_COLS - 1) {
            strcat(buffer[cur_row - 1], buffer[cur_row]);
            memmove(&buffer[cur_row], &buffer[cur_row + 1], (size_t)(line_count - cur_row) * MAX_COLS);
            line_count--; cur_row--; cur_col = prev_len; modified = true;
        }
    }
}

static void read_file() {
    if (!filename[0]) return;
    FILE *f = fopen(filename, "r");
    if (!f) { snprintf(cmd_feedback, sizeof(cmd_feedback), "E212: Can't open file"); return; }
    line_count = 0; char line[MAX_COLS];
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
        strncpy(buffer[line_count], line, MAX_COLS - 1);
        buffer[line_count][MAX_COLS - 1] = '\0';
        line_count++; if (line_count >= MAX_LINES) break;
    }
    fclose(f);
    if (line_count == 0) { line_count = 1; buffer[0][0] = '\0'; }
    cur_row = 0; cur_col = 0; top_row = 0; modified = false;
    snprintf(cmd_feedback, sizeof(cmd_feedback), "Read %s", filename);
}

static void write_file() {
    if (!filename[0]) { snprintf(cmd_feedback, sizeof(cmd_feedback), "E32: No file name"); return; }
    FILE *f = fopen(filename, "w");
    if (!f) { snprintf(cmd_feedback, sizeof(cmd_feedback), "E212: Can't open file for writing"); return; }
    for (int i = 0; i < line_count; i++) fprintf(f, "%s\n", buffer[i]);
    fclose(f); modified = false;
    snprintf(cmd_feedback, sizeof(cmd_feedback), "Written to %s", filename);
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        strncpy(filename, argv[1], sizeof(filename) - 1);
        filename[sizeof(filename) - 1] = '\0';
    }

    initscr();
    raw();
    keypad(stdscr, TRUE);
    noecho();
    curs_set(1);

    const GruvboxHardColors *theme = &gruvbox_hard;
    init_colors(theme);

    for (int i = 0; i < MAX_LINES; i++) buffer[i][0] = '\0';
    if (filename[0]) read_file();

    EditorMode mode = MODE_NORMAL;
    char cmdline[128] = {0};
    int cmdlen = 0;

    while (1) {
        if (is_emacs_header_present()) syntax_c = true;

        if (mode == MODE_VISUAL) timeout(68); 
        else timeout(-1); 

        draw(mode, cmdline, theme);
        int ch = getch();

        // 1. ESC / Alt+v Toggle
        if (ch == 27) { 
            nodelay(stdscr, TRUE);
            int next = getch();
            nodelay(stdscr, FALSE);
            
            if (next == 'v' || next == ERR) {
                if (mode == MODE_VISUAL) mode = MODE_NORMAL;
                else mode = MODE_VISUAL;
                continue;
            }
            mode = MODE_NORMAL;
        }

        // 2. Visual Mode Scrolling
        if (mode == MODE_VISUAL) {
            if (ch == ERR) {
                int max_y, max_x;
                getmaxyx(stdscr, max_y, max_x);
                int view_height = max_y - 2;
                int last_visible_row = top_row + view_height - 1;
                if (last_visible_row >= line_count) last_visible_row = line_count - 1;

                if (cur_row >= last_visible_row) cur_row = top_row;
                else cur_row++;

                int len = (int)strlen(buffer[cur_row]);
                if (cur_col > len) cur_col = len;
            } else {
                mode = MODE_NORMAL;
            }
            continue;
        }

        // 3. Normal / Insert / Command Modes
        if (mode == MODE_NORMAL) {
            if (ch == 'i') {
                mode = MODE_INSERT;
                cmd_feedback[0] = '\0';
            } else if (ch == ':') {
                mode = MODE_COMMAND;
                cmdlen = 0; cmdline[0] = '\0';
            } else if (ch == KEY_DOWN && cur_row < line_count - 1) {
                cur_row++;
                int len = (int)strlen(buffer[cur_row]);
                if (cur_col > len) cur_col = len;
            } else if (ch == KEY_UP && cur_row > 0) {
                cur_row--;
                int len = (int)strlen(buffer[cur_row]);
                if (cur_col > len) cur_col = len;
            } else if (ch == KEY_LEFT && cur_col > 0) {
                cur_col--;
            } else if (ch == KEY_RIGHT) {
                int len = (int)strlen(buffer[cur_row]);
                if (cur_col < len) cur_col++;
            }
        }
        else if (mode == MODE_INSERT) {
            if (ch == '\n') {
                if (is_comment_line(cur_row)) auto_comment_newline();
                else insert_newline();
            }
            else if (ch == '\t') for (int s = 0; s < 4; s++) insert_char(' ');
            else if (ch == KEY_BACKSPACE || ch == 127) backspace();
            else if (ch >= 32 && ch <= 126) {
                insert_char(ch);
                int max_y, max_x;
                getmaxyx(stdscr, max_y, max_x);
                // Auto-comment on screen side
                if (is_comment_line(cur_row) && (cur_col + 6 >= max_x - 1)) {
                    auto_comment_newline();
                }
            }
            else if (ch == KEY_LEFT && cur_col > 0) cur_col--;
            else if (ch == KEY_RIGHT) {
                int len = (int)strlen(buffer[cur_row]);
                if (cur_col < len) cur_col++;
            } else if (ch == KEY_UP && cur_row > 0) {
                cur_row--;
                int len = (int)strlen(buffer[cur_row]);
                if (cur_col > len) cur_col = len;
            } else if (ch == KEY_DOWN && cur_row < line_count - 1) {
                cur_row++;
                int len = (int)strlen(buffer[cur_row]);
                if (cur_col > len) cur_col = len;
            }
        }
        else if (mode == MODE_COMMAND) {
            if (ch == 10) { 
                cmd_feedback[0] = '\0';
                if (strcmp(cmdline, "q") == 0) {
                    if (modified) {
                        snprintf(cmd_feedback, sizeof(cmd_feedback), "E37: No write since last change");
                        mode = MODE_NORMAL;
                    } else break;
                } else if (strcmp(cmdline, "q!") == 0 || strcmp(cmdline, "!q") == 0) break;
                else if (strcmp(cmdline, "w") == 0) write_file();
                else if (strcmp(cmdline, "wq") == 0) { write_file(); break; }
                else if (strcmp(cmdline, "tsc") == 0) {
                    syntax_c = true;
                    snprintf(cmd_feedback, sizeof(cmd_feedback), "C syntax enabled");
                } else if (strcmp(cmdline, "tsc!") == 0 || strcmp(cmdline, "!tsc") == 0) {
                    syntax_c = false;
                    snprintf(cmd_feedback, sizeof(cmd_feedback), "C syntax disabled");
                }
                mode = MODE_NORMAL;
            } else if (ch == KEY_BACKSPACE || ch == 127) {
                if (cmdlen > 0) cmdline[--cmdlen] = '\0';
            } else if (ch >= 32 && ch <= 126) {
                if (cmdlen < (int)sizeof(cmdline) - 1) {
                    cmdline[cmdlen++] = (char)ch; cmdline[cmdlen] = '\0';
                }
            }
        }
    }

    endwin();
    return 0;
}
