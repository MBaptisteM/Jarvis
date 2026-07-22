#include "TPs_handler.h"

// Get the path to the main folder or create it
// Example : ~/Jarvis/TPs/
char* CreateTPsPath(){
    if (mkdir(TPs_FOLDER_NAME, 0755) && !opendir(TPs_FOLDER_NAME))
        errx(EXIT_FAILURE, "ERROR Impossible to create the folder : %s", TPs_FOLDER_NAME);

    char command_mark[SIZE_OF_STRING];
    snprintf(command_mark, sizeof(command_mark), "touch %s/%s", TPs_FOLDER_NAME, MARKED_FILE_NAME);

    if (system(command_mark))
        errx(EXIT_FAILURE, "ERROR Impossible to create the file : %s/%s", TPs_FOLDER_NAME, MARKED_FILE_NAME);

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



// 0 : create | 1 : already created | 2 : cloned
char* GetOrCreateRepoRoot(){
    char* path = GetTPsPath();

    if (path == NULL){
        // Already exist check
        char cmd_exist[SIZE_OF_STRING];
        snprintf(cmd_exist, sizeof(cmd_exist),
                "gh repo view \"%s\" > /dev/null 2>&1",
                REPO_NAME);

        if (system(cmd_exist) == 0){
            snprintf(cmd_exist, sizeof(cmd_exist),
                "gh repo clone \"%s\"",
                REPO_NAME);
            if (system(cmd_exist))
                errx(EXIT_FAILURE, "ERROR Impossible to clone the root repo");

            char* cwd = getcwd(NULL, 0);
            char* new_path = malloc(strlen(cwd) + strlen(REPO_NAME) + 5);
            snprintf(new_path, strlen(cwd) + strlen(REPO_NAME) + 2, "%s/%s/", cwd,  REPO_NAME);

            printf("<FIRST|%s>\n", new_path);

            return new_path;
        }

        path = CreateTPsPath();

        InstallGH();

        if (chdir(path) != 0)
            err(EXIT_FAILURE, "chdir(%s)", path);

        // Initialise
        if (access(".git", F_OK) != 0)
            __RunCommand("git init -b main");

        // Auth GitHub
        if (system("gh auth status > /dev/null 2>&1") != 0)
        {
            printf("GitHub authentication required.\n");

            if (system("gh auth login") != 0)
                errx(EXIT_FAILURE, "GitHub authentication failed");
        }

        // README.md
        FILE *f = fopen("README.md", "w");
        if (!f)
            err(EXIT_FAILURE, "README.md");

        fputs(README_CONTENT, f);
        fclose(f);

        // .gitignore
        f = fopen(".gitignore", "w");
        if (!f)
            err(EXIT_FAILURE, ".gitignore");

        fputs(GITIGNORE_CONTENT, f);
        fputs("\n**/.git\n", f);
        fclose(f);

        // Add files
        if (system(
            "find . "
            "-path './.git' -prune -o "
            "-path '*/.git' -prune -o "
            "-type f -print0 | xargs -0 git add -- "
            "> /dev/null 2>&1"
        )){}

        // Commit if necessary
        if (system("git diff --cached --quiet") != 0)
            __RunCommand("git commit -m \"Initial commit\"");

        char command[4096];

        snprintf(
            command,
            sizeof(command),
            "gh repo create \"%s\" "
            "--private "
            "--source=. "
            "--remote=origin "
            "--push",
            REPO_NAME
        );

        __RunCommand(command);
        printf("<TOP|%s>\n", path);
    }
    printf("<%s>\n", path);
    return path;
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

// Rename .git.backup to .git
void RestoreGitFolders(){
    if (system(
    "find . -type d -name '.temp_renames_git' "
    "-exec sh -c 'mv \"$1\" \"$(dirname \"$1\")/.git\"' _ {} \\;"
    "> /dev/null 2>&1"
    )){}
}




void CleanupOnExit(){
    // Wait for sub process
    while (waitpid(-1, NULL, WNOHANG) > 0);
    
    // Reset the terminal
    if (system("stty sane") == -1)
        errx(EXIT_FAILURE, "ERROR Impossible to reset the terminal");
}

int AddRepoRoot(){
    atexit(CleanupOnExit);

    char *path = GetOrCreateRepoRoot();

    if (chdir(path) != 0)
        err(EXIT_FAILURE, "chdir(%s)", path);

    // Rename .git
    if (system(
        "find . -type d -name '.git' ! -path './.git' "
        "-exec sh -c 'mv \"$1\" \"$(dirname \"$1\")/.temp_renames_git\"' _ {} \\; "
        "> /dev/null 2>&1"
    )) {}

    // Add
    if (system("git add .")){
        printf("\033[31mWARNING Impossible to add elements in the root repo\033[0m\n");
        RestoreGitFolders();
        return EXIT_FAILURE;
    }

    // Restore
    RestoreGitFolders();

    return EXIT_SUCCESS;
}

int PushRepoRoot(char* commit_name){
    char *path = GetOrCreateRepoRoot();

    if (chdir(path) != 0)
        err(EXIT_FAILURE, "chdir(%s)", path);

    // Commit
    char commit_command[SIZE_OF_STRING + strlen(commit_name)];
    snprintf(commit_command, SIZE_OF_STRING + strlen(commit_name), "git commit -m \"%s\" > /dev/null", commit_name);
    if (system(commit_command)){
        printf("\033[31mWARNING Impossible to commit the root repo\033[0m\n");
        return EXIT_FAILURE;
    }


    // Push
    if(system("git push")){
        printf("\033[31mWARNING Impossible to push the root repo\033[0m\n");
        return EXIT_FAILURE;
    }

    printf("\033[32mRepo root sucessfuly pushed\033[0m\n");

    return EXIT_SUCCESS;
}

int PushRepo(char* repo_path, char* commit_name, char* tag){
    atexit(CleanupOnExit);

    if (repo_path != NULL && chdir(repo_path) != 0)
        return -1;
    
    char current_folder[SIZE_OF_STRING * 2];
    if (getcwd(current_folder, sizeof(current_folder)) != NULL)
        WriteInfo("current", current_folder);


    // Add
    if(system("git add . ")){
        printf("\033[31mWARNING Impossible to add elements in the repo\033[0m\n");
        return EXIT_FAILURE;
    }

    // Tags
    if (tag != NULL){
        char tag_command[SIZE_OF_STRING + strlen(tag)];
        snprintf(tag_command, SIZE_OF_STRING + strlen(tag), "git tag -ma %s", tag);

        char* error_message;
        int i = 0;
        while (system(tag_command) && i < 50){
            if (i == 0)
                error_message = "\033[31mWARNING Impossible to add the tags of the repo\033[0m\n";
            i++;

            __AddOneToTag(&tag);

            snprintf(tag_command, SIZE_OF_STRING + strlen(tag), "git tag -ma %s", tag);
        }
        if (i == 50){
            printf("%s", error_message);
            return EXIT_FAILURE;
        }   
    }

    // Commit
    char commit_command[SIZE_OF_STRING + strlen(commit_name)];
    snprintf(commit_command, SIZE_OF_STRING + strlen(commit_name), "git commit -m \"%s\"", commit_name);
    if(system(commit_command)){
        printf("\033[31mWARNING Impossible to commit the repo\033[0m\n");
        return EXIT_FAILURE;
    }

    // Push
    if((tag == NULL && system("git push")) || 
        (tag != NULL && system("git push --tags"))){
        printf("\033[31mWARNING Impossible to push the repo\033[0m\n");
        return EXIT_FAILURE;
    }


    printf("\033[32mRepo sucessfuly pushed\033[0m\n");

    return EXIT_SUCCESS;
}

void __RunCommand(const char *command)
{
    int ret = system(command);

    if (ret != 0)
        errx(EXIT_FAILURE, "ERROR while running : %s", command);
}


void __AddOneToTag(char** tag){
    char* c = *tag;

    char stubborn[SIZE_OF_STRING] = "";

    int i = 0;
    char num_str[SIZE_OF_STRING];
    while (*c != 0){
        if (*c != '-'){
            num_str[i++] = *c;
        }
        else{
            num_str[i] = 0;
            i = 0;
            strcat(stubborn, num_str);
            strcat(stubborn, "-");
        }
           
        c++;
    }
    num_str[i] = 0;

    i = 0;

    int num = 0;
    while (num_str[i] != 0){
        num = num * 10 + num_str[i++] - '0';
    }
    num ++;

    // free(*tag);
    // *tag = malloc(strlen(stubborn) + log10(num) + 2);
    snprintf(*tag, strlen(stubborn) + log10(num) + 2, "%s%d", stubborn, num);
}


void MergeFodldersClone(char* Tps_path, char* clone_path){
    Queue q ={0};

    Enqueue(&q, Tps_path);

    while (q.front){
        char *current_dir = Dequeue(&q);

        if (!current_dir)
            continue;

        DIR *dir = opendir(current_dir);

        if (!dir){
            free(current_dir);
            continue;
        }

        struct dirent *entry;

        while ((entry = readdir(dir)) != NULL){
            // Ignore . and ..
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            char full_path[MAX_PATH_SIZE];

            snprintf(full_path, sizeof(full_path), "%s/%s",
                    current_dir, entry->d_name);

            // Folder cloned
            if (strcmp(entry->d_name, REPO_NAME) == 0)
                continue;

            // Check if present in clone_folder else add it
            char path_in_clone[MAX_PATH_SIZE];
            snprintf(path_in_clone, MAX_PATH_SIZE,
                    "%s/%s", clone_path, entry->d_name);

            if (access(path_in_clone, F_OK) != 0){
                if (IsDirectory(full_path))
                    mkdir(path_in_clone, 0755);
                else{
                    FILE *f = fopen(path_in_clone, "w");
                    if (f)
                        fclose(f);
                }
            }

            // Add subfolders
            if (IsDirectory(full_path)){
                Enqueue(&q, full_path);
            }
        }

        closedir(dir);
        free(current_dir);
    }
}