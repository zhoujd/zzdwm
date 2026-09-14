#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

/* Default password used if the PW environment variable is not set at runtime */
#define BACKUP_PASSWORD "admin123"
#define MAX_ATTEMPTS 3

/* Extracted layout function to easily refresh the screen on failure thresholds */
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
    char input[256]; /* Fixed to a proper buffer array for holding passcodes */
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
            
            /* If failed 3 times, reset the interface and counter */
            if (failed_attempts >= MAX_ATTEMPTS) {
                printf("Too many failed attempts. Re-initializing...\n");
                sleep(2); /* Give user a brief moment to see the warning */
                failed_attempts = 0;
                draw_lock_screen();
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
