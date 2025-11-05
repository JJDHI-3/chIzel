// Emacs style mode select -*- C -*-
// --------------------------------

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>
#include "gruvecolors.h"   // your Gruvbox header

#define MAX_LINES 1000
#define MAX_COLS  512

typedef enum {
    MODE_NORMAL,
    MODE_INSERT,
    MODE_COMMAND
} EditorMode;

static char buffer[MAX_LINES][MAX_COLS];
static int line_count = 1;
static int cur_row = 0, cur_col = 0;
static int top_row = 0; // ADDED: Top visible line of the buffer
static bool modified = false;
static char filename[256] = {0};
static char cmd_feedback[128] = {0};
static bool syntax_c = false; // toggled by :tsc, disabled by :tsc! or :!tsc

#define MAX_COLS 512
extern char buffer[/* MAX_LINES */][MAX_COLS]; 
extern int line_count;

static const char *EMACS_HEADER = "// Emacs style mode select -*- C -*-";

bool is_emacs_header_present() {
    // 1. Check if the buffer is non-empty
    if (line_count == 0) {
        return false;
    }

    // 2. Use strcmp to compare the content of buffer[0] with the expected header.
    // strcmp returns 0 if the two strings are identical.
    if (strcmp(buffer[0], EMACS_HEADER) == 0) {
        return true;
    }

    return false;
}

// Color pair IDs
enum {
    CP_NORMAL = 1,
    CP_STATUS,
    CP_COMMAND,
    CP_CURSORLINE,
    CP_TILDE,
    CP_C_COMMENT,      // grey
    CP_C_STRING,       // green for strings/quotes and <> content
    CP_C_PREPROC,      // preprocessor directive green (# and whole directive): 0x8ec07b
    CP_C_TYPE,         // types (int, char, etc.): 0xfab92e
    CP_C_BRACKET,      // (), {}, []: darker tone of 0xfab92e
    CP_C_NUMBER,       // numbers: 0xd3869b
    CP_C_SPECIAL       // 'once' and pragma-style tokens: 0xd3869b
};

// Helper: scale 0–255 to ncurses 0–1000
static short scale255(int x) { return (short)((x * 1000) / 255); }

static void define_rgb(short idx, uint32_t hex) {
    short r = scale255((hex >> 16) & 0xFF);
    short g = scale255((hex >> 8) & 0xFF);
    short b = scale255(hex & 0xFF);
    init_color(idx, r, g, b);
}

// Make a slightly darker variant of a hex color (~80%)
static uint32_t darker(uint32_t hex) {
    int r = (hex >> 16) & 0xFF;
    int g = (hex >> 8) & 0xFF;
    int b = hex & 0xFF;
    r = (int)(r * 0.80);
    g = (int)(g * 0.80);
    b = (int)(b * 0.80);
    return (uint32_t)((r << 16) | (g << 8) | b);
}

