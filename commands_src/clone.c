#include "clone.h"


// Strictly parse the course id of an EPITA repository url.
static int __ParseCourseId(const char *url, int *bimester, char *stubborn,
                           size_t stubborn_size)
{
    const char *start = url;

    while (1)
    {
        const char *end = start;
        while (*end != '\0' && *end != '/' && *end != ':')
            end++;

        size_t length = (size_t)(end - start);
        size_t i = 0;
        while (i < length && isalpha((unsigned char)start[i]))
            i++;

        const char *p = start + i;
        if (i > 0 && length - i == 14 && p[0] == '-'
            && isdigit((unsigned char)p[1]) && isdigit((unsigned char)p[2])
            && isdigit((unsigned char)p[3]) && p[4] == '-'
            && isalpha((unsigned char)p[5]) && p[6] == '-'
            && isdigit((unsigned char)p[7]) && isdigit((unsigned char)p[8])
            && p[9] == '-' && isdigit((unsigned char)p[10])
            && isdigit((unsigned char)p[11]) && isdigit((unsigned char)p[12])
            && isdigit((unsigned char)p[13]))
        {
            int num_bimester = (p[2] - '0') * 10 + (p[3] - '0');
            if (num_bimester < 1)
                return EXIT_FAILURE;

            if (bimester != NULL)
                *bimester = num_bimester;

            // Core of the nomenclature, kept as is (ex : 205-p-05)
            if (stubborn != NULL && stubborn_size >= 10)
            {
                snprintf(stubborn, stubborn_size, "%c%c%c-%c-%c%c", p[1], p[2],
                         p[3], (char)tolower((unsigned char)p[5]), p[7], p[8]);
            }
            return EXIT_SUCCESS;
        }

        if (*end == '\0')
            break;
        start = end + 1;
    }

    return EXIT_FAILURE;
}

// baptiste.mahe@git.forge.epita.fr:p/epita-prepa-computer-science/prog-208-p-05-2029/epita-prepa-computer-science-prog-208-p-05-2029-baptiste.mahe.git
// https://intra.forge.epita.fr/epita-prepa-computer-science/prog-101-p-00-2029/root/prog-101-p-00-2029

