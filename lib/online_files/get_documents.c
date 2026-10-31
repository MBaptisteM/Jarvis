#include "get_documents.h"

// Terminal width (80 when unknown)
static int __TerminalWidth(void)
{
    struct winsize size;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 20)
        return size.ws_col;
    return 80;
}

// One frame : "<spinner> <label> : <last line written by the command>"
static void __DrawSpinner(size_t frame, const char *label, const char *last)
{
    static const char *frames[] = { "|", "/", "-", "\\" };

    int room = __TerminalWidth() - (int)strlen(label) - 6;

    printf("\r\033[K\033[36m%s\033[0m %s", frames[frame % 4], label);
    if (room > 8 && last[0] != '\0')
    {
        size_t length = strlen(last);
        if (length > (size_t)room)
        {
            length = (size_t)room;
            // Do not cut a UTF-8 character in half
            while (length > 0 && ((unsigned char)last[length] & 0xC0) == 0x80)
                length--;
        }
        printf(" \033[90m: %.*s\033[0m", (int)length, last);
    }
    fflush(stdout);
}

// Run a shell command while showing a spinner and the last line of its output,
// so the user can see that something is happening (installation, download...).
// Without a terminal (pipe, script) the output is simply passed through.
// The output of a failed command is displayed. Returns the exit code of the
// command (-1 if it could not be run).
static int __RunWithSpinner(const char *label, const char *cmd)
{
    int interactive = isatty(STDOUT_FILENO);

    int fds[2];
    if (pipe(fds) != 0)
        return -1;

    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if (pid < 0)
    {
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    if (pid == 0)
    {
        close(fds[0]);
        // Nobody can answer a question while the spinner is running
        int null_fd = open("/dev/null", O_RDONLY);
        if (null_fd >= 0)
        {
            dup2(null_fd, STDIN_FILENO);
            close(null_fd);
        }
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);
        close(fds[1]);
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }
    close(fds[1]);

    if (!interactive)
    {
        printf("%s...\n", label);
        fflush(stdout);
    }

    char *log = interactive ? malloc(SPINNER_LOG_MAX + 1) : NULL;
    size_t log_length = 0;
    char line[512];
    size_t line_length = 0;
    char last[512] = "";
    int in_escape = 0;

    size_t frame = 0;
    unsigned reads = 0;
    if (interactive)
        __DrawSpinner(frame++, label, last);

    struct pollfd pfd = { fds[0], POLLIN, 0 };
    int open_pipe = 1;
    while (open_pipe)
    {
        int ready = poll(&pfd, 1, SPINNER_FRAME_MS);
        if (ready < 0)
        {
            if (errno == EINTR)
                continue;
            break;
        }

        if (ready > 0)
        {
            char buf[512];
            ssize_t count = read(fds[0], buf, sizeof(buf));
            if (count < 0 && errno == EINTR)
                continue;

            if (count <= 0)
                open_pipe = 0;
            else if (!interactive)
            {
                fwrite(buf, 1, (size_t)count, stdout);
                fflush(stdout);
            }
            else
            {
                // Keep the end of the output, shown if the command fails
                if (log != NULL)
                {
                    size_t size = (size_t)count;
                    if (log_length + size > SPINNER_LOG_MAX)
                    {
                        size_t drop = log_length + size - SPINNER_LOG_MAX;
                        memmove(log, log + drop, log_length - drop);
                        log_length -= drop;
                    }
                    memcpy(log + log_length, buf, size);
                    log_length += size;
                }

                // Remember the last line (colors and control codes removed)
                for (ssize_t i = 0; i < count; i++)
                {
                    unsigned char c = (unsigned char)buf[i];
                    if (in_escape == 1)
                        in_escape = c == '[' ? 2 : 0;
                    else if (in_escape == 2)
                    {
                        if (c >= '@' && c <= '~')
                            in_escape = 0;
                    }
                    else if (c == 0x1b)
                        in_escape = 1;
                    else if (c == '\n' || c == '\r')
                    {
                        if (line_length > 0)
                        {
                            line[line_length] = '\0';
                            memcpy(last, line, line_length + 1);
                            line_length = 0;
                        }
                    }
                    else if (c >= 0x20 && line_length < sizeof(line) - 1)
                        line[line_length++] = (char)c;
                }
            }
        }

        // No clock needed : one frame at each pause of the command (poll
        // timeout), and one every few reads when it writes without stopping
        if (interactive && open_pipe && (ready == 0 || ++reads % 8 == 0))
            __DrawSpinner(frame++, label, last);
    }
    close(fds[0]);

    int status = 0;
    pid_t waited;
    do
        waited = waitpid(pid, &status, 0);
    while (waited < 0 && errno == EINTR);

    int code = (waited >= 0 && WIFEXITED(status)) ? WEXITSTATUS(status) : -1;

    if (interactive)
    {
        printf("\r\033[K");
        if (code != 0 && log != NULL && log_length > 0)
        {
            log[log_length] = '\0';
            fputs(log, stdout);
            if (log[log_length - 1] != '\n')
                putchar('\n');
        }
        fflush(stdout);
    }
    free(log);

    return code;
}

