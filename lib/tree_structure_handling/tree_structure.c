#include "tree_structure.h"

#include <sys/wait.h>

int IsEmpty(char *path)
{
    DIR *dir;
    struct dirent *entry;

    dir = opendir(path);

    if (dir == NULL)
    {
        errx(-1, "ERROR Impossible to open the folder %s", path);
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0
            && strcmp(entry->d_name, ".git") != 0)
        {
            closedir(dir);
            return 0;
        }
    }

    closedir(dir);
    return 1;
}

int CleanFolder(char *folder_path)
{
    DIR *dir;
    struct dirent *entry;

    dir = opendir(folder_path);

    if (dir == NULL)
    {
        errx(-1, "ERROR Impossible to open the folder %s", folder_path);
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0
            && strcmp(entry->d_name, ".git") != 0)
        {
            char to_remove_path[1024];
            snprintf(to_remove_path, sizeof(to_remove_path), "%s/%s",
                     folder_path, entry->d_name);
            pid_t child = fork();
            if (child < 0)
                err(EXIT_FAILURE, "fork");
            if (child == 0)
            {
                execlp("rm", "rm", "-rf", "--", to_remove_path, (char *)NULL);
                _exit(EXIT_FAILURE);
            }
            int status;
            if (waitpid(child, &status, 0) < 0 || !WIFEXITED(status)
                || WEXITSTATUS(status) != 0)
                errx(EXIT_FAILURE, "ERROR Impossible to remove %s/%s",
                     folder_path, entry->d_name);
        }
    }

    UiSuccess("Content of %s deleted successfully.", folder_path);

    closedir(dir);

    return EXIT_SUCCESS;
}

// Try to find the mark file, return the path if find, NULL else
char *FindMarkedFileBFS()
{
    return FindFileBFS(MARKED_FILE_NAME);
}

// Try to find the file <file_to_find>, return the path if find, NULL else
char *FindFileBFS(const char *file_to_find)
{
    const char *home = getenv("HOME");

    if (!home)
        return NULL;

    Queue q = { 0 };

    Enqueue(&q, home);

    while (q.front)
    {
        char *current_dir = Dequeue(&q);

        if (!current_dir)
            continue;

        DIR *dir = opendir(current_dir);

        if (!dir)
        {
            free(current_dir);
            continue;
        }

        struct dirent *entry;

        while ((entry = readdir(dir)) != NULL)
        {
            // Ignore . and ..
            if (strcmp(entry->d_name, ".") == 0
                || strcmp(entry->d_name, "..") == 0)
                continue;

            char full_path[MAX_PATH_SIZE];

            snprintf(full_path, sizeof(full_path), "%s/%s", current_dir,
                     entry->d_name);

            // File found
            if (strcmp(entry->d_name, file_to_find) == 0)
            {
                closedir(dir);

                char *result = strdup(current_dir);

                free(current_dir);

                // Clean the queue
                while (q.front)
                {
                    char *tmp = Dequeue(&q);
                    free(tmp);
                }

                return result;
            }

            // Add subfolders
            if (IsDirectory(full_path))
            {
                Enqueue(&q, full_path);
            }
        }

        closedir(dir);
        free(current_dir);
    }

    return NULL;
}


// Does <parent>/<name>/.git exist ?
static int __IsRepoNamed(const char *parent, const char *name)
{
    char path[MAX_PATH_SIZE];
    snprintf(path, sizeof(path), "%s/%s/.git", parent, name);
    struct stat st;
    return lstat(path, &st) == 0;
}

// Look for <name> (a git repository) exactly "depth" levels under "dir".
// Returns the parent folder of the match (malloc'd), NULL otherwise.
static char *__SearchAtDepth(const char *dir, const char *name, int depth)
{
    DIR *d = opendir(dir);
    if (d == NULL)
        return NULL;

    char *result = NULL;
    struct dirent *entry;
    while (result == NULL && (entry = readdir(d)) != NULL)
    {
        // ".", "..", .git, .cache, .local ...
        if (entry->d_name[0] == '.')
            continue;

        char path[MAX_PATH_SIZE];
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);

        struct stat st;
        if (lstat(path, &st) != 0 || !S_ISDIR(st.st_mode))
            continue;

        if (depth == 1)
        {
            if (strcmp(entry->d_name, name) == 0 && __IsRepoNamed(dir, name))
            {
                result = strdup(dir);
                if (result == NULL)
                    err(EXIT_FAILURE, "strdup");
            }
        }
        else
            result = __SearchAtDepth(path, name, depth - 1);
    }

    closedir(d);
    return result;
}

// Fast search of the root repository : from the current folder, then from
// $HOME, at most "max_depth" layers deep (closest match first). Returns the
// parent folder of the repository (same contract as FindFileBFS), NULL if not
// found.
char *FindRootRepoNear(const char *name, int max_depth)
{
    char *cwd = getcwd(NULL, 0);
    const char *home = getenv("HOME");

    const char *starts[2] = { cwd, home };
    for (int s = 0; s < 2; s++)
    {
        if (starts[s] == NULL || starts[s][0] == '\0')
            continue;
        if (s == 1 && cwd != NULL && strcmp(cwd, home) == 0)
            continue; // already searched

        for (int depth = 1; depth <= max_depth; depth++)
        {
            char *found = __SearchAtDepth(starts[s], name, depth);
            if (found != NULL)
            {
                free(cwd);
                return found;
            }
        }
    }

    free(cwd);
    return NULL;
}

// Check if the given element is a directory
int IsDirectory(const char *path)
{
    struct stat st;

    if (stat(path, &st) != 0)
        return 0;

    return S_ISDIR(st.st_mode);
}

// Queue Definition for bfs
void Enqueue(Queue *q, const char *path)
{
    QueueNode *node = malloc(sizeof(QueueNode));
    if (!node)
        err(EXIT_FAILURE, "malloc");

    strncpy(node->path, path, MAX_PATH_SIZE - 1);
    node->path[MAX_PATH_SIZE - 1] = '\0';
    node->next = NULL;

    if (q->rear)
        q->rear->next = node;
    else
        q->front = node;

    q->rear = node;
}

char *Dequeue(Queue *q)
{
    if (!q->front)
        return NULL;

    QueueNode *tmp = q->front;

    char *path = strdup(tmp->path);
    if (path == NULL)
        err(EXIT_FAILURE, "strdup");

    q->front = tmp->next;

    if (q->rear == tmp)
        q->rear = NULL;

    free(tmp);

    return path;
}