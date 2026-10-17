#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "TPs_handler.h"
#include "ui.h"

static int __IsGitRoot(void)
{
    FILE *pipe = popen("git rev-parse --show-prefix 2>/dev/null", "r");
    if (pipe == NULL)
        return 0;

    char prefix[4096];
    int has_output = fgets(prefix, sizeof(prefix), pipe) != NULL;
    int result = pclose(pipe);
    if (!has_output || result != 0)
        return 0;

    prefix[strcspn(prefix, "\r\n")] = '\0';
    return prefix[0] == '\0';
}

static int __GetHeadHash(char *hash, size_t hash_size)
{
    FILE *pipe = popen("git rev-parse --verify HEAD 2>/dev/null", "r");
    if (pipe == NULL)
        return EXIT_FAILURE;

    int has_output = fgets(hash, hash_size, pipe) != NULL;
    int result = pclose(pipe);
    if (!has_output || result != 0)
        return EXIT_FAILURE;

    hash[strcspn(hash, "\r\n")] = '\0';
    return hash[0] == '\0' ? EXIT_FAILURE : EXIT_SUCCESS;
}

static void __PrintRestoreCommand(const char *root_path, const char *hash)
{
    // Path between single quotes, every ' becomes '\''
    char *quoted = malloc(4 * strlen(root_path) + 3);
    if (quoted == NULL)
        err(EXIT_FAILURE, "malloc");

    size_t k = 0;
    quoted[k++] = '\'';
    for (const char *c = root_path; *c != '\0'; c++)
    {
        if (*c == '\'')
        {
            memcpy(quoted + k, "'\\''", 4);
            k += 4;
        }
        else
            quoted[k++] = *c;
    }
    quoted[k++] = '\'';
    quoted[k] = '\0';

    UiPanel(stdout, UI_INFO, "Restore",
            "To restore the root repository before erase, run:\n"
            "  git -C %s restore --source=%s --staged --worktree .\n"
            "Then run 'jarvis push root' to publish the restored files.",
            quoted, hash);
    free(quoted);
}

int main(int argc, char *argv[])
{
    if (argc > 1 && argv[1] != NULL)
        errx(EXIT_FAILURE, "Usage: jarvis erase");

    char *root_path = GetTPsPath();
    if (root_path == NULL)
        errx(EXIT_FAILURE, "ERROR Impossible to find the root repository");

    if (chdir(root_path) != 0)
        err(EXIT_FAILURE, "chdir(%s)", root_path);
    if (!__IsGitRoot())
        errx(EXIT_FAILURE,
             "ERROR The Jarvis root path is not the root of a Git repository: "
             "%s",
             root_path);

    UiWarning("This will push a backup, delete all content in the root "
              "repository except .git, and push the cleaned repository:\n%s\n\n"
              "The backup push replaces remote-only commits. Ignored files are "
              "not included in the Git backup.",
              root_path);
    printf("Continue? [y/N] ");
    fflush(stdout);

    char confirmation[16];
    if (fgets(confirmation, sizeof(confirmation), stdin) == NULL
        || (confirmation[0] != 'y' && confirmation[0] != 'Y'))
    {
        UiInfo("Erase cancelled.");
        free(root_path);
        return EXIT_SUCCESS;
    }

    if (AddRepoRoot() != EXIT_SUCCESS
        || PushRepoRootForce("Backup before erase") != EXIT_SUCCESS)
    {
        free(root_path);
        return EXIT_FAILURE;
    }

    char backup_hash[128];
    if (__GetHeadHash(backup_hash, sizeof(backup_hash)) != EXIT_SUCCESS)
        errx(EXIT_FAILURE,
             "ERROR Could not identify the root backup commit; nothing was "
             "deleted");

    if (CleanFolder(".") != EXIT_SUCCESS)
    {
        __PrintRestoreCommand(root_path, backup_hash);
        free(root_path);
        return EXIT_FAILURE;
    }

    int result = AddRepoRoot();
    if (result == EXIT_SUCCESS)
        result = PushRepoRoot("Erase root repository");

    __PrintRestoreCommand(root_path, backup_hash);
    free(root_path);
    return result == EXIT_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}