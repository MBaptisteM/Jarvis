#include "push.h"

int ReadRootPath(char **value);
int PushRepoStatus(char *repo_path, char *commit_name, char *tag);
int PushRepoRootStatus(char *commit_name, int staging_failed);

static int __StatusToExit(int status)
{
    return status == JARVIS_PUSH_PUSHED || status == JARVIS_PUSH_NOTHING
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}

static const char *__StatusLabel(int status)
{
    switch (status)
    {
    case JARVIS_PUSH_PUSHED:
        return "pushed";
    case JARVIS_PUSH_NOTHING:
        return "nothing to push (already up to date)";
    case JARVIS_PUSH_PARTIAL:
        return "pushed, but incomplete";
    default:
        return "NOT pushed";
    }
}

// Stage, commit and push the root repository. A staging problem is reported
// but does not prevent pushing what could be staged.
static int __PushRootOnly(char *commit_name)
{
    int staging_failed = AddRepoRoot() != EXIT_SUCCESS;
    return PushRepoRootStatus(commit_name, staging_failed);
}

// Same folder ?
static int __SameFolder(const char *a, const char *b)
{
    char *real_a = realpath(a, NULL);
    char *real_b = realpath(b, NULL);
    int same = real_a != NULL && real_b != NULL && strcmp(real_a, real_b) == 0;
    free(real_a);
    free(real_b);
    return same;
}

// Is this folder the root repository itself ? The saved root path may be the
// root repository or its parent folder (<saved path>/EPITA-TPs).
static int __IsRootPath(const char *path)
{
    char *root = NULL;
    if (ReadRootPath(&root) != EXIT_SUCCESS || root == NULL)
        return 0;

    int same = __SameFolder(path, root);
    if (!same)
    {
        size_t size = strlen(root) + strlen(REPO_NAME) + 2;
        char *nested = malloc(size);
        if (nested == NULL)
            err(EXIT_FAILURE, "malloc");
        snprintf(nested, size, "%s/%s", root, REPO_NAME);

        char *git_dir = malloc(size + sizeof("/.git"));
        if (git_dir == NULL)
            err(EXIT_FAILURE, "malloc");
        snprintf(git_dir, size + sizeof("/.git"), "%s/.git", nested);

        // The repository is inside the saved path only if it has a .git
        char *root_git = malloc(strlen(root) + sizeof("/.git") + 1);
        if (root_git == NULL)
            err(EXIT_FAILURE, "malloc");
        sprintf(root_git, "%s/.git", root);
        if (access(root_git, F_OK) != 0 && access(git_dir, F_OK) == 0)
            same = __SameFolder(path, nested);

        free(root_git);
        free(git_dir);
        free(nested);
    }

    free(root);
    return same;
}

// -t does not apply to the root repository alone
static void __WarnTagIgnored(const char *tag)
{
    if (tag != NULL)
        UiWarning("The tag \"%s\" is ignored : the root repository alone is "
                  "pushed without a tag.",
                  tag);
}

// Value of an option : "-n value" / "--name value" (the value is the next
// argument) or "--name=value". Returns NULL if argv[k] is not this option.
// *k is moved onto the value when it is the next argument.
static char *__OptionValue(int argc, char *argv[], int *k, char short_name,
                           const char *long_name)
{
    const char *arg = argv[*k];
    size_t long_length = strlen(long_name);

    if (arg[0] != '-')
        return NULL;

    if (short_name != '\0' && arg[1] == short_name && arg[2] == '\0')
    {
        if (*k + 1 >= argc)
            errx(EXIT_FAILURE, "ERROR The option %s needs a value", arg);
        return argv[++(*k)];
    }

    if (arg[1] == '-' && strncmp(arg + 2, long_name, long_length) == 0)
    {
        if (arg[2 + long_length] == '=')
            return (char *)arg + 3 + long_length;
        if (arg[2 + long_length] == '\0')
        {
            if (*k + 1 >= argc)
                errx(EXIT_FAILURE, "ERROR The option %s needs a value", arg);
            return argv[++(*k)];
        }
    }

    return NULL;
}

// Set the repository to push from a path, "root" or a "current" keyword
static void __SetPath(char **path, char *value, char *current_folder)
{
    if (strcasecmp(value, "root") == 0)
        *path = value;
    else if (IsCurrentKeyword(value))
    {
        if (ReadInfo("current", path))
            errx(EXIT_FAILURE,
                 "ERROR trying to push the current repository but no "
                 "current repository found");
        *current_folder = 1;
    }
    else
        *path = value;
}

