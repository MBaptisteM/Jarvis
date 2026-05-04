#include "main.h"

// Main function that choose which command should be executed
int main(int argc, char* argv[]){

    // No command entered case : call help if it is possible
    if (argc < 2){
        char help_path[256];
        snprintf(help_path, sizeof(help_path), "%s/%s", GetCommandPath(), COMMAND_HELP);

        if (access(help_path, X_OK) != 0)  
            errx(EXIT_FAILURE, "ERROR you need to select a command and command %s not found", COMMAND_HELP);

        printf("ERROR you need to select a command\n");

        char *default_args[] = {COMMAND_HELP, NULL};
        execvp(help_path, default_args);
    }

    // A command has been entered case : call the command if it is possible
    else{
        char command_path[256];

        snprintf(command_path, sizeof(command_path), "%s/%s", GetCommandPath(), argv[1]);

        if (access(command_path, X_OK) != 0)  
            errx(EXIT_FAILURE, "ERROR command %s not found", argv[1]);
            
        execv(command_path, &argv[1]);
    }

    // Error case, impossible to call any command
    errx(EXIT_FAILURE,"ERROR unknown error, command impossible to execute");
}