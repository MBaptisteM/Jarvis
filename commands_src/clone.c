#include "clone.h"


// baptiste.mahe@git.forge.epita.fr:p/epita-prepa-computer-science/prog-208-p-05-2029/epita-prepa-computer-science-prog-208-p-05-2029-baptiste.mahe.git
// https://intra.forge.epita.fr/epita-prepa-computer-science/prog-101-p-00-2029/root/prog-101-p-00-2029

int main(int argc, char* argv[]){
    atexit(CleanupOnExit);
    
    // Check if there is a git remote
    if (argc < 2)
        errx(EXIT_FAILURE, "ERROR you must enter the git remote to clone your repository");

    if (__SameStr(argv[1], "current")){
        // Get the current repository
        argv[1] = GetCurrentRepo();
    }


    if (!__IsEpitaRepo(argv[1])){
        printf("\033[1;31mWARNING This string is not an epita repository.\033[0m\n");
        printf("\033[1m-> Classic clone\033[0m\n\n");

        size_t command_size = strlen(argv[1]) + SIZE_OF_STRING;
        char command_clone[command_size];
        snprintf(command_clone, command_size, "git submodule add --force %s", argv[1]);

        if(system(command_clone))
            errx(EXIT_FAILURE, "ERROR Impossible to clone the repository");

        return EXIT_SUCCESS;
    }
    else{
        // Do it here to avoid multithreading problems
        char auth_path[512];
        snprintf(auth_path, sizeof(auth_path), "%s/auth.json", GetSubjectFolderPath());

        // If not already Auth
        if (access(auth_path, F_OK) != 0) {
            Auth();
        }
    }

    // Preload subject and given files
    pid_t pid_get_given_files = fork();
    if (pid_get_given_files == 0){
        int given_files_return_value = GivenFilesDownload(argv[1]);

        if (given_files_return_value == -1)
            errx(EXIT_FAILURE, "ERROR Impossible to get the given files, check your connection or try to Auth again.");

        exit(given_files_return_value);
    }

    pid_t pid_get_subjects = fork();
    if (pid_get_subjects == 0){

        if (SubjectDownload(argv[1]))
            errx(EXIT_FAILURE, "ERROR Impossible to get the subjects, check your connection or try to Auth again.");
            
        exit(EXIT_SUCCESS);
    }
    

    // CREATE PATH
    char* main_folder_path = GetOrCreateRepoRoot();
    if (main_folder_path == NULL)
        return EXIT_FAILURE;


    size_t size_relative_path;
    char** relative_path = __GetRelavitvePath(argv[1], &size_relative_path);


    size_t size_main_folder_path = strlen(main_folder_path);

    for (size_t i = 0; i < size_relative_path; i++){
        if (__OneLayerFindOrCreate(main_folder_path, relative_path[i], 1)){
            errx( EXIT_FAILURE, "ERROR Impossible to find or create te folder : %s%s", 
                                main_folder_path, relative_path[i]);
        }

        size_t size_full_path = size_main_folder_path + strlen(relative_path[i]) + 2;
        main_folder_path = realloc(main_folder_path, size_full_path);
        if (main_folder_path == NULL)
            errx(EXIT_FAILURE, "ERROR Impossible to allocate memory, not enough space. (Try to rerun)");
        
        size_t j = 0;
        while (relative_path[i][j] != '\0'){
            main_folder_path[size_main_folder_path + j] = relative_path[i][j];
            j++;
        }
        main_folder_path[size_main_folder_path + j] = '/';
        main_folder_path[size_main_folder_path + j + 1] = '\0';

        free(relative_path[i]);
        size_main_folder_path = size_full_path - 1;
        
    }
    free(relative_path);


    // Bool to know if the repo is new or empty
    int repo_exists_or_filled = 0;
    char repo_name[SIZE_OF_STRING];

    size_t size_repo_path = size_main_folder_path + SIZE_OF_STRING + 2;
    char repo_path[size_repo_path];
    


    char *read_data;
    int already_cloned = ReadInfo(argv[1], &read_data) == 0;


    //format : type/id/root... | goal format : type-id-name
    const char *local_url_repo = GetLocalUrlRepo(argv[1]);

    int need_to_be_renamed = 0;

    if (already_cloned){
        size_t size_old_repo_path = size_main_folder_path + strlen(read_data) + 2;
        char old_repo_path[size_old_repo_path];
        snprintf(old_repo_path, size_old_repo_path, "%s%s", main_folder_path, read_data);
        

        if (access(old_repo_path, F_OK) == 0){
            repo_exists_or_filled = 1;
            
            __PrintPages(local_url_repo);

            printf("\n\033[1;31mWARNING This repository already exists.\033[0m\n");
        }
            

        size_t i = 0;
        while ((repo_name[i] = read_data[i]) != 0){
            i++;
        }

        snprintf(repo_path, size_repo_path, "%s%s", main_folder_path, repo_name);

        if (read_data[0] < 'A' || read_data[0] > 'Z')
            need_to_be_renamed = 1;

        free(read_data);
    }

    if (!repo_exists_or_filled){
        need_to_be_renamed = 1;

        // Get and save the name of the repo
        size_t i = 0;
        const char *c = local_url_repo;

        // extract type
        while (*c != '/')
            repo_name[i++] = *(c++);
        
        c++;
        repo_name[i++] = '-';
        
        // extract id
        while (*c != '/')
            repo_name[i++] = *(c++);


        repo_name[i++] = '-';
        size_t j = 0;


        //extract name from name@git...
        while (argv[1][j] != '@')
            repo_name[i++] = argv[1][j++];
            
        
        repo_name[i] = 0;


        snprintf(repo_path, size_repo_path, "%s%s", main_folder_path, repo_name);

        __PrintPages(local_url_repo);


        // CLONE
        size_t command_size = size_main_folder_path + strlen(argv[1]) + SIZE_OF_STRING;
        char command_clone[command_size];
        // snprintf(command_clone, command_size, "git -C %s clone %s", main_folder_path, argv[1]);
        snprintf(command_clone, command_size, "git -C %s submodule add --force %s", main_folder_path, argv[1]);


        // Check if the clone is empty
        if(system(command_clone)){
            // Handle error in previous execution
            if (access(repo_path, F_OK) == 0){
                WriteInfo(argv[1], repo_name);

                repo_exists_or_filled = 1;
                printf("\n\033[1;31mWARNING This repository already exists.\033[0m\n");
            }
            else
                errx(EXIT_FAILURE, "ERROR Impossible to clone the repository");
        }
        else{
            repo_exists_or_filled = repo_exists_or_filled || !IsEmpty(repo_path);

            if (repo_exists_or_filled){
                printf("\n\033[1;31mWARNING This repository is not empty.\033[0m\n");
            } 
        }
          
    }

    // Make a choice if the repo is already filled
    if (repo_exists_or_filled){
        printf("\033[1mWhat should be done ?\033[0m\n\n");

        const char* choices[] = {
            "Add the tree structure missing files/folders.",
            "Do not change the content of the repo.",
            "Erase the repo content and create the expected tree structure."
        };

        switch(ChoiceMCQ(choices, 3)){
            case 0:
                AddGivenFilesUsefulParts(repo_path, pid_get_given_files);

                CreateSubjectElements(repo_path, pid_get_subjects);
                break;
            case 1:
                kill(pid_get_given_files, SIGKILL);

                if (need_to_be_renamed){
                    waitpid(pid_get_subjects, NULL, 0);
                    printf("\033[32mSubject downloaded.\033[0m\n");
                }
                else{
                    kill(pid_get_subjects, SIGKILL);
                    waitpid(pid_get_subjects, NULL, 0);
                }
                waitpid(pid_get_given_files, NULL, 0);

                break;
            case 2:
                if (CleanFolder(repo_path))
                    errx(EXIT_FAILURE, "ERROR Impossible to erase the datas of the folder %s, check the permissions.\n", repo_path);

                UncompressGivenFiles(repo_path, pid_get_given_files);

                CreateSubjectElements(repo_path, pid_get_subjects);
                break;
            default:
                errx(EXIT_FAILURE, "ERROR Impossible to get the choice");
        }
    }
    else{
        UncompressGivenFiles(repo_path, pid_get_given_files);

        CreateSubjectElements(repo_path, pid_get_subjects);
    }

    // Rename the child repository
    if (need_to_be_renamed)
        __RenameRepo(repo_path, argv[1]);

    char* name_repo;
    if (ReadInfo(argv[1], &name_repo))
        errx(EXIT_FAILURE, "ERROR Impossible to read Jarvis files");
    snprintf(repo_path, size_repo_path, "%s%s", main_folder_path, name_repo);


    free(main_folder_path);

    // Push the repos (parent & child (if necessary))
    pid_t pid_push_root = fork();
    if (pid_push_root == 0){
        char* repo_name;
        if (ReadInfo(argv[1], &repo_name))
            errx(EXIT_FAILURE, "Impossible to read info file");
        
        char commit_name[strlen(repo_name) + SIZE_OF_STRING];
        snprintf(commit_name, strlen(repo_name) + SIZE_OF_STRING, "Clone %s", repo_name);

        exit(AddRepoRoot() || PushRepoRoot(commit_name));
    }
    else{
        int result_child;
        int result_push = PushRepo(repo_path, "Tree structure", NULL);

        waitpid(pid_push_root, &result_child, 0);

        // Clone case
        if (result_child == 2){
            // TODO
            // Copy all the files in TPs folder
            char* TPs_path = GetTPsPath();
            char clone_path[strlen(TPs_path) + SIZE_OF_STRING];
            snprintf(clone_path, sizeof(clone_path), "%s/%s", TPs_path, REPO_NAME);
            MergeFodldersClone(TPs_path, clone_path);

            // Move the clone folder
            char command_move[strlen(REPO_NAME) + SIZE_OF_STRING];
            snprintf(command_move, sizeof(command_move),
                    "mv %s ../", REPO_NAME);
            if (chdir(TPs_path) != 0 || system(command_move) != 0)
                errx(EXIT_FAILURE, "ERROR Impossible to execute commands");
        }

        if (result_push == -1)
                errx(EXIT_FAILURE, "ERROR %s no such file directory", repo_path);
    }


    __PrintPages(local_url_repo);

    return EXIT_SUCCESS;
}


