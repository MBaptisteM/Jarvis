#include "tree_structure.h"


int IsEmpty(char* path){
    DIR *dir;
    struct dirent *entry;

    dir = opendir(path);

    if (dir == NULL) {
        errx(-1, "ERROR Impossible to open the folder %s", path);
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 && strcmp(entry->d_name, ".git") != 0){
            closedir(dir);
            return 0;
        }
    }

    closedir(dir);
    return 1;
}

int CleanFolder(char *folder_path){
    DIR *dir;
    struct dirent *entry;

    dir = opendir(folder_path);

    if (dir == NULL) {
        errx(-1, "ERROR Impossible to open the folder %s", folder_path);
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 && strcmp(entry->d_name, ".git") != 0){
            char to_remove_path[1024];
            snprintf(to_remove_path, 1024, "rm -rf %s/%s", folder_path, entry->d_name);
            if (system(to_remove_path))
                errx (EXIT_FAILURE, "ERROR Impossible ro remove %s/%s", folder_path, entry->d_name);
        }
    }

    printf("Content of %s deleted successfully.\n", folder_path);

    closedir(dir);

    return EXIT_SUCCESS;
}