#include "TPs_handler.h"

// Get the path to the main folder or create it
// Example : ~/Jarvis/TPs/
char* GetOrCreateTPsPath(){
    char* main_folder_path;

    int is_main_folder_created = ReadInfo(NAME_PATH_INFO_FILE, &main_folder_path);

    if (is_main_folder_created == EXIT_FAILURE){
        if (mkdir(TPs_FOLDER_NAME, 0755) && !opendir(TPs_FOLDER_NAME)){
            errx(EXIT_FAILURE, "ERROR Impossible to create the folder : %s", TPs_FOLDER_NAME);
        }

        char *path = getcwd(NULL, 0);
        if (path == NULL)
            return NULL;
        
        size_t path_size = 0;
        while (path[path_size] != '\0')
            path_size++;

        size_t main_folder_size = 0;
        while (TPs_FOLDER_NAME[main_folder_size])
            main_folder_size++;

        size_t total_size = path_size + main_folder_size + 3;

        path = realloc(path, total_size);

        char* final_path = malloc(total_size);
        snprintf(final_path, total_size, "%s/%s/", path, TPs_FOLDER_NAME);

        free(path);
        
        WriteInfo(NAME_PATH_INFO_FILE, final_path);

        return final_path;
    }

    return main_folder_path;
}


// Create a private repo named REPO_NAME in root
void CreateRepoTPs(){
    char *path = "to_change";

    InstallGH();

    char command[4096];

    // git initialisation
    __RunCommand("git init");


    // Create README
    char *readme_content = "";
    snprintf(command, sizeof(command),
        "cd \"%s\" && echo \"# %s\" > README.md",
        path,
        README_CONTENT);

    __RunCommand(command);


    // Create .gitignore
    snprintf(command, sizeof(command),
        "cd \"%s\" && echo \"# %s\" > .gitignore",
        path,
        GITIGNORE_CONTENT);

    __RunCommand(command);

    
    // First Commit
    snprintf(command, sizeof(command),
        "cd \"%s\" && "
        "git add . && "
        "git commit -m \"Initial commit\"",
        path);

    __RunCommand(command);


    // Create repo + push
    snprintf(command, sizeof(command),
        "cd \"%s\" && "
        "gh repo create \"%s\" "
        "--private "
        "--source=. "
        "--remote=origin "
        "--push",
        path,
        REPO_NAME);

    __RunCommand(command);
}


void InstallGH(){
    // Check if gh exists
    if (system("command -v gh > /dev/null 2>&1") == 0)
        return;

    // Install gh
    __RunCommand("sudo apt update");
    __RunCommand("sudo apt install -y curl");

    __RunCommand("curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg "
            "| sudo dd of=/usr/share/keyrings/githubcli-archive-keyring.gpg");

    __RunCommand("sudo chmod go+r /usr/share/keyrings/githubcli-archive-keyring.gpg");

    __RunCommand("echo \"deb [arch=$(dpkg --print-architecture) "
            "signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] "
            "https://cli.github.com/packages stable main\" "
            "| sudo tee /etc/apt/sources.list.d/github-cli.list > /dev/null");

    __RunCommand("sudo apt update");
    __RunCommand("sudo apt install -y gh"); 
}

// Push the repo located on root
void PushRepoTPs(){
    char *path = "to_change";

    char command[4096];

    snprintf(command, sizeof(command),
        "cd \"%s\" && "
        "git add . && "
        "git commit -m \"Auto commit\" && "
        "git push",
        path);

    __RunCommand(command);
}


void __RunCommand(const char *command)
{
    int ret = system(command);

    if (ret != 0)
        errx(EXIT_FAILURE, "ERROR while running : %s", command);
}