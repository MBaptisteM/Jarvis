#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define COMMANDS_PATH "./commands/"
#define COMMAND_HELP "help"

int main(int argc, char* argv[]){
    if (argc < 2){
        char help_path[256];
        snprintf(help_path, sizeof(help_path), COMMANDS_PATH "%s", COMMAND_HELP);

        if (access(help_path, X_OK) != 0)  
            errx(EXIT_FAILURE, "ERROR you need to select a command and command %s not found", COMMAND_HELP);

        printf("ERROR you need to select a command, this is the list of all jarvis commands :\n");

        char *default_args[] = {COMMAND_HELP, NULL};
        execvp(help_path, default_args);
    }
    else{
        char command_path[256];
        snprintf(command_path, sizeof(command_path), COMMANDS_PATH "%s", argv[1]);

        if (access(command_path, X_OK) != 0)  
            errx(EXIT_FAILURE, "ERROR command %s not found", argv[1]);
            
        execv(command_path, &argv[1]);
    }

    errx(EXIT_FAILURE,"ERROR unknown error, command impossible to execute");
}