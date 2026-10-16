#include "current_handling.h"

#include <ctype.h>

typedef struct
{
    char *url;
    char ***repos_url;
    int *ind_repos;
    pthread_mutex_t *lock;
    int id;
} ThreadArg;

void *ThreadExtractPages(void *arg)
{
    ThreadArg *t_arg = (ThreadArg *)arg;

    char file_name[SIZE_OF_STRING];
    snprintf(file_name, sizeof(file_name), "%s%d%s", "1Layer", t_arg->id,
             ".html");
    PageDownload(t_arg->url, file_name);

    char file_path[strlen(file_name) + SIZE_OF_STRING];
    char *dotjarvis_path = NULL;
    if (GetDotJarvisPath(&dotjarvis_path))
        errx(EXIT_FAILURE, "ERROR Impossible to find the .jarvis file");
    snprintf(file_path, sizeof(file_path), "%s/%s", dotjarvis_path, file_name);
    free(dotjarvis_path);

    pthread_mutex_lock(t_arg->lock);

    int size_projects;
    char **list_projects = ExtractProjects(file_path, &size_projects);
    for (int i = 0; i < size_projects; i++)
    {
        (*t_arg->repos_url)[(*t_arg->ind_repos)++] = list_projects[i];
    }

    free(list_projects);

    pthread_mutex_unlock(t_arg->lock);

    free(t_arg);
    return NULL;
}

void *ThreadExtractURL(void *arg)
{
    ThreadArg *t_arg = (ThreadArg *)arg;

    char file_name[SIZE_OF_STRING];
    snprintf(file_name, sizeof(file_name), "%s%d%s", "2Layer", t_arg->id,
             ".html");
    PageDownload(t_arg->url, file_name);

    char file_path[strlen(file_name) + SIZE_OF_STRING];
    char *dotjarvis_path = NULL;
    if (GetDotJarvisPath(&dotjarvis_path))
        errx(EXIT_FAILURE, "ERROR Impossible to find the .jarvis file");
    snprintf(file_path, sizeof(file_path), "%s/%s", dotjarvis_path, file_name);
    free(dotjarvis_path);

    pthread_mutex_lock(t_arg->lock);

    int size_projects;
    char **list_projects = ExtractProjects(file_path, &size_projects);
    for (int i = 0; i < size_projects; i++)
    {
        (*t_arg->repos_url)[(*t_arg->ind_repos)++] = list_projects[i];
    }
    free(list_projects);

    pthread_mutex_unlock(t_arg->lock);

    free(t_arg);
    return NULL;
}

