#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

/* Configuration constants */
#define BACKUP_PASSWORD "admin123"
#define MAX_ATTEMPTS 3
#define LOCKOUT_DURATION_SECS 10

/* Extracted layout function to easily refresh the screen */
void draw_lock_screen() {
    /* Clear screen, home cursor, and hide cursor tracking */
    printf("\033[2J\033[H\033[?25l");
    printf("===========================================\n");
    printf("   Terminal Session Locked (User Space)    \n");
    printf("===========================================\n\n");
}

void set_echo(int enable) {
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);
    if (!enable)
        tty.c_lflag &= ~ECHO;  /* Disable character echoing on the terminal */
    else
        tty.c_lflag |= ECHO;   /* Restore character echoing */
    tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

int main() {
    char input[256]; /* Buffer array for holding passcodes safely */
    char *target_password;
    int failed_attempts = 0;

    /* Read the password dynamically from the environment variable */
    target_password = getenv("PW");
    
    /* Fallback to the backup password if PW is empty or not provided */
    if (target_password == NULL) {
        target_password = BACKUP_PASSWORD;
    }

    /* Ignore standard user-space termination signals (Ctrl+C, Ctrl+\, Ctrl+Z) */
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    /* Render initial UI */
    draw_lock_screen();

    while (1) {
        printf("Enter Passcode: ");
        fflush(stdout);
        
        set_echo(0); /* Hide keystrokes while typing the password */
        if (fgets(input, sizeof(input), stdin) == NULL) {
            clearerr(stdin);
            printf("\n");
            continue;
        }
        set_echo(1); /* Restore keyboard echo after input */
        printf("\n");

        /* Strip the trailing newline character from fgets input */
        input[strcspn(input, "\n")] = 0;

        /* Compare the user input against the target passcode */
        if (strcmp(input, target_password) == 0) {
            printf("Unlocked successfully.\n");
            break;
        } else {
            failed_attempts++;
            printf("Permission Denied.\n");
            
            /* If failed 3 times, enforce a timed lockout penalty */
            if (failed_attempts >= MAX_ATTEMPTS) {
                printf("Too many failed attempts!\n");
                
                /* Dynamic in-place countdown loop */
                for (int i = LOCKOUT_DURATION_SECS; i > 0; i--) {
                    /* \r moves cursor to the beginning of the line, keeping the screen clean */
                    printf("\r[!] Terminal cooling down... Try again in %d seconds. ", i);
                    fflush(stdout);
                    sleep(1);
                }
                
                /* 
                 * FIX: Purge all keys typed during the 10-second countdown.
                 * TCIFLUSH flushes data received but not read from the terminal buffer.
                 */
                tcflush(STDIN_FILENO, TCIFLUSH);

                /* Reset counter */
                failed_attempts = 0;
                
                /* Wipe out all previous input/output history visually and restart cleanly */
                draw_lock_screen();
                continue; 
            } else {
                /* Display remaining attempts left */
                printf("Attempts remaining: %d\n\n", MAX_ATTEMPTS - failed_attempts);
            }
        }
    }

    /* Restore the cursor and perform a final screen clear upon exit */
    printf("\033[?25h\033[2J\033[H");
    return 0;
}