int main(int argc, char *argv[])
{
    char *path = NULL;

    char *name_commit = NULL;

    char *tag = NULL;

    char current_folder = 0;

    int k = 1;
    while (argc > k)
    {
        char *option_value;

        // Options : -n/--name <name>, -t/--tag <name>, -p/--path <path>
        if ((option_value = __OptionValue(argc, argv, &k, 'n', "name")) != NULL)
        {
            // --name is the same as --name
            if (option_value[0] == '\0')
                errx(EXIT_FAILURE, "ERROR The commit name is empty");
            name_commit = option_value;
        }
        else if ((option_value = __OptionValue(argc, argv, &k, 't', "tag"))
                 != NULL)
        {
            if (option_value[0] == '\0')
                errx(EXIT_FAILURE, "ERROR The tag name is empty");
            tag = option_value;
        }
        else if ((option_value = __OptionValue(argc, argv, &k, 'p', "path"))
                 != NULL)
        {
            if (option_value[0] == '\0')
                errx(EXIT_FAILURE, "ERROR The path is empty");
            __SetPath(&path, option_value, &current_folder);
        }
        // Case root
        else if (strcasecmp(argv[k], "root") == 0)
        {
            path = argv[k];
        }
        // Case current
        else if (IsCurrentKeyword(argv[k]))
        {
            if (ReadInfo("current", &path))
                errx(EXIT_FAILURE,
                     "ERROR trying to push the current repository but no "
                     "current repository found");
            current_folder = 1;
        }
        else if (argv[k][0] == '-' && argv[k][1] != '\0')
        {
            errx(EXIT_FAILURE,
                 "ERROR Unknown option %s (use -n/--name, -t/--tag, "
                 "-p/--path)",
                 argv[k]);
        }
        else
        {
            // Case tag
            size_t i = 0;
            while (COMMAND_TAG[i] != 0 && tolower(argv[k][i]) == COMMAND_TAG[i])
                i++;

            if (COMMAND_TAG[i] == 0)
                tag = &(argv[k][i]);

            else
            {
                // Case name
                while (COMMAND_NAME[i] != 0
                       && tolower(argv[k][i]) == COMMAND_NAME[i])
                    i++;

                if (COMMAND_NAME[i] == 0)
                    name_commit = &(argv[k][i]);

                // Case path
                else
                    path = argv[k];
            }
        }

        k++;
    }

    // Push only root if path == root
    if (path != NULL && strcasecmp(path, "root") == 0)
    {
        __WarnTagIgnored(tag);
        return __StatusToExit(
            __PushRootOnly(name_commit != NULL ? name_commit : "Manual push"));
    }

    // Get the path
    char path_allocated = 0;
    if (path == NULL)
    {
        path = getcwd(NULL, 0);
        if (path == NULL)
            err(EXIT_FAILURE, "getcwd");
        path_allocated = 1;
    }
    else if (current_folder)
        path_allocated = 1;

    // Get the repo name
    char repo_name[SIZE_OF_STRING];
    int i = 0;
    char *c = path;
    char *anc = NULL;
    while (*c != 0)
    {
        if (*c != '/')
            repo_name[i++] = *c;
        else
            i = 0;
        anc = c;
        c++;
    }
    repo_name[i] = 0;

    // Remove the last \n
    if (anc != NULL && *anc == '\n')
    {
        *anc = 0;
        repo_name[i - 1] = 0;
    }

    //  Get the path if folder moved
    if (current_folder && access(path, F_OK) != 0)
    {
        if (path_allocated)
            free(path);

        char *parent_path = FindFileBFS(repo_name);
        if (parent_path == NULL)
            errx(EXIT_FAILURE,
                 "ERROR Impossible to find the current repository %s locally",
                 repo_name);

        // FindFileBFS returns the parent folder of what was found
        size_t found_size = strlen(parent_path) + strlen(repo_name) + 2;
        path = malloc(found_size);
        if (path == NULL)
            err(EXIT_FAILURE, "malloc");
        snprintf(path, found_size, "%s/%s", parent_path, repo_name);
        free(parent_path);
        path_allocated = 1;
    }

    // Run from the root repository itself : it is pushed on its own (trying to
    // "git add" it as a normal repository fails on the nested repositories)
    if (__IsRootPath(path))
    {
        __WarnTagIgnored(tag);
        int status =
            __PushRootOnly(name_commit != NULL ? name_commit : "Manual push");
        if (path_allocated)
            free(path);
        return __StatusToExit(status);
    }

    // Get the tag and the name_commit
    char name_commit_allocated = 0;
    if (tag != NULL)
    {
        __GetTagID(&tag, path);
        if (name_commit == NULL)
            name_commit = tag;
    }
    else if (name_commit == NULL)
    {
        name_commit_allocated = 1;
        __GetNameCommit(&name_commit, path);
    }

    // Get the name comit for the root
    char name_commit_root[SIZE_OF_STRING + strlen(name_commit)];
    snprintf(name_commit_root, sizeof(name_commit_root), "%s | %s", name_commit,
             repo_name);

    // 0. Automatic "jarvis format" (off by default, see 'jarvis custom'). A
    // problem is reported by FormatTree but never prevents the push.
    if (IsAutoFormatEnabled())
    {
        UiInfo("Automatic format before the push...");
        FormatTree(path);
    }

    // 1. The repository itself. One after the other (not at the same time) :
    // the root repository records the new commit of the repository, and
    // hiding the .git of an empty nested repository while it is being pushed
    // would break it.
    int repo_status = PushRepoStatus(path, name_commit, tag);

    // 2. The root repository, even if the first step failed
    int root_status = __PushRootOnly(name_commit_root);

    if (path_allocated)
        free(path);
    if (tag != NULL)
        free(tag);
    if (name_commit_allocated)
        free(name_commit);

    int result = __StatusToExit(repo_status) | __StatusToExit(root_status);
    if (result != EXIT_SUCCESS)
        UiWarning("Push finished with problems :\nRepository : %s\nRoot "
                  "repository : %s",
                  __StatusLabel(repo_status), __StatusLabel(root_status));

    return result;
}

