#include "info_file.h"

#include <ctype.h>
#include <unistd.h>

// REWRITE TO BECOME READINFO

static const char *__InfoValueForKey(const char *line, const char *key)
{
    while (*line == '\0' || isspace((unsigned char)*line))
        line++;
    if (*line++ != '"')
        return NULL;

    size_t key_length = strlen(key);
    if (strncmp(line, key, key_length) != 0 || line[key_length] != '"')
        return NULL;
    line += key_length + 1;

    while (isspace((unsigned char)*line))
        line++;
    if (*line++ != ':')
        return NULL;
    while (isspace((unsigned char)*line))
        line++;
    if (*line++ != '"')
        return NULL;

    return line;
}

static void __RemoveNullBytes(char *line, ssize_t line_length)
{
    ssize_t output_index = 0;
    for (ssize_t input_index = 0; input_index < line_length; input_index++)
    {
        if (line[input_index] != '\0')
            line[output_index++] = line[input_index];
    }
    line[output_index] = '\0';
}

// Return the value associated to the key in entry
int ReadInfo(char *key, char **value)
{
    // vérifie l'existence du fichier -> sinon : renvoi null -> aucun chemin
    // n'est prévu à cet effet si existe check existence du dossier inscrit dans
    // le fichier -> sinon chercher le nouveau path -> error

    *value = NULL;
    char *info_file_full_path = GetInfoPath();
    FILE *info_file = fopen(info_file_full_path, "r");
    free(info_file_full_path);

    if (info_file == NULL)
    {
        __CreateInfoFile();
        return EXIT_FAILURE;
    }

    char *line = NULL;
    size_t line_capacity = 0;
    ssize_t line_length;
    while ((line_length = getline(&line, &line_capacity, info_file)) >= 0)
    {
        __RemoveNullBytes(line, line_length);
        const char *value_start = __InfoValueForKey(line, key);
        if (value_start == NULL)
            continue;

        const char *value_end = strchr(value_start, '"');
        if (value_end == NULL)
            continue;

        size_t value_length = (size_t)(value_end - value_start);
        *value = malloc(value_length + 1);
        if (*value == NULL)
        {
            free(line);
            fclose(info_file);
            err(EXIT_FAILURE, "malloc");
        }
        memcpy(*value, value_start, value_length);
        (*value)[value_length] = '\0';
        free(line);
        fclose(info_file);
        return EXIT_SUCCESS;
    }
    free(line);
    fclose(info_file);

    return EXIT_FAILURE;
}

int __CreateInfoFile()
{
    char *info_file_full_path = GetInfoPath();
    printf("Create : %s\n", info_file_full_path);
    FILE *info_file = fopen(info_file_full_path, "w");
    free(info_file_full_path);

    if (info_file == NULL)
        return EXIT_FAILURE;

    fclose(info_file);

    return EXIT_SUCCESS;
}

int WriteInfo(char *key, char *value)
{
    char *info_file_full_path = GetInfoPath();
    FILE *info_file = fopen(info_file_full_path, "r");

    if (info_file == NULL)
    {
        free(info_file_full_path);
        return EXIT_FAILURE;
    }

    size_t temporary_path_size = strlen(info_file_full_path) + sizeof(".tmp");
    char *temporary_path = malloc(temporary_path_size);
    if (temporary_path == NULL)
    {
        fclose(info_file);
        free(info_file_full_path);
        return EXIT_FAILURE;
    }
    snprintf(temporary_path, temporary_path_size, "%s.tmp", info_file_full_path);
    FILE *new_file = fopen(temporary_path, "w");

    if (new_file == NULL)
    {
        fclose(info_file);
        free(temporary_path);
        free(info_file_full_path);
        return EXIT_FAILURE;
    }

    char *line = NULL;
    size_t line_capacity = 0;
    ssize_t line_length;
    int data_modified = 0;
    int write_failed = 0;

    while ((line_length = getline(&line, &line_capacity, info_file)) >= 0)
    {
        __RemoveNullBytes(line, line_length);
        if (!data_modified && __InfoValueForKey(line, key) != NULL)
        {
            if (fprintf(new_file, "\"%s\" : \"%s\",\n", key, value) < 0)
                write_failed = 1;
            data_modified = 1;
        }
        else if (fputs(line, new_file) == EOF)
        {
            write_failed = 1;
        }
    }

    if (ferror(info_file))
        write_failed = 1;
    if (!data_modified
        && fprintf(new_file, "\"%s\" : \"%s\",\n", key, value) < 0)
        write_failed = 1;

    free(line);
    fclose(info_file);

    if (fclose(new_file) != 0)
        write_failed = 1;
    if (!write_failed && rename(temporary_path, info_file_full_path) != 0)
        write_failed = 1;
    if (write_failed)
        remove(temporary_path);

    free(temporary_path);
    free(info_file_full_path);

    return write_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}