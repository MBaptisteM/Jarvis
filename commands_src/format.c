#include <string.h>
#include <sys/stat.h>

#include "TPs_handler.h"
#include "format_handler.h"

#define USAGE "Usage: jarvis format [<path> | -p <path>]"

int main(int argc, char *argv[])
{
    const char *path = ".";
    int path_given = 0;

    for (int k = 1; k < argc; k++)
    {
        const char *value = argv[k];

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

    return FormatTree(path);
}