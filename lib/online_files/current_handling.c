#include "current_handling.h"


typedef struct {
    char* url;
    char*** repos_url;
    int* ind_repos;
    pthread_mutex_t* lock;
    int id;
} ThreadArg;

void* ThreadExtractPages(void* arg){
    ThreadArg* t_arg = (ThreadArg*)arg;

    char file_name[SIZE_OF_STRING];
    snprintf(file_name, sizeof(file_name), "%s%d%s", "1Layer", t_arg->id, ".html");
    PageDownload(t_arg->url, file_name);

    char file_path[strlen(file_name) + SIZE_OF_STRING];
    char* dotjarvis_path;
    if (GetDotJarvisPath(&dotjarvis_path))
        errx(EXIT_FAILURE, "ERROR Impossible to find the .jarvis file");
    snprintf(file_path, sizeof(file_path), "%s/%s", dotjarvis_path, file_name);

    pthread_mutex_lock(t_arg->lock);

    int size_projects;
    char** list_projects = ExtractProjects(file_path, &size_projects);
    for (int i = 0; i < size_projects; i++){
        (*t_arg->repos_url)[(*t_arg->ind_repos)++] = list_projects[i];
    }

    free(list_projects);

    pthread_mutex_unlock(t_arg->lock);

    free(t_arg);
    return NULL;
}

void* ThreadExtractURL(void* arg){
    ThreadArg* t_arg = (ThreadArg*)arg;

    char file_name[SIZE_OF_STRING];
    snprintf(file_name, sizeof(file_name), "%s%d%s", "2Layer", t_arg->id, ".html");
    PageDownload(t_arg->url, file_name);

    char file_path[strlen(file_name) + SIZE_OF_STRING];
    char* dotjarvis_path;
    if (GetDotJarvisPath(&dotjarvis_path))
        errx(EXIT_FAILURE, "ERROR Impossible to find the .jarvis file");
    snprintf(file_path, sizeof(file_path), "%s/%s", dotjarvis_path, file_name);

    pthread_mutex_lock(t_arg->lock);

    int size_projects;
    char** list_projects = ExtractProjects(file_path, &size_projects);
    for (int i = 0; i < size_projects; i++){
        (*t_arg->repos_url)[(*t_arg->ind_repos)++] = list_projects[i];
    }
    free(list_projects);

    pthread_mutex_unlock(t_arg->lock);

    free(t_arg);
    return NULL;
}

char* GetCurrentRepo(){
    int ind_urls = 0;
    char* projects_url[256];

    projects_url[ind_urls++] = strdup("");

    pthread_t threads[256];
    pthread_mutex_t lock;
    pthread_mutex_init(&lock, NULL);

    int ind_pages = 0;
    char** pages_url = calloc(256, sizeof(char*));

    for (int i = 0; i < ind_urls; i++){
        ThreadArg* arg = malloc(sizeof(ThreadArg));
        if (arg == NULL)
            err(EXIT_FAILURE, "malloc failed");

        arg->url = projects_url[i];
        arg->repos_url = &pages_url;
        arg->ind_repos = &ind_pages;
        arg->lock = &lock;
        arg->id = i;

        if (pthread_create(&(threads[i]), NULL, ThreadExtractPages, arg) != 0) {
            errx(EXIT_FAILURE, "ERROR while creating the thread %d", i);
        }
    }

    for (int i = 0; i < ind_urls; i++){
        if (pthread_join(threads[i], NULL) != 0){
            errx(EXIT_FAILURE, "ERROR Impossible to find the current repository (try to retry)");
        }
    }

    if (ind_pages == 0)
        errx(EXIT_FAILURE, "ERROR No project page found on the home page");

    int ind_repos = 0;
    char** repos_url = calloc(512, sizeof(char*));

    for (int i = 0; i < ind_pages; i++) {
        ThreadArg* arg = malloc(sizeof(ThreadArg));
        if (arg == NULL)
            err(EXIT_FAILURE, "malloc failed");

        arg->url = pages_url[i];
        arg->repos_url = &repos_url;
        arg->ind_repos = &ind_repos;
        arg->lock = &lock;
        arg->id = i;

        if (pthread_create(&threads[i], NULL, ThreadExtractURL, arg) != 0) {
            errx(EXIT_FAILURE, "ERROR while creating the thread %d", i);
        }
    }

    for (int i = 0; i < ind_pages; i++) {
        if (pthread_join(threads[i], NULL) != 0){
            errx(EXIT_FAILURE, "ERROR Impossible to find the current repository (try to retry)");
        }
    }

    for (int i = 0; i < ind_pages; i++)
        free(pages_url[i]);
    free(pages_url);

    pthread_mutex_destroy(&lock);

    if (ind_repos <= 0)
        errx(EXIT_FAILURE, "ERROR No repository found");

    int final_ind = 0;
    printf("\033[1mWhich repository do you want to clone ?\033[0m\n\n");

    char** repos_mcq = calloc(ind_repos, sizeof(char*));
    for (int i = 0; i < ind_repos; i++){
        const char* last_slash = strrchr(repos_url[i], '/');

        if (last_slash == NULL) {
            repos_mcq[i] = strdup(repos_url[i]);
        } else {
            size_t length = last_slash - repos_url[i];
            char* one_mcq = malloc(length + 1);
            strncpy(one_mcq, repos_url[i], length);
            one_mcq[length] = '\0';
            repos_mcq[i] = one_mcq;
        }
    }
    final_ind = ChoiceMCQ((const char**)repos_mcq, ind_repos);

    for (int i = 0; i < ind_repos; i++)
        free(repos_mcq[i]);
    free(repos_mcq);

    if (final_ind < 0)
        errx(EXIT_FAILURE, "ERROR Impossible to get the value of the repository's url");

    char* final_url = malloc(SIZE_OF_STRING);
    if (final_url == NULL)
        err(EXIT_FAILURE, "malloc failed");

    if (repos_url[final_ind] == NULL)
        errx(EXIT_FAILURE, "ERROR impossible to get the value after the choice");

    snprintf(final_url, SIZE_OF_STRING, "%s", repos_url[final_ind]);

    for (int i = 0; i < ind_repos; i++)
        free(repos_url[i]);
    free(repos_url);

    return ExtractRepo(final_url);
}


