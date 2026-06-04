#include "subject_handling.h"

// Get the subject
int SubjectDownload(char* repo_name){
    // printf("Trying to get the subject...\n");

    int subject_result = GetSubject(repo_name);

    if (subject_result){
        // printf("Second try to get the subject...\n");
        subject_result = GetSubject(repo_name);
    }

    if (subject_result)
        errx(EXIT_FAILURE, "ERROR Impossible to get the subject (failed twice). [Retry or Auth again]");

    

    // printf("\033[32mSubject downloaded.\033[0m\n");
    return EXIT_SUCCESS;
}

// TODO faire les givens dans un autre fichier et refaire arbo puis check chaque fichier et remplacer
int CreateSubjectElements(char *repo_path, pid_t pid){
    waitpid(pid, NULL, 0);
    printf("\033[32mSubject downloaded.\033[0m\n");
    
    char subject_path[SIZE_OF_STRING + 20];
    snprintf(subject_path, SIZE_OF_STRING, "%s/%s", GetSubjectFolderPath(), "subject.html");    

    FILE *fp = fopen(subject_path, "r");
    if (!fp)
        errx(EXIT_FAILURE, "ERROR Impossible to read the subject.");

    
    if (chdir(repo_path) != 0)
        errx(EXIT_FAILURE, "ERROR Impossible to navigate through folders.");


    char line[4096];
    int started = 0;
    int previous_num_layer = 0;
    char name[SIZE_OF_STRING];

    while (fgets(line, sizeof(line), fp))
    {
        char *p = line;
        int num_layer = 0;

        while (1){
            if (*p == ' ' || *p == '\t')
                p++;
            else if (strncmp(p, "├", strlen("├")) == 0){
                p += strlen("├");
                num_layer++;
            }
            else if (strncmp(p, "│", strlen("│")) == 0){
                p += strlen("│");
                num_layer++;
            }
            else if (strncmp(p, "└", strlen("└")) == 0){
                p += strlen("└");
                num_layer++;
            }
            else if (strncmp(p, "─", strlen("─")) == 0){
                p += strlen("─");
            }
            else
                break;
        }


        if (num_layer > 0){
            if (started){
                // Create previous element
                if (access(name, F_OK)){
                    printf("-create element\n");
                    if (num_layer > previous_num_layer)
                        mkdir(name, 0755);
                    else{
                        FILE *f = fopen(name, "w");
                        if (f)
                            fclose(f);
                    }
                }
                
                
                if (num_layer > previous_num_layer){
                    if (chdir(name) != 0)
                        errx(EXIT_FAILURE, "ERROR Impossible to navigate through folders.");
                }

                // Go in the right folder to create this element
                while (num_layer < previous_num_layer){
                    previous_num_layer--;

                    if (chdir("..") != 0)
                        errx(EXIT_FAILURE, "ERROR Impossible to navigate through folders.");
                }
            }
            else
                started = 1;
            
            previous_num_layer = num_layer;

            // Extract the name of the current element
            size_t i = 0;
            while (*p != '\n' && *p != ' ' && *p != '\t' && strncmp(p, "├", strlen("├")) 
                    && strncmp(p, "│", strlen("│"))  && strncmp(p, "└", strlen("└"))){

                name[i++] = *(p++);
            }
            name[i] = 0;
        }
        else if (started)
            break;
    }

    
    fclose(fp);

    char rm_suject[SIZE_OF_STRING + 30];
    snprintf(rm_suject, sizeof(rm_suject), "rm -f %s", subject_path);

    /*
    if (system(rm_suject))
        errx(EXIT_FAILURE, "ERROR Impossible to remove the subject.");
    */

    return EXIT_SUCCESS;
}



char* GetSubjectModules(char *subject_path){
    FILE *file = fopen(subject_path, "r");

    if (!file) {
        errx(EXIT_FAILURE, "ERROR Impossible to open the subject");
    }

    char buffer[1024 + 1];
    int found = 0;
    char* content_h2;

    while (fgets(buffer, 1024, file)){
        char *start;

        if (!found){
            start = strstr(buffer, "<h2>");

            if (start) {
                found = 1;
                start += 4;

                char *end = strstr(start, "</h2>");

                // h2 fully in the buffer
                if (end) {
                    *end = 0;
                    content_h2 = start;
                    break;
                }

                content_h2 = start;
            }
        } 
        else{
            char *end = strstr(buffer, "</h2>");

            if (end) {
                *end = 0;
                strcat(content_h2, buffer);
                break;
            }
            strcat(content_h2, buffer);
        }
    }

    if (!found)
        errx(EXIT_FAILURE, "ERROR Impossible to find the modules studied.");


    char *c = content_h2;

    char *modules = malloc(512);
    size_t i = 0;

    while (*c != 0){
        if (*c == '<')
            while (*(c++) != '>');

        if (*c == ',')
            modules[i++] = '-';
        
        else if (*c != ' '){
            if (*c >= 'A' && *c <= 'Z')
                *c -= 'A' - 'a';
        
            modules[i++] = *c;
        }
        
        c++;
    }
    modules[i] = 0;

    fclose(file);
    return modules;
}