// Install (visibly) what setup_get_subject needs : Node.js, playwright, Chromium.
// Done once, in the foreground, before any background download starts : the
// parallel children do not install the same packages at the same time.
// A spinner and the last line of the installation output are displayed, so the
// first run does not look frozen.
// EXIT_SUCCESS if everything is ready.
int EnsureDependencies(void)
{
    const char *subject_path = GetSubjectFolderPath();

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "\"%s/setup_get_subject\" setup \"%s\" \"%s\"",
             subject_path, URL, subject_path);

    UiInfo("Preparing Jarvis : Node.js, Playwright and Chromium.\n"
           "The first run downloads them, it can take a few minutes.");
    fflush(stdout);

    if (__RunWithSpinner("Checking the dependencies", cmd) != 0)
    {
        UiError("Impossible to install the dependencies of Jarvis.");
        return EXIT_FAILURE;
    }

    UiSuccess("Dependencies are ready.");
    fflush(stdout);

    return EXIT_SUCCESS;
}

// download any page with it url -> .jarvis/subject.html
int DowloadPage(char *url, char *file_name)
{
    const char *subject_path = GetSubjectFolderPath();

    char *jarvis_path = NULL;
    if (GetDotJarvisPath(&jarvis_path) != EXIT_SUCCESS)
        errx(EXIT_FAILURE, "ERROR Impossible to find the .jarvis directory");

    char page_path[strlen(URL) + strlen(url) + 10];
    snprintf(page_path, sizeof(page_path), "%s%s", URL, url);

    char auth_path[512];
    snprintf(auth_path, sizeof(auth_path), "%s/auth.json", subject_path);

    // If not already Auth
    if (access(auth_path, F_OK) != 0 && Auth() != EXIT_SUCCESS)
        errx(EXIT_FAILURE, "ERROR You are not authenticated. Run 'jarvis auth' "
                           "and sign in to the forge.");

    char cmd_subject[1024];
    snprintf(cmd_subject, sizeof(cmd_subject),
             "%s/./setup_get_subject run \"%s\" %s %s %s " JARVIS_DETACH_TTY,
             subject_path, page_path, subject_path, file_name, jarvis_path);
    free(jarvis_path);

    FILE *fp_page = popen(cmd_subject, "r");
    if (fp_page == NULL)
    {
        errx(EXIT_FAILURE, "ERROR Impossible to load the page %s.", page_path);
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), fp_page) != NULL)
    {
        printf("%s", buffer);
    }

    int status = pclose(fp_page);
    if (!WIFEXITED(status))
        errx(EXIT_FAILURE, "ERROR Impossible to download the document");

    return EXIT_SUCCESS;
}

