#include "choice.h"

// State wanted while a question is displayed (raw mode)
static struct termios choice_raw;
static int choice_raw_valid = 0;

// Another process can put the terminal back in normal mode while a question is
// displayed (a download that ends in the background, a git / ssh command...) :
// the keys are then written on the screen (^[[A, ^[[B) instead of being read
// by the menu. Check it and set the raw mode again if needed.
static void __KeepRawMode(void)
{
    struct termios now;
    if (!choice_raw_valid || tcgetattr(STDIN_FILENO, &now) != 0)
        return;

    if ((now.c_lflag & (ECHO | ICANON)) != 0)
        tcsetattr(STDIN_FILENO, TCSANOW, &choice_raw);
}

// Read one byte of stdin without ever staying blocked with a wrong terminal
// state. timeout_ms < 0 : wait as long as needed.
// 1 : a byte was read | 0 : timeout | -1 : end of input
static int __ReadByte(unsigned char *c, int timeout_ms)
{
    int waited = 0;
    while (1)
    {
        __KeepRawMode();

        int tick = CHOICE_TICK_MS;
        if (timeout_ms >= 0 && timeout_ms - waited < tick)
            tick = timeout_ms - waited;

        struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
        int ready = poll(&pfd, 1, tick);
        if (ready < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }

        if (ready > 0)
        {
            ssize_t count = read(STDIN_FILENO, c, 1);
            if (count == 1)
                return 1;
            if (count < 0 && errno == EINTR)
                continue;
            return -1;
        }

        waited += tick;
        if (timeout_ms >= 0 && waited >= timeout_ms)
            return 0;
    }
}

// An empty option ("") is a blank separator : it is displayed as an empty line
// and can never be selected
static int __Selectable(const char *options[], int index)
{
    return options[index][0] != '\0';
}

// Next selectable option from "from" in the direction step (+1 / -1), looping.
// Stays on "from" when there is no other selectable option.
static int __NextSelectable(const char *options[], int num_choices, int from,
                            int step)
{
    int index = from;
    for (int k = 0; k < num_choices; k++)
    {
        index = (index + step + num_choices) % num_choices;
        if (__Selectable(options, index))
            return index;
    }
    return from;
}

// Let the user choose and option and return the index of the option
// (-1 if the input is closed). Empty options ("") are blank separators that
// cannot be selected, the index returned still counts them.
int ChoiceMCQ(const char *options[], int num_choices)
{
    struct termios orig;
    __EnableRawMode(&orig);

    int choice = 0;
    int closed = 0;
    if (num_choices > 0 && !__Selectable(options, 0))
        choice = __NextSelectable(options, num_choices, 0, 1);

    while (1)
    {
        for (int i = 0; i < num_choices; i++)
        {
            if (!__Selectable(options, i))
                printf("\n");
            else if (i == choice)
                printf("> \033[7m%s\033[0m\n", options[i]);
            else
                printf("  %s\n", options[i]);
        }
        fflush(stdout);

        unsigned char c;
        if (__ReadByte(&c, -1) <= 0)
        {
            closed = 1;
            break;
        }

        if (c == 27)
        {
            // Arrow : ESC [ A / ESC [ B (ESC O A / ESC O B in some terminals)
            unsigned char start;
            unsigned char dir;
            if (__ReadByte(&start, CHOICE_ESC_MS) == 1
                && (start == '[' || start == 'O')
                && __ReadByte(&dir, CHOICE_ESC_MS) == 1)
            {
                if (dir == 'A')
                    choice = __NextSelectable(options, num_choices, choice, -1);
                if (dir == 'B')
                    choice = __NextSelectable(options, num_choices, choice, 1);
            }
        }
        else if (c == '\n' || c == '\r')
        {
            break;
        }

        // Moove the cursor
        printf("\033[%uA", num_choices);

        for (int i = 0; i < num_choices; i++)
        {
            // Erase the line
            printf("\033[2K");
        }
    }

    // Moove the cursor
    printf("\033[%uA", num_choices);

    for (int i = 0; i < num_choices; i++)
    {
        // Erase the line
        printf("\033[2K\n");
    }

    // Moove the cursor
    printf("\033[%uA", num_choices);
    fflush(stdout);

    __DisableRawMode(&orig);
    return closed ? -1 : choice;
}

void __EnableRawMode(struct termios *orig)
{
    struct termios raw;
    tcgetattr(STDIN_FILENO, orig);
    raw = *orig;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    choice_raw = raw;
    choice_raw_valid = 1;
}

void __DisableRawMode(struct termios *orig)
{
    choice_raw_valid = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, orig);
    if (system("stty sane") == -1)
        errx(EXIT_FAILURE, "ERROR Impossible to reset the terminal");
}