int main(int argc, char *argv[])
{
    atexit(CleanupOnExit);
    char *current_repo_url = NULL;

    // Ask once if the user accepts to be counted (GitHub star)
    CounterAsk();

    // Case no argument, or "all": clone the root EPITA-TPs repository itself
    // (bootstrap), instead of a specific TP repository.
    if (argc < 2 || strcasecmp(argv[1], "all") == 0)
    {
        __CloneParentRepo();
        return EXIT_SUCCESS;
    }

    if (IsCurrentKeyword(argv[1]))
    {
        // Get the current repository (the pages are downloaded in parallel :
        // the dependencies must be installed before, once)
        if (EnsureDependencies() != EXIT_SUCCESS)
            return EXIT_FAILURE;

        UiInfo("Looking for your current repositories on the forge...");
        fflush(stdout);
        current_repo_url = GetCurrentRepo();
        if (current_repo_url == NULL)
        {
            UiError("No git url available for this repository, nothing was "
                    "cloned.");
            return EXIT_FAILURE;
        }
        argv[1] = current_repo_url;
    }

    if (!__IsEpitaRepo(argv[1]))
    {
        UiWarning("This string is not an epita repository.\n"
                  "Classic clone.");

        size_t command_size = strlen(argv[1]) + SIZE_OF_STRING;
        char command_clone[command_size];
        snprintf(command_clone, command_size,
                 "git submodule add --force \"%s\"", argv[1]);

        if (system(command_clone))
            errx(EXIT_FAILURE, "ERROR Impossible to clone the repository");

        free(current_repo_url);
        return EXIT_SUCCESS;
    }
    else
    {
        UiInfo("Repository to clone :\n%s%s", URL, GetLocalUrlRepo(argv[1]));

        // Install the dependencies here, in the foreground : the output is
        // visible and the background downloads below do not install them
        // at the same time
        if (EnsureDependencies() != EXIT_SUCCESS)
            return EXIT_FAILURE;

        // Do it here to avoid multithreading problems
        char auth_path[512];
        snprintf(auth_path, sizeof(auth_path), "%s/auth.json",
                 GetSubjectFolderPath());

        // If not already Auth
        if (access(auth_path, F_OK) != 0)
        {
            Auth();
        }
    }

    // Preload subject and given files
    UiInfo("Downloading the subject and the given files in the background...");
    fflush(stdout);
    pid_t pid_get_given_files = fork();
    if (pid_get_given_files == 0)
    {
        int given_files_return_value = GivenFilesDownload(argv[1]);

        if (given_files_return_value == -1)
            errx(EXIT_FAILURE,
                 "ERROR Impossible to get the given files, check your "
                 "connection or try to Auth again.");

        exit(given_files_return_value);
    }

    pid_t pid_get_subjects = fork();
    if (pid_get_subjects == 0)
    {
        exit(SubjectDownload(argv[1]));
    }

    // CREATE PATH
    UiInfo("Looking for the root repository (can take a few seconds)...");
    fflush(stdout);
    char *main_folder_path = GetOrCreateRepoRoot();
    if (main_folder_path == NULL)
        return EXIT_FAILURE;

    // The root path must end with '/', otherwise the relative folders
    // ("Others", "S1/B1") get glued to its name (ex : EPITA-TPsOthers)
    size_t root_length = strlen(main_folder_path);
    if (root_length == 0 || main_folder_path[root_length - 1] != '/')
    {
        char *slashed_path = realloc(main_folder_path, root_length + 2);
        if (slashed_path == NULL)
        {
            free(main_folder_path);
            err(EXIT_FAILURE, "realloc");
        }
        main_folder_path = slashed_path;
        main_folder_path[root_length] = '/';
        main_folder_path[root_length + 1] = '\0';
    }

    size_t size_relative_path;
    char **relative_path = __GetRelavitvePath(argv[1], &size_relative_path);

    size_t size_main_folder_path = strlen(main_folder_path);

    for (size_t i = 0; i < size_relative_path; i++)
    {
        if (__OneLayerFindOrCreate(main_folder_path, relative_path[i], 1))
        {
            errx(EXIT_FAILURE,
                 "ERROR Impossible to find or create the folder : %s%s",
                 main_folder_path, relative_path[i]);
        }

        size_t size_full_path =
            size_main_folder_path + strlen(relative_path[i]) + 2;
        char *resized_main_folder_path =
            realloc(main_folder_path, size_full_path);
        if (resized_main_folder_path == NULL)
        {
            free(main_folder_path);
            errx(EXIT_FAILURE,
                 "ERROR Impossible to allocate memory, not enough space. (Try "
                 "to rerun)");
        }
        main_folder_path = resized_main_folder_path;

        size_t j = 0;
        while (relative_path[i][j] != '\0')
        {
            main_folder_path[size_main_folder_path + j] = relative_path[i][j];
            j++;
        }
        main_folder_path[size_main_folder_path + j] = '/';
        main_folder_path[size_main_folder_path + j + 1] = '\0';

        free(relative_path[i]);
        size_main_folder_path = size_full_path - 1;
    }
    free(relative_path);

    // Bool to know if the repo is new or empty
    int repo_exists_or_filled = 0;
    char repo_name[SIZE_OF_STRING];

    size_t size_repo_path = size_main_folder_path + SIZE_OF_STRING + 2;
    char repo_path[size_repo_path];

    char *read_data;
    int already_cloned = ReadInfo(argv[1], &read_data) == 0;

    // format : type/id/root... | goal format : type-id-name
    const char *local_url_repo = GetLocalUrlRepo(argv[1]);

    int need_to_be_renamed = 0;

    if (already_cloned)
    {
        size_t size_old_repo_path =
            size_main_folder_path + strlen(read_data) + 2;
        char old_repo_path[size_old_repo_path];
        snprintf(old_repo_path, size_old_repo_path, "%s%s", main_folder_path,
                 read_data);

        if (access(old_repo_path, F_OK) == 0)
        {
            repo_exists_or_filled = 1;

            __PrintPages(local_url_repo);

            UiWarning("This repository already exists.");
        }

        size_t i = 0;
        while ((repo_name[i] = read_data[i]) != 0)
        {
            i++;
        }

        snprintf(repo_path, size_repo_path, "%s%s", main_folder_path,
                 repo_name);

        if (read_data[0] < 'A' || read_data[0] > 'Z')
            need_to_be_renamed = 1;

        free(read_data);
    }

    if (!repo_exists_or_filled)
    {
        need_to_be_renamed = 1;

        // Get and save the name of the repo
        size_t i = 0;
        const char *c = local_url_repo;

        // extract type
        while (*c != '/')
            repo_name[i++] = *(c++);

        c++;
        repo_name[i++] = '-';

        // extract id
        while (*c != '/')
            repo_name[i++] = *(c++);

        repo_name[i++] = '-';
        size_t j = 0;

        // extract name from name@git...
        while (argv[1][j] != '@')
            repo_name[i++] = argv[1][j++];

        repo_name[i] = 0;

        snprintf(repo_path, size_repo_path, "%s%s", main_folder_path,
                 repo_name);

        __PrintPages(local_url_repo);

        // CLONE
        UiInfo("Cloning %s...", argv[1]);
        fflush(stdout);
        size_t command_size =
            size_main_folder_path + strlen(argv[1]) + SIZE_OF_STRING;
        char command_clone[command_size];
        // snprintf(command_clone, command_size, "git -C %s clone %s",
        // main_folder_path, argv[1]);
        snprintf(command_clone, command_size,
                 "git -C \"%s\" submodule add --force \"%s\"", main_folder_path,
                 argv[1]);

        // Several clones may run at the same time ("jarvis fill") : only one
        // at a time can touch the index and .gitmodules of the root repository
        int git_lock = JarvisLockAcquire("git.lock");
        int clone_status = system(command_clone);
        JarvisLockRelease(git_lock);

        // Check if the clone is empty
        if (clone_status)
        {
            // Handle error in previous execution
            if (access(repo_path, F_OK) == 0)
            {
                WriteInfo(argv[1], repo_name);

                repo_exists_or_filled = 1;
                UiWarning("This repository already exists.");
            }
            else
                errx(EXIT_FAILURE, "ERROR Impossible to clone the repository");
        }
        else
        {
            repo_exists_or_filled =
                repo_exists_or_filled || !IsEmpty(repo_path);

            if (repo_exists_or_filled)
            {
                UiWarning("This repository is not empty.");
            }
        }
    }

    // Make a choice if the repo is already filled
    if (repo_exists_or_filled)
    {
        // "jarvis fill" clones many repositories : no question, the content
        // of a repository that is not empty is never changed
        int choice = 1;
        if (getenv("JARVIS_FILL") == NULL)
        {
            UiQuestion("What should be done ?");

            const char *choices[] = {
                "Add the tree structure missing files/folders.",
                "Do not change the content of the repo.",
                "Erase the repo content and create the expected tree "
                "structure."
            };

            choice = ChoiceMCQ(choices, 3);
        }
        else
            UiInfo("This repository is not empty, its content is kept.");

        switch (choice)
        {
        case 0:
            UiInfo("Waiting for the subject and the given files...");
            fflush(stdout);
            AddGivenFilesUsefulParts(repo_path, pid_get_given_files);

            CreateSubjectElements(repo_path, pid_get_subjects);
            break;
        case 1:
            kill(pid_get_given_files, SIGKILL);

            if (need_to_be_renamed)
            {
                UiInfo("Waiting for the subject to be downloaded...");
                fflush(stdout);
                int subject_status;
                if (waitpid(pid_get_subjects, &subject_status, 0) >= 0
                    && WIFEXITED(subject_status)
                    && WEXITSTATUS(subject_status) == EXIT_SUCCESS)
                    UiSuccess("Subject downloaded.");
            }
            else
            {
                kill(pid_get_subjects, SIGKILL);
                waitpid(pid_get_subjects, NULL, 0);
            }
            waitpid(pid_get_given_files, NULL, 0);

            break;
        case 2:
            if (CleanFolder(repo_path))
                errx(EXIT_FAILURE,
                     "ERROR Impossible to erase the datas of the folder %s, "
                     "check the permissions.\n",
                     repo_path);

            UiInfo("Waiting for the subject and the given files...");
            fflush(stdout);
            UncompressGivenFiles(repo_path, pid_get_given_files);

            CreateSubjectElements(repo_path, pid_get_subjects);
            break;
        default:
            errx(EXIT_FAILURE, "ERROR Impossible to get the choice");
        }
    }
    else
    {
        UiInfo("Waiting for the subject and the given files...");
        fflush(stdout);
        UncompressGivenFiles(repo_path, pid_get_given_files);

        CreateSubjectElements(repo_path, pid_get_subjects);
    }

    // Rename the child repository (only possible when the course id could be
    // deduced from the url, else the repo keeps its name inside "Others")
    if (need_to_be_renamed)
    {
        if (__ParseCourseId(argv[1], NULL, NULL, 0) == EXIT_SUCCESS)
        {
            UiInfo("Renaming the repository...");
            fflush(stdout);
            __RenameRepo(repo_path, argv[1]);
        }
        else
            WriteInfo(argv[1], repo_name);
    }

    char *name_repo;
    if (ReadInfo(argv[1], &name_repo))
        errx(EXIT_FAILURE, "ERROR Impossible to read Jarvis files");
    snprintf(repo_path, size_repo_path, "%s%s", main_folder_path, name_repo);

    // Save this repository as the current one
    WriteInfo("current", repo_path);

    free(name_repo);
    free(main_folder_path);

    UiInfo("Pushing the repositories to the forge...");
    fflush(stdout);

    // Push the repos (parent & child (if necessary))
    // ("jarvis fill" pushes the root repository once, after all the clones)
    pid_t pid_push_root = getenv("JARVIS_FILL") != NULL ? -2 : fork();
    if (pid_push_root == 0)
    {
        char *repo_name;
        if (ReadInfo(argv[1], &repo_name))
            errx(EXIT_FAILURE, "Impossible to read info file");

        char commit_name[strlen(repo_name) + SIZE_OF_STRING];
        snprintf(commit_name, strlen(repo_name) + SIZE_OF_STRING, "Clone %s",
                 repo_name);
        free(repo_name);

        exit(AddRepoRoot() || PushRepoRoot(commit_name));
    }
    else
    {
        int result_child = 0;
        int result_push = PushRepo(repo_path, "Tree structure", NULL);

        if (pid_push_root > 0)
            waitpid(pid_push_root, &result_child, 0);

        // Clone case
        if (result_child == 2)
        {
            // TODO
            // Copy all the files in TPs folder
            char *TPs_path = GetTPsPath();
            if (TPs_path == NULL)
                errx(EXIT_FAILURE, "ERROR Impossible to locate the TPs folder");
            char clone_path[strlen(TPs_path) + SIZE_OF_STRING];
            snprintf(clone_path, sizeof(clone_path), "%s/%s", TPs_path,
                     REPO_NAME);
            MergeFodldersClone(TPs_path, clone_path);

            // Move the clone folder
            char command_move[strlen(REPO_NAME) + SIZE_OF_STRING];
            snprintf(command_move, sizeof(command_move), "mv %s ../",
                     REPO_NAME);
            if (chdir(TPs_path) != 0 || system(command_move) != 0)
                errx(EXIT_FAILURE, "ERROR Impossible to execute commands");
            free(TPs_path);
        }

        if (result_push == -1)
            errx(EXIT_FAILURE, "ERROR %s no such file directory", repo_path);
    }

    __PrintPages(local_url_repo);

    free(current_repo_url);
    return EXIT_SUCCESS;
}

