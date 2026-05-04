#include <get_subject.h>

// log and save the subject
int GetSubject(char* repo_name){

    const char* local_url_repo = GetLocalUrlRepo(repo_name);

    char cmd[1024];

    const char* subject_path = GetSubjectPath();

    char auth_path[512];
    snprintf(auth_path, sizeof(auth_path), "%s/auth.json", subject_path);

    // If not already login
    if (access(auth_path, F_OK) != 0) {
        Auth();
    }

    // Session connected, load the subject
    snprintf(cmd, sizeof(cmd),
        "%s/./setup_get_subject run \"%s%s\" %s", subject_path, URL, local_url_repo, subject_path);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        errx(EXIT_FAILURE, "ERROR Impossible to load the subject.");
    }

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), fp)) {
        printf("%s", buffer);
    }

    pclose(fp);
    return EXIT_SUCCESS;
}

// Authentification to forge
int Auth(){
    char cmd[1024];

    const char* subject_path = GetSubjectPath();

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

            i = 0;
            actual_character++;

            while (*actual_character != 0 && *actual_character != '/'){
                tp_id[i] = *actual_character;
                actual_character++;
                i++;
            }

            break;
        }

        if (*actual_character != BEGINING_REPO_LOCAL_PATH[0])
            actual_character++;
    }

    if (*actual_character == 0)
        errx(EXIT_FAILURE, "ERROR %s is not an EPITA repo.", repo_name);


    static char local_url_repo[1024];
    snprintf(local_url_repo, 1024, "%s/%s/root/%s/EMBEDDED_subject.html", type_course, tp_id, tp_id);

    return local_url_repo;
}