int PageDownload(char* url, char* file_name){
    int page_result = DowloadPage(url, file_name);

    if (page_result){
        page_result = DowloadPage(url, file_name);
    }

    if (page_result)
        errx(EXIT_FAILURE, "ERROR Impossible to get the home page (failed twice). [Retry or Auth again]");

    printf("Downloaded with %s%s in %s\n", URL, url, file_name);
    return EXIT_SUCCESS;
}


char** ExtractProjects(char *file_path, int* size){
    FILE *file = fopen(file_path, "r");

    if (!file) {
        errx(EXIT_FAILURE, "ERROR Impossible to open the file %s", file_path);
    }

    char buffer[1024 + 1];

    *size = 0;
    char** projects_pages = calloc(512, sizeof(char*));

    while (fgets(buffer, 1024, file)){
        if (strstr(buffer, "Finished"))
            break;

        char* c = strstr(buffer, "project");

        while (c) {
            char before = (c == buffer) ? ' ' : *(c - 1);
            char after = *(c + 7);

            if ((before == ' ' || before == '"' || before == '\'')
                && (after == ' ' || after == '"' || after == '\'')) {

                char* href = strstr(buffer, "href");

                if (href) {
                    href += 6;

                    char* page = malloc(SIZE_OF_STRING);
                    int i = 0;

                    while (*href != '\"' && *href != 0 && i < SIZE_OF_STRING - 1){
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


char* ExtractRepo(char* url){
    if (PageDownload(url, "RepoRoot.html") == EXIT_FAILURE)
        errx(EXIT_FAILURE, "ERROR impossible to download the repo page (check yout internet connection)");

    char file_path[SIZE_OF_STRING];
    char* dotjarvis_path;
    if (GetDotJarvisPath(&dotjarvis_path))
        errx(EXIT_FAILURE, "ERROR Impossible to find the .jarvis file");
    snprintf(file_path, sizeof(file_path), "%s/RepoRoot.html", dotjarvis_path);

    FILE *file = fopen(file_path, "r");

    if (!file)
        errx(EXIT_FAILURE, "ERROR Impossible to open the file %s", file_path);

    char buffer[2048 + 1];
    char *c = NULL;

    while (fgets(buffer, 1024, file) && c == NULL){
        c = strstr(buffer, "list__item");
    }

    if (c == NULL)
        errx(EXIT_FAILURE, "ERROR Root of the repository not found");

    c = strstr(c, "href");
    c += 6;

    int ind_next_page = 0;
    char path_next_page[SIZE_OF_STRING];
    while (*c != '\"')
        path_next_page[ind_next_page++] = *(c++);

    path_next_page[ind_next_page] = 0;

    char url_next_page[SIZE_OF_STRING * 2];
    snprintf(url_next_page, sizeof(url_next_page), "%s/%s", url, path_next_page);

    fclose(file);

    char temp_str[SIZE_OF_STRING] = "";
    char second_temp[SIZE_OF_STRING] = "";
    c = url;
    while (*c != 0){
        int i = 0;
        strcpy(second_temp, temp_str);
        while (*c != 0 && *c != '/')
            temp_str[i++] = *(c++);
        temp_str[i] = 0;
        c++;
    }

    char* new_url = malloc(SIZE_OF_STRING);
    int ind = 0;
    while (url[ind] != 0){
        new_url[ind] = url[ind];
        ind++;
    }
    new_url[ind++] = '/';
    int i = 0;
    while (second_temp[i] != 0)
        new_url[ind++] = second_temp[i++];
    new_url[ind] = 0;

    if (PageDownload(new_url, "RepoPage.html"))
        errx(EXIT_FAILURE, "ERROR impossible to download the repo page (check yout internet connection)");

    snprintf(file_path, sizeof(file_path), "%s/RepoPage.html", dotjarvis_path);

    FILE *file2 = fopen(file_path, "r");

    if (!file2)
        errx(EXIT_FAILURE, "ERROR Impossible to open the file %s", file_path);

    char* ch = NULL;
    while (ch == NULL && fgets(buffer, 2048, file2)){
        ch = strstr(buffer, "gitUrl");
    }
    if (ch == NULL)
        errx(EXIT_FAILURE, "ERROR Page of the repository not found");

    ch = strstr(buffer, "value");
    ch += 7;

    int ind_repo_url = 0;
    char* repo_url = malloc(SIZE_OF_STRING);
    while (*ch != '\"')
        repo_url[ind_repo_url++] = *(ch++);
    repo_url[ind_repo_url] = 0;

    fclose(file2);

    return repo_url;
}