void __PrintPages(const char *local_url_repo)
{
    UiPanel(stdout, UI_INFO, "Pages",
            "Repo page :\n%s%s\n\nSubject page :\n%s%s%s", URL, local_url_repo,
            URL, local_url_repo, SUBJECT);
}

int __SameStr(char *s1, char *s2)
{
    while (*s1 && *s2
           && (tolower((unsigned char)*s1) == tolower((unsigned char)*s2)))
    {
        s1++;
        s2++;
    }
    return !*s1 && !*s2
        && tolower((unsigned char)*s1) - tolower((unsigned char)*s2) == 0;
}

int __RenameRepo(char *repo_path, char *repo_name)
{
    // STUBBORN
    char stubborn[SIZE_OF_STRING];
    size_t i = 0;

    char *c = NULL;

    int unused_bimester;
    if (__ParseCourseId(repo_name, &unused_bimester, stubborn, sizeof(stubborn))
        != EXIT_SUCCESS)
    {
        char *fallback = strrchr(repo_path, '/');
        fallback = fallback == NULL ? repo_path : fallback + 1;
        UiWarning("Impossible to deduce the course of this repository; "
                  "keeping the name %s.",
                  fallback);
        WriteInfo(repo_name, fallback);
        return EXIT_FAILURE;
    }

    // NAME
    char *raw_name = NULL;

    DIR *dir;
    struct dirent *entry;
    dir = opendir(repo_path);

    if (dir == NULL)
    {
        errx(EXIT_FAILURE, "ERROR Impossible to open folder %s", repo_path);
    }

    struct stat st;
    while ((entry = readdir(dir)) != NULL)
    {
        // Ignore ./ and ../
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0
            || strcmp(entry->d_name, ".git") == 0)
            continue;

        char path_new_element[1024];
        snprintf(path_new_element, sizeof(path_new_element), "%s/%s", repo_path,
                 entry->d_name);

        if (stat(path_new_element, &st) == 0 && S_ISDIR(st.st_mode))
        {
            // raw_name = entry->d_name;
            raw_name = malloc(SIZE_OF_STRING);
            if (raw_name == NULL)
            {
                closedir(dir);
                err(EXIT_FAILURE, "malloc");
            }
            strcpy(raw_name, entry->d_name);
            break;
        }
    }
    closedir(dir);

    if (raw_name == NULL)
    {
        UiWarning("The repository doesn't have a valid tree structure.");
        WriteInfo(repo_name, repo_name);
        return EXIT_FAILURE;
    }

    char name[SIZE_OF_STRING];
    i = 0;
    int need_upper = 1;

    c = raw_name;

    while (*c != 0)
    {
        if (need_upper && *c != '_' && *c != '-')
        {
            if (*c >= 'a' && *c <= 'z')
                name[i++] = *c + 'A' - 'a';
            else
                name[i++] = *c;

            need_upper = 0;
        }
        else
        {
            if (*c == '_' || *c == '-')
                need_upper = 1;
            else
                name[i++] = *c;
        }
        c++;
    }
    name[i] = 0;

    free(raw_name);

    // MODULES
    char subject_path[SIZE_OF_STRING + 20];
    snprintf(subject_path, SIZE_OF_STRING, "%s/%s", GetSubjectFolderPath(),
             "subject.html");

    char *modules = GetSubjectModules(subject_path);
    if (!modules)
    {
        char *fallback_name = strrchr(repo_path, '/');
        fallback_name = fallback_name == NULL ? repo_path : fallback_name + 1;
        UiWarning("Subject unavailable or invalid; keeping repository name "
                  "%s.",
                  fallback_name);
        WriteInfo(repo_name, fallback_name);
        return EXIT_SUCCESS;
    }

    // ALL TOGETHER & RENAME
    char full_name[strlen(stubborn) + strlen(name) + strlen(modules) + 20];
    snprintf(full_name, sizeof(full_name), "%s-%s-%s", stubborn, name, modules);
    free(modules);

    // char rename_command[strlen(repo_path) + sizeof(full_name) + 20];
    // snprintf(rename_command, sizeof(rename_command), "mv %s %s", repo_path,
    // full_name);

    if (chdir(repo_path) != 0 || chdir("..") != 0)
        errx(EXIT_FAILURE, "ERROR Impossible to navigate through folders");

    char *path;
    if ((path = getcwd(NULL, 0)) == NULL)
        errx(EXIT_FAILURE, "ERROR Impossible to get the current repository");

    char *root_folder;
    if (ReadRootPath(&root_folder))
        errx(EXIT_FAILURE, "ERROR Impossible to find the root repo");

    size_t root_len = strlen(root_folder);

    // 1. Rendre repo_path relatif
    char *rel_repo_path = repo_path;
    if (strncmp(repo_path, root_folder, root_len) == 0)
    {
        rel_repo_path += root_len;
        if (*rel_repo_path == '/')
            rel_repo_path++;
    }

    // 2. Rendre path relatif
    char *rel_path = path;
    if (strncmp(path, root_folder, root_len) == 0)
    {
        rel_path += root_len;
        if (*rel_path == '/')
            rel_path++;
    }

    // 3. Construire le chemin de destination final complet (relatif)
    char final_dest[512];
    snprintf(final_dest, sizeof(final_dest), "%s/%s", rel_path, full_name);

    // --- SOLUTION SANS COMMIT, AVEC NETTOYAGE COMPLET DES REFERENCES ---
    // "git mv" moves the folder AND updates the index and .gitmodules, so the
    // submodule keeps a valid url. If the repo is not registered as a
    // submodule (ex : empty repository), fall back to a plain mv.
    char rename_command[2048];
    snprintf(rename_command, sizeof(rename_command),
             "cd \"%s\" && "
             "(git mv -- \"%s\" \"%s\" || mv -- \"%s\" \"%s\") && "
             "(git submodule absorbgitdirs -- \"%s\" || true)",
             root_folder, rel_repo_path, final_dest, rel_repo_path, final_dest,
             final_dest);

    int rename_lock = JarvisLockAcquire("git.lock");
    int rename_status = system(rename_command);
    JarvisLockRelease(rename_lock);
    if (rename_status)
        errx(EXIT_FAILURE, "ERROR Impossible to rename the git repo folder");

    // ---- END TODO

    WriteInfo(repo_name, full_name);

    free(root_folder);
    free(path);

    return EXIT_SUCCESS;
}

