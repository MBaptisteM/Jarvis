#include "counter.h"

// Has the question already been asked (whatever the answer was) ?
static int __CounterAlreadyAsked(void)
{
    char *value = NULL;
    if (ReadInfo(COUNTER_KEY, &value) != 0)
        return 0;
    free(value);
    return 1;
}

// Is the user connected to GitHub ? (GitHub CLI installed and authenticated)
static int __CounterGithubConnected(void)
{
    return system("gh auth status >/dev/null 2>&1") == 0;
}

// Add a star on the repository of Jarvis with the account of the user
static int __CounterStar(void)
{
    return system("gh api --method PUT /user/starred/" COUNTER_REPO
                  " >/dev/null 2>&1")
        == 0;
}

// Ask once if the user accepts to be counted. Never asks without a terminal
// (scripts, "jarvis fill" clones...), nor when the user is not connected to
// GitHub : the question is then asked another time.
static void CounterAsk(void)
{
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)
        || getenv("JARVIS_FILL") != NULL)
        return;

    if (__CounterAlreadyAsked() || !__CounterGithubConnected())
        return;

    UiQuestion("You are connected to GitHub.\n"
               "Do you accept to be counted as a user of Jarvis ? The number "
               "of users is public on " COUNTER_REPO
               "\nIt helps a lot the staff of Jarvis.\n"
               "This question is asked only once and won't affect Jarvis' behaviour.\n"
               "You can use the command 'jarvis help counter' for more "
               "information on how the counter works.");

    // "No" first : pressing enter without thinking never counts anybody
    const char *options[] = { "Yes, count me", "No, thanks" };
    int choice = ChoiceMCQ(options, 2);
    if (choice < 0)
        return;

    WriteInfo(COUNTER_KEY, choice == 0 ? "accepted" : "declined");

    if (choice != 0)
        return;

    if (__CounterStar())
        UiSuccess("Thank you ! You are now counted as a user of Jarvis.");
    else
        UiWarning("Impossible to add the star with GitHub. You can add it "
                  "yourself on https://github.com/" COUNTER_REPO);
}