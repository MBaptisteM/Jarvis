#include <stdlib.h>
#include <stdio.h>

int main(){
    printf("This is a list of all Jarvis commands :\n\n");

    // command 1
    printf("test1\n\n");

    // clone
    printf("->clone\n");
    printf("\tdescription\n");
    printf("\tExample : jarvis clone <git remote>\n\n");

    // uninstall
    printf("->uninstall\n");
    printf("\tTool to uninstall jarvis, there is no parameter to add\n");
    printf("\tExample : jarvis uninstall\n");

    return EXIT_SUCCESS;
}