char *GetCurrentRepo()
{
    int ind_urls = 0;
    char *projects_url[256];

    projects_url[ind_urls++] = strdup("");
    if (projects_url[0] == NULL)
        err(EXIT_FAILURE, "strdup");

    pthread_t threads[256];
    pthread_mutex_t lock;
    pthread_mutex_init(&lock, NULL);

    int ind_pages = 0;
    char **pages_url = calloc(256, sizeof(char *));
    if (pages_url == NULL)
        err(EXIT_FAILURE, "calloc");

    for (int i = 0; i < ind_urls; i++)
    {
        ThreadArg *arg = malloc(sizeof(ThreadArg));
        if (arg == NULL)
            err(EXIT_FAILURE, "malloc failed");

        arg->url = projects_url[i];
        arg->repos_url = &pages_url;
        arg->ind_repos = &ind_pages;
        arg->lock = &lock;
        arg->id = i;

        if (pthread_create(&(threads[i]), NULL, ThreadExtractPages, arg) != 0)
        {
            free(arg);
            errx(EXIT_FAILURE, "ERROR while creating the thread %d", i);
        }
    }

    for (int i = 0; i < ind_urls; i++)
    {
        if (pthread_join(threads[i], NULL) != 0)
        {
            errx(EXIT_FAILURE,
                 "ERROR Impossible to find the current repository (try to "
                 "retry)");
        }
    }

    if (ind_pages == 0)
        errx(EXIT_FAILURE, "ERROR No project page found on the home page");

    int ind_repos = 0;
    char **repos_url = calloc(512, sizeof(char *));
    if (repos_url == NULL)
        err(EXIT_FAILURE, "calloc");

    for (int i = 0; i < ind_pages; i++)
    {
        ThreadArg *arg = malloc(sizeof(ThreadArg));
        if (arg == NULL)
            err(EXIT_FAILURE, "malloc failed");

        arg->url = pages_url[i];
        arg->repos_url = &repos_url;
        arg->ind_repos = &ind_repos;
        arg->lock = &lock;
        arg->id = i;

        if (pthread_create(&threads[i], NULL, ThreadExtractURL, arg) != 0)
        {
            free(arg);
            errx(EXIT_FAILURE, "ERROR while creating the thread %d", i);
        }
    }

    for (int i = 0; i < ind_pages; i++)
    {
        if (pthread_join(threads[i], NULL) != 0)
        {
            errx(EXIT_FAILURE,
                 "ERROR Impossible to find the current repository (try to "
                 "retry)");
        }
    }

    for (int i = 0; i < ind_pages; i++)
        free(pages_url[i]);
    free(pages_url);
    free(projects_url[0]);

    pthread_mutex_destroy(&lock);

    if (ind_repos <= 0)
        errx(EXIT_FAILURE, "ERROR No repository found");

    int final_ind = 0;
    printf("\033[1mWhich repository do you want to clone ?\033[0m\n\n");

    char **repos_mcq = calloc(ind_repos, sizeof(char *));
    if (repos_mcq == NULL)
        err(EXIT_FAILURE, "calloc");
    for (int i = 0; i < ind_repos; i++)
    {
        const char *last_slash = strrchr(repos_url[i], '/');

        if (last_slash == NULL)
        {
            repos_mcq[i] = strdup(repos_url[i]);
            if (repos_mcq[i] == NULL)
                err(EXIT_FAILURE, "strdup");
        }
        else
        {
            size_t length = last_slash - repos_url[i];
            char *one_mcq = malloc(length + 1);
            if (one_mcq == NULL)
                err(EXIT_FAILURE, "malloc");
            strncpy(one_mcq, repos_url[i], length);
            one_mcq[length] = '\0';
            repos_mcq[i] = one_mcq;
        }
    }
    final_ind = ChoiceMCQ((const char **)repos_mcq, ind_repos);

    for (int i = 0; i < ind_repos; i++)
        free(repos_mcq[i]);
    free(repos_mcq);

    if (final_ind < 0)
        errx(EXIT_FAILURE,
             "ERROR Impossible to get the value of the repository's url");

    char *final_url = malloc(SIZE_OF_STRING);
    if (final_url == NULL)
        err(EXIT_FAILURE, "malloc failed");

    if (repos_url[final_ind] == NULL)
        errx(EXIT_FAILURE,
             "ERROR impossible to get the value after the choice");

    snprintf(final_url, SIZE_OF_STRING, "%s", repos_url[final_ind]);

    for (int i = 0; i < ind_repos; i++)
        free(repos_url[i]);
    free(repos_url);

    char *repo_url = ExtractRepo(final_url);
    free(final_url);
    return repo_url;
}

int PageDownload(char *url, char *file_name)
{
    int page_result = DowloadPage(url, file_name);

    if (page_result)
    {
        page_result = DowloadPage(url, file_name);
    }

    if (page_result)
        errx(EXIT_FAILURE,
             "ERROR Impossible to get the home page (failed twice). [Retry or "
             "Auth again]");

    printf("Downloaded with %s%s in %s\n", URL, url, file_name);
    return EXIT_SUCCESS;
}

char **ExtractProjects(char *file_path, int *size)
{
    FILE *file = fopen(file_path, "r");

    if (!file)
    {
        errx(EXIT_FAILURE, "ERROR Impossible to open the file %s", file_path);
    }

    char buffer[1024 + 1];

    *size = 0;
    char **projects_pages = calloc(512, sizeof(char *));
    if (projects_pages == NULL)
        err(EXIT_FAILURE, "calloc");

    while (fgets(buffer, 1024, file))
    {
        if (strstr(buffer, "Finished"))
            break;

        char *c = strstr(buffer, "project");

        while (c)
        {
            char before = (c == buffer) ? ' ' : *(c - 1);
            char after = *(c + 7);

            if ((before == ' ' || before == '"' || before == '\'')
                && (after == ' ' || after == '"' || after == '\''))
            {
                char *href = strstr(buffer, "href");

                if (href)
                {
                    href += 6;

                    char *page = malloc(SIZE_OF_STRING);
                    if (page == NULL)
                    {
                        fclose(file);
                        for (int i = 0; i < *size; i++)
                            free(projects_pages[i]);
                        free(projects_pages);
                        err(EXIT_FAILURE, "malloc");
                    }
                    int i = 0;

                    while (*href != '\"' && *href != 0
                           && i < SIZE_OF_STRING - 1)
                    {
                        page[i++] = *(href++);
                    }
                    page[i] = 0;

                    projects_pages[(*size)++] = page;
                }
                break;
            }

            c = strstr(c + 1, "project");
        }
    }

    fclose(file);

    return projects_pages;
}

