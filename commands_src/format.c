#include "format.h"

#define FORMAT_BATCH_SIZE 64

// Extensions handled by clang-format
static const char *EXTENSIONS[] = {
    ".cpp",   ".cc",     ".cxx",     ".c++",        ".cp",        ".hpp",
    ".hh",    ".hxx",    ".h++",     ".inline.hpp", ".inl",       ".tpp",
    ".txx",   ".c",      ".h",       ".cu",         ".cuh",       ".ixx",
    ".cppm",  ".mxx",    ".ccm",     ".js",         ".mjs",       ".cjs",
    ".ts",    ".tsx",    ".cs",      ".java",       ".json",      ".ipynb",
    ".m",     ".mm",     ".proto",   ".protodevel", ".textproto", ".textpb",
    ".txtpb", ".pb.txt", ".asciipb", ".td",         ".v",         ".vh",
    ".sv",    ".svh"
};

// Folders never formatted
static const char *IGNORED_FOLDERS[] = { ".git", ".git.jarvis-tmp",
                                         "node_modules" };

static char **files = NULL;
static size_t files_count = 0;
static size_t files_capacity = 0;

static int __HasExtension(const char *name)
{
    size_t length = strlen(name);
    for (size_t i = 0; i < sizeof(EXTENSIONS) / sizeof(EXTENSIONS[0]); i++)
    {
        size_t extension_length = strlen(EXTENSIONS[i]);
        // The name must be more than the extension (".c" alone is not a file
        // to format)
        if (length > extension_length
            && strcmp(name + length - extension_length, EXTENSIONS[i]) == 0)
            return 1;
    }
    return 0;
}

static int __IsIgnoredFolder(const char *name)
{
    for (size_t i = 0; i < sizeof(IGNORED_FOLDERS) / sizeof(IGNORED_FOLDERS[0]);
         i++)
        if (strcmp(name, IGNORED_FOLDERS[i]) == 0)
            return 1;
    return 0;
}

static int __Visit(const char *path, const struct stat *info, int type,
                   struct FTW *position)
{
    (void)info;
    const char *name = path + position->base;

    if (type == FTW_D || type == FTW_DNR)
        return position->level > 0 && __IsIgnoredFolder(name) ? FTW_SKIP_SUBTREE
                                                              : 0;

    if (type != FTW_F || !__HasExtension(name))
        return 0;

    if (files_count == files_capacity)
    {
        size_t capacity = files_capacity == 0 ? 64 : files_capacity * 2;
        char **grown = realloc(files, capacity * sizeof(char *));
        if (grown == NULL)
            err(EXIT_FAILURE, "realloc");
        files = grown;
        files_capacity = capacity;
    }

    files[files_count] = strdup(path);
    if (files[files_count] == NULL)
        err(EXIT_FAILURE, "strdup");
    files_count++;
    return 0;
}

// Run clang-format -i on these files (no shell, so any file name is safe).
// Returns 0 when it succeeded.
static int __RunClangFormat(char **batch, size_t count)
{
    char **argv = malloc((count + 3) * sizeof(char *));
    if (argv == NULL)
        err(EXIT_FAILURE, "malloc");

    argv[0] = "clang-format";
    argv[1] = "-i";
    for (size_t i = 0; i < count; i++)
        argv[i + 2] = batch[i];
    argv[count + 2] = NULL;

    int status = -1;
    pid_t pid = fork();
    if (pid == 0)
    {
        execvp("clang-format", argv);
        _exit(127);
    }
    free(argv);

    if (pid < 0 || waitpid(pid, &status, 0) < 0)
        return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

// Is clang-format installed ?
static int __ClangFormatExists(void)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        execlp("clang-format", "clang-format", "--version", (char *)NULL);
        _exit(127);
    }

    int status = 0;
    if (pid < 0 || waitpid(pid, &status, 0) < 0)
        return 0;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int main(int argc, char *argv[])
{
    if (argc > 1 && argv[1] != NULL)
        errx(EXIT_FAILURE, "Usage: jarvis format");

    if (!__ClangFormatExists())
    {
        UiError("clang-format was not found. Install it (ex : sudo apt install "
                "clang-format) and try again.");
        return EXIT_FAILURE;
    }

    if (nftw(".", __Visit, 32, FTW_PHYS | FTW_ACTIONRETVAL) != 0)
    {
        UiError("Impossible to browse the current folder.");
        return EXIT_FAILURE;
    }

    if (files_count == 0)
    {
        UiInfo("No file to format in the current folder.");
        return EXIT_SUCCESS;
    }

    // Formatting is done by batches ; if a batch fails, its files are
    // retried one by one to know exactly which ones failed.
    size_t formatted = 0;
    size_t failed = 0;
    for (size_t i = 0; i < files_count; i += FORMAT_BATCH_SIZE)
    {
        size_t count = files_count - i < FORMAT_BATCH_SIZE ? files_count - i
                                                           : FORMAT_BATCH_SIZE;

        if (__RunClangFormat(files + i, count) == 0)
        {
            formatted += count;
            continue;
        }

        for (size_t j = i; j < i + count; j++)
        {
            if (__RunClangFormat(files + j, 1) == 0)
                formatted++;
            else
            {
                failed++;
                UiWarning("Impossible to format %s", files[j]);
            }
        }
    }

    for (size_t i = 0; i < files_count; i++)
        free(files[i]);
    free(files);

    if (failed > 0)
    {
        UiWarning("%zu file(s) formatted, %zu file(s) NOT formatted (see "
                  "above).",
                  formatted, failed);
        return EXIT_FAILURE;
    }

    UiSuccess("%zu file(s) formatted.", formatted);
    return EXIT_SUCCESS;
}