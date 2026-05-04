#include <get_subject.h>

// login and save the subject
int GetSubject(char* local_url_repo){
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
        errx(EXIT_FAILURE, "ERROR : Impossible to load the subject.");
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
        errx(EXIT_FAILURE, "ERROR : impossible to auth.");

    printf("Auth completed, you can now use Jarvis!\n");
    return EXIT_SUCCESS;
}