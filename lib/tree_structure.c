#include "tree_structure.h"


int IsEmpty(char* path){
    DIR *dir;
    struct dirent *entry;

    dir = opendir(path);

    if (dir == NULL) {
        errx(-1, "ERROR Impossible to open the folder %s", path);
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0){
            closedir(dir);
            return 0;
        }
    }

    closedir(dir);
    return 1;
}