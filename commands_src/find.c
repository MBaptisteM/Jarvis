#include "find.h"

int main(int argc, char* argv[]){
    // Case no argument, or "root": print the url of the root repository
    if (argc < 2 || strcasecmp(argv[1], "root") == 0){
        __FindRoot();
        return EXIT_SUCCESS;
    }

    // Case current: find the repository saved as "current" in the logs
    if (strcasecmp(argv[1], "current") == 0){
        __FindCurrent();
        return EXIT_SUCCESS;
    }

    // Case a specific git repo (already cloned, or an EPITA url not yet
    // registered), or a keyword to look for in repo titles
    __FindRepository(argv[1]);

    return EXIT_SUCCESS;
}


// Find and print the path of the root repository (saved as "main_path"
// in the info files)
void __FindRoot(void){
    char* main_folder_path;
    if (ReadInfo("main_path", &main_folder_path))
        errx(EXIT_FAILURE, "ERROR Impossible to find the root repository (did you clone anything yet?)");

    __TrimTrailing(main_folder_path);

    printf("%s\n", main_folder_path);

    free(main_folder_path);
}


// Strip trailing '\n', '\r', spaces or tabs that might have slipped into a
// value read from the info files
void __TrimTrailing(char* str){
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r' || str[len - 1] == ' ' || str[len - 1] == '\t')){
        str[len - 1] = 0;
        len--;
    }
}


// Find and print the path of the repository saved as "current"
// ("current" can be stored either as an absolute path, or as a path
// relative to the root repository, i.e. "main_path")
void __FindCurrent(void){
    char* main_folder_path;
    if (ReadInfo("main_path", &main_folder_path))
        errx(EXIT_FAILURE, "ERROR Impossible to find the root repository (did you clone anything yet?)");

    __TrimTrailing(main_folder_path);

    char* current_value;
    if (ReadInfo("current", &current_value)){
        free(main_folder_path);
        errx(EXIT_FAILURE, "ERROR Trying to find the current repository but no current repository found");
    }

    __TrimTrailing(current_value);

    char* path;
    if (current_value[0] == '/'){
        // Already an absolute path
        path = current_value;
    }
    else{
        // Relative to the root repository
        size_t main_len = strlen(main_folder_path);
        int need_slash = (main_len > 0 && main_folder_path[main_len - 1] != '/');

        size_t size_path = main_len + strlen(current_value) + (need_slash ? 2 : 1);
        path = malloc(size_path);
        snprintf(path, size_path, "%s%s%s", main_folder_path, need_slash ? "/" : "", current_value);

        free(current_value);
    }

    // Get the repo name from the path, used as a fallback if it moved
    char repo_name[SIZE_OF_STRING];
    int i = 0;
    char* c = path;
    while (*c != 0){
        if (*c != '/')
            repo_name[i++] = *c;
        else
            i = 0;
        c++;
    }
    repo_name[i] = 0;

    // Get the path again if the folder moved
    if (access(path, F_OK) != 0){
        free(path);
        path = FindFileBFS(repo_name);

        if (path == NULL){
            free(main_folder_path);
            errx(EXIT_FAILURE, "ERROR Impossible to find the current repository %s locally (try to re-clone it)", repo_name);
        }

        // Keep the current repository up to date in the logs
        WriteInfo("current", path);
    }

    printf("%s\n", path);

    free(path);
    free(main_folder_path);
}


