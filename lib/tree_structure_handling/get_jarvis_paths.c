#include <get_jarvis_paths.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Get the commands_path depending on how the tool has been installed
const char *GetCommandPath()
{
    char *home_path = NULL;
    if (__GetRootPath(&home_path) != EXIT_SUCCESS
        && __GetLocalPath(&home_path) != EXIT_SUCCESS)
    {
        if (access("./commands", F_OK) == 0)
            return "./commands";
        else
            errx(EXIT_FAILURE,
                 "ERROR Impossible to find the path to jarvis commands, try to "
                 "install again.");
    }

    static char commands_path[512];
    snprintf(commands_path, sizeof(commands_path), "%s/cmd", home_path);

    free(home_path);

    return commands_path;
}

// Get the subject_path depending on how the tool has been installed
const char *GetSubjectFolderPath()
{
    // "jarvis fill" gives every parallel clone its own working folder, so that
    // they do not overwrite each other's subject.html / given files
    const char *override_path = getenv("JARVIS_SUBJECT_DIR");
    if (override_path != NULL && override_path[0] != '\0')
        return override_path;

    char *home_path = NULL;
    if (GetDotJarvisPath(&home_path))
    {
        if (access("./subject", F_OK) == 0)
            return "./subject";
        else
            errx(EXIT_FAILURE,
                 "ERROR Impossible to find the path to jarvis commands, try to "
                 "install again.");
    }

    static char subject_path[512];
    snprintf(subject_path, sizeof(subject_path), "%s/subject", home_path);

    free(home_path);

    return subject_path;
}

// Get path of the info file
char *GetInfoPath()
{
    char *home_path = NULL;
    if (GetDotJarvisPath(&home_path))
        errx(EXIT_FAILURE, "ERROR Impossible to find home path.");

    char *info_file_full_path = malloc(512);
    if (info_file_full_path == NULL)
    {
        free(home_path);
        err(EXIT_FAILURE, "malloc");
    }
    snprintf(info_file_full_path, 512, "%s/%s", home_path,
             INFORMATION_FILE_NAME);
    free(home_path);

    return info_file_full_path;
}

#ifndef REPO_NAME
#define REPO_NAME "EPITA-TPs"
#endif

char *FindFileBFS(const char *file_to_find);

// Is there a .git inside this folder ?
static int __HasGitFolder(const char *path)
{
    size_t size = strlen(path) + sizeof("/.git");
    char *git_path = malloc(size);
    if (git_path == NULL)
        return 0;

    snprintf(git_path, size, "%s/.git", path);
    int result = access(git_path, F_OK) == 0;
    free(git_path);
    return result;
}

// Search the root repository (REPO_NAME) in the arborescence, starting from
// $HOME, and register it in the info file. Returns its path with a trailing
// '/', or NULL if it can't be found anywhere.
static char *__SearchRootRepo(void)
{
    char *parent = FindFileBFS(REPO_NAME);
    if (parent == NULL)
        return NULL;

    size_t size = strlen(parent) + strlen(REPO_NAME) + 3;
    char *root = malloc(size);
    if (root == NULL)
    {
        free(parent);
        err(EXIT_FAILURE, "malloc");
    }
    snprintf(root, size, "%s/%s/", parent, REPO_NAME);
    free(parent);

    if (!__HasGitFolder(root))
    {
        free(root);
        return NULL;
    }

    WriteInfo(NAME_PATH_INFO_FILE, root);
    if (strcmp(NAME_PATH_INFO_FILE, "main_path") != 0)
        WriteInfo("main_path", root);

    return root;
}

// Same contract as ReadInfo("main_path", value) (0 : found | 1 : not found),
// but when the saved root repository is missing or no longer valid, it is
// searched again in the arborescence and the info file is updated.
int ReadRootPath(char **value)
{
    *value = NULL;

    char *stored = NULL;
    if (ReadInfo("main_path", &stored) == EXIT_SUCCESS && stored != NULL)
    {
        int valid = __HasGitFolder(stored);

        // The saved path may be the parent of the root repository
        if (!valid)
        {
            size_t size = strlen(stored) + strlen(REPO_NAME) + 2;
            char *nested = malloc(size);
            if (nested == NULL)
                err(EXIT_FAILURE, "malloc");
            snprintf(nested, size, "%s/%s", stored, REPO_NAME);
            valid = __HasGitFolder(nested);
            free(nested);
        }

        if (valid)
        {
            *value = stored;
            return EXIT_SUCCESS;
        }
    }

    // Not saved, or the saved path is gone : search it again
    char *found = __SearchRootRepo();
    if (found != NULL)
    {
        free(stored);
        *value = found;
        return EXIT_SUCCESS;
    }

    // Not a git repository yet (e.g. folder just created) but it exists
    if (stored != NULL && access(stored, F_OK) == 0)
    {
        *value = stored;
        return EXIT_SUCCESS;
    }

    free(stored);
    return EXIT_FAILURE;
}


