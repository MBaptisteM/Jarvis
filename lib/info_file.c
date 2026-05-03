#include "info_file.h"

// REWRITE TO BECOME READINFO
// Return the path to the main folder
int GetAbsolutePath(char *main_folder_path){
    // vérifie l'existence du fichier -> sinon : renvoi null -> aucun chemin n'est prévu à cet effet
    // si existe check existence du dossier inscrit dans le fichier -> sinon chercher le nouveau path -> error

    char* info_file_full_path = GetInfoFullPath();
    FILE *info_file = fopen(info_file_full_path, "r");
    free(info_file_full_path);

    if (info_file == NULL){
        CreateInfoFile();
        return EXIT_FAILURE;
    }


    int i = 0;
    char actual_character;

    while ((actual_character = fgetc(info_file)) != EOF && actual_character != '\n'){
        main_folder_path[i] = actual_character;
        i++;
    }
    main_folder_path[i] = '\0';

    if (i == 0){
        fclose(info_file);
        return EXIT_FAILURE;
    }

    // Check if the path exists

    fclose(info_file);

    return EXIT_SUCCESS;
}


int CreateInfoFile(){
    char *info_file_full_path = GetInfoFullPath();
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
    char* info_file_full_path = GetInfoFullPath();
    FILE *info_file = fopen(info_file_full_path, "r");
    

    if (info_file == NULL)
        return EXIT_FAILURE;


    char new_file_full_path[512];
    char *home = getenv("HOME");
    snprintf(new_file_full_path, sizeof(new_file_full_path), 
            "%s%s%s", home, INFORMATION_FILE_PATH, "temp");
    FILE *new_file = fopen(new_file_full_path, "w");

    if (new_file == NULL)
        return EXIT_FAILURE;

    
    size_t len_key = strlen(key);

    char buffer[SIZE_OF_STRING];
    size_t index_buffer = 0;

    int data_modified = 0;

    while ( (buffer[index_buffer++] = fgetc(info_file)) != EOF ) {


        size_t index_key = 0;
        while ((buffer[index_buffer++] = fgetc(info_file)) != '"');

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
            while ((fgets(buffer, sizeof(buffer), info_file)) != EOF)
                fputs(buffer, new_file);
            
            break;
        }

        // Write the line in the new file
        while ( (buffer[index_buffer++] = fgetc(info_file)) != EOF ){
            if (buffer[index_buffer - 1] == ','){
                buffer[index_buffer] = '\0';
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

char *GetInfoFullPath(){
    char *home = getenv("HOME");
    size_t size = strlen(home) + strlen(INFORMATION_FILE_PATH) + strlen(INFORMATION_FILE_NAME) + 1;
    char* info_file_full_path = malloc(size);
    snprintf(info_file_full_path, size, 
            "%s%s%s", home, INFORMATION_FILE_PATH, INFORMATION_FILE_NAME);
    
    return info_file_full_path;
}