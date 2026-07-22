#pragma once

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>


#define MAX_PATH_SIZE 4096
#define MARKED_FILE_NAME ".jarvis_mark"


int IsEmpty(char* path);
int CleanFolder(char *folder_path);


typedef struct QueueNode{
    char path[MAX_PATH_SIZE];
    struct QueueNode *next;
} QueueNode;

typedef struct{
    QueueNode *front;
    QueueNode *rear;
} Queue;

void Enqueue(Queue *q, const char *path);
char *Dequeue(Queue *q);


int IsDirectory(const char *path);
char *FindMarkedFileBFS();
char *FindFileBFS(const char *file_to_find);