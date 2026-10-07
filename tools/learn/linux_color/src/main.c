#include <ncurses.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Color pair definitions
#define PAIR_TEXT     1
#define PAIR_KEYWORD  2
#define PAIR_STRING   3
#define PAIR_COMMENT  4
#define PAIR_INCLUDE  5

// A simple list of C keywords to check against
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

// Custom function to parse and draw a single line with multi-line comment state tracking
void draw_syntax_line(int row, const char *line, int *in_block_comment) {
    int col = 0;
    int i = 0;
    int len = strlen(line);

    while (i < len) {
        // 1. If we are currently inside an active multi-line block comment
        if (*in_block_comment) {
            attron(COLOR_PAIR(PAIR_COMMENT));
            while (i < len) {
                // Check if we hit the closing multi-line token "*/"
                if (line[i] == '*' && i + 1 < len && line[i+1] == '/') {
                    mvaddch(row, col++, line[i++]); // Print '*'
                    mvaddch(row, col++, line[i++]); // Print '/'
                    *in_block_comment = 0;          // Exit comment state
                    attroff(COLOR_PAIR(PAIR_COMMENT));
                    break; 
                }
                // Print comment contents
                if (line[i] != '\n' && line[i] != '\r') {
                    mvaddch(row, col++, line[i]);
                }
                i++;
            }
            continue;
        }

        // 2. Handle opening of a multi-line block comment (/*)
        if (line[i] == '/' && i + 1 < len && line[i+1] == '*') {
            *in_block_comment = 1;
            attron(COLOR_PAIR(PAIR_COMMENT));
            mvaddch(row, col++, line[i++]); // Print '/'
            mvaddch(row, col++, line[i++]); // Print '*'
            continue;
        }

        // 3. Handle single-line comments (//)
        if (line[i] == '/' && i + 1 < len && line[i+1] == '/') {
            attron(COLOR_PAIR(PAIR_COMMENT));
            while (i < len && line[i] != '\n' && line[i] != '\r') {
                mvaddch(row, col++, line[i++]);
            }
            attroff(COLOR_PAIR(PAIR_COMMENT));
            continue;
        }

        // 4. Handle Preprocessor Directives (#include, etc)
        if (line[i] == '#') {
            attron(COLOR_PAIR(PAIR_INCLUDE));
            while (i < len && !isspace((unsigned char)line[i])) {
                mvaddch(row, col++, line[i++]);
            }
            attroff(COLOR_PAIR(PAIR_INCLUDE));
            continue;
        }

        // 5. Handle Strings ("...")
        if (line[i] == '"') {
            attron(COLOR_PAIR(PAIR_STRING));
            mvaddch(row, col++, line[i++]); // Print opening quote
            while (i < len && line[i] != '"') {
                mvaddch(row, col++, line[i++]);
            }
            if (i < len && line[i] == '"') {
                mvaddch(row, col++, line[i++]); // Print closing quote
            }
            attroff(COLOR_PAIR(PAIR_STRING));
            continue;
        }

        // 6. Handle Word Tokens (Keywords vs Identifiers)
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

        // 7. Default: Standard text / operators / whitespace
        attron(COLOR_PAIR(PAIR_TEXT));
        if (line[i] != '\n' && line[i] != '\r') {
            mvaddch(row, col++, line[i]);
        }
        attroff(COLOR_PAIR(PAIR_TEXT));
        i++;
    }
}

int main() {
    initscr();
    cbreak();
    noecho();
    curs_set(0);

    if (!has_colors()) {
        endwin();
        printf("Error: Terminal doesn't support color.\n");
        return 1;
    }
    start_color();

    /*
     * 1. Tell ncurses to map color ID -1 to the terminal's
     * true native default layout/canvas instead of hardware macros.
     */
    use_default_colors();
    assume_default_colors(-1, -1);

    // 2. Remap color pairs using -1 as the true black/default background
    init_pair(PAIR_TEXT,    COLOR_WHITE,   -1);
    init_pair(PAIR_KEYWORD, COLOR_CYAN,    -1);
    init_pair(PAIR_STRING,  COLOR_YELLOW,  -1);
    init_pair(PAIR_COMMENT, COLOR_GREEN,   -1);
    init_pair(PAIR_INCLUDE, COLOR_MAGENTA, -1);

    // 3. Flood fill the empty canvas memory space with the transparent/true pair
    bkgd(COLOR_PAIR(PAIR_TEXT));

    FILE *file = fopen("hello.c", "r");
    if (!file) {
        endwin();
        printf("Error: Could not open file 'hello.c'.\n");
        return 1;
    }

    char buffer[256];
    int current_row = 1;
    int in_block_comment = 0; // State persistent cross-line flag

    // Clear standard screen background layout before printing code lines
    clear();

    mvprintw(0, 0, "--- Rendering: hello.c (Press any key to exit) ---");

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