// Default folder prefix of a layer ("year", "semester" or "bimester")
const char *GetDefaultLayerPrefix(const char *layer)
{
    if (strcmp(layer, "year") == 0)
        return "Y";
    if (strcmp(layer, "semester") == 0)
#ifdef SEMESTER
        return SEMESTER;
#else
        return "S";
#endif
#ifdef BIMESTER
    return BIMESTER;
#else
    return "B";
#endif
}

// Tree configuration : stored in <root repository>/.jarvis_config
#define JARVIS_CONFIG_FILE_NAME ".jarvis_config"
#define DEFAULT_OTHERS_NAME "Others"
#define CONFIG_PREFIX_MAX 32
#define CONFIG_OTHERS_MAX 64

typedef struct
{
    int enabled[3];
    char prefix[3][CONFIG_PREFIX_MAX + 1];
    char others[CONFIG_OTHERS_MAX + 1];
} JarvisConfig;

static const char *CONFIG_LAYERS[3] = { "year", "semester", "bimester" };

static JarvisConfig jarvis_config;
static int jarvis_config_loaded = 0;

static int __ConfigLayerIndex(const char *layer)
{
    for (int i = 0; i < 3; i++)
        if (strcmp(layer, CONFIG_LAYERS[i]) == 0)
            return i;
    return -1;
}

static int __ConfigValidPrefix(const char *prefix)
{
    size_t length = strlen(prefix);
    if (length == 0 || length > CONFIG_PREFIX_MAX || prefix[0] == '.')
        return 0;
    for (size_t i = 0; i < length; i++)
    {
        unsigned char c = (unsigned char)prefix[i];
        if (!isalnum(c) && c != '-' && c != '_' && c != '.')
            return 0;
    }
    return 1;
}

// The name of the "Others" folder is a single folder name
static int __ConfigValidOthers(const char *name)
{
    size_t length = strlen(name);
    if (length == 0 || length > CONFIG_OTHERS_MAX || name[0] == '.')
        return 0;
    for (size_t i = 0; i < length; i++)
    {
        unsigned char c = (unsigned char)name[i];
        if (c == '/' || c == '\\' || c == '"' || c == '\'' || c == '='
            || iscntrl(c))
            return 0;
    }
    return 1;
}

static void __ConfigDefaults(JarvisConfig *config)
{
    for (int i = 0; i < 3; i++)
    {
        config->enabled[i] = i != 0;
        snprintf(config->prefix[i], sizeof(config->prefix[i]), "%s",
                 GetDefaultLayerPrefix(CONFIG_LAYERS[i]));
    }
    snprintf(config->others, sizeof(config->others), "%s", DEFAULT_OTHERS_NAME);
}

// Path of the config file (malloc'd), NULL when the root repository is unknown
static char *__ConfigPath(void)
{
    char *root = NULL;
    if (ReadRootPath(&root) != EXIT_SUCCESS || root == NULL)
        return NULL;

    size_t length = strlen(root);
    while (length > 1 && root[length - 1] == '/')
        root[--length] = '\0';

    // The saved path may be the parent of the root repository
    if (!__HasGitFolder(root))
    {
        size_t nested_size = length + strlen(REPO_NAME) + 2;
        char *nested = malloc(nested_size);
        if (nested == NULL)
            err(EXIT_FAILURE, "malloc");
        snprintf(nested, nested_size, "%s/%s", root, REPO_NAME);
        if (__HasGitFolder(nested))
        {
            free(root);
            root = nested;
            length = strlen(root);
        }
        else
            free(nested);
    }

    size_t size = length + sizeof("/" JARVIS_CONFIG_FILE_NAME) + 1;
    char *path = malloc(size);
    if (path == NULL)
        err(EXIT_FAILURE, "malloc");
    snprintf(path, size, "%s/%s", root, JARVIS_CONFIG_FILE_NAME);
    free(root);
    return path;
}

