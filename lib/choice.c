#include "choice.h"


// Let the user choose and option and return the index of the option
int ChoiceMCQ(const char *options[], size_t num_choices){
    struct termios orig;
    EnableRawMode(&orig);


    int choice = 0;

    printf("\n\n\n");

    while (1) {
        // Moove the cursor
        printf("\033[%zuA", num_choices);

        for (int i = 0; i < num_choices; i++) {
            // Erase the line
            printf("\033[2K");

            if (i == choice)
                printf("> %s\n", options[i]);
            else
                printf("  %s\n", options[i]);
        }

        char c = getchar();

        if (c == 27) {
            getchar();

            char dir = getchar();
            if (dir == 'A') 
                choice = (choice - 1 + num_choices) % num_choices;

            if (dir == 'B') 
                choice = (choice + 1) % num_choices;
        } else if (c == '\n') {
            break;
        }
    }

    DisableRawMode(&orig);
    return choice;
}


void EnableRawMode(struct termios *orig){
    struct termios raw;
    tcgetattr(STDIN_FILENO, orig);
    raw = *orig;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}


void DisableRawMode(struct termios *orig){
    tcsetattr(STDIN_FILENO, TCSAFLUSH, orig);
}