#include "push.h"

int main(int argc, char* argv[]){
    char* path = NULL;

    if (argc > 1){
        path = argv[1];

        // Case root
        if (strcasecmp(path, "root") == 0){
            if (CreateRepoRoot())
                return PushRepoRoot();
            return EXIT_SUCCESS;
        }
    }


    // Push root and local repo at the same time
    pid_t pid_push_root = fork();
    if (pid_push_root == 0){
        if (CreateRepoRoot())
            exit (PushRepoRoot());

        exit(EXIT_SUCCESS);
    }
    else{
        if (PushRepo(path)){
            waitpid(pid_push_root, NULL, 0);
        }
        else{
            int return_pid;
            waitpid(pid_push_root, &return_pid, 0);

            if (return_pid)
                errx(EXIT_FAILURE, "ERROR Impossible to push the repo");
        }
    }

    return EXIT_SUCCESS;
}


