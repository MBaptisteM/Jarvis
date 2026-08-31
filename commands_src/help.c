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
        else if (strcasecmp(argv[1], COMMAND_HELP) == 0){
            HelpAuth();
        }
        else if (strcasecmp(argv[1], COMMAND_PULL) == 0){
            HelpPull();
        }
        else if (strcasecmp(argv[1], COMMAND_FIND) == 0){
            HelpFind();
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

    // pull
    printf("pull\t\tCommand to pull and update repositories.\n");

    // find
    printf("find\t\tCommand to find specific repositories.\n");

    // uninstall
    printf("uninstall\tCommand to uninstall Jarvis.\n");

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
    printf("│  %s (no argument) : Find or clone the root repo.\n", COMMAND_CLONE);
    printf("│  %s <repo remote> : Clone the specified remote repo.\n", COMMAND_CLONE);

    printf("│\n├─ " BOLD "EXAMPLE\n" RESET);
    printf("│\n");
    printf("│  jarvis %s\n", COMMAND_CLONE);
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
    printf("│  Find or clone the root repo.\n");
    printf("│  Disclaimer : 1st call may install many packages.\n");

    printf("│\n├─ " BOLD "USAGE\n" RESET);
    printf("│\n");
    printf("│  %s (no argument) : Find or clone the root repo.\n", COMMAND_AUTH);
    printf("│  Then connect to your account.\n");
    printf("│  When you are connected, close the window.\n");

    printf("│\n├─ " BOLD "EXAMPLE\n" RESET);
    printf("│\n");
    printf("│  jarvis %s\n", COMMAND_AUTH);
    printf("│\n└─\n");
}

void HelpPull(){
    printf("│\n");
    printf("│  Pull and update repositories.\n");
    printf("│  Can be used to update the whole tree, a specific repository, or the current one.\n");

    printf("│\n├─ " BOLD "USAGE\n" RESET);
    printf("│\n");
    printf("│  %s [all]         : Pull the root repo and all repos in the tree structure (default).\n", COMMAND_PULL);
    printf("│  %s current       : Pull the current repo.\n", COMMAND_PULL);
    printf("│  %s <repo name>   : Pull the repo matching this name (if already pulled).\n", COMMAND_PULL);
    printf("│  %s <path>        : Pull the repo located at the given path.\n", COMMAND_PULL);

    printf("│\n├─ " BOLD "EXAMPLE\n" RESET);
    printf("│\n");
    printf("│  jarvis %s all\n", COMMAND_PULL);
    printf("│  jarvis %s current\n", COMMAND_PULL);
    printf("│  jarvis %s my_project\n", COMMAND_PULL);
    printf("│  jarvis %s src/utils\n", COMMAND_PULL);
    printf("│\n└─\n");
}

void HelpFind(){
    printf("│\n");
    printf("│  Find specific repositories based on context, URL, or topics.\n");

    printf("│\n├─ " BOLD "USAGE\n" RESET);
    printf("│\n");
    printf("│  %s [root]        : Find the root repo (default).\n", COMMAND_FIND);
    printf("│  %s current       : Find the current repo.\n", COMMAND_FIND);
    printf("│  %s <repo url>    : Find the local cloned repo corresponding to this URL.\n", COMMAND_FIND);
    printf("│  %s <notion>      : Find repos where this specific notion is addressed.\n", COMMAND_FIND);

    printf("│\n├─ " BOLD "EXAMPLE\n" RESET);
    printf("│\n");
    printf("│  jarvis %s root\n", COMMAND_FIND);
    printf("│  jarvis %s current\n", COMMAND_FIND);
    printf("│  jarvis %s firstname.name@git.forge.epita.fr:p/...\n", COMMAND_FIND);
    printf("│  jarvis %s \"binary trees\"\n", COMMAND_FIND);
    printf("│\n└─\n");
}