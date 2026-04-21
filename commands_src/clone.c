#include "../lib/info_file.h"
#include "clone.h"


// baptiste.mahe@git.forge.epita.fr:p/epita-prepa-computer-science/prog-208-p-05-2029/
// epita-prepa-computer-science-prog-208-p-05-2029-baptiste.mahe.git

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


    size_t size_relative_path;
    char** relative_path = GetRelavitvePath(argv[1], &size_relative_path);
    for (size_t i = 0; i < size_relative_path; i++){
        printf("%s/", relative_path[i]);
        free(relative_path[i]);
    }
    printf("\n");

    free(relative_path);
    //free(size_relative_path);

    char* main_folder_path = GetOrCreateMainFolderPath();
    if (main_folder_path == NULL)
        return EXIT_FAILURE;

    printf("path main folder : %s\n", main_folder_path);
    free(main_folder_path);

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

char* GetOrCreateMainFolderPath(){
    char* main_folder_path = malloc(SIZE_OF_STRING);

    int is_main_folder_created = GetAbsolutePath(main_folder_path);

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

        size_t total_size = path_size + main_folder_size + 2;

        path = realloc(path, total_size);

        char* final_path = malloc(total_size);
        snprintf(final_path, total_size, "%s/%s", path, MAIN_FOLDER);

        free(path);
        
        SetMainFolderPath(final_path, total_size);

        return final_path;
    }

    return main_folder_path;
}


// Find a file or find or create a folder -> 0 if found / 1 if not found
int OneLayerFind(char *path, char *name, int is_folder){
    
    // Open the current directory
    DIR *dir;
    struct dirent *file;
    struct stat info;

    dir = opendir(path);

    if (dir == NULL)
        return EXIT_FAILURE;

    // Iterate throw all files and check if they are what we are looking for
    while ((file = readdir(dir)) != NULL) {
        if (stat(file->d_name, &info) == 0) {
            if (S_ISDIR(info.st_mode) && is_folder) {
                // Folder case
                if (strcmp(name, file->d_name)){
                    closedir(dir);
                    return EXIT_SUCCESS;
                }
            }
            else if (!is_folder){
                // Not a folder case
                if (strcmp(name, file->d_name)){
                    closedir(dir);
                    return EXIT_SUCCESS;
                }
            }
        }
    }

    if (is_folder){
        if (mkdir(name, 0755) && !opendir(name)){
            errx(EXIT_FAILURE, "ERROR Impossible to create the folder : %s", name);
        }
        return EXIT_SUCCESS;
    }

    closedir(dir);
    return EXIT_FAILURE;
}