// Check if it is an epita repo
int __IsEpitaRepo(char *repo_name)
{
    int i = 0;
    while (repo_name[i] != '\0' && repo_name[i] != '@')
        i++;

    char *reference_str = "@git.forge.epita.fr";

    int j = 0;
    while (repo_name[i + j] != '\0' && reference_str[j] != '\0'
           && repo_name[i + j] == reference_str[j])
        j++;

    return reference_str[j] == '\0';
}

// Extract the relative path of that repo
// Example : { S1, B1, P1-GaulishVillage-i3-shell-git } (for now : { S1, B1 })
char **__GetRelavitvePath(char *repo_name, size_t *size)
{
    char **relative_path = malloc(SIZE_OF_STRING);
    if (relative_path == NULL)
        err(EXIT_FAILURE, "malloc");
    (*size) = 0;

    int num_bimester;
    if (__ParseCourseId(repo_name, &num_bimester, NULL, 0) != EXIT_SUCCESS)
    {
        // Impossible to deduce the semester/bimester : "Others" folder
        (*size) = 1;
        relative_path[0] = GetOthersFolderName();
        if (relative_path[0] == NULL)
        {
            free(relative_path);
            err(EXIT_FAILURE, "strdup");
        }

        // "jarvis fill" gives the project of the repository (ex :
        // epita-prepa-computer-science) : <Others>/<project>/
        const char *project = getenv("JARVIS_PROJECT");
        if (project != NULL && project[0] != '\0' && project[0] != '.'
            && strchr(project, '/') == NULL)
        {
            relative_path[1] = strdup(project);
            if (relative_path[1] == NULL)
            {
                free(relative_path[0]);
                free(relative_path);
                err(EXIT_FAILURE, "strdup");
            }
            (*size) = 2;
        }
        return relative_path;
    }
    // Layers chosen with "jarvis custom" (Year / Semester / Bimester), maybe
    // none
    (*size) = GetLayerFolders(num_bimester, relative_path);

    return relative_path;
}

