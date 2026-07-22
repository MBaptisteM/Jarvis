#include "push.h"

int main(int argc, char* argv[]){
    char* path = NULL;

    char* name_commit = NULL;

    char* tag = NULL;

    char current_folder = 0;

    int k = 1;
    while (argc > k){
        // Case root
        if (strcasecmp(argv[k], "root") == 0){
            path = argv[k];
        }
        // Case current
        else if (strcasecmp(argv[k], "current") == 0){
            if(ReadInfo("current", &path))
                errx(EXIT_FAILURE, "ERRO trying to push the current repository but no current repository found");
            current_folder = 1;
        }
        else{
            // Case tag
            size_t i = 0;
            while (COMMAND_TAG[i] != 0 && 
                tolower(argv[k][i]) == COMMAND_TAG[i])
                i++;

            if (COMMAND_TAG[i] == 0)
                tag = &(argv[k][i]);

            else{
                // Case name
                while (COMMAND_NAME[i] != 0 && 
                    tolower(argv[k][i]) == COMMAND_NAME[i])
                    i++;

                if (COMMAND_NAME[i] == 0)
                    name_commit = &(argv[k][i]);

                // Case path
                else
                    path = argv[k];
            }
        }

        k++;
    }
    
    // Push only root is path == root
    if (path != NULL && strcasecmp(path, "root") == 0){
        if (CreateRepoRoot()){
            return AddRepoRoot() || 
            PushRepoRoot(name_commit != NULL ? 
                        name_commit : "Manual push");
        }
        return EXIT_SUCCESS;
    }

    
    // Get the path
    char path_allocated = 0;
    if (path == NULL){
        path = malloc(SIZE_OF_STRING * 2);
        path_allocated = 1;

        FILE *fp = popen("pwd", "r");
        if (!fp)
            errx(EXIT_FAILURE, "Impossible to read the console");

        if (fgets(path, SIZE_OF_STRING * 2, fp)){}

        pclose(fp);
    }

    // Get the repo name
    char repo_name[SIZE_OF_STRING];
    int i = 0;
    char* c = path;
    char* anc = NULL;
    while (*c != 0){
        if (*c != '/')
            repo_name[i++] = *c;
        else
            i = 0;
        anc = c;
        c++;  
    }
    repo_name[i] = 0;

    // Remove the last \n
    if (anc != NULL && *anc == '\n'){
        *anc = 0;
        repo_name[i - 1] = 0;
    }

    //  Get the path if folder moved
    if (current_folder && access(path, F_OK) != 0){
        if (path_allocated)
            free(path);
        path_allocated = 0;

        path = FindFileBFS(repo_name);
    }

    // Get the tag and the name_commit
    char name_commit_allocated = 0;
    if (tag != NULL){
        __GetTagID(&tag, path);
        if (name_commit == NULL)
            name_commit = tag;
    }
    else if (name_commit == NULL){
        name_commit_allocated = 1;
        __GetNameCommit(&name_commit, path);
    }
    
    // Get the name comit for the root
    char name_commit_root[SIZE_OF_STRING + strlen(name_commit)];
    snprintf(name_commit_root, sizeof(name_commit_root), 
            "%s | %s", name_commit, repo_name);


    // If repo root already created add
    int result_add = -1;
    if (system("git remote get-url origin > /dev/null 2>&1") == 0)
        result_add = AddRepoRoot();

    // Push root and local repo at the same time
    pid_t pid_push_root = fork();
    if (pid_push_root == 0){
        if (result_add == -1)
            CreateRepoRoot();
        else if (result_add == EXIT_SUCCESS)
            exit (PushRepoRoot(name_commit_root));
        
        exit(EXIT_FAILURE);
    }
    else{
        if (PushRepo(path, name_commit, tag)){}
        waitpid(pid_push_root, NULL, 0);
    }

    if (path_allocated)
        free(path);
    if (tag != NULL)
        free(tag);
    if (name_commit_allocated)
        free(name_commit);

    return EXIT_SUCCESS;
}

void __GetNameCommit(char** name_commit, char* path){
    *name_commit = malloc(SIZE_OF_STRING);
    *name_commit = "regular-push-";

    __GetId(name_commit, path);
}

void __GetTagID(char** tag, char* path){
    // Add '-' at the end of tag if not already there
    if (*tag != NULL){
        char* c = *tag;
        while (*(c + 1) != 0)
            c++;
        if (*c != '-')
            strcat(*tag, "-");
    }

    __GetId(tag, path);
}


// Generate the name of the commit
void __GetId(char** entry, char* path){
    // Get the stubborn
    char* stubborn = *entry;


    // Get the id
    char id_added = 0;

    // Check if there is an initial commit
    if (system("git rev-parse --quiet --verify HEAD >/dev/null 2>&1") == 0){

        char command_names_push[SIZE_OF_STRING + strlen(path)];
        snprintf(command_names_push, SIZE_OF_STRING + strlen(path), 
                "git -C %s --no-pager log --format=%s", path, "%s");

        FILE *fp = popen(command_names_push, "r");
        char line[1024];

        if (!fp)
            errx(EXIT_FAILURE, "Impossible to read the console");

        while (fgets(line, sizeof(line), fp) != NULL){
            // Check the stubborn
            int i = 0;
            while (line[i] != 0 && 
                (*entry)[i] != 0 &&
                tolower((*entry)[i]) == tolower(line[i]))
                i++;

            if ((*entry)[i] == 0 && line[i] != 0){
                // Add the id
                int n = (int)strtol(&(line[i]), NULL, 10) + 1;

                *entry = malloc(SIZE_OF_STRING + strlen(stubborn));

                snprintf(*entry, 
                    SIZE_OF_STRING + strlen(stubborn), 
                    "%s%d", stubborn, n);

                id_added = 1;
                break;
            }
        }

        pclose(fp);
    }

    if (!id_added){
        *entry = malloc(SIZE_OF_STRING + strlen(stubborn));
        snprintf(*entry, 
                SIZE_OF_STRING + strlen(stubborn), 
                "%s0", stubborn);
    }
}