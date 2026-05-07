#include <get_subject.h>

// log and save the subject
int GetSubject(char* repo_name){

    const char* local_url_repo = GetLocalUrlRepo(repo_name);

    const char* subject_path = GetSubjectFolderPath();

    char auth_path[512];
    snprintf(auth_path, sizeof(auth_path), "%s/auth.json", subject_path);

    // If not already login
    if (access(auth_path, F_OK) != 0) {
        Login();
    }

    // Get subject
    char cmd_subject[1024];
    snprintf(cmd_subject, sizeof(cmd_subject),
        "%s/./setup_get_subject run \"%s%s%s\" %s", subject_path, URL, local_url_repo, SUBJECT, subject_path);

    FILE *fp_subject = popen(cmd_subject, "r");
    if (!fp_subject) {
        errx(EXIT_FAILURE, "ERROR Impossible to load the subject.");
    }
    pclose(fp_subject);

    return EXIT_SUCCESS;
}

// log and download the given files -> EXIT_FAILURE if no given file
int GetGivenFiles(char* repo_name){

    const char* local_url_repo = GetLocalUrlRepo(repo_name);

    const char* subject_path = GetSubjectFolderPath();

    char auth_path[512];
    snprintf(auth_path, sizeof(auth_path), "%s/auth.json", subject_path);

    // If not already login
    if (access(auth_path, F_OK) != 0) {
        Login();
    }
    
    // Get given files
    char cmd_given_files[1024];
    snprintf(cmd_given_files, sizeof(cmd_given_files),
        "%s/./setup_get_subject download \"%s%s%s\" %s", subject_path, URL, local_url_repo, GIVEN_FILES, subject_path);


    int status = system(cmd_given_files);
    if (status == -1) 
        errx(-1, "ERROR Impossible to look for the given files");

    int exit_code = WEXITSTATUS(status);
        
    return exit_code; // 0 : success | 1 : failure
}

// Login to forge
int Login(){
    char cmd[1024];

    const char* subject_path = GetSubjectFolderPath();

    char auth_path[512];
    snprintf(auth_path, sizeof(auth_path), "%s/auth.json", subject_path);

    snprintf(cmd, sizeof(cmd), 
        "%s/./setup_get_subject login %s %s", subject_path, URL, subject_path);

    if (system(cmd) == -1)
        errx(EXIT_FAILURE, "ERROR impossible to auth.");

    return EXIT_SUCCESS;
}

#define a 1000

// Get the forge local url from a repo name
const char* GetLocalUrlRepo(char* repo_name){
    char* actual_character = repo_name;
    size_t index = 0;
    size_t size_beginin_repo_path = strlen(BEGINING_REPO_LOCAL_PATH);

    char type_course[256];
    char tp_id[256];


    while (*actual_character != 0){
        while (index < size_beginin_repo_path && *actual_character == BEGINING_REPO_LOCAL_PATH[index]){
            index++;
            actual_character++;
        }

        if (index == size_beginin_repo_path){
            size_t i = 0;
            while (*actual_character != 0 && *actual_character != '/'){
                type_course[i] = *actual_character;
                actual_character++;
                i++;
            }
            type_course[i] = 0;

            i = 0;
            actual_character++;

            while (*actual_character != 0 && *actual_character != '/'){
                tp_id[i] = *actual_character;
                actual_character++;
                i++;
            }
            tp_id[i] = 0;

            break;
        }

        if (*actual_character != BEGINING_REPO_LOCAL_PATH[0])
            actual_character++;
    }

    if (*actual_character == 0)
        errx(EXIT_FAILURE, "ERROR %s is not an EPITA repo.", repo_name);


    static char local_url_repo[1024];

    snprintf(local_url_repo, 1024, "%s/%s/root/%s/", type_course, tp_id, tp_id);

    return local_url_repo;
}