void __PrintPages(const char *local_url_repo){
    printf("\n\033[1;32mRepo page :\033[0m\n\033[1m%s%s\033[0m\n\n", URL, local_url_repo);
    printf("\033[1;32mSubject page :\033[0m\n\033[1m%s%s%s\033[0m\n\n", URL,  local_url_repo, SUBJECT);
}

int __SameStr(char* s1, char* s2){
    while (*s1 && *s2 && (tolower((unsigned char)*s1) == tolower((unsigned char)*s2))) {
        s1++;
        s2++;
    }
    return !*s1 && !*s2 && tolower((unsigned char)*s1) - tolower((unsigned char)*s2) == 0;
}



int __RenameRepo(char *repo_path, char *repo_name){
    // STUBBORN
    char stubborn[SIZE_OF_STRING];
    size_t i = 0;

    char *c = repo_name;

    // Skip useless part
    while (*c != 0 && (*c < '0' || *c > '9'))
        c++;
    
    while (*c != 0 && *c != '-')
        c++;

    if (*c == 0)
        errx(EXIT_FAILURE, "ERROR This is not an epita repository.");
    
    c++;

    // Letter
    if (*c >= 'a' && *c <= 'z')
        stubborn[i++] = *(c++) + 'A' - 'a';
    else
        stubborn[i++] = *(c++);

    c++;

    // Number
    if (*c != '0')
        stubborn[i++] = *c;
    c++;
    stubborn[i++] = *c;

    stubborn[i] = 0;


    // NAME
    char *raw_name = NULL;

    DIR *dir;
    struct dirent *entry;
    dir = opendir(repo_path);

    if (dir == NULL) {
        errx(EXIT_FAILURE, "ERROR Impossible to open folder %s", repo_path);
    }

    struct stat st;
    while ((entry = readdir(dir)) != NULL){
        // Ignore ./ and ../
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || strcmp(entry->d_name, ".git") == 0)
            continue;

        char path_new_element[1024];
        snprintf(path_new_element, sizeof(path_new_element), "%s/%s", repo_path, entry->d_name);


        if (stat(path_new_element, &st) == 0 && S_ISDIR(st.st_mode)){
            // raw_name = entry->d_name;
            raw_name = malloc(SIZE_OF_STRING);
            strcpy(raw_name, entry->d_name);
            break;
        }
    }
    closedir(dir);

    if (raw_name == NULL){
        printf("\033[31mWARNING The repository doesn't have a valid tree structure.\033[0m\n");
        WriteInfo(repo_name, repo_name);
        return EXIT_FAILURE;
    }
        
    
    char name[SIZE_OF_STRING];
    i = 0;
    int need_upper = 1;

    c = raw_name;

    while (*c != 0){
        if (need_upper && *c != '_' && *c != '-'){
            if (*c >= 'a' && *c <= 'z')
                name[i++] = *c + 'A' - 'a';
            else
                name[i++] = *c;

            need_upper = 0;
        }
        else{
            if (*c == '_' || *c == '-')
                need_upper = 1;
            else
                name[i++] = *c;
        }
        c++;
    }
    name[i] = 0;

    free(raw_name);
    

    // MODULES
    char subject_path[SIZE_OF_STRING + 20];
    snprintf(subject_path, SIZE_OF_STRING, "%s/%s", GetSubjectFolderPath(), "subject.html");

    char *modules = GetSubjectModules(subject_path);
    if (!modules)
        errx(EXIT_FAILURE, "Impossible to read the subject.");



    // ALL TOGETHER & RENAME
    char full_name[strlen(stubborn) + strlen(name) + strlen(modules) + 20];
    snprintf(full_name, sizeof(full_name), "%s-%s-%s", stubborn, name, modules);


    // char rename_command[strlen(repo_path) + sizeof(full_name) + 20];
    // snprintf(rename_command, sizeof(rename_command), "mv %s %s", repo_path, full_name);

    

    if (chdir(repo_path) != 0 || chdir("..") != 0)
        errx(EXIT_FAILURE, "ERROR Impossible to navigate through folders");

    char* path;
    if ((path = getcwd(NULL, 0)) == NULL)
        errx(EXIT_FAILURE, "ERROR Imossible to get the current repository");

    // // Execute rename
    // if (system(rename_command))
    //     errx(EXIT_FAILURE, "ERROR Impossible to rename the repo folder");

    /// ----- TODO REANEME NE FONCTIONNE PAS
    char* root_folder;
    if (ReadInfo("main_path", &root_folder))
        errx(EXIT_FAILURE, "ERROR Impossible to find the root repo");

    size_t root_len = strlen(root_folder);

    // 1. Rendre repo_path relatif
    char *rel_repo_path = repo_path;
    if (strncmp(repo_path, root_folder, root_len) == 0) {
        rel_repo_path += root_len;
        if (*rel_repo_path == '/') rel_repo_path++;
    }

    // 2. Rendre path relatif
    char *rel_path = path;
    if (strncmp(path, root_folder, root_len) == 0) {
        rel_path += root_len;
        if (*rel_path == '/') rel_path++;
    }

    // 3. Construire le chemin de destination final complet (relatif)
    char final_dest[512];
    snprintf(final_dest, sizeof(final_dest), "%s/%s", rel_path, full_name);

    // --- SOLUTION SANS COMMIT, AVEC NETTOYAGE COMPLET DES REFERENCES ---
    char rename_command[2048];
    snprintf(rename_command, sizeof(rename_command),
        "cd \"%s\" && "
        "mv \"%s\" \"%s\" && "
        "git submodule absorbgitdirs \"%s\" && "
        "git add \"%s\"",
        root_folder,
        rel_repo_path, final_dest,
        final_dest,
        final_dest);

    printf("COMMAND : %s\n", rename_command);

    if (system(rename_command))
        errx(EXIT_FAILURE, "ERROR Impossible to rename the git repo folder");

    // ---- END TODO


    WriteInfo(repo_name, full_name);


    return EXIT_SUCCESS;
}



