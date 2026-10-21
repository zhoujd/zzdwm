#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <locale.h>
#include <ncursesw/ncurses.h>

// Define window pointers for the three zones and their respective inner sub-windows
WINDOW *win_top_box, *win_top;
WINDOW *win_mid_box, *win_mid;
WINDOW *win_bot_box, *win_bot;

pthread_mutex_t ui_mutex = PTHREAD_MUTEX_INITIALIZER;
int is_running = 1;

// Initializes a composite layout containing a border and an independent scrollable sub-window
void create_managed_window(int height, int width, int start_y, int start_x,
                           const char* title, WINDOW **box_out, WINDOW **sub_out) {
    // 1. Create the outer window dedicated to rendering the borders
    *box_out = newwin(height, width, start_y, start_x);
    box(*box_out, 0, 0);
    mvwprintw(*box_out, 0, 2, " %s ", title);
    wrefresh(*box_out);

    // 2. Create the inner window for writing text (padded by 1 cell to prevent breaking borders)
    *sub_out = newwin(height - 2, width - 2, start_y + 1, start_x + 1);

    // 3. Core feature: Enable native automatic scrolling for the inner window
    scrollok(*sub_out, TRUE);
    idlok(*sub_out, TRUE);
    wsetscrreg(*sub_out, 0, height - 3);
}

// Background thread simulating the AI Agent (Tool Calls and streaming responses)
void* ai_agent_simulation(void* arg) {
    (void)arg;
    sleep(1); // Wait for the UI layout to finish setting up in the main thread

    // Simulate logs in the Agent Thoughts & System Status panel
    pthread_mutex_lock(&ui_mutex);
    wprintw(win_top, "> Invoking system tool: gcc -o test test.c\n");
    wprintw(win_top, "> Compilation error: test.c:5:10: error...\n");
    wrefresh(win_top);
    pthread_mutex_unlock(&ui_mutex);

    // Simulate token streaming in the AI Response panel (supports UTF-8 multi-line scrolling)
    const char* tokens[] = {
        "I ", "noticed ", "your ", "code ", "is ", "missing ", "a ", "semicolon. ",
        "I ", "have ", "fixed ", "this ", "bug ", "for ", "you.\n",
        "> Attempting to recompile...\n",
        "> Compilation successful! Binary file generated successfully.\n",
        "You can now run `./test` to verify the execution result.\n",
        "Feel free to type a prompt below if you have more Linux C questions!\n",
        "(Testing automatic scrolling endurance...)\n",
        "Line A: This line proves the underlying Ncurses viewport auto-shift.\n",
        "Line B: Older lines will be gracefully pushed upward as new tokens arrive.\n",
        NULL
    };

    for(int i = 0; tokens[i] != NULL && is_running; i++) {
        pthread_mutex_lock(&ui_mutex);
        wprintw(win_mid, "%s", tokens[i]);
        wrefresh(win_mid); // Localized refresh creates the smooth typewriter streaming effect
        pthread_mutex_unlock(&ui_mutex);
        usleep(250000); // 250ms interval between each streaming token
    }

    return NULL;
}

int main() {
    // Critical: Initialize system locale to support UTF-8 characters. Must be called before initscr()
    setlocale(LC_ALL, "");

    // Initialize the ncurses screen environment
    initscr();
    cbreak();             // Disable line buffering; keys are immediately available to the program
    noecho();             // Do not automatically echo typed characters onto the terminal screen
    keypad(stdscr, TRUE); // Enable mapping of function keys, arrow keys, and system shortcuts
    curs_set(1);          // Make the cursor visible

    int parent_x, parent_y;
    getmaxyx(stdscr, parent_y, parent_x);

    // Calculate window heights (the combined heights must dynamically adapt to the terminal size)
    int top_h = 6;
    int bot_h = 3;
    int mid_h = parent_y - top_h - bot_h; // Dynamically expand the middle panel to fill up space

    // Initialize the three managed pane sections
    create_managed_window(top_h, parent_x, 0, 0, "1. Agent Thoughts & System Status", &win_top_box, &win_top);
    create_managed_window(mid_h, parent_x, top_h, 0, "2. AI Response (Streaming Output)", &win_mid_box, &win_mid);
    create_managed_window(bot_h, parent_x, top_h + mid_h, 0, "3. Keyboard Input (User Prompt)", &win_bot_box, &win_bot);

    // Focus the cursor inside the bottom prompt pane by default
    wmove(win_bot, 0, 0);
    wprintw(win_bot, "Prompt > ");
    wrefresh(win_bot);

    // Launch the asynchronous AI background thread
    pthread_t ai_thread;
    pthread_create(&ai_thread, NULL, ai_agent_simulation, NULL);

    // Main thread: Keyboard input capture loop
    char input_buf[256];
    int buf_idx = 0;

    while (is_running) {
        int ch = wgetch(win_bot); // Block and wait for a keystroke in the input pane

        if (ch == KEY_F(1) || ch == 27) { // Exit cleanly if F1 or ESC is pressed
            is_running = 0;
            break;
        }

        pthread_mutex_lock(&ui_mutex);
        if (ch == '\n' || ch == KEY_ENTER) {
            // User pressed Enter; submit the completed prompt buffer
            input_buf[buf_idx] = '\0';

            // Mirror the user's prompt directly into the conversational response stream
            wprintw(win_mid, "\n[User]: %s\n", input_buf);
            wrefresh(win_mid);

            // Wipe out the current text inside the input box and reset the prompt cursor
            wclear(win_bot);
            wprintw(win_bot, "Prompt > ");
            wrefresh(win_bot);
            buf_idx = 0;
        }
        else if ((ch == KEY_BACKSPACE || ch == 127 || ch == '\b') && buf_idx > 0) {
            // Handle Backspace operations
            buf_idx--;
            int curr_y, curr_x;
            getyx(win_bot, curr_y, curr_x);
            mvwaddch(win_bot, curr_y, curr_x - 1, ' ');
            wmove(win_bot, curr_y, curr_x - 1);
            wrefresh(win_bot);
        }
        else if (ch >= 32 && ch <= 126 && buf_idx < 250) {
            // Printable ASCII characters are written onto the screen and captured into the buffer
            input_buf[buf_idx++] = ch;
            waddch(win_bot, ch);
            wrefresh(win_bot);
        }
        pthread_mutex_unlock(&ui_mutex);
    }

    // Join the background thread and clean up terminal resources to restore system shell state
    pthread_join(ai_thread, NULL);
    delwin(win_top); delwin(win_top_box);
    delwin(win_mid); delwin(win_mid_box);
    delwin(win_bot); delwin(win_bot_box);
    endwin();

    printf("TUI Agent Exited Gracefully.\n");
    return 0;
}