char *GetInfoPath();

// ---------------------------------------------------------------------------
// ExtractRepo helpers : none of them stops the program, they only return NULL
// when they cannot do their job so that the next fallback can take over.
// ---------------------------------------------------------------------------

static char *__DuplicateRange(const char *start, size_t length)
{
    char *copy = malloc(length + 1);
    if (copy == NULL)
        err(EXIT_FAILURE, "malloc");
    memcpy(copy, start, length);
    copy[length] = '\0';
    return copy;
}

// Ask a question on stdin, NULL if the answer is empty or stdin is closed
static char *__PromptLine(const char *message)
{
    printf("%s", message);
    fflush(stdout);

    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), stdin) == NULL)
        return NULL;

    buffer[strcspn(buffer, "\r\n")] = '\0';
    if (buffer[0] == '\0')
        return NULL;

    return __DuplicateRange(buffer, strlen(buffer));
}

// Download a page without ever stopping the program. 0 on success.
static int __SoftPageDownload(char *url, char *file_name)
{
    int result = DowloadPage(url, file_name);
    if (result)
        result = DowloadPage(url, file_name);

    if (result)
        printf("\033[33mWARNING Impossible to download %s\033[0m\n", url);
    else
        printf("Downloaded with %s%s in %s\n", URL, url, file_name);

    return result;
}

// Read the git url written on the repository page (gitUrl ... value="...")
static char *__FindGitUrlInPage(const char *file_path)
{
    FILE *file = fopen(file_path, "r");
    if (file == NULL)
        return NULL;

    char buffer[2048 + 1];
    char *result = NULL;
    int in_git_url = 0;

    while (result == NULL && fgets(buffer, sizeof(buffer), file))
    {
        if (strstr(buffer, "gitUrl"))
            in_git_url = 1;
        if (!in_git_url)
            continue;

        char *value = strstr(buffer, "value");
        if (value == NULL || strlen(value) < 8)
            continue;
        value += 7;

        size_t length = 0;
        while (value[length] != '\0' && value[length] != '"')
            length++;
        if (length > 0)
            result = __DuplicateRange(value, length);
    }

    fclose(file);
    return result;
}

// Find the EPITA login (firstname.name) without stopping : 1. in the urls Jarvis
// already knows, 2. in the git email, 3. in the ssh banner of the forge
static char *__FindForgeLogin(void)
{
    char *login = NULL;

    // 1. Urls already registered in the info file ("login@git.forge.epita.fr:...")
    char *info_path = GetInfoPath();
    FILE *info_file = fopen(info_path, "r");
    free(info_path);
    if (info_file != NULL)
    {
        char *line = NULL;
        size_t capacity = 0;
        while (login == NULL && getline(&line, &capacity, info_file) >= 0)
        {
            char *at = strstr(line, "@git.forge.epita.fr");
            if (at == NULL)
                continue;

            char *start = at;
            while (start > line && start[-1] != '"' && start[-1] != ' ')
                start--;
            if (at > start && (size_t)(at - start) < 256)
                login = __DuplicateRange(start, (size_t)(at - start));
        }
        free(line);
        fclose(info_file);
    }
    if (login != NULL)
        return login;

    // 2. git config user.email = firstname.name@epita.fr
    FILE *pipe = popen("git config --get user.email 2>/dev/null", "r");
    if (pipe != NULL)
    {
        char email[256];
        if (fgets(email, sizeof(email), pipe) != NULL)
        {
            email[strcspn(email, "\r\n")] = '\0';
            char *at = strchr(email, '@');
            if (at != NULL && at != email && strcmp(at, "@epita.fr") == 0)
                login = __DuplicateRange(email, (size_t)(at - email));
        }
        pclose(pipe);
    }
    if (login != NULL)
        return login;

    // 3. Banner "Welcome @firstname.name from <ip>" printed by the forge
    pipe = popen("ssh -o StrictHostKeyChecking=no -o BatchMode=yes "
                 "-o ConnectTimeout=10 -T git@git.forge.epita.fr 2>&1",
                 "r");
    if (pipe != NULL)
    {
        char line[512];
        while (login == NULL && fgets(line, sizeof(line), pipe) != NULL)
        {
            char name[256];
            if (sscanf(line, "Welcome @%255s", name) == 1)
                login = __DuplicateRange(name, strlen(name));
        }
        pclose(pipe);
    }

    return login;
}