static void init_colors(const GruvboxHardColors *t) {
    start_color();
    use_default_colors();

    bool can_custom = can_change_color() && COLORS >= 64;

    // UI indices from your theme (unchanged)
    short IDX_BG = 20, IDX_FG = 21, IDX_STATUS_BG = 22, IDX_STATUS_FG = 23;
    short IDX_CMD_FG = 24, IDX_LINEHL_BG = 25, IDX_TILDE = 26;

    // Syntax indices (explicit hex as requested)
    short IDX_COMMENT = 27;                   // t->comment
    short IDX_STRING = 28;                    // t->string (green)
    short IDX_PREPROC = 29;                   // 0x8ec07b (preprocessor green)
    short IDX_TYPE = 30;                      // 0xfab92e (type yellow/orange)
    short IDX_BRACKET = 31;                   // darker(0xfab92e)
    short IDX_NUMBER = 32;                    // 0xd3869b (purple)
    short IDX_SPECIAL = 33;                   // 0xd3869b (purple for 'once' etc.)

    if (can_custom) {
        // UI
        define_rgb(IDX_BG, t->bg);
        define_rgb(IDX_FG, t->fg);
        define_rgb(IDX_STATUS_BG, t->status_bg);
        define_rgb(IDX_STATUS_FG, t->status_fg);
        define_rgb(IDX_CMD_FG, t->mode_normal);
        define_rgb(IDX_LINEHL_BG, t->line_highlight);
        define_rgb(IDX_TILDE, t->line_number);

        // Syntax colors
        define_rgb(IDX_COMMENT, t->comment);
        define_rgb(IDX_STRING, t->string);
        define_rgb(IDX_PREPROC, 0x8ec07b);
        define_rgb(IDX_TYPE, 0xfab92e);
        define_rgb(IDX_BRACKET, 0xd65a0e);
        define_rgb(IDX_NUMBER, 0xd3869b);
        define_rgb(IDX_SPECIAL, 0xd3869b);

        // Pairs
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
        // Fallback approximations
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

// Utility: check word boundary
static bool is_word_boundary(char c) {
    return !(isalnum((unsigned char)c) || c == '_');
}

// Draw a single line with C syntax highlighting
// UPDATED: Takes 'screen_row' (where to draw) and 'buffer_row' (what to read)
static void draw_line_with_syntax(int screen_row, int buffer_row, int gutter, int max_x) {
    const char *line = buffer[buffer_row];
    const char *p = line;
    int x = gutter;
    bool in_string = false, in_char = false, in_comment = false, in_preproc = false;

    // Preprocessor line: first non-space is '#'
    const char *q = line;
    while (*q == ' ' || *q == '\t') q++;
    if (*q == '#') {
        in_preproc = true;
        attron(COLOR_PAIR(CP_C_PREPROC));
    }

    while (*p && x < max_x - 1) {
        // Comments
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

        // Brackets: darker type color
        if (*p=='(' || *p==')' || *p=='{' || *p=='}' || *p=='[' || *p==']') {
            attron(COLOR_PAIR(CP_C_BRACKET));
            mvaddch(screen_row, x, *p); p++; x++;
            attroff(COLOR_PAIR(CP_C_BRACKET));
            continue;
        }

        // Strings (ensure both quotes green, with escaping)
        if (*p == '"' && !in_char) {
            attron(COLOR_PAIR(CP_C_STRING));
            mvaddch(screen_row, x, *p); p++; x++;
            while (*p && x < max_x - 1) {
                mvaddch(screen_row, x, *p);
                if (*p == '"' && (*(p - 1) != '\\')) {
                    p++; x++;
                    break;
                }
                p++; x++;
            }
            attroff(COLOR_PAIR(CP_C_STRING));
            continue;
        }

        // Char literals
        if (*p == '\'' && !in_string) {
            attron(COLOR_PAIR(CP_C_STRING));
            mvaddch(screen_row, x, *p); p++; x++;
            while (*p && x < max_x - 1) {
                mvaddch(screen_row, x, *p);
                if (*p == '\'' && (*(p - 1) != '\\')) {
                    p++; x++;
                    break;
                }
                p++; x++;
            }
            attroff(COLOR_PAIR(CP_C_STRING));
            continue;
        }

        // Angle-brackets in includes: green
        if (*p == '<' && in_preproc) {
            attron(COLOR_PAIR(CP_C_STRING));
            mvaddch(screen_row, x, *p); p++; x++;
            while (*p && x < max_x - 1) {
                mvaddch(screen_row, x, *p);
                if (*p == '>') {
                    p++; x++;
                    break;
                }
                p++; x++;
            }
            attroff(COLOR_PAIR(CP_C_STRING));
            continue;
        }

        // Numbers: purple (0xd3869b)
        if (isdigit((unsigned char)*p)) {
            attron(COLOR_PAIR(CP_C_NUMBER));
            while (*p && x < max_x - 1 && (isalnum((unsigned char)*p) || *p=='.' || *p=='x' || *p=='X')) {
                mvaddch(screen_row, x, *p);
                p++; x++;
            }
            attroff(COLOR_PAIR(CP_C_NUMBER));
            continue;
        }

        // Types (yellow/orange 0xfab92e): word list with boundaries
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

        // Preprocessor directive body: whole directive green, with special tokens purple
        if (in_preproc) {
            // Detect special token 'pragma' and 'once' in the directive and color 'once' purple
            if (strncmp(p, "once", 4) == 0 && is_word_boundary((p==line)?' ':*(p-1)) && is_word_boundary(p[4])) {
                attron(COLOR_PAIR(CP_C_SPECIAL));
                mvaddnstr(screen_row, x, "once", 4);
                attroff(COLOR_PAIR(CP_C_SPECIAL));
                p += 4; x += 4;
                continue;
            }
            if (strncmp(p, "pragma", 6) == 0 && is_word_boundary((p==line)?' ':*(p-1)) && is_word_boundary(p[6])) {
                // 'pragma' itself stays preproc green
                attron(COLOR_PAIR(CP_C_PREPROC));
                mvaddnstr(screen_row, x, "pragma", 6);
                attroff(COLOR_PAIR(CP_C_PREPROC));
                p += 6; x += 6;
                continue;
            }
            // #define FOO 123 → identifiers stay preproc green, numbers are purple
            if (isdigit((unsigned char)*p)) {
                attron(COLOR_PAIR(CP_C_NUMBER));
                while (*p && x < max_x - 1 && (isalnum((unsigned char)*p) || *p=='.')) {
                    mvaddch(screen_row, x, *p);
                    p++; x++;
                }
                attroff(COLOR_PAIR(CP_C_NUMBER));
                continue;
            }
            // Default preproc green
            attron(COLOR_PAIR(CP_C_PREPROC));
            mvaddch(screen_row, x, *p); p++; x++;
            attroff(COLOR_PAIR(CP_C_PREPROC));
            continue;
        }

        // Default
        mvaddch(screen_row, x, *p); p++; x++;
    }
}

// NEW: Function to adjust the viewport to keep the cursor visible
static void scroll_view_to_cursor() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    int view_height = max_y - 2; // -2 for status/cmd lines

    // If cursor is above the viewport
    if (cur_row < top_row) {
        top_row = cur_row;
    }

    // If cursor is below the viewport
    if (cur_row >= top_row + view_height) {
        top_row = cur_row - view_height + 1;
    }

    if (top_row < 0) top_row = 0;
}


static void draw(EditorMode mode, const char *cmdline, const GruvboxHardColors *t) {
    clear();

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    int gutter = 4; // "NNN " width

    // NEW: Scroll the view before drawing
    scroll_view_to_cursor();

    // Draw buffer lines and tildes
    for (int i = 0; i < max_y - 2; i++) {
        int buffer_row = i + top_row; // Calculate buffer line index

        if (buffer_row < line_count) {
            if (buffer_row == cur_row) attron(COLOR_PAIR(CP_CURSORLINE));
            else attron(COLOR_PAIR(CP_NORMAL));

            mvprintw(i, 0, "%d ", buffer_row + 1); // Draw line number at screen row 'i'
            if (syntax_c) {
                // Call with screen row 'i' and buffer row 'buffer_row'
                draw_line_with_syntax(i, buffer_row, gutter, max_x);
            } else {
                // Draw buffer line at screen row 'i'
                mvaddnstr(i, gutter, buffer[buffer_row], max_x - gutter - 1);
            }

            if (buffer_row == cur_row) attroff(COLOR_PAIR(CP_CURSORLINE));
            else attroff(COLOR_PAIR(CP_NORMAL));
        } else {
            attron(COLOR_PAIR(CP_TILDE));
            mvaddch(i, 0, '~');
            attroff(COLOR_PAIR(CP_TILDE));
        }
    }

    // Status line
    attron(COLOR_PAIR(CP_STATUS));
    mvhline(max_y - 2, 0, ' ', max_x);
    const char *m =
        (mode == MODE_NORMAL) ? "NORMAL" :
        (mode == MODE_INSERT) ? "INSERT" : "COMMAND";
    mvprintw(max_y - 2, 0, "-- %s -- %s %s   row:%d col:%d   lines:%d   %s",
             m,
             filename[0] ? filename : "[No Name]",
             modified ? "[+]" : "",
             cur_row + 1, cur_col + 1, line_count,
             cmd_feedback);
    attroff(COLOR_PAIR(CP_STATUS));

    // Command line
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

    // Place cursor (account for gutter and scroll offset)
    int vis_row = cur_row - top_row; // NEW: Cursor's screen row calculation
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

    memmove(&buffer[cur_row + 1], &buffer[cur_row],
            (size_t)(line_count - cur_row) * MAX_COLS);
    strncpy(buffer[cur_row + 1], tail, MAX_COLS - 1);
    buffer[cur_row + 1][MAX_COLS - 1] = '\0';

    line_count++;
    cur_row++;
    cur_col = 0;
    modified = true;
}

static void insert_char(int ch) {
    int len = (int)strlen(buffer[cur_row]);
    if (len >= MAX_COLS - 1) return;
    memmove(&buffer[cur_row][cur_col + 1],
            &buffer[cur_row][cur_col],
            (size_t)(len - cur_col + 1));
    buffer[cur_row][cur_col] = (char)ch;
    cur_col++;
    modified = true;
}

static void backspace() {
    if (cur_col > 0) {
        int len = (int)strlen(buffer[cur_row]);
        memmove(&buffer[cur_row][cur_col - 1],
                &buffer[cur_row][cur_col],
                (size_t)(len - cur_col + 1));
        cur_col--;
        modified = true;
    } else if (cur_row > 0) {
        int prev_len = (int)strlen(buffer[cur_row - 1]);
        int cur_len = (int)strlen(buffer[cur_row]);
        if (prev_len + cur_len < MAX_COLS - 1) {
            strcat(buffer[cur_row - 1], buffer[cur_row]);
            memmove(&buffer[cur_row], &buffer[cur_row + 1],
                    (size_t)(line_count - cur_row) * MAX_COLS);
            line_count--;
            cur_row--;
            cur_col = prev_len;
            modified = true;
        }
    }
}

static void read_file() {
    if (!filename[0]) return;
    FILE *f = fopen(filename, "r");
    if (!f) {
        snprintf(cmd_feedback, sizeof(cmd_feedback), "E212: Can't open file");
        return;
    }

    line_count = 0;
    char line[MAX_COLS];
    while (fgets(line, sizeof(line), f)) {
        // strip trailing newline
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
            line[--n] = '\0';
        }
        strncpy(buffer[line_count], line, MAX_COLS - 1);
        buffer[line_count][MAX_COLS - 1] = '\0';
        line_count++;
        if (line_count >= MAX_LINES) break;
    }
    fclose(f);

    if (line_count == 0) {
        line_count = 1;
        buffer[0][0] = '\0';
    }

    cur_row = 0;
    cur_col = 0;
    top_row = 0; // NEW: Reset scroll
    modified = false;
    snprintf(cmd_feedback, sizeof(cmd_feedback), "Read %s", filename);
}

