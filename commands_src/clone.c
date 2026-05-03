#include "../lib/info_file.h"
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


    if (!IsEpitaRepo(argv[1])){
        printf("this string is not an epita repo\n");

        // Clone le repo

        return EXIT_SUCCESS;
    }
    

    char* main_folder_path = GetOrCreateMainFolderPath();
    if (main_folder_path == NULL)
        return EXIT_FAILURE;

    size_t size_relative_path;
    char** relative_path = GetRelavitvePath(argv[1], &size_relative_path);


    size_t size_main_folder_path = strlen(main_folder_path);

    for (size_t i = 0; i < size_relative_path; i++){
        if (OneLayerFindOrCreate(main_folder_path, relative_path[i], 1)){
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

    size_t command_size = size_main_folder_path + strlen(argv[1]) + SIZE_OF_STRING;
    char command_clone[command_size];

    snprintf(command_clone, command_size, "git -C %s clone %s", main_folder_path, argv[1]);

    if(system(command_clone))
        errx(EXIT_FAILURE, "ERROR Impossible to clone the repo");

    free(main_folder_path);


    // git clone <rep> <path>


    // Chercher les dossiers dans l'arborescence
    // Créer les dossiers si ils n'existent pas -> aller dedans sinon
    // Clone

    // Chercher le dossier download
    // Extraire les fichiers si ils existent
    // Si existe : dezip given files
    // Si existe : lire le pdf + créer dossiers/fichiers + remplir .gitignore

    // git add
    // Check gestion du github si activé
    // Clone / Push si github activé (check si clone déjà fait)
    // Envoyer l'utilisateur dans le repo

    return EXIT_SUCCESS;
}

// Check if it is an epita repo
int IsEpitaRepo(char* repo_name){
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
char** GetRelavitvePath(char *repo_name, size_t *size){
    char** relative_path = malloc(SIZE_OF_STRING);
    (*size) = 0;

    int k = 0;
    
    while (repo_name[k] < '0' ||  repo_name[k] > '9')
        k++;

    /*snprintf(relative_path, SIZE_OF_STRING, 
            "%s%d/%s%d/", SEMESTER, (repo_name[k] - '0') * 2, 
            BIMESTER, (repo_name[k + 1] - '0') * 10 + (repo_name[k + 2] - '0'));*/
        
    (*size)++;
    relative_path[0] = malloc(SIZE_OF_STRING);
    snprintf(relative_path[0], SIZE_OF_STRING, "%s%d", 
            SEMESTER, (repo_name[k] - '0') * 2);

    (*size)++;
    relative_path[1] = malloc(SIZE_OF_STRING);
    snprintf(relative_path[1], SIZE_OF_STRING, "%s%d", 
            BIMESTER, (repo_name[k + 1] - '0') * 10 + (repo_name[k + 2] - '0'));
    
    k += 3;

    (void)size;
    return relative_path;
}

// Get the path to the main folder or create it
// Example : ~/Jarvis/TPs/
char* GetOrCreateMainFolderPath(){
    char* main_folder_path = malloc(SIZE_OF_STRING);

    int is_main_folder_created = ReadInfo(NAME_PATH_INFO_FILE, main_folder_path);

    if (is_main_folder_created == EXIT_FAILURE){
        free(main_folder_path);
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
int OneLayerFindOrCreate(char *path, char *name, int is_folder){
    
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