void __GetNameCommit(char **name_commit, char *path)
{
    *name_commit = "regular-push-";

    __GetId(name_commit, path);
}

void __GetTagID(char **tag, char *path)
{
    size_t tag_length = strlen(*tag);
    int needs_separator = tag_length > 0 && (*tag)[tag_length - 1] != '-';
    char *prefix = malloc(tag_length + (size_t)needs_separator + 1);
    if (prefix == NULL)
        err(EXIT_FAILURE, "malloc");

    memcpy(prefix, *tag, tag_length);
    if (needs_separator)
        prefix[tag_length++] = '-';
    prefix[tag_length] = '\0';

    char *entry = prefix;
    __GetId(&entry, path);
    free(prefix);
    *tag = entry;
}

// Generate the name of the commit
void __GetId(char **entry, char *path)
{
    // Get the stubborn
    char *stubborn = *entry;

    // Get the id
    char id_added = 0;

    // Check if there is an initial commit (in the repository being pushed)
    char command_head[SIZE_OF_STRING + strlen(path)];
    snprintf(command_head, sizeof(command_head),
             "git -C \"%s\" rev-parse --quiet --verify HEAD >/dev/null 2>&1",
             path);
    if (system(command_head) == 0)
    {
        char command_names_push[SIZE_OF_STRING + strlen(path)];
        snprintf(command_names_push, SIZE_OF_STRING + strlen(path),
                 "git -C \"%s\" --no-pager log --format=%s", path, "%s");

        FILE *fp = popen(command_names_push, "r");
        char line[1024];

        if (!fp)
            errx(EXIT_FAILURE, "Impossible to read the console");

        while (fgets(line, sizeof(line), fp) != NULL)
        {
            // Check the stubborn
            int i = 0;
            while (line[i] != 0 && (*entry)[i] != 0
                   && tolower((*entry)[i]) == tolower(line[i]))
                i++;

            if ((*entry)[i] == 0 && line[i] != 0)
            {
                // Add the id
                int n = (int)strtol(&(line[i]), NULL, 10) + 1;

                char *new_entry = malloc(SIZE_OF_STRING + strlen(stubborn));
                if (new_entry == NULL)
                    err(EXIT_FAILURE, "malloc");

                snprintf(new_entry, SIZE_OF_STRING + strlen(stubborn), "%s%d",
                         stubborn, n);
                *entry = new_entry;

                id_added = 1;
                break;
            }
        }

        pclose(fp);
    }

    if (!id_added)
    {
        char *new_entry = malloc(SIZE_OF_STRING + strlen(stubborn));
        if (new_entry == NULL)
            err(EXIT_FAILURE, "malloc");
        snprintf(new_entry, SIZE_OF_STRING + strlen(stubborn), "%s0", stubborn);
        *entry = new_entry;
    }
}