static void write_file() {
    if (!filename[0]) {
        snprintf(cmd_feedback, sizeof(cmd_feedback), "E32: No file name");
        return;
    }
    FILE *f = fopen(filename, "w");
    if (!f) {
        snprintf(cmd_feedback, sizeof(cmd_feedback), "E212: Can't open file for writing");
        return;
    }
    for (int i = 0; i < line_count; i++) {
        fprintf(f, "%s\n", buffer[i]);
    }
    fclose(f);
    modified = false;
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

    // Initialize empty buffer
    for (int i = 0; i < MAX_LINES; i++) buffer[i][0] = '\0';
    if (filename[0]) read_file();

    EditorMode mode = MODE_NORMAL;
    char cmdline[128] = {0};
    int cmdlen = 0;

    draw(mode, cmdline, theme);

    int ch;
    while (1) {
        ch = getch();

        if (mode == MODE_NORMAL) {
            if (ch == 'i') {
                mode = MODE_INSERT;
                cmd_feedback[0] = '\0';
                draw(mode, cmdline, theme);
                continue;
            } else if (ch == ':') {
                mode = MODE_COMMAND;
                cmdlen = 0;
                cmdline[0] = '\0';
                draw(mode, cmdline, theme);
                continue;
            } else if (ch == 27) { // ESC
                mode = MODE_NORMAL;
                draw(mode, cmdline, theme);
                continue;
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
            if (ch == 27) { // ESC → NORMAL (immediate)
                mode = MODE_NORMAL;
                draw(mode, cmdline, theme);
                continue;
            } else if (ch == '\n') {
                insert_newline();
            } else if (ch == '\t') {
                for (int s = 0; s < 4; s++) insert_char(' ');
            } else if (ch == KEY_BACKSPACE || ch == 127) {
                backspace();
            } else if (ch >= 32 && ch <= 126) {
                insert_char(ch);
            } else if (ch == KEY_LEFT && cur_col > 0) {
                cur_col--;
            } else if (ch == KEY_RIGHT) {
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
            if (ch == 10) { // Enter
                cmd_feedback[0] = '\0';
                if (strcmp(cmdline, "q") == 0) {
                    if (modified) {
                        snprintf(cmd_feedback, sizeof(cmd_feedback),
                                 "E37: No write since last change (add ! to override)");
                        mode = MODE_NORMAL;
                        draw(mode, cmdline, theme);
                        continue;
                    } else {
                        break;
                    }
                } else if (strcmp(cmdline, "q!") == 0 || strcmp(cmdline, "!q") == 0) {
                    break;
                } else if (strcmp(cmdline, "w") == 0) {
                    write_file();
                    mode = MODE_NORMAL;
                    draw(mode, cmdline, theme);
                    continue;
                } else if (strcmp(cmdline, "wq") == 0) {
                    write_file();
                    break;
                } else if (strcmp(cmdline, "tsc") == 0) {
                    syntax_c = true;
                    if (!is_emacs_header_present()) {
                        // Insert Emacs style header if not already present
                        const char *header = "// Emacs style mode select -*- C -*-";
                        if (line_count == 0 || strcmp(buffer[0], header) != 0) {
                            // Shift lines down
                            if (line_count < MAX_LINES) {
                                memmove(&buffer[1], &buffer[0], line_count * MAX_COLS);
                                strncpy(buffer[0], header, MAX_COLS - 1);
                                buffer[0][MAX_COLS - 1] = '\0';
                                line_count++;
                                modified = true;
                            }
                        }
                    }

                    snprintf(cmd_feedback, sizeof(cmd_feedback), "C syntax highlighting enabled");
                    mode = MODE_NORMAL;
                    draw(mode, cmdline, theme);
                    continue;
                } else if (strcmp(cmdline, "tsc!") == 0 || strcmp(cmdline, "!tsc") == 0) {
                    syntax_c = false; // disable only with bang
                    snprintf(cmd_feedback, sizeof(cmd_feedback), "C syntax highlighting disabled");
                    mode = MODE_NORMAL;
                    draw(mode, cmdline, theme);
                    continue;
                } else {
                    snprintf(cmd_feedback, sizeof(cmd_feedback), "Not an editor command: %s", cmdline);
                }
                mode = MODE_NORMAL;
                draw(mode, cmdline, theme);
                continue;
            } else if (ch == 27) { // ESC cancels command
                mode = MODE_NORMAL;
                draw(mode, cmdline, theme);
                continue;
            } else if (ch == KEY_BACKSPACE || ch == 127) {
                if (cmdlen > 0) {
                    cmdline[--cmdlen] = '\0';
                }
            } else if (ch >= 32 && ch <= 126) {
                if (cmdlen < (int)sizeof(cmdline) - 1) {
                    cmdline[cmdlen++] = (char)ch;
                    cmdline[cmdlen] = '\0';
                }
            }
        }

        draw(mode, cmdline, theme);
    }

    endwin();
    return 0;
}