// Find a specific repository, in three steps :
// 1. repo_arg is a git remote already registered in the info files (same
//    lookup pattern as __PullRepository in pull.c)
// 2. repo_arg looks like an EPITA git url, but isn't registered under this
//    exact key yet -> recompute its expected place in the arborescence
// 3. repo_arg is not a git remote at all -> treat it as a keyword and search
//    it inside every repo title
void __FindRepository(char* repo_arg){
    char* repo_name;

    // Case 1: repo_arg is a known git remote already registered in the info files
    if (ReadInfo(repo_arg, &repo_name) == 0){
        __TrimTrailing(repo_name);

        char* main_folder_path;
        if (ReadInfo("main_path", &main_folder_path))
            errx(EXIT_FAILURE, "ERROR Impossible to find the root repository (did you clone anything yet?)");

        __TrimTrailing(main_folder_path);

        size_t repo_path_size = strlen(main_folder_path) + strlen(repo_name) + 2;
        char repo_path[repo_path_size];
        snprintf(repo_path, repo_path_size, "%s%s", main_folder_path, repo_name);

        // Get the path again if the folder moved
        if (access(repo_path, F_OK) != 0){
            char* found_path = FindFileBFS(repo_name);
            if (found_path == NULL)
                errx(EXIT_FAILURE, "ERROR Impossible to find the repository %s locally (try to re-clone it)", repo_name);

            printf("%s\n", found_path);
            free(found_path);
        }
        else
            printf("%s\n", repo_path);

        free(repo_name);
        free(main_folder_path);
        return;
    }

    // Case 2: repo_arg looks like an EPITA git url (e.g. not yet cloned
    // under this exact key, or the info file entry got lost) -> recompute
    // its expected place in the arborescence instead of a useless full-url
    // keyword search
    if (__IsEpitaUrl(repo_arg)){
        __FindEpitaUrlInTree(repo_arg);
        return;
    }

    // Case 3: repo_arg is not a git remote at all, treat it as a keyword
    // and search it inside every repo title
    __FindByKeyword(repo_arg);
}


// Check if repo_arg looks like an EPITA git url (same check as
// __IsEpitaRepo in clone.c, reimplemented here since each command is its
// own binary)
int __IsEpitaUrl(char* url){
    int i = 0;
    while (url[i] != '\0' && url[i] != '@')
        i++;

    char *reference_str = "@git.forge.epita.fr";

    int j = 0;
    while (url[i + j] != '\0' && reference_str[j] != '\0' && url[i + j] == reference_str[j])
        j++;

    return reference_str[j] == '\0';
}


// Extract the "S{semester}/B{bimester}" location of a repository from its
// url, the same way __GetRelavitvePath does in clone.c
void __ExtractRelativePath(char* url, char* semester_out, char* bimester_out){
    int k = 0;
    while (url[k] < '0' || url[k] > '9')
        k++;

    int num_bimester = (url[k + 1] - '0') * 10 + (url[k + 2] - '0');
    int num_semester = num_bimester / 2 + num_bimester % 2;

    snprintf(semester_out, SIZE_OF_STRING, "%s%d", SEMESTER, num_semester);
    snprintf(bimester_out, SIZE_OF_STRING, "%s%d", BIMESTER, num_bimester);
}


// Extract the "stubborn" prefix (type letter + number, e.g. "P4") that
// every repo title starts with, the same way __RenameRepo does in clone.c.
// Returns 0 on success, 1 if the url doesn't have the expected shape.
int __ExtractStubborn(char* url, char* stubborn_out){
    char* c = url;

    while (*c != 0 && (*c < '0' || *c > '9'))
        c++;

    while (*c != 0 && *c != '-')
        c++;

    if (*c == 0)
        return 1;

    c++;

    size_t i = 0;

    if (*c >= 'a' && *c <= 'z')
        stubborn_out[i++] = *(c++) + 'A' - 'a';
    else
        stubborn_out[i++] = *(c++);

    c++;

    if (*c != '0')
        stubborn_out[i++] = *c;
    c++;
    stubborn_out[i++] = *c;

    stubborn_out[i] = 0;

    return 0;
}


