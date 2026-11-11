#include "choice.h"

// State wanted while a question is displayed (raw mode)
static struct termios choice_raw;
static int choice_raw_valid = 0;

// Width of the terminal when the menu was last drawn (0 : unknown, the options
// are then never cut)
static int choice_columns = 0;

// Returned by __ReadByte when the window was resized
#define CHOICE_RESIZED 2

// Has the window been resized since the last call ? The width is compared at
// each tick, so it works wherever the OS can give the size of the terminal,
// with no signal handler to interrupt the calls.
static int __WindowResized(void)
{
    int now = UiTerminalColumns(stdout);
    if (now == choice_columns)
        return 0;

    choice_columns = now;
    return 1;
}

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
// 1 : a byte was read | 0 : timeout | -1 : end of input | CHOICE_RESIZED : the
// window was resized (only when waiting without timeout)
static int __ReadByte(unsigned char *c, int timeout_ms)
{
    int waited = 0;
    while (1)
    {
        __KeepRawMode();
        if (timeout_ms < 0 && __WindowResized())
            return CHOICE_RESIZED;

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

// An option is displayed on one terminal row : when the window is too narrow it
// is cut and ends with an ellipsis, so that the cursor moves used to redraw the
// menu stay right
static void __PrintOption(const char *text)
{
    size_t length = strlen(text);
    int cut = 0;

    if (choice_columns > 0)
    {
        // "> " before the text, and the last column is kept free
        int room = choice_columns > 11 ? choice_columns - 3 : 8;
        size_t fit = UiFitBytes(text, room);
        cut = fit < length;
        length = cut ? UiFitBytes(text, room - 1) : length;
    }

    printf("%.*s%s", (int)length, text, cut ? "\xE2\x80\xA6" : "");
}

static void __DrawMenu(const char *options[], int num_choices, int choice)
{
    for (int i = 0; i < num_choices; i++)
    {
        if (!__Selectable(options, i))
        {
            printf("\n");
            continue;
        }

        printf(i == choice ? "> \033[7m" : "  ");
        __PrintOption(options[i]);
        printf(i == choice ? "\033[0m\n" : "\n");
    }
    fflush(stdout);
}

// Back to the first row of the menu, and clear everything under it
static void __EraseMenu(int num_choices)
{
    if (num_choices > 0)
        printf("\033[%dA", num_choices);
    printf("\033[J");
    fflush(stdout);
}

// Let the user choose and option and return the index of the option
// (-1 if the input is closed). Empty options ("") are blank separators that
// cannot be selected, the index returned still counts them. The menu is drawn
// again when the window is resized.
int ChoiceMCQ(const char *options[], int num_choices)
{
    struct termios orig;
    __EnableRawMode(&orig);
    choice_columns = UiTerminalColumns(stdout);

    int choice = 0;
    int closed = 0;
    if (num_choices > 0 && !__Selectable(options, 0))
        choice = __NextSelectable(options, num_choices, 0, 1);

    while (1)
    {
        __DrawMenu(options, num_choices, choice);

        unsigned char c;
        int key = __ReadByte(&c, -1);
        if (key == CHOICE_RESIZED)
        {
            __EraseMenu(num_choices); // drawn again at the new width
            continue;
        }
        if (key <= 0)
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

        __EraseMenu(num_choices);
    }

    __EraseMenu(num_choices);
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
