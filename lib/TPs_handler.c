#include "TPs_handler.h"

// Get the path to the main folder or create it
// Example : ~/Jarvis/TPs/
char* GetOrCreateTPsPath(){
    char* main_folder_path = GetTPsPath();

    if (main_folder_path == NULL){
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

    return main_folder_path;
}



// 0 : create | 1 : already created
int CreateRepoRoot(void)
{
    char *path = GetOrCreateTPsPath();

    InstallGH();

    if (chdir(path) != 0)
        err(EXIT_FAILURE, "chdir(%s)", path);

    // Already exist check
    if (system("git remote get-url origin > /dev/null 2>&1") == 0)
        return 1;

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

    // Ajout des fichiers
    __RunCommand(
        "find . "
        "-path './.git' -prune -o "
        "-path '*/.git' -prune -o "
        "-type f -print0 | xargs -0 git add --"
    );

    // Commit si nécessaire
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

    return 0;
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
    int ret = system("find . -type d -name '.git.backup' "
           "-exec sh -c 'mv \"$1\" \"${1%.backup}\"' _ '{}' \\;");
    (void)ret;
    
    // Wait for childs
    while (waitpid(-1, NULL, WNOHANG) > 0);
}



char *g_error_msg = NULL;

static void CleanupOnExit(){
    // Attendre tous les sous-processus
    while (waitpid(-1, NULL, WNOHANG) > 0);
    
    // Reset propre du terminal
    if (system("stty sane"))
        errx(EXIT_FAILURE, "ERROR Impossible to reset the terminal");
    
    // Réafficher le message d'erreur après le reset
    if (g_error_msg)
        fprintf(stderr, "\n%s\n", g_error_msg);
}

void PushRepoRoot(){
    atexit(CleanupOnExit);

    char *path = GetOrCreateTPsPath();

    if (chdir(path) != 0)
        err(EXIT_FAILURE, "chdir(%s)", path);

    // Rename .git
    if (system("find . -type d -name '.git' ! -path './.git' "
               "-exec sh -c 'mv \"$1\" \"$1.backup\"' _ '{}' \\;") != 0){
        g_error_msg = "ERROR Impossible to rename .git files";
        RestoreGitFolders();
        exit(EXIT_FAILURE);
    }

    // Commit
    if (system("git add .") || system("git commit -m \"Auto commit\"")){
        g_error_msg = "ERROR Impossible to commit";
        RestoreGitFolders();
        exit(EXIT_FAILURE);
    }

    // Restore
    RestoreGitFolders();

    // Push
    __RunCommand("git push");
}

void __RunCommand(const char *command)
{
    int ret = system(command);

    if (ret != 0)
        errx(EXIT_FAILURE, "ERROR while running : %s", command);
}