// "/epita-ing/net1-2627/root" + login ->
// "login@git.forge.epita.fr:p/epita-ing/net1-2627/epita-ing-net1-2627-login.git"
static char *__BuildGitUrl(const char *url, const char *login)
{
    char copy[SIZE_OF_STRING];
    snprintf(copy, sizeof(copy), "%s", url);

    char *parts[64];
    int count = 0;
    for (char *token = strtok(copy, "/"); token != NULL && count < 64;
         token = strtok(NULL, "/"))
        parts[count++] = token;

    if (count < 3 || strcmp(parts[count - 1], "root") != 0)
        return NULL;

    const char *type = parts[count - 3];
    const char *id = parts[count - 2];

    size_t size = 2 * strlen(login) + 2 * strlen(type) + 2 * strlen(id) + 64;
    char *git_url = malloc(size);
    if (git_url == NULL)
        err(EXIT_FAILURE, "malloc");
    snprintf(git_url, size, "%s@git.forge.epita.fr:p/%s/%s/%s-%s-%s.git", login,
             type, id, type, id, login);
    return git_url;
}

// Find the git url of the repository whose root page is <url>
// (ex : /epita-ing/net1-2627/root). Never stops the program : tries the
// repository page, then deduces the url from the EPITA login, then asks the
// user. Returns NULL only if nothing worked (the caller must handle it).
char *ExtractRepo(char *url)
{
    char *repo_url = NULL;
    char *dotjarvis_path = NULL;

    // 1. The repository page is <root url>/<id>
    //    (ex : /epita-ing/net1-2627/root/net1-2627)
    if (GetDotJarvisPath(&dotjarvis_path) == EXIT_SUCCESS)
    {
        char trimmed[SIZE_OF_STRING];
        snprintf(trimmed, sizeof(trimmed), "%s", url);
        size_t length = strlen(trimmed);
        while (length > 0 && trimmed[length - 1] == '/')
            trimmed[--length] = '\0';

        char *last_slash = strrchr(trimmed, '/');
        if (last_slash != NULL && last_slash != trimmed)
        {
            char head[SIZE_OF_STRING];
            snprintf(head, sizeof(head), "%.*s", (int)(last_slash - trimmed),
                     trimmed);
            const char *id = strrchr(head, '/');
            id = id == NULL ? head : id + 1;

            if (id[0] != '\0')
            {
                char page_url[SIZE_OF_STRING * 2];
                snprintf(page_url, sizeof(page_url), "%s/%s", trimmed, id);

                char file_path[SIZE_OF_STRING * 2];
                snprintf(file_path, sizeof(file_path), "%s/RepoPage.html",
                         dotjarvis_path);

                // Never read a page left by a previous run
                remove(file_path);

                if (__SoftPageDownload(page_url, "RepoPage.html") == 0)
                    repo_url = __FindGitUrlInPage(file_path);
            }
        }
    }
    free(dotjarvis_path);

    // 2. Deduce it : login@git.forge.epita.fr:p/<type>/<id>/<type>-<id>-<login>.git
    if (repo_url == NULL)
    {
        printf("\033[33mWARNING Git url not found on the repository page, "
               "deducing it from the repository path.\033[0m\n");

        char *login = __FindForgeLogin();
        if (login == NULL)
            login = __PromptLine("EPITA login (firstname.name) : ");

        if (login != NULL)
        {
            repo_url = __BuildGitUrl(url, login);
            free(login);
        }

        if (repo_url != NULL)
            printf("Deduced git url : \033[1m%s\033[0m\n", repo_url);
    }

    // 3. Last resort : ask the user
    if (repo_url == NULL)
        repo_url = __PromptLine("Please enter the git url of the repository "
                                "(empty to cancel) :\n> ");

    return repo_url;
}