// Write the config (atomically). 0 on success.
static int __ConfigWrite(const char *path, const JarvisConfig *config)
{
    size_t tmp_size = strlen(path) + sizeof(".tmp");
    char *tmp = malloc(tmp_size);
    if (tmp == NULL)
        err(EXIT_FAILURE, "malloc");
    snprintf(tmp, tmp_size, "%s.tmp", path);

    FILE *file = fopen(tmp, "w");
    if (file == NULL)
    {
        free(tmp);
        return EXIT_FAILURE;
    }

    int failed = 0;
    failed |= fprintf(file,
                      "# Jarvis tree configuration (shared with every Jarvis "
                      "using this root repository).\n"
                      "# Edit it with 'jarvis custom'.\n") < 0;
    for (int i = 0; i < 3; i++)
        failed |= fprintf(file, "layer_%s = %s\n", CONFIG_LAYERS[i],
                          config->enabled[i] ? "on" : "off") < 0;
    for (int i = 0; i < 3; i++)
        failed |= fprintf(file, "prefix_%s = %s\n", CONFIG_LAYERS[i],
                          config->prefix[i]) < 0;
    failed |= fprintf(file, "others_name = %s\n", config->others) < 0;
    failed |= fclose(file) != 0;

    if (!failed && rename(tmp, path) != 0)
        failed = 1;
    if (failed)
        remove(tmp);
    free(tmp);
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}

static char *__ConfigTrim(char *str)
{
    while (isspace((unsigned char)*str))
        str++;
    size_t length = strlen(str);
    while (length > 0 && isspace((unsigned char)str[length - 1]))
        str[--length] = '\0';
    return str;
}

