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

// Helper function to check if a word is a C keyword
int is_keyword(const char *word) {
    for (size_t i = 0; i < KEYWORD_COUNT; i++) {
        if (strcmp(word, keywords[i]) == 0) return 1;
    }
    return 0;
}

// Custom function to parse and draw a single line with colors
void draw_syntax_line(int row, const char *line) {
    int col = 0;
    int i = 0;
    int len = strlen(line);

    // Initial state: default text color
    attron(COLOR_PAIR(PAIR_TEXT));

    while (i < len) {
        // 1. Handle single-line comments (//) or multi-line block comments (/*)
        if ((line[i] == '/' && line[i+1] == '/') || (line[i] == '/' && line[i+1] == '*')) {
            attron(COLOR_PAIR(PAIR_COMMENT));
            while (i < len && line[i] != '\n') {
                mvaddch(row, col++, line[i++]);
            }
            attroff(COLOR_PAIR(PAIR_COMMENT));
            continue;
        }

        // 2. Handle Preprocessor Directives (#include, etc)
        if (line[i] == '#') {
            attron(COLOR_PAIR(PAIR_INCLUDE));
            while (i < len && !isspace(line[i])) {
                mvaddch(row, col++, line[i++]);
            }
            attroff(COLOR_PAIR(PAIR_INCLUDE));
            continue;
        }

        // 3. Handle Strings ("...")
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

        // 4. Handle Word Tokens (Extract alphanumeric blocks to check for keywords)
        if (isalpha(line[i]) || line[i] == '_') {
            char word[64];
            int w_len = 0;
            while (i < len && (isalnum(line[i]) || line[i] == '_') && w_len < 63) {
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
            }
            col += w_len;
            continue;
        }

        // 5. Default: Print standard punctuation and whitespace
        attron(COLOR_PAIR(PAIR_TEXT));
        if (line[i] != '\n' && line[i] != '\r') {
            mvaddch(row, col++, line[i]);
        }
        i++;
    }
}

int main() {
    // Initialize ncurses environment
    initscr();
    cbreak();
    noecho();
    curs_set(0); // Hide the terminal cursor

    if (!has_colors()) {
        endwin();
        printf("Error: Terminal doesn't support color.\n");
        return 1;
    }
    start_color();

    // Map color schemas
    init_pair(PAIR_TEXT,    COLOR_WHITE,   COLOR_BLACK);
    init_pair(PAIR_KEYWORD, COLOR_CYAN,    COLOR_BLACK);
    init_pair(PAIR_STRING,  COLOR_YELLOW,  COLOR_BLACK);
    init_pair(PAIR_COMMENT, COLOR_GREEN,   COLOR_BLACK);
    init_pair(PAIR_INCLUDE, COLOR_MAGENTA, COLOR_BLACK);

    // Open target source code file
    FILE *file = fopen("hello.c", "r");
    if (!file) {
        endwin();
        printf("Error: Could not open file 'hello.c'. Please create it first!\n");
        return 1;
    }

    // Read file line-by-line and feed lines to the color parser
    char buffer[256];
    int current_row = 1;
    
    mvprintw(0, 0, "--- Rendering: hello.c (Press any key to exit) ---");
    
    while (fgets(buffer, sizeof(buffer), file) && current_row < LINES - 1) {
        draw_syntax_line(current_row, buffer);
        current_row++;
    }
    fclose(file);

    // Render loop processing finish
    refresh();
    getch();

    endwin();
    return 0;
}
