#include <ncurses.h>
#include <stdlib.h>

int main() {
    // 1. Initialize the standard ncurses screen
    initscr(); 

    // 2. Check if the terminal supports color capabilities
    if (has_colors() == FALSE) {
        endwin();
        printf("Your terminal does not support color.\n");
        exit(1);
    }

    // 3. Initialize the ncurses color engine
    start_color(); 

    /* 
     * 4. Define custom color pairs using: init_pair(pair_id, foreground, background);
     * Note: Pair ID 0 is reserved for the terminal's default layout.
     */
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLUE);
    init_pair(3, COLOR_BLACK, COLOR_WHITE);

    // 5. Apply and print text using the color pairs
    
    // Example using attron/attroff
    attron(COLOR_PAIR(1));
    printw("This text is Red on a Black background.\n");
    attroff(COLOR_PAIR(1));

    // Example using the A_BOLD attribute to brighten colors
    attron(COLOR_PAIR(2) | A_BOLD);
    printw("This text is Bold Green on a Blue background.\n");
    attroff(COLOR_PAIR(2) | A_BOLD);

    // Example using mvprintw to place colored text at specific coordinates
    attron(COLOR_PAIR(3));
    mvprintw(5, 2, "This text is Black on a White background at row 5, col 2.");
    attroff(COLOR_PAIR(3));

    // Refresh the physical screen to render changes, then wait for user input
    refresh();
    getch(); 

    // 6. Safely clean up and close the window
    endwin(); 
    return 0;
}
