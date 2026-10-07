#include <ncurses.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

// Color pair definitions
#define PAIR_TEXT     1
#define PAIR_KEYWORD  2
#define PAIR_STRING   3
#define PAIR_COMMENT  4
#define PAIR_INCLUDE  5

// Simple list of C keywords
const char *keywords[] = {
    "int", "char", "return", "if", "else", "while", "for", "void", "switch"
};
#define KEYWORD_COUNT (sizeof(keywords) / sizeof(keywords[0]))

int is_keyword(const char *word) {
    for (size_t i = 0; i < KEYWORD_COUNT; i++) {
        if (strcmp(word, keywords[i]) == 0) return 1;
    }
    return 0;
}

// Parses and draws a single line with multi-line comment state tracking
void draw_syntax_line(int row, const char *line, int *in_block_comment) {
    int col = 0;
    int i = 0;
    int len = strlen(line);

    while (i < len) {
        if (*in_block_comment) {
            attron(COLOR_PAIR(PAIR_COMMENT));
            while (i < len) {
                if (line[i] == '*' && i + 1 < len && line[i+1] == '/') {
                    mvaddch(row, col++, line[i++]);
                    mvaddch(row, col++, line[i++]);
                    *in_block_comment = 0;
                    attroff(COLOR_PAIR(PAIR_COMMENT));
                    break;
                }
                if (line[i] != '\n' && line[i] != '\r') {
                    mvaddch(row, col++, line[i]);
                }
                i++;
            }
            continue;
        }

        if (line[i] == '/' && i + 1 < len && line[i+1] == '*') {
            *in_block_comment = 1;
            attron(COLOR_PAIR(PAIR_COMMENT));
            mvaddch(row, col++, line[i++]);
            mvaddch(row, col++, line[i++]);
            continue;
        }

        if (line[i] == '/' && i + 1 < len && line[i+1] == '/') {
            attron(COLOR_PAIR(PAIR_COMMENT));
            while (i < len && line[i] != '\n' && line[i] != '\r') {
                mvaddch(row, col++, line[i++]);
            }
            attroff(COLOR_PAIR(PAIR_COMMENT));
            continue;
        }

        if (line[i] == '#') {
            attron(COLOR_PAIR(PAIR_INCLUDE));
            while (i < len && !isspace((unsigned char)line[i])) {
                mvaddch(row, col++, line[i++]);
            }
            attroff(COLOR_PAIR(PAIR_INCLUDE));
            continue;
        }

        if (line[i] == '"') {
            attron(COLOR_PAIR(PAIR_STRING));
            mvaddch(row, col++, line[i++]);
            while (i < len && line[i] != '"') {
                mvaddch(row, col++, line[i++]);
            }
            if (i < len && line[i] == '"') {
                mvaddch(row, col++, line[i++]);
            }
            attroff(COLOR_PAIR(PAIR_STRING));
            continue;
        }

        if (isalpha((unsigned char)line[i]) || line[i] == '_') {
            char word[64];
            int w_len = 0;
            while (i < len && (isalnum((unsigned char)line[i]) || line[i] == '_') && w_len < 63) {
                word[w_len++] = line[i++];
            }
            word[w_len] = '\0';

            if (is_keyword(word)) {
                attron(COLOR_PAIR(PAIR_KEYWORD) | A_BOLD);
                mvprintw(row, col, "%s", word);
                attroff(COLOR_PAIR(PAIR_KEYWORD) | A_BOLD);
            } else {
                attron(COLOR_PAIR(PAIR_TEXT));
                mvprintw(row, col, "%s", word);
                attroff(COLOR_PAIR(PAIR_TEXT));
            }
            col += w_len;
            continue;
        }

        attron(COLOR_PAIR(PAIR_TEXT));
        if (line[i] != '\n' && line[i] != '\r') {
            mvaddch(row, col++, line[i]);
        }
        attroff(COLOR_PAIR(PAIR_TEXT));
        i++;
    }
}

int main(int argc, char *argv[]) {
    // 1. Validate that a file argument was provided
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <path_to_c_file>\n", argv[0]);
        return 1;
    }

    // Try to open the user-provided file before initializing ncurses
    FILE *file = fopen(argv[1], "r");
    if (!file) {
        perror("Error opening file");
        return 1;
    }

    // 2. Initialize ncurses environment
    initscr();
    cbreak();
    noecho();
    curs_set(0);

    if (!has_colors()) {
        endwin();
        fclose(file);
        printf("Error: Terminal doesn't support color.\n");
        return 1;
    }
    start_color();

    // Force strict black background profile settings
    use_default_colors();
    assume_default_colors(-1, -1);

    init_pair(PAIR_TEXT,    COLOR_WHITE,   -1);
    init_pair(PAIR_KEYWORD, COLOR_CYAN,    -1);
    init_pair(PAIR_STRING,  COLOR_YELLOW,  -1);
    init_pair(PAIR_COMMENT, COLOR_GREEN,   -1);
    init_pair(PAIR_INCLUDE, COLOR_MAGENTA, -1);

    bkgd(COLOR_PAIR(PAIR_TEXT));
    clear();

    // 3. Dynamic header using the passed filename
    mvprintw(0, 0, "--- Rendering: %s (Press any key to exit) ---", argv[1]);

    char buffer[256];
    int current_row = 1;
    int in_block_comment = 0;

    while (fgets(buffer, sizeof(buffer), file) && current_row < LINES - 1) {
        draw_syntax_line(current_row, buffer, &in_block_comment);
        current_row++;
    }
    fclose(file);

    refresh();
    getch();

    endwin();
    return 0;
}
