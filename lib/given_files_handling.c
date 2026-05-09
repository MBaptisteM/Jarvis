#include "given_files_handling.h"



// Get the given files
// 0 : no given files | 1 : given files downloaded | else : error
int GivenFilesDownload(char* repo_name){
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


int AddGivenFilesUsefulParts(char *folder_path, char* repo_name){
    // Create temp
    char temp_folder[SIZE_OF_STRING];
    snprintf(temp_folder, SIZE_OF_STRING, "%s/%s", GetSubjectFolderPath(), TEMPORARY_FOLDER);

    mkdir(temp_folder, 0755);

    // Copy given files elements
    if (UncompressGivenFiles(temp_folder, repo_name) == 0){
        __GivenFilesCopy(temp_folder, folder_path);
    }

    // Delete temp
    char remove_temp_command[SIZE_OF_STRING + 8];
    snprintf(remove_temp_command, sizeof(remove_temp_command), "rm -rf %s", temp_folder);

    if (system (remove_temp_command))
        errx(EXIT_FAILURE, "ERROR Impossible to delete folders.");
    
    return EXIT_SUCCESS;
}




int UncompressGivenFiles(char *folder_path, char* repo_name){
    if (GivenFilesDownload(repo_name) == 0)
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

    while ((entry = readdir(dir)) != NULL){

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
            } 
            else{
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