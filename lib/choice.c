#include "choice.h"


// Let the user choose and option and return the index of the option
int ChoiceMCQ(const char *options[], int num_choices){
    struct termios orig;
    __EnableRawMode(&orig);


    int choice = 0;

    while (1) {

        for (int i = 0; i < num_choices; i++) {
            if (i == choice)
                printf("> \033[7m%s\033[0m\n", options[i]);
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


        // Moove the cursor
        printf("\033[%uA", num_choices);

        for (int i = 0; i < num_choices; i++) {
            // Erase the line
            printf("\033[2K");
        }
    }

    // Moove the cursor
    printf("\033[%uA", num_choices);

    for (int i = 0; i < num_choices; i++) {
        // Erase the line
        printf("\033[2K\n");
    }

    // Moove the cursor
    printf("\033[%uA", num_choices);
    

    __DisableRawMode(&orig);
    return choice;
}


void __EnableRawMode(struct termios *orig){
    struct termios raw;
    tcgetattr(STDIN_FILENO, orig);
    raw = *orig;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}


void __DisableRawMode(struct termios *orig){
    tcsetattr(STDIN_FILENO, TCSAFLUSH, orig);
}