// Check if it is an epita repo
int __IsEpitaRepo(char* repo_name){
    int i = 0;
    while (repo_name[i] != '\0' && repo_name[i] != '@')
        i++;

    char *reference_str = "@git.forge.epita.fr";

    int j = 0;
    while (repo_name[i + j] != '\0' && reference_str[j] != '\0' && repo_name[i + j] == reference_str[j])
        j++;

    return reference_str[j] == '\0';
}


// Extract the relative path of that repo
// Example : { S1, B1, P1-GaulishVillage-i3-shell-git } (for now : { S1, B1 })
char** __GetRelavitvePath(char *repo_name, size_t *size){
    char** relative_path = malloc(SIZE_OF_STRING);
    (*size) = 0;

    int k = 0;

    while (repo_name[k] < '0' ||  repo_name[k] > '9')
        k++;
        
    int num_bimester = (repo_name[k + 1] - '0') * 10 + (repo_name[k + 2] - '0');
    int num_semester = num_bimester / 2 + num_bimester % 2;

    (*size)++;
    relative_path[0] = malloc(SIZE_OF_STRING);
    snprintf(relative_path[0], SIZE_OF_STRING, "%s%d", 
            SEMESTER, num_semester);

    (*size)++;
    relative_path[1] = malloc(SIZE_OF_STRING);
    snprintf(relative_path[1], SIZE_OF_STRING, "%s%d", 
            BIMESTER, num_bimester);
    
    k += 3;

    (void)size;
    return relative_path;
}