// Given an EPITA git url that isn't registered as-is in the info files,
// find its place in the arborescence: go straight to S{semester}/B{bimester}
// and look for a repo title starting with the "stubborn" prefix (e.g. "P4-")
void __FindEpitaUrlInTree(char* url){
    char* main_folder_path;
    if (ReadInfo("main_path", &main_folder_path))
        errx(EXIT_FAILURE, "ERROR Impossible to find the root repository (did you clone anything yet?)");

    __TrimTrailing(main_folder_path);

    char semester[SIZE_OF_STRING];
    char bimester[SIZE_OF_STRING];
    __ExtractRelativePath(url, semester, bimester);

    char stubborn[SIZE_OF_STRING];
    int stubborn_failed = __ExtractStubborn(url, stubborn);

    size_t size_target_path = strlen(main_folder_path) + strlen(semester) + strlen(bimester) + 3;
    char target_path[size_target_path];
    snprintf(target_path, size_target_path, "%s%s/%s", main_folder_path, semester, bimester);

    free(main_folder_path);

    if (access(target_path, F_OK) != 0)
        errx(EXIT_FAILURE, "ERROR Impossible to find the %s folder, this repository doesn't seem to have been cloned yet.", target_path);

    int found_any = 0;

    // Look for a repo folder starting with the stubborn prefix (e.g. "P4-")
    if (!stubborn_failed){
        DIR* dir = opendir(target_path);
        if (dir != NULL){
            struct dirent* entry;
            struct stat info;

            while ((entry = readdir(dir)) != NULL){
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                    continue;

                size_t size_full_path = strlen(target_path) + strlen(entry->d_name) + 2;
                char full_path[size_full_path];
                snprintf(full_path, size_full_path, "%s/%s", target_path, entry->d_name);

                if (stat(full_path, &info) != 0 || !S_ISDIR(info.st_mode))
                    continue;

                size_t len_stubborn = strlen(stubborn);
                if (strncasecmp(entry->d_name, stubborn, len_stubborn) == 0 && entry->d_name[len_stubborn] == '-'){
                    printf("%s\n", full_path);
                    found_any = 1;
                }
            }

            closedir(dir);
        }
    }

    if (!found_any)
        printf("\033[1;31mWARNING Impossible to find this repository's title in %s (it might not be cloned yet, or the tree structure is unexpected).\033[0m\n", target_path);
}


// Search every repo title for "word" as a case-insensitive substring,
// printing the path of every match. The search is strictly bounded to the
// children of the root repository (main_path) - it never looks outside of it.
void __FindByKeyword(char* word){
    char* main_folder_path;
    if (ReadInfo("main_path", &main_folder_path))
        errx(EXIT_FAILURE, "ERROR Impossible to find the root repository (did you clone anything yet?)");

    __TrimTrailing(main_folder_path);

    // Fail loudly instead of silently if the recorded root doesn't exist
    // anymore, instead of just printing a confusing "not found" at the end
    if (access(main_folder_path, F_OK) != 0)
        errx(EXIT_FAILURE, "ERROR The root repository %s doesn't exist (anymore), check your Jarvis setup.", main_folder_path);

    int found_any = 0;
    __SearchTree(main_folder_path, word, &found_any);

    if (!found_any)
        printf("\033[1;31mWARNING No repository title contains \"%s\" (searched from %s).\033[0m\n", word, main_folder_path);

    free(main_folder_path);
}


// Case-insensitive substring search: is "word" contained in "haystack" ?
int __ContainsWord(const char* haystack, const char* word){
    size_t len_haystack = strlen(haystack);
    size_t len_word = strlen(word);

    if (len_word == 0 || len_word > len_haystack)
        return 0;

    for (size_t i = 0; i + len_word <= len_haystack; i++){
        size_t j = 0;
        while (j < len_word && tolower((unsigned char)haystack[i + j]) == tolower((unsigned char)word[j]))
            j++;

        if (j == len_word)
            return 1;
    }

    return 0;
}


// Recursively walk the tree starting at "path", printing the path of every
// folder whose name contains "word" (case-insensitive). Does not go inside
// a matching folder (no need to search inside an already found repo), nor
// inside ".git" folders.
void __SearchTree(const char* path, const char* word, int* found_any){
    DIR* dir = opendir(path);
    if (dir == NULL)
        return;

    struct dirent* entry;
    struct stat info;

    while ((entry = readdir(dir)) != NULL){
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || strcmp(entry->d_name, ".git") == 0)
            continue;

        // Build the child path, avoiding a double '/' when "path" already ends with one
        size_t path_len = strlen(path);
        int need_slash = (path_len > 0 && path[path_len - 1] != '/');

        size_t size_full_path = path_len + strlen(entry->d_name) + (need_slash ? 2 : 1);
        char full_path[size_full_path];
        snprintf(full_path, size_full_path, "%s%s%s", path, need_slash ? "/" : "", entry->d_name);

        if (stat(full_path, &info) != 0 || !S_ISDIR(info.st_mode))
            continue;

        if (__ContainsWord(entry->d_name, word)){
            printf("%s\n", full_path);
            (*found_any) = 1;
            continue;
        }

        __SearchTree(full_path, word, found_any);
    }

    closedir(dir);
}