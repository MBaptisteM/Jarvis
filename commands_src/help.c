#include "help.h"

int main(int argc, char* argv[]){
    if (argc < 2){
        Help();
    }
    else{
        printf("┌─ " BOLD "COMMAND : %s\n" RESET, argv[1]);

        if (strcasecmp(argv[1], COMMAND_CLONE) == 0){
            HelpClone();
        }
        else if (strcasecmp(argv[1], COMMAND_UNINSTALL) == 0){
            HelpUninstall();
        }
        else if (strcasecmp(argv[1], COMMAND_AUTH) == 0){
            HelpAuth();
        }
        else{
            printf("└! "BOLD RED"ERROR   : Command unknown."RESET"\n\n");
            Help();
        }
    }

    return EXIT_SUCCESS;
}

void Help(){
    printf("\n"BOLD"COMMAND\t\tDESCRIPTION"RESET"\n");
    printf("---------------------------------\n");

    // auth
    printf("auth\t\tCommand to authentify to an epita account.\n");

    // clone
    printf("clone\t\tCommand to clone a repo.\n");

    // uninstall
    printf("uninstall\tCommand to uninstall Jarvis.\n");

    // epita
    printf("epita\t\tNot Implemented.\n");

    printf("\n"BOLD"Type help <command> or <command> --help for more informations on a command."RESET"\n");
    printf("├─ (ex : jarvis help %s)\n", COMMAND_AUTH);
    printf("└─ (ex : jarvis %s --help)\n", COMMAND_AUTH);
}

void HelpClone(){
    printf("│\n");
    printf("│  Clone the repo.\n");
    printf("│  Handle the tree structure of all the repos.\n");
    printf("│  Create the tree structure of the TP if EPITA repo.\n");
    printf("│  Push the modifications.\n");
    printf("│  Disclaimer : 1st call may install many packages.\n");

    printf("│\n├─ " BOLD "USAGE\n" RESET);
    printf("│\n");
    printf("│  %s <repo remote>\n", COMMAND_CLONE);

    printf("│\n├─ " BOLD "EXAMPLE\n" RESET);
    printf("│\n");
    printf("│  jarvis %s firstname.name@git.forge.epita.fr:p/...\n", COMMAND_CLONE);
    printf("│\n└─\n");
}

void HelpUninstall(){
    printf("│\n");
    printf("│  Clone the repo.\n");
    printf("│  Handle the tree structure of all the repos.\n");
    printf("│  Create the tree structure of the TP if EPITA repo.\n");
    printf("│  Push the modifications.\n");

    printf("│\n├─ " BOLD "USAGE\n" RESET);
    printf("│\n");
    printf("│  %s (no argument)\n", COMMAND_UNINSTALL);
    printf("│  Then write \"y\" to confirm.\n");

    printf("│\n├─ " BOLD "EXAMPLE\n" RESET);
    printf("│\n");
    printf("│  jarvis %s\n", COMMAND_UNINSTALL);
    printf("│\n└─\n");
}

void HelpAuth(){
    printf("│\n");
    printf("│  Opens a connection window.\n");
    printf("│  Saves the connection cookies for the next uses.\n");
    printf("│  Disclaimer : 1st call may install many packages.\n");

    printf("│\n├─ " BOLD "USAGE\n" RESET);
    printf("│\n");
    printf("│  %s (no argument)\n", COMMAND_AUTH);
    printf("│  Then connect to your account.\n");
    printf("│  When you are connected, close the window.\n");

    printf("│\n├─ " BOLD "EXAMPLE\n" RESET);
    printf("│\n");
    printf("│  jarvis %s\n", COMMAND_AUTH);
    printf("│\n└─\n");
}