// log and save the subject
int GetSubject(char *repo_name)
{
    const char *local_url_repo = GetLocalUrlRepo(repo_name);

    const char *subject_path = GetSubjectFolderPath();

    char auth_path[512];
    snprintf(auth_path, sizeof(auth_path), "%s/auth.json", subject_path);

    // If not already Auth
    if (access(auth_path, F_OK) != 0 && Auth() != EXIT_SUCCESS)
        errx(EXIT_FAILURE, "ERROR You are not authenticated. Run 'jarvis auth' "
                           "and sign in to the forge.");

    // Get subject
    char cmd_subject[1024];
    snprintf(cmd_subject, sizeof(cmd_subject),
             "%s/./setup_get_subject run \"%s%s%s\" %s " JARVIS_DETACH_TTY,
             subject_path, URL, local_url_repo, SUBJECT, subject_path);

    FILE *fp_subject = popen(cmd_subject, "r");
    if (!fp_subject)
        return EXIT_FAILURE;

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), fp_subject) != NULL)
    {
        printf("%s", buffer);
    }

    int status = pclose(fp_subject);
    if (status == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

// log and download the given files -> EXIT_FAILURE if no given file
int GetGivenFiles(char *repo_name)
{
    const char *local_url_repo = GetLocalUrlRepo(repo_name);

    const char *subject_path = GetSubjectFolderPath();

    char auth_path[512];
    snprintf(auth_path, sizeof(auth_path), "%s/auth.json", subject_path);

    // If not already Auth
    if (access(auth_path, F_OK) != 0 && Auth() != EXIT_SUCCESS)
        errx(EXIT_FAILURE, "ERROR You are not authenticated. Run 'jarvis auth' "
                           "and sign in to the forge.");

    // Get given files
    char cmd_given_files[1024];
    snprintf(cmd_given_files, sizeof(cmd_given_files),
             "%s/./setup_get_subject download \"%s%s%s\" %s "
             "</dev/null >/dev/null 2>&1",
             subject_path, URL, local_url_repo, GIVEN_FILES, subject_path);

    int status = system(cmd_given_files);
    if (status == -1)
        errx(-1, "ERROR Impossible to look for the given files");

    int exit_code = WEXITSTATUS(status);

    return exit_code; // 0 : success | 1 : failure
}

// Two ways to get the session cookies (auth.json) :
//  - browser (default)
//  - console

#define AUTH_MAX_ATTEMPTS 3
#define AUTH_INPUT_MAX 256
#define AUTH_CHECK_FILE "AuthCheck.html"
#define AUTH_CHECK_MAX (2 * 1024 * 1024)

// Check that the saved session really is a signed-in one : the home page is
// downloaded with it and must not be the sign-in form (password field), and
// must contain the projects. A window closed without signing in, or wrong
// credentials, still leave an auth.json behind : its presence proves nothing.
// 1 : signed in | 0 : not signed in (or impossible to check)
static int __SessionIsValid(const char *subject_path)
{
    char *jarvis_path = NULL;
    if (GetDotJarvisPath(&jarvis_path) != EXIT_SUCCESS)
        return 0;

    char file_path[1024];
    snprintf(file_path, sizeof(file_path), "%s/%s", jarvis_path,
             AUTH_CHECK_FILE);
    remove(file_path); // never read a page left by a previous run

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "\"%s/setup_get_subject\" run \"%s\" \"%s\" %s \"%s\" "
             ">/dev/null 2>&1",
             subject_path, URL, subject_path, AUTH_CHECK_FILE, jarvis_path);
    free(jarvis_path);

    UiInfo("Checking that you are signed in...");
    fflush(stdout);

    int status = system(cmd);
    if (status == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return 0;

    FILE *page = fopen(file_path, "r");
    if (page == NULL)
        return 0;

    char *content = malloc(AUTH_CHECK_MAX + 1);
    if (content == NULL)
    {
        fclose(page);
        return 0;
    }
    size_t length = fread(content, 1, AUTH_CHECK_MAX, page);
    fclose(page);
    remove(file_path);
    content[length] = '\0';

    for (size_t i = 0; i < length; i++)
        content[i] = (char)tolower((unsigned char)content[i]);

    int signed_in = strstr(content, "type=\"password\"") == NULL
        && strstr(content, "type='password'") == NULL
        && strstr(content, "type=password") == NULL
        && strstr(content, "project") != NULL;

    free(content);
    return signed_in;
}

// Lines written by the script that Jarvis now says itself, with its own style
static int __IsAuthNoise(const char *line)
{
    // The script writes colors in the middle of its sentences
    return strstr(line, "JARVIS_AUTH_WINDOW") != NULL
        || strstr(line, "connect to your forge account") != NULL
        || strstr(line, "Then close the window") != NULL
        || strstr(line, "Auth saved") != NULL;
}

static void __Wipe(char *buffer, size_t size)
{
    volatile char *p = buffer;
    while (size-- > 0)
        *p++ = 0;
}

// Read one line on stdin (echo disabled when hide is set). 0 if not empty.
static int __ReadAuthLine(const char *prompt, char *buffer, size_t size,
                          int hide)
{
    printf("%s", prompt);
    fflush(stdout);

    struct termios saved;
    int restore = 0;
    if (hide && tcgetattr(STDIN_FILENO, &saved) == 0)
    {
        struct termios silent = saved;
        silent.c_lflag &= ~(tcflag_t)ECHO;
        restore = tcsetattr(STDIN_FILENO, TCSAFLUSH, &silent) == 0;
    }

    char *result = fgets(buffer, (int)size, stdin);

    if (restore)
    {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved);
        printf("\n");
    }

    if (result == NULL)
        return EXIT_FAILURE;

    buffer[strcspn(buffer, "\r\n")] = '\0';
    return buffer[0] == '\0' ? EXIT_FAILURE : EXIT_SUCCESS;
}

