#include "tree_structure.h"


int IsEmpty(char* path){
    DIR *dir;
    struct dirent *entry;

    dir = opendir(path);

    if (dir == NULL) {
        errx(-1, "ERROR Impossible to open the folder %s", path);
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 && strcmp(entry->d_name, ".git") != 0){
            closedir(dir);
            return 0;
        }
    }

    closedir(dir);
    return 1;
}


int CleanFolder(char *folder_path){
    DIR *dir;
    struct dirent *entry;

    dir = opendir(folder_path);

    if (dir == NULL) {
        errx(-1, "ERROR Impossible to open the folder %s", folder_path);
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 && strcmp(entry->d_name, ".git") != 0){
            char to_remove_path[1024];
            snprintf(to_remove_path, 1024, "rm -rf %s/%s", folder_path, entry->d_name);
            if (system(to_remove_path))
                errx (EXIT_FAILURE, "ERROR Impossible ro remove %s/%s", folder_path, entry->d_name);
        }
    }

    printf("Content of %s deleted successfully.\n", folder_path);

    closedir(dir);

    return EXIT_SUCCESS;
}


// Try to find the mark file, return the path if find, NULL else
char *FindMarkedFileBFS(){
    return __FindFileBFS(MARKED_FILE_NAME);
}

// Try to find the file <file_to_find>, return the path if find, NULL else
char *__FindFileBFS(const char *file_to_find){
    const char *home = getenv("HOME");

    if (!home)
        return NULL;

    Queue q ={0};

    __Enqueue(&q, home);

    while (q.front){
        char *current_dir = __Dequeue(&q);

        if (!current_dir)
            continue;

        DIR *dir = opendir(current_dir);

        if (!dir){
            free(current_dir);
            continue;
        }

        struct dirent *entry;

        while ((entry = readdir(dir)) != NULL){
            // Ignore . and ..
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            char full_path[MAX_PATH_SIZE];

            snprintf(full_path, sizeof(full_path), "%s/%s",
                    current_dir, entry->d_name);

            // File found
            if (strcmp(entry->d_name, file_to_find) == 0){
                closedir(dir);

                char *result = strdup(current_dir);

                free(current_dir);

                // Clean the queue
                while (q.front){
                    char *tmp = __Dequeue(&q);
                    free(tmp);
                }

                return result;
            }

            // Add subfolders
            if (__IsDirectory(full_path)){
                __Enqueue(&q, full_path);
            }
        }

        closedir(dir);
        free(current_dir);
    }

    return NULL;
}

// Check if the given element is a directory
int __IsDirectory(const char *path){
    struct stat st;

    if (stat(path, &st) != 0)
        return 0;

    return S_ISDIR(st.st_mode);
}

// Queue Definition for bfs
void __Enqueue(Queue *q, const char *path){
    QueueNode *node = malloc(sizeof(QueueNode));
    if (!node)
        return;

    strncpy(node->path, path, MAX_PATH_SIZE - 1);
    node->path[MAX_PATH_SIZE - 1] = '\0';
    node->next = NULL;

    if (q->rear)
        q->rear->next = node;
    else
        q->front = node;

    q->rear = node;
}

char *__Dequeue(Queue *q){
    if (!q->front)
        return NULL;

    QueueNode *tmp = q->front;

    char *path = strdup(tmp->path);

    q->front = tmp->next;

    if (!q->front)
        q->rear = NULL;

    free(tmp);

    return path;
}