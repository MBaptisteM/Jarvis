#include "clone.h"


// baptiste.mahe@git.forge.epita.fr:p/epita-prepa-computer-science/prog-208-p-05-2029/epita-prepa-computer-science-prog-208-p-05-2029-baptiste.mahe.git
// https://intra.forge.epita.fr/epita-prepa-computer-science/prog-101-p-00-2029/root/prog-101-p-00-2029

// https://intra.forge.epita.fr/ + {epita-prepa-computer-science/prog-208-p-05-2029/} + root/ + {prog-208-p-05-2029}
// -> connexion au site internet -> récupère subject + given files

// Créer un fichier jarvis (qui sert de repere pour connaitre la base)
// Stocker l'avancée de chaque push des repos
// Stocker le chemin vers ce fichier dans un autre fichier dont je connais toujours l'emplacement

int main(int argc, char* argv[]){
    // Check if there is a git remote
    if (argc < 2)
        errx(EXIT_FAILURE, "ERROR you must enter the git remote to clone your repository");


    if (!__IsEpitaRepo(argv[1])){
        printf("\033[1;31mWARNING This string is not an epita repository.\033[0m\n");
        printf("\033[1m-> Classic clone\033[0m\n\n");

        size_t command_size = strlen(argv[1]) + SIZE_OF_STRING;
        char command_clone[command_size];
        snprintf(command_clone, command_size, "git clone %s", argv[1]);

        if(system(command_clone))
            errx(EXIT_FAILURE, "ERROR Impossible to clone the repository");

        return EXIT_SUCCESS;
    }
    

    // CREATE PATH

    char* main_folder_path = __GetOrCreateMainFolderPath();
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

    

    if (already_cloned){
        size_t size_old_repo_path = size_main_folder_path + strlen(read_data) + 2;
        char old_repo_path[size_old_repo_path];
        snprintf(old_repo_path, size_old_repo_path, "%s%s", main_folder_path, read_data);

        if (access(old_repo_path, F_OK) == 0){
            repo_exists_or_filled = 1;
            printf("\n\033[1;31mWARNING This repository already exists.\033[0m\n");
        }

        size_t i = 0;
        while ((repo_name[i] = read_data[i]) != 0){
            i++;
        }

        snprintf(repo_path, size_repo_path, "%s%s", main_folder_path, repo_name);

        free(read_data);
    }

    if (!repo_exists_or_filled){
        // CLONE
        size_t command_size = size_main_folder_path + strlen(argv[1]) + SIZE_OF_STRING;
        char command_clone[command_size];
        snprintf(command_clone, command_size, "git -C %s clone %s", main_folder_path, argv[1]);


        // Get and save the name of the repo

        //format : type/id/root... | goal format : type-id-name
        const char *local_url_repo = GetLocalUrlRepo(argv[1]);

        size_t i = 0;

        // extract type
        while (*local_url_repo != '/')
            repo_name[i++] = *(local_url_repo++);
        
        local_url_repo++;
        repo_name[i++] = '-';
        
        // extract id
        while (*local_url_repo != '/')
            repo_name[i++] = *(local_url_repo++);


        repo_name[i++] = '-';
        size_t j = 0;

        //extract name from name@git...
        while (argv[1][j] != '@')
            repo_name[i++] = argv[1][j++];
            
        
        repo_name[i] = 0;

        snprintf(repo_path, size_repo_path, "%s%s", main_folder_path, repo_name);

        // PAS argv[1] mais nom apres rename
        // Check if the clone is empty
        size_t size_repo_path = size_main_folder_path + strlen(repo_name) + 2;
        char repo_path[size_repo_path];
        snprintf(repo_path, size_repo_path, "%s%s", main_folder_path, repo_name);


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

    if (repo_exists_or_filled){
        printf("\033[1mWhat should be done ?\033[0m\n\n");

        const char* choices[] = {
            "Add the tree structure missing files/folders.",
            "Do not change the content of the repo.",
            "Erase the repo content and create the expected tree structure."
        };
        switch(ChoiceMCQ(choices, 3)){
            case 0:
                __AddGivenFilesUsefulParts(repo_path, argv[1]);

                __CreateSubjectElements(repo_path, argv[1]);
                break;
            case 1:
                break;
            case 2:
                if (CleanFolder(repo_path))
                    errx(EXIT_FAILURE, "ERROR Impossible to erase the datas of the folder %s, check the permissions.\n", repo_path);

                __UncompressGivenFiles(repo_path, argv[1]);

                __CreateSubjectElements(repo_path, argv[1]);
                break;
            default:
                errx(EXIT_FAILURE, "ERROR Impossible to get the choice");
        }
    }
    else{
        __UncompressGivenFiles(repo_path, argv[1]);

        __CreateSubjectElements(repo_path, argv[1]);
    }


    // fonctions :
        // OK 1 - clean le repo
        // OK 1 - unzip given file
        // OK 3 - unzip given file et moove si pas deja dans repo -> del le reste
        // OK 5 - creer éléments manquand de l'arborescence du sujet
        // 2 - rename le repo (si pas déjà rename)

    // next step :
        // 4 - creer si necessaire repo git root
        // 2 - push sur repo root
        // 2 - push sur repo local


    free(main_folder_path);

    return EXIT_SUCCESS;
}

// Get the given files
// 0 : no given files | 1 : given files downloaded | else : error
int __GivenFilesHandling(char* repo_name){
    printf("Checking for given files...\n");
    int given_files = GetGivenFiles(repo_name);

    if (given_files != 0 && given_files != 1){
        printf("Second try to check for given files...\n");
        given_files = GetGivenFiles(repo_name);
    }
    
    if (given_files == 0)
        return 1;
    else if (given_files == 1)
        return 0;
    errx(-1, "ERROR Impossible to get the given files (failed twice). [Retry or login again]");
}


// Get the subject
int __SubjectHandling(char* repo_name){
    printf("Trying to get the subject...\n");

    int subject_result = GetSubject(repo_name);

    if (subject_result){
        printf("Second try to get the subject...\n");
        subject_result = GetSubject(repo_name);
    }

    if (subject_result)
        errx(EXIT_FAILURE, "ERROR Impossible to get the subject (failed twice). [Retry or login again]");

    printf("\033[32mSubject downloaded.\033[0m\n");
    return EXIT_SUCCESS;
}

int __CreateSubjectElements(char *repo_path, char *repo_name){
    __SubjectHandling(repo_name);

    char subject_path[SIZE_OF_STRING + 20];
    snprintf(subject_path, SIZE_OF_STRING, "%s/%s", GetSubjectFolderPath(), "subject.html");

    FILE *fp = fopen(subject_path, "r");
    if (!fp)
        errx(EXIT_FAILURE, "ERROR Impossible to read the subject.");

    
    if (chdir(repo_path) != 0)
        errx(EXIT_FAILURE, "ERROR Impossible to navigate through folders.");


    char line[4096];
    int started = 0;
    int previous_num_layer = 0;
    char name[SIZE_OF_STRING];

    while (fgets(line, sizeof(line), fp))
    {
        char *p = line;
        int num_layer = 0;

        while (1){
            if (*p == ' ' || *p == '\t')
                p++;
            else if (strncmp(p, "├", strlen("├")) == 0){
                p += strlen("├");
                num_layer++;
            }
            else if (strncmp(p, "│", strlen("│")) == 0){
                p += strlen("│");
                num_layer++;
            }
            else if (strncmp(p, "└", strlen("└")) == 0){
                p += strlen("└");
                num_layer++;
            }
            else if (strncmp(p, "─", strlen("─")) == 0){
                p += strlen("─");
            }
            else
                break;
        }


        if (num_layer > 0){
            if (started){
                // Create previous element
                if (access(name, F_OK)){

                    if (num_layer > previous_num_layer)
                        mkdir(name, 0755);
                    else{
                        FILE *f = fopen(name, "w");
                        if (f)
                            fclose(f);
                    }
                }
                
                
                if (num_layer > previous_num_layer){
                    if (chdir(name) != 0)
                        errx(EXIT_FAILURE, "ERROR Impossible to navigate through folders.");
                }

                // Go in the right folder to create this element
                while (num_layer < previous_num_layer){
                    previous_num_layer--;

                    if (chdir("..") != 0)
                        errx(EXIT_FAILURE, "ERROR Impossible to navigate through folders.");
                }
            }
            else
                started = 1;
            
            previous_num_layer = num_layer;

            // Extract the name of the current element
            size_t i = 0;
            while (*p != '\n' && *p != ' ' && *p != '\t' && strncmp(p, "├", strlen("├")) 
                    && strncmp(p, "│", strlen("│"))  && strncmp(p, "└", strlen("└"))){

                name[i++] = *(p++);
            }
            name[i] = 0;
        }
        else if (started)
            break;
    }

    fclose(fp);

    char rm_suject[SIZE_OF_STRING + 30];
    snprintf(rm_suject, sizeof(rm_suject), "rm -f %s", subject_path);

    if (system(rm_suject))
        errx(EXIT_FAILURE, "ERROR Impossible to remove the subject.");

    return EXIT_SUCCESS;
}

int __UncompressGivenFiles(char *folder_path, char* repo_name){
    if (__GivenFilesHandling(repo_name) == 0)
        return EXIT_FAILURE;

    const char* subject_path = GetSubjectFolderPath();

    // Uncompress
    char command_unzip[SIZE_OF_STRING];
    snprintf(command_unzip, SIZE_OF_STRING, "tar -xvf %s/%s -C %s/", subject_path, GIVEN_FILES, folder_path);

    if (system (command_unzip))
        errx(EXIT_FAILURE, "ERROR Impossible to uncompress given files.");


    // Remove Tar file
    char command_remove[SIZE_OF_STRING];
    snprintf(command_remove, SIZE_OF_STRING, "rm -rf %s/%s", subject_path, GIVEN_FILES);

    if (system (command_remove))
        errx(EXIT_FAILURE, "ERROR Impossible to remove given files zip file.");
    
    return EXIT_SUCCESS;
}

int __AddGivenFilesUsefulParts(char *folder_path, char* repo_name){
    // Create temp
    char temp_folder[SIZE_OF_STRING];
    snprintf(temp_folder, SIZE_OF_STRING, "%s/%s", GetSubjectFolderPath(), TEMPORARY_FOLDER);

    mkdir(temp_folder, 0755);

    // Copy given files elements
    if (__UncompressGivenFiles(temp_folder, repo_name) == 0){
        __GivenFilesCopy(temp_folder, folder_path);
    }

    // Delete temp
    char remove_temp_command[SIZE_OF_STRING + 8];
    snprintf(remove_temp_command, sizeof(remove_temp_command), "rm -rf %s", temp_folder);

    if (system (remove_temp_command))
        errx(EXIT_FAILURE, "ERROR Impossible to delete folders.");
    
    return EXIT_SUCCESS;
}

int __GivenFilesCopy(char *path_folder, char *targer_folder){
    DIR *dir;
    struct dirent *entry;

    dir = opendir(path_folder);

    if (dir == NULL) {
        errx(EXIT_FAILURE, "ERROR Impossible to open folder %s", path_folder);
    }

    struct stat st;
    char path_temp[1024];
    char path_target[1024];

    while ((entry = readdir(dir)) != NULL) {

        // Ignore ./ and ../
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(path_temp, sizeof(path_temp), "%s/%s", path_folder, entry->d_name);

        snprintf(path_target, sizeof(path_target), "%s/%s", targer_folder, entry->d_name);

        if (stat(path_temp, &st) == 0){
            if (S_ISDIR(st.st_mode)){
                // Folder case
                if (access(path_target, F_OK)){
                    mkdir(path_target, 0755);
                }
                __GivenFilesCopy(path_temp, path_target);
            } else{
                // File case
                if (access(path_target, F_OK)){
                    FILE *f = fopen(path_target, "w");
                        if (f)
                            fclose(f);
                }
            }
        }
    }

    closedir(dir);
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

// Get the path to the main folder or create it
// Example : ~/Jarvis/TPs/
char* __GetOrCreateMainFolderPath(){
    char* main_folder_path;

    int is_main_folder_created = ReadInfo(NAME_PATH_INFO_FILE, &main_folder_path);

    if (is_main_folder_created == EXIT_FAILURE){
        if (mkdir(MAIN_FOLDER, 0755) && !opendir(MAIN_FOLDER)){
            errx(EXIT_FAILURE, "ERROR Impossible to create the folder : %s", MAIN_FOLDER);
        }

        char *path = getcwd(NULL, 0);
        if (path == NULL)
            return NULL;
        
        size_t path_size = 0;
        while (path[path_size] != '\0')
            path_size++;

        size_t main_folder_size = 0;
        while (MAIN_FOLDER[main_folder_size])
            main_folder_size++;

        size_t total_size = path_size + main_folder_size + 3;

        path = realloc(path, total_size);

        char* final_path = malloc(total_size);
        snprintf(final_path, total_size, "%s/%s/", path, MAIN_FOLDER);

        free(path);
        
        WriteInfo(NAME_PATH_INFO_FILE, final_path);

        return final_path;
    }

    return main_folder_path;
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