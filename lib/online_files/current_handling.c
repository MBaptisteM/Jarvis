#include "current_handling.h"


typedef struct {
    char* url;
    char*** repos_url;
    int* ind_repos;
    pthread_mutex_t* lock;
    int id;
} ThreadArg;

// Function for childs, extract the urls from pages
void* ThreadExtractPages(void* arg){
    ThreadArg* t_arg = (ThreadArg*)arg;

    // --- Download page
    char file_name[SIZE_OF_STRING];
    snprintf(file_name, sizeof(file_name), "%s%d%s", "1Layer", t_arg->id, ".html");
    PageDownload(t_arg->url, file_name);

    char file_path[strlen(file_name) + SIZE_OF_STRING];
    char* dotjarvis_path;
    if (GetDotJarvisPath(&dotjarvis_path))
        errx(EXIT_FAILURE, "ERROR Impossible to find the .jarvis file");
    snprintf(file_path, sizeof(file_path), "%s/%s", dotjarvis_path, file_name);
    // --- Page downloaded


    // --- Locked
    pthread_mutex_lock(t_arg->lock);
    
    int size_projects;
    char** list_projects = ExtractProjects(file_path, &size_projects);
    for (int i = 0; i < size_projects; i++){
        (*t_arg->repos_url)[(*t_arg->ind_repos)++] = list_projects[i];
    }
        

    free(list_projects);
    
    pthread_mutex_unlock(t_arg->lock);
    // --- Unlocked

    free(t_arg);
    return NULL;
}

void* ThreadExtractURL(void* arg){
    ThreadArg* t_arg = (ThreadArg*)arg;

    // --- Download page
    char file_name[SIZE_OF_STRING];
    snprintf(file_name, sizeof(file_name), "%s%d%s", "2Layer", t_arg->id, ".html");
    PageDownload(t_arg->url, file_name);

    char file_path[strlen(file_name) + SIZE_OF_STRING];
    char* dotjarvis_path;
    if (GetDotJarvisPath(&dotjarvis_path))
        errx(EXIT_FAILURE, "ERROR Impossible to find the .jarvis file");
    snprintf(file_path, sizeof(file_path), "%s/%s", dotjarvis_path, file_name);
    // --- Page downloaded


    // --- Locked
    pthread_mutex_lock(t_arg->lock);

    int size_projects;
    char** list_projects = ExtractProjects(file_path, &size_projects);
    for (int i = 0; i < size_projects; i++){
        (*t_arg->repos_url)[(*t_arg->ind_repos)++] = list_projects[i];
    }
    free(list_projects);

    // char* first_url = ExtractProjects(file_path);

    // char* student_name;
    // if (ReadInfo("student_name", &student_name)){
    //     char final_url[SIZE_OF_STRING];
    //     snprintf(final_url, sizeof(final_url), "%s@git.forge.epita.fr:p/%s", student_name, ),
    //     (*t_arg->repos_url)[*t_arg->ind_repos] = 
    // }

    pthread_mutex_unlock(t_arg->lock);
    // --- Unlocked


    free(t_arg);
    return NULL;
}

char* GetCurrentRepo(){
    // if (PageDownload("", "home.html") == EXIT_FAILURE)
    //     return NULL;
    
    int ind_urls = 0;
    char* projects_url[256];
    // Extract //TODO (Remplir ind_urls et projects_url)

    projects_url[ind_urls++] = strdup("");  // home page

    pthread_t threads[256];
    pthread_mutex_t lock;
    pthread_mutex_init(&lock, NULL);

    int ind_pages = 0;
    char** pages_url = calloc(256, sizeof(char*)); 



    // one thread for every page
    // page maths/prog/algo...
    // LAYER 1
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


    int ind_repos = 0;
    char** repos_url = calloc(512, sizeof(char*));

    // one thread for every page
    // page all TPs
    // LAYER 2
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


    free(pages_url);

    pthread_mutex_destroy(&lock);


    int final_ind = 0;
    if (ind_repos > 0){
        printf("\033[1mWhich repository do you want to clone ?\033[0m\n\n");

        char** repos_mcq = calloc(ind_repos, sizeof(char*));
        for (int i = 0; i < ind_repos; i++){
            char* one_mcq = malloc(SIZE_OF_STRING);

            // skip "/root" at the end
            const char* last_slash = strrchr(repos_url[i], '/');
            size_t length = last_slash - repos_url[i];
            if (last_slash == NULL)
                one_mcq = strdup(repos_url[i]);
            else
                strncpy(one_mcq, repos_url[i], length);

            
            one_mcq[length] = '\0';
            repos_mcq[i] = one_mcq;
        }
        final_ind = ChoiceMCQ((const char**)repos_mcq, ind_repos);

        for (int i = 0; i < ind_repos; i++)
            free(repos_mcq[i]);
        free(repos_mcq);

        if (final_ind < 0)
            errx(EXIT_FAILURE, "ERROR Impossible to get the value of the repository's url");
    }
    else if (ind_repos < 0)
        errx(EXIT_FAILURE, "ERROR no repo found");

    char* final_url = malloc(SIZE_OF_STRING);
    if (final_url == NULL) 
        err(EXIT_FAILURE, "malloc failed");

    if (repos_url[final_ind] == NULL)
        errx(EXIT_FAILURE, "ERROR impossible to get the value after the choice");
        
    snprintf(final_url, SIZE_OF_STRING, "%s%s/root", "URL", repos_url[final_ind]);


    return ExtractRepo(final_url);
}