// Find a file or find or create a folder -> 0 if found / 1 if not found
int __OneLayerFindOrCreate(char *path, char *name, int is_folder){
    
    // Open the current directory
    DIR *dir;
    struct dirent *file;
    struct stat info;

    dir = opendir(path);

    if (dir == NULL)
        return EXIT_FAILURE;

    // Iterate trough all files and check if they are what we are looking for
    while ((file = readdir(dir)) != NULL) {
        if (stat(file->d_name, &info) == 0) {
            if (S_ISDIR(info.st_mode) && is_folder) {
                // Folder case
                if (strcmp(name, file->d_name) == 0){
                    closedir(dir);
                    return EXIT_SUCCESS;
                }
            }
            else if (!is_folder){
                // Not a folder case
                if (strcmp(name, file->d_name) == 0){
                    closedir(dir);
                    return EXIT_SUCCESS;
                }
            }
        }
    }

    if (is_folder){
        size_t size_fill_path = strlen(path) + strlen(name) + 2;
        char fullpath[size_fill_path];
        if (snprintf(fullpath, size_fill_path, "%s%s", path, name) < 0)
            return EXIT_FAILURE;

        if (mkdir(fullpath, 0755) && !opendir(fullpath)){
            errx(EXIT_FAILURE, "ERROR Impossible to create the folder : %s (Check the rights or try to rerun)", fullpath);
        }
        //printf("Jarvis created : %s\n", fullpath);
        return EXIT_SUCCESS;
    }

    closedir(dir);
    return EXIT_FAILURE;
}