// Browser : the window is opened by the script, the user signs in and closes it
static int __AuthBrowser(const char *subject_path, const char *auth_path)
{
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s/./setup_get_subject auth %s %s",
             subject_path, URL, subject_path);

    UiInfo("Opening the browser window (the first time, it can take a moment "
           "to prepare it)...");
    fflush(stdout);

    // A session left by a previous run must not count as a success
    remove(auth_path);

    FILE *script = popen(cmd, "r");
    if (script == NULL)
        UiErrx(EXIT_FAILURE, "Impossible to start the authentication.");

    // Keep the output of the script (packages installation...) but not its
    // own messages
    // The instructions are shown right when the window opens, after the
    // installation output, so they stay visible
    int shown = 0;
    char line[512];
    while (fgets(line, sizeof(line), script) != NULL)
    {
        if (!shown
            && (strstr(line, "JARVIS_AUTH_WINDOW") != NULL
                || strstr(line, "connect to your forge account") != NULL))
        {
            UiPanelBold(stdout, UI_SUCCESS, "Auth",
                        "Please connect to your forge account.\n"
                        "Then close the window.");
            fflush(stdout);
            shown = 1;
        }
        if (!__IsAuthNoise(line))
            printf("%s", line);
    }
    fflush(stdout);
    pclose(script);

    if (access(auth_path, F_OK) != 0)
    {
        UiError("Authentication failed : no session was saved. Try again with "
                "'jarvis auth'.");
        return EXIT_FAILURE;
    }

    if (!__SessionIsValid(subject_path))
    {
        remove(auth_path);
        UiError("Authentication failed : you are not signed in to the forge "
                "(window closed before signing in?). Try again with "
                "'jarvis auth'.");
        return EXIT_FAILURE;
    }

    UiSuccess("Authentication saved.");
    return EXIT_SUCCESS;
}

// Run the console authentication of the script. Returns its exit code (-1 if
// it could not be run).
static int __RunAuthConsole(const char *subject_path, const char *username,
                            const char *password)
{
    char script[1024];
    snprintf(script, sizeof(script), "%s/setup_get_subject", subject_path);

    int fds[2];
    if (pipe(fds) != 0)
        return -1;

    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0)
    {
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    if (pid == 0)
    {
        close(fds[1]);
        dup2(fds[0], STDIN_FILENO);
        close(fds[0]);
        execl(script, script, "auth-console", URL, subject_path, (char *)NULL);
        _exit(127);
    }
    close(fds[0]);

    // The script may exit before reading everything
    void (*previous)(int) = signal(SIGPIPE, SIG_IGN);
    FILE *to_script = fdopen(fds[1], "w");
    if (to_script != NULL)
    {
        fprintf(to_script, "%s\n%s\n", username, password);
        fclose(to_script);
    }
    else
        close(fds[1]);
    signal(SIGPIPE, previous);

    int status;
    if (waitpid(pid, &status, 0) < 0 || !WIFEXITED(status))
        return -1;
    return WEXITSTATUS(status);
}