// Apply one "key = value" to a config. 0 if it was accepted.
static int __ConfigApply(JarvisConfig *config, const char *key,
                         const char *value)
{
    if (strncmp(key, "layer_", 6) == 0)
    {
        int index = __ConfigLayerIndex(key + 6);
        if (index < 0 || (strcmp(value, "on") != 0 && strcmp(value, "off") != 0))
            return EXIT_FAILURE;
        config->enabled[index] = strcmp(value, "on") == 0;
        return EXIT_SUCCESS;
    }
    if (strncmp(key, "prefix_", 7) == 0)
    {
        int index = __ConfigLayerIndex(key + 7);
        if (index < 0 || !__ConfigValidPrefix(value))
            return EXIT_FAILURE;
        snprintf(config->prefix[index], sizeof(config->prefix[index]), "%s",
                 value);
        return EXIT_SUCCESS;
    }
    if (strcmp(key, "others_name") == 0)
    {
        if (!__ConfigValidOthers(value))
            return EXIT_FAILURE;
        snprintf(config->others, sizeof(config->others), "%s", value);
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}

// Load the config once per process. Created with the default values when the
// file does not exist. Without a root repository, the defaults are used (and
// nothing is cached, so the file is read as soon as the root exists).
static void __ConfigLoad(void)
{
    if (jarvis_config_loaded)
        return;

    JarvisConfig config;
    __ConfigDefaults(&config);

    char *path = __ConfigPath();
    if (path == NULL)
    {
        jarvis_config = config;
        return;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        if (access(path, F_OK) != 0)
            __ConfigWrite(path, &config);
    }
    else
    {
        char *line = NULL;
        size_t capacity = 0;
        while (getline(&line, &capacity, file) >= 0)
        {
            char *content = __ConfigTrim(line);
            if (content[0] == '#' || content[0] == '\0')
                continue;

            char *equal = strchr(content, '=');
            if (equal == NULL)
                continue;
            *equal = '\0';
            __ConfigApply(&config, __ConfigTrim(content),
                          __ConfigTrim(equal + 1));
        }
        free(line);
        fclose(file);
    }

    free(path);
    jarvis_config = config;
    jarvis_config_loaded = 1;
}

// Make sure the config file exists in the root repository (created with the
// default values otherwise)
void EnsureConfigFile(void)
{
    __ConfigLoad();
}

// Change one setting ("layer_<layer>" = on/off, "prefix_<layer>" = text,
// "others_name" = folder name) and save it. 0 on success.
int SetConfigValue(const char *key, const char *value)
{
    __ConfigLoad();

    char *path = __ConfigPath();
    if (path == NULL)
        return EXIT_FAILURE;

    JarvisConfig updated = jarvis_config;
    int result = __ConfigApply(&updated, key, value);
    if (result == EXIT_SUCCESS)
        result = __ConfigWrite(path, &updated);
    free(path);

    if (result == EXIT_SUCCESS)
    {
        jarvis_config = updated;
        jarvis_config_loaded = 1;
    }
    return result;
}

// Is this layer used to classify the repositories ?
int IsLayerEnabled(const char *layer)
{
    __ConfigLoad();

    int index = __ConfigLayerIndex(layer);
    return index < 0 ? 0 : jarvis_config.enabled[index];
}

// Folder prefix of a layer (malloc'd, the caller must free it)
char *GetLayerPrefix(const char *layer)
{
    __ConfigLoad();

    int index = __ConfigLayerIndex(layer);
    char *value = strdup(index < 0 ? GetDefaultLayerPrefix(layer)
                                   : jarvis_config.prefix[index]);
    if (value == NULL)
        err(EXIT_FAILURE, "strdup");
    return value;
}

// Name of the folder used when the course can't be deduced ("Others" by
// default), malloc'd, the caller must free it
char *GetOthersFolderName(void)
{
    __ConfigLoad();

    char *value = strdup(jarvis_config.others);
    if (value == NULL)
        err(EXIT_FAILURE, "strdup");
    return value;
}

// Folder names (malloc'd, outermost first) of the enabled layers for a
// bimester number : ex : { "Y2", "S4", "B8" }. Returns how many there are
// (0 to 3, 0 means the repositories go directly in the root repository).
size_t GetLayerFolders(int bimester, char **folders)
{
    int semester = bimester / 2 + bimester % 2;
    int year = semester / 2 + semester % 2;

    const char *layers[3] = { "year", "semester", "bimester" };
    int numbers[3] = { year, semester, bimester };

    size_t count = 0;
    for (int i = 0; i < 3; i++)
    {
        if (!IsLayerEnabled(layers[i]))
            continue;

        char *prefix = GetLayerPrefix(layers[i]);
        size_t size = strlen(prefix) + 16;
        folders[count] = malloc(size);
        if (folders[count] == NULL)
            err(EXIT_FAILURE, "malloc");
        snprintf(folders[count], size, "%s%d", prefix, numbers[i]);
        free(prefix);
        count++;
    }

    return count;
}

// Get path of the main folder TPs (root folder) | NULL if not found
char *GetTPsPath()
{
    char *TPs_path = NULL;

    if (!ReadInfo(NAME_PATH_INFO_FILE, &TPs_path))
    {
        if (access(TPs_path, F_OK) == 0)
            return TPs_path;
        free(TPs_path);
    }

    // Saved path missing or gone : search the root repository again
    TPs_path = __SearchRootRepo();
    if (TPs_path != NULL)
        return TPs_path;

    char *marked_path = FindMarkedFileBFS();
    if (marked_path == NULL)
        return NULL;

    size_t marked_size = strlen(marked_path) + 2;
    TPs_path = malloc(marked_size);
    if (TPs_path == NULL)
    {
        free(marked_path);
        err(EXIT_FAILURE, "malloc");
    }
    snprintf(TPs_path, marked_size, "%s/", marked_path);
    free(marked_path);
    WriteInfo(NAME_PATH_INFO_FILE, TPs_path);

    return TPs_path;
}

// Get the path to the system-wide Jarvis installation
int __GetRootPath(char **path)
{
    *path = NULL;
    if (access("/usr/local/lib/jarvis", F_OK) == 0)
    {
        *path = malloc(512);
        if (!*path)
            return EXIT_FAILURE;

        snprintf(*path, 512, "%s", "/usr/local/lib/jarvis");
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}

// Get the path to the user-local Jarvis installation
int __GetLocalPath(char **path)
{
    *path = NULL;
    char *home = getenv("HOME");
    if (home)
    {
        *path = malloc(512);

        if (!*path)
            return EXIT_FAILURE;

        snprintf(*path, 512, "%s/.local/lib/jarvis", home);

        if (access(*path, F_OK) == 0)
            return EXIT_SUCCESS;
        free(*path);
        *path = NULL;
    }

    return EXIT_FAILURE;
}

// Prefer the XDG config directory when it already exists.
int GetDotJarvisPath(char **path)
{
    *path = NULL;
    char *home = getenv("HOME");
    if (home)
    {
        *path = malloc(512);

        if (!*path)
            return EXIT_FAILURE;

        char config_path[512];
        struct stat config_stat;
        snprintf(config_path, sizeof(config_path), "%s/.config", home);
        if (stat(config_path, &config_stat) == 0
            && S_ISDIR(config_stat.st_mode))
            snprintf(*path, 512, "%s/.config/.jarvis", home);
        else
            snprintf(*path, 512, "%s/.jarvis", home);

        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}