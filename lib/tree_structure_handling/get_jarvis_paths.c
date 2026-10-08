#include <get_jarvis_paths.h>

// Get the commands_path depending on how the tool has been installed
const char *GetCommandPath(){

    char *home_path;
    if (__GetRootPath(&home_path) && __GetLocalPath(&home_path)){
        if (access("./commands", F_OK) == 0)
            return "./commands";
        else
            errx(EXIT_FAILURE, "ERROR Impossible to find the path to jarvis commands, try to install again.");
    }  

    static char commands_path[512];
    snprintf(commands_path, sizeof(commands_path), "%s/cmd", home_path);

    free(home_path);

    return commands_path;
}

// Get the subject_path depending on how the tool has been installed
const char *GetSubjectFolderPath(){

    char *home_path;
    if (GetDotJarvisPath(&home_path)){
        if (access("./subject", F_OK) == 0) 
            return "./subject";
        else
            errx(EXIT_FAILURE, "ERROR Impossible to find the path to jarvis commands, try to install again.");
    }

    static char subject_path[512];
    snprintf(subject_path, sizeof(subject_path), "%s/subject", home_path);

    free(home_path);

    return subject_path;
}

// Get path of the info file
char *GetInfoPath(){

    char *home_path = NULL;
    if (GetDotJarvisPath(&home_path))
        errx(EXIT_FAILURE, "ERROR Impoossible to find home path.");

    char* info_file_full_path = malloc(512);
    snprintf(info_file_full_path, 512, 
            "%s/%s", home_path, INFORMATION_FILE_NAME);
    free(home_path);
    
    return info_file_full_path;
}


// Get path of the main folder TPs (root folder) | NULL if not found
char *GetTPsPath(){
    char *TPs_path = NULL;

    if (!ReadInfo(NAME_PATH_INFO_FILE, &TPs_path) && access(TPs_path, F_OK) == 0)
        return TPs_path;

    TPs_path = FindMarkedFileBFS();
    if (TPs_path == NULL)
        return TPs_path;

    strcat(TPs_path, "/");
    WriteInfo(NAME_PATH_INFO_FILE, TPs_path);

    return TPs_path;
}



// Get the path to the system-wide Jarvis installation
int __GetRootPath(char** path){
    if (access("/usr/local/lib/jarvis", F_OK) == 0) {
        *path = malloc(512);
        if (!*path) 
            return EXIT_FAILURE;

        snprintf(*path, 512, "%s", "/usr/local/lib/jarvis");
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}

// Get the path to the user-local Jarvis installation
int __GetLocalPath(char** path){
    char *home = getenv("HOME");
    if (home) {
        *path = malloc(512);

        if (!*path) 
            return EXIT_FAILURE;

        snprintf(*path, 512, "%s/.local/lib/jarvis", home);
        
        if (access(*path, F_OK) == 0)
            return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}

// Prefer the XDG config directory when it already exists.
int GetDotJarvisPath(char** path){
    char *home = getenv("HOME");
    if (home) {
        *path = malloc(512);

        if (!*path) 
            return EXIT_FAILURE;

        char config_path[512];
        struct stat config_stat;
        snprintf(config_path, sizeof(config_path), "%s/.config", home);
        if (stat(config_path, &config_stat) == 0 && S_ISDIR(config_stat.st_mode))
            snprintf(*path, 512, "%s/.config/.jarvis", home);
        else
            snprintf(*path, 512, "%s/.jarvis", home);
        
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}
