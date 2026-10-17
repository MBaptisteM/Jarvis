#include "subject_handling.h"
#include <ctype.h>

// Get the subject
int SubjectDownload(char *repo_name)
{
    // printf("Trying to get the subject...\n");

    int subject_result = GetSubject(repo_name);

    if (subject_result)
    {
        // printf("Second try to get the subject...\n");
        subject_result = GetSubject(repo_name);
    }

    if (subject_result)
        return EXIT_FAILURE;

    // printf("\033[32mSubject downloaded.\033[0m\n");
    return EXIT_SUCCESS;
}

int CreateSubjectElements(char *repo_path, pid_t pid)
{
    int status;
    if (waitpid(pid, &status, 0) < 0 || !WIFEXITED(status)
        || WEXITSTATUS(status) != EXIT_SUCCESS)
        return EXIT_FAILURE;
    UiSuccess("Subject downloaded.");

    char subject_path[SIZE_OF_STRING + 20];
    snprintf(subject_path, SIZE_OF_STRING, "%s/%s", GetSubjectFolderPath(),
             "subject.html");

    FILE *fp = fopen(subject_path, "r");
    if (!fp)
        return EXIT_FAILURE;

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

        while (1)
        {
            if (*p == ' ' || *p == '\t')
                p++;
            else if (strncmp(p, "├", strlen("├")) == 0)
            {
                p += strlen("├");
                num_layer++;
            }
            else if (strncmp(p, "│", strlen("│")) == 0)
            {
                p += strlen("│");
                num_layer++;
            }
            else if (strncmp(p, "└", strlen("└")) == 0)
            {
                p += strlen("└");
                num_layer++;
            }
            else if (strncmp(p, "─", strlen("─")) == 0)
            {
                p += strlen("─");
            }
            else
                break;
        }

        if (num_layer > 0)
        {
            if (started)
            {
                // Create previous element
                if (access(name, F_OK))
                {
                    if (num_layer > previous_num_layer)
                        mkdir(name, 0755);
                    else
                    {
                        FILE *f = fopen(name, "w");
                        if (f)
                            fclose(f);
                    }
                }

                if (num_layer > previous_num_layer)
                {
                    if (chdir(name) != 0)
                        errx(EXIT_FAILURE,
                             "ERROR Impossible to navigate through folders.");
                }

                // Go in the right folder to create this element
                while (num_layer < previous_num_layer)
                {
                    previous_num_layer--;

                    if (chdir("..") != 0)
                        errx(EXIT_FAILURE,
                             "ERROR Impossible to navigate through folders.");
                }
            }
            else
                started = 1;

            previous_num_layer = num_layer;

            // Extract the name of the current element
            size_t i = 0;
            while (*p != '\n' && *p != ' ' && *p != '\t'
                   && strncmp(p, "├", strlen("├"))
                   && strncmp(p, "│", strlen("│"))
                   && strncmp(p, "└", strlen("└")))
            {
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

char *GetSubjectModules(char *subject_path)
{
    FILE *file = fopen(subject_path, "r");

    if (!file)
        return NULL;

    char buffer[1024 + 1];
    int found = 0;
    char *content_h2 = NULL;

    while (fgets(buffer, 1024, file))
    {
        char *start;

        if (!found)
        {
            start = strstr(buffer, "<h2>");

            if (start)
            {
                found = 1;
                start += 4;

                char *end = strstr(start, "</h2>");

                // h2 fully in the buffer
                if (end)
                {
                    *end = 0;
                    content_h2 = start;
                    break;
                }

                else
                {
                    fclose(file);
                    return NULL;
                }
            }
        }
    }

    if (!found)
    {
        fclose(file);
        return NULL;
    }

    char *c = content_h2;

    char *modules = malloc(512);
    if (modules == NULL)
    {
        fclose(file);
        err(EXIT_FAILURE, "malloc");
    }
    size_t i = 0;

    int inside_tag = 0;
    for (size_t j = 0; c[j] != '\0'; j++)
    {
        unsigned char character = (unsigned char)c[j];
        if (character == '<')
        {
            inside_tag = 1;
            continue;
        }
        if (inside_tag)
        {
            if (character == '>')
                inside_tag = 0;
            continue;
        }
        if (isspace(character))
            continue;
        if (i >= 511)
        {
            free(modules);
            fclose(file);
            return NULL;
        }
        modules[i++] = (char)(character == ',' ? '-' : tolower(character));
    }
    if (inside_tag || i == 0)
    {
        free(modules);
        fclose(file);
        return NULL;
    }
    modules[i] = '\0';

    fclose(file);
    return modules;
}