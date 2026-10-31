#include "tidy.h"

// Problems were found without --fix : if some of them can be fixed, ask
// whether to fix them. Returns the number of problems left.
static int __OfferFix(const char *path, int problems)
{
    if (problems <= 0 || TidyLastFixable() == 0 || !isatty(STDIN_FILENO))
        return problems;

    UiQuestion("Fix automatically what can be fixed (include guard names, "
               "#endif comments, final newline) ?");

    const char *options[] = { "No, leave the files as they are",
                              "Yes, fix what can be fixed" };

    if (ChoiceMCQ(options, 2) != 1)
        return problems;

    return TidyTreeFix(path);
}

int main(int argc, char *argv[])
{
    const char *path = ".";
    int path_given = 0;
    int fix = 0;

    for (int k = 1; k < argc; k++)
    {
        const char *value = argv[k];

        // -f, --fix : fix what can be fixed
        if (strcmp(argv[k], "-f") == 0 || strcmp(argv[k], "--fix") == 0)
        {
            fix = 1;
            continue;
        }

        // -p <path>, --path <path>, --path=<path>
        if (strcmp(argv[k], "-p") == 0 || strcmp(argv[k], "--path") == 0)
        {
            if (k + 1 >= argc)
                errx(EXIT_FAILURE, "ERROR The option %s needs a value\n" USAGE,
                     argv[k]);
            value = argv[++k];
        }
        else if (strncmp(argv[k], "--path=", 7) == 0)
            value = argv[k] + 7;
        else if (argv[k][0] == '-' && argv[k][1] != '\0')
            errx(EXIT_FAILURE, "ERROR Unknown option %s\n" USAGE, argv[k]);

        if (path_given)
            errx(EXIT_FAILURE, "ERROR Only one path can be given\n" USAGE);
        if (value[0] == '\0')
            errx(EXIT_FAILURE, "ERROR The path is empty\n" USAGE);

        path = value;
        path_given = 1;
    }

    struct stat info;
    if (stat(path, &info) != 0)
    {
        UiError("%s does not exist.", path);
        return EXIT_FAILURE;
    }
    if (!S_ISDIR(info.st_mode))
    {
        UiError("%s is not a folder.", path);
        return EXIT_FAILURE;
    }

    // Problems left (or the folder could not be browsed) : not zero
    int problems = fix ? TidyTreeFix(path) : __OfferFix(path, TidyTree(path));
    return problems == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}