// Get the page with multiple tries
int PageDownload(char* url, char* file_name){
    int page_result = DowloadPage(url, file_name);

    if (page_result){
        page_result = DowloadPage(url, file_name);
    }

    if (page_result)
        errx(EXIT_FAILURE, "ERROR Impossible to get the home page (failed twice). [Retry or Auth again]");


    printf("Downloaded with %s%s in %s\n", URL, url, file_name);   //Important keep until end of debug
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
        char *c;

        if (strstr(buffer, "Finished"))
            break;

        c = strstr(buffer, "\"project\"");

        if (c) {
            c = strstr(buffer, "href");

            c += 6; // href="

            char* page = malloc(SIZE_OF_STRING);
            int i = 0;

            while (*c != '\"'){
                page[i++] = *(c++);
            }
            page[i++] = 0;

            projects_pages[(*size)++] = page;
        }
    }

    fclose(file);

    return projects_pages;
}


char* ExtractRepo(char* url){
    if (PageDownload(url, "RepoPage.html"))
        errx(EXIT_FAILURE, "ERROR impossible to download the repo page (check yout internet connection)");

    char file_path[SIZE_OF_STRING];
    char* dotjarvis_path;
    if (GetDotJarvisPath(&dotjarvis_path))
        errx(EXIT_FAILURE, "ERROR Impossible to find the .jarvis file");
    snprintf(file_path, sizeof(file_path), "%s/RepoPage.html", dotjarvis_path);

    FILE *file = fopen(file_path, "r");

    if (!file)
        errx(EXIT_FAILURE, "ERROR Impossible to open the file %s", file_path);

    char buffer[1024 + 1];
    char *c = NULL;

    while (fgets(buffer, 1024, file) && c == NULL){
        c = strstr(buffer, "list__item");
    }

    if (c == NULL)
        errx(EXIT_FAILURE, "ERROR Page of the repository not found");

    c = strstr(c, "href");
    c += 6; //hreaf="

    int ind_next_page = 0;
    char path_next_page[SIZE_OF_STRING];
    while (*c != '\"')
        path_next_page[ind_next_page++] = *(c++);
    
    path_next_page[ind_next_page] = 0;

    char url_next_page[SIZE_OF_STRING * 2];
    snprintf(url_next_page, sizeof(url_next_page), "%s/%s", url, path_next_page);

    fclose(file);


    // Second page
    if (PageDownload(url, "RepoPage.html"))
        errx(EXIT_FAILURE, "ERROR impossible to download the repo page (check yout internet connection)");

    snprintf(file_path, sizeof(file_path), "%s/RepoPage.html", dotjarvis_path);

    FILE *file2 = fopen(file_path, "r");

    if (!file2)
        errx(EXIT_FAILURE, "ERROR Impossible to open the file %s", file_path);


    
    c = NULL;
    while (fgets(buffer, 1024, file2) && c == NULL){
        c = strstr(buffer, "gitUrl");
    }
    if (c == NULL)
        errx(EXIT_FAILURE, "ERROR Page of the repository not found");

    c = strstr(buffer, "value");
    c += 7; //value="

    int ind_repo_url = 0;
    char* repo_url = malloc(SIZE_OF_STRING);
    while (*c != '\"')
        repo_url[ind_repo_url++] = *(c++);
    repo_url[ind_repo_url] = 0;


    fclose(file2);
    return repo_url;
}
