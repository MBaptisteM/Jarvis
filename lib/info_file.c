#include "info_file.h"
#include <get_jarvis_paths.h>

// REWRITE TO BECOME READINFO

// Return the value associated to the key in entry
int ReadInfo(char* key, char **value){
    // vérifie l'existence du fichier -> sinon : renvoi null -> aucun chemin n'est prévu à cet effet
    // si existe check existence du dossier inscrit dans le fichier -> sinon chercher le nouveau path -> error

    char* info_file_full_path = GetInfoPath();
    FILE *info_file = fopen(info_file_full_path, "r");
    free(info_file_full_path);


    if (info_file == NULL){
        __CreateInfoFile();
        return EXIT_FAILURE;
    }


    size_t len_key = strlen(key);
    char actual_char;

    while ( (actual_char = fgetc(info_file)) != EOF ) {
        size_t index_key = 0;
        

        while (actual_char != EOF){
            if (actual_char == '"')
                break;
            actual_char = fgetc(info_file);
        }


        while (index_key < len_key && (actual_char = fgetc(info_file)) == key[index_key]){
            index_key ++;
        }

        if (index_key == len_key && fgetc(info_file) == '"'){
            // We are on the line that contains the key we were looking for

            while ( (actual_char = fgetc(info_file)) != EOF ){
                if (actual_char == '"')
                    break;
            }

            *value = malloc(SIZE_OF_STRING);
            size_t i = 0;
            while ( ((*value)[i++] = fgetc(info_file)) != EOF ){
                if ((*value)[i - 1] == '"')
                    break;
            }

            (*value)[i - 1] = 0;

            fclose(info_file);

            return EXIT_SUCCESS;
        }

        while ( (actual_char = fgetc(info_file)) != EOF ){
            if (actual_char == ','){
                break;
            }
        }
    }
    fclose(info_file);

    return EXIT_FAILURE;
}

int __CreateInfoFile(){
    char *info_file_full_path = GetInfoPath();
    printf("create : %s\n", info_file_full_path);
    FILE *info_file = fopen(info_file_full_path, "w");
    free(info_file_full_path);

    if (info_file == NULL)
        return EXIT_FAILURE;
    
    fclose(info_file);

    return EXIT_SUCCESS;
}


int WriteInfo(char* key, char* value){

    // Put main_folder_path at the begining of file
    char* info_file_full_path = GetInfoPath();
    FILE *info_file = fopen(info_file_full_path, "r");
    

    if (info_file == NULL)
        return EXIT_FAILURE;


    char new_file_full_path[512];
    char *home = getenv("HOME");
    snprintf(new_file_full_path, sizeof(new_file_full_path), 
            "%s%s/%s", home, NAME_JARVIS_FOLDER, "temp");
    FILE *new_file = fopen(new_file_full_path, "w");

    if (new_file == NULL)
        return EXIT_FAILURE;

    
    size_t len_key = strlen(key);

    char buffer[SIZE_OF_STRING];
    size_t index_buffer = 0;

    int data_modified = 0;

    while ( (buffer[index_buffer++] = fgetc(info_file)) != EOF ) {


        size_t index_key = 0;
        while (buffer[index_buffer - 1] != EOF){
            if (buffer[index_buffer - 1] == '"')
                break;
            buffer[index_buffer++] = fgetc(info_file);
        }

        while (index_key < len_key && (buffer[index_buffer++] = fgetc(info_file)) == key[index_key]){
            index_key ++;
        }
        if (index_key == len_key && (buffer[index_buffer++] = fgetc(info_file)) == '"'){
            // We are on the line that contains the key we were looking for
            fprintf(new_file, "\"%s\" : \"%s\",\n", key, value);

            data_modified = 1;

            // Pass the current line
            while ( (buffer[index_buffer++] = fgetc(info_file)) != EOF ){
                if (buffer[index_buffer - 1] == ',')
                    break;
            }

            // Write all the folder
            while ((fgets(buffer, sizeof(buffer), info_file)) != NULL)
                fputs(buffer, new_file);
            
            break;
        }

        // Write the line in the new file
        while ( (buffer[index_buffer++] = fgetc(info_file)) != EOF ){
            if (buffer[index_buffer - 1] == ','){
                buffer[index_buffer + 1] = 0;
                fputs(buffer, new_file);
                break;
            }
        }

        index_buffer = 0;
    }

    if (!data_modified)
        fprintf(new_file, "\"%s\" : \"%s\",\n", key, value);

    fclose(info_file);
    fclose(new_file);

    remove(info_file_full_path);
    rename(new_file_full_path, info_file_full_path);

    free(info_file_full_path);
    
    return EXIT_SUCCESS;
}