// Find a file or find or create a folder -> 0 if found / 1 if not found
int __OneLayerFindOrCreate(char *path, char *name, int is_folder)
{
    // Open the current directory
    DIR *dir;
    struct dirent *file;
    struct stat info;

    dir = opendir(path);

    if (dir == NULL)
        return EXIT_FAILURE;

    // Iterate trough all files and check if they are what we are looking for
    while ((file = readdir(dir)) != NULL)
    {
        // stat the entry relative to "path", not to the current directory
        char entry_path[4096];
        snprintf(entry_path, sizeof(entry_path), "%s/%s", path, file->d_name);
        if (stat(entry_path, &info) == 0)
        {
            if (S_ISDIR(info.st_mode) && is_folder)
            {
                // Folder case
                if (strcmp(name, file->d_name) == 0)
                {
                    closedir(dir);
                    return EXIT_SUCCESS;
                }
            }
            else if (!is_folder)
            {
                // Not a folder case
                if (strcmp(name, file->d_name) == 0)
                {
                    closedir(dir);
                    return EXIT_SUCCESS;
                }
            }
        }
    }

    closedir(dir);

    if (is_folder)
    {
        size_t size_fill_path = strlen(path) + strlen(name) + 2;
        char fullpath[size_fill_path];
        if (snprintf(fullpath, size_fill_path, "%s%s", path, name) < 0)
            return EXIT_FAILURE;

        if (mkdir(fullpath, 0755) && !IsDirectory(fullpath))
        {
            errx(EXIT_FAILURE,
                 "ERROR Impossible to create the folder : %s (Check the rights "
                 "or try to rerun)",
                 fullpath);
        }
        // printf("Jarvis created : %s\n", fullpath);
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}