// Console : username and password typed here, the script signs in by itself
static int __AuthConsole(const char *subject_path, const char *auth_path)
{
    for (int attempt = 1; attempt <= AUTH_MAX_ATTEMPTS; attempt++)
    {
        UiPanel(stdout, UI_INFO, "Auth",
                "Enter your EPITA forge credentials.\n"
                "The password is not displayed and is not saved, only the "
                "session is.");

        char username[AUTH_INPUT_MAX];
        char password[AUTH_INPUT_MAX];
        if (__ReadAuthLine("Username : ", username, sizeof(username), 0)
                != EXIT_SUCCESS
            || __ReadAuthLine("Password : ", password, sizeof(password), 1)
                != EXIT_SUCCESS)
        {
            __Wipe(password, sizeof(password));
            UiError("No username or password provided.");
            return EXIT_FAILURE;
        }

        UiInfo("Signing in...");
        remove(auth_path); // a previous session must not count as a success
        int code = __RunAuthConsole(subject_path, username, password);
        __Wipe(username, sizeof(username));
        __Wipe(password, sizeof(password));

        if (code == 0 && access(auth_path, F_OK) == 0)
        {
            if (__SessionIsValid(subject_path))
            {
                UiSuccess("Authentication saved.");
                return EXIT_SUCCESS;
            }
            // The script saved a session but it is not a signed-in one
            remove(auth_path);
            code = 2;
        }

        if (code == 2 && attempt < AUTH_MAX_ATTEMPTS)
        {
            UiWarning("Wrong username or password (%d/%d), try again.", attempt,
                      AUTH_MAX_ATTEMPTS);
            continue;
        }

        if (code == 2)
            UiError("Wrong username or password.");
        else
            UiError("Authentication failed. Try again with 'jarvis auth'.");
        return EXIT_FAILURE;
    }

    return EXIT_FAILURE;
}

// Auth to forge : a browser window (default) or the console
int Auth()
{
    const char *subject_path = GetSubjectFolderPath();

    char auth_path[512];
    snprintf(auth_path, sizeof(auth_path), "%s/auth.json", subject_path);

    // Nobody to answer (script, pipe...) : browser, as before
    int console = 0;
    if (isatty(STDIN_FILENO) && isatty(STDOUT_FILENO))
    {
        UiQuestion("How do you want to authenticate ?");

        const char *options[] = {
            "Browser : sign in on the forge in a window (default)",
            "Console : type your username and password here"
        };
        console = ChoiceMCQ(options, 2) == 1;
    }

    return console ? __AuthConsole(subject_path, auth_path)
                   : __AuthBrowser(subject_path, auth_path);
}

// The saved session does not work (expired, never signed in...) : forget it,
// tell the user and ask to sign in again. EXIT_SUCCESS once signed in.
int Reauth(void)
{
    char auth_path[512];
    snprintf(auth_path, sizeof(auth_path), "%s/auth.json",
             GetSubjectFolderPath());
    remove(auth_path);

    UiWarning("Your session on the forge is not valid (expired or not signed "
              "in). Please sign in again.");
    fflush(stderr);

    return Auth();
}

#define a 1000

// Get the forge local url from a repo name
const char *GetLocalUrlRepo(char *repo_name)
{
    char *actual_character = repo_name;
    size_t index = 0;
    size_t size_beginin_repo_path = strlen(BEGINING_REPO_LOCAL_PATH);

    char type_course[256];
    char tp_id[256];

    while (*actual_character != 0)
    {
        while (index < size_beginin_repo_path
               && *actual_character == BEGINING_REPO_LOCAL_PATH[index])
        {
            index++;
            actual_character++;
        }

        if (index == size_beginin_repo_path)
        {
            size_t i = 0;
            while (*actual_character != 0 && *actual_character != '/')
            {
                type_course[i] = *actual_character;
                actual_character++;
                i++;
            }
            type_course[i] = 0;

            i = 0;
            actual_character++;

            while (*actual_character != 0 && *actual_character != '/')
            {
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