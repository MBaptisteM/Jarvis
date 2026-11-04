#include "install_handler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "info_file.h"
#include "ui.h"

typedef enum
{
    METHOD_NIX,
    METHOD_APT,
    METHOD_COUNT
} InstallMethod;

static const char *METHOD_NAMES[METHOD_COUNT] = { "nix", "apt" };

static int __Exists(const char *binary)
{
    char command[256];
    snprintf(command, sizeof(command), "command -v %s > /dev/null 2>&1",
             binary);
    return system(command) == 0;
}

static int __RunFormat(const char *fmt, const char *package)
{
    char command[512];
    snprintf(command, sizeof(command), fmt, package);
    return system(command);
}

// "nix profile add" only exists in recent versions of Nix, older ones use
// "nix profile install" : both are tried.
// 0 : success | -1 : nix is not installed
static int __TryNix(const char *package)
{
    if (!__Exists("nix"))
        return -1;
    if (__RunFormat("nix profile add nixpkgs#%s", package) == 0)
        return 0;
    return __RunFormat("nix profile install nixpkgs#%s", package);
}

// 0 : success | -1 : apt-get (or the rights to use it) is missing
static int __TryApt(const char *package)
{
    if (!__Exists("apt-get"))
        return -1;
    if (geteuid() == 0)
        return __RunFormat("apt-get install -y %s", package);
    if (!__Exists("sudo"))
        return -1;
    return __RunFormat("sudo apt-get install -y %s", package);
}

static int __TryMethod(InstallMethod method, const char *nix_pkg,
                       const char *apt_pkg)
{
    return method == METHOD_NIX ? __TryNix(nix_pkg) : __TryApt(apt_pkg);
}

// Method saved in the info file (-1 if none)
static int __SavedMethod(void)
{
    char *value = NULL;
    int found = -1;
    if (ReadInfo(INSTALL_METHOD_KEY, &value) == EXIT_SUCCESS && value != NULL)
        for (int m = 0; m < METHOD_COUNT; m++)
            if (strcmp(value, METHOD_NAMES[m]) == 0)
                found = m;
    free(value);
    return found;
}

// A method worked : remember it if it is not the one that was expected
static void __RememberMethod(InstallMethod worked, InstallMethod expected,
                             int was_saved)
{
    if (was_saved && worked == expected)
        return;

    WriteInfo(INSTALL_METHOD_KEY, (char *)METHOD_NAMES[worked]);
    if (worked != expected)
        UiInfo("Installing with '%s' did not work but '%s' did : the install "
               "method is now set to '%s' (it was wrongly deduced).",
               METHOD_NAMES[expected], METHOD_NAMES[worked],
               METHOD_NAMES[worked]);
}

int InstallPackage(const char *nix_pkg, const char *apt_pkg,
                   const char *binary)
{
    if (__Exists(binary))
        return 0;

    // Expected method : the saved one, else deduced from what is installed
    int saved = __SavedMethod();
    InstallMethod expected = saved >= 0 ? (InstallMethod)saved
        : __Exists("nix")               ? METHOD_NIX
                                        : METHOD_APT;

    for (int k = 0; k < METHOD_COUNT; k++)
    {
        InstallMethod method = (InstallMethod)((expected + k) % METHOD_COUNT);

        if (__TryMethod(method, nix_pkg, apt_pkg) == 0 && __Exists(binary))
        {
            __RememberMethod(method, expected, saved >= 0);
            return 0;
        }
        UiWarning("Installing %s with %s failed.", binary,
                  METHOD_NAMES[method]);
    }

    UiError("Impossible to install %s (tried : nix, apt).", binary);
    return -1;
}
