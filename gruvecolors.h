#ifndef GRUVECOLORS_H
#define GRUVECOLORS_H

#include <stdint.h>

typedef struct GruvboxHardColors {
    // basic
    uint32_t bg;         // Background
    uint32_t fg;         // Default foreground
    uint32_t cursor;     // Cursor color
    uint32_t selection;  // Visual selection
    uint32_t line_number; // Line number gutter
    uint32_t status_bg;  // Status bar background
    uint32_t status_fg;  // Status bar text
    uint32_t mode_normal; // Mode indicator normal
    uint32_t mode_insert; // Mode indicator insert
    uint32_t mode_visual; // Mode indicator visual

    // syntax highlighting
    uint32_t comment;
    uint32_t keyword;
    uint32_t string;
    uint32_t number;
    uint32_t boolean;
    uint32_t constant;
    uint32_t type;
    uint32_t function;
    uint32_t variable;
    uint32_t operator;
    uint32_t error;
    uint32_t warning;
    uint32_t search;      // Highlight for search
    uint32_t line_highlight; // Current line highlight
} GruvboxHardColors;

// Example initialization with typical Gruvbox Hard Dark hex codes
static GruvboxHardColors gruvbox_hard = {
    .bg = 0x1d2021,          // dark background
    .fg = 0xebdbb2,          // default foreground
    .cursor = 0xd65d0e,      // bright orange
    .selection = 0x3c3836,   // darker gray for selection
    .line_number = 0x928374, // muted gray
    .status_bg = 0x1d2021,   // same as bg
    .status_fg = 0xebdbb2,   // bright fg
    .mode_normal = 0xb8bb26, // green
    .mode_insert = 0x83a598, // blue
    .mode_visual = 0xd3869b, // purple/magenta

    .comment = 0x928374,     // gray
    .keyword = 0xfb4934,     // red
    .string = 0xb8bb26,      // green
    .number = 0xd65d0e,      // orange
    .boolean = 0xd65d0e,     // orange
    .constant = 0xd3869b,    // purple
    .type = 0x83a598,        // blue
    .function = 0xb8bb26,    // green
    .variable = 0xebdbb2,    // fg
    .operator = 0xebdbb2,    // fg
    .error = 0xfb4934,       // red
    .warning = 0xfe8019,     // bright orange/yellow
    .search = 0xfe8019,      // same highlight as warning
    .line_highlight = 0x32